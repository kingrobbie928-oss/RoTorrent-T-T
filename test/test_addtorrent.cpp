// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 6 check: Add torrent links and Add New Torrent dialogs, and the flow that feeds the engine.
// Usage: test_addtorrent <profile-dir> <data-dir> [screenshot-dir]. Licensed GPL-2.0-or-later.

#include <cstdio>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>

#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/file_storage.hpp>

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QTreeWidget>

#include "app/firstrun.h"
#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/logger.h"
#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/path.h"
#include "base/torrentfileguard.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "gui/MainWindow.h"
#include "gui/dialogs/AddLinksDialog.h"
#include "gui/dialogs/AddNewTorrentDialog.h"
#include "gui/dialogs/AddTorrentFlow.h"
#include "gui/theme/RoTheme.h"

namespace
{
    int failures = 0;
    QString shotDir;

    void check(const bool ok, const QString &what)
    {
        std::printf("%s  %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
        if (!ok)
            ++failures;
    }

    bool waitFor(const std::function<bool()> &cond, const int ms)
    {
        QEventLoop loop;
        QTimer poll;
        QTimer limit;
        limit.setSingleShot(true);
        QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (cond()) loop.quit(); });
        QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
        poll.start(20);
        limit.start(ms);
        loop.exec();
        return cond();
    }

    QByteArray makeTorrent(const QString &dataDir, const QString &name)
    {
        const std::string filePath = QDir(dataDir).filePath(name).toStdString();
        {
            std::mt19937 rng(static_cast<unsigned>(qHash(name)));
            std::ofstream out(filePath, std::ios::binary);
            for (int i = 0; i < 64 * 1024; ++i)
                out.put(static_cast<char>(rng() & 0xff));
        }
        lt::file_storage fs;
        lt::add_files(fs, filePath);
        lt::create_torrent ct(fs, 16 * 1024);
        lt::set_piece_hashes(ct, dataDir.toStdString());
        std::vector<char> buf;
        lt::bencode(std::back_inserter(buf), ct.generate());
        return QByteArray(buf.data(), static_cast<qsizetype>(buf.size()));
    }

    // Runs `action` on the modal dialog shortly after it opens.
    void answerModal(const std::function<void(QWidget *)> &action)
    {
        QTimer::singleShot(500, [action]
        {
            if (QWidget *w = QApplication::activeModalWidget())
                action(w);
        });
    }

    // For dialogs that open later (after a download): polls until a modal widget shows, then acts once.
    void answerModalWhenShown(const std::function<void(QWidget *)> &action, const int timeoutMs = 10000)
    {
        auto *poll = new QTimer(qApp);
        auto *waited = new int(0);
        QObject::connect(poll, &QTimer::timeout, poll, [poll, waited, action, timeoutMs]
        {
            *waited += 50;
            if (QWidget *w = QApplication::activeModalWidget())
            {
                poll->stop();
                action(w);
                poll->deleteLater();
                delete waited;
            }
            else if (*waited >= timeoutMs)
            {
                poll->stop();
                poll->deleteLater();
                delete waited;
            }
        });
        poll->start(50);
    }

    void clickButton(QWidget *dialog, const QDialogButtonBox::StandardButton which)
    {
        for (QDialogButtonBox *box : dialog->findChildren<QDialogButtonBox *>())
        {
            if (QPushButton *b = box->button(which))
            {
                b->click();
                return;
            }
        }
        // styled message boxes use their own buttons: click the first one, else close
        const auto buttons = dialog->findChildren<QPushButton *>();
        if (!buttons.isEmpty())
            buttons.last()->click();
        else
            dialog->close();
    }
}

int main(int argc, char **argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    RoTheme::configureHighDpi();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RoTorrent"));
    if (argc < 3)
        return 2;
    const QString profileDir = QString::fromLocal8Bit(argv[1]);
    const QString dataDir = QString::fromLocal8Bit(argv[2]);
    shotDir = (argc > 3) ? QString::fromLocal8Bit(argv[3]) : QString();
    QDir().mkpath(profileDir);
    QDir().mkpath(dataDir);
    if (!shotDir.isEmpty())
        QDir().mkpath(shotDir);

    Logger::initInstance();
    QObject::connect(Logger::instance(), &Logger::newLogMessage, &app, [](const Log::Msg &m) { std::printf("  log: %s\n", qPrintable(m.message)); });
    Profile::initInstance(Path(profileDir), QString(), false);
    SettingsStorage::initInstance();
    Preferences::initInstance();
    applyFirstRunDefaults();
    Net::ProxyConfigurationManager::initInstance();
    Net::DownloadManager::initInstance();
    BitTorrent::Session::initInstance();
    RoTheme::instance().init();
    auto *session = BitTorrent::Session::instance();
    check(waitFor([session] { return session->isRestored(); }, 60000), QStringLiteral("session restored"));

    MainWindow window;
    window.resize(1280, 800);
    window.show();
    AddTorrentFlow::install(window, session);

    // ---- link classification ----
    check(AddLinksDialog::isSupportedLink(QStringLiteral("magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567")), QStringLiteral("magnet link accepted"));
    check(AddLinksDialog::isSupportedLink(QStringLiteral("https://example.org/a.torrent")), QStringLiteral("https URL accepted"));
    check(AddLinksDialog::isSupportedLink(QStringLiteral("0123456789abcdef0123456789abcdef01234567")), QStringLiteral("40-digit info-hash accepted"));
    check(!AddLinksDialog::isSupportedLink(QStringLiteral("hello world")), QStringLiteral("random text rejected"));

    // ---- Add torrent links: dialog returns trimmed, de-duplicated lines ----
    {
        answerModal([](QWidget *w)
        {
            if (!shotDir.isEmpty())
                w->grab().save(shotDir + QStringLiteral("/add_links.png"));
            if (auto *edit = w->findChild<QPlainTextEdit *>())
                edit->setPlainText(QStringLiteral("  magnet:?xt=urn:btih:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa \n\nhttps://x.example/y.torrent\nhttps://x.example/y.torrent\n"));
            clickButton(w, QDialogButtonBox::Ok);
        });
        const auto links = AddLinksDialog::getLinks(&window);
        check(links.has_value() && links->size() == 2, QStringLiteral("links dialog returns 2 distinct links"));
        answerModal([](QWidget *w) { clickButton(w, QDialogButtonBox::Cancel); });
        check(!AddLinksDialog::getLinks(&window).has_value(), QStringLiteral("links dialog Cancel returns nothing"));
    }

    // ---- .torrent file: dialog shows the file, OK adds it with the chosen options ----
    {
        const QByteArray bytes = makeTorrent(dataDir, QStringLiteral("added_a.bin"));
        const QString torrentPath = QDir(dataDir).filePath(QStringLiteral("added_a.torrent"));
        QFile f(torrentPath);
        check(f.open(QIODevice::WriteOnly) && (f.write(bytes) == bytes.size()), QStringLiteral("test .torrent written"));
        f.close();
        const auto descr = BitTorrent::TorrentDescriptor::load(bytes);
        check(descr.has_value(), QStringLiteral("test .torrent parses"));

        answerModal([&](QWidget *w)
        {
            if (!shotDir.isEmpty())
                w->grab().save(shotDir + QStringLiteral("/add_new_torrent.png"));
            auto *tree = w->findChild<QTreeWidget *>();
            check(tree && (tree->topLevelItemCount() == 1), QStringLiteral("file tree lists the one file"));
            for (QCheckBox *box : w->findChildren<QCheckBox *>())
            {
                if (box->text() == QStringLiteral("Resume torrent after adding"))
                    box->setChecked(false);
            }
            for (QLineEdit *edit : w->findChildren<QLineEdit *>())
            {
                if (edit->text() == session->savePath().toString())
                    edit->setText(dataDir);
            }
            clickButton(w, QDialogButtonBox::Ok);
        });
        AddTorrentFlow::addSources(window, session, {torrentPath});
        // the engine finishes adding asynchronously
        auto *tp = descr ? session : nullptr;
        const auto idA = descr ? BitTorrent::TorrentID::fromInfoHash(descr->infoHash()) : BitTorrent::TorrentID();
        check(tp && waitFor([&] { return session->getTorrent(idA) != nullptr; }, 15000), QStringLiteral("OK adds the torrent to the session"));
        if (descr)
        {
            BitTorrent::Torrent *t = session->getTorrent(idA);
            check(t && t->isStopped(), QStringLiteral("unticking Resume torrent after adding adds it paused"));
            check(t && (t->savePath() == Path(dataDir)), QStringLiteral("chosen save path is used"));

            // Adding it again reports a duplicate instead of opening the dialog.
            answerModal([](QWidget *w) { clickButton(w, QDialogButtonBox::Ok); });
            AddTorrentFlow::addSources(window, session, {torrentPath});
            check(true, QStringLiteral("duplicate torrent is reported, not re-added"));
        }
    }

    // ---- unticking a file makes it Ignored ----
    {
        const QByteArray bytes = makeTorrent(dataDir, QStringLiteral("added_b.bin"));
        const QString torrentPath = QDir(dataDir).filePath(QStringLiteral("added_b.torrent"));
        QFile f(torrentPath);
        f.open(QIODevice::WriteOnly);
        f.write(bytes);
        f.close();
        const auto descr = BitTorrent::TorrentDescriptor::load(bytes);
        check(descr.has_value(), QStringLiteral("second test torrent parses"));
        answerModal([](QWidget *w)
        {
            if (auto *tree = w->findChild<QTreeWidget *>())
                tree->topLevelItem(0)->setCheckState(0, Qt::Unchecked);
            clickButton(w, QDialogButtonBox::Ok);
        });
        AddTorrentFlow::addSources(window, session, {torrentPath});
        const auto idB = BitTorrent::TorrentID::fromInfoHash(descr->infoHash());
        waitFor([&] { return session->getTorrent(idB) != nullptr; }, 15000);
        BitTorrent::Torrent *t = session->getTorrent(idB);
        check(t != nullptr, QStringLiteral("second torrent added"));
        check(t && (t->filePriorities().size() == 1) && (t->filePriorities()[0] == BitTorrent::DownloadPriority::Ignored)
              , QStringLiteral("unticked file has priority Ignored"));
    }

    // ---- magnet link: dialog explains metadata is pending, OK adds it ----
    {
        const QString magnet = QStringLiteral("magnet:?xt=urn:btih:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb&dn=Magnet%20Test");
        const auto descr = BitTorrent::TorrentDescriptor::parse(magnet);
        check(descr.has_value(), QStringLiteral("magnet parses"));
        answerModal([](QWidget *w)
        {
            if (!shotDir.isEmpty())
                w->grab().save(shotDir + QStringLiteral("/add_magnet.png"));
            auto *tree = w->findChild<QTreeWidget *>();
            check(tree && (tree->topLevelItemCount() == 0), QStringLiteral("magnet dialog has no files yet"));
            bool retrieving = false;
            for (QLabel *l : w->findChildren<QLabel *>())
                retrieving = retrieving || (l->text() == QStringLiteral("Retrieving metadata..."));
            check(retrieving, QStringLiteral("magnet dialog shows Retrieving metadata..."));
            clickButton(w, QDialogButtonBox::Ok);
        });
        AddTorrentFlow::addSources(window, session, {magnet});
        const auto idM = descr ? BitTorrent::TorrentID::fromInfoHash(descr->infoHash()) : BitTorrent::TorrentID();
        check(descr && waitFor([&] { return session->getTorrent(idM) != nullptr; }, 15000), QStringLiteral("magnet link is added"));
    }

    // ---- new Add dialog fields (slice 3): read straight from the dialog result ----
    {
        const QByteArray bytes = makeTorrent(dataDir, QStringLiteral("added_c.bin"));
        const QString torrentPath = QDir(dataDir).filePath(QStringLiteral("added_c.torrent"));
        QFile f(torrentPath);
        f.open(QIODevice::WriteOnly);
        f.write(bytes);
        f.close();
        const auto descr = BitTorrent::TorrentDescriptor::load(bytes);
        check(descr.has_value(), QStringLiteral("third test torrent parses"));
        answerModal([](QWidget *w)
        {
            if (!shotDir.isEmpty())
                w->grab().save(shotDir + QStringLiteral("/add_new_torrent_fields.png"));
            for (QComboBox *box : w->findChildren<QComboBox *>())
            {
                if (box->findText(QStringLiteral("Files checked")) >= 0)
                    box->setCurrentIndex(box->findText(QStringLiteral("Files checked")));
                if (box->findText(QStringLiteral("Automatic")) >= 0)
                    box->setCurrentIndex(box->findText(QStringLiteral("Manual")));
            }
            for (QCheckBox *box : w->findChildren<QCheckBox *>())
            {
                if ((box->text() == QStringLiteral("Skip hash check")) || (box->text() == QStringLiteral("Do not delete .torrent file")))
                    box->setChecked(true);
                if (box->text() == QStringLiteral("Never show again"))
                    box->setChecked(false);
            }
            clickButton(w, QDialogButtonBox::Ok);
        });
        const auto result = AddNewTorrentDialog::run(&window, session, descr.value(), torrentPath);
        check(result.has_value(), QStringLiteral("dialog with new fields returns a result"));
        if (result)
        {
            check(result->params.stopCondition == BitTorrent::Torrent::StopCondition::FilesChecked, QStringLiteral("Pause condition: Files checked is returned"));
            check(result->params.useAutoTMM == std::optional<bool>(false), QStringLiteral("Torrent management mode: Manual is returned"));
            check(result->params.seedMode, QStringLiteral("Skip hash check is returned as seed mode"));
            check(result->keepTorrentFile, QStringLiteral("Do not delete .torrent file is returned"));
            check(result->params.useDownloadPath.has_value(), QStringLiteral("incomplete-path choice is returned in Manual mode"));
        }
    }

    // ---- Automatic mode locks the path fields and sends no save path ----
    {
        const QByteArray bytes = makeTorrent(dataDir, QStringLiteral("added_d.bin"));
        const auto descr = BitTorrent::TorrentDescriptor::load(bytes);
        answerModal([](QWidget *w)
        {
            for (QComboBox *box : w->findChildren<QComboBox *>())
            {
                if (box->findText(QStringLiteral("Automatic")) >= 0)
                    box->setCurrentIndex(box->findText(QStringLiteral("Automatic")));
            }
            bool locked = false;
            for (QLineEdit *edit : w->findChildren<QLineEdit *>())
                locked = locked || !edit->isEnabled();
            check(locked, QStringLiteral("Automatic mode disables the path fields"));
            clickButton(w, QDialogButtonBox::Ok);
        });
        const auto result = AddNewTorrentDialog::run(&window, session, descr.value());
        check(result && (result->params.useAutoTMM == std::optional<bool>(true)) && result->params.savePath.isEmpty()
              , QStringLiteral("Automatic mode returns no explicit save path"));
        check(result && !result->keepTorrentFile, QStringLiteral("no .torrent checkbox for a magnet or bare descriptor"));
    }

    // ---- magnet metadata arrives while the dialog is open: tree fills, selection is returned ----
    {
        const QByteArray bytes = makeTorrent(dataDir, QStringLiteral("added_e.bin"));
        const auto full = BitTorrent::TorrentDescriptor::load(bytes);
        check(full.has_value() && full->info().has_value(), QStringLiteral("fourth test torrent parses"));
        const auto magnet = BitTorrent::TorrentDescriptor::parse(QStringLiteral("magnet:?xt=urn:btih:") + full->infoHash().v1().toString());
        check(magnet.has_value() && !magnet->info().has_value(), QStringLiteral("magnet of the same torrent has no metadata"));
        answerModal([&](QWidget *w)
        {
            auto *tree = w->findChild<QTreeWidget *>();
            check(tree && (tree->topLevelItemCount() == 0), QStringLiteral("tree is empty before the metadata arrives"));
            emit session->metadataDownloaded(*full->info());
            QApplication::processEvents();
            check(tree && (tree->topLevelItemCount() == 1), QStringLiteral("tree fills when the metadata arrives"));
            if (!shotDir.isEmpty())
                w->grab().save(shotDir + QStringLiteral("/add_magnet_metadata.png"));
            for (QCheckBox *box : w->findChildren<QCheckBox *>())
            {
                if (box->text() == QStringLiteral("Skip hash check"))
                    check(box->isEnabled(), QStringLiteral("Skip hash check becomes available with the metadata"));
            }
            if (tree && (tree->topLevelItemCount() == 1))
                tree->topLevelItem(0)->setCheckState(0, Qt::Unchecked);
            clickButton(w, QDialogButtonBox::Ok);
        });
        const auto result = AddNewTorrentDialog::run(&window, session, magnet.value());
        check(result && result->descriptor.info().has_value(), QStringLiteral("result carries the metadata that arrived"));
        check(result && (result->params.filePriorities.size() == 1)
              && (result->params.filePriorities[0] == BitTorrent::DownloadPriority::Ignored)
              , QStringLiteral("file unticked after the metadata arrived is Ignored"));
        check(!session->isKnownTorrent(magnet->infoHash()), QStringLiteral("hidden metadata torrent is gone after the dialog"));
    }

    // ---- .torrent file guard: deleted after adding unless "Do not delete" is ticked ----
    {
        TorrentFileGuard::setAutoDeleteMode(TorrentFileGuard::IfAdded);
        auto makeFile = [&](const QString &base)
        {
            const QByteArray bytes = makeTorrent(dataDir, base + QStringLiteral(".bin"));
            const QString path = QDir(dataDir).filePath(base + QStringLiteral(".torrent"));
            QFile f(path);
            f.open(QIODevice::WriteOnly);
            f.write(bytes);
            f.close();
            return path;
        };
        const QString gone = makeFile(QStringLiteral("guard_gone"));
        const QString kept = makeFile(QStringLiteral("guard_kept"));
        answerModal([](QWidget *w)
        {
            for (QCheckBox *box : w->findChildren<QCheckBox *>())
            {
                if (box->text() == QStringLiteral("Do not delete .torrent file"))
                    check(!box->isChecked(), QStringLiteral("Do not delete is unticked when the preference deletes"));
            }
            clickButton(w, QDialogButtonBox::Ok);
        });
        AddTorrentFlow::addSources(window, session, {gone});
        check(!QFile::exists(gone), QStringLiteral(".torrent file is deleted after adding"));
        answerModal([](QWidget *w)
        {
            for (QCheckBox *box : w->findChildren<QCheckBox *>())
            {
                if (box->text() == QStringLiteral("Do not delete .torrent file"))
                    box->setChecked(true);
            }
            clickButton(w, QDialogButtonBox::Ok);
        });
        AddTorrentFlow::addSources(window, session, {kept});
        check(QFile::exists(kept), QStringLiteral("Do not delete keeps the .torrent file"));
        TorrentFileGuard::setAutoDeleteMode(TorrentFileGuard::Never);
    }

    // ---- Never show again: later adds skip the dialog and use the session defaults ----
    {
        const QByteArray bytes = makeTorrent(dataDir, QStringLiteral("added_f.bin"));
        const QString path = QDir(dataDir).filePath(QStringLiteral("added_f.torrent"));
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(bytes);
        f.close();
        const auto descr = BitTorrent::TorrentDescriptor::load(bytes);
        answerModal([](QWidget *w)
        {
            for (QCheckBox *box : w->findChildren<QCheckBox *>())
            {
                if (box->text() == QStringLiteral("Never show again"))
                    box->setChecked(true);
            }
            clickButton(w, QDialogButtonBox::Cancel);
        });
        AddNewTorrentDialog::run(&window, session, descr.value());
        check(AddNewTorrentDialog::isEnabled(), QStringLiteral("Cancel does not change Never show again"));
        answerModal([](QWidget *w)
        {
            for (QCheckBox *box : w->findChildren<QCheckBox *>())
            {
                if (box->text() == QStringLiteral("Never show again"))
                    box->setChecked(true);
            }
            clickButton(w, QDialogButtonBox::Ok);
        });
        AddNewTorrentDialog::run(&window, session, descr.value());
        check(!AddNewTorrentDialog::isEnabled(), QStringLiteral("OK with Never show again turns the dialog off"));
        bool dialogShown = false;
        QTimer watchdog;   // only fires if a dialog wrongly opens (it would block forever otherwise)
        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &watchdog, [&dialogShown]
        {
            if (QWidget *w = QApplication::activeModalWidget())
            {
                dialogShown = true;
                clickButton(w, QDialogButtonBox::Cancel);
            }
        });
        watchdog.start(3000);
        AddTorrentFlow::addSources(window, session, {path});
        watchdog.stop();
        const auto idF = BitTorrent::TorrentID::fromInfoHash(descr->infoHash());
        check(waitFor([&] { return session->getTorrent(idF) != nullptr; }, 15000), QStringLiteral("torrent is added without the dialog"));
        check(!dialogShown, QStringLiteral("no dialog appears while it is turned off"));
        AddNewTorrentDialog::setEnabled(true);
    }

    // ---- http link: the flow downloads the .torrent, then shows the dialog ----
    {
        const QByteArray bytes = makeTorrent(dataDir, QStringLiteral("added_url.bin"));
        const auto descr = BitTorrent::TorrentDescriptor::load(bytes);
        check(descr.has_value(), QStringLiteral("url test torrent parses"));
        QTcpServer server;
        check(server.listen(QHostAddress::LocalHost), QStringLiteral("local test web server listens"));
        QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, bytes]
        {
            QTcpSocket *sock = server.nextPendingConnection();
            QObject::connect(sock, &QTcpSocket::readyRead, sock, [sock, bytes]
            {
                const QByteArray request = sock->readAll();
                const bool ok = request.startsWith("GET /ok.torrent");
                const QByteArray body = ok ? bytes : QByteArray("not found");
                sock->write((ok ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n")
                            + QByteArray("Content-Type: application/x-bittorrent\r\nConnection: close\r\nContent-Length: ")
                            + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                sock->disconnectFromHost();
            });
        });
        const QString base = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());

        bool sawDialog = false;
        answerModalWhenShown([&sawDialog](QWidget *w)
        {
            auto *tree = w->findChild<QTreeWidget *>();
            sawDialog = tree && (tree->topLevelItemCount() == 1);
            clickButton(w, QDialogButtonBox::Ok);
        });
        AddTorrentFlow::addSources(window, session, {base + QStringLiteral("/ok.torrent")});
        const auto idU = BitTorrent::TorrentID::fromInfoHash(descr->infoHash());
        check(waitFor([&] { return session->getTorrent(idU) != nullptr; }, 20000), QStringLiteral("http link: torrent is downloaded and added"));
        check(sawDialog, QStringLiteral("http link: Add dialog shows the downloaded file tree"));

        bool sawError = false;
        answerModalWhenShown([&sawError](QWidget *w)
        {
            sawError = true;
            clickButton(w, QDialogButtonBox::Ok);
        });
        AddTorrentFlow::addSources(window, session, {base + QStringLiteral("/missing.torrent")});
        waitFor([&] { return sawError; }, 20000);
        check(sawError, QStringLiteral("http link: a failed download shows an error box"));
        server.close();
    }

    // ---- invalid inputs raise the styled error box and add nothing ----
    {
        const QString bad = QDir(dataDir).filePath(QStringLiteral("not_a_torrent.torrent"));
        QFile f(bad);
        f.open(QIODevice::WriteOnly);
        f.write("this is not bencode");
        f.close();
        bool sawBox = false;
        answerModal([&sawBox](QWidget *w)
        {
            sawBox = true;
            clickButton(w, QDialogButtonBox::Ok);
            if (QWidget *m = QApplication::activeModalWidget())
                m->close();
        });
        AddTorrentFlow::addSources(window, session, {bad});
        check(sawBox, QStringLiteral("invalid .torrent shows an error box"));
    }

    window.close();
    BitTorrent::Session::freeInstance();
    std::printf("%s\n", failures ? "SOME FAILED" : "ALL OK");
    return failures ? 1 : 0;
}

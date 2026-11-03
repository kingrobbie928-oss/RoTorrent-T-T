// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 6 check: Statistics, Speed limits and About dialogs against a real session.
// Usage: test_infodialogs <profile-dir> <data-dir> [screenshot-dir]. Licensed GPL-2.0-or-later.

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
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QLineEdit>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextBrowser>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>

#include "app/firstrun.h"
#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/logger.h"
#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "gui/MainWindow.h"
#include "gui/dialogs/InfoDialogs.h"
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
    InfoDialogs::install(window, session);

    // ---- Statistics ----
    {
        answerModal([](QWidget *w)
        {
            if (!shotDir.isEmpty())
                w->grab().save(shotDir + QStringLiteral("/statistics.png"));
            QStringList keys;
            for (const QLabel *l : w->findChildren<QLabel *>())
            {
                if (l->objectName() == QStringLiteral("dlgKey"))
                    keys << l->text();
            }
            check(keys.size() == 13, QStringLiteral("statistics shows 13 rows"));
            check(!keys.contains(QStringLiteral("Request latency:")), QStringLiteral("no Request latency row"));
            check(keys.contains(QStringLiteral("All-time share ratio:")) && keys.contains(QStringLiteral("Queued tracker announces:"))
                  , QStringLiteral("statistics has the user and tracker rows"));
            w->close();
        });
        window.hooks().statistics();
    }

    // ---- Speed limits ----
    {
        session->setGlobalUploadSpeedLimit(0);
        session->setGlobalDownloadSpeedLimit(0);
        answerModal([](QWidget *w)
        {
            if (!shotDir.isEmpty())
                w->grab().save(shotDir + QStringLiteral("/speed_limits.png"));
            const auto spins = w->findChildren<QSpinBox *>();
            check(spins.size() == 4, QStringLiteral("speed dialog has four fields"));
            if (spins.size() == 4)
            {
                spins[0]->setValue(500);   // upload
                spins[1]->setValue(2000);  // download
            }
            clickButton(w, QDialogButtonBox::Ok);
        });
        window.hooks().speedLimits();
        check(session->globalUploadSpeedLimit() == 500 * 1024, QStringLiteral("upload limit applied (500 KiB/s)"));
        check(session->globalDownloadSpeedLimit() == 2000 * 1024, QStringLiteral("download limit applied (2000 KiB/s)"));
        answerModal([](QWidget *w)
        {
            const auto spins = w->findChildren<QSpinBox *>();
            if (spins.size() == 4)
                spins[0]->setValue(0);
            clickButton(w, QDialogButtonBox::Cancel);
        });
        window.hooks().speedLimits();
        check(session->globalUploadSpeedLimit() == 500 * 1024, QStringLiteral("Cancel leaves the limits alone"));
        session->setGlobalUploadSpeedLimit(0);
        session->setGlobalDownloadSpeedLimit(0);
    }

    // ---- About ----
    {
        answerModal([](QWidget *w)
        {
            if (!shotDir.isEmpty())
                w->grab().save(shotDir + QStringLiteral("/about.png"));
            auto *tabs = w->findChild<QTabWidget *>();
            check(tabs && (tabs->count() == 6), QStringLiteral("about has six tabs"));
            if (tabs)
            {
                tabs->setCurrentIndex(5);
                const QString text = tabs->currentWidget()->findChild<QTextBrowser *>() ? QString() : QString();
                Q_UNUSED(text)
                auto *b = qobject_cast<QTextBrowser *>(tabs->widget(5));
                check(b && b->toPlainText().contains(QString::fromLatin1(qVersion())), QStringLiteral("Software Used lists the real Qt version"));
                auto *credit = qobject_cast<QTextBrowser *>(tabs->widget(1));
                check(credit && credit->toPlainText().contains(QStringLiteral("qBittorrent")), QStringLiteral("Authors credits qBittorrent"));
                tabs->setCurrentIndex(0);
            }
            w->close();
        });
        window.hooks().about();
    }

    window.close();
    BitTorrent::Session::freeInstance();
    std::printf("%s\n", failures ? "SOME FAILED" : "ALL OK");
    return failures ? 1 : 0;
}

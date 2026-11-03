// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 6 slice 4 check: the Torrent Creator dialog makes a valid torrent another client can open.
// The torrent is verified with plain libtorrent (not RoTorrent's own readers): every v1 piece hash is recomputed
// from the files on disk. Usage: test_torrentcreator <profile-dir> <data-dir> [screenshot-dir].
// Licensed GPL-2.0-or-later.

#include <cstdio>
#include <fstream>
#include <random>

#include <libtorrent/file_storage.hpp>
#include <libtorrent/torrent_info.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEventLoop>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QUrl>

#include "app/firstrun.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/sharelimits.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "gui/MainWindow.h"
#include "gui/dialogs/TorrentCreatorDialog.h"
#include "gui/theme/RoTheme.h"
#include "gui/widgets/MainToolbar.h"

namespace
{
    int failures = 0;

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

    void writeRandom(const QString &path, const qint64 size, const unsigned seed)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        std::mt19937 rng(seed);
        std::ofstream out(path.toStdString(), std::ios::binary);
        for (qint64 i = 0; i < size; ++i)
            out.put(static_cast<char>(rng() & 0xff));
    }

    // Recomputes every v1 piece hash of `torrentFile` from the files under `parent` and compares.
    // Pad files (hybrid torrents) count as zeros. Returns an empty string when everything matches.
    QString verifyV1Pieces(const QString &torrentFile, const QString &parent)
    {
        lt::error_code ec;
        const lt::torrent_info ti(torrentFile.toStdString(), ec);
        if (ec)
            return QStringLiteral("libtorrent cannot read the file: %1").arg(QString::fromStdString(ec.message()));
        if (!ti.info_hashes().has_v1())
            return QStringLiteral("no v1 hashes");

        const int pieceLength = ti.piece_length();
        QCryptographicHash hash(QCryptographicHash::Sha1);
        int piece = 0;
        int inPiece = 0;
        QString problem;

        const auto feed = [&](const QByteArray &data)
        {
            qsizetype offset = 0;
            while (offset < data.size())
            {
                const int n = static_cast<int>(qMin<qsizetype>(pieceLength - inPiece, data.size() - offset));
                hash.addData(QByteArrayView(data.constData() + offset, n));
                inPiece += n;
                offset += n;
                if (inPiece == pieceLength)
                {
                    const lt::sha1_hash expected = ti.hash_for_piece(lt::piece_index_t {piece});
                    if (hash.result() != QByteArray(expected.data(), 20) && problem.isEmpty())
                        problem = QStringLiteral("piece %1 hash differs").arg(piece);
                    hash.reset();
                    inPiece = 0;
                    ++piece;
                }
            }
        };

        const lt::file_storage &fs = ti.files();
        for (lt::file_index_t i {0}; i < fs.end_file(); ++i)
        {
            const qint64 size = fs.file_size(i);
            if (fs.pad_file_at(i))
            {
                feed(QByteArray(static_cast<qsizetype>(size), '\0'));
                continue;
            }
            QFile f(QDir(parent).filePath(QString::fromStdString(fs.file_path(i))));
            if (!f.open(QIODevice::ReadOnly))
                return QStringLiteral("cannot read %1").arg(f.fileName());
            if (f.size() != size)
                return QStringLiteral("size of %1 differs").arg(f.fileName());
            while (!f.atEnd())
                feed(f.read(1 << 20));
        }
        if (inPiece > 0)
        {
            const lt::sha1_hash expected = ti.hash_for_piece(lt::piece_index_t {piece});
            if (hash.result() != QByteArray(expected.data(), 20) && problem.isEmpty())
                problem = QStringLiteral("last piece hash differs");
            ++piece;
        }
        if (problem.isEmpty() && (piece != ti.num_pieces()))
            problem = QStringLiteral("piece count %1 != %2").arg(piece).arg(ti.num_pieces());
        return problem;
    }

    template <typename T>
    T *child(QWidget *w, const char *name)
    {
        return w->findChild<T *>(QString::fromLatin1(name));
    }

    struct Notices
    {
        int errors = 0;
        int infos = 0;
        QString lastError;
        QString lastInfo;
    };

    void attach(TorrentCreatorDialog &dialog, Notices &n)
    {
        dialog.setNotifier([&n](const bool error, const QString &, const QString &text)
        {
            if (error)
            {
                ++n.errors;
                n.lastError = text;
            }
            else
            {
                ++n.infos;
                n.lastInfo = text;
            }
        });
    }

    // Creates with the dialog's current form and waits for it to end one way or the other.
    enum class Outcome { Created, Failed, Cancelled, Timeout };
    Outcome createAndWait(TorrentCreatorDialog &dialog, const Path &dest, const int ms = 60000)
    {
        Outcome out = Outcome::Timeout;
        const auto c1 = QObject::connect(&dialog, &TorrentCreatorDialog::created, &dialog, [&](const Path &) { out = Outcome::Created; });
        const auto c2 = QObject::connect(&dialog, &TorrentCreatorDialog::failed, &dialog, [&](const QString &) { out = Outcome::Failed; });
        const auto c3 = QObject::connect(&dialog, &TorrentCreatorDialog::cancelled, &dialog, [&] { out = Outcome::Cancelled; });
        if (!dialog.startCreation(dest))
            out = Outcome::Failed;
        waitFor([&] { return out != Outcome::Timeout; }, ms);
        QObject::disconnect(c1);
        QObject::disconnect(c2);
        QObject::disconnect(c3);
        return out;
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
    const QString shotDir = (argc > 3) ? QString::fromLocal8Bit(argv[3]) : QString();
    QDir().mkpath(profileDir);
    QDir().mkpath(dataDir);
    if (!shotDir.isEmpty())
        QDir().mkpath(shotDir);

    Logger::initInstance();
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
    TorrentCreatorDialog::install(window, session);
    check(static_cast<bool>(window.hooks().createTorrent), QStringLiteral("install() sets the createTorrent hook"));

    // ---- Tools menu action and toolbar button open the Creator ----
    {
        const auto opensCreator = [&](const std::function<void()> &trigger)
        {
            bool opened = false;
            QTimer::singleShot(300, [&opened]
            {
                if (auto *w = qobject_cast<TorrentCreatorDialog *>(QApplication::activeModalWidget()))
                {
                    opened = true;
                    w->reject();
                }
            });
            trigger();
            return opened;
        };
        check(opensCreator([&] { window.action(QStringLiteral("creator"))->trigger(); }), QStringLiteral("Tools > Torrent Creator opens the dialog"));
        check(opensCreator([&] { window.action(QStringLiteral("createTorrentFile"))->trigger(); }), QStringLiteral("File > Create Torrent opens the dialog"));
        check(opensCreator([&] { Q_EMIT window.toolbar()->createRequested(); }), QStringLiteral("toolbar Create torrent opens the dialog"));
    }

    // ---- piece size list ----
    {
        const QList<int> sizes = TorrentCreatorDialog::pieceSizes();
        check((sizes.size() == 15) && (sizes.first() == 0) && (sizes.at(1) == 16 * 1024) && (sizes.last() == 128 * 1024 * 1024)
              , QStringLiteral("piece sizes: Auto, 16 KiB ... 128 MiB"));
    }

    // ---- generated payload ----
    const QString root = QDir(dataDir).filePath(QStringLiteral("payload"));
    writeRandom(root + QStringLiteral("/alpha.bin"), 300 * 1024 + 123, 1);
    writeRandom(root + QStringLiteral("/sub/beta.bin"), 70 * 1024, 2);
    writeRandom(root + QStringLiteral("/.hidden.bin"), 20 * 1024, 3);
    const QString outDir = QDir(dataDir).filePath(QStringLiteral("out"));
    QDir().mkpath(outDir);

    // ---- A: hybrid folder torrent with every field; seeding on; verified against the files ----
    {
        TorrentCreatorDialog dialog(&window, session);
        Notices notes;
        attach(dialog, notes);
        dialog.show();
        check(child<QPushButton>(&dialog, "tcCreate")->text() == QStringLiteral("Create Torrent"), QStringLiteral("accept button reads Create Torrent"));
        if (!shotDir.isEmpty())
        {
            dialog.grab().save(shotDir + QStringLiteral("/creator_empty_dark.png"));
        }

        dialog.setInputPath(Path(root));
        child<QComboBox>(&dialog, "tcFormat")->setCurrentIndex(1);   // Hybrid
        auto *pieceBox = child<QComboBox>(&dialog, "tcPieceSize");
        pieceBox->setCurrentIndex(pieceBox->findData(16 * 1024));
        child<QCheckBox>(&dialog, "tcPrivate")->setChecked(true);
        child<QCheckBox>(&dialog, "tcIgnoreDotfiles")->setChecked(true);
        child<QCheckBox>(&dialog, "tcSeed")->setChecked(true);
        child<QCheckBox>(&dialog, "tcIgnoreLimits")->setChecked(true);
        child<QPlainTextEdit>(&dialog, "tcTrackers")->setPlainText(QStringLiteral("http://tracker-one.example/announce\n\n\n\nudp://tracker-two.example:6969/announce\n"));
        child<QPlainTextEdit>(&dialog, "tcWebSeeds")->setPlainText(QStringLiteral(" http://seed.example/files/ \n\n"));
        child<QPlainTextEdit>(&dialog, "tcComment")->setPlainText(QStringLiteral("made by the creator test"));
        child<QLineEdit>(&dialog, "tcSource")->setText(QStringLiteral("RoTest"));

        const Path dest = Path(outDir) / Path(QStringLiteral("payload.torrent"));
        const auto params = dialog.buildParams(dest);
        check((params.trackers == QStringList {QStringLiteral("http://tracker-one.example/announce"), QString(), QStringLiteral("udp://tracker-two.example:6969/announce")}),
              QStringLiteral("tracker box: blank line = next tier, runs collapse, lines trimmed"));
        check(params.urlSeeds == QStringList {QStringLiteral("http://seed.example/files/")}, QStringLiteral("web seed box: trimmed, blanks dropped"));

        // piece count button
        child<QPushButton>(&dialog, "tcCalc")->click();
        const bool counted = waitFor([&] { return child<QLabel>(&dialog, "tcPieceCount")->text().toInt() > 0; }, 15000);
        check(counted, QStringLiteral("Calculate number of pieces fills the number"));
        const int countedPieces = child<QLabel>(&dialog, "tcPieceCount")->text().toInt();

        QList<int> seen;
        QObject::connect(child<QProgressBar>(&dialog, "tcProgress"), &QProgressBar::valueChanged, &dialog, [&seen](const int v) { seen << v; });
        const Outcome out = createAndWait(dialog, dest);
        check(out == Outcome::Created, QStringLiteral("creation finishes (errors: %1)").arg(notes.lastError));
        check(child<QProgressBar>(&dialog, "tcProgress")->value() == 100, QStringLiteral("progress bar reaches 100%"));
        check(!dialog.isCreating() && child<QWidget>(&dialog, "tcScroll")->isEnabled(), QStringLiteral("form is usable again afterwards"));
        check((notes.infos == 1) && notes.lastInfo.contains(QStringLiteral("payload.torrent")), QStringLiteral("success message names the file"));
        check(QFile::exists(dest.data()), QStringLiteral("torrent file written"));

        // independent verification with plain libtorrent
        lt::error_code ec;
        const lt::torrent_info ti(dest.toString().toStdString(), ec);
        check(!ec, QStringLiteral("libtorrent opens the new torrent"));
        if (!ec)
        {
            check(ti.info_hashes().has_v1() && ti.info_hashes().has_v2(), QStringLiteral("hybrid: both info hashes present"));
            check(ti.piece_length() == 16 * 1024, QStringLiteral("piece length is 16 KiB"));
            check(ti.priv(), QStringLiteral("private flag set"));
            check(ti.name() == "payload", QStringLiteral("torrent name is the folder name"));
            check(ti.comment() == "made by the creator test", QStringLiteral("comment stored"));
            check(ti.trackers().size() == 2, QStringLiteral("two tracker tiers"));
            check((ti.web_seeds().size() == 1) && (ti.web_seeds().front().url == "http://seed.example/files/"), QStringLiteral("web seed stored"));
            int real = 0;
            bool hiddenSeen = false;
            for (lt::file_index_t i {0}; i < ti.files().end_file(); ++i)
            {
                if (ti.files().pad_file_at(i))
                    continue;
                ++real;
                hiddenSeen = hiddenSeen || (ti.files().file_path(i).find(".hidden") != std::string::npos);
            }
            check((real == 2) && !hiddenSeen, QStringLiteral("2 files, dotfile ignored"));
            check(ti.num_pieces() == countedPieces, QStringLiteral("Calculate number of pieces matched the real count (%1)").arg(countedPieces));
        }
        const QString problem = verifyV1Pieces(dest.toString(), dataDir);
        check(problem.isEmpty(), QStringLiteral("every v1 piece hash matches the files on disk %1").arg(problem));
        check(seen.size() > 2, QStringLiteral("progress was reported in steps (%1 updates)").arg(seen.size()));

        // re-read with RoTorrent's own reader and follow the torrent into the session
        const auto loaded = BitTorrent::TorrentDescriptor::loadFromFile(dest);
        check(loaded.has_value(), QStringLiteral("RoTorrent's reader accepts it"));
        if (loaded)
        {
            const auto id = BitTorrent::TorrentID::fromInfoHash(loaded->infoHash());
            check(waitFor([&] { return session->getTorrent(id) != nullptr; }, 15000), QStringLiteral("seeding on: the torrent is in the session"));
            if (auto *t = session->getTorrent(id))
            {
                check(t->isPrivate(), QStringLiteral("session torrent is private"));
                check(t->savePath() == Path(dataDir), QStringLiteral("save path is the folder that holds the payload"));
                check((t->shareLimits().ratioLimit == BitTorrent::NO_RATIO_LIMIT) && (t->shareLimits().seedingTimeLimit == BitTorrent::NO_SEEDING_TIME_LIMIT)
                      , QStringLiteral("'Ignore share ratio limits' removes the limits"));
                t->forceRecheck();
                const bool rechecked = waitFor([t] { return !t->isChecking() && (t->progress() >= 1.0); }, 30000);
                check(rechecked, QStringLiteral("engine recheck: all data present and valid (progress %1)").arg(t->progress()));
                session->removeTorrent(id, BitTorrent::TorrentRemoveOption::KeepContent);
                waitFor([&] { return session->getTorrent(id) == nullptr; }, 10000);
            }
        }
        dialog.reject();
    }

    // ---- settings were saved by the previous dialog ----
    {
        TorrentCreatorDialog dialog(&window, session);
        check(child<QCheckBox>(&dialog, "tcPrivate")->isChecked(), QStringLiteral("saved: private flag"));
        check(child<QComboBox>(&dialog, "tcPieceSize")->currentData().toInt() == 16 * 1024, QStringLiteral("saved: piece size"));
        check(child<QLineEdit>(&dialog, "tcSource")->text() == QStringLiteral("RoTest"), QStringLiteral("saved: source"));
        check(child<QPlainTextEdit>(&dialog, "tcComment")->toPlainText() == QStringLiteral("made by the creator test"), QStringLiteral("saved: comment"));
        check(child<QCheckBox>(&dialog, "tcIgnoreLimits")->isChecked(), QStringLiteral("saved: ignore share limits"));
        check(dialog.inputPath() == Path(root), QStringLiteral("saved: last path"));
        dialog.reject();
    }

    // ---- B: v2-only single file, Auto piece size, dotfiles included for a folder; not seeding ----
    {
        const QString single = QDir(dataDir).filePath(QStringLiteral("single.bin"));
        writeRandom(single, 200 * 1024, 7);
        TorrentCreatorDialog dialog(&window, session);
        Notices notes;
        attach(dialog, notes);
        dialog.setInputPath(Path(single));
        child<QComboBox>(&dialog, "tcFormat")->setCurrentIndex(0);   // V2
        child<QComboBox>(&dialog, "tcPieceSize")->setCurrentIndex(0);   // Auto
        child<QCheckBox>(&dialog, "tcSeed")->setChecked(false);
        check(!child<QCheckBox>(&dialog, "tcIgnoreLimits")->isEnabled(), QStringLiteral("'Ignore share ratio limits' follows the seeding box"));
        child<QCheckBox>(&dialog, "tcSeed")->setChecked(true);
        child<QCheckBox>(&dialog, "tcIgnoreLimits")->setChecked(false);
        child<QCheckBox>(&dialog, "tcPrivate")->setChecked(false);
        child<QPlainTextEdit>(&dialog, "tcTrackers")->clear();
        child<QPlainTextEdit>(&dialog, "tcWebSeeds")->clear();
        child<QPlainTextEdit>(&dialog, "tcComment")->clear();
        child<QLineEdit>(&dialog, "tcSource")->clear();
        const Path dest = Path(outDir) / Path(QStringLiteral("single.torrent"));
        check(createAndWait(dialog, dest) == Outcome::Created, QStringLiteral("v2 single file: created"));
        lt::error_code ec;
        const lt::torrent_info ti(dest.toString().toStdString(), ec);
        check(!ec && ti.info_hashes().has_v2() && !ti.info_hashes().has_v1(), QStringLiteral("v2 only: no v1 hash"));
        {
            // libtorrent pads a v2 file to the piece boundary, so count only the real files
            int real = 0;
            qint64 realSize = 0;
            for (lt::file_index_t i {0}; !ec && (i < ti.files().end_file()); ++i)
            {
                if (ti.files().pad_file_at(i))
                    continue;
                ++real;
                realSize += ti.files().file_size(i);
            }
            check(!ec && (real == 1) && (realSize == 200 * 1024), QStringLiteral("v2: one real file of the right size (%1 file(s), %2 bytes)").arg(real).arg(realSize));
        }
        const auto loaded = BitTorrent::TorrentDescriptor::loadFromFile(dest);
        check(loaded.has_value(), QStringLiteral("v2: RoTorrent's reader accepts it"));
        if (loaded)
        {
            const auto id = BitTorrent::TorrentID::fromInfoHash(loaded->infoHash());
            check(waitFor([&] { return session->getTorrent(id) != nullptr; }, 15000), QStringLiteral("v2: seeding on, torrent is in the session"));
            if (auto *t = session->getTorrent(id))
            {
                check(t->shareLimits().ratioLimit == BitTorrent::DEFAULT_RATIO_LIMIT, QStringLiteral("limits untouched when 'Ignore share ratio limits' is off"));
                t->forceRecheck();
                check(waitFor([t] { return !t->isChecking() && (t->progress() >= 1.0); }, 30000), QStringLiteral("v2: engine recheck verifies the v2 hashes (progress %1)").arg(t->progress()));
                session->removeTorrent(id, BitTorrent::TorrentRemoveOption::KeepContent);
                waitFor([&] { return session->getTorrent(id) == nullptr; }, 10000);
            }
        }
        dialog.reject();
    }

    // ---- C: v1-only, dotfiles kept ----
    {
        TorrentCreatorDialog dialog(&window, session);
        Notices notes;
        attach(dialog, notes);
        dialog.setInputPath(Path(root));
        child<QComboBox>(&dialog, "tcFormat")->setCurrentIndex(2);   // V1
        child<QCheckBox>(&dialog, "tcIgnoreDotfiles")->setChecked(false);
        child<QCheckBox>(&dialog, "tcSeed")->setChecked(false);
        child<QCheckBox>(&dialog, "tcPrivate")->setChecked(false);
        const Path dest = Path(outDir) / Path(QStringLiteral("v1.torrent"));
        check(createAndWait(dialog, dest) == Outcome::Created, QStringLiteral("v1 folder: created"));
        lt::error_code ec;
        const lt::torrent_info ti(dest.toString().toStdString(), ec);
        check(!ec && ti.info_hashes().has_v1() && !ti.info_hashes().has_v2(), QStringLiteral("v1 only: no v2 hash"));
        check(!ec && (ti.files().num_files() == 3), QStringLiteral("dotfile included when 'Ignore dotfiles' is off"));
        check(verifyV1Pieces(dest.toString(), dataDir).isEmpty(), QStringLiteral("v1: piece hashes match the files"));
        const auto v1 = BitTorrent::TorrentDescriptor::loadFromFile(dest);
        check(v1.has_value() && (session->getTorrent(BitTorrent::TorrentID::fromInfoHash(v1->infoHash())) == nullptr)
              , QStringLiteral("seeding off: nothing was added to the session"));
        dialog.reject();
    }

    // ---- D: failures ----
    {
        TorrentCreatorDialog dialog(&window, session);
        Notices notes;
        attach(dialog, notes);
        dialog.setInputPath(Path(QDir(dataDir).filePath(QStringLiteral("does-not-exist"))));
        check(!dialog.startCreation(Path(outDir) / Path(QStringLiteral("x.torrent"))) && (notes.errors == 1)
              , QStringLiteral("unreadable path: refused with an error message"));
        check(!dialog.isCreating(), QStringLiteral("unreadable path: no worker started"));

        dialog.setInputPath(Path(root));
        child<QCheckBox>(&dialog, "tcSeed")->setChecked(false);
        const Path badDest = Path(QDir(dataDir).filePath(QStringLiteral("single.bin/below-a-file/x.torrent")));   // a file is in the way
        notes = {};
        check(createAndWait(dialog, badDest) == Outcome::Failed, QStringLiteral("unwritable destination: reported as failed"));
        check((notes.errors == 1) && !notes.lastError.isEmpty(), QStringLiteral("unwritable destination: error text shown (%1)").arg(notes.lastError));
        check(!dialog.isCreating() && child<QWidget>(&dialog, "tcScroll")->isEnabled(), QStringLiteral("after a failure the form is usable again"));

        // only dotfiles in the folder
        const QString onlyDot = QDir(dataDir).filePath(QStringLiteral("onlydot"));
        writeRandom(onlyDot + QStringLiteral("/.a"), 1024, 9);
        dialog.setInputPath(Path(onlyDot));
        child<QCheckBox>(&dialog, "tcIgnoreDotfiles")->setChecked(true);
        notes = {};
        check(createAndWait(dialog, Path(outDir) / Path(QStringLiteral("dot.torrent"))) == Outcome::Failed, QStringLiteral("everything filtered out: reported as failed"));
        check(!QFile::exists(QDir(outDir).filePath(QStringLiteral("dot.torrent"))), QStringLiteral("no file left behind after a failure"));
        dialog.reject();
    }

    // ---- E: Cancel ----
    {
        const QString big = QDir(dataDir).filePath(QStringLiteral("big.bin"));
        {
            QFile f(big);
            f.open(QIODevice::WriteOnly);
            f.resize(6LL * 1024 * 1024 * 1024);   // sparse: costs no disk, but hashing it takes a few seconds
        }
        TorrentCreatorDialog dialog(&window, session);
        Notices notes;
        attach(dialog, notes);
        dialog.show();
        dialog.setInputPath(Path(big));
        child<QComboBox>(&dialog, "tcFormat")->setCurrentIndex(2);
        child<QComboBox>(&dialog, "tcPieceSize")->setCurrentIndex(1);   // 16 KiB: many progress steps
        child<QCheckBox>(&dialog, "tcSeed")->setChecked(false);
        const Path dest = Path(outDir) / Path(QStringLiteral("big.torrent"));
        struct Count { int count() const { return n; } int n = 0; };
        Count cancelled;
        Count created;
        QObject::connect(&dialog, &TorrentCreatorDialog::cancelled, &dialog, [&cancelled] { ++cancelled.n; });
        QObject::connect(&dialog, &TorrentCreatorDialog::created, &dialog, [&created](const Path &) { ++created.n; });
        check(dialog.startCreation(dest) && dialog.isCreating(), QStringLiteral("big file: creation started"));
        check(!child<QWidget>(&dialog, "tcScroll")->isEnabled() && !child<QPushButton>(&dialog, "tcCreate")->isEnabled()
              , QStringLiteral("while creating the form and Create are locked"));
        check(child<QPushButton>(&dialog, "tcCancel")->isEnabled(), QStringLiteral("Cancel stays available while creating"));
        check(waitFor([&] { return child<QProgressBar>(&dialog, "tcProgress")->value() >= 2; }, 20000), QStringLiteral("real progress is shown (>= 2%)"));
        const int atCancel = child<QProgressBar>(&dialog, "tcProgress")->value();
        child<QPushButton>(&dialog, "tcCancel")->click();
        check(waitFor([&] { return cancelled.count() == 1; }, 20000), QStringLiteral("Cancel stops the creation (was at %1%)").arg(atCancel));
        check((created.count() == 0) && (notes.errors == 0) && (notes.infos == 0), QStringLiteral("cancelled: no success, no error box"));
        check(!dialog.isCreating() && child<QWidget>(&dialog, "tcScroll")->isEnabled() && dialog.isVisible()
              , QStringLiteral("cancelled: form is back and the dialog stays open"));
        check(child<QProgressBar>(&dialog, "tcProgress")->value() == 0, QStringLiteral("cancelled: progress reset"));
        check(!QFile::exists(dest.data()), QStringLiteral("cancelled: no torrent file written"));
        // Cancel when idle closes the dialog
        child<QPushButton>(&dialog, "tcCancel")->click();
        check(!dialog.isVisible(), QStringLiteral("Cancel when idle closes the dialog"));
        QFile::remove(big);
    }

    // ---- destroying the dialog while creating does not hang ----
    {
        const QString big = QDir(dataDir).filePath(QStringLiteral("big2.bin"));
        {
            QFile f(big);
            f.open(QIODevice::WriteOnly);
            f.resize(6LL * 1024 * 1024 * 1024);
        }
        QElapsedTimer timer;
        {
            TorrentCreatorDialog dialog(&window, session);
            Notices notes;
            attach(dialog, notes);
            dialog.setInputPath(Path(big));
            child<QComboBox>(&dialog, "tcFormat")->setCurrentIndex(2);
            child<QComboBox>(&dialog, "tcPieceSize")->setCurrentIndex(1);
            child<QCheckBox>(&dialog, "tcSeed")->setChecked(false);
            dialog.startCreation(Path(outDir) / Path(QStringLiteral("big2.torrent")));
            waitFor([&] { return child<QProgressBar>(&dialog, "tcProgress")->value() >= 1; }, 20000);
            timer.start();
        }   // destroyed while running
        check(timer.elapsed() < 10000, QStringLiteral("destroying the dialog mid-creation stops the worker (%1 ms)").arg(timer.elapsed()));
        QFile::remove(big);
        check(!QFile::exists(QDir(outDir).filePath(QStringLiteral("big2.torrent"))), QStringLiteral("nothing written after destroying mid-creation"));
    }

    // ---- drop zone ----
    {
        TorrentCreatorDialog dialog(&window, session);
        dialog.show();
        auto *zone = dialog.findChild<QWidget *>(QStringLiteral("dropZone"));
        check(zone != nullptr, QStringLiteral("drop zone exists"));
        if (zone)
        {
            QMimeData mime;
            mime.setUrls({QUrl::fromLocalFile(root), QUrl::fromLocalFile(dataDir)});
            QDragEnterEvent enter(QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(zone, &enter);
            check(enter.isAccepted() && zone->property("drag").toBool(), QStringLiteral("dragging over: zone highlighted"));
            if (!shotDir.isEmpty())
                dialog.grab().save(shotDir + QStringLiteral("/creator_drag_dark.png"));
            QDragLeaveEvent leave;
            QApplication::sendEvent(zone, &leave);
            check(!zone->property("drag").toBool(), QStringLiteral("drag leaves: highlight gone"));
            QApplication::sendEvent(zone, &enter);
            QDropEvent drop(QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(zone, &drop);
            check(!zone->property("drag").toBool() && (dialog.inputPath() == Path(root)), QStringLiteral("drop: first path goes into the Path box"));
        }
        // anywhere else in the dialog
        QMimeData mime2;
        mime2.setUrls({QUrl::fromLocalFile(dataDir)});
        QDragEnterEvent enter2(QPoint(5, 5), Qt::CopyAction, &mime2, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&dialog, &enter2);
        QDropEvent drop2(QPoint(5, 5), Qt::CopyAction, &mime2, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&dialog, &drop2);
        check(dialog.inputPath() == Path(dataDir), QStringLiteral("drop on the dialog body also sets the path (%1, accepted %2)").arg(dialog.inputPath().toString()).arg(drop2.isAccepted()));
        dialog.reject();
    }

    // ---- screenshots, both themes ----
    if (!shotDir.isEmpty())
    {
        for (const bool dark : {true, false})
        {
            RoTheme::instance().setDark(dark);
            TorrentCreatorDialog dialog(&window, session);
            dialog.setInputPath(Path(root));
            dialog.show();
            QApplication::processEvents();
            dialog.grab().save(shotDir + QStringLiteral("/creator_%1.png").arg(dark ? QStringLiteral("dark") : QStringLiteral("light")));
            // scrolled to the bottom: the Fields card
            if (auto *sa = dialog.findChild<QScrollArea *>(QStringLiteral("tcScroll")))
                sa->verticalScrollBar()->setValue(sa->verticalScrollBar()->maximum());
            QApplication::processEvents();
            dialog.grab().save(shotDir + QStringLiteral("/creator_%1_fields.png").arg(dark ? QStringLiteral("dark") : QStringLiteral("light")));
            dialog.reject();
        }
        RoTheme::instance().setDark(true);
    }

    std::printf("%s (%d failures)\n", failures ? "FAILED" : "ALL OK", failures);
    return failures ? 1 : 0;
}

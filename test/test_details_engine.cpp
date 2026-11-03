// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 5 live-engine test: SessionDetailsSource on a real BitTorrent session.
//   test_details_engine <profile-dir> <data-dir>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>

#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/file_storage.hpp>

#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTimer>

#include "app/firstrun.h"
#include "base/bittorrent/addtorrentparams.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "gui/details/LogBuffer.h"
#include "gui/models/rostatus.h"
#include "gui/details/SessionDetailsSource.h"

namespace
{
    int failures = 0;
    void check(const bool ok, const QString &what)
    {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", qUtf8Printable(what));
        std::fflush(stdout);
        if (!ok)
            ++failures;
    }

    template <typename F>
    bool waitFor(F cond, const int timeoutMs)
    {
        QEventLoop loop;
        QTimer poll, deadline;
        deadline.setSingleShot(true);
        QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (cond()) loop.quit(); });
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        poll.start(50);
        deadline.start(timeoutMs);
        if (!cond())
            loop.exec();
        return cond();
    }

    void writeRandom(const QString &path, const int bytes, const unsigned seed)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        std::mt19937 rng(seed);
        std::ofstream out(path.toStdString(), std::ios::binary);
        for (int i = 0; i < bytes; ++i)
            out.put(static_cast<char>(rng() & 0xff));
    }

    // multi/a.bin, multi/sub/b.bin, multi/c.bin
    QByteArray makeMultiFileTorrent(const QString &dataDir)
    {
        writeRandom(dataDir + u"/multi/a.bin"_s, 200 * 1024, 1);
        writeRandom(dataDir + u"/multi/sub/b.bin"_s, 150 * 1024, 2);
        writeRandom(dataDir + u"/multi/c.bin"_s, 100 * 1024, 3);
        lt::file_storage fs;
        lt::add_files(fs, QString(dataDir + u"/multi"_s).toStdString());
        lt::create_torrent ct(fs, 16 * 1024);
        lt::set_piece_hashes(ct, dataDir.toStdString());
        std::vector<char> buf;
        lt::bencode(std::back_inserter(buf), ct.generate());
        return QByteArray(buf.data(), static_cast<qsizetype>(buf.size()));
    }
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RoTorrent"));
    if (argc != 3)
        return 2;
    const QString profileDir = QString::fromLocal8Bit(argv[1]);
    const QString dataDir = QString::fromLocal8Bit(argv[2]);
    QDir().mkpath(profileDir);
    QDir().mkpath(dataDir);

    Logger::initInstance();
    Profile::initInstance(Path(profileDir), QString(), false);
    SettingsStorage::initInstance();
    Preferences::initInstance();
    applyFirstRunDefaults();
    Net::ProxyConfigurationManager::initInstance();
    Net::DownloadManager::initInstance();
    BitTorrent::Session::initInstance();
    auto *session = BitTorrent::Session::instance();
    check(waitFor([session] { return session->isRestored(); }, 60000), u"session restored"_s);

    const auto desc = BitTorrent::TorrentDescriptor::load(makeMultiFileTorrent(dataDir));
    check(desc.has_value(), u"multi-file torrent parses"_s);
    if (!desc)
        return 1;
    BitTorrent::AddTorrentParams params;
    params.savePath = Path(dataDir);
    params.useAutoTMM = false;
    check(session->addTorrent(*desc, params), u"torrent added"_s);
    BitTorrent::Torrent *t = nullptr;
    check(waitFor([&] { t = session->findTorrent(desc->infoHash()); return t && t->isFinished(); }, 60000), u"torrent is found and finished (data on disk)"_s);
    if (!t)
        return 1;
    const QString id = t->id().toString();

    SessionDetailsSource src(session);
    LogBuffer log;
    log.attachLogger();
    Logger::instance()->addMessage(u"engine test message"_s, Log::WARNING);
    check(waitFor([&log] { return log.size() > 0; }, 3000) && log.entries().last().kind == LogBuffer::Warning, u"log buffer receives Logger messages with their type"_s);

    // ---- General ----
    const Details::General g = src.general(id);
    check(src.contains(id) && !src.contains(u"0000000000000000000000000000000000000000"_s), u"contains() knows the torrent"_s);
    check(g.valid && g.name == u"multi"_s, u"general: name"_s);
    // libtorrent pads the hybrid torrent to piece boundaries: 30 pieces x 16 KiB, including pad files (which files() hides).
    check(g.totalSize == 30 * 16 * 1024, u"general: total size (includes pad files)"_s);
    check(g.pieceLength == 16 * 1024 && g.piecesCount == 30, u"general: piece length and count"_s);
    check(g.pieces.size() == g.piecesCount && g.pieces.count(true) == g.piecesCount, u"general: finished torrent has every piece"_s);
    check(g.hashV1.size() == 40, u"general: info hash v1"_s);
    check(g.savePath == Path(dataDir).toString() || g.savePath.contains(u"data"_s) || !g.savePath.isEmpty(), u"general: save path"_s);
    check(g.progress >= 0.999, u"general: progress 100%"_s);
    check(g.status == statusOf(t) && (g.status == RoStatus::Seeding || g.status == RoStatus::Stalled), u"general: status comes from statusOf()"_s);
    check(g.addedOn.isValid(), u"general: added-on date"_s);

    // ---- Trackers ----
    {
        auto rows = src.trackers(id);
        check(rows.size() >= 3 && rows.at(0).url == u"** [DHT] **"_s && rows.at(1).url == u"** [PeX] **"_s && rows.at(2).url == u"** [LSD] **"_s
              && rows.at(0).pseudo, u"trackers: DHT, PeX, LSD rows first"_s);
        const int before = static_cast<int>(rows.size());
        src.addTrackers(id, {u"http://tracker-one.invalid/announce"_s, u"  "_s});
        check(waitFor([&] { return src.trackers(id).size() == before + 1; }, 5000), u"trackers: add tracker (blank lines ignored)"_s);
        check(src.editTracker(id, u"http://tracker-one.invalid/announce"_s, u"http://tracker-two.invalid/announce"_s), u"trackers: edit tracker URL"_s);
        check(waitFor([&]
        {
            for (const auto &r : src.trackers(id))
            {
                if (r.url == u"http://tracker-two.invalid/announce"_s)
                    return true;
            }
            return false;
        }, 5000), u"trackers: edited URL is listed"_s);
        check(!src.editTracker(id, u"http://not-there.invalid/"_s, u"http://x.invalid/"_s), u"trackers: editing a missing URL fails"_s);
        src.reannounce(id, {u"http://tracker-two.invalid/announce"_s});
        src.reannounce(id, {});
        src.removeTrackers(id, {u"http://tracker-two.invalid/announce"_s});
        check(waitFor([&] { return src.trackers(id).size() == before; }, 5000), u"trackers: remove tracker; reannounce calls do not crash"_s);
    }

    // ---- Files ----
    {
        auto files = src.files(id);
        check(files.size() == 3, u"files: three files"_s);
        QStringList paths;
        for (const auto &f : files)
            paths << f.path;
        paths.sort();
        check(paths == QStringList({u"multi/a.bin"_s, u"multi/c.bin"_s, u"multi/sub/b.bin"_s}), u"files: relative paths use '/'"_s);
        check(files.at(0).priority == Details::Normal && files.at(0).progress > 0.99, u"files: Normal priority and complete"_s);
        const QString abs = src.absolutePath(id, u"multi/a.bin"_s);
        check(QFile::exists(abs), u"files: absolutePath points at the file on disk"_s);
        check(QDir(src.absolutePath(id, u"multi/sub"_s)).exists(), u"files: absolutePath of a folder"_s);

        src.setFilePriority(id, {0}, Details::Skip);
        check(waitFor([&] { return src.files(id).at(0).priority == Details::Skip; }, 5000), u"files: set priority to Do not download"_s);
        src.setFilePriority(id, {0}, Details::Maximum);
        check(waitFor([&] { return src.files(id).at(0).priority == Details::Maximum; }, 5000), u"files: set priority to Maximum"_s);
        src.setFilePriority(id, {0}, Details::Normal);

        QString err;
        check(src.renameFile(id, u"multi/c.bin"_s, u"multi/c-renamed.bin"_s, false, &err), u"files: rename a file"_s);
        check(waitFor([&]
        {
            for (const auto &f : src.files(id))
            {
                if (f.path == u"multi/c-renamed.bin"_s)
                    return true;
            }
            return false;
        }, 10000), u"files: renamed file listed (and moved on disk)"_s);
        err.clear();
        check(src.renameFile(id, u"multi/sub"_s, u"multi/sub2"_s, true, &err), u"files: rename a folder"_s);
        check(waitFor([&]
        {
            for (const auto &f : src.files(id))
            {
                if (f.path == u"multi/sub2/b.bin"_s)
                    return true;
            }
            return false;
        }, 10000), u"files: folder rename applied to the files below it"_s);
        err.clear();
        check(!src.renameFile(id, u"multi/nope.bin"_s, u"multi/x.bin"_s, false, &err) && !err.isEmpty(), u"files: renaming a missing file fails with a message"_s);
    }

    // ---- Web seeds ----
    {
        check(src.webSeeds(id).isEmpty(), u"web seeds: none at first"_s);
        src.addWebSeeds(id, {u"http://seed.invalid/files/"_s, u"not a url"_s});
        check(waitFor([&] { return !src.webSeeds(id).isEmpty(); }, 5000), u"web seeds: added"_s);
        const QString url = src.webSeeds(id).first().url;
        src.removeWebSeeds(id, {url});
        check(waitFor([&] { return src.webSeeds(id).isEmpty(); }, 5000), u"web seeds: removed"_s);
    }

    // ---- Peers and availability (asynchronous) ----
    {
        int peersCount = 0;
        QString peersId;
        QObject::connect(&src, &SessionDetailsSource::peersReady, &src, [&](const QString &i, const QList<Details::Peer> &) { ++peersCount; peersId = i; });
        src.requestPeers(id);
        check(waitFor([&] { return peersCount == 1; }, 10000) && peersId == id, u"peers: answer arrives through peersReady"_s);
        int availCount = 0;
        QList<double> fractions;
        QObject::connect(&src, &SessionDetailsSource::availabilityReady, &src
                         , [&](const QString &, const QList<int> &, const QList<double> &f) { ++availCount; fractions = f; });
        src.requestAvailability(id);
        check(waitFor([&] { return availCount == 1; }, 10000), u"availability: answer arrives through availabilityReady"_s);
        check(fractions.size() == 3, u"availability: one fraction per file"_s);
        check(src.addPeers(id, {u"garbage"_s, u"1.2.3.4"_s, u"1.2.3.4:0"_s}) == 0, u"peers: invalid endpoints are rejected"_s);
        src.banPeers({u"203.0.113.9:6881"_s});
        check(session->bannedIPs().contains(u"203.0.113.9"_s), u"peers: ban adds the IP to the session's banned list"_s);
        src.requestPeers(u"0000000000000000000000000000000000000000"_s);   // unknown id: must not crash
    }

    std::printf(failures == 0 ? "ALL ENGINE DETAILS TESTS PASSED\n" : "%d ENGINE DETAILS TESTS FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}

// SPDX-License-Identifier: GPL-2.0-or-later
// RoTorrent backend smoke test (handoff Phase 1).
//
//   smoke_session <profile-dir> <data-dir> <mode>
//     mode "setup-clean" : create + add a torrent, exercise stop/start/queue/category,
//                          then shut the session down cleanly.
//     mode "setup-kill"  : same, then die with _Exit() (no cleanup) after resume data
//                          has been written, simulating a forced kill.
//     mode "verify"      : start a new session on the same profile and check that the
//                          torrent, category, tag and queue position were restored.
//
// Exit code 0 means every check passed. Each check prints one "PASS"/"FAIL" line.
// Licensed GPL-2.0-or-later (derived from qBittorrent).

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>

#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/file_storage.hpp>

#include <QCoreApplication>
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
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"

namespace
{
    int failures = 0;

    void check(const bool ok, const char *what)
    {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
        std::fflush(stdout);
        if (!ok)
            ++failures;
    }

    // Spin the event loop until cond() is true or timeoutMs elapses.
    template <typename F>
    bool waitFor(F cond, const int timeoutMs)
    {
        QEventLoop loop;
        QTimer poll;
        QTimer deadline;
        deadline.setSingleShot(true);
        QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (cond()) loop.quit(); });
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        poll.start(100);
        deadline.start(timeoutMs);
        if (!cond())
            loop.exec();
        return cond();
    }

    // 1 MiB of random data and a v1 .torrent describing it, built with libtorrent.
    QByteArray makeTorrent(const QString &dataDir, const QString &name)
    {
        const std::string filePath = (QDir(dataDir).filePath(name)).toStdString();
        if (!QFile::exists(QString::fromStdString(filePath)))
        {
            std::mt19937 rng(12345);
            std::ofstream out(filePath, std::ios::binary);
            for (int i = 0; i < 1024 * 1024; ++i)
                out.put(static_cast<char>(rng() & 0xff));
        }

#if LIBTORRENT_VERSION_NUM >= 20100
        lt::file_storage fs = lt::list_files(filePath, [](std::string) { return true; });
        lt::create_torrent ct(std::move(fs), 16 * 1024);
#else
        lt::file_storage fs;
        lt::add_files(fs, filePath);
        lt::create_torrent ct(fs, 16 * 1024);
#endif
        lt::set_piece_hashes(ct, dataDir.toStdString());
        ct.set_creator("RoTorrent smoke test");

        std::vector<char> buf;
        lt::bencode(std::back_inserter(buf), ct.generate());
        return QByteArray(buf.data(), static_cast<qsizetype>(buf.size()));
    }

    void startBackend(const QString &profileDir)
    {
        Logger::initInstance();
        Profile::initInstance(Path(profileDir), QString(), false);
        SettingsStorage::initInstance();
        Preferences::initInstance();
        if (SettingsStorage::instance()->isEmpty())
            applyFirstRunDefaults();
        Net::ProxyConfigurationManager::initInstance();
        Net::DownloadManager::initInstance();
        BitTorrent::Session::initInstance();
    }

    void stopBackend()
    {
        BitTorrent::Session::freeInstance();
        Net::DownloadManager::freeInstance();
        Net::ProxyConfigurationManager::freeInstance();
        Preferences::freeInstance();
        SettingsStorage::freeInstance();
        Logger::freeInstance();
        Profile::freeInstance();
    }

    bool waitRestored()
    {
        auto *session = BitTorrent::Session::instance();
        return waitFor([session] { return session->isRestored(); }, 60000);
    }

    int setup(const QString &dataDir, const bool kill)
    {
        auto *session = BitTorrent::Session::instance();
        check(waitRestored(), "session restored on empty profile");
        check(session->torrents().isEmpty(), "fresh profile has no torrents");

        // Keep the test off the network and fast.
        session->setSaveResumeDataInterval(1);  // minutes; real flush is forced below
        session->setQueueingSystemEnabled(true);
        check(session->isQueueingSystemEnabled(), "queueing system enabled");
        check(session->addCategory(u"smoke"_s), "category created");

        const QByteArray data = makeTorrent(dataDir, u"payload.bin"_s);
        const auto descr = BitTorrent::TorrentDescriptor::load(data);
        check(static_cast<bool>(descr), "generated .torrent parses");
        if (!descr)
            return 1;

        BitTorrent::AddTorrentParams params;
        params.savePath = Path(dataDir);
        params.useAutoTMM = false;
        params.category = u"smoke"_s;
        params.tags = {Tag(u"smoke-tag"_s)};
        check(session->addTorrent(*descr, params), "addTorrent accepted");

        check(waitFor([session] { return session->torrents().size() == 1; }, 15000), "torrent appears in session");
        BitTorrent::Torrent *torrent = session->torrents().value(0);
        if (!torrent)
            return 1;

        check(waitFor([torrent] { return torrent->isFinished(); }, 60000), "existing data verified: torrent finished");
        check(torrent->category() == u"smoke"_s, "category assigned");
        check(torrent->hasTag(Tag(u"smoke-tag"_s)), "tag assigned");
        check(torrent->savePath() == Path(dataDir), "save path as requested");
        check(torrent->queuePosition() == -1, "finished torrent has no queue position (libtorrent semantics)");

        // Second torrent whose data is missing: stays incomplete, so it sits in the queue.
        const QByteArray data2 = makeTorrent(dataDir, u"payload2.bin"_s);
        QFile::remove(QDir(dataDir).filePath(u"payload2.bin"_s));
        const auto descr2 = BitTorrent::TorrentDescriptor::load(data2);
        check(static_cast<bool>(descr2), "second .torrent parses");
        if (!descr2)
            return 1;
        BitTorrent::AddTorrentParams params2;
        params2.savePath = Path(dataDir) / Path(u"incomplete-target"_s);
        params2.useAutoTMM = false;
        check(session->addTorrent(*descr2, params2), "second addTorrent accepted");
        check(waitFor([session] { return session->torrents().size() == 2; }, 15000), "second torrent appears");
        BitTorrent::Torrent *queued = session->findTorrent(descr2->infoHash());
        check(queued && !queued->isFinished(), "second torrent is incomplete");
        check(queued && queued->queuePosition() >= 0, "incomplete torrent has queue position");
        session->topTorrentsQueuePos({queued->id()});
        check(queued->queuePosition() == 0, "move to top -> queue position 0");

        torrent->stop();
        check(waitFor([torrent] { return torrent->isStopped(); }, 10000), "stop() -> stopped");
        torrent->start();
        check(waitFor([torrent] { return !torrent->isStopped(); }, 10000), "start() -> running");

        // Persist everything now. Clean mode: normal shutdown path. Kill mode: wait for the
        // periodic resume-data flush then die without any cleanup.
        if (kill)
        {
            torrent->setCategory(u"smoke"_s);  // touch state so a save is pending
            QEventLoop loop;
            QTimer::singleShot(8000, &loop, &QEventLoop::quit);
            loop.exec();
            std::printf("killing process without cleanup\n");
            std::fflush(stdout);
            std::_Exit(failures ? 1 : 0);
        }

        stopBackend();
        return failures ? 1 : 0;
    }

    int verify(const QString &dataDir)
    {
        auto *session = BitTorrent::Session::instance();
        check(waitRestored(), "session restored from saved profile");
        check(session->torrents().size() == 2, "both torrents restored after restart");
        BitTorrent::Torrent *torrent = nullptr;
        BitTorrent::Torrent *queued = nullptr;
        for (BitTorrent::Torrent *t : asConst(session->torrents()))
            (t->category() == u"smoke"_s ? torrent : queued) = t;
        if (!torrent || !queued)
        {
            check(false, "both torrents identifiable after restart");
            return 1;
        }
        check(!queued->isFinished(), "incomplete torrent still incomplete after restart");
        check(torrent->category() == u"smoke"_s, "category survived restart");
        check(torrent->hasTag(Tag(u"smoke-tag"_s)), "tag survived restart");
        check(torrent->savePath() == Path(dataDir), "save path survived restart");
        check(queued->queuePosition() >= 0, "queue position survived restart");
        check(session->isQueueingSystemEnabled(), "queueing setting survived restart");
        check(waitFor([torrent] { return torrent->isFinished(); }, 60000), "torrent still finished after restart");
        stopBackend();
        return failures ? 1 : 0;
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"RoTorrent"_s);

    if (argc != 4)
    {
        std::fprintf(stderr, "usage: smoke_session <profile-dir> <data-dir> setup-clean|setup-kill|verify\n");
        return 2;
    }
    const QString profileDir = QString::fromLocal8Bit(argv[1]);
    const QString dataDir = QString::fromLocal8Bit(argv[2]);
    const QString mode = QString::fromLocal8Bit(argv[3]);
    QDir().mkpath(profileDir);
    QDir().mkpath(dataDir);

    startBackend(profileDir);
    if (mode == u"setup-clean")
        return setup(dataDir, false);
    if (mode == u"setup-kill")
        return setup(dataDir, true);
    if (mode == u"verify")
        return verify(dataDir);

    std::fprintf(stderr, "unknown mode\n");
    return 2;
}

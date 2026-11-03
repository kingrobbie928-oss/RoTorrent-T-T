// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 3 live-data test: a real BitTorrent session feeds TransferListModel through SessionAdapter.
//   test_adapter <profile-dir> <data-dir>
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
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "base/utils/misc.h"
#include "gui/models/SessionAdapter.h"
#include "gui/models/TorrentFilterProxy.h"
#include "gui/models/TransferListModel.h"

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

    QByteArray makeTorrent(const QString &dataDir, const QString &name, const bool keepData)
    {
        const std::string filePath = QDir(dataDir).filePath(name).toStdString();
        {
            std::mt19937 rng(static_cast<unsigned>(qHash(name)));
            std::ofstream out(filePath, std::ios::binary);
            for (int i = 0; i < 512 * 1024; ++i)
                out.put(static_cast<char>(rng() & 0xff));
        }
        lt::file_storage fs;
        lt::add_files(fs, filePath);
        lt::create_torrent ct(fs, 16 * 1024);
        lt::set_piece_hashes(ct, dataDir.toStdString());
        std::vector<char> buf;
        lt::bencode(std::back_inserter(buf), ct.generate());
        if (!keepData)
            QFile::remove(QString::fromStdString(filePath));
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
    session->setQueueingSystemEnabled(true);
    session->addCategory(u"Linux ISOs"_s);

    TransferListModel model;
    SessionAdapter adapter(session, &model);
    qint64 downSeen = -1;
    QObject::connect(&adapter, &SessionAdapter::ratesChanged, &model, [&downSeen](qint64 d, qint64) { downSeen = d; });

    // finished torrent (data present) with a category; incomplete torrent (data missing)
    const auto d1 = BitTorrent::TorrentDescriptor::load(makeTorrent(dataDir, u"seeded.bin"_s, true));
    const auto d2 = BitTorrent::TorrentDescriptor::load(makeTorrent(dataDir, u"missing.bin"_s, false));
    check(d1 && d2, u"two generated torrents parse"_s);
    if (!d1 || !d2)
        return 1;

    BitTorrent::AddTorrentParams p1;
    p1.savePath = Path(dataDir);
    p1.useAutoTMM = false;
    p1.category = u"Linux ISOs"_s;
    BitTorrent::AddTorrentParams p2;
    p2.savePath = Path(dataDir) / Path(u"nowhere"_s);
    p2.useAutoTMM = false;
    check(session->addTorrent(*d1, p1) && session->addTorrent(*d2, p2), u"both torrents added"_s);

    check(waitFor([&model] { return model.rowCount() == 2; }, 20000), u"model got two rows from the live session"_s);
    BitTorrent::Torrent *t1 = session->findTorrent(d1->infoHash());
    BitTorrent::Torrent *t2 = session->findTorrent(d2->infoHash());
    check(t1 && t2, u"engine reports both torrents"_s);
    if (!t1 || !t2)
        return 1;

    check(waitFor([&] { return model.row(model.indexOfId(t1->id().toString())).progress >= 100.0; }, 60000), u"finished torrent reaches 100% in the model"_s);
    // Let state settle, then every row's status must equal statusOf(torrent) at that moment.
    waitFor([] { return false; }, 1500);
    bool agree = true;
    for (BitTorrent::Torrent *t : {t1, t2})
    {
        const TorrentRow &r = model.row(model.indexOfId(t->id().toString()));
        agree = agree && (r.status == statusOf(t)) && (r.name == t->name()) && (r.totalSize == t->totalSize());
    }
    check(agree, u"row status, name and size equal the engine's (statusOf is the only place status is decided)"_s);

    const TorrentRow &r1 = model.row(model.indexOfId(t1->id().toString()));
    const TorrentRow &r2 = model.row(model.indexOfId(t2->id().toString()));
    check(r1.category == u"Linux ISOs"_s && r2.category.isEmpty(), u"categories carried into rows"_s);
    check(r1.trackerHost.isEmpty(), u"torrent without trackers is Trackerless"_s);
    check(r1.sizeText == Utils::Misc::friendlyUnit(t1->totalSize()), u"size text uses the engine's formatter"_s);
    check(TransferListModel::queueText(r1) == u"*"_s, u"finished torrent shows * in #"_s);
    check(!r2.finished() && (r2.queuePos >= 0) && (TransferListModel::queueText(r2) == QString::number(r2.queuePos + 1))
          , u"incomplete torrent shows its 1-based queue position"_s);

    // state change through the engine reaches the model
    t1->stop();
    check(waitFor([&] { return model.row(model.indexOfId(t1->id().toString())).status == RoStatus::Completed; }, 15000)
          , u"stopping a finished torrent turns its row Completed"_s);
    t1->start();
    check(waitFor([&] { return model.row(model.indexOfId(t1->id().toString())).status != RoStatus::Completed; }, 15000)
          , u"starting it again leaves Completed"_s);

    // category change reaches the model
    check(session->addCategory(u"Docs"_s), u"second category created"_s);
    t2->setCategory(u"Docs"_s);
    check(waitFor([&] { return model.row(model.indexOfId(t2->id().toString())).category == u"Docs"_s; }, 10000), u"category change reaches the model"_s);

    check(waitFor([&] { return downSeen >= 0; }, 5000), u"session speed updates reach the speed card signal"_s);

    // filtering a live model through the real proxy
    TorrentFilterProxy proxy;
    proxy.setSourceModel(&model);
    RowFilter f;
    f.category = u"Docs"_s;
    proxy.setFilter(f);
    check(proxy.rowCount() == 1, u"category filter over live rows"_s);

    // removal
    session->removeTorrent(t2->id());
    check(waitFor([&model] { return model.rowCount() == 1; }, 15000), u"removing a torrent removes its row"_s);

    BitTorrent::Session::freeInstance();
    Net::DownloadManager::freeInstance();
    Net::ProxyConfigurationManager::freeInstance();
    Preferences::freeInstance();
    SettingsStorage::freeInstance();
    Logger::freeInstance();
    Profile::freeInstance();
    std::printf(failures ? "%d FAILED\n" : "adapter OK\n", failures);
    return failures ? 1 : 0;
}

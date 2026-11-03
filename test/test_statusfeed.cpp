// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 8 live test: status bar, offline banner and toasts driven by a real BitTorrent session.
//   test_statusfeed <profile-dir> <data-dir>
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
#include "base/bittorrent/sessionstatus.h"
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
#include <QPushButton>

#include "gui/GuiSettings.h"
#include "gui/MainWindow.h"
#include "gui/StatusFeed.h"
#include "gui/theme/RoTheme.h"
#include "gui/tray/NotificationFeed.h"
#include "gui/tray/RoTray.h"
#include "gui/widgets/NetBanner.h"
#include "gui/widgets/StatusBar.h"

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
    RoTheme::configureHighDpi();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RoTorrent"));
    QApplication::setQuitOnLastWindowClosed(false);
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
    RoTheme::instance().init();

    MainWindow window;
    auto *tray = new RoTray(&window, true);
    window.setTray(tray);
    new NotificationFeed(session, tray);
    auto *feed = new StatusFeed(session, &window);
    feed->setOfflineGrace(300);
    window.show();
    RoStatusBar *bar = window.statusBar();

    // ---- status bar follows the session ----
    feed->refresh();
    const bool listening = session->isListening();
    const QString conn = bar->connectionText();
    check(listening ? ((conn == u"Connected"_s) || (conn == u"Firewalled"_s)) : (conn == u"Offline"_s)
          , u"connection chip agrees with the session (listening=%1): %2"_s.arg(listening).arg(conn));
    check(bar->dhtVisible() == session->isDHTEnabled(), u"DHT chip follows 'DHT enabled'"_s);
    check(bar->dhtText() == u"%1 DHT nodes"_s.arg(session->status().dhtNodes), u"DHT chip shows the session's node count"_s);
    check(bar->freeSpaceText().startsWith(u"Free space: "_s), u"free space chip is filled: "_s + bar->freeSpaceText());

    session->setGlobalDownloadSpeedLimit(500 * 1024);
    session->setGlobalUploadSpeedLimit(0);
    feed->refresh();
    check(bar->limitChipVisible() && bar->limitText().contains(u"500"_s) && bar->limitText().endsWith(QString(QChar(0x221E)))
          , u"global limit 500 KiB/s down, unlimited up: "_s + bar->limitText());
    session->setAltGlobalDownloadSpeedLimit(100 * 1024);
    session->setAltGlobalUploadSpeedLimit(50 * 1024);
    session->setAltGlobalSpeedLimitEnabled(true);
    waitFor([&] { return bar->limitText().contains(u"100"_s); }, 5000);
    check(bar->limitText().contains(u"100"_s) && bar->limitText().contains(u"50"_s), u"alternative limits take over live: "_s + bar->limitText());
    session->setAltGlobalSpeedLimitEnabled(false);
    session->setGlobalDownloadSpeedLimit(0);
    feed->refresh();
    check(!bar->limitChipVisible(), u"no limits: chip hidden"_s);

    // ---- offline banner (only if the sandbox lets us take the session offline) ----
    const QString oldIface = session->networkInterface();
    session->setNetworkInterface(u"rotorrent-no-such-interface"_s);
    if (waitFor([session] { return !session->isListening(); }, 15000))
    {
        feed->refresh();
        check(bar->connectionText() == u"Offline"_s, u"offline: chip says Offline"_s);
        check(waitFor([feed] { return feed->bannerShown(); }, 5000) && window.banner()->isVisibleTo(&window), u"offline for longer than the grace period: banner shown"_s);
        window.banner()->retryButton()->click();
        check(true, u"Retry re-applies the listen interfaces without crashing"_s);
        session->setNetworkInterface(oldIface);
        check(waitFor([session] { return session->isListening(); }, 20000), u"back online after restoring the interface"_s);
        feed->refresh();
        check(waitFor([&] { feed->refresh(); return !feed->bannerShown(); }, 5000) && !window.banner()->isVisibleTo(&window), u"banner hides when the connection returns"_s);
    }
    else
    {
        std::printf("SKIP  could not take the session offline here; banner covered by the unit test only\n");
        session->setNetworkInterface(oldIface);
    }

    // ---- notifications from real session events ----
    GuiSettings::setNotificationsEnabled(true);
    GuiSettings::setTorrentAddedNotificationsEnabled(true);
    Preferences::instance()->setSystemTrayEnabled(true);
    tray->applySettings();
    const int before = tray->history().size();
    const auto d1 = BitTorrent::TorrentDescriptor::load(makeTorrent(dataDir, u"seeded.bin"_s, true));
    check(bool(d1), u"generated torrent parses"_s);
    if (!d1)
        return 1;
    BitTorrent::AddTorrentParams p;
    p.savePath = Path(dataDir);
    p.useAutoTMM = false;
    check(session->addTorrent(*d1, p), u"torrent added"_s);
    check(waitFor([&] { return tray->history().size() > before; }, 20000), u"adding a torrent raises a 'Torrent added' toast"_s);
    const auto added = tray->history().last();
    check(added.title == u"Torrent added"_s && added.text.contains(u"seeded.bin"_s) && added.kind == NoticeKind::Info, u"toast text names the torrent: "_s + added.text);

    BitTorrent::Torrent *t = session->findTorrent(d1->infoHash());
    check(t != nullptr, u"engine reports the torrent"_s);
    if (t)
    {
        emit session->torrentFinished(t);
        check(tray->history().last().title == u"Download completed"_s && tray->history().last().kind == NoticeKind::Ok
              && tray->history().last().text.contains(u"has finished downloading"_s), u"torrentFinished -> 'Download completed' toast"_s);
        emit session->torrentIOError(t, u"disk is full"_s);
        check(tray->history().last().title == u"I/O Error"_s && tray->history().last().kind == NoticeKind::Crit
              && tray->history().last().text.contains(u"disk is full"_s), u"torrentIOError -> 'I/O Error' toast with the reason"_s);
    }
    for (Toast *toast : tray->toasts())
        toast->dismiss();

    std::printf(failures ? "\n%d FAILED\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}

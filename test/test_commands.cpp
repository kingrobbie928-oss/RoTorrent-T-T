// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 4 integration test: the real MainWindow on a real BitTorrent session (offscreen).
//   test_commands <profile-dir> <data-dir>
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
#include <QMenu>
#include <QMenuBar>
#include <QRegularExpression>
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
#include "gui/MainWindow.h"
#include "gui/actions/TorrentController.h"
#include "gui/chrome/RoHoverIcon.h"
#include "gui/models/SessionAdapter.h"
#include "gui/models/TransferListModel.h"
#include "gui/theme/RoTheme.h"
#include "gui/widgets/MainToolbar.h"
#include "gui/widgets/Sidebar.h"
#include "gui/widgets/TransferListView.h"

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
        poll.start(50);
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
            for (int i = 0; i < 256 * 1024; ++i)
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

    void collectTexts(const QMenu *menu, QStringList &out)
    {
        for (const QAction *a : menu->actions())
        {
            if (a->isSeparator())
                continue;
            out << a->text() << a->toolTip();
            if (a->menu())
                collectTexts(a->menu(), out);
        }
    }

    QAction *find(const QMenu *menu, const QString &text)
    {
        for (QAction *a : menu->actions())
        {
            if (a->text() == text)
                return a;
            if (a->menu())
                if (QAction *inner = find(a->menu(), text))
                    return inner;
        }
        return nullptr;
    }
}

int main(int argc, char **argv)
{
    RoTheme::configureHighDpi();
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
    RoTheme::instance().init();
    auto *session = BitTorrent::Session::instance();
    check(waitFor([session] { return session->isRestored(); }, 60000), u"session restored"_s);

    MainWindow window;
    window.resize(1280, 800);
    auto *adapter = new SessionAdapter(session, window.transferModel(), &window);
    auto *controller = new TorrentController(session, &window);
    window.setController(controller);
    window.show();
    check(!window.queueingEnabled(), u"queueing off at start: queue state follows the engine"_s);

    // ---- data: one finished torrent, three unfinished ones (queue order) ----
    BitTorrent::AddTorrentParams done;
    done.savePath = Path(dataDir);
    done.useAutoTMM = false;
    BitTorrent::AddTorrentParams todo;
    todo.savePath = Path(dataDir) / Path(u"nowhere"_s);
    todo.useAutoTMM = false;
    const auto dFinished = BitTorrent::TorrentDescriptor::load(makeTorrent(dataDir, u"finished.bin"_s, true));
    const auto d1 = BitTorrent::TorrentDescriptor::load(makeTorrent(dataDir, u"u1.bin"_s, false));
    const auto d2 = BitTorrent::TorrentDescriptor::load(makeTorrent(dataDir, u"u2.bin"_s, false));
    const auto d3 = BitTorrent::TorrentDescriptor::load(makeTorrent(dataDir, u"u3.bin"_s, false));
    check(dFinished && d1 && d2 && d3, u"four generated torrents parse"_s);
    if (!(dFinished && d1 && d2 && d3))
        return 1;
    check(session->addTorrent(*dFinished, done) && session->addTorrent(*d1, todo)
          && session->addTorrent(*d2, todo) && session->addTorrent(*d3, todo), u"four torrents added"_s);
    check(waitFor([&] { return window.transferModel()->rowCount() == 4; }, 20000), u"window model shows four rows"_s);

    const QString idF = dFinished->infoHash().toTorrentID().toString();
    const QString id1 = d1->infoHash().toTorrentID().toString();
    const QString id2 = d2->infoHash().toTorrentID().toString();
    const QString id3 = d3->infoHash().toTorrentID().toString();
    BitTorrent::Torrent *tF = session->getTorrent(dFinished->infoHash().toTorrentID());
    BitTorrent::Torrent *t3 = session->getTorrent(d3->infoHash().toTorrentID());
    BitTorrent::Torrent *t1 = session->getTorrent(d1->infoHash().toTorrentID());
    check(tF && t1 && t3, u"engine reports the torrents"_s);
    check(waitFor([&] { return tF->isFinished(); }, 60000), u"the finished torrent finishes checking"_s);
    waitFor([] { return false; }, 800);
    auto rowOf = [&](const QString &id) -> const TorrentRow & { return window.transferModel()->row(window.transferModel()->indexOfId(id)); };
    auto select = [&](const QString &id)
    {
        for (int i = 0; i < window.filterProxy()->rowCount(); ++i)
        {
            const QModelIndex src = window.filterProxy()->mapToSource(window.filterProxy()->index(i, 0));
            if (window.transferModel()->row(src.row()).id == id)
            {
                window.transferView()->selectRow(i);
                break;
            }
        }
        waitFor([] { return false; }, 30);
    };

    // ---- Edit menu / toolbar states follow the selection (upstream rules) ----
    select(idF);
    check(window.selectedIds() == QStringList {idF}, u"selecting a row feeds the commands"_s);
    check(window.action(u"pause"_s)->isEnabled() && !window.action(u"resume"_s)->isEnabled() && window.action(u"remove"_s)->isEnabled()
          , u"running torrent: Pause and Remove enabled, Resume disabled"_s);
    check(!window.action(u"queueTop"_s)->isEnabled(), u"finished torrent: queue actions disabled"_s);

    // ---- Pause / Resume through the real actions ----
    window.action(u"pause"_s)->trigger();
    check(waitFor([&] { return rowOf(idF).status == RoStatus::Completed; }, 15000), u"Pause on a finished torrent turns the row Completed"_s);
    check(waitFor([&] { return window.action(u"resume"_s)->isEnabled(); }, 5000)
          && !window.action(u"pause"_s)->isEnabled(), u"after Pause: Resume enabled, Pause disabled"_s);
    window.action(u"resume"_s)->trigger();
    check(waitFor([&] { return rowOf(idF).status != RoStatus::Completed; }, 15000), u"Resume leaves Completed"_s);

    // ---- Force Resume from the context menu ----
    {
        QMenu *menu = window.buildTorrentMenu({idF});
        check(menu != nullptr, u"context menu exists for a selected torrent"_s);
        QAction *force = find(menu, u"Force Resume"_s);
        check(force != nullptr && force->isEnabled(), u"context menu offers Force Resume"_s);
        if (force)
            force->trigger();
        check(waitFor([&] { return tF->isForced(); }, 10000), u"Force Resume puts the torrent in forced mode"_s);
        delete menu;
        menu = window.buildTorrentMenu({idF});
        check(find(menu, u"Force Resume"_s) == nullptr && find(menu, u"Resume"_s) != nullptr, u"forced torrent: menu offers Resume, not Force Resume"_s);
        delete menu;
        controller->resume({idF});
        check(waitFor([&] { return !tF->isForced(); }, 10000), u"Resume leaves forced mode"_s);
    }

    // ---- Queueing live: off hides every queue control; on shows them ----
    const auto toolbarButton = [&](const QString &tip) -> QWidget *
    {
        for (RoHoverIcon *b : window.toolbar()->findChildren<RoHoverIcon *>())
            if (b->toolTip() == tip)
                return b;
        return nullptr;
    };
    QWidget *topButton = toolbarButton(u"Top of Queue"_s);
    check(topButton != nullptr, u"toolbar has the Top of Queue button"_s);
    check(!topButton->isVisibleTo(window.toolbar()), u"queueing off: toolbar queue group hidden"_s);
    check(window.transferView()->isColumnHidden(TransferListModel::QueuePos), u"queueing off: # column hidden"_s);
    check(!window.action(u"queueTop"_s)->isVisible(), u"queueing off: Edit-menu queue items hidden"_s);
    {
        select(id1);
        QMenu *menu = window.buildTorrentMenu({id1});
        check(find(menu, u"Queue"_s) == nullptr, u"queueing off: no Queue submenu in the context menu"_s);
        delete menu;
    }
    session->setQueueingSystemEnabled(true);
    check(waitFor([&] { return window.queueingEnabled(); }, 5000), u"turning queueing on reaches the window"_s);
    check(!window.transferView()->isColumnHidden(TransferListModel::QueuePos), u"queueing on: # column shown"_s);
    check(window.action(u"queueTop"_s)->isVisible() && topButton->isVisibleTo(window.toolbar()), u"queueing on: Edit-menu items and toolbar group shown"_s);
    {
        select(id1);
        check(window.action(u"queueBottom"_s)->isEnabled() && topButton->isEnabled(), u"unfinished torrent selected: queue controls enabled"_s);
        QMenu *menu = window.buildTorrentMenu({id1});
        check(find(menu, u"Queue"_s) != nullptr && find(menu, u"Move to top"_s) != nullptr, u"queueing on: Queue submenu present"_s);
        delete menu;
        select(idF);
        check(!window.action(u"queueBottom"_s)->isEnabled() && !topButton->isEnabled(), u"finished torrent selected: queue controls disabled"_s);
        menu = window.buildTorrentMenu({idF});
        check(find(menu, u"Queue"_s) == nullptr, u"finished torrent: no Queue submenu"_s);
        delete menu;
    }

    // ---- Queue moves (toolbar signal path and Edit menu) ----
    select(id3);
    const int before = t3->queuePosition();
    window.action(u"queueTop"_s)->trigger();
    check(waitFor([&] { return t3->queuePosition() == 0; }, 5000) && before > 0, u"Top of Queue moves the torrent to position 0"_s);
    window.action(u"queueBottom"_s)->trigger();
    check(waitFor([&] { return t3->queuePosition() == before; }, 5000), u"Bottom of Queue moves it to the last position"_s);
    window.action(u"queueUp"_s)->trigger();
    check(waitFor([&] { return t3->queuePosition() == before - 1; }, 5000), u"Move Up Queue moves it one step"_s);
    window.action(u"queueDown"_s)->trigger();
    check(waitFor([&] { return t3->queuePosition() == before; }, 5000), u"Move Down Queue moves it back"_s);
    check(waitFor([&] { return rowOf(id3).queuePos == t3->queuePosition(); }, 5000), u"the # column follows the engine position"_s);

    // ---- Categories, ATM, flags, rename ----
    check(controller->addCategory(u"Docs"_s, QString()), u"category added through the controller"_s);
    check(waitFor([&] { return controller->categories().contains(u"Docs"_s); }, 2000), u"controller lists the category"_s);
    window.transferModel()->rowsChanged();
    check(waitFor([&] { return !window.sidebar()->tree()->findItems(u"Docs"_s, Qt::MatchExactly | Qt::MatchRecursive).isEmpty(); }, 3000)
          , u"an empty category still shows in the sidebar"_s);
    controller->setCategory({id1}, u"Docs"_s);
    check(waitFor([&] { return rowOf(id1).category == u"Docs"_s; }, 5000), u"category assignment reaches the row"_s);
    {
        select(id1);
        QMenu *menu = window.buildTorrentMenu({id1});
        QAction *docs = nullptr;
        for (QAction *a : menu->actions())
            if (a->menu() && a->text() == u"Category"_s)
                for (QAction *c : a->menu()->actions())
                    if (c->text() == u"Docs"_s)
                        docs = c;
        check(docs && docs->isCheckable() && docs->isChecked(), u"category submenu checks the current category"_s);
        delete menu;
    }
    controller->setAutoTMM({id1}, true);
    check(waitFor([&] { return t1->isAutoTMMEnabled(); }, 5000), u"Automatic Torrent Management switches on"_s);
    controller->setAutoTMM({id1}, false);
    controller->setSequential({id1}, true);
    controller->setFirstLast({id1}, true);
    check(t1->isSequentialDownload() && t1->hasFirstLastPiecePriority(), u"sequential and first/last piece options reach the engine"_s);
    controller->setSequential({idF}, true);
    check(!tF->isSequentialDownload(), u"sequential is ignored for a finished torrent"_s);
    check(controller->rename(id1, u"Renamed torrent"_s), u"rename accepted"_s);
    check(waitFor([&] { return rowOf(id1).name == u"Renamed torrent"_s; }, 5000), u"the new name reaches the row"_s);
    check(!controller->rename(id1, u"   "_s), u"empty name rejected"_s);

    // ---- Copy submenu texts ----
    check(controller->copyText({id1}, TorrentController::CopyKind::Name) == u"Renamed torrent"_s, u"copy Name"_s);
    check(controller->copyText({id1}, TorrentController::CopyKind::InfoHashV1) == d1->infoHash().v1().toString(), u"copy info hash v1"_s);
    check(controller->copyText({id1}, TorrentController::CopyKind::InfoHashV2) == d1->infoHash().v2().toString(), u"copy info hash v2 (hybrid torrent: its v2 hash)"_s);
    check(controller->copyText({id1}, TorrentController::CopyKind::MagnetLink).startsWith(u"magnet:?xt=urn:btih:"_s), u"copy magnet link"_s);
    check(controller->copyText({id1}, TorrentController::CopyKind::TorrentId) == id1, u"copy torrent ID"_s);

    // ---- Export .torrent ----
    {
        const QString out = QDir(dataDir).filePath(u"exported.torrent"_s);
        QString error;
        check(controller->exportTorrent(idF, out, &error) && QFile::exists(out), u"Export .torrent writes a file"_s);
        check(!controller->exportTorrent(idF, dataDir, &error) && !error.isEmpty()
              , u"Export .torrent reports an error for a bad path"_s);
    }

    // ---- Sidebar menus act on every torrent in the filter ----
    {
        RowFilter f;
        f.status = RowFilter::StatusKind::All;
        check(window.idsMatching(f).size() == 4, u"sidebar All filter matches every torrent"_s);
        RowFilter cat;
        cat.category = u"Docs"_s;
        check(window.idsMatching(cat) == QStringList {id1}, u"sidebar category filter matches its torrents"_s);
        QMenu *menu = window.buildSidebarMenu(u"category"_s, u"Docs"_s);
        check(menu && find(menu, u"Add category..."_s) && find(menu, u"Edit category..."_s) && find(menu, u"Remove category"_s)
              && find(menu, u"Remove unused categories"_s) && find(menu, u"Pause torrents"_s), u"category row menu has every entry"_s);
        delete menu;
        menu = window.buildSidebarMenu(u"status"_s, static_cast<int>(RowFilter::StatusKind::Paused));
        check(menu && find(menu, u"Resume torrents"_s) && find(menu, u"Pause torrents"_s) && find(menu, u"Remove torrents"_s)
              && !find(menu, u"Add category..."_s), u"status row menu: Resume / Pause / Remove torrents only"_s);
        delete menu;
        menu = window.buildSidebarMenu(u"tracker"_s, u"tracker.example"_s);
        check(menu && find(menu, u"Copy tracker URL"_s), u"tracker row menu has Copy tracker URL"_s);
        delete menu;
        {
            // Copy tracker URL must give the full announce URL, not just the host.
            BitTorrent::Torrent *t = controller->session()->getTorrent(BitTorrent::TorrentID::fromString(id1));
            check(t != nullptr, u"torrent 1 is reachable for the tracker-URL check"_s);
            if (t)
            {
                t->addTrackers({{u"udp://copy.example.org:6969/announce"_s, 0}, {u"https://copy.example.org/announce"_s, 1}});
                const QString urls = controller->trackerUrlsForHost({id1}, u"copy.example.org"_s);
                check(urls.split(QLatin1Char('\n')).size() == 2 && urls.contains(u"udp://copy.example.org:6969/announce"_s)
                      && urls.contains(u"https://copy.example.org/announce"_s), u"tracker URL copy returns the full announce URLs"_s);
                check(controller->trackerUrlsForHost({id1}, u"no-such-host.example"_s).isEmpty(), u"tracker URL copy: unknown host gives nothing"_s);
            }
        }
        menu = window.buildSidebarMenu(u"empty"_s, {});
        check(menu && find(menu, u"Add category..."_s) && find(menu, u"Remove unused categories"_s) && !find(menu, u"Resume torrents"_s)
              , u"empty-space menu: Add category / Remove unused categories"_s);
        delete menu;
        controller->setCategory({id1}, QString());
        check(waitFor([&] { return rowOf(id1).category.isEmpty(); }, 5000), u"Reset category clears it"_s);
        check(controller->removeUnusedCategories() == 1 && !controller->categories().contains(u"Docs"_s), u"Remove unused categories removes the empty one"_s);
    }

    // ---- Pause Session / Resume Session ----
    {
        QAction *sess = window.action(u"session"_s);
        check(sess->text() == u"Pause Session"_s && sess->shortcut() == QKeySequence(u"Ctrl+Shift+P"_s), u"session action starts as Pause Session (Ctrl+Shift+P)"_s);
        const bool fWasStopped = tF->isStopped();
        sess->trigger();
        check(controller->sessionPaused() && tF->isStopped() && t1->isStopped() && t3->isStopped(), u"Pause Session stops every active torrent"_s);
        check(sess->text() == u"Resume Session"_s && sess->shortcut() == QKeySequence(u"Ctrl+Shift+S"_s), u"the action toggles label and shortcut"_s);
        sess->trigger();
        check(!controller->sessionPaused() && tF->isStopped() == fWasStopped && !t1->isStopped(), u"Resume Session restores the previous states"_s);
        check(sess->text() == u"Pause Session"_s, u"the action toggles back"_s);
    }

    // ---- View toggles and persistence ----
    {
        check(window.toolbar()->isVisible(), u"toolbar visible by default"_s);
        window.action(u"viewToolbar"_s)->setChecked(false);
        check(window.toolbar()->isHidden() && !SettingsStorage::instance()->loadValue<bool>(u"Gui/ShowToolbar"_s, true), u"View > Top Toolbar hides it and persists"_s);
        window.action(u"viewToolbar"_s)->setChecked(true);
        window.action(u"viewSidebar"_s)->setChecked(false);
        check(window.sidebar()->isHidden(), u"View > Filters Sidebar hides the sidebar"_s);
        window.action(u"viewSidebar"_s)->setChecked(true);
        window.action(u"auto_shutdown"_s)->setChecked(true);
        check(Preferences::instance()->shutdownWhenDownloadsComplete() && !Preferences::instance()->shutdownqBTWhenDownloadsComplete()
              , u"On Downloads Done choice is stored in the engine's Preferences (Application::allTorrentsFinished reads them)"_s);
        window.action(u"auto_exit"_s)->setChecked(true);
        check(Preferences::instance()->shutdownqBTWhenDownloadsComplete() && !Preferences::instance()->shutdownWhenDownloadsComplete()
              , u"the radio group is exclusive in Preferences too"_s);
        Preferences::instance()->setShutdownqBTWhenDownloadsComplete(false);
        Preferences::instance()->setRebootWhenDownloadsComplete(true);
        window.syncAutoAction();
        check(window.action(u"auto_reboot"_s)->isChecked(), u"the menu follows the engine's flags (it resets them after acting)"_s);
        window.action(u"auto_nothing"_s)->setChecked(true);
        check(!Preferences::instance()->rebootWhenDownloadsComplete(), u"Do nothing clears every flag"_s);
    }

    // ---- Shortcuts match 8.15 ----
    {
        const struct { const char *key; const char *seq; } shortcuts[] =
        {
            {"addFile", "Ctrl+O"}, {"addLink", "Ctrl+U"}, {"exit", "Ctrl+Q"}, {"resume", "Ctrl+S"}, {"pause", "Ctrl+P"}
            , {"stats", "Ctrl+I"}, {"options", "Alt+O"}, {"docs", "F1"}
        };
        bool all = true;
        for (const auto &s : shortcuts)
            all = all && (window.action(QString::fromLatin1(s.key))->shortcut() == QKeySequence(QString::fromLatin1(s.seq)));
        check(all, u"shortcuts match handoff 8.15"_s);
    }

    // ---- Label audit (Appendix C): no Start / Stop / Stopped / Finished / qBittorrent in any user-visible string ----
    {
        QStringList texts;
        for (const QAction *top : window.titleMenuBar()->actions())
            if (top->menu())
            {
                texts << top->text();
                collectTexts(top->menu(), texts);
            }
        select(idF);
        for (const QString &id : {idF, id1, id3})
            if (QMenu *m = window.buildTorrentMenu({id}))
            {
                collectTexts(m, texts);
                delete m;
            }
        for (const QVariant &p : {QVariant(static_cast<int>(RowFilter::StatusKind::Paused)), QVariant(0)})
            if (QMenu *m = window.buildSidebarMenu(u"status"_s, p))
            {
                collectTexts(m, texts);
                delete m;
            }
        for (RoHoverIcon *b : window.toolbar()->findChildren<RoHoverIcon *>())
            texts << b->toolTip();
        const QRegularExpression bad(u"\\b(Start|Stop|Stopped|Finished|Force Start)\\b|qBittorrent"_s);
        QStringList hits;
        for (const QString &t : std::as_const(texts))
            if (bad.match(t).hasMatch())
                hits << t;
        check(hits.isEmpty(), u"label audit: no Start / Stop / Stopped / Finished / qBittorrent (checked %1 strings)"_s.arg(texts.size()));
        for (const QString &h : std::as_const(hits))
            std::printf("  offending: %s\n", qUtf8Printable(h));
        check(texts.contains(u"Resume"_s) && texts.contains(u"Pause"_s) && texts.contains(u"Pause Session"_s), u"audit sees the Resume / Pause wording"_s);
        check(!texts.contains(u"Tags"_s) && !texts.contains(u"Plugins"_s) && !texts.contains(u"Donate"_s), u"removed features are absent from the menus"_s);
    }

    // ---- Optional screenshots for visual comparison with the kit (ROTORRENT_SHOTS=<dir>) ----
    if (const QString shots = qEnvironmentVariable("ROTORRENT_SHOTS"); !shots.isEmpty())
    {
        QDir().mkpath(shots);
        select(id1);
        waitFor([] { return false; }, 400);
        window.grab().save(shots + u"/main.png"_s);
        if (QMenu *menu = window.buildTorrentMenu({id1}))
        {
            menu->ensurePolished();
            menu->grab().save(shots + u"/torrent_menu.png"_s);
            delete menu;
        }
        if (QMenu *menu = window.buildSidebarMenu(u"category"_s, u"Docs"_s))
        {
            menu->grab().save(shots + u"/sidebar_menu.png"_s);
            delete menu;
        }
        const auto menus = window.titleMenuBar()->actions();
        for (int i = 0; i < menus.size(); ++i)
            if (QMenu *m = menus[i]->menu())
                m->grab().save(shots + u"/menu_%1.png"_s.arg(i));
        Preferences::instance()->setConfirmTorrentDeletion(true);
        QTimer::singleShot(400, [&window, shots]
        {
            if (QWidget *w = QApplication::activeModalWidget())
            {
                w->grab().save(shots + u"/remove_dialog.png"_s);
                w->close();
            }
        });
        window.removeIds({id2});
        QTimer::singleShot(400, [shots]
        {
            if (QWidget *w = QApplication::activeModalWidget())
                w->close();
        });
        waitFor([] { return false; }, 100);
        Preferences::instance()->setConfirmTorrentDeletion(false);
    }

    // ---- Remove (confirmation off so no modal dialog blocks the test) ----
    {
        Preferences::instance()->setConfirmTorrentDeletion(false);
        select(id3);
        window.action(u"remove"_s)->trigger();
        check(waitFor([&] { return window.transferModel()->rowCount() == 3; }, 15000), u"Remove deletes the row and the torrent"_s);
        controller->remove({id2}, false);
        check(waitFor([&] { return window.transferModel()->rowCount() == 2; }, 15000), u"controller remove works for a list of ids"_s);
    }

    delete adapter;
    BitTorrent::Session::freeInstance();
    Net::DownloadManager::freeInstance();
    Net::ProxyConfigurationManager::freeInstance();
    Preferences::freeInstance();
    SettingsStorage::freeInstance();
    Logger::freeInstance();
    Profile::freeInstance();
    std::printf(failures ? "%d FAILED\n" : "commands OK\n", failures);
    return failures ? 1 : 0;
}

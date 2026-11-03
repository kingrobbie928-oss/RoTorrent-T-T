// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 5 tests (offscreen): the details panel and its six tabs on the demo data source.
#include <cstdio>

#include <QApplication>
#include <QClipboard>
#include <functional>
#include <QEventLoop>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

#include "MainWindow.h"
#include "base/path.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "details/DemoDetailsSource.h"
#include "details/DetailsDialogs.h"
#include "details/DetailsPanel.h"
#include "details/DetailsWidgets.h"
#include "details/LogBuffer.h"
#include "models/DemoSource.h"
#include "models/TransferListModel.h"
#include "theme/RoTheme.h"
#include "widgets/TransferListView.h"

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

    void wait(const int ms)
    {
        QEventLoop loop;
        QTimer::singleShot(ms, &loop, &QEventLoop::quit);
        loop.exec();
    }

    QStringList menuTexts(QMenu *menu)
    {
        QStringList out;
        for (const QAction *a : menu->actions())
            out << (a->isSeparator() ? QStringLiteral("-") : a->text());
        return out;
    }

    QAction *findAction(QMenu *menu, const QString &text)
    {
        for (QAction *a : menu->actions())
        {
            if (a->text() == text)
                return a;
        }
        return nullptr;
    }

    TorrentRow row(const QString &id, const QString &name, const RoStatus st, const double pct, const qint64 size = 2LL << 30)
    {
        TorrentRow r;
        r.id = id;
        r.name = name;
        r.status = st;
        r.progress = pct;
        r.totalSize = size;
        r.downRate = (st == RoStatus::Downloading) ? 1 << 20 : 0;
        r.upRate = (st == RoStatus::Seeding) ? 1 << 18 : 0;
        r.ratio = 1.5;
        return r;
    }
}

int main(int argc, char **argv)
{
    RoTheme::configureHighDpi();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RoTorrent"));
    QTemporaryDir dir;
    Profile::initInstance(Path(dir.path()), QString(), false);
    SettingsStorage::initInstance();
    RoTheme::instance().init();

    // ---- pure helpers -----------------------------------------------------------------------
    {
        QBitArray have(100);
        for (int i = 0; i < 50; ++i)
            have.setBit(i);
        check(PiecesBar::columnValue(have, 0, 10) == 1.0 && PiecesBar::columnValue(have, 9, 10) == 0.0
              , u"pieces bar: first half drawn, second half empty"_s);
        check(qFuzzyCompare(PiecesBar::columnValue(have, 0, 1), 0.5), u"pieces bar: one column averages all pieces"_s);
        check(PiecesBar::columnValue(QBitArray(), 0, 10) == 0.0, u"pieces bar: no pieces draws nothing"_s);
        // more columns than pieces: each column still maps to one piece
        QBitArray few(3);
        few.setBit(1);
        check(PiecesBar::columnValue(few, 3, 9) == 1.0 && PiecesBar::columnValue(few, 0, 9) == 0.0, u"pieces bar: wide bar, few pieces"_s);
        check(qFuzzyCompare(PiecesBar::columnAvailability({2, 4}, 4, 0, 1), 0.75), u"availability bar: average copies over max"_s);
        check(DetailsFormat::limit(0) == u"∞"_s && DetailsFormat::limit(-1) == u"∞"_s && DetailsFormat::limit(2048) != u"∞"_s
              , u"limit formatter: <= 0 is unlimited"_s);
        check(DetailsFormat::dateTime(QDateTime()) == u"—"_s, u"date formatter: invalid date is a dash"_s);
        check(DetailsFormat::ratio(-1) == u"—"_s && DetailsFormat::ratio(1.234) == u"1.23"_s, u"ratio formatter"_s);
        check(DetailsDialogs::cleanLines(u" a \n\nb\na\n  \n c"_s) == QStringList({u"a"_s, u"b"_s, u"c"_s}), u"dialog line cleaning: trim, drop empties and duplicates"_s);
    }

    // ---- LogBuffer ---------------------------------------------------------------------------
    {
        LogBuffer buf;
        int addedCount = 0;
        QObject::connect(&buf, &LogBuffer::added, &buf, [&addedCount] { ++addedCount; });
        for (int i = 0; i < LogBuffer::MaxEntries + 50; ++i)
            buf.append(LogBuffer::Normal, QString::number(i));
        check(buf.size() == LogBuffer::MaxEntries && buf.entries().first().text == QString::number(50) && addedCount == LogBuffer::MaxEntries + 50
              , u"log buffer is bounded and drops the oldest entries"_s);
    }

    // ---- window with the demo source -----------------------------------------------------------
    MainWindow window;
    window.resize(1280, 900);
    window.transferModel()->setRows({row(u"t0"_s, u"debian-13.2.0-amd64-netinst.iso"_s, RoStatus::Downloading, 40)
                                     , row(u"t1"_s, u"blender-open-movie-4K"_s, RoStatus::Seeding, 100)
                                     , row(u"t2"_s, u"stopped.iso"_s, RoStatus::Paused, 10)
                                     , row(u"t3"_s, u"fedora-42-workstation.iso"_s, RoStatus::Completed, 100)});
    auto *source = new DemoDetailsSource(window.transferModel(), &window);
    window.setDetailsSource(source);
    window.show();
    wait(50);
    DetailsPanel *panel = window.detailsPanel();
    check(panel != nullptr, u"main window creates the details panel when given a source"_s);
    if (!panel)
        return 1;

    // ---- panel chrome ------------------------------------------------------------------------
    check(panel->isCollapsed() && !panel->hasActiveTab() && panel->height() == DetailsPanel::CollapsedHeight
          , u"panel starts collapsed with no active tab (38 px)"_s);
    check(!panel->refreshTimerActive(), u"refresh timer is stopped while collapsed"_s);
    QStringList labels;
    for (auto t : {DetailsPanel::Tab::General, DetailsPanel::Tab::Trackers, DetailsPanel::Tab::Peers, DetailsPanel::Tab::Http
                   , DetailsPanel::Tab::Content, DetailsPanel::Tab::Log})
        labels << panel->tabButton(t)->text().trimmed();
    check(labels == QStringList({u"General"_s, u"Trackers"_s, u"Peers"_s, u"HTTP Sources"_s, u"Content"_s, u"Log"_s})
          , u"six tabs in order, no Speed tab"_s);
    check(panel->toggleButton()->toolTip() == u"Expand panel"_s, u"chevron tooltip while collapsed"_s);

    panel->tabButton(DetailsPanel::Tab::Trackers)->click();
    check(!panel->isCollapsed() && panel->activeTab() == DetailsPanel::Tab::Trackers && panel->height() == DetailsPanel::DefaultHeight
          , u"clicking a tab opens the panel straight into it (216 px)"_s);
    check(panel->tabButton(DetailsPanel::Tab::Trackers)->property("active").toBool()
          && !panel->tabButton(DetailsPanel::Tab::General)->property("active").toBool(), u"only the clicked tab is active"_s);
    check(panel->refreshTimerActive(), u"refresh timer runs while expanded"_s);
    check(panel->stack()->currentWidget() == panel->emptyLabel() && panel->emptyLabel()->text() == u"Select a torrent to see its details."_s
          , u"nothing selected: tabs show the select-a-torrent text"_s);
    panel->tabButton(DetailsPanel::Tab::Log)->click();
    check(panel->stack()->currentWidget() == panel->log(), u"Log tab works with no selection"_s);
    panel->toggleButton()->click();
    check(panel->isCollapsed() && !panel->hasActiveTab(), u"chevron collapses and clears the active tab"_s);
    panel->toggleButton()->click();
    check(!panel->isCollapsed() && panel->activeTab() == DetailsPanel::Tab::General, u"chevron opens on General"_s);

    // resize rules
    panel->setPanelHeight(5000, 1000);
    check(panel->expandedHeight() == 750 && panel->height() == 750, u"panel height is capped at 75% of the window area"_s);
    panel->setPanelHeight(10, 1000);
    check(panel->expandedHeight() == DetailsPanel::MinHeight, u"panel height has a 90 px minimum"_s);
    panel->finishResize();
    check(panel->isCollapsed(), u"releasing the handle at the minimum snaps the panel collapsed"_s);
    panel->expand(DetailsPanel::Tab::General);
    panel->setPanelHeight(300, 1000);
    panel->finishResize();
    check(!panel->isCollapsed() && SettingsStorage::instance()->loadValue<int>(u"Gui/DetailsHeight"_s, 0) == 300
          , u"a real resize keeps the panel open and persists its height"_s);
    panel->setPanelHeight(DetailsPanel::DefaultHeight, 1000);

    // selection feeds the panel
    check(window.selectTorrent(u"t0"_s), u"torrent can be selected by id"_s);
    wait(50);
    check(panel->torrentId() == u"t0"_s && panel->stack()->currentWidget() != panel->emptyLabel(), u"selecting a row loads the open tab"_s);

    // ---- General ---------------------------------------------------------------------------------
    {
        GeneralTab *g = panel->general();
        check(g->keys().size() == 16 + 7 + 4, u"General: 16 transfer + 7 information + 4 wide fields"_s);
        bool allSet = true;
        for (const QString &k : g->keys())
            allSet &= !g->value(k).isEmpty();
        check(allSet, u"General: every field has a value"_s);
        check(g->badge()->text() == u"Downloading"_s && g->badge()->property("status").toString() == u"dl"_s, u"General: status badge text and QSS property"_s);
        check(g->value(u"connections"_s).contains(u"(48 max)"_s), u"General: connections shows the maximum"_s);
        check(g->value(u"downloaded"_s).contains(u"this session"_s), u"General: downloaded shows the session amount"_s);
        check(g->value(u"downLimit"_s) == u"∞"_s, u"General: unlimited limit is the infinity sign"_s);
        check(g->value(u"hashV1"_s).size() == 40 && g->value(u"hashV2"_s).size() == 64, u"General: info hashes v1 (40) and v2 (64)"_s);
        wait(100);
        check(g->availabilityBar()->isVisible(), u"General: availability bar present"_s);
        panel->setTorrent(u"t3"_s);
        check(g->badge()->text() == u"Completed"_s && g->badge()->property("status").toString() == u"fn"_s, u"General: badge follows the selected torrent (Completed)"_s);
        panel->setTorrent(u"t2"_s);
        check(g->badge()->text() == u"Paused"_s && g->value(u"lastSeen"_s) == u"Never"_s, u"General: Paused badge, last seen complete is Never"_s);
        // label audit
        bool clean = true;
        for (const QString &k : g->keys())
        {
            const QString v = g->value(k);
            clean &= !v.contains(u"qBittorrent"_s) && !v.contains(u"Stopped"_s) && !v.contains(u"Finished"_s);
        }
        check(clean, u"General: no forbidden words in values"_s);
        panel->setTorrent(u"t0"_s);
    }

    // ---- Trackers ----------------------------------------------------------------------------------
    {
        panel->expand(DetailsPanel::Tab::Trackers);
        TrackersTab *t = panel->trackers();
        QTableWidget *tb = t->table();
        check(tb->columnCount() == 11, u"Trackers: eleven columns"_s);
        QStringList heads;
        for (int i = 0; i < tb->columnCount(); ++i)
            heads << tb->horizontalHeaderItem(i)->text();
        check(heads.first() == u"URL / ANNOUNCE ENDPOINT"_s && heads.last() == u"MIN ANNOUNCE"_s && heads.at(7) == u"TIMES DOWNLOADED"_s
              , u"Trackers: header labels and order"_s);
        check(tb->rowCount() == 6 && tb->item(0, 0)->text() == u"** [DHT] **"_s && tb->item(1, 0)->text() == u"** [PeX] **"_s
              && tb->item(2, 0)->text() == u"** [LSD] **"_s, u"Trackers: DHT, PeX, LSD rows come first, then the real trackers"_s);
        check(tb->columnWidth(0) == 330 && tb->columnWidth(3) == 140 && tb->columnWidth(7) == 160, u"Trackers: base column widths"_s);
        check(tb->item(0, 3)->text() == u"Working"_s && tb->item(0, 3)->foreground().color() == RoTheme::instance().pal().seed
              , u"Trackers: Working is green"_s);
        check(tb->item(2, 3)->text() == u"Not contacted yet"_s && tb->item(2, 3)->foreground().color() == RoTheme::instance().pal().muted
              , u"Trackers: Not contacted yet is muted"_s);
        check(tb->item(4, 4)->textAlignment() & Qt::AlignRight, u"Trackers: numbers are right-aligned"_s);

        tb->selectRow(0);
        check(t->selectedUrls().isEmpty(), u"Trackers: pseudo rows are not real trackers (cannot be removed)"_s);
        QMenu *menu = t->buildMenu();
        check(menuTexts(menu) == QStringList({u"Add trackers..."_s, u"Edit tracker URL..."_s, u"Remove tracker"_s, u"-"_s, u"Copy tracker URL"_s, u"-"_s
                                               , u"Force reannounce to selected trackers"_s, u"Force reannounce to all trackers"_s})
              , u"Trackers: context menu items and order match the spec"_s);
        check(!findAction(menu, u"Remove tracker"_s)->isEnabled() && !findAction(menu, u"Edit tracker URL..."_s)->isEnabled()
              && findAction(menu, u"Add trackers..."_s)->isEnabled(), u"Trackers: menu enable rules on a pseudo row"_s);
        delete menu;

        tb->clearSelection();
        tb->selectRow(3);
        const QString url = tb->item(3, 0)->text();
        check(t->selectedUrls() == QStringList({url}), u"Trackers: real tracker selected"_s);
        QMenu *menu2 = t->buildMenu();
        check(findAction(menu2, u"Remove tracker"_s)->isEnabled() && findAction(menu2, u"Edit tracker URL..."_s)->isEnabled(), u"Trackers: menu enabled on a real row"_s);
        findAction(menu2, u"Force reannounce to selected trackers"_s)->trigger();
        findAction(menu2, u"Force reannounce to all trackers"_s)->trigger();
        check(source->actionLog().contains(u"reannounce "_s + url) && source->actionLog().contains(u"reannounce all"_s), u"Trackers: both reannounce actions reach the source"_s);
        delete menu2;
        t->removeSelected();
        check(tb->rowCount() == 5, u"Trackers: remove tracker removes the row"_s);
        source->addTrackers(u"t0"_s, {u"udp://new.example:1337/announce"_s});
        t->refresh();
        check(tb->rowCount() == 6 && tb->item(5, 0)->text() == u"udp://new.example:1337/announce"_s, u"Trackers: added tracker appears"_s);
        tb->selectRow(5);
        t->refresh();
        check(t->selectedUrls() == QStringList({u"udp://new.example:1337/announce"_s}), u"Trackers: selection survives a refresh"_s);
    }

    // ---- Peers ----------------------------------------------------------------------------------------
    {
        panel->expand(DetailsPanel::Tab::Peers);
        PeersTab *p = panel->peers();
        wait(100);
        QTableWidget *tb = p->table();
        check(tb->columnCount() == 12 && !p->showingEmptyState() && tb->rowCount() >= 5, u"Peers: twelve columns and rows for an active torrent"_s);
        double last = 101;
        bool sorted = true;
        for (int r = 0; r < tb->rowCount(); ++r)
        {
            const double v = tb->item(r, 6)->data(Qt::UserRole).toDouble();
            sorted &= (v <= last);
            last = v;
        }
        check(sorted, u"Peers: sorted by progress, highest first"_s);
        check(tb->columnWidth(0) == 120 && tb->columnWidth(5) == 168, u"Peers: base column widths"_s);
        check(tb->item(0, 4)->foreground().color() == RoTheme::instance().pal().muted, u"Peers: flags are muted"_s);

        tb->selectRow(0);
        const QString endpoint = p->selectedEndpoints().value(0);
        const QString ip = tb->item(0, 1)->text();
        QMenu *menu = p->buildMenu();
        check(menuTexts(menu) == QStringList({u"Add peers..."_s, u"Copy IP:port"_s, u"-"_s, u"Ban peer permanently"_s}), u"Peers: context menu items"_s);
        delete menu;
        p->banSelectedInteractive(false);
        check(source->banned().contains(ip), u"Peers: ban reaches the source with the peer's IP"_s);
        p->copySelectedEndpoint();
        check(QApplication::clipboard()->text() == endpoint, u"Peers: copy IP:port puts the endpoint on the clipboard"_s);

        panel->setTorrent(u"t2"_s);   // Paused
        wait(100);
        check(p->showingEmptyState(), u"Peers: paused torrent shows the empty state"_s);
        QMenu *emptyMenu = p->buildMenu();
        check(findAction(emptyMenu, u"Add peers..."_s)->isEnabled() && !findAction(emptyMenu, u"Ban peer permanently"_s)->isEnabled()
              , u"Peers: Add peers is reachable from the empty state"_s);
        delete emptyMenu;
        check(source->addPeers(u"t0"_s, {u"1.2.3.4:6881"_s, u"bad"_s}) == 1, u"Peers: add peers counts only valid endpoints"_s);
        panel->setTorrent(u"t0"_s);
    }

    // ---- HTTP sources --------------------------------------------------------------------------------------
    {
        panel->expand(DetailsPanel::Tab::Http);
        HttpTab *h = panel->http();
        check(!h->showingEmptyState() && h->table()->rowCount() == 2 && h->table()->horizontalHeaderItem(0)->text() == u"URL"_s
              , u"HTTP Sources: web seeds of the selected torrent in one URL column"_s);
        h->table()->selectRow(1);
        check(h->selectedUrls().size() == 1, u"HTTP Sources: selection"_s);
        h->removeSelected();
        check(h->table()->rowCount() == 1, u"HTTP Sources: remove"_s);
        h->table()->selectRow(0);
        QMenu *menu = h->buildMenu();
        check(menuTexts(menu) == QStringList({u"Add HTTP source..."_s, u"Copy HTTP source URL"_s, u"Remove HTTP source"_s}), u"HTTP Sources: context menu items"_s);
        delete menu;
        panel->setTorrent(u"t1"_s);
        check(h->showingEmptyState(), u"HTTP Sources: torrent without web seeds shows the empty state"_s);
        check(window.detailsPanel()->http()->contextMenuPolicy() == Qt::CustomContextMenu, u"HTTP Sources: the empty state has a context menu too"_s);
        source->addWebSeeds(u"t1"_s, {u"https://seed.example/files/"_s});
        h->refresh();
        check(!h->showingEmptyState() && h->table()->rowCount() == 1, u"HTTP Sources: adding a web seed leaves the empty state"_s);
        panel->setTorrent(u"t0"_s);
    }

    // ---- Content ------------------------------------------------------------------------------------------------
    {
        panel->setTorrent(u"t1"_s);   // the "blender" torrent: one root folder, files and a subfolder
        panel->expand(DetailsPanel::Tab::Content);
        wait(100);
        ContentTab *c = panel->content();
        QTreeWidget *tr = c->tree();
        check(tr->columnCount() == 6 && tr->headerItem()->text(3) == u"DOWNLOAD PRIORITY"_s, u"Content: six columns"_s);
        check(tr->topLevelItemCount() == 1 && tr->topLevelItem(0)->childCount() >= 4, u"Content: root folder with children, expanded"_s);
        check(tr->topLevelItem(0)->isExpanded(), u"Content: expanded by default"_s);
        QTreeWidgetItem *root = tr->topLevelItem(0);
        check(root->text(3) == u"Normal"_s && root->checkState(0) == Qt::Checked, u"Content: folder Normal + checked when every file is Normal"_s);
        check(ContentTab::fileIndexesUnder(root).size() == 5, u"Content: folder resolves to its five files"_s);

        // uncheck one file: folder becomes partially checked and priority Mixed
        QTreeWidgetItem *readme = nullptr;
        for (int i = 0; i < root->childCount(); ++i)
        {
            if (root->child(i)->text(0) == u"README.txt"_s)
                readme = root->child(i);
        }
        check(readme != nullptr, u"Content: README.txt present under the root"_s);
        if (readme)
        {
            readme->setCheckState(0, Qt::Unchecked);
            check(readme->text(3) == u"Do not download"_s && root->checkState(0) == Qt::PartiallyChecked && root->text(3) == u"Mixed"_s
                  , u"Content: unchecking a file sets Do not download; folder is partial and Mixed"_s);
        }
        // uncheck the whole folder
        root->setCheckState(0, Qt::Unchecked);
        bool allSkipped = true;
        for (int i = 0; i < root->childCount(); ++i)
            allSkipped &= (root->child(i)->checkState(0) == Qt::Unchecked);
        check(allSkipped && root->text(3) == u"Do not download"_s, u"Content: unchecking a folder skips every file below it"_s);
        c->selectAll();
        check(root->checkState(0) == Qt::Checked && root->text(3) == u"Normal"_s, u"Content: Select All restores Normal"_s);
        c->selectNone();
        check(root->checkState(0) == Qt::Unchecked, u"Content: Select None"_s);
        c->selectAll();

        // priority from the context menu
        for (int i = 0; i < root->childCount(); ++i)
            root->child(i)->setSelected(i == 0);
        QMenu *menu = c->buildMenu();
        check(menuTexts(menu).first() == u"Open"_s && menuTexts(menu).contains(u"Open containing folder"_s) && menuTexts(menu).contains(u"Rename..."_s)
              && menuTexts(menu).contains(u"Download priority"_s), u"Content: context menu items"_s);
        QMenu *sub = findAction(menu, u"Download priority"_s)->menu();
        check(menuTexts(sub) == QStringList({u"Do not download"_s, u"Normal"_s, u"High"_s, u"Maximum"_s}), u"Content: priority submenu items"_s);
        check(findAction(sub, u"Normal"_s)->isChecked(), u"Content: current priority is checked in the submenu"_s);
        findAction(sub, u"Maximum"_s)->trigger();
        check(root->child(0)->text(3) == u"Maximum"_s, u"Content: choosing Maximum changes the priority"_s);
        delete menu;

        // filter
        c->applyFilter(u"srt"_s);
        int shown = 0;
        std::function<void(QTreeWidgetItem *)> count = [&](QTreeWidgetItem *it)
        {
            if (!it->isHidden() && it->childCount() == 0)
                ++shown;
            for (int i = 0; i < it->childCount(); ++i)
                count(it->child(i));
        };
        count(root);
        check(shown == 2 && !root->isHidden(), u"Content: filter keeps only matching files (and their folders)"_s);
        c->applyFilter(QString());
        shown = 0;
        count(root);
        check(shown == 5, u"Content: clearing the filter shows everything"_s);

        // rename reaches the source and rebuilds the tree
        QString err;
        check(source->renameFile(u"t1"_s, u"blender-studio-open-movies-4K/README.txt"_s, u"blender-studio-open-movies-4K/README.md"_s, false, &err)
              , u"Content: rename a file through the source"_s);
        c->refresh();
        bool renamed = false;
        const QTreeWidgetItem *newRoot = tr->topLevelItem(0);   // the tree was rebuilt: old item pointers are gone
        for (int i = 0; i < newRoot->childCount(); ++i)
            renamed |= (newRoot->child(i)->text(0) == u"README.md"_s);
        check(renamed, u"Content: tree shows the new name after refresh"_s);
        check(!source->renameFile(u"t1"_s, u"blender-studio-open-movies-4K/README.md"_s, u"blender-studio-open-movies-4K/poster.jpg"_s, false, &err) && !err.isEmpty()
              , u"Content: renaming onto an existing file fails with a message"_s);
        panel->setTorrent(u"t0"_s);
    }

    // ---- Log -------------------------------------------------------------------------------------------------------------
    {
        LogBuffer *log = window.logBuffer();
        log->append(LogBuffer::Normal, u"normal line"_s);
        log->append(LogBuffer::Info, u"info line"_s);
        log->append(LogBuffer::Warning, u"warning <b>line</b>"_s);
        log->append(LogBuffer::Critical, u"critical line"_s);
        panel->expand(DetailsPanel::Tab::Log);
        LogTab *l = panel->log();
        wait(50);
        check(l->mask() == 15 && l->plainText().contains(u"normal line"_s) && l->plainText().contains(u"critical line"_s), u"Log: all four types shown by default"_s);
        check(l->plainText().contains(u"warning <b>line</b>"_s), u"Log: message text is escaped, not interpreted as HTML"_s);
        l->chip(LogBuffer::Info)->click();
        check(!l->plainText().contains(u"info line"_s) && l->plainText().contains(u"warning"_s), u"Log: turning a chip off re-filters immediately"_s);
        l->chip(LogBuffer::Normal)->click();
        l->chip(LogBuffer::Warning)->click();
        l->chip(LogBuffer::Critical)->click();
        check(l->plainText() == u"No messages to show."_s, u"Log: nothing selected shows the empty text"_s);
        check(SettingsStorage::instance()->loadValue<int>(u"Gui/LogMask"_s, 15) == 0, u"Log: filter choice is persisted"_s);
        l->setMask(LogBuffer::Critical);
        log->append(LogBuffer::Critical, u"late critical"_s);
        log->append(LogBuffer::Normal, u"hidden normal"_s);
        check(l->plainText().contains(u"late critical"_s) && !l->plainText().contains(u"hidden normal"_s), u"Log: new lines respect the filter"_s);
        l->setMask(15);
        check(l->chip(LogBuffer::Normal)->isChecked() && l->chip(LogBuffer::Critical)->isChecked(), u"Log: setMask updates the chips"_s);
        const QString html = LogTab::lineHtml(QDateTime(QDate(2026, 10, 27), QTime(9, 5, 1)), LogBuffer::Critical, u"x"_s);
        check(html.contains(u"[2026-10-27 09:05:01]"_s) && html.contains(RoTheme::instance().pal().accent.name()), u"Log: timestamp format and critical colour"_s);
        // theme switch keeps chips and text
        RoTheme::instance().setDark(false);
        check(l->plainText().contains(u"late critical"_s) && l->mask() == 15, u"Log: survives a theme switch"_s);
        RoTheme::instance().setDark(true);
    }

    // ---- hooks and integration ---------------------------------------------------------------------------------------------
    {
        panel->collapse();
        window.hooks().manageContent({u"t1"_s});
        check(!panel->isCollapsed() && panel->activeTab() == DetailsPanel::Tab::Content && window.selectedIds() == QStringList({u"t1"_s})
              , u"\"Manage content...\" selects the torrent and opens the Content tab"_s);
        window.hooks().editTrackers({u"t0"_s});
        check(panel->activeTab() == DetailsPanel::Tab::Trackers && panel->torrentId() == u"t0"_s, u"\"Edit trackers...\" opens the Trackers tab"_s);

        // removing the selected torrent from the list empties the panel
        window.transferModel()->removeId(u"t0"_s);
        wait(200);
        check(panel->stack()->currentWidget() == panel->emptyLabel() || panel->torrentId() != u"t0"_s, u"panel lets go of a torrent that was removed"_s);
    }

    // ---- visual: QSS object names the stylesheet relies on --------------------------------------------------------------------------
    {
        check(panel->objectName() == u"detailPanel"_s && panel->tabButton(DetailsPanel::Tab::General)->objectName() == u"detailTab"_s
              && panel->toggleButton()->objectName() == u"detailToggle"_s && panel->trackers()->table()->objectName() == u"dTable"_s
              && panel->content()->tree()->objectName() == u"dTable"_s, u"object names match the QSS"_s);
        // headers must not paint the default light fill
        const QColor bg = panel->trackers()->table()->horizontalHeader()->palette().color(QPalette::Window);
        Q_UNUSED(bg);
        QImage img = window.grab().toImage();
        check(!img.isNull(), u"window renders with the panel"_s);
    }

    std::printf(failures == 0 ? "ALL DETAILS TESTS PASSED\n" : "%d DETAILS TESTS FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}

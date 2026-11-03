// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 3 tests (offscreen): filter/sort rules, sidebar counts == rows shown, empty states,
// column rules, animation clock, and performance with 500 rows.
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif
#include <cstdio>

#include <QApplication>
#include <QEventLoop>
#include <QHeaderView>
#include <QTimer>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTreeWidgetItem>

#include "MainWindow.h"
#include "base/path.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "models/DemoSource.h"
#include "models/TorrentFilterProxy.h"
#include "models/TransferListModel.h"
#include "theme/RoTheme.h"
#include "widgets/RoSearchBox.h"
#include "widgets/Sidebar.h"
#include "widgets/TorrentDelegate.h"
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

    using Clock = std::chrono::steady_clock;
    double msSince(const Clock::time_point t) { return std::chrono::duration<double, std::milli>(Clock::now() - t).count(); }

    // QApplication::quit() closes every window, so waits use a private event loop instead.
    void wait(const int ms)
    {
        QEventLoop loop;
        QTimer::singleShot(ms, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // CPU time used by this process so far, in ms.
    double cpuMs()
    {
#ifdef _WIN32
        // MSVC's std::clock() is wall-clock time, not CPU time; use the real process CPU time.
        FILETIME c, e, k, u;
        if (GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u))
        {
            const auto ticks = [](const FILETIME &f) { return static_cast<double>((static_cast<unsigned long long>(f.dwHighDateTime) << 32) | f.dwLowDateTime); };
            return (ticks(k) + ticks(u)) / 10000.0;   // 100 ns units -> ms
        }
#endif
        return 1000.0 * static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
    }

    TorrentRow row(const QString &id, const QString &name, const RoStatus st, const double pct
                   , const QString &cat = {}, const QString &tracker = {})
    {
        TorrentRow r;
        r.id = id;
        r.name = name;
        r.status = st;
        r.progress = pct;
        r.category = cat;
        r.trackerHost = tracker;
        r.sizeText = QStringLiteral("1 GB");
        r.downText = r.upText = QStringLiteral("0 KB/s");
        r.etaText = QStringLiteral("\u2014");
        r.ratioText = QStringLiteral("0.00");
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

    // ---- filter and sort rules on a small hand-made set -------------------------------------
    {
        TransferListModel model;
        TorrentFilterProxy proxy;
        proxy.setSourceModel(&model);
        model.setRows({row(u"1"_s, u"Alpha Linux.iso"_s, RoStatus::Downloading, 40, u"ISOs"_s, u"t1.org"_s)
                       , row(u"2"_s, u"beta-media.mkv"_s, RoStatus::Seeding, 100, u"Media"_s, u"t1.org"_s)
                       , row(u"3"_s, u"gamma data.zip"_s, RoStatus::Paused, 10, QString(), QString())
                       , row(u"4"_s, u"delta.iso"_s, RoStatus::MissingFiles, 50, u"ISOs"_s, u"t2.org"_s)
                       , row(u"5"_s, u"epsilon.iso"_s, RoStatus::Error, 5)
                       , row(u"6"_s, u"zeta.bin"_s, RoStatus::Completed, 100, u"Media"_s)});
        check(proxy.rowCount() == 6, u"no filter shows every row"_s);

        RowFilter f;
        f.status = RowFilter::StatusKind::Error;
        proxy.setFilter(f);
        check(proxy.rowCount() == 2, u"Error filter includes Missing Files rows"_s);
        f = RowFilter();
        f.status = RowFilter::StatusKind::Completed;
        proxy.setFilter(f);
        check(proxy.rowCount() == 1, u"Completed filter shows only Completed (not Seeding)"_s);
        f = RowFilter();
        f.category = QString();
        proxy.setFilter(f);
        check(proxy.rowCount() == 2, u"Uncategorized = rows with no category"_s);
        f = RowFilter();
        f.tracker = QString();
        proxy.setFilter(f);
        check(proxy.rowCount() == 3, u"Trackerless = rows with no tracker"_s);
        f = RowFilter();
        f.category = u"ISOs"_s;
        f.status = RowFilter::StatusKind::Downloading;
        proxy.setFilter(f);
        check(proxy.rowCount() == 1, u"category AND status combine"_s);
        f = RowFilter();
        proxy.setFilter(f);
        proxy.setSearch(u"  linux ALPHA "_s);
        check(proxy.rowCount() == 1, u"search is case-insensitive, trimmed, every term must match"_s);
        proxy.setSearch(u"nonsense"_s);
        check(proxy.rowCount() == 0, u"search with no hit shows nothing"_s);
        proxy.setSearch(QString());

        proxy.sort(TransferListModel::Name, Qt::AscendingOrder);
        check(proxy.index(0, 0).data(TransferListModel::RowRole).value<const TorrentRow *>()->id == u"1"_s, u"sort by name ascending"_s);
        proxy.sort(TransferListModel::QueuePos, Qt::AscendingOrder);
        const TorrentRow *last = proxy.index(proxy.rowCount() - 1, 0).data(TransferListModel::RowRole).value<const TorrentRow *>();
        check(last->finished(), u"queue sort puts finished (*) torrents last"_s);
        check(TransferListModel::queueText(row(u"x"_s, u"x"_s, RoStatus::Seeding, 100)) == u"*"_s, u"finished torrent shows * in the # column"_s);
        TorrentRow q = row(u"y"_s, u"y"_s, RoStatus::Queued, 0);
        q.queuePos = 2;
        check(TransferListModel::queueText(q) == u"3"_s, u"queue position is shown 1-based"_s);

        // in-place update keeps the row and emits one change
        TorrentRow upd = model.row(0);
        upd.progress = 77;
        model.upsert(upd);
        check(model.rowCount() == 6 && model.row(model.indexOfId(upd.id)).progress == 77, u"upsert updates by id without adding a row"_s);
        model.removeId(u"3"_s);
        check(model.rowCount() == 5 && model.indexOfId(u"3"_s) < 0, u"removeId removes exactly that row"_s);
    }

    // ---- sidebar counts equal the rows the same filter shows ----------------------------------
    {
        MainWindow w;
        w.resize(1280, 800);
        w.show();
        w.transferModel()->setRows(DemoSource::generate(240, 11));
        w.setQueueingEnabled(true);
        QApplication::processEvents();
        w.sidebar()->refreshCounts();

        int checked = 0;
        bool allEqual = true;
        QString firstBad;
        QTreeWidget *tree = w.sidebar()->tree();
        for (int s = 0; s < tree->topLevelItemCount(); ++s)
        {
            QTreeWidgetItem *section = tree->topLevelItem(s);
            for (int i = 0; i < section->childCount(); ++i)
            {
                QTreeWidgetItem *it = section->child(i);
                RowFilter f;
                const QString group = it->data(0, SideDelegate::GroupRole).toString();
                const QVariant payload = it->data(0, SideDelegate::PayloadRole);
                if (group == u"status")
                    f.status = static_cast<RowFilter::StatusKind>(payload.toInt());
                else if (group == u"category")
                    f.category = payload.toString();
                else
                    f.tracker = payload.toString();
                w.filterProxy()->setFilter(f);
                ++checked;
                if (w.filterProxy()->rowCount() != it->data(0, SideDelegate::CountRole).toInt())
                {
                    allEqual = false;
                    if (firstBad.isEmpty())
                        firstBad = it->text(0);
                }
            }
        }
        check(checked >= 14, QStringLiteral("checked %1 sidebar rows (status, categories, trackers)").arg(checked));
        check(allEqual, QStringLiteral("every sidebar count equals the rows its filter shows %1").arg(firstBad));
        check(tree->topLevelItem(0)->childCount() == 9, u"STATUS has the nine rows"_s);
        check(w.sidebar()->tree()->topLevelItemCount() == 3, u"three sections, no Tags section"_s);
        check(w.sidebar()->tree()->topLevelItem(2)->data(0, SideDelegate::CollapsedRole).toBool(), u"TRACKERS starts collapsed"_s);
        check(!w.sidebar()->tree()->topLevelItem(0)->data(0, SideDelegate::CollapsedRole).toBool(), u"STATUS starts expanded"_s);

        // status counts partition the list: the status rows (without All and Error-merging) add up
        int sum = 0;
        for (int i = 1; i < 9; ++i)
            sum += tree->topLevelItem(0)->child(i)->data(0, SideDelegate::CountRole).toInt();
        check(sum == w.transferModel()->rowCount(), u"the eight status rows add up to the total (statusOf is a partition)"_s);

        // section collapse is wired to a click
        emit tree->itemClicked(tree->topLevelItem(2), 0);
        check(!tree->topLevelItem(2)->data(0, SideDelegate::CollapsedRole).toBool() && tree->topLevelItem(2)->isExpanded(), u"clicking a section header expands it"_s);

        // clicking a status row filters the table and keeps the search text
        w.sidebar()->resetFilter();
        w.searchBox()->setText(u"distro-1"_s);
        QApplication::processEvents();
        wait(250);
        const int searched = w.filterProxy()->rowCount();
        check(searched > 0 && searched < 240, QStringLiteral("search narrows the list (%1 of 240)").arg(searched));

        // ---- empty states ------------------------------------------------------------------------
        w.searchBox()->setText(u"zzzz-no-such-torrent"_s);
        wait(250);
        std::printf("INFO empty: visible=%d title='%s' proxyRows=%d total=%d search='%s' viewportVisible=%d geom=%dx%d\n"
                    , w.transferView()->emptyState()->isVisible(), qUtf8Printable(w.transferView()->emptyState()->titleText())
                    , w.filterProxy()->rowCount(), w.transferModel()->rowCount(), qUtf8Printable(w.filterProxy()->filter().search)
                    , w.transferView()->viewport()->isVisible(), w.transferView()->emptyState()->width(), w.transferView()->emptyState()->height());
        check(w.transferView()->emptyState()->isVisible() && w.transferView()->emptyState()->titleText().contains(u"No results for"_s)
              , u"no search results -> 'No results for' state"_s);
        w.searchBox()->clear();
        QApplication::processEvents();
        check(!w.transferView()->emptyState()->isVisible(), u"clearing the search hides the empty state"_s);

        w.transferModel()->setRows({row(u"1"_s, u"only.iso"_s, RoStatus::Seeding, 100)});
        w.sidebar()->refreshCounts();
        QTreeWidgetItem *errorItem = tree->topLevelItem(0)->child(8);
        emit tree->itemClicked(errorItem, 0);
        QApplication::processEvents();
        check(w.transferView()->emptyState()->isVisible() && w.transferView()->emptyState()->titleText().contains(u"No torrents in Error"_s)
              , u"empty filter -> 'No torrents in Error' state"_s);
        w.sidebar()->resetFilter();
        w.transferModel()->setRows({});
        wait(300);   // the empty state follows model changes through a 120 ms debounce
        check(w.transferView()->emptyState()->isVisible() && w.transferView()->emptyState()->titleText() == u"No torrents yet"_s
              , u"no torrents at all -> 'No torrents yet' state"_s);
        w.close();
    }

    // ---- columns, row geometry, animation clock ---------------------------------------------------
    {
        MainWindow w;
        w.resize(1280, 800);
        w.setQueueingEnabled(true);
        w.transferModel()->setRows(DemoSource::fromJson(QStringLiteral(ROTORRENT_TEST_DATA "/torrents.json")));
        w.show();
        wait(250);
        TransferListView *v = w.transferView();
        v->rescaleNow();

        check(v->viewport()->width() == 1070, QStringLiteral("table viewport is 1070 px wide at 1280 px window (got %1)").arg(v->viewport()->width()));
        check(v->columnWidth(TransferListModel::Progress) == 150 && v->columnWidth(TransferListModel::Status) == 150
              && v->columnWidth(TransferListModel::Down) == 110 && v->columnWidth(TransferListModel::Up) == 110
              && v->columnWidth(TransferListModel::Eta) == 100 && v->columnWidth(TransferListModel::Ratio) == 90
              , u"column widths equal the reference base widths at 1070 px"_s);
        check(v->columnWidth(TransferListModel::QueuePos) == 48, u"# column is 48 px"_s);
        check(v->horizontalHeader()->visualIndex(TransferListModel::QueuePos) == 0, u"# is the first column"_s);
        check(v->rowHeight(0) >= 53, u"rows are at least 53 px"_s);
        int sumW = 0;
        for (int c = 0; c < TransferListModel::ColumnCount; ++c)
            sumW += v->columnWidth(c);
        check(sumW == v->viewport()->width(), u"columns fill the viewport exactly at the reference width"_s);

        w.setQueueingEnabled(false);
        v->rescaleNow();
        check(v->isColumnHidden(TransferListModel::QueuePos), u"# is hidden when queueing is off"_s);
        check(v->columnWidth(TransferListModel::Name) > 300, u"Name takes the freed width"_s);

        w.resize(700, 600);
        wait(250);
        v->rescaleNow();
        check(v->columnWidth(TransferListModel::Progress) >= 142, u"Progress never narrower than its 142 px minimum"_s);
        const bool scrolls = (v->horizontalScrollBar()->maximum() > 0) || (v->columnWidth(TransferListModel::Name) >= 100);
        check(scrolls, u"narrow window: columns shrink to their minimums, then the table scrolls sideways"_s);
        std::printf("INFO narrow: window=%dx%d viewport=%d nameCol=%d row0=%d row1=%d\n", w.width(), w.height(), v->viewport()->width()
                    , v->columnWidth(TransferListModel::Name), v->rowHeight(0), v->rowHeight(1));
        bool wrapped = false;
        for (int r = 0; r < v->model()->rowCount(); ++r)
            wrapped = wrapped || (v->rowHeight(r) > 53);
        check(wrapped, u"narrow window: long names wrap and rows grow instead of clipping"_s);
        w.resize(1280, 800);
        w.setQueueingEnabled(true);
        wait(250);

        // the demo data has Downloading and Checking rows in view -> clock runs
        check(v->animationRunning(), u"animation clock runs while a visible row animates"_s);
        // a list with nothing animated -> clock stops
        w.transferModel()->setRows({row(u"1"_s, u"a.iso"_s, RoStatus::Seeding, 100), row(u"2"_s, u"b.iso"_s, RoStatus::Paused, 20)});
        wait(250);
        check(!v->animationRunning(), u"animation clock stops when no visible row needs it"_s);
        w.close();
    }

    // ---- performance with 500 rows --------------------------------------------------------------
    {
        MainWindow w;
        w.resize(1280, 800);
        w.setQueueingEnabled(true);
        w.show();
        QApplication::processEvents();

        auto t = Clock::now();
        w.transferModel()->setRows(DemoSource::generate(500, 3));
        wait(400);
        const double loadMs = msSince(t);
        check(loadMs < 1500, QStringLiteral("500 rows load, lay out and render in %1 ms (< 1500)").arg(loadMs, 0, 'f', 0));

        TransferListView *v = w.transferView();
        // Scroll through the whole list in steps and time a full viewport repaint at each stop.
        const int steps = 60;
        double total = 0;
        double worst = 0;
        const int maxScroll = v->verticalScrollBar()->maximum();
        for (int i = 0; i < steps; ++i)
        {
            v->verticalScrollBar()->setValue(maxScroll * i / (steps - 1));
            auto f = Clock::now();
            QPixmap frame = v->viewport()->grab();
            const double ms = msSince(f);
            total += ms;
            worst = qMax(worst, ms);
            Q_UNUSED(frame);
        }
        check(total / steps < 16.0, QStringLiteral("scrolling 500 rows: average %1 ms per full-viewport frame (< 16)").arg(total / steps, 0, 'f', 1));
        check(worst < 60.0, QStringLiteral("scrolling 500 rows: worst frame %1 ms (< 60)").arg(worst, 0, 'f', 1));

        // Idle CPU. First with nothing animating (all rows Paused): near zero.
        QList<TorrentRow> quiet = DemoSource::generate(500, 3);
        for (TorrentRow &r : quiet)
        {
            if ((r.status == RoStatus::Downloading) || (r.status == RoStatus::Checking))
                r.status = RoStatus::Paused;
        }
        w.transferModel()->setRows(quiet);
        wait(400);
        const double c0 = cpuMs();
        const auto w0 = Clock::now();
        wait(2000);
        const double idlePct = 100.0 * (cpuMs() - c0) / msSince(w0);
        check(!v->animationRunning(), u"idle list with nothing animated: clock is stopped"_s);
        check(idlePct < 3.0, QStringLiteral("idle CPU with nothing animating: %1%% (< 3)").arg(idlePct, 0, 'f', 1));

        // Then with animated rows on screen: only visible animated rows repaint.
        w.transferModel()->setRows(DemoSource::generate(500, 3));
        v->verticalScrollBar()->setValue(0);
        wait(400);
        const double c1 = cpuMs();
        const auto w1 = Clock::now();
        wait(2000);
        const double animPct = 100.0 * (cpuMs() - c1) / msSince(w1);
        check(v->animationRunning(), u"animated rows on screen: clock is running"_s);
        check(animPct < 35.0, QStringLiteral("CPU while animating visible rows (offscreen, software rendering): %1%% (< 35)").arg(animPct, 0, 'f', 1));
        std::printf("INFO idle %.1f%%, animating %.1f%%, load %.0f ms, avg frame %.1f ms\n", idlePct, animPct, loadMs, total / steps);
        w.close();
    }

    SettingsStorage::freeInstance();
    Profile::freeInstance();
    std::printf(failures ? "%d FAILED\n" : "transferlist OK\n", failures);
    return failures ? 1 : 0;
}

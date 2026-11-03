// SPDX-License-Identifier: GPL-2.0-or-later
// The torrent table (handoff 8.7): fixed columns with responsive widths, rows that grow instead of
// clipping, an animation clock that runs only while a visible row needs it, hover repaint, and the
// empty-state overlay. objectName "torrentTable" is what the QSS targets.
#pragma once

#include <QElapsedTimer>
#include <QTableView>
#include <QTimer>

#include "EmptyState.h"
#include "TorrentDelegate.h"
#include "models/TorrentFilterProxy.h"
#include "models/TransferListModel.h"

class TransferListView final : public QTableView
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TransferListView)

public:
    // Column widths at the reference viewport (PROGRESS, STATUS, DOWN, UP, ETA, RATIO), measured
    // against the html at 1070 px = 1280 px window minus the 210 px sidebar.
    static constexpr int COL_BASE[6] = {150, 150, 110, 110, 100, 90};
    static constexpr int BASE_VIEWPORT = 1070;
    static constexpr int QUEUE_COLUMN_WIDTH = 48;

    TransferListView(TransferListModel *source, TorrentFilterProxy *proxy, QWidget *parent = nullptr);

    TorrentDelegate *rowDelegate() const { return m_delegate; }
    EmptyState *emptyState() const { return m_empty; }

    void setQueueColumnVisible(bool on);            // "#" is shown only while queueing is on
    bool animationRunning() const { return m_timer.isActive(); }
    QString selectedId() const;
    QStringList selectedIds() const;               // all selected rows (single selection today)

    // Recompute column widths and row heights now (normally debounced).
    void rescaleNow();

    // Decides which empty state (if any) to show. sideLabel is the active sidebar row's name.
    void updateEmptyState(const QString &sideLabel);

signals:
    void selectedIdChanged(const QString &id);   // empty when nothing is selected
    void torrentActivated(const QString &id);    // double-click on a row (Options > Behavior picks the action)
    void addTorrentRequested();
    void addLinkRequested();
    void clearSearchRequested();
    void showAllRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool viewportEvent(QEvent *event) override;

private:
    void scheduleRescale();
    void invalidateMins();
    QList<qreal> minContent();
    void refreshAnimation();
    void tick();

    TransferListModel *m_source;
    TorrentFilterProxy *m_proxy;
    TorrentDelegate *m_delegate;
    EmptyState *m_empty;
    QTimer m_timer;
    QTimer m_rescaleTimer;
    QElapsedTimer m_clock;
    QList<qreal> m_mins;
    bool m_minsDirty = true;
    bool m_queueVisible = false;
    int m_hoverRow = -1;
    int m_lastViewportWidth = -1;
    double m_time = 0;
};

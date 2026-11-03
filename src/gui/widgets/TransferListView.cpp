// SPDX-License-Identifier: GPL-2.0-or-later
#include "TransferListView.h"

#include <QHeaderView>
#include <QMouseEvent>
#include <QRegion>
#include <QScrollBar>

#include "RoPaint.h"
#include "theme/RoTheme.h"

constexpr int TransferListView::COL_BASE[6];

TransferListView::TransferListView(TransferListModel *source, TorrentFilterProxy *proxy, QWidget *parent)
    : QTableView(parent)
    , m_source(source)
    , m_proxy(proxy)
    , m_delegate(new TorrentDelegate(this))
{
    setObjectName(QStringLiteral("torrentTable"));
    setModel(proxy);
    connect(this, &QAbstractItemView::doubleClicked, this, [this]
    {
        if (const QString id = selectedId(); !id.isEmpty())
            emit torrentActivated(id);
    });
    setItemDelegate(m_delegate);
    verticalHeader()->hide();
    setShowGrid(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setFrameShape(QFrame::NoFrame);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setEditTriggers(NoEditTriggers);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QHeaderView *hh = horizontalHeader();
    QFont hf = RoPaint::font(10.5, 600);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 0.63);
    hh->setFont(hf);
    hh->setHighlightSections(false);
    hh->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    hh->setStretchLastSection(false);
    hh->setSectionResizeMode(QHeaderView::Fixed);   // widths come from rescaleNow(); the table scrolls sideways if it cannot fit
    hh->setSectionsClickable(true);
    for (int c = 0; c < 6; ++c)
        setColumnWidth(c + 1, COL_BASE[c]);
    setColumnWidth(TransferListModel::QueuePos, QUEUE_COLUMN_WIDTH);
    hh->moveSection(TransferListModel::QueuePos, 0);   // "#" is the first column, as in qBittorrent
    setColumnHidden(TransferListModel::QueuePos, true);

    verticalHeader()->setDefaultSectionSize(53);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);   // heights are set by rescaleNow(): rows grow when text wraps

    setSortingEnabled(true);
    hh->setSortIndicator(-1, Qt::AscendingOrder);

    m_empty = new EmptyState(viewport());

    m_timer.setInterval(40);
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &TransferListView::tick);
    m_rescaleTimer.setSingleShot(true);
    m_rescaleTimer.setInterval(0);
    connect(&m_rescaleTimer, &QTimer::timeout, this, &TransferListView::rescaleNow);

    // Row structure changes need new heights and minimum widths; plain data updates do not.
    connect(proxy, &QAbstractItemModel::rowsInserted, this, [this] { invalidateMins(); refreshAnimation(); });
    connect(proxy, &QAbstractItemModel::rowsRemoved, this, [this] { invalidateMins(); refreshAnimation(); });
    connect(proxy, &QAbstractItemModel::modelReset, this, [this] { invalidateMins(); refreshAnimation(); });
    connect(proxy, &QAbstractItemModel::layoutChanged, this, [this] { scheduleRescale(); refreshAnimation(); });
    connect(proxy, &QAbstractItemModel::dataChanged, this, [this] { refreshAnimation(); });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { refreshAnimation(); });
    connect(&RoTheme::instance(), &RoTheme::changed, this, [this]
    {
        invalidateMins();   // Vibrant changes the Status column's minimum width
        refreshAnimation();
        viewport()->update();
    });
    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]
    {
        emit selectedIdChanged(selectedId());
    });
}

QString TransferListView::selectedId() const
{
    const QModelIndexList rows = selectionModel()->selectedRows();
    if (rows.isEmpty())
        return {};
    return m_source->row(m_proxy->mapToSource(rows.first()).row()).id;
}

void TransferListView::setQueueColumnVisible(const bool on)
{
    m_queueVisible = on;
    setColumnHidden(TransferListModel::QueuePos, !on);
    scheduleRescale();
}

void TransferListView::invalidateMins()
{
    m_minsDirty = true;
    scheduleRescale();
}

void TransferListView::scheduleRescale()
{
    m_rescaleTimer.start();
}

void TransferListView::resizeEvent(QResizeEvent *event)
{
    QTableView::resizeEvent(event);
    scheduleRescale();
}

QList<qreal> TransferListView::minContent()
{
    if (!m_minsDirty && !m_mins.isEmpty())
        return m_mins;

    const bool vib = RoTheme::instance().vibrant();
    QFont hf = RoPaint::font(10.5, 600);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 0.63);
    const QFontMetricsF hm(hf);

    QList<qreal> mins(TransferListModel::ColumnCount, 0);
    const int n = m_source->rowCount();
    // Name: widest unbreakable chunk; with no rows at all a sensible floor.
    mins[TransferListModel::Name] = (n == 0) ? 32 + 28 + 10 + 60 : 0;
    mins[TransferListModel::Progress] = 16 + 110 + 16;   // the bar is a fixed 110 px
    mins[TransferListModel::Status] = (n == 0) ? 32 + 18 + TorrentDelegate::BADGE_ICON + 5 + 60 : 0;
    for (const int c : {TransferListModel::Down, TransferListModel::Up, TransferListModel::Eta, TransferListModel::Ratio})
        mins[c] = (n == 0) ? 72 : 0;
    mins[TransferListModel::QueuePos] = QUEUE_COLUMN_WIDTH;

    for (int i = 0; i < n; ++i)
    {
        const TorrentRow &row = m_source->row(i);
        for (const int c : {TransferListModel::Name, TransferListModel::Status, TransferListModel::Down
                            , TransferListModel::Up, TransferListModel::Eta, TransferListModel::Ratio})
            mins[c] = qMax(mins[c], TorrentDelegate::minContentWidth(row, c, vib));
    }
    // A header word never wraps.
    for (int c = 0; c < TransferListModel::ColumnCount; ++c)
        mins[c] = qMax(mins[c], hm.horizontalAdvance(m_source->headerData(c, Qt::Horizontal).toString().toUpper()) + 32);

    m_mins = mins;
    m_minsDirty = false;
    return m_mins;
}

void TransferListView::rescaleNow()
{
    const int vw = viewport()->width();
    if (vw <= 0)
        return;

    const bool widthChanged = (vw != m_lastViewportWidth);
    m_lastViewportWidth = vw;

    // html: <table width:100%> with auto layout. A wide window shares the extra width proportionally;
    // a narrow one shrinks columns to their min-content and then the table scrolls sideways.
    const QList<qreal> mins = minContent();
    const qreal scale = static_cast<qreal>(vw) / BASE_VIEWPORT;
    QList<int> widths(TransferListModel::ColumnCount, 0);
    int sum = 0;
    for (int c = 0; c < 6; ++c)
    {
        widths[c + 1] = qMax(qRound(mins[c + 1]), qRound(COL_BASE[c] * scale));
        sum += widths[c + 1];
    }
    widths[TransferListModel::QueuePos] = m_queueVisible ? qRound(mins[TransferListModel::QueuePos]) : 0;
    sum += widths[TransferListModel::QueuePos];
    widths[0] = qMax(qRound(mins[0]), vw - sum);

    for (int c = 0; c < TransferListModel::ColumnCount; ++c)
    {
        if ((c == TransferListModel::QueuePos) && !m_queueVisible)
            continue;   // the hidden column keeps its own width
        if (columnWidth(c) != widths[c])
            setColumnWidth(c, widths[c]);
    }
    Q_UNUSED(widthChanged);
    resizeRowsToContents();
    viewport()->update();
    refreshAnimation();
}

bool TransferListView::viewportEvent(QEvent *event)
{
    switch (event->type())
    {
    case QEvent::MouseMove:
    case QEvent::HoverMove:
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::Leave:
    {
        // Hover is painted by the delegate from the cursor position; repaint the row we left and the
        // row we entered across their whole width (a cell-only repaint left seams).
        int row = -1;
        if (event->type() == QEvent::MouseMove)
            row = rowAt(static_cast<QMouseEvent *>(event)->position().toPoint().y());
        else if ((event->type() != QEvent::Leave) && (event->type() != QEvent::HoverLeave))
            row = rowAt(viewport()->mapFromGlobal(QCursor::pos()).y());
        if (row != m_hoverRow)
        {
            for (const int r : {m_hoverRow, row})
            {
                if (r >= 0)
                    viewport()->update(QRect(0, rowViewportPosition(r), viewport()->width(), rowHeight(r)));
            }
            m_hoverRow = row;
        }
        break;
    }
    default:
        break;
    }
    return QTableView::viewportEvent(event);
}

void TransferListView::refreshAnimation()
{
    // Run the clock only while at least one visible row needs it.
    const bool vib = RoTheme::instance().vibrant();
    bool needed = false;
    const int first = rowAt(0);
    if (first >= 0)
    {
        const int h = viewport()->height();
        for (int r = first; r < m_proxy->rowCount(); ++r)
        {
            if (rowViewportPosition(r) > h)
                break;
            const TorrentRow &row = m_source->row(m_proxy->mapToSource(m_proxy->index(r, 0)).row());
            if (TorrentDelegate::needsAnimation(row, vib))
            {
                needed = true;
                break;
            }
        }
    }
    if (needed && !m_timer.isActive())
    {
        m_clock.start();
        m_timer.start();
    }
    else if (!needed && m_timer.isActive())
    {
        m_timer.stop();
    }
}

void TransferListView::tick()
{
    m_time += m_clock.restart() / 1000.0;
    m_delegate->setTime(m_time);

    // Repaint only the rows that animate.
    const bool vib = RoTheme::instance().vibrant();
    QRegion dirty;
    const int first = rowAt(0);
    if (first < 0)
        return;
    const int h = viewport()->height();
    for (int r = first; r < m_proxy->rowCount(); ++r)
    {
        const int y = rowViewportPosition(r);
        if (y > h)
            break;
        const TorrentRow &row = m_source->row(m_proxy->mapToSource(m_proxy->index(r, 0)).row());
        if (TorrentDelegate::needsAnimation(row, vib))
            dirty += QRect(0, y, viewport()->width(), rowHeight(r));
    }
    viewport()->update(dirty);
}

void TransferListView::updateEmptyState(const QString &sideLabel)
{
    const int total = m_source->rowCount();
    const int shown = m_proxy->rowCount();
    if (shown > 0)
    {
        m_empty->dismiss();
        return;
    }
    const RowFilter &f = m_proxy->filter();
    if (total == 0)
    {
        m_empty->present(QStringLiteral("m-magnet"), tr("No torrents yet")
                         , tr("Add a .torrent file or paste a magnet link to start downloading.")
                         , {{tr("Add torrent"), [this] { emit addTorrentRequested(); }}
                            , {tr("Add link"), [this] { emit addLinkRequested(); }}});
    }
    else if (!f.search.isEmpty())
    {
        m_empty->present(QStringLiteral("search"), tr("No results for \u201c%1\u201d").arg(f.search)
                         , tr("Check the spelling or try a different search.")
                         , {{tr("Clear search"), [this] { emit clearSearchRequested(); }}});
    }
    else
    {
        m_empty->present(QStringLiteral("all"), tr("No torrents in %1").arg(sideLabel)
                         , tr("Torrents will appear here when they match this filter.")
                         , {{tr("Show all torrents"), [this] { emit showAllRequested(); }}});
    }
}

QStringList TransferListView::selectedIds() const
{
    QStringList ids;
    for (const QModelIndex &index : selectionModel()->selectedRows())
        ids.append(m_source->row(m_proxy->mapToSource(index).row()).id);
    return ids;
}

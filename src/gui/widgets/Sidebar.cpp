// SPDX-License-Identifier: GPL-2.0-or-later
#include "Sidebar.h"

#include <functional>

#include <QEvent>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QTransform>
#include <QVBoxLayout>

#include "RoPaint.h"
#include "TorrentDelegate.h"
#include "base/utils/misc.h"
#include "models/TransferListModel.h"
#include "theme/RoTheme.h"

using RoPaint::font;

namespace
{
    QColor paletteColour(const RoPalette &p, const QString &name)
    {
        if (name == u"seed") return p.seed;
        if (name == u"info") return p.info;
        if (name == u"stall") return p.stall;
        if (name == u"queued") return p.queued;
        if (name == u"check") return p.check;
        if (name == u"muted") return p.muted;
        if (name == u"accent") return p.accent;
        if (name == u"seedA") return p.seedA;
        if (name == u"infoA") return p.infoA;
        if (name == u"stallA") return p.stallA;
        if (name == u"queuedA") return p.queuedA;
        if (name == u"checkA") return p.checkA;
        if (name == u"accentA") return p.accentA;
        if (name == u"surface3") return p.surface3;
        return {};
    }
}

// ---------------------------------------------------------------------------------------------

bool SideTree::viewportEvent(QEvent *event)
{
    const bool r = QTreeWidget::viewportEvent(event);
    switch (event->type())
    {
    case QEvent::HoverEnter:
    case QEvent::HoverMove:
    case QEvent::HoverLeave:
    case QEvent::Leave:
    case QEvent::MouseMove:
        viewport()->update();
        break;
    default:
        break;
    }
    return r;
}

QSize SideDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const
{
    if (index.data(KindRole).toString() == u"section")
        return {100, 26 + ((index.row() == 0) ? 0 : 20)};   // first header 26 px, later ones 46 px (20 px of space above)
    return {100, 31};
}

void SideDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const
{
    RoTheme &theme = RoTheme::instance();
    const RoPalette &P = theme.pal();
    const QString kind = idx.data(KindRole).toString();
    QRectF r = QRectF(opt.rect).adjusted(0, 0, -10, 0);   // reserve the scroll bar's own gutter
    const bool hover = opt.state & QStyle::State_MouseOver;

    p->save();
    p->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

    if (kind == u"section")
    {
        const int pad = (idx.row() == 0) ? 0 : 20;
        r.setTop(r.top() + pad);
        r.setHeight(26);
        if (hover)
        {
            p->setPen(Qt::NoPen);
            p->setBrush(P.surface2);
            p->drawRoundedRect(r, 7, 7);
        }
        QFont f = font(11, 700);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0.99);
        p->setFont(f);
        p->setPen(hover ? P.text : P.muted);
        p->drawText(r.adjusted(8, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, idx.data(Qt::DisplayRole).toString().toUpper());

        QPixmap chev = theme.iconPx(QStringLiteral("chev"), hover ? P.muted : P.faint, 11);
        if (idx.data(CollapsedRole).toBool())
            chev = chev.transformed(QTransform().rotate(-90), Qt::SmoothTransformation);   // collapsed: points right
        p->drawPixmap(roSnap(p, QPointF(r.right() - 8 - 11, r.center().y() - 5.5)), chev);
    }
    else
    {
        const bool active = idx.data(ActiveRole).toBool();
        const QString sc = idx.data(ColourRole).toString();
        const QString sbg = idx.data(WashRole).toString();
        QColor fg = P.muted;
        QBrush bg(Qt::NoBrush);
        std::optional<QColor> border;

        if (!sc.isEmpty() && (sbg == u"completed"))
        {
            // Completed: not a translucent wash. The same solid green gradient as a Completed row in
            // Vibrant mode, white text and icon, no outline.
            if (active || hover)
            {
                bg = TorrentDelegate::washBrush(P, theme.isLight(), RoStatus::Completed, r.adjusted(.5, .5, -.5, -.5));
                fg = Qt::white;
            }
        }
        else if (!sc.isEmpty())
        {
            const QColor c = paletteColour(P, sc);
            const QColor b = paletteColour(P, sbg);
            if (active)
            {
                bg = QBrush(b);
                fg = c;
            }
            else if (hover)
            {
                bg = QBrush(b);
                fg = c;
                border = c;
            }
        }
        else if (active)
        {
            bg = QBrush(P.surface3);
            fg = P.text;
        }
        else if (hover)
        {
            bg = QBrush(P.surface2);
            fg = P.text;
        }

        const QRectF pill = r.adjusted(.5, .5, -.5, -.5);   // half-pixel inset keeps the edge crisp
        p->setPen(Qt::NoPen);
        if (bg.style() != Qt::NoBrush)
        {
            p->setBrush(bg);
            p->drawRoundedRect(pill, 7, 7);
        }
        if (border)
        {
            p->setPen(QPen(*border, 1));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(pill, 7, 7);
        }

        p->drawPixmap(roSnap(p, QPointF(r.left() + 9, r.center().y() - 7)), theme.iconPx(idx.data(IconRole).toString(), fg, 14));

        const QString count = QString::number(idx.data(CountRole).toInt());   // no parentheses
        const QFont lf = font(13, active ? 600 : 400);
        const qreal tw = r.width() - 9 - 14 - 9 - 8 - QFontMetricsF(font(11)).horizontalAdvance(count) - 10;
        p->setFont(lf);
        p->setPen(fg);
        p->drawText(QRectF(r.left() + 9 + 14 + 9, r.top(), tw, r.height()), Qt::AlignVCenter
                    , QFontMetricsF(lf).elidedText(idx.data(Qt::DisplayRole).toString(), Qt::ElideRight, tw));

        p->setFont(font(11));
        p->setPen(((active || (!sc.isEmpty() && hover)) && (!sc.isEmpty() || active)) ? fg : P.faint);
        p->drawText(r.adjusted(0, 0, -8, 0), Qt::AlignVCenter | Qt::AlignRight, count);
    }
    p->restore();
}

// ---------------------------------------------------------------------------------------------

SparkLine::SparkLine(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(40);
}

void SparkLine::addSample(const qint64 downBytes, const qint64 upBytes)
{
    constexpr int keep = 24;
    m_rawDown.append(downBytes);
    m_rawUp.append(upBytes);
    while (m_rawDown.size() > keep)
    {
        m_rawDown.removeFirst();
        m_rawUp.removeFirst();
    }
    // Auto-scale to the busiest sample in the window, with a floor so an idle line stays low.
    qint64 peak = 100 * 1024;
    for (int i = 0; i < m_rawDown.size(); ++i)
        peak = qMax(peak, qMax(m_rawDown[i], m_rawUp[i]));
    m_down.clear();
    m_up.clear();
    for (int i = 0; i < m_rawDown.size(); ++i)
    {
        m_down.append(100.0 * m_rawDown[i] / peak);
        m_up.append(100.0 * m_rawUp[i] / peak);
    }
    update();
}

void SparkLine::setSamples(const QList<double> &down, const QList<double> &up)
{
    m_down = down;
    m_up = up;
    update();
}

void SparkLine::paintEvent(QPaintEvent *)
{
    if (m_down.size() < 2)
        return;
    const RoPalette &P = RoTheme::instance().pal();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const qreal W = width();
    const qreal H = height();

    const auto points = [&](const QList<double> &arr)
    {
        QList<QPointF> pts;
        for (int i = 0; i < arr.size(); ++i)
            pts.append(QPointF(i / static_cast<qreal>(arr.size() - 1) * W, H - arr[i] / 100.0 * H));
        return pts;
    };
    const struct { const QList<double> *arr; QColor colour; qreal alpha; } series[] =
        {{&m_down, P.seed, 0.28}, {&m_up, P.info, 0.24}};

    for (const auto &s : series)   // soft filled areas (28% / 24%)
    {
        QList<QPointF> pts = points(*s.arr);
        QPolygonF area;
        area << QPointF(0, H);
        for (const QPointF &pt : pts)
            area << pt;
        area << QPointF(W, H);
        QColor fill = s.colour;
        fill.setAlphaF(s.alpha);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPolygon(area);
    }
    for (const auto &s : series)   // 1.6 px lines
    {
        QPolygonF line;
        for (const QPointF &pt : points(*s.arr))
            line << pt;
        p.setPen(QPen(s.colour, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(line);
    }
}

// ---------------------------------------------------------------------------------------------

namespace
{
    // Right edge of the sidebar: drag to resize 160-420 px; a small accent pill shows on hover or drag.
    class SideResizeHandle final : public QWidget
    {
    public:
        explicit SideResizeHandle(Sidebar *side)
            : QWidget(side)
            , m_side(side)
        {
            setCursor(Qt::SizeHorCursor);
            setAttribute(Qt::WA_Hover);
            setFixedWidth(Sidebar::HANDLE_WIDTH);
        }

    protected:
        void enterEvent(QEnterEvent *) override { m_hover = true; update(); }
        void leaveEvent(QEvent *) override { m_hover = false; update(); }
        void paintEvent(QPaintEvent *) override
        {
            if (!(m_hover || m_drag))
                return;
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(RoTheme::instance().pal().accent);
            p.drawRoundedRect(QRectF(width() - 7, height() / 2.0 - 17, 4, 34), 2, 2);
        }
        void mousePressEvent(QMouseEvent *e) override
        {
            if (e->button() == Qt::LeftButton)
            {
                m_drag = true;
                m_x0 = e->globalPosition().x();
                m_w0 = m_side->width();
                update();
            }
        }
        void mouseMoveEvent(QMouseEvent *e) override
        {
            if (!m_drag)
                return;
            const int w = static_cast<int>(qBound<qreal>(Sidebar::MIN_WIDTH, m_w0 + e->globalPosition().x() - m_x0, Sidebar::MAX_WIDTH));
            m_side->setFixedWidth(w);
            emit m_side->widthDragged();   // the table viewport changed too: reflow its columns
        }
        void mouseReleaseEvent(QMouseEvent *) override { m_drag = false; update(); }

    private:
        Sidebar *m_side;
        bool m_hover = false;
        bool m_drag = false;
        qreal m_x0 = 0;
        int m_w0 = 0;
    };

    class SpeedCard final : public QFrame
    {
    public:
        using QFrame::QFrame;
        std::function<void()> onClick;

    protected:
        void mousePressEvent(QMouseEvent *e) override
        {
            if ((e->button() == Qt::LeftButton) && onClick)
                onClick();
        }
    };
}

Sidebar::Sidebar(TransferListModel *source, QWidget *parent)
    : QFrame(parent)
    , m_source(source)
{
    setObjectName(QStringLiteral("sidebar"));
    setFixedWidth(DEFAULT_WIDTH);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    m_tree = new SideTree;
    m_tree->setObjectName(QStringLiteral("sideTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(false);
    m_tree->setIndentation(0);
    m_tree->setItemDelegate(new SideDelegate(m_tree));
    m_tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setMouseTracking(true);
    m_tree->setSelectionMode(QAbstractItemView::NoSelection);
    m_tree->setFocusPolicy(Qt::NoFocus);
    m_tree->setExpandsOnDoubleClick(false);
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos)
    {
        const QTreeWidgetItem *item = m_tree->itemAt(pos);
        const QPoint global = m_tree->viewport()->mapToGlobal(pos);
        if (!item)
            emit menuRequested(global, QStringLiteral("empty"), {});
        else if (item == m_categorySection)
            emit menuRequested(global, QStringLiteral("categoryHeader"), {});
        else if (item->data(0, SideDelegate::KindRole).toString() == u"item")
            emit menuRequested(global, item->data(0, SideDelegate::GroupRole).toString()
                               , item->data(0, SideDelegate::PayloadRole));
    });
    v->addWidget(m_tree, 1);

    // Speed card (relocated from the status bar): 14 / 16 / 14 / 10 px padding, two rows + sparkline.
    auto *card = new SpeedCard;
    card->setObjectName(QStringLiteral("graphBox"));
    card->setCursor(Qt::PointingHandCursor);
    card->onClick = [this] { emit speedCardClicked(); };
    m_card = card;
    auto *cl = new QVBoxLayout(card);
    cl->setContentsMargins(14, 16, 14, 10);
    cl->setSpacing(6);
    const struct { const char *name; QLabel **value; QColor RoPalette::*dot; } rows[] =
        {{QT_TR_NOOP("Download"), &m_downValue, &RoPalette::seed}, {QT_TR_NOOP("Upload"), &m_upValue, &RoPalette::info}};
    for (const auto &rw : rows)
    {
        auto *row = new QHBoxLayout;
        auto *key = new QLabel;
        key->setObjectName(QStringLiteral("graphKey"));
        key->setTextFormat(Qt::RichText);
        key->setText(QStringLiteral("<span style=\"color:%1\">\u25cf</span>&nbsp;%2")
                     .arg((RoTheme::instance().pal().*rw.dot).name(), tr(rw.name)));
        *rw.value = new QLabel(QStringLiteral("0 B/s"));
        (*rw.value)->setObjectName(QStringLiteral("graphVal"));
        row->addWidget(key);
        row->addStretch();
        row->addWidget(*rw.value);
        cl->addLayout(row);
    }
    cl->addSpacing(4);
    m_spark = new SparkLine;
    cl->addWidget(m_spark);
    v->addWidget(card);

    m_handle = new SideResizeHandle(this);
    m_handle->raise();

    m_statusSection = addSection(tr("Status"), false);
    m_categorySection = addSection(tr("Categories"), false);
    m_trackerSection = addSection(tr("Trackers"), true);
    m_tree->expandItem(m_statusSection);
    m_tree->expandItem(m_categorySection);
    m_trackerSection->setExpanded(false);

    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *it) { onItemClicked(it); });
    connect(&RoTheme::instance(), &RoTheme::changed, this, [this]
    {
        // Dot colours in the speed card follow the palette.
        const RoPalette &pal = RoTheme::instance().pal();
        const auto keys = m_card->findChildren<QLabel *>(QStringLiteral("graphKey"));
        const QColor cols[] = {pal.seed, pal.info};
        const QString names[] = {tr("Download"), tr("Upload")};
        for (int i = 0; i < keys.size() && i < 2; ++i)
            keys[i]->setText(QStringLiteral("<span style=\"color:%1\">\u25cf</span>&nbsp;%2").arg(cols[i].name(), names[i]));
        m_tree->viewport()->update();
    });

    m_label = tr("All torrents");
    refreshCounts();
}

void Sidebar::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    m_handle->setGeometry(width() - HANDLE_WIDTH, 0, HANDLE_WIDTH, height());
    m_handle->raise();
}

QTreeWidgetItem *Sidebar::addSection(const QString &name, const bool collapsed)
{
    auto *it = new QTreeWidgetItem({name});
    it->setData(0, SideDelegate::KindRole, QStringLiteral("section"));
    it->setData(0, SideDelegate::CollapsedRole, collapsed);
    it->setFlags(Qt::ItemIsEnabled);
    m_tree->addTopLevelItem(it);
    return it;
}

QTreeWidgetItem *Sidebar::addItem(QTreeWidgetItem *parent, const QString &name, const QString &icon, const int count
                                  , const QString &group, const QVariant &payload
                                  , const QString &colour, const QString &wash)
{
    auto *it = new QTreeWidgetItem(parent, {name});
    it->setData(0, SideDelegate::KindRole, QStringLiteral("item"));
    it->setData(0, SideDelegate::ColourRole, colour);
    it->setData(0, SideDelegate::WashRole, wash);
    it->setData(0, SideDelegate::IconRole, icon);
    it->setData(0, SideDelegate::CountRole, count);
    it->setData(0, SideDelegate::ActiveRole, false);
    it->setData(0, SideDelegate::GroupRole, group);
    it->setData(0, SideDelegate::PayloadRole, payload);
    return it;
}

void Sidebar::rebuild()
{
    // Category and tracker rows follow the data. Status rows are fixed and created once.
    if (m_statusSection->childCount() == 0)
    {
        using K = RowFilter::StatusKind;
        const struct { const char *name; const char *icon; K kind; const char *colour; const char *wash; } rows[] =
        {
            {QT_TR_NOOP("All torrents"), "all", K::All, "", ""},
            {QT_TR_NOOP("Downloading"), "dl", K::Downloading, "seed", "seedA"},
            {QT_TR_NOOP("Seeding"), "sd", K::Seeding, "info", "infoA"},
            {QT_TR_NOOP("Completed"), "cp", K::Completed, "seed", "completed"},
            {QT_TR_NOOP("Paused"), "st", K::Paused, "stall", "stallA"},
            {QT_TR_NOOP("Queued"), "qu", K::Queued, "queued", "queuedA"},
            {QT_TR_NOOP("Checking"), "ck", K::Checking, "check", "checkA"},
            {QT_TR_NOOP("Stalled"), "pa", K::Stalled, "muted", "surface3"},
            {QT_TR_NOOP("Error"), "er", K::Error, "accent", "accentA"},
        };
        for (const auto &r : rows)
            addItem(m_statusSection, tr(r.name), QString::fromLatin1(r.icon), 0, QStringLiteral("status")
                    , static_cast<int>(r.kind), QString::fromLatin1(r.colour), QString::fromLatin1(r.wash));
    }

    QStringList categories;
    QStringList trackers;
    bool uncategorised = false;
    bool trackerless = false;
    for (int i = 0; i < m_source->rowCount(); ++i)
    {
        const TorrentRow &row = m_source->row(i);
        if (row.category.isEmpty())
            uncategorised = true;
        else if (!categories.contains(row.category))
            categories.append(row.category);
        if (row.trackerHost.isEmpty())
            trackerless = true;
        else if (!trackers.contains(row.trackerHost))
            trackers.append(row.trackerHost);
    }
    for (const QString &known : std::as_const(m_knownCategories))
        if (!known.isEmpty() && !categories.contains(known))
            categories.append(known);
    categories.sort(Qt::CaseInsensitive);
    trackers.sort(Qt::CaseInsensitive);
    if (uncategorised)
        categories.prepend(QString());   // empty payload = Uncategorized
    if (trackerless)
        trackers.prepend(QString());     // empty payload = Trackerless

    if (categories != m_shownCategories)
    {
        qDeleteAll(m_categorySection->takeChildren());
        for (const QString &c : categories)
            addItem(m_categorySection, c.isEmpty() ? tr("Uncategorized") : c, QStringLiteral("folder"), 0
                    , QStringLiteral("category"), c);
        m_shownCategories = categories;
        // A category that vanished can no longer be the active filter.
        if (m_filter.category && !categories.contains(*m_filter.category))
            m_filter.category.reset();
    }
    if (trackers != m_shownTrackers)
    {
        qDeleteAll(m_trackerSection->takeChildren());
        for (const QString &t : trackers)
            addItem(m_trackerSection, t.isEmpty() ? tr("Trackerless") : t, QStringLiteral("globe"), 0
                    , QStringLiteral("tracker"), t);
        m_shownTrackers = trackers;
        if (m_filter.tracker && !trackers.contains(*m_filter.tracker))
            m_filter.tracker.reset();
    }
    m_categorySection->setExpanded(!m_categorySection->data(0, SideDelegate::CollapsedRole).toBool());
    m_trackerSection->setExpanded(!m_trackerSection->data(0, SideDelegate::CollapsedRole).toBool());
}

void Sidebar::setKnownCategories(const QStringList &categories)
{
    if (categories == m_knownCategories)
        return;
    m_knownCategories = categories;
    refreshCounts();
}

void Sidebar::refreshCounts()
{
    const RowFilter before = m_filter;
    rebuild();
    if (m_filter != before)   // a category or tracker row vanished while it was the active filter
        emit filterChanged(m_filter, m_label);

    // The count of a row is the number of rows THAT filter alone shows: the very same predicate the
    // table uses, so a sidebar number can never disagree with the list.
    const auto countOf = [this](const RowFilter &f)
    {
        int n = 0;
        for (int i = 0; i < m_source->rowCount(); ++i)
            n += f.matches(m_source->row(i)) ? 1 : 0;
        return n;
    };

    const auto visit = [&](QTreeWidgetItem *section)
    {
        for (int i = 0; i < section->childCount(); ++i)
        {
            QTreeWidgetItem *it = section->child(i);
            const QString group = it->data(0, SideDelegate::GroupRole).toString();
            RowFilter f;
            if (group == u"status")
                f.status = static_cast<RowFilter::StatusKind>(it->data(0, SideDelegate::PayloadRole).toInt());
            else if (group == u"category")
                f.category = it->data(0, SideDelegate::PayloadRole).toString();
            else
                f.tracker = it->data(0, SideDelegate::PayloadRole).toString();
            it->setData(0, SideDelegate::CountRole, countOf(f));
        }
    };
    visit(m_statusSection);
    visit(m_categorySection);
    visit(m_trackerSection);
    updateActiveMarks();
}

void Sidebar::updateActiveMarks()
{
    const auto mark = [&](QTreeWidgetItem *section)
    {
        for (int i = 0; i < section->childCount(); ++i)
        {
            QTreeWidgetItem *it = section->child(i);
            const QString group = it->data(0, SideDelegate::GroupRole).toString();
            const QVariant payload = it->data(0, SideDelegate::PayloadRole);
            bool on = false;
            if (group == u"status")
                on = (static_cast<int>(m_filter.status) == payload.toInt());
            else if (group == u"category")
                on = m_filter.category && (*m_filter.category == payload.toString());
            else
                on = m_filter.tracker && (*m_filter.tracker == payload.toString());
            it->setData(0, SideDelegate::ActiveRole, on);
        }
    };
    mark(m_statusSection);
    mark(m_categorySection);
    mark(m_trackerSection);
    m_tree->viewport()->update();
}

void Sidebar::onItemClicked(QTreeWidgetItem *item)
{
    if (item->data(0, SideDelegate::KindRole).toString() == u"section")
    {
        // Section collapsing is not automatic: the delegate only paints the state.
        const bool collapsed = !item->data(0, SideDelegate::CollapsedRole).toBool();
        item->setData(0, SideDelegate::CollapsedRole, collapsed);
        item->setExpanded(!collapsed);
        m_tree->viewport()->update();
        return;
    }

    const QString group = item->data(0, SideDelegate::GroupRole).toString();
    const QVariant payload = item->data(0, SideDelegate::PayloadRole);
    if (group == u"status")
    {
        m_filter.status = static_cast<RowFilter::StatusKind>(payload.toInt());
    }
    else if (group == u"category")
    {
        // Clicking the active category again goes back to "all categories" (there is no All row).
        if (m_filter.category && (*m_filter.category == payload.toString()))
            m_filter.category.reset();
        else
            m_filter.category = payload.toString();
    }
    else
    {
        if (m_filter.tracker && (*m_filter.tracker == payload.toString()))
            m_filter.tracker.reset();
        else
            m_filter.tracker = payload.toString();
    }
    m_label = item->text(0);
    updateActiveMarks();
    emit filterChanged(m_filter, m_label);
}

void Sidebar::resetFilter()
{
    m_filter = RowFilter();
    m_label = tr("All torrents");
    updateActiveMarks();
    emit filterChanged(m_filter, m_label);
}

void Sidebar::setRates(const qint64 downBytes, const qint64 upBytes)
{
    m_downValue->setText(Utils::Misc::friendlyUnit(downBytes, true));
    m_upValue->setText(Utils::Misc::friendlyUnit(upBytes, true));
    m_spark->addSample(downBytes, upBytes);
}

void Sidebar::showDemoSpeedCard()
{
    // The demo's series (Python random.Random(7)), already normalised to 0..100.
    static const QList<double> down = {49.714982944994873, 54.243017136190488, 55.319477265391157, 42.059445008632359
        , 28.184314761891905, 15.279977469130472, 13.015553143405889, 8, 11.82299667216768, 14.136085130692639
        , 28.423738298480245, 39.177792069940629, 28.505444570663755, 22.759899293721787, 13.181690691439911
        , 17.349094759225437, 18.781428730512175, 8, 13.411999195453578, 8, 8, 16.831384445674736
        , 9.1542797673393217, 9.9101748816828632};
    static const QList<double> up = {18.01698347849004, 9.4657092118408954, 6.779487550092604, 6.9282022138810078
        , 5.6011158871287243, 4, 10.537042493440762, 5.0018217855810523, 13.956000634721164, 11.889610127736766
        , 4, 4, 4, 10.322527182400627, 11.954530455649952, 9.4024813101645748, 4, 4, 4, 5.7112372701527754, 4
        , 7.9798886745914253, 9.4683628797648467, 16.971112791233423};
    m_spark->setSamples(down, up);
    m_downValue->setText(QStringLiteral("4.2 MB/s"));
    m_upValue->setText(QStringLiteral("1.1 MB/s"));
}

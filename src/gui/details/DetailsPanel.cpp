// SPDX-License-Identifier: GPL-2.0-or-later
#include "DetailsPanel.h"

#include <QEnterEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include "DetailsSource.h"
#include "LogBuffer.h"
#include "base/settingsstorage.h"
#include "theme/RoTheme.h"

namespace
{
    const QString HEIGHT_KEY = QStringLiteral("Gui/DetailsHeight");

    struct TabInfo { DetailsPanel::Tab tab; const char *label; const char *icon; };
    const TabInfo TABS[] =
    {
        {DetailsPanel::Tab::General, QT_TRANSLATE_NOOP("DetailsPanel", "General"), "info"}, {DetailsPanel::Tab::Trackers, QT_TRANSLATE_NOOP("DetailsPanel", "Trackers"), "globe"}
        , {DetailsPanel::Tab::Peers, QT_TRANSLATE_NOOP("DetailsPanel", "Peers"), "peers"}, {DetailsPanel::Tab::Http, QT_TRANSLATE_NOOP("DetailsPanel", "HTTP Sources"), "http"}
        , {DetailsPanel::Tab::Content, QT_TRANSLATE_NOOP("DetailsPanel", "Content"), "content"}, {DetailsPanel::Tab::Log, QT_TRANSLATE_NOOP("DetailsPanel", "Log"), "m-log"}
    };

    // A tab button: icon follows active / hover (a QSS rule cannot recolour a baked-in pixmap).
    class TabButton final : public QPushButton
    {
    public:
        TabButton(const QString &text, const QString &icon, QWidget *parent)
            : QPushButton(QStringLiteral(" ") + text, parent), m_icon(icon)
        {
            setObjectName(QStringLiteral("detailTab"));
            setCursor(Qt::PointingHandCursor);
            setFocusPolicy(Qt::NoFocus);
            setIconSize({13, 13});
            setAttribute(Qt::WA_Hover);
            setProperty("active", false);
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);   // tabs never shrink, the row clips
            refreshIcon(false);
        }
        void setActive(const bool on)
        {
            setProperty("active", on);
            style()->unpolish(this);
            style()->polish(this);
            refreshIcon(underMouse());
        }
        void refreshIcon(const bool hover)
        {
            const RoPalette &p = RoTheme::instance().pal();
            setIcon(RoTheme::instance().icon(m_icon, (property("active").toBool() || hover) ? p.accent : p.muted, 13));
        }
    protected:
        void enterEvent(QEnterEvent *e) override { refreshIcon(true); QPushButton::enterEvent(e); }
        void leaveEvent(QEvent *e) override { refreshIcon(false); QPushButton::leaveEvent(e); }
    private:
        QString m_icon;
    };

    // Chevron that rotates 180 degrees while the panel is collapsed.
    class ChevronButton final : public QToolButton
    {
    public:
        explicit ChevronButton(QWidget *parent) : QToolButton(parent)
        {
            setObjectName(QStringLiteral("detailToggle"));
            setCursor(Qt::PointingHandCursor);
            setFocusPolicy(Qt::NoFocus);
            setAttribute(Qt::WA_Hover);
            setFixedSize(28, 28);
        }
        bool collapsedLook = true;
    protected:
        void paintEvent(QPaintEvent *e) override
        {
            QToolButton::paintEvent(e);   // QSS hover chip
            const RoPalette &p = RoTheme::instance().pal();
            QPixmap pm = RoTheme::instance().iconPx(QStringLiteral("chev"), underMouse() ? p.text : p.faint, 12);
            if (collapsedLook)
                pm = pm.transformed(QTransform().rotate(180), Qt::SmoothTransformation);
            QPainter painter(this);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            const qreal dpr = pm.devicePixelRatio();
            painter.drawPixmap(roSnap(&painter, QPointF((width() - pm.width() / dpr) / 2, (height() - pm.height() / dpr) / 2)), pm);
        }
    };

    // The 10 px strip on the panel's top edge.
    class Handle final : public QWidget
    {
    public:
        explicit Handle(DetailsPanel *panel) : QWidget(panel), m_panel(panel)
        {
            setCursor(Qt::SizeVerCursor);
            setAttribute(Qt::WA_Hover);
            setFixedHeight(DetailsPanel::HandleHeight);
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
            p.drawRoundedRect(QRectF(width() / 2.0 - 17, 3, 34, 4), 2, 2);
        }
        void mousePressEvent(QMouseEvent *e) override
        {
            if (e->button() != Qt::LeftButton)
                return;
            if (m_panel->isCollapsed())
                m_panel->expand(DetailsPanel::Tab::General);
            m_drag = true;
            m_y0 = e->globalPosition().y();
            m_h0 = m_panel->height();
            update();
        }
        void mouseMoveEvent(QMouseEvent *e) override
        {
            if (!m_drag)
                return;
            const int parentH = m_panel->parentWidget() ? m_panel->parentWidget()->height() : 1000;
            m_panel->setPanelHeight(static_cast<int>(m_h0 - (e->globalPosition().y() - m_y0)), parentH);
        }
        void mouseReleaseEvent(QMouseEvent *) override
        {
            if (!m_drag)
                return;
            m_drag = false;
            update();
            m_panel->finishResize();
        }
    private:
        DetailsPanel *m_panel;
        bool m_hover = false, m_drag = false;
        qreal m_y0 = 0;
        int m_h0 = 0;
    };
}

DetailsPanel::DetailsPanel(DetailsSource *source, LogBuffer *log, QWidget *parent)
    : QFrame(parent)
    , m_source(source)
{
    setObjectName(QStringLiteral("detailPanel"));
    m_height = qMax(MinHeight + 10, SettingsStorage::instance()->loadValue<int>(HEIGHT_KEY, DefaultHeight));

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_row = new QFrame;
    m_row->setObjectName(QStringLiteral("detailTabsRow"));
    auto *rl = new QHBoxLayout(m_row);
    rl->setContentsMargins(16, 0, 16, 0);
    rl->setSpacing(2);
    for (const TabInfo &t : TABS)
    {
        auto *b = new TabButton(tr(t.label), QString::fromLatin1(t.icon), m_row);
        const Tab tab = t.tab;
        connect(b, &QPushButton::clicked, this, [this, tab] { expand(tab); });
        rl->addWidget(b);
        m_buttons.insert(static_cast<int>(t.tab), b);
    }
    rl->addStretch(1);
    auto *chev = new ChevronButton(m_row);
    connect(chev, &QToolButton::clicked, this, &DetailsPanel::toggle);
    m_toggle = chev;
    rl->addWidget(m_toggle);
    // The row never forces the window wider: it is clipped instead.
    m_row->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    lay->addWidget(m_row);

    m_stack = new QStackedWidget;
    m_empty = new QLabel(tr("Select a torrent to see its details."));
    m_empty->setObjectName(QStringLiteral("dEmpty"));
    m_empty->setContentsMargins(22, 22, 22, 0);
    m_empty->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_general = new GeneralTab(source);
    m_generalScroll = new QScrollArea;
    m_generalScroll->setObjectName(QStringLiteral("detailScroll"));
    m_generalScroll->setWidgetResizable(true);
    m_generalScroll->setFrameShape(QFrame::NoFrame);
    m_generalScroll->setWidget(m_general);
    m_trackers = new TrackersTab(source);
    m_peers = new PeersTab(source);
    m_http = new HttpTab(source);
    m_content = new ContentTab(source);
    m_log = new LogTab(source, log);
    for (QWidget *w : {static_cast<QWidget *>(m_empty), static_cast<QWidget *>(m_generalScroll), static_cast<QWidget *>(m_trackers)
                       , static_cast<QWidget *>(m_peers), static_cast<QWidget *>(m_http), static_cast<QWidget *>(m_content)
                       , static_cast<QWidget *>(m_log)})
        m_stack->addWidget(w);
    lay->addWidget(m_stack, 1);

    m_handle = new Handle(this);
    m_timer.setInterval(1500);
    connect(&m_timer, &QTimer::timeout, this, &DetailsPanel::refreshNow);
    connect(&RoTheme::instance(), &RoTheme::changed, this, [this] { updateTabIcons(); updateToggle(); });
    apply();
}

DetailsTab *DetailsPanel::tabWidget(const Tab tab) const
{
    switch (tab)
    {
    case Tab::General: return m_general;
    case Tab::Trackers: return m_trackers;
    case Tab::Peers: return m_peers;
    case Tab::Http: return m_http;
    case Tab::Content: return m_content;
    case Tab::Log: return m_log;
    }
    return nullptr;
}

void DetailsPanel::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    m_handle->setGeometry(0, 0, width(), HandleHeight);
    m_handle->raise();
}

void DetailsPanel::expand(const Tab tab)
{
    m_collapsed = false;
    m_hasTab = true;
    m_tab = tab;
    apply();
}

void DetailsPanel::collapse()
{
    m_collapsed = true;
    m_hasTab = false;
    apply();
}

void DetailsPanel::toggle()
{
    if (m_collapsed)
        expand(Tab::General);
    else
        collapse();
}

void DetailsPanel::setTorrent(const QString &id)
{
    m_id = id;
    // Only the visible tab is loaded; the others pick the torrent up when they are opened.
    showCurrentPage();
}

void DetailsPanel::setRefreshInterval(const int ms)
{
    m_timer.setInterval(qMax(250, ms));
}

void DetailsPanel::refreshNow()
{
    if (m_collapsed || !m_hasTab || m_id.isEmpty() || m_tab == Tab::Log)
        return;
    tabWidget(m_tab)->refresh();
}

void DetailsPanel::setPanelHeight(const int height, const int parentHeight)
{
    const int maxHeight = qMax(MinHeight, static_cast<int>(parentHeight * 0.75));
    m_height = qBound(MinHeight, height, maxHeight);
    setFixedHeight(m_height);
}

void DetailsPanel::finishResize()
{
    if (height() <= MinHeight + 4)
    {
        collapse();
        return;
    }
    SettingsStorage::instance()->storeValue(HEIGHT_KEY, m_height);
}

void DetailsPanel::updateTabIcons()
{
    for (int i = 0; i < m_buttons.size(); ++i)
    {
        auto *b = static_cast<TabButton *>(m_buttons.value(i));
        b->refreshIcon(b->underMouse());
    }
}

void DetailsPanel::updateToggle()
{
    auto *chev = static_cast<ChevronButton *>(m_toggle);
    chev->collapsedLook = m_collapsed;
    m_toggle->setToolTip(m_collapsed ? tr("Expand panel") : tr("Collapse panel"));
    m_toggle->update();
}

void DetailsPanel::showCurrentPage()
{
    if (m_collapsed || !m_hasTab)
        return;
    if (m_tab == Tab::Log)
    {
        m_stack->setCurrentWidget(m_log);
        return;
    }
    if (m_id.isEmpty() || !m_source->contains(m_id))
    {
        m_stack->setCurrentWidget(m_empty);
        return;
    }
    DetailsTab *tab = tabWidget(m_tab);
    if (tab->torrentId() != m_id)
        tab->setTorrent(m_id);
    else
        tab->refresh();
    m_stack->setCurrentWidget(m_tab == Tab::General ? static_cast<QWidget *>(m_generalScroll) : static_cast<QWidget *>(tab));
}

void DetailsPanel::apply()
{
    m_stack->setVisible(!m_collapsed);
    setFixedHeight(m_collapsed ? CollapsedHeight : m_height);
    for (const TabInfo &t : TABS)
        static_cast<TabButton *>(m_buttons.value(static_cast<int>(t.tab)))->setActive(!m_collapsed && m_hasTab && m_tab == t.tab);
    updateToggle();
    if (m_collapsed)
        m_timer.stop();
    else
        m_timer.start();
    showCurrentPage();
    emit expandedChanged(!m_collapsed);
}

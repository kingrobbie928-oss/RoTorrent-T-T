// SPDX-License-Identifier: GPL-2.0-or-later
#include "RoHoverIcon.h"

#include <QPainter>
#include <QStyleOptionToolButton>
#include <QStylePainter>

#include "theme/RoTheme.h"

RoHoverIcon::RoHoverIcon(const QString &iconName, QColor RoPalette::*normal, QColor RoPalette::*hover
                         , const bool whiteOnHover, const int px, QWidget *parent)
    : QToolButton(parent)
    , m_name(iconName)
    , m_normal(normal)
    , m_hover(hover)
    , m_whiteOnHover(whiteOnHover)
    , m_px(px)
{
    setAttribute(Qt::WA_Hover);
    setIconSize({px, px});
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    refresh(false);
    connect(&RoTheme::instance(), &RoTheme::changed, this, qOverload<>(&RoHoverIcon::refresh));
}

void RoHoverIcon::setIconName(const QString &iconName)
{
    m_name = iconName;
    refresh(false);
}

void RoHoverIcon::refresh(const bool hover)
{
    const RoPalette &pal = RoTheme::instance().pal();
    QColor colour = pal.*(hover ? m_hover : m_normal);
    if (hover && m_whiteOnHover)
        colour = Qt::white;
    m_pixmap = RoTheme::instance().iconPx(m_name, colour, m_px);
    update();
}

void RoHoverIcon::enterEvent(QEnterEvent *event)
{
    refresh(true);
    QToolButton::enterEvent(event);
}

void RoHoverIcon::leaveEvent(QEvent *event)
{
    refresh(false);
    QToolButton::leaveEvent(event);
}

void RoHoverIcon::paintEvent(QPaintEvent *)
{
    QStylePainter painter(this);
    QStyleOptionToolButton opt;
    initStyleOption(&opt);
    opt.icon = QIcon();
    opt.text.clear();
    opt.toolButtonStyle = Qt::ToolButtonIconOnly;
    painter.drawComplexControl(QStyle::CC_ToolButton, opt);   // QSS chrome: hover chip, danger red

    if (m_pixmap.isNull())
        return;
    const qreal dpr = m_pixmap.devicePixelRatio();
    const qreal w = m_pixmap.width() / dpr;
    const qreal h = m_pixmap.height() / dpr;
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (!isEnabled())
        painter.setOpacity(0.35);
    painter.drawPixmap(roSnap(&painter, QPointF((width() - w) / 2, (height() - h) / 2)), m_pixmap);
}

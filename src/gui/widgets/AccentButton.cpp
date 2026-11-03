// SPDX-License-Identifier: GPL-2.0-or-later
#include "AccentButton.h"

#include <QPainter>
#include <QStyleOptionButton>

#include "theme/RoTheme.h"

AccentButton::AccentButton(const QString &text, const QString &iconName, const int px, QWidget *parent)
    : QPushButton(text, parent)
    , m_icon(iconName)
    , m_px(px)
{
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setIconSize({px, px});
    QPixmap placeholder(px, px);   // keeps the text / padding layout identical to an icon button
    placeholder.fill(Qt::transparent);
    setIcon(QIcon(placeholder));
    connect(&RoTheme::instance(), &RoTheme::changed, this, qOverload<>(&QWidget::update));
}

void AccentButton::paintEvent(QPaintEvent *event)
{
    QPushButton::paintEvent(event);
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QStyleOptionButton opt;
    initStyleOption(&opt);
    const QRect content = style()->subElementRect(QStyle::SE_PushButtonContents, &opt, this);
    const int textWidth = text().isEmpty() ? 0 : fontMetrics().horizontalAdvance(text());
    const int gap = text().isEmpty() ? 0 : 4;
    const qreal total = m_px + gap + textWidth;
    const qreal x = content.x() + (content.width() - total) / 2.0;
    const qreal y = content.y() + (content.height() - m_px) / 2.0;
    const QColor colour = underMouse() ? QColor(Qt::white) : RoTheme::instance().pal().accent;
    p.drawPixmap(roSnap(&p, QPointF(x, y)), RoTheme::instance().iconPx(m_icon, colour, m_px));
}

void AccentButton::enterEvent(QEnterEvent *event)
{
    QPushButton::enterEvent(event);
    update();
}

void AccentButton::leaveEvent(QEvent *event)
{
    QPushButton::leaveEvent(event);
    update();
}

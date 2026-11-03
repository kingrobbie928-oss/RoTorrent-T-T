// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "OptionsCategoryDelegate.h"

#include <QPainter>

#include "theme/RoTheme.h"
#include "widgets/RoPaint.h"

void OptionsCategoryDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const RoPalette &P = RoTheme::instance().pal();
    const bool selected = option.state & QStyle::State_Selected;
    const bool hover = option.state & QStyle::State_MouseOver;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Half-pixel inset so the pill's edge is crisp; one pixel of air above and below.
    const QRectF pill = QRectF(option.rect).adjusted(0.5, 1.5, -0.5, -1.5);
    if (selected || hover)
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(selected ? P.surface3 : P.surface2);
        painter->drawRoundedRect(pill, 8, 8);
    }

    const QColor fg = (selected || hover) ? P.text : P.muted;
    const QString icon = index.data(Qt::UserRole).toString();
    if (!icon.isEmpty())
    {
        painter->drawPixmap(roSnap(painter, QPointF(pill.left() + IconInset, pill.center().y() - (IconSize / 2.0)))
                            , RoTheme::instance().iconPx(icon, fg, IconSize));
    }

    painter->setFont(RoPaint::font(13, selected ? 600 : 400));
    painter->setPen(fg);
    const qreal textX = pill.left() + IconInset + IconSize + LabelGap;
    painter->drawText(QRectF(textX, pill.top(), pill.right() - textX, pill.height())
                      , Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, index.data(Qt::DisplayRole).toString());
    painter->restore();
}

QSize OptionsCategoryDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    return {option.rect.width(), RowHeight};
}

// SPDX-License-Identifier: GPL-2.0-or-later
// Paints the Options category list (handoff 8.13): rows 38 px tall; an 18 px icon 14 px from the
// pill's left edge; the label exactly 12 px after the icon (13 px, semi-bold when selected); hover =
// surface2 pill, selected = surface3 pill, 8 px radius; icons go from muted to the text colour on
// hover and selection. The icon name is stored in the item's Qt::UserRole.
// Licensed GPL-2.0-or-later.
#pragma once

#include <QStyledItemDelegate>

class OptionsCategoryDelegate final : public QStyledItemDelegate
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OptionsCategoryDelegate)

public:
    static constexpr int RowHeight = 38;
    static constexpr int IconSize = 18;
    static constexpr int IconInset = 14;
    static constexpr int LabelGap = 12;

    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

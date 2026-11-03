// SPDX-License-Identifier: GPL-2.0-or-later
// Menu icons (handoff 8.4). Every item has a 16 px icon, muted at rest and text-coloured when
// highlighted. Menus with checkable items reserve a 34 px area laid out [13 px box | 4 px gap |
// 16 px icon]; Qt draws no indicator for an item that has an icon, so the checkbox / radio dot is
// painted INSIDE the icon pixmap and refreshed when the action toggles or the theme changes.
// Licensed GPL-2.0-or-later.
#pragma once

#include <QIcon>
#include <QString>

class QAction;

namespace RoMenu
{
    enum class Box { None, Check, Radio };

    // Plain 15 px icon (menus without a check column).
    QIcon icon(const QString &name, int px = 15);
    // 34 px icon area; `box` + `on` paint the checkbox / radio state in front of the glyph.
    QIcon wideIcon(const QString &name, Box box, bool on);

    // Sets the icon on `action` and keeps it in sync: toggled() and theme changes re-render it.
    // wide = the menu has a check column (set the dynamic property checks="true" on that menu).
    void bind(QAction *action, const QString &name, bool wide);
}

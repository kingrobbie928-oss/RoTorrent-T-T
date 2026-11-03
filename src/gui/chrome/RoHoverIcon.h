// SPDX-License-Identifier: GPL-2.0-or-later
// Icon button whose glyph colour follows hover (handoff 8.1, demo class HoverIcon).
// A QSS hover rule can restyle the button chrome but never a baked-in pixmap, so the glyph is
// painted here, centred on the device pixel grid so it stays crisp at fractional scaling.
#pragma once

#include <QToolButton>

#include "theme/Palette.h"

class RoHoverIcon : public QToolButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RoHoverIcon)

public:
    // normal / hover are palette members (for example &RoPalette::muted); whiteOnHover overrides
    // the hover colour with white (danger buttons: red chip + white glyph).
    RoHoverIcon(const QString &iconName, QColor RoPalette::*normal, QColor RoPalette::*hover
                , bool whiteOnHover, int px, QWidget *parent = nullptr);

    void setIconName(const QString &iconName);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void refresh(bool hover);
    void refresh() { refresh(underMouse()); }

    QString m_name;
    QColor RoPalette::*m_normal;
    QColor RoPalette::*m_hover;
    bool m_whiteOnHover;
    int m_px;
    QPixmap m_pixmap;
};

// SPDX-License-Identifier: GPL-2.0-or-later
// Outlined accent button (Add torrent / Add torrent from URL; objectName addBtn / linkBtn).
// The QSS hover rule fills the button with the accent colour but cannot recolour a baked icon,
// so the glyph is painted here (accent at rest, white on hover), snapped to the device pixel grid.
// Licensed GPL-2.0-or-later.
#pragma once

#include <QPushButton>

class AccentButton final : public QPushButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AccentButton)

public:
    AccentButton(const QString &text, const QString &iconName, int px = 14, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_icon;
    int m_px;
};

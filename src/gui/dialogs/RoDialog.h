// SPDX-License-Identifier: GPL-2.0-or-later
// Shared dialog shell (handoff 8.12): frameless, modal, centred on the main window, 1 px border.
// 41 px title row = 40 px + a Hairline divider drawn on top: RO logo (16 px, 12 px from the left),
// 10 px gap, title (13 px semi-bold), 46 x 40 close button. Dragging the title row moves the dialog.
// objectName appDialog (frame); the body widget is the "roUi" content area.
// Licensed GPL-2.0-or-later.
#pragma once

#include <QDialog>

class QLabel;
class QVBoxLayout;

class RoDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RoDialog)

public:
    RoDialog(QWidget *parent, const QString &title, int width);

    // The content area below the title row. Add a widget named "roUi" (or use setContent()).
    QVBoxLayout *body() const { return m_body; }
    void setContent(QWidget *content);

    int run();   // centre on the owner, then exec()

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void rebuildLogo();

    QWidget *m_owner;
    QLabel *m_logo = nullptr;
    QVBoxLayout *m_body = nullptr;
};

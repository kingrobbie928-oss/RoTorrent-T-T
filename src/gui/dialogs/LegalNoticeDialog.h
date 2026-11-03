// SPDX-License-Identifier: GPL-2.0-or-later
// First-run legal notice (handoff 8.12, kit extra_screens.build_legal). The wording comes from
// app/legalnotice.cpp (the existing legal notice logic); this dialog only draws it. One button, OK.
// Licensed GPL-2.0-or-later.

#pragma once

#include "RoDialog.h"

class QPushButton;

class LegalNoticeDialog final : public RoDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LegalNoticeDialog)

public:
    // `title` is the window title ("Legal Notice"), `body` the notice, `footer` the muted last line.
    // Blocks until the user presses OK (or closes the window).
    static void showNotice(QWidget *parent, const QString &title, const QString &body, const QString &footer);

    LegalNoticeDialog(QWidget *parent, const QString &title, const QString &body, const QString &footer);

    QString headingText() const { return m_heading; }
    QPushButton *okButton() const { return m_ok; }

private:
    QString m_heading;
    QPushButton *m_ok = nullptr;
};

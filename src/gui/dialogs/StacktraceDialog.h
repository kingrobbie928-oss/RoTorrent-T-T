// SPDX-License-Identifier: GPL-2.0-or-later
// Crash info (handoff 8.12, kit extra_screens.build_stacktrace): a large read-only card with the
// bug-report text, and one Close button. Shown by app/crashhandler from inside a signal handler, so
// it keeps to plain widgets and does no work beyond building the text. Licensed GPL-2.0-or-later.

#pragma once

#include "RoDialog.h"

class QPushButton;
class QTextBrowser;

class StacktraceDialog final : public RoDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(StacktraceDialog)

public:
    explicit StacktraceDialog(QWidget *parent = nullptr);

    // Fills the card: headline, where to report, version block, the caught signal and the trace.
    void setText(const QString &signalName, const QString &stacktrace);

    // The plain text a user would paste into a bug report.
    QString reportText() const;
    QTextBrowser *textView() const { return m_text; }
    QPushButton *closeButton() const { return m_close; }

private:
    QTextBrowser *m_text = nullptr;
    QPushButton *m_close = nullptr;
};

// SPDX-License-Identifier: GPL-2.0-or-later
// The one generic styled message box (handoff 8.12). Replaces every QMessageBox in the product.
// Licensed GPL-2.0-or-later.
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QWidget;

namespace RoMessageBox
{
    enum class Kind { None, Info, Question, Warning, Critical };

    struct Result
    {
        QString label;          // button clicked; empty when closed with the X
        bool checked = false;   // state of the optional checkbox
    };

    struct Options
    {
        Kind kind = Kind::Info;
        QStringList buttons {QObject::tr("OK")};   // left to right
        QString primary;      // accent-filled button (default: the last)
        QString defaultButton;   // focus (default: the primary)
        QString detail;       // muted second paragraph
        QString checkText;    // optional checkbox
        bool checkInitial = false;
        int width = 440;
    };

    Result show(QWidget *parent, const QString &title, const QString &text, const Options &options);

    inline Result show(QWidget *parent, const QString &title, const QString &text)
    {
        return show(parent, title, text, Options {});
    }

    // Yes / No style confirmation. Returns true when `confirmLabel` was chosen.
    bool confirm(QWidget *parent, const QString &title, const QString &text
                 , const QString &confirmLabel, const QString &detail = {});
    void error(QWidget *parent, const QString &title, const QString &text);
    void warning(QWidget *parent, const QString &title, const QString &text);
    void info(QWidget *parent, const QString &title, const QString &text);
}

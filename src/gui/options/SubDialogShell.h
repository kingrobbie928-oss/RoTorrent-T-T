// SPDX-License-Identifier: GPL-2.0-or-later
// Hosts a QDialog body built from a .ui file (the "roUi" convention of handoff 8.12) in the RoDialog
// shell and runs it modally. The body's accept() / reject() close the shell. `afterRun` is called with
// the result while the body still exists, so the caller can read its fields. Licensed GPL-2.0-or-later.
#pragma once

#include <functional>

#include <algorithm>

#include <QAbstractSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLayout>
#include <QGuiApplication>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>

#include "dialogs/RoDialog.h"

// The theme's [ - | value | + ] steppers take room that upstream's native spin boxes did not need, so the
// fixed-width spin boxes of the .ui files (ports, schedule times, limits) clip their value. Give them 128 px:
// five-digit ports and "00:00" fit between the steppers.
inline void widenSpinBoxes(QWidget *root)
{
    constexpr int Wanted = 128;
    for (QAbstractSpinBox *spin : root->findChildren<QAbstractSpinBox *>())
    {
        spin->setMinimumWidth(std::max(spin->minimumWidth(), Wanted));
        if (spin->maximumWidth() < spin->minimumWidth())
            spin->setMaximumWidth(spin->minimumWidth());
    }
}

// Puts a tall section in a scroll area that is as tall as the content wants, but no taller than a dialog may be
// (handoff 8.12: screen height - 120, minus the 43 px title row and `reservedHeight` for the controls around it).
// Over-tall content then scrolls instead of being squeezed until its rows overlap.
inline QScrollArea *wrapInScrollArea(QWidget *content, QWidget *parent, const int reservedHeight)
{
    auto *scroll = new QScrollArea(parent);
    scroll->setObjectName(QStringLiteral("advScroll"));   // transparent, no frame (theme)
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(content);

    // The themed minimum heights only apply once polished, and nested layouts cache earlier hints.
    content->ensurePolished();
    for (QWidget *child : content->findChildren<QWidget *>())
        child->ensurePolished();
    for (QLayout *nested : content->findChildren<QLayout *>())
        nested->invalidate();
    if (content->layout())
        content->layout()->invalidate();
    const QSize want = content->minimumSizeHint();

    int usable = 100000;
    if (const QScreen *screen = QGuiApplication::primaryScreen())
        usable = screen->availableGeometry().height() - 120 - 43 - reservedHeight;
    scroll->setMinimumHeight(std::min(want.height() + 2, std::max(usable, 200)));
    scroll->setMinimumWidth(want.width() + scroll->verticalScrollBar()->sizeHint().width() + 4);
    return scroll;
}

// Marks a footer button as the primary (accent-filled) one. The button is already polished by the time we
// get here, and Qt does not restyle on a dynamic property change by itself, so re-polish it.
inline void markPrimary(QPushButton *button)
{
    if (!button)
        return;
    button->setProperty("primary", true);
    button->style()->unpolish(button);
    button->style()->polish(button);
}

inline int runInShell(QWidget *owner, QDialog *body, const std::function<void(int)> &afterRun = {})
{
    body->setWindowFlags(Qt::Widget);
    body->setObjectName(QStringLiteral("roUi"));
    if (auto *buttons = body->findChild<QDialogButtonBox *>())
    {
        markPrimary(buttons->button(QDialogButtonBox::Ok));
    }

    // The .ui files carry upstream's geometry; the themed controls need more room, and a layout squeezed below
    // its minimum overlaps its rows. Grow the body to what its layout asks for.
    // Measure after polishing every child: the theme's minimum heights (32 px fields, buttons) only apply once
    // a widget is polished, so an earlier measurement comes out too small.
    widenSpinBoxes(body);
    body->ensurePolished();
    for (QWidget *child : body->findChildren<QWidget *>())
        child->ensurePolished();
    // Nested layouts cache the hints they computed before the theme's minimum heights applied; clear them all.
    for (QLayout *nested : body->findChildren<QLayout *>())
        nested->invalidate();
    if (QLayout *layout = body->layout())
    {
        layout->invalidate();
        layout->activate();
        const QSize need = layout->totalMinimumSize();
        body->resize(std::max(body->width(), need.width()), std::max(body->height(), need.height()));
    }

    RoDialog shell(owner, body->windowTitle(), body->width() + 2);
    shell.setContent(body);
    QObject::connect(body, &QDialog::finished, &shell, &QDialog::done);
    const int result = shell.run();
    if (afterRun)
        afterRun(result);
    return result;
}

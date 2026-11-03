// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "ShutdownConfirmDialog.h"

#include <QCheckBox>
#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#include "base/preferences.h"
#include "theme/RoTheme.h"

namespace
{
    struct ActionText
    {
        QString message;
        QString confirm;
        QString title;
    };

    ActionText textFor(const ShutdownDialogAction action)
    {
        switch (action)
        {
        case ShutdownDialogAction::Exit:
            return {QObject::tr("RoTorrent will now exit."), QObject::tr("Exit Now"), QObject::tr("Exit confirmation")};
        case ShutdownDialogAction::Shutdown:
            return {QObject::tr("The computer is going to shutdown."), QObject::tr("Shutdown Now"), QObject::tr("Shutdown confirmation")};
        case ShutdownDialogAction::Suspend:
            return {QObject::tr("The computer is going to enter suspend mode."), QObject::tr("Suspend Now"), QObject::tr("Suspend confirmation")};
        case ShutdownDialogAction::Hibernate:
            return {QObject::tr("The computer is going to enter hibernation mode."), QObject::tr("Hibernate Now"), QObject::tr("Hibernate confirmation")};
        case ShutdownDialogAction::Reboot:
            return {QObject::tr("The computer is going to reboot."), QObject::tr("Reboot Now"), QObject::tr("Reboot confirmation")};
        }
        return {};
    }
}

bool ShutdownConfirmDialog::askForConfirmation(QWidget *parent, const ShutdownDialogAction action)
{
    if ((action == ShutdownDialogAction::Exit) && Preferences::instance()->dontConfirmAutoExit())
        return true;
    ShutdownConfirmDialog dialog(parent, action);
    return dialog.run() == QDialog::Accepted;
}

ShutdownConfirmDialog::ShutdownConfirmDialog(QWidget *parent, const ShutdownDialogAction action
                                             , const int seconds, const std::chrono::milliseconds tick)
    : RoDialog(parent, textFor(action).title, 410)
    , m_action {action}
    , m_secondsLeft {seconds}
    , m_message {textFor(action).message}
{
    const RoPalette &P = RoTheme::instance().pal();

    // Always on top: the countdown must be seen even when the window is hidden in the tray.
    setWindowFlag(Qt::WindowStaysOnTopHint, true);

    auto *host = new QDialog;
    host->setObjectName(QStringLiteral("roUi"));
    host->setWindowFlags(Qt::Widget);
    auto *v = new QVBoxLayout(host);
    v->setContentsMargins(12, 14, 12, 12);
    v->setSpacing(10);

    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);
    auto *icon = new QLabel;
    icon->setObjectName(QStringLiteral("shutdownIcon"));
    icon->setPixmap(RoTheme::instance().iconPx(QStringLiteral("m-warn"), P.check, 28));
    icon->setFixedSize(32, 32);
    icon->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    grid->addWidget(icon, 0, 0, 2, 1, Qt::AlignTop);
    m_text = new QLabel;
    m_text->setObjectName(QStringLiteral("shutdownText"));
    m_text->setWordWrap(true);
    m_text->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    grid->addWidget(m_text, 0, 1);
    if (action == ShutdownDialogAction::Exit)
    {
        m_never = new QCheckBox(tr("Don't show again"));
        m_never->setObjectName(QStringLiteral("neverShowAgain"));
        grid->addWidget(m_never, 1, 1);
    }
    grid->setColumnStretch(1, 1);
    v->addLayout(grid);

    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    row->addStretch(1);
    m_confirm = new QPushButton(textFor(action).confirm);
    m_confirm->setProperty("primary", true);
    m_cancel = new QPushButton(tr("Cancel"));
    for (QPushButton *b : {m_confirm, m_cancel})
    {
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumWidth(84);
        row->addWidget(b);
    }
    // Cancel is the default and has the focus: a stray Enter must never shut the computer down.
    m_cancel->setDefault(true);
    m_cancel->setFocus();
    v->addLayout(row);
    setContent(host);

    connect(m_confirm, &QPushButton::clicked, this, &ShutdownConfirmDialog::accept);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);

    m_timer = new QTimer(this);
    m_timer->setInterval(tick);
    connect(m_timer, &QTimer::timeout, this, &ShutdownConfirmDialog::onTick);
    connect(this, &QDialog::finished, m_timer, &QTimer::stop);
    updateText();
}

QString ShutdownConfirmDialog::messageText() const
{
    return m_text->text();
}

void ShutdownConfirmDialog::updateText()
{
    m_text->setText(m_message + u'\n' + tr("You can cancel the action within %1 seconds.").arg(m_secondsLeft));
}

void ShutdownConfirmDialog::showEvent(QShowEvent *event)
{
    RoDialog::showEvent(event);
    if (!m_timer->isActive())
        m_timer->start();
}

void ShutdownConfirmDialog::onTick()
{
    --m_secondsLeft;
    updateText();
    if (m_secondsLeft <= 0)
    {
        m_timer->stop();
        accept();   // the countdown ran out: carry on with the action
    }
}

void ShutdownConfirmDialog::accept()
{
    // Remember "Don't show again" only for Exit, the one action that has the preference.
    if (m_never)
        Preferences::instance()->setDontConfirmAutoExit(m_never->isChecked());
    RoDialog::accept();
}

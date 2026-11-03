// SPDX-License-Identifier: GPL-2.0-or-later
// Shutdown / exit confirmation with a countdown (handoff 8.12, kit extra_screens.build_shutdown).
// Shown when "On Downloads Done" fires: the action runs when the user confirms or when the countdown
// reaches zero; Cancel (the default button), Esc and the X abandon it. This dialog only asks; the
// action itself (exit, shutdown, suspend, hibernate, reboot) is carried out by the application (Phase 8).
// Licensed GPL-2.0-or-later.

#pragma once

#include <chrono>

#include "base/types.h"
#include "RoDialog.h"

class QCheckBox;
class QLabel;
class QPushButton;
class QTimer;

class ShutdownConfirmDialog final : public RoDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ShutdownConfirmDialog)

public:
    static constexpr int DefaultSeconds = 15;

    // True when the user confirmed or the countdown ran out; false on Cancel / Esc / X.
    // Blocks. Returns at once with true for Exit when the "Don't show again" preference is set.
    static bool askForConfirmation(QWidget *parent, ShutdownDialogAction action);

    // `tick` is the length of one countdown step (one second in the product, shorter in tests).
    ShutdownConfirmDialog(QWidget *parent, ShutdownDialogAction action, int seconds = DefaultSeconds
                          , std::chrono::milliseconds tick = std::chrono::seconds(1));

    ShutdownDialogAction action() const { return m_action; }
    int secondsLeft() const { return m_secondsLeft; }
    QString messageText() const;
    QPushButton *confirmButton() const { return m_confirm; }
    QPushButton *cancelButton() const { return m_cancel; }
    // Only the Exit action has this box: it is the one choice the preferences can remember.
    QCheckBox *neverShowAgainBox() const { return m_never; }

    void accept() override;

protected:
    void showEvent(QShowEvent *event) override;

private:
    void updateText();
    void onTick();

    ShutdownDialogAction m_action;
    int m_secondsLeft;
    QString m_message;
    QLabel *m_text = nullptr;
    QCheckBox *m_never = nullptr;
    QPushButton *m_confirm = nullptr;
    QPushButton *m_cancel = nullptr;
    QTimer *m_timer = nullptr;
};

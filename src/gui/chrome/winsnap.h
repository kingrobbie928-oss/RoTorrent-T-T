// SPDX-License-Identifier: GPL-2.0-or-later
// Windows Snap for the frameless main window (handoff 8.3). Port of the demo's winsnap.py.
//
// Windows only offers Snap to windows that carry WS_THICKFRAME / WS_MAXIMIZEBOX. Qt's
// FramelessWindowHint creates a plain popup without them, so we (1) add the styles after the
// native window exists and (2) answer WM_NCCALCSIZE so the client area still covers the whole
// window (no native frame). While maximized the window rectangle spills past the screen, so the
// client area is clamped to the monitor's work area (this also keeps the taskbar visible).
// Everything is a no-op on other platforms and when ROTORRENT_NO_SNAP is set.
#pragma once

#include <QByteArray>
#include <QtGlobal>

namespace RoWinSnap
{
    // Call once after the native window exists, with the HWND (QWidget::winId()).
    void enable(quintptr hwnd);

    // Call from QWidget::nativeEvent. Returns true when the message was handled.
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result);
}

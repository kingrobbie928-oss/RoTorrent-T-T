// SPDX-License-Identifier: GPL-2.0-or-later
#include "winsnap.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace
{
    bool snapEnabled()
    {
        static const bool on = !qEnvironmentVariableIsSet("ROTORRENT_NO_SNAP");
        return on;
    }
}

void RoWinSnap::enable(const quintptr hwndValue)
{
    if (!snapEnabled() || (hwndValue == 0))
        return;

    // A failure here must never stop the application from starting.
    const HWND hwnd = reinterpret_cast<HWND>(hwndValue);
    const LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
    ::SetWindowLongPtrW(hwnd, GWL_STYLE, style | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0
                   , SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

bool RoWinSnap::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (!snapEnabled() || (eventType != "windows_generic_MSG") || !message || !result)
        return false;

    const MSG *msg = static_cast<const MSG *>(message);

    // Fix for the blank taskbar: after a Snap-maximize, Windows sizes a WS_THICKFRAME window to the whole
    // monitor (plus the invisible frame). A window whose rectangle covers the monitor is treated as a
    // full-screen app, so the taskbar is hidden/blanked even though our client area stops at the work
    // area. Tell Windows the maximized rectangle is the work area. Qt still gets the message afterwards
    // (we return false) so it can fill in the minimum / maximum tracking sizes.
    if (msg->message == WM_GETMINMAXINFO)
    {
        const HMONITOR monitor = ::MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info {};
        info.cbSize = sizeof(info);
        if (monitor && ::GetMonitorInfoW(monitor, &info))
        {
            auto *mmi = reinterpret_cast<MINMAXINFO *>(msg->lParam);
            mmi->ptMaxPosition.x = info.rcWork.left - info.rcMonitor.left;
            mmi->ptMaxPosition.y = info.rcWork.top - info.rcMonitor.top;
            mmi->ptMaxSize.x = info.rcWork.right - info.rcWork.left;
            mmi->ptMaxSize.y = info.rcWork.bottom - info.rcWork.top;
        }
        return false;
    }

    if ((msg->message != WM_NCCALCSIZE) || !msg->wParam)
        return false;

    if (::IsZoomed(msg->hwnd))
    {
        // Maximized: the client area is the monitor's work area (taskbar stays visible).
        auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
        const HMONITOR monitor = ::MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info {};
        info.cbSize = sizeof(info);
        if (monitor && ::GetMonitorInfoW(monitor, &info))
            params->rgrc[0] = info.rcWork;
    }

    *result = 0;   // 0 = the whole window is client area: no native frame
    return true;
}

#else  // not Windows: Snap does not apply

void RoWinSnap::enable(quintptr) {}
bool RoWinSnap::nativeEvent(const QByteArray &, void *, qintptr *) { return false; }

#endif

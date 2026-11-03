// SPDX-License-Identifier: GPL-2.0-or-later
// Statistics, Speed limits and About dialogs (handoff 8.12). Licensed GPL-2.0-or-later.

#pragma once

class QWidget;
class MainWindow;

namespace BitTorrent
{
    class Session;
}

namespace StatisticsDialog
{
    // 420 px, four sections, live values from the session (refreshed every 1.5 s while open).
    void show(QWidget *parent, BitTorrent::Session *session);
}

namespace SpeedLimitsDialog
{
    // Global and alternative limits in KiB/s, 0 = unlimited. Applies on OK; returns true if accepted.
    bool show(QWidget *parent, BitTorrent::Session *session);
}

namespace AboutDialog
{
    void show(QWidget *parent);
}

namespace InfoDialogs
{
    // Connects the Statistics, Speed limits and About hooks of the window.
    void install(MainWindow &window, BitTorrent::Session *session);
}

// SPDX-License-Identifier: GPL-2.0-or-later
// Connects Tools > Options, the toolbar Settings button and Alt+O to the Options dialog, and applies the
// settings that change what the main window shows. Licensed GPL-2.0-or-later.
#pragma once

class IApplication;
class MainWindow;

namespace OptionsLauncher
{
    void install(MainWindow &window, IApplication *app);

    // Re-reads the settings the window follows: the queueing controls, the details refresh interval and the
    // window title suffix (Advanced > "Customize application instance name"). Called once at start-up and
    // after every Apply / OK. The colour scheme needs nothing here: RoTheme repaints in place.
    void applyLiveEffects(MainWindow &window, const IApplication *app);
}

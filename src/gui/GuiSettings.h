// SPDX-License-Identifier: GPL-2.0-or-later
// GUI-only settings that have no home in the ported backend's Preferences (Phase 7).
// Stored in RoTorrent.ini under the Gui/ group (handoff 7.1). Licensed GPL-2.0-or-later.
#pragma once

#include <QtGlobal>

// What a double-click on a torrent does (values are the ones the preferences store; same as upstream).
enum DoubleClickAction
{
    TOGGLE_STOP = 0,   // "Pause / resume torrent" (the stored name is upstream's)
    OPEN_DEST = 1,
    PREVIEW_FILE = 2,
    NO_ACTION = 3,
    SHOW_OPTIONS = 4
};

namespace GuiSettings
{
    // Options > Behavior > "RoTorrent window state on start up".
    enum class WindowState
    {
        Normal = 0,
        Minimized = 1,
        Hidden = 2   // honoured once the tray exists (Phase 8); until then it opens minimized
    };

    // Options > Advanced > "Display notifications" (on by default) and "...for added torrents" (off).
    // RoTray::notify() reads these (Phase 8).
    bool notificationsEnabled();
    void setNotificationsEnabled(bool enabled);
    bool torrentAddedNotificationsEnabled();
    void setTorrentAddedNotificationsEnabled(bool enabled);

    WindowState startUpWindowState();
    void setStartUpWindowState(WindowState state);
}

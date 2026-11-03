// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "GuiSettings.h"

#include "base/settingvalue.h"

namespace
{
    const QString START_UP_WINDOW_STATE_KEY = QStringLiteral("Gui/StartUpWindowState");
    const QString NOTIFICATIONS_KEY = QStringLiteral("Gui/Notifications/Enabled");
    const QString TORRENT_ADDED_NOTIFICATIONS_KEY = QStringLiteral("Gui/Notifications/TorrentAdded");
}

bool GuiSettings::notificationsEnabled()
{
    return SettingsStorage::instance()->loadValue<bool>(NOTIFICATIONS_KEY, true);
}

void GuiSettings::setNotificationsEnabled(const bool enabled)
{
    SettingsStorage::instance()->storeValue(NOTIFICATIONS_KEY, enabled);
}

bool GuiSettings::torrentAddedNotificationsEnabled()
{
    return SettingsStorage::instance()->loadValue<bool>(TORRENT_ADDED_NOTIFICATIONS_KEY, false);
}

void GuiSettings::setTorrentAddedNotificationsEnabled(const bool enabled)
{
    SettingsStorage::instance()->storeValue(TORRENT_ADDED_NOTIFICATIONS_KEY, enabled);
}

GuiSettings::WindowState GuiSettings::startUpWindowState()
{
    const int value = SettingsStorage::instance()->loadValue<int>(START_UP_WINDOW_STATE_KEY, 0);
    if ((value < 0) || (value > static_cast<int>(WindowState::Hidden)))
        return WindowState::Normal;
    return static_cast<WindowState>(value);
}

void GuiSettings::setStartUpWindowState(const WindowState state)
{
    SettingsStorage::instance()->storeValue(START_UP_WINDOW_STATE_KEY, static_cast<int>(state));
}

// SPDX-License-Identifier: GPL-2.0-or-later
// Turns session events into toasts (handoff 8.11): Download completed, I/O Error, Torrent added. The engine
// already writes the matching Log lines ("Torrent download finished", "File error alert", "Added new torrent"),
// so nothing is logged twice. Licensed GPL-2.0-or-later.
#pragma once

#include <QObject>

class RoTray;

namespace BitTorrent
{
    class Session;
}

class NotificationFeed final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(NotificationFeed)

public:
    NotificationFeed(BitTorrent::Session *session, RoTray *tray);
};

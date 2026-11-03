// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "NotificationFeed.h"

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "RoTray.h"

NotificationFeed::NotificationFeed(BitTorrent::Session *session, RoTray *tray)
    : QObject(tray)
{
    using BitTorrent::Session;
    connect(session, &Session::torrentFinished, this, [tray](const BitTorrent::Torrent *torrent)
    {
        tray->notify(tr("Download completed")
                     , tr("'%1' has finished downloading.", "e.g: xxx.avi has finished downloading.").arg(torrent->name())
                     , NoticeKind::Ok);
    });
    connect(session, &Session::torrentIOError, this
            , [tray](const BitTorrent::Torrent *torrent, const QString &message)
    {
        tray->notify(tr("I/O Error", "i.e: Input/Output Error")
                     , tr("An I/O error occurred for torrent '%1'.\n Reason: %2"
                          , "e.g: An error occurred for torrent 'xxx.avi'.\n Reason: disk is full.")
                       .arg(torrent->name(), message)
                     , NoticeKind::Crit);
    });
    connect(session, &Session::torrentAdded, this, [tray](const BitTorrent::Torrent *torrent)
    {
        tray->notify(tr("Torrent added"), tr("'%1' was added.", "e.g: xxx.avi was added.").arg(torrent->name())
                     , NoticeKind::Info, true);
    });
}

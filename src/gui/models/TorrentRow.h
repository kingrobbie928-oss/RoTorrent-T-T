// SPDX-License-Identifier: GPL-2.0-or-later
// One row of the transfer list, as plain data (handoff 8.7). The model never touches the engine:
// SessionAdapter fills these from BitTorrent::Torrent, DemoSource fills them from JSON or a generator,
// so every row state can be rendered and tested without a live session.
// Licensed GPL-2.0-or-later.
#pragma once

#include <QMetaType>
#include <QString>

#include "rostatus.h"

struct TorrentRow
{
    QString id;                 // stable key (info hash text)
    QString name;
    qint64 totalSize = 0;
    double progress = 0;        // 0..100
    RoStatus status = RoStatus::Paused;
    qint64 downRate = 0;        // bytes/s
    qint64 upRate = 0;
    qint64 etaSeconds = -1;     // -1 = unknown / infinite
    double ratio = 0;
    int queuePos = -1;          // engine queue position, -1 for finished torrents
    QString category;           // empty = uncategorised
    QString trackerHost;        // first tracker's host, empty = trackerless

    // Display strings. SessionAdapter formats them with the engine's own helpers; DemoSource may
    // set them verbatim from the demo JSON. The model sorts on the numeric fields above.
    QString sizeText;
    QString downText;
    QString upText;
    QString etaText;
    QString ratioText;

    bool finished() const { return progress >= 100.0; }
};

Q_DECLARE_METATYPE(const TorrentRow *)

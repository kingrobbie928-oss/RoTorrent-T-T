// SPDX-License-Identifier: GPL-2.0-or-later
// Plain data for the details panel (handoff 8.9). The tabs never touch the engine: a DetailsSource
// (SessionDetailsSource on a live session, DemoDetailsSource for the preview and tests) fills these.
// Licensed GPL-2.0-or-later.
#pragma once

#include <QBitArray>
#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>

#include "models/rostatus.h"

namespace Details
{
    // Values match BitTorrent::DownloadPriority so they can be handed to the engine unchanged.
    enum Priority : int { Skip = 0, Normal = 1, High = 6, Maximum = 7, Mixed = -1 };

    struct General
    {
        bool valid = false;
        QString id;
        QString name;
        RoStatus status = RoStatus::Paused;
        bool hasMetadata = true;

        // TRANSFER
        qint64 timeActiveSecs = 0;
        qint64 etaSecs = -1;              // -1 = unknown
        int connections = 0;
        int connectionsLimit = -1;        // <= 0: unlimited
        qint64 downloaded = 0;            // all-time (totalDownload)
        qint64 uploaded = 0;
        qint64 downloadedSession = 0;
        qint64 uploadedSession = 0;
        int seeds = 0, seedsTotal = 0;
        int peers = 0, peersTotal = 0;
        qint64 downSpeed = 0, upSpeed = 0;
        qint64 downLimit = 0, upLimit = 0;   // bytes/s, <= 0: unlimited
        qint64 wasted = 0;
        double ratio = 0;                 // < 0: unknown
        qint64 reannounceInSecs = -1;
        QDateTime lastSeenComplete;       // invalid = never
        double popularity = 0;

        // INFORMATION
        qint64 totalSize = 0;
        int piecesCount = 0;
        qint64 pieceLength = 0;
        int piecesHave = 0;
        QString createdBy;
        QDateTime addedOn, completedOn, createdOn;
        bool isPrivate = false;
        QString hashV1, hashV2, savePath, comment;

        // Bars. `pieces` is the have-bitmap; `availability` is filled by the async request
        // (copies per piece, -1 / empty when unknown).
        QBitArray pieces;
        QList<int> availability;
        double progress = 0;              // 0..1
    };

    enum class TrackerState { NotContacted, Working, Updating, NotWorking, Disabled };

    struct Tracker
    {
        QString url;                      // real URL, or "** [DHT] **" for the three pseudo rows
        int tier = -1;                    // -1 for pseudo rows
        bool pseudo = false;
        QString btProtocol;               // "v1", "v2", "v1 & v2" or ""
        TrackerState state = TrackerState::NotContacted;
        int peers = -1, seeds = -1, leeches = -1, downloaded = -1;   // -1 = N/A
        QString message;
        qint64 nextAnnounceSecs = -1, minAnnounceSecs = -1;
    };

    struct Peer
    {
        QString ip;
        int port = 0;
        QString country;                  // display name, may be empty
        QString countryCode;
        QString connection;               // "BT" or "µTP"
        QString flags;
        QString flagsDescription;
        QString client;
        double progress = 0;              // 0..1
        qint64 downSpeed = 0, upSpeed = 0, downloaded = 0, uploaded = 0;
        double relevance = 0;
        QString endpoint() const { return ip.contains(QLatin1Char(':')) ? QStringLiteral("[%1]:%2").arg(ip).arg(port) : QStringLiteral("%1:%2").arg(ip).arg(port); }
    };

    struct WebSeed
    {
        QString url;
    };

    struct File
    {
        int index = 0;
        QString path;                     // relative path with '/' separators
        qint64 size = 0;
        double progress = 0;              // 0..1
        Priority priority = Normal;
        double availability = -1;         // 0..1+, -1 = unknown
    };
}

Q_DECLARE_METATYPE(Details::General)

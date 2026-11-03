// SPDX-License-Identifier: GPL-2.0-or-later
// RoTorrent: user-facing torrent status (handoff section 6.5).
// One pure function maps the engine's 19 TorrentState values to nine statuses.
// The sidebar filters and the table badge both use it so counts always agree.
// Licensed GPL-2.0-or-later (derived from qBittorrent).
#pragma once

#include "base/bittorrent/torrent.h"

enum class RoStatus
{
    Downloading,   // dl
    Seeding,       // sd
    Completed,     // fn  (finished and stopped)            ASSUMPTION per 6.5
    Paused,        // st  (unfinished and stopped)
    Queued,        // qu
    Checking,      // ck  (also "Moving", label differs)    ASSUMPTION per 6.5
    Stalled,       // pa
    Error,         // er
    MissingFiles   // mf  (sidebar counts it under Error)
};

namespace RoStatusInfo
{
    // Two-letter code used by the demo and by QSS dynamic properties.
    constexpr const char *code(const RoStatus s)
    {
        switch (s)
        {
        case RoStatus::Downloading: return "dl";
        case RoStatus::Seeding: return "sd";
        case RoStatus::Completed: return "fn";
        case RoStatus::Paused: return "st";
        case RoStatus::Queued: return "qu";
        case RoStatus::Checking: return "ck";
        case RoStatus::Stalled: return "pa";
        case RoStatus::Error: return "er";
        case RoStatus::MissingFiles: return "mf";
        }
        return "";
    }

    // Sidebar "Error" row counts both Error and MissingFiles.
    constexpr bool countsAsErrorInSidebar(const RoStatus s)
    {
        return (s == RoStatus::Error) || (s == RoStatus::MissingFiles);
    }
}

constexpr RoStatus statusOf(const BitTorrent::TorrentState state)
{
    using BitTorrent::TorrentState;
    switch (state)
    {
    case TorrentState::Downloading:
    case TorrentState::ForcedDownloading:
    case TorrentState::DownloadingMetadata:
    case TorrentState::ForcedDownloadingMetadata:
        return RoStatus::Downloading;
    case TorrentState::Uploading:
    case TorrentState::ForcedUploading:
        return RoStatus::Seeding;
    case TorrentState::StoppedUploading:
        return RoStatus::Completed;
    case TorrentState::StoppedDownloading:
        return RoStatus::Paused;
    case TorrentState::QueuedDownloading:
    case TorrentState::QueuedUploading:
        return RoStatus::Queued;
    case TorrentState::CheckingResumeData:
    case TorrentState::CheckingDownloading:
    case TorrentState::CheckingUploading:
    case TorrentState::Moving:
        return RoStatus::Checking;
    case TorrentState::StalledDownloading:
    case TorrentState::StalledUploading:
        return RoStatus::Stalled;
    case TorrentState::MissingFiles:
        return RoStatus::MissingFiles;
    case TorrentState::Error:
        return RoStatus::Error;
    case TorrentState::Unknown:
        break;
    }
    return RoStatus::Checking;  // Unknown is transient (session not ready): show as Checking. ASSUMPTION
}

inline RoStatus statusOf(const BitTorrent::Torrent *const torrent)
{
    return statusOf(torrent->state());
}

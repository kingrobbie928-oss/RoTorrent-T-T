// SPDX-License-Identifier: GPL-2.0-or-later
// Unit test: every engine TorrentState maps to the status table in handoff 6.5.
#include <cstdio>
#include "gui/models/rostatus.h"

using BitTorrent::TorrentState;

namespace
{
    int failures = 0;

    void expect(const TorrentState s, const RoStatus want, const char *name)
    {
        if (statusOf(s) != want)
        {
            std::printf("FAIL %-28s got %s want %s\n", name
                , RoStatusInfo::code(statusOf(s)), RoStatusInfo::code(want));
            ++failures;
        }
    }
}

#define CASE(state, want) expect(TorrentState::state, RoStatus::want, #state)

int main()
{
    CASE(Downloading, Downloading);
    CASE(ForcedDownloading, Downloading);
    CASE(DownloadingMetadata, Downloading);
    CASE(ForcedDownloadingMetadata, Downloading);
    CASE(Uploading, Seeding);
    CASE(ForcedUploading, Seeding);
    CASE(StoppedUploading, Completed);
    CASE(StoppedDownloading, Paused);
    CASE(QueuedDownloading, Queued);
    CASE(QueuedUploading, Queued);
    CASE(CheckingResumeData, Checking);
    CASE(CheckingDownloading, Checking);
    CASE(CheckingUploading, Checking);
    CASE(Moving, Checking);
    CASE(StalledDownloading, Stalled);
    CASE(StalledUploading, Stalled);
    CASE(Error, Error);
    CASE(MissingFiles, MissingFiles);
    CASE(Unknown, Checking);

    static_assert(RoStatusInfo::countsAsErrorInSidebar(RoStatus::MissingFiles));
    static_assert(!RoStatusInfo::countsAsErrorInSidebar(RoStatus::Paused));

    std::printf(failures ? "%d FAILED\n" : "all 19 states OK\n", failures);
    return failures ? 1 : 0;
}

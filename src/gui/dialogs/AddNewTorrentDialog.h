// SPDX-License-Identifier: GPL-2.0-or-later
// "Add New Torrent" dialog (handoff 8.12, addnewtorrentdialog.ui fields).
// Shown once per chosen .torrent file or magnet link. Licensed GPL-2.0-or-later.

#pragma once

#include <optional>

#include <QString>

#include "base/bittorrent/addtorrentparams.h"
#include "base/bittorrent/torrentdescriptor.h"

class QWidget;

namespace BitTorrent
{
    class Session;
}

namespace AddNewTorrentDialog
{
    struct Result
    {
        // The descriptor to add. For a magnet link it carries the metadata when that arrived while the
        // dialog was open (then the file selection in `params` refers to it).
        BitTorrent::TorrentDescriptor descriptor;
        BitTorrent::AddTorrentParams params;
        bool keepTorrentFile = false;   // "Do not delete .torrent file" ticked (only offered for files)
    };

    // Runs the dialog modally. Returns nullopt when the user cancelled.
    // For a magnet link the dialog asks the engine for the metadata ("Retrieving metadata...") and
    // fills the file tree when it arrives; OK stays available meanwhile (the torrent is then added
    // without a file selection). `sourcePath` is the .torrent file path, empty for magnets.
    // `initial` pre-fills the fields (command line: --save-path, --category, --add-paused, ...) and every
    // field it carries that the dialog has no widget for (tags, limits) is returned unchanged in the result.
    std::optional<Result> run(QWidget *parent, BitTorrent::Session *session
                              , const BitTorrent::TorrentDescriptor &descriptor, const QString &sourcePath = {}
                              , const BitTorrent::AddTorrentParams &initial = {});

    // "Never show again": when false the flow adds torrents straight away with the session defaults.
    bool isEnabled();
    void setEnabled(bool enabled);
}

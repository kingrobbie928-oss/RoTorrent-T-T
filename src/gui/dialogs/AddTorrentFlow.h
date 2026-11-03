// SPDX-License-Identifier: GPL-2.0-or-later
// Connects File > Add Torrent File / Add Torrent Link (and the toolbar buttons) to the real engine:
// file picker first, then the Add New Torrent dialog per file (handoff 8.12). Licensed GPL-2.0-or-later.

#pragma once

#include <functional>
#include <optional>

#include <QStringList>

#include "base/bittorrent/addtorrentparams.h"

class MainWindow;

namespace BitTorrent
{
    class Session;
}

namespace AddTorrentFlow
{
    // Optional override for http(s) / ftp links (return true when handled). Without it, or when it returns
    // false, the flow downloads the torrent itself with Net::DownloadManager (cookies, proxy and size limit
    // from the preferences) and then shows the Add New Torrent dialog like for a file.
    using UrlHandler = std::function<bool(const QString &url)>;

    // How a batch of sources is added when it does not come from the menus: the command line, a second
    // instance, drag and drop. `params` pre-fills the Add New Torrent dialog (or is used as given when the
    // dialog is skipped); `skipDialog` true adds without the dialog, false shows it even when "Never show
    // again" is on, and unset follows that setting.
    struct Options
    {
        BitTorrent::AddTorrentParams params;
        std::optional<bool> skipDialog;
    };

    void install(MainWindow &window, BitTorrent::Session *session, UrlHandler urlHandler = {});

    // The same work, callable from drag and drop or a second instance. Each entry is a .torrent path,
    // a magnet link or an info-hash.
    void addSources(MainWindow &window, BitTorrent::Session *session, const QStringList &sources
                    , const UrlHandler &urlHandler = {});
    void addSources(MainWindow &window, BitTorrent::Session *session, const QStringList &sources
                    , const Options &options, const UrlHandler &urlHandler = {});
}

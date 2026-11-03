// SPDX-License-Identifier: GPL-2.0-or-later
// "Add torrent links" dialog (handoff 8.12, downloadfromurldialog.ui layout).
// Licensed GPL-2.0-or-later.

#pragma once

#include <optional>

#include <QStringList>

class QWidget;

namespace AddLinksDialog
{
    // One entry per non-empty line, duplicates removed, order kept. nullopt when cancelled.
    std::optional<QStringList> getLinks(QWidget *parent, const QString &initialText = {});

    // Accepted link kinds: magnet URIs, http(s) / ftp URLs, 40 or 64 digit info-hashes.
    bool isSupportedLink(const QString &link);
}

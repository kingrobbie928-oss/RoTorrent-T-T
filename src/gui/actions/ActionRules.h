// SPDX-License-Identifier: GPL-2.0-or-later
// Pure enable / visibility rules for torrent commands (handoff 6.5 last bullet, 8.8).
// They reproduce qBittorrent's TransferListWidget::displayListMenu logic over a list of facts, so
// the rules are unit-testable without a session and work for any selection size (multi-select is a
// one-line switch in the view). Licensed GPL-2.0-or-later (derived from qBittorrent).
#pragma once

#include <QList>
#include <QString>
#include <Qt>

struct TorrentFacts
{
    QString id;
    QString category;
    bool hasMetadata = true;
    bool finished = false;
    bool stopped = false;
    bool forced = false;
    bool errored = false;
    bool missingFiles = false;
    bool checking = false;
    bool queued = false;
    bool autoTMM = false;
    bool sequential = false;
    bool firstLast = false;
    bool superSeeding = false;
    bool hasInfoHashV1 = true;
    bool hasInfoHashV2 = false;
};

struct CommandPlan
{
    bool any = false;                 // something is selected
    bool resume = false;              // "Resume" offered / enabled
    bool pause = false;               // "Pause" offered / enabled
    bool forceResume = false;
    bool single = false;              // Rename... and Manage content... only for one torrent
    bool anyUnfinished = false;       // Queue commands, sequential, first/last
    bool anyMetadata = false;         // Preview, Force recheck
    bool showSuperSeeding = false;    // only while every selected torrent is finished
    bool canReannounce = false;
    bool copyInfoHashV1 = false;
    bool copyInfoHashV2 = false;
    bool allSameCategory = true;
    QString commonCategory;
    Qt::CheckState autoTMM = Qt::Unchecked;
    Qt::CheckState superSeeding = Qt::Unchecked;
    Qt::CheckState sequential = Qt::Unchecked;
    Qt::CheckState firstLast = Qt::Unchecked;
};

namespace ActionRules
{
    CommandPlan plan(const QList<TorrentFacts> &selection);

    // Queue commands exist only while the queueing system is on and an unfinished torrent is selected.
    inline bool queueCommandsEnabled(const CommandPlan &p, const bool queueingOn)
    {
        return queueingOn && p.any && p.anyUnfinished;
    }
}

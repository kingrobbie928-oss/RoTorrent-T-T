// SPDX-License-Identifier: GPL-2.0-or-later
// The add-trackers, add-peers and add-HTTP-source dialogs (handoff 8.12 "Tracker and peer dialogs";
// layouts follow trackersadditiondialog.ui, peersadditiondialog.ui and autoexpandabledialog.ui).
// All use the RoDialog shell. Licensed GPL-2.0-or-later.
#pragma once

#include <optional>

#include <QString>
#include <QStringList>

class QWidget;

namespace DetailsDialogs
{
    // Each returns the non-empty trimmed lines, or nullopt when cancelled / nothing was entered.
    std::optional<QStringList> addTrackers(QWidget *parent);
    std::optional<QStringList> addPeers(QWidget *parent);
    std::optional<QStringList> addHttpSources(QWidget *parent);

    // Shared helper, public for tests: split on newlines, trim, drop empties and duplicates.
    QStringList cleanLines(const QString &text);
}

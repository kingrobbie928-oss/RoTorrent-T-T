// SPDX-License-Identifier: GPL-2.0-or-later
// Deterministic rows for development, screenshots and tests: the kit's torrents.json, and a
// generator that cycles through every status so each row state can be rendered without an engine.
#pragma once

#include <QList>
#include <QString>

#include "TorrentRow.h"

namespace DemoSource
{
    // rows from the kit's data/torrents.json (names, texts and statuses are taken verbatim).
    QList<TorrentRow> fromJson(const QString &path, QString *error = nullptr);

    // `count` rows cycling through all nine statuses, with categories and trackers, reproducible by seed.
    QList<TorrentRow> generate(int count, unsigned seed = 7);
}

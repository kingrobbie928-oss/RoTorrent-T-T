// SPDX-License-Identifier: GPL-2.0-or-later
// RoTorrent first-run defaults. Replaces qBittorrent's upgrade.{h,cpp}: RoTorrent has no
// legacy settings to migrate, so only the "current" defaults for a fresh profile are kept.
// Licensed GPL-2.0-or-later (derived from qBittorrent).
#pragma once

// Writes the defaults that differ from the getters' built-in fallbacks into settings.
// Must run once, for a profile with no settings yet.
void applyFirstRunDefaults();

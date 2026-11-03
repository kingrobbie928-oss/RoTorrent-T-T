#!/bin/sh
# Phase 9 compliance: every source file carries a licence marker, the GPL texts and attributions are present, and the
# version is single-sourced. Third-party / upstream headers are never touched.
cd "$(dirname "$0")/.." || exit 2
rc=0
for f in COPYING COPYING.GPLv2 COPYING.GPLv3 AUTHORS src/resources/ro/fonts/OFL.txt docs/UPSTREAM.md docs/THIRD_PARTY.md docs/IDENTITY.md; do
    [ -s "$f" ] || { echo "MISSING: $f"; rc=1; }
done
for f in $(find src test -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) ! -name version.h); do
    head -c 3000 "$f" | grep -qiE 'copyright|SPDX-License-Identifier|Licensed GPL' || { echo "NO LICENCE MARKER: $f"; rc=1; }
done
grep -q "Based on qBittorrent\|based on qBittorrent" src/gui/dialogs/AboutDialog.cpp || { echo "About dialog does not credit qBittorrent"; rc=1; }
grep -q "4df3aff937992c371fe02fd66133eb0c9d825830" docs/UPSTREAM.md || { echo "UPSTREAM.md lacks the pinned commit"; rc=1; }
grep -q "@PROJECT_VERSION_MAJOR@" src/base/version.h.in || { echo "version.h.in is not driven by project(VERSION)"; rc=1; }
[ $rc -eq 0 ] && echo "OK: licence markers, GPL texts, font licence, upstream pin, attribution, single-sourced version" || echo "FAIL: licence checks"
exit $rc

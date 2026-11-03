#!/bin/sh
# Phase 1 "done when": src/base must not reference Qt Widgets, RSS, search, plugins or the Web UI.
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT/src/base" || exit 2
HITS=$(grep -rnE '#include *[<"](QtWidgets|QWidget|QApplication|QMainWindow|QDialog)|base/(rss|search|plugins)/|webui/|RSS::|SearchPluginManager|PluginsEngine|DISABLE_WEBUI|ENABLE_PLUGINS' . | grep -v '^\./CMakeLists.txt')
if [ -n "$HITS" ]; then echo "$HITS"; echo "FAIL: forbidden references in src/base"; exit 1; fi
echo "OK: src/base is clean"

# Phase 7: no Search, RSS or Web UI key may remain in Preferences, and no page, control or code for them
# in the Options dialog (comments and the trimming tool that explains the removal are exempt).
cd "$ROOT" || exit 2
PREF=$(grep -niE 'webui|web_ui|\brss\b|searchengine|search_engine|\bplugin' src/base/preferences.h src/base/preferences.cpp)
OPTS=$(grep -rniE 'webui|\brss\b|search' src/gui/options --include=*.cpp --include=*.h --include=*.ui \
       | grep -v -E ':[0-9]+: *(//|\*)')
if [ -n "$PREF$OPTS" ]; then echo "$PREF"; echo "$OPTS"; echo "FAIL: Search / RSS / Web UI left in Preferences or Options"; exit 1; fi
echo "OK: no Search, RSS or Web UI in Preferences or Options"

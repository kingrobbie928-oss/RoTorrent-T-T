#!/bin/sh
# Phase 4 "done when": no "Start", "Stop", "Stopped", "Force Start", "Finished" or "qBittorrent" in a
# user-visible string of the GUI (Appendix C). Looks at every tr("...") / QStringLiteral literal in src/gui.
# Exceptions: the About "Based on" credit and licence text (file About*), and demo data files (DemoSource,
# DemoDetailsSource: fake peer client names that never ship in a real session).
cd "$(dirname "$0")/../src/gui" || exit 2
HITS=$(grep -rnE '(tr|QT_TR_NOOP|QStringLiteral)\("[^"]*(\bStart\b|\bStop\b|\bStopped\b|Force Start|\bFinished\b|qBittorrent)[^"]*"' . \
       | grep -v -E '/About|toolTip|\.ui:|/DemoDetailsSource|/DemoSource' )
if [ -n "$HITS" ]; then echo "$HITS"; echo "FAIL: forbidden wording in user-visible strings"; exit 1; fi
# Phase 7: Options is mostly .ui text and tooltips, which the pass above skips. Scan it strictly: every
# <string> of the .ui files and every quoted literal of the Options sources, tooltips included.
WORDS='\bStart\b|\bStop\b|\bStopped\b|Force Start|\bFinished\b|qBittorrent'
UIHITS=$(grep -nE "<string>[^<]*($WORDS)" options/*.ui)
CPPHITS=$(grep -nE "\"[^\"]*($WORDS)[^\"]*\"" options/*.cpp | grep -v -E 'Copyright|^[^:]*:[0-9]+: *(//|\*)')
if [ -n "$UIHITS$CPPHITS" ]; then echo "$UIHITS"; echo "$CPPHITS"; echo "FAIL: forbidden wording in the Options dialog"; exit 1; fi
echo "OK: label dictionary respected in src/gui (Options .ui text and tooltips included)"

#!/bin/sh
# Runs test_chrome offscreen at every display scale listed in handoff 8.3.
# Usage: run_chrome.sh <path-to-test_chrome>
BIN="$1"
export QT_QPA_PLATFORM=offscreen
for S in 1 1.25 1.5 1.75 2.25 2.5 3; do
    echo "== scale $S =="
    QT_SCALE_FACTOR=$S "$BIN" | grep -E "FAIL|OK|FAILED" || exit 1
done

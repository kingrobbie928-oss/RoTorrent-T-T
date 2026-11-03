#!/bin/sh
# Runs the Phase 1 smoke test twice: once with a clean shutdown, once with a forced kill.
# Usage: run_smoke.sh <path-to-smoke_session>
set -e
BIN="$1"
for MODE in setup-clean setup-kill; do
    W=$(mktemp -d)
    echo "== $MODE =="
    "$BIN" "$W/profile" "$W/data" "$MODE"
    "$BIN" "$W/profile" "$W/data" verify
    rm -rf "$W"
done
echo "SMOKE OK"

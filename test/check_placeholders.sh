#!/bin/sh
# Phase 9: lists every placeholder that must be replaced before a public release (docs/PLACEHOLDERS.md).
#   check_placeholders.sh            report only (exit 0)
#   RELEASE=1 check_placeholders.sh  exit 1 while any placeholder remains: the release gate
cd "$(dirname "$0")/.." || exit 2
HITS=$(grep -rIn -E 'rotorrent\.example|team@rotorrent' src dist --include=* 2>/dev/null | grep -v -E '^\S+:[0-9]+:\s*//\s*[A-Za-z{"]')
N=$(echo "$HITS" | grep -c -E '^[^:]+:[0-9]+:')
echo "$HITS" | grep -E '^[^:]+:[0-9]+:' | sed -E 's/^([^:]+):[0-9]+:.*(https?:\/\/[A-Za-z0-9._\/-]*rotorrent\.example[A-Za-z0-9._\/-]*|team@rotorrent\.example).*/\1  \2/' | sort -u
echo "-- $N placeholder line(s)"
if [ "$RELEASE" = "1" ] && [ "$N" -gt 0 ]; then echo "FAIL: placeholders remain; fix them (docs/PLACEHOLDERS.md) before releasing"; exit 1; fi
exit 0

#!/bin/sh
# Phase 9: translations/rotorrent_en.ts must match what lupdate extracts from the shipped sources, so every
# user-visible string is translatable and nothing was added without being extracted.
#   check_translations.sh          verify (CI)
#   check_translations.sh --update rewrite the .ts file
# Needs Qt's lupdate (qt6-l10n-tools / Qt bin folder); skipped with a notice when it is not installed.
R="$(cd "$(dirname "$0")/.." && pwd)"; cd "$R" || exit 2
LUP="${LUPDATE:-$(command -v lupdate-qt6 || command -v lupdate || ls /usr/lib/qt6/bin/lupdate 2>/dev/null)}"
[ -x "$LUP" ] || { echo "SKIP: lupdate not found (set LUPDATE=...)"; exit 0; }
FILES=$(find src/gui src/app src/base -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.ui' \) \
        ! -name main_window_preview.cpp ! -name 'Demo*' | sort)
TS=translations/rotorrent_en.ts
OUT="$TS"; [ "$1" = "--update" ] || OUT="$(mktemp)"
cp "$TS" "$OUT" 2>/dev/null || true
LOG=$("$LUP" -no-obsolete -locations none $FILES -ts "$OUT" 2>&1)
if echo "$LOG" | grep -q "cannot be called without context"; then
    echo "$LOG" | grep "without context"
    echo "FAIL: a tr() / QT_TR_NOOP at file scope has no class context: use QT_TRANSLATE_NOOP(\"Class\", ...)"; exit 1
fi
if [ "$1" = "--update" ]; then echo "updated $TS"; exit 0; fi
# lupdate is run with -locations none, so the output is stable and a plain byte comparison is enough
if cmp -s "$TS" "$OUT"; then
    echo "OK: translations/rotorrent_en.ts is up to date ($(grep -c '<message' "$TS") strings)"; rm -f "$OUT"; exit 0
fi
rm -f "$OUT"; echo "FAIL: translations/rotorrent_en.ts is out of date. Run: test/check_translations.sh --update"; exit 1

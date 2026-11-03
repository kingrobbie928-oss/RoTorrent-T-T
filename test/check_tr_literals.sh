#!/bin/sh
# Phase 9: a user-visible setter must never be given a bare string literal. Catches setText("...")/setToolTip("...")
# and friends that skip tr(). The brand name alone ("RoTorrent") is allowed. Multi-line tr() calls are fine, and so are literals that only wrap markup (they start with "<").
cd "$(dirname "$0")/.." || exit 2
HITS=$(grep -rnE '\b(setText|setWindowTitle|setToolTip|setPlaceholderText|setStatusTip|setTitle|setLabelText|setWhatsThis|showMessage|setSpecialValueText|setSuffix|setPrefix)\(\s*(QStringLiteral\()?u?"[^"]*[A-Za-z]{3}' \
       src/gui src/app --include=*.cpp --include=*.h \
       | grep -v -E 'main_window_preview|/Demo|^\S+:[0-9]+:\s*//' \
       | grep -v -E '\((QStringLiteral\()?u?"<' \
       | grep -v -E '\((QStringLiteral\()?u?"RoTorrent"(_s)?\)?\)' )
if [ -n "$HITS" ]; then echo "$HITS"; echo "FAIL: user-visible text without tr()"; exit 1; fi
echo "OK: no user-visible setter takes an untranslated literal"

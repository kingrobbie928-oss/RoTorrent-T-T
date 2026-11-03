#!/bin/sh
# Runs every automated check. Usage: run_all.sh <build-dir>
# Needs a Qt platform that works headless: sets QT_QPA_PLATFORM=offscreen.
B="$1"; T="$(dirname "$0")"; export QT_QPA_PLATFORM=offscreen; rc=0
run() { echo "== $1"; shift; "$@" || { echo "FAILED: $*"; rc=1; }; }
run base-clean   "$T/check_base_clean.sh"
run labels       "$T/check_labels.sh"
run tr-literals  "$T/check_tr_literals.sh"
run translations "$T/check_translations.sh"
run licences     "$T/check_licences.sh"
run placeholders "$T/check_placeholders.sh"   # report only; RELEASE=1 makes it a gate (docs/RELEASE.md)
run rostatus     "$B/test/test_rostatus"
run actionrules  "$B/test/test_actionrules"
run chrome       "$T/run_chrome.sh" "$B/test/test_chrome"
run transferlist "$B/test/test_transferlist"
W=$(mktemp -d); run adapter "$B/test/test_adapter" "$W/profile" "$W/data"; rm -rf "$W"
W2=$(mktemp -d); run commands "$B/test/test_commands" "$W2/profile" "$W2/data"; rm -rf "$W2"
W3=$(mktemp -d); run addtorrent "$B/test/test_addtorrent" "$W3/profile" "$W3/data"; rm -rf "$W3"
W5=$(mktemp -d); run torrentcreator "$B/test/test_torrentcreator" "$W5/profile" "$W5/data"; rm -rf "$W5"
W4=$(mktemp -d); run infodialogs "$B/test/test_infodialogs" "$W4/profile" "$W4/data"; rm -rf "$W4"
W6=$(mktemp -d); run managementdialogs "$B/test/test_managementdialogs" "$W6/profile" "$W6/data"; rm -rf "$W6"
W8=$(mktemp -d); run options "$B/test/test_options" "$W8/profile" "$W8/data"; rm -rf "$W8"
W7=$(mktemp -d); run appdialogs "$B/test/test_appdialogs" "$W7/profile"; rm -rf "$W7"
run dragdrop      "$B/test/test_dragdrop"
run statusbar     "$B/test/test_statusbar"
run tray          "$B/test/test_tray"
run power         "$B/test/test_power"
run updatecheck   "$B/test/test_updatecheck"
W9=$(mktemp -d); run statusfeed "$B/test/test_statusfeed" "$W9/profile" "$W9/data"; rm -rf "$W9"
run instances    "$T/run_instances.sh" "$B/rotorrent"
run smoke        "$T/run_smoke.sh" "$B/test/smoke_session"
[ $rc -eq 0 ] && echo "ALL OK" || echo "SOME FAILED"
exit $rc

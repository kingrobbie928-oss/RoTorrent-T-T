#!/bin/sh
# Phase 6 slice 7: the real rotorrent executable, offscreen. Starts a first instance, then starts second
# instances that must forward to it and exit: a .torrent path with --skip-dialog, a magnet link, and no
# arguments at all (which raises the first window). Checks the torrents reach the first instance's profile.
# Usage: run_instances.sh <path-to-rotorrent>
BIN="$1"; export QT_QPA_PLATFORM=offscreen
W=$(mktemp -d); rc=0
fail() { echo "FAIL  $1"; rc=1; }
pass() { echo "PASS  $1"; }

python3 - "$W" <<'PY'
import hashlib, os, sys
def b(x):
    if isinstance(x, int): return b'i%de' % x
    if isinstance(x, bytes): return b'%d:' % len(x) + x
    if isinstance(x, str): return b(x.encode())
    if isinstance(x, dict): return b'd' + b''.join(b(k) + b(v) for k, v in sorted(x.items())) + b'e'
w = sys.argv[1]; data = os.urandom(70000); pl = 32768
open(w + '/sample.bin', 'wb').write(data)
pieces = b''.join(hashlib.sha1(data[i:i + pl]).digest() for i in range(0, len(data), pl))
open(w + '/sample.torrent', 'wb').write(b({'announce': 'http://127.0.0.1:9/announce',
    'info': {'name': 'sample.bin', 'length': len(data), 'piece length': pl, 'pieces': pieces}}))
PY
[ -f "$W/sample.torrent" ] || { echo "FAIL  could not generate the test torrent (python3 needed)"; exit 1; }

"$BIN" --profile="$W/profile" --confirm-legal-notice > "$W/first.log" 2>&1 &
FIRST=$!
sleep 8
kill -0 $FIRST 2>/dev/null && pass "first instance is running" || { fail "first instance died"; cat "$W/first.log"; exit 1; }

timeout 30 "$BIN" --profile="$W/profile" --skip-dialog=true --add-paused=true --save-path="$W" "$W/sample.torrent"
[ $? -eq 0 ] && pass "second instance with a .torrent forwards and exits 0" || fail "second instance (torrent) did not exit 0"

timeout 30 "$BIN" --profile="$W/profile" --skip-dialog=true --add-paused=true \
    "magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567&dn=testmagnet"
[ $? -eq 0 ] && pass "second instance with a magnet link forwards and exits 0" || fail "second instance (magnet) did not exit 0"

timeout 30 "$BIN" --profile="$W/profile"
[ $? -eq 0 ] && pass "second instance without arguments exits 0" || fail "second instance (no arguments) did not exit 0"

sleep 5
B="$W/profile/RoTorrent/data/BT_backup"
ls "$B"/*.torrent > /dev/null 2>&1 && pass "the .torrent reached the first instance's session" || fail "no .torrent stored by the first instance"
ls "$B"/0123456789abcdef0123456789abcdef01234567.fastresume > /dev/null 2>&1 && pass "the magnet reached the first instance's session" || fail "magnet not stored by the first instance"
kill -0 $FIRST 2>/dev/null && pass "first instance survived all of it" || fail "first instance died"

kill $FIRST 2>/dev/null; wait $FIRST 2>/dev/null
rm -rf "$W"
[ $rc -eq 0 ] && echo "INSTANCES OK" || echo "INSTANCES FAILED"
exit $rc

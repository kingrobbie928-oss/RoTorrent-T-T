# RoTorrent identity (handoff section 4.3)

| Item | Value | Where |
| --- | --- | --- |
| HTTP User-Agent | `RoTorrent/<version>` (1.0.0) | `sessionimpl.cpp`, `RT_PRODUCT_NAME` in `src/base/identity.h` |
| Peer-ID client code | `qB` (owner decision, overrides doc 4.3 default) | `RT_PEER_ID_CODE` in `src/base/identity.h` |
| Incomplete-file extension | `.!rt` | `RT_INCOMPLETE_EXT`, `QB_EXT` in `bittorrent/common.h` |
| Settings file | `RoTorrent.ini` | `settingsstorage.cpp` |
| Torrent creator tag | `RoTorrent 1.0.0` | `torrentcreator.cpp` |

## Owner decision: peer-ID code `qB`
The owner chose to keep qBittorrent's peer-ID code instead of a RoTorrent-specific one.
Peers and trackers will see peer IDs like `-qB1000-...` (RoTorrent's own version numbers),
while the HTTP User-Agent stays `RoTorrent/1.0.0`.

Things to be aware of:
- The doc's default was a distinct code, because `qB` identifies the client as qBittorrent.
- A `qB` code with a version that does not exist upstream, combined with a different
  User-Agent, can look inconsistent. Trackers that whitelist by version may still refuse it.
  If that happens, the fingerprint version can be pinned to a real qBittorrent release in
  `sessionimpl.cpp` (the `lt::generate_fingerprint` call).
- Some private trackers treat client spoofing as a rule violation. That risk is the owner's.

## Deliberately unchanged
- C++ namespaces and class names (keeps diffs against upstream easy).
- Resume-data dictionary keys `qBt-*` in `bencoderesumedatastorage.cpp`. They are local
  file keys, never sent to the network. Renaming them would break nothing today but
  would make upstream merges harder; revisit if you want them renamed.
- Copyright headers and the GPL text. RoTorrent is GPL-2.0-or-later; credit qBittorrent in About.

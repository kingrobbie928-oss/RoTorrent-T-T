# GPL compliance notes

This is a practical checklist, not legal advice; have the owner confirm anything commercial.

## Position
RoTorrent copies qBittorrent source (`src/base`, trimmed `src/app`) and is therefore a derivative work, released under
**GNU GPL v2 or later**. qBittorrent's own source is GPL v2 or later with an OpenSSL linking exception; its binary is GPL v3 or
later because of GPL v3 assets. RoTorrent uses none of those assets, so its binary stays GPL v2 or later.

## What is in place
| Obligation | Where |
|---|---|
| Keep original copyright headers and the OpenSSL exception | Untouched in every copied file (`test/check_licences.sh`) |
| Licence marker on RoTorrent's own files | `SPDX-License-Identifier: GPL-2.0-or-later` (144 files added in Phase 9) |
| GPL texts and AUTHORS shipped | `COPYING`, `COPYING.GPLv2`, `COPYING.GPLv3`, `AUTHORS` in the repo and in `licenses/` of every zip |
| Source with every binary | `RoTorrent-<ver>-source.zip` in the same release, plus `SOURCE-OFFER.txt` and the public repository |
| Credit qBittorrent | About dialog ("Based on qBittorrent project"), licence tab, `docs/UPSTREAM.md` |
| Do not pass off as qBittorrent | Own name, own User-Agent, own settings file / profile folder, own incomplete-file extension (`docs/IDENTITY.md`). **Exception:** the peer-ID code is `qB` (owner decision; see below) |
| Upstream baseline recorded | `docs/UPSTREAM.md`: commit `4df3aff9...`, **verified against GitHub in Phase 9** |
| Font licence | Nunito Sans, SIL OFL 1.1, text in `src/resources/ro/fonts/OFL.txt`; the font files embed the same licence |

## Open items
- **Peer-ID `qB`.** It makes peers and trackers see the client as qBittorrent while the User-Agent says RoTorrent. This is not a
  copyright problem (peer-ID codes are not licensed), but it contradicts the "do not impersonate" intent of handoff section 4.3 and
  some private trackers treat it as spoofing. The owner accepted the risk; revisit before a public release.
- **Environment variable names** such as `QBT_CONFIRM_LEGAL_NOTICE` still carry upstream's prefix (kept for upstream merges).
- **Qt LGPL notice and source offer** and the vcpkg licence texts must be added at packaging time (`docs/RELEASE.md` step 6).
- **Logos and icons** come from the design kit; the owner must confirm the right to license them.
- If RoTorrent is ever distributed on a platform with extra terms (some app stores), re-check GPL compatibility first.

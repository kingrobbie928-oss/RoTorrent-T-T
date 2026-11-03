# Upstream: qBittorrent

RoTorrent's backend (`src/base`) and the non-GUI parts of `src/app` are ported from qBittorrent.
This file records the exact baseline, as required by the handoff specification (section 4.1),
so the port can be audited and later upstream fixes can be merged.

| Item | Value |
| --- | --- |
| Project | qBittorrent (https://www.qbittorrent.org, https://github.com/qbittorrent/qBittorrent) |
| Baseline | Snapshot of `master`, file `qBittorrent-master.zip` |
| Commit | `4df3aff937992c371fe02fd66133eb0c9d825830` |
| Snapshot date | 2026-10-05 |
| Source of the hash | The zip archive comment of `qBittorrent-master.zip` (GitHub archive), not a git checkout |
| Upstream licence | Source: GPL v2 or later (with OpenSSL linking exception). Binary: GPL v3 or later (GPL v3 assets, which RoTorrent does not use) |
| RoTorrent licence | GPL v2 or later |

**Verified (Phase 9, 2 Nov 2026):** the commit exists on github.com/qbittorrent/qBittorrent (author Chocobo1, 5 Oct 2026,
"Search: Relax plugin versioning rule"), and seven files of the supplied zip (`session.h`, `sessionimpl.cpp`, `preferences.cpp`,
`torrentimpl.cpp`, `path.cpp`, `CMakeLists.txt`, `COPYING`) are byte-identical to the files at that commit. This was a sample, not
a full tree comparison.

## What was copied
- `src/base` in full, preserving subfolders and file names.
- `src/app`: `main`, `application`, `applicationinstancemanager`, `localpeer`, `cmdoptions`,
  `filelogger`, `legalnotice`, trimmed.
- `COPYING`, `COPYING.GPLv2`, `COPYING.GPLv3`, `AUTHORS`.
- Layouts of the dialogs in the design kit were taken from upstream `.ui` files (copyright headers kept).

## What was removed from the port
- `src/base/rss`, `src/base/search`, `src/base/plugins`, Lua (`3rdparty/lua`, `luabridge`).
- `net/dnsupdater`, `foreignapps` (Python detection), `utils/apikey`, `utils/password`, UI lock.
- Web UI, RSS, search, dynamic DNS and Python settings in `preferences.*`.
- `src/app/upgrade.*` and `signalhandler.*`; Qt::Xml, DBus and macOS / Linux branches.
- All of `src/gui`: no qBittorrent GUI code is compiled into RoTorrent.

## What was added or changed in the port
- `TorrentFilter::Queued` (upstream has no such status).
- `Session::reconfigureListening()` (public wrapper for the private `configureListeningInterface()`), used by the offline
  banner's Retry button (Phase 8). Three lines in `session.h`, `sessionimpl.h`, `sessionimpl.cpp`, marked `RoTorrent`.
- Identity strings: User-Agent, peer-ID code, `.!rt` incomplete-file extension, `RoTorrent.ini`
  (see `docs/IDENTITY.md`).
- Deliberately unchanged: C++ namespaces and class names, copyright headers, resume-data keys `qBt-*`.

## Phase 7: GUI files ported from `src/gui` (Options)
Unlike `src/base` these are copied for their logic and re-skinned; every copied file keeps the upstream header and
gains a RoTorrent copyright line. They live in `src/gui/options/`.

| Upstream file | RoTorrent file | What changed |
| --- | --- | --- |
| `optionsdialog.ui` | `optionsdialog.ui` | **Generated** by `tools/trim_options_ui.py` (never hand-edit): Search, RSS and Web UI pages out, controls of removed features out, wording and links rebranded |
| `optionsdialog.{h,cpp}` | `optionsdialog.{h,cpp}` | Body `QDialog` named `roUi`, hosted in `RoDialog`; Web UI / RSS / Search / macOS / Unix code resolved with `unifdef`; `IGUIApplication` replaced by `IApplication`; colour scheme bound to `RoTheme`; English-only language combo |
| `advancedsettings.{h,cpp}` | `AdvancedPage.{h,cpp}` | Same rows, bindings and order; presentation rebuilt as two cards; six rows dropped (see STATUS.md) |
| `fspathedit*.{h,cpp}` | same names | Themed folder icon |
| `watchedfolderoptionsdialog`, `watchedfoldersmodel`, `banlistoptionsdialog` | same names | Hosted in the shell through `SubDialogShell.h`; saved-size logic removed |
| `addtorrentparamswidget`, `torrentsharelimitswidget`, `flowlayout` | same names | Tags editor removed; "Pause torrent" wording |

Not ported on purpose: `uithemedialog` (custom UI themes, decision 13), `ipsubnetwhitelistoptionsdialog` (it edits the
Web UI's authentication whitelist, `WebUI/AuthSubnetWhitelist`).
To redo the trimming against a newer upstream: `python3 tools/trim_options_ui.py <upstream optionsdialog.ui> src/gui/options/optionsdialog.ui`
(and `--other` for the other `.ui` files), then re-resolve the `.cpp` with
`unifdef -UQ_OS_MACOS -UQ_OS_UNIX -DDISABLE_WEBUI -UQBT_HAS_COLORSCHEME_OPTION -UQBT_USES_DBUS -DQBT_USES_LIBTORRENT2 -UDISABLE_GUI`
and diff against `optionsdialog.cpp` here.

## Merging later upstream fixes
1. Diff `src/base` here against the same paths at the baseline commit above to isolate RoTorrent's changes.
2. Apply the upstream range `4df3aff..<new commit>` for the same paths.
3. Update the commit and date in the table and add a line to the history below.

## History
| Date | Upstream commit | Note |
| --- | --- | --- |
| 2026-10-05 | `4df3aff937992c371fe02fd66133eb0c9d825830` | Initial port baseline |

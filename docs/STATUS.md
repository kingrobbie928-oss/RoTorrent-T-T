# Build status

## Done
- Repo skeleton, CMake (root, cmake/Modules, src, test), GPL files, AUTHORS.
- src/base ported from qBittorrent master. Removed: rss, search, plugins, Lua, dnsupdater,
  foreignapps (Python detection), apikey, password, UI lock, Qt::Xml, DBus, macOS.
- Preferences stripped of Web UI, RSS, Search, Dynamic DNS, Python path and UI-lock accessors.
- Identity (docs/IDENTITY.md): User-Agent `RoTorrent/1.0.0`, peer-ID code `qB` (owner decision),
  `.!rt`, `RoTorrent.ini`, version 1.0.0.
- TorrentFilter::Queued added. gui/models/rostatus.h: statusOf() + unit test (all 19 states).
- src/app ported and trimmed: Application, single-instance guard (applicationinstancemanager,
  localpeer), cmdoptions (Web UI and nox options removed), file logger, legal notice, main.
  upgrade.* and signalhandler.* dropped; first-run defaults live in app/firstrun.cpp.
  Builds headless (DISABLE_GUI) as `rotorrent`; `rotorrent --version` prints `RoTorrent v1.0.0`.
- test/smoke_session + test/run_smoke.sh, test/check_base_clean.sh.

## Phase 2 (theme engine and window chrome): built
- src/resources/ro: theme (dark/light QSS, tokens.json), Nunito Sans, 82 icons (m-rss, m-plug, m-lock dropped),
  logos, recoloured QSS icons, all compiled in as Qt resources (prefix `:/ro`).
- tools/build_theme.py relocated; running it reproduces every generated file byte for byte.
- gui/theme/RoTheme: PassThrough rounding, Fusion, 13 px application font, QSS loader (@ICONS), live
  dark/light/Vibrant switching with persistence (Gui/Theme, Gui/Vibrant), device-pixel icon cache.
- gui/chrome: RoFramelessWindow (45 px title row, logo with baked 85% opacity, menu bar chips, 46 x 44
  caption buttons, Hairline, system move/resize with 6 px grab zone, double-click maximize, min size
  300 x 160), RoHoverIcon, winsnap (Windows Snap port), PerMonitorV2 manifest + .rc in dist/windows.
- gui/MainWindow: the five menus (placeholders except File > Exit) and a temporary strip with the
  theme and Vibrant toggles. `rotorrent_window` is the Phase 2 dev executable (no engine).

## Phase 3 (transfer list and sidebar on live data): built
- models: TorrentRow (plain data), TransferListModel (update by id), TorrentFilterProxy + RowFilter (ONE predicate used by
  the table and by the sidebar counts), SessionAdapter (BitTorrent::Session -> rows; statusOf() decides the status),
  DemoSource (the kit's torrents.json and a generator covering every status).
- widgets: TorrentDelegate (port of the demo's row painter: wash, progress bar, badge, name tile, Vibrant washes,
  Checking stripes, selection outline, error tint, "#" column), TransferListView (responsive columns from the 1070 px
  reference, rows grow instead of clipping, hover repaint, animation clock that runs only while a visible row needs
  it), EmptyState (no torrents / no results / empty filter), Sidebar (STATUS 9 rows, CATEGORIES with Uncategorized,
  TRACKERS with Trackerless, collapsible, no Tags, speed card + sparkline, 160-420 px drag edge), RoSearchBox (debounced).
- `rotorrent_window --engine` runs the window on a real session; `--demo` / `--generate` use demo rows.
- docs/phase3/*.png are renders of the demo data (dark, light, both Vibrant modes).

## Phase 4 (toolbar, menus, actions, context menus): built

- **Toolbar** (`widgets/MainToolbar`): 8.5 structure, queue group that hides with queueing off, accent link / Add buttons
  with self-painted glyphs, theme + Vibrant toggles moved here from the temporary control.
- **Menus** (`MainWindow.cpp`): File / Edit / View / Tools / Help per 8.4, shortcuts per 8.15, 34 px checkbox / radio column
  painted inside the icon (`widgets/MenuIcons`), Pause Session <-> Resume Session toggle, On Downloads Done (stored only),
  View toggles persisted under `Gui/`.
- **Rules** (`actions/ActionRules`): qBittorrent's `displayListMenu` enable / visibility logic as a pure function over a list of
  facts (multi-select ready; the view is still single-select). **Engine commands** (`actions/TorrentController`): resume, pause,
  force resume, remove, queue moves, location, rename, category (add / edit / remove / remove unused), ATM, sequential,
  first-last, super seeding, recheck, reannounce, copy, export, open destination, Pause / Resume Session.
- **Context menus**: torrent menu (8.8) and sidebar menus (8.6, no Tags). Empty categories now appear in the sidebar
  (`Sidebar::setKnownCategories`).
- **Dialogs**: `RoDialog` shell (title row, hairline, close button), `RoMessageBox` (the one styled box), Remove dialog,
  Rename, category dialog. Quit confirmation (No / Yes / Always Yes). Primary button first, as in the kit.
- **Tests**: `test_actionrules` (20 checks), `test_commands` (about 80 checks on a real session and the real window, including the
  label audit and queue visibility), `check_labels.sh`. `docs/QA.md` is the manual script.

Deviations and assumptions to veto:
- Queueing toggle: the engine has no signal for it, so `TorrentController` notices it on the session stats tick (about 1.5 s) and
  exposes `refreshQueueing()` for the Options dialog (phase 7) to call at once. Backend untouched.
- Dialogs and tabs from later phases are **placeholders** behind `MainWindow::hooks()` (Add torrent, Creator, Statistics,
  Options, Cookies, About, Speed limits, Manage content, Edit trackers, Torrent options, Preview). Each shows a styled box.
- Status Bar (View menu) only persists and emits `viewToggled`; the bar itself is phase 8.
- Sidebar "Copy tracker URL" copies the tracker **host** shown in the row, not the full announce URL.
- Remove dialog remembers the "delete files" checkbox automatically (`Gui/RemoveContentDefault`).
- "Reannounce" is also disabled for errored / missing-files torrents (matches the tooltip text; upstream only checks stopped / checking / queued).

Not verified: nothing here has run on real Windows or been compiled with MSVC; menus were compared with the kit only offscreen
(popup corners are square, as in the demo). Native file / folder pickers were not exercised headless.

## Phase 5 (details panel and tabs): built

`src/gui/details/`. The panel sits under the transfer list in `MainWindow` (`setDetailsSource`); it starts collapsed
(38 px) with no active tab, opens into the clicked tab at 216 px, and the chevron opens General / collapses.
Drag handle (10 px strip): 90 px minimum, 75% of the parent's height maximum, snaps collapsed at the minimum,
height persisted (`Gui/DetailsHeight`). One refresh timer (the list's refresh interval) runs only while expanded and
refreshes only the visible tab.

- **Data layer.** Tabs read a `DetailsSource`. `SessionDetailsSource` is the live-engine implementation (general info,
  trackers incl. DHT/PeX/LSD rows, web seeds, files, async peers and piece / file availability, and the actions: add /
  edit / remove / reannounce trackers, add / ban peers, add / remove web seeds, file priority, rename file / folder through
  the backend's own `renameFile` / `renameFolder`). `DemoDetailsSource` invents stable data for the preview and tests.
- **General**: PROGRESS (real have-pieces) and AVAILABILITY (real copies per piece) bars, 16 TRANSFER + 7 INFORMATION
  fields, info hashes v1 / v2, save path, comment (shown as plain text).
- **Trackers** (11 columns, spec widths and colours), **Peers** (12 columns, progress bar cell, empty state),
  **HTTP Sources** (empty state; right-click works on it), **Content** (tree with checkboxes, tri-state folders, Mixed
  priority, filter, Select All / None, Open, Open containing folder, Rename, priority submenu), **Log** (four chips,
  coloured lines, bounded 20,000-line buffer fed by the ported `Logger`, filter persisted in `Gui/LogMask`).
- **Dialogs**: Add trackers, Add peers, Add HTTP source (on the `RoDialog` shell); Edit tracker URL and Rename use
  `RoInputDialog`; ban asks through `RoMessageBox`.
- **Wiring**: "Manage content..." opens the Content tab, "Edit trackers..." opens the Trackers tab (the tracker-list dialog
  is phase 6). `rotorrent_window --details <tab> [--select N]` opens a tab for screenshots; `--engine` uses the live source.
- **Theme**: `tools/build_theme.py` gained one rule (`QHeaderView` transparent inside `#dTable`), without it table headers
  in the panel painted the default light fill. Regenerated `dark.qss` / `light.qss`.

Verified: `test_details` (105 checks, demo data), `test_details_engine` (live libtorrent session: 40 checks) and the
earlier suites (`ctest`, `test_commands`, `test_adapter`) all pass on Linux. Screenshots of every tab rendered in Dark and
Light and inspected.

Deviations / assumptions to report: Content uses `QTreeWidget` with in-place updates, not a model like upstream's
`TorrentContentModel`; Trackers shows one row per tracker (no per-endpoint child rows); the 75% cap uses the height of the
table + panel column, not the whole window; ETA of a finished torrent shows the engine's infinity sign.

### Not done / not verified (Phase 5)
- Never compiled with MSVC or run on Windows. Linux, offscreen platform only.
- Real mouse interaction untested: dragging the handle, right-click menus popping up, clicking chips (the code paths are
  called directly in tests). The three add dialogs and Rename were not driven through a visible dialog.
- "Open containing folder" opens the folder; it does not select the file in Explorer.
- Peer country comes from the engine's GeoIP only if enabled; no flag icons (not in the design).
- Performance with thousands of files or hundreds of peers not measured.
- General tab downloaded-pieces bar does not show pieces currently downloading.

## Verified (Linux, GCC 13, Qt 6.7 headers via conda, libtorrent 2.0.10 from apt)
- rotorrent_base, rotorrent_app_core, rotorrent (headless exe), test_rostatus, smoke_session build.
- test_rostatus: all 19 states OK.
- run_smoke.sh: SMOKE OK. For a clean shutdown and for a forced kill (_Exit), then a restart:
  torrents, category, tag, save path, queue position and the queueing setting are restored; stop/start,
  move-to-top and finished-data verification work.
- Phase 2, offscreen on Linux with Qt 6.7: `rotorrent_window --shot` title row is pixel-identical (0 of
  57,600 pixels differ) to the kit's screenshots/dark.png and light.png at 100%.
- test_chrome via run_chrome.sh passes 28 checks at scale factors 1, 1.25, 1.5, 1.75, 2.25, 2.5 and 3:
  row/button geometry, buttons end exactly at the hairline, 44 px is whole device pixels, live theme and
  Vibrant switch keeps the window and its size, persistence, icon cache.
- winsnap.cpp compiles warning-free for Windows (MinGW, -Wall -Wextra) and for the non-Windows stub.
- Phase 3 (offscreen, Qt 6.7): test_transferlist, 40+ checks, also run at 125% and 175%:
  filter and sort rules, every sidebar count equals the rows its filter shows (20 rows checked on 240 generated torrents),
  the eight status rows add up to the total, empty states, column widths equal the reference at 1070 px, narrow-window
  wrapping and horizontal scroll, animation clock starts and stops with visible rows.
  Performance with 500 rows: loads and renders in about 0.4 s, 2.8 ms average and under 6 ms worst per full-viewport
  frame while scrolling, idle CPU 0.1% with nothing animating, about 4% while animating (software rendering).
- test_adapter: a real session (two generated torrents) fills the model; status/name/size equal the engine's,
  category changes, stop/start, speed signal, filtering and removal all reach the model.
- Reference comparison of the table region (demo data, `--queueing`, phase 0.35): dark Vibrant 0 differing pixels,
  dark and light 140 of 490,504 (all in the scrollbar strip) when the table height matches the reference's, light
  Vibrant differs by at most 1/255 in one channel. Speed card and sparkline match; two sidebar icons (All, Error)
  differ by sub-pixel placement (about 126 px).
- Seam scan at 100%, 125% and 175% in dark/light, with and without Vibrant: no vertical seam lines
  (the detector was verified against an injected faint line).
- check_base_clean.sh: src/base has no Widgets/RSS/search/plugins/Web UI references.

## Not done / not verified (Phase 3)
- The window on a real session is shown only through `rotorrent_window --engine`; the real `rotorrent` executable is
  still headless and does not open this window yet.
- Not exercised with a real pointer: sidebar edge dragging, row hover and selection by mouse, header-click sorting,
  scrolling feel, empty-state buttons. Tests call the same code paths but no mouse is involved.
- Performance numbers are offscreen software rendering on Linux with a synthetic list; GPU, Windows and a live session
  with 200+ real torrents have not been measured. CPU is process CPU time over 2 s, a coarse measure.
- Sidebar right-click menus, row context menu and toolbar are Phase 4. Clicking the speed card only emits a signal
  (the Speed limits dialog is Phase 7). Single-row selection only (the multi-select decision is still open).
- The reference screenshots show a toolbar, expanded details panel and status bar that do not exist yet, so only the
  table and sidebar regions were compared, and the non-Vibrant references needed a table height of 558 px to line up.
- Sidebar counts differ from the demo images because the demo hard-codes its numbers; ours are computed from the rows.
- A finished torrent with no peers shows as Stalled (the engine's StalledUploading), as the 6.5 table specifies.
- Tracker host is the first http/https/udp tracker URL's host; torrents with only DHT/PeX/LSD count as Trackerless.
- Window preview runs on the Linux toolchain only; MSVC has not built any GUI code.

## Not done / not verified (Phase 2)
- **Windows Snap has never run.** Only compiled. Snap (top edge, half screen, drag-down restore,
  Win+Arrow), WM_NCCALCSIZE maximize clamping, one/two monitors and Windows 10/11 are untested.
- Caption-button hover (red Close), edge-resize cursors and window dragging need a real pointer;
  offscreen cannot exercise them. The "no red over the divider" rule is verified geometrically
  (buttons end at y=44, hairline at y=44), not with per-scale hover screenshots.
- Comparison against the reference was done at 100% only, and only for the title row (the rest of the
  window does not exist yet). 125% and the other scales were checked numerically, not by image.
- Menus are placeholders; popup menus were not screenshotted (grab() does not capture popups).
- The manifest is not embedded or tested (no Windows build). Nunito Sans license is unverified (docs/THIRD_PARTY.md).
- `rotorrent_window` is not the real `rotorrent` executable; the GUI is not wired to Application yet.

## Not done / not verified (earlier phases)
- Windows/MSVC build has never been run. Windows-only code (utils/reg.cpp, utils/os.cpp power
  paths, Application memory-priority code) is unchecked.
- Only the default resume-data storage (fastresume files) was exercised. SQLite storage was not.
- Not exercised yet from the 6.4 checklist: magnet/URL adds, watched folders, share limits,
  proxy/IP filter, scheduler, torrent creator via the app, embedded tracker, external-program
  and e-mail actions, shutdown actions, move-on-category-change.
- The wire identity (User-Agent and peer ID actually sent) was not captured with a packet or
  tracker test; it is set in code only.
- Application.cpp still contains upstream's `#ifndef DISABLE_GUI` branches (MainWindow,
  DesktopIntegration, etc.). They are inert while DISABLE_GUI is defined and must be replaced
  with RoTorrent's own GUI hook in Phase 2.
- StackTrace support (stacktrace.cpp) is copied but not wired into the build.
- Organization domain is a placeholder (`rotorrent.invalid`), as are URLs and contacts.
- No GUI work (phases 2-9), no packaging, no installer. Phase 0's "empty window printing
  library versions" is not built.
- The Options text "Append .!qB extension" lives in GUI code, which does not exist yet.

## Phase 6 (in progress): add-torrent flows

Built and tested (test/test_addtorrent, part of run_all.sh; real engine, offscreen Qt 6.7.3, Linux):
- `AddLinksDialog`: "Add torrent links" (bold heading, multi-line box, italic hint, OK primary / Cancel).
  Trims, drops empty lines and duplicates; pre-fills a link found on the clipboard.
- `AddNewTorrentDialog`: save path + Browse, category (existing categories fill their save path),
  content layout, Resume after adding, add to top of queue, sequential, first/last pieces, file tree with
  tri-state checkboxes, Select All / None. Unticked files become `DownloadPriority::Ignored`.
- `AddTorrentFlow`: File > Add Torrent File (picker first, one dialog per file), Add Torrent Link,
  toolbar and empty-state buttons. Bad file / bad link -> styled error box. Already-known torrent -> info box.
  Hooked into `rotorrent_window --engine`.

Deviations and gaps (veto or schedule):
- The checkbox reads "Resume torrent after adding", not upstream's "Start torrent": Appendix C forbids "Start".
- Closed in slice 3 (below): magnet metadata, http(s) links, the missing Add dialog fields.
- Closed in slice 7: drag and drop onto the main window, command-line adds, second-instance forwarding.
- Not started: Torrent Creator, Cookies, Torrent options, shutdown countdown, legal notice, crash info,
  tracker / peer dialogs, preview picker.
- check_labels.sh now skips DemoSource / DemoDetailsSource (fake peer client names in demo data).
- Build verified with GCC 13 / Linux only; nothing built with MSVC or run on Windows.

### Phase 6 slice 3: finish the Add flow (test/test_addtorrent, in run_all.sh)
- **Magnet links**: the dialog asks the engine for the metadata (`Session::downloadMetadata`) and shows
  "Retrieving metadata..." with a busy bar. When `metadataDownloaded` arrives for the same torrent (v1 or v2 hash
  of a hybrid) the file tree fills, Select All / None, Save as .torrent and Skip hash check become available, and the
  returned descriptor carries the metadata so the file selection applies. OK stays available while waiting (the
  torrent is then added without a selection). The hidden metadata torrent is always cancelled when the dialog closes.
- **New fields**: Torrent Management Mode (Manual / Automatic; Automatic locks the path fields and sends no save
  path), Use another path for incomplete torrent + path, Pause condition (None / Metadata received / Files checked),
  Skip hash check (seed mode), Do not delete .torrent file (files only), Save as .torrent file..., Never show again.
- **Never show again** is stored in the preferences (`AddNewTorrentDialog/Enabled`, see Phase 7); while off the flow adds with the session
  defaults and no dialog. Closed in Phase 7: the setting is now `Preferences::isAddNewTorrentDialogEnabled`, shared with Options > Downloads > "Display torrent content and some options".
- **.torrent file guard**: the flow wraps a file source in `TorrentFileGuard`, so the Options' delete mode applies;
  ticking Do not delete keeps the file. The box starts ticked only when the mode is Never.
- **http(s) / ftp links**: `AddTorrentFlow` now downloads with `Net::DownloadManager` (cookies, proxy, size limit from
  the preferences), handles redirects to a magnet, then shows the dialog like for a file. A `UrlHandler` from the full
  application still overrides it. Failed downloads show the styled error box.
- Tested: result fields, Automatic mode, metadata arriving while the dialog is open, guard delete / keep,
  Never show again on / off, and a local web server for a good link and a 404.
- Deviation: the label is "Pause condition", not upstream's "Stop condition" (Appendix C forbids "Stop").
- Not verified: a real magnet with real peers (the metadata arrival is simulated by emitting the signal);
  https; the dialog height on small screens (it is taller now); nothing built with MSVC or run on Windows.

### Phase 6 slice 4: Torrent Creator (test/test_torrentcreator, in run_all.sh)
- `TorrentCreatorDialog` (`dialogs/TorrentCreatorDialog.*`, on the `RoDialog` shell, 594 px): three cards in a scroll area (Select
  file/folder to share, Settings, Fields) and a fixed footer (Progress, Create Torrent, Cancel). Opens from File > Create Torrent,
  Tools > Torrent Creator and the toolbar (`TorrentCreatorDialog::install`, also wired in `rotorrent_window --demo`).
- Drop zone (`RoDropZone`): dashed target, accent outline and red wash while dragging over (QSS `dropZone[drag=true]`), drop puts the
  first local path into the Path box; the rest of the dialog accepts drops too. Select file / Select folder pickers (symlinks are not
  resolved, except on Windows, as upstream).
- Settings: Torrent format (V2 / Hybrid / V1), Piece size (Auto, 16 KiB ... 128 MiB), Calculate number of pieces (background thread),
  Ignore dotfiles, Private, Seed immediately after creating, Ignore share ratio limits (follows the seeding box). Fields: Tracker URLs
  (a blank line starts the next tier), Web seed URLs, Comments, Source. All values persist under `TorrentCreator/` in the settings.
- Real work: the ported `BitTorrent::TorrentCreator` runs on the dialog's own single-thread pool. Progress comes from its
  `progressUpdated`; the form locks while it runs. **Cancel** stops the creator at its next checkpoint, shows nothing, resets the bar,
  re-enables the form and keeps the dialog; Cancel / X / Esc when idle close it. Destroying the dialog mid-creation interrupts the worker.
- On success: "Torrent created" box; with seeding on the file is added to the session in seed mode (`savePath` = the folder that
  holds the payload, no auto TMM), with the share limits removed when "Ignore share ratio limits" is ticked. Failures
  (unreadable path, unwritable target, everything filtered out) show the styled error box and leave no file behind.
- Tested (real engine, offscreen Qt 6.7.3): the torrent is read back with **plain libtorrent** and every v1 piece hash is recomputed
  from the files on disk (hybrid and v1 cases); hybrid has both info hashes, v2-only has no v1 hash, v1-only no v2 hash; piece length,
  private flag, name, comment, two tracker tiers, web seed, dotfile handling; the Calculate count equals the real piece count; the
  session takes the torrent, a forced recheck reaches 100% (hybrid and v2-only); share limits; saved settings come back; unreadable
  path, bad target and empty selection fail cleanly; Cancel on a 6 GiB sparse file (progress shown, cancelled, no file); drop zone
  highlight and drops; File / Tools / toolbar entries open the dialog. Screenshots in `docs/phase6/` (dark, light, scrolled, drag).

Deviations and assumptions to veto:
- **Not through `TorrentCreationManager`.** The work division says "through the torrent creation manager". The manager has no
  progress signals, no cancel call and its seeding step cannot remove share limits, so the dialog drives `TorrentCreator` directly, as
  upstream's own dialog does. The manager stays unused (it is the Web API's helper).
- Label: "Seed immediately after creating", not the kit's "Start seeding immediately" (Appendix C forbids "Start").
- Default format is **Hybrid** (upstream's default, opens in every client); the kit screenshot shows V2.
- The piece size list is the full upstream list (16 KiB to 128 MiB, powers of two), not the kit's seven sample entries.
- The kit's "Optimize alignment" group is not shown: it is a libtorrent 1 feature and does nothing on libtorrent 2.
- Create Torrent is disabled until a path is chosen (upstream lets you press it and shows an error).
- Not verified: Windows / MSVC; real pointer drags from Explorer; the native save-file and folder pickers (tests call
  `startCreation` directly); a very large real payload; opening the output in another client by hand (the plain-libtorrent
  piece-hash check stands in for it, run the QA row on a second client).

### Phase 6 slice 2: Statistics, Speed limits, About (test/test_infodialogs, in run_all.sh)
- `StatisticsDialog` (420 px): 13 rows in four sections, values from `Session::status()` / `cacheStatus()`,
  refreshed every 1.5 s while open, no "Request latency" row. Write / read cache overload use upstream's
  ratio (queue / (queue + used buffers)).
- `SpeedLimitsDialog`: global and alternative upload / download limits, KiB/s with an infinity symbol for 0.
  OK writes to the session (bytes/s), Cancel changes nothing. Hooked to the sidebar speed card hook.
- `AboutDialog` (640 px, own file so the label audit allows the qBittorrent credit): six tabs, placeholder
  URLs on rotorrent.example, real Qt / libtorrent / Boost / OpenSSL / zlib versions, DB-IP attribution.
- Closed in slice 7: Speed limits now has a slider beside each spin box, and the About tab has the 70 px RO logo.
- Gap: no "alternative limits enabled" toggle (that lives in the status-bar chip, not built yet).
- All three are wired into `rotorrent_window --engine` only; the real `rotorrent` executable is still headless.

### Phase 6 slice 5: Torrent options, Cookies, Tracker list, Preview picker (test/test_managementdialogs, in run_all.sh)
- `TorrentOptionsDialog` (600 px, scrolling): Automatic Torrent Management, Save at (+ incomplete-torrent path), Category (editable),
  upload / download sliders + KiB/s spin boxes, share limits (Ratio, Seeding time, Inactive seeding time, Mode, Action), Disable
  DHT / PeX / LSD, sequential, first and last pieces. Works on one or many torrents (upstream rules): a field that differs shows a
  partial check, the inequality sign or a blank combo entry and is written only when the user changes it. DHT / PeX / LSD are fixed
  and explained for private torrents. "Default" reads "From category" once a category is chosen.
- `CookiesDialog`: table (Domain, Path, Name, Value, Expiration date; "Session" when none), add and trash buttons, in-place editing.
  OK stores the whole list in `Net::DownloadManager`, Cancel stores nothing. New cookies expire in two years (upstream).
- `TrackerListDialog`: kit text, one URL per line, blank line = next tier. Shows the trackers common to the selection; OK replaces the
  trackers of every selected torrent with the edited list (upstream behaviour: trackers that were not common are dropped).
- `PreviewSelectDialog`: previewable files only (name, size, progress), sorted by name, double click or Preview opens the file, one
  previewable file skips the dialog, none shows a message, a file not on disk shows "Preview impossible". Several selected torrents
  are handled one after another.
- Hooks: Tools > Manage Cookies, context menu Torrent options / Edit trackers / Preview file. The Edit trackers hook no longer jumps
  to the Trackers tab when the engine is on (the demo window keeps the redirect).
- Theme fix: `QTreeView::item` had padding only in the selected state, so unselected rows lost their 6 px indent in Qt 6.7 (visible
  in the Cookies and Preview lists). The plain item rule now carries `background: transparent`; `tools/build_theme.py` regenerates both QSS files.
- Tests (real engine, offscreen Qt 6.7.3): about 75 checks, covering every field write-back, multi-torrent partial states, Cancel,
  private torrents, cookie add / edit / delete / OK / Cancel through the hook, tier text round trip and common subset, tracker
  replace on two torrents, preview filtering / sorting / open / single file / no previewable file / missing file, and a label audit.
  Screenshots in `docs/phase6/` (options single and multi, cookies, tracker list, preview picker; dark and light where listed).

Deviations and assumptions to veto:
- Dialogs are modal (RoDialog is), the kit's Torrent options is non-modal with raise-existing.
- Path fields use a line edit + Browse... button (as the Add dialog), the kit draws a folder icon button.
- Share limit action "Pause torrent" replaces upstream's "Stop torrent" (Appendix C).
- Preview label keeps upstream's torrent name: `... from torrent "<name>" support previewing ...`, the kit text has no name.
- Cookie values are shown in full (middle-elided, full text in the tooltip). The kit's bullet masking of a passkey is not built.
- Torrent options: the incomplete path field is filled with the session default when ticked and empty (upstream).
- Not verified: Windows / MSVC, native folder pickers, real double-clicks, opening a preview in a real media player (tests replace
  the opener), a Torrent options dialog opened on hundreds of torrents.
- Wired into the real `rotorrent` executable in slice 7 (see below).

### Phase 6 slice 6: App-level dialogs (test/test_appdialogs, in run_all.sh)
- `ShutdownConfirmDialog` (`dialogs/ShutdownConfirmDialog.*`, 410 px, on `RoDialog`): the five confirmations (Exit, Shutdown, Suspend,
  Hibernate, Reboot) with the kit's wording, a 15 s countdown that confirms by itself at zero, accent confirm button, Cancel as the
  default button and focus (a stray Enter never shuts the computer down), always on top, Esc / X / Cancel abandon it.
  `askForConfirmation(parent, action)` keeps upstream's name and returns true or false. "Don't show again" is stored in
  `ShutdownConfirmDlg/DontConfirmAutoExit` and `askForConfirmation(Exit)` then returns true without a dialog. The dialog only asks:
  doing the action is Phase 8. `app/application.cpp` (dead until slice 7, it is built with DISABLE_GUI) now includes and calls this dialog.
- `LegalNoticeDialog` (`dialogs/LegalNoticeDialog.*`, 460 px): heading "Before you start", the notice, the muted "No further notices
  will be issued.", one accent OK. The wording is still produced by `app/legalnotice.cpp` (unchanged text logic); its GUI branch now calls
  `LegalNoticeDialog::showNotice` instead of `QMessageBox`. The headless console branch is untouched.
- `StacktraceDialog` (`dialogs/StacktraceDialog.*`, 642 px): read-only card with the red headline, bug-report line with a link,
  version block (RoTorrent, libtorrent, Qt, Boost, OpenSSL, zlib, OS), "Caught signal" and the fenced trace; outlined accent Close button.
  The link uses the accent colour (the default dark blue is unreadable on the dark card).
- **Stack trace support in the build**: new option `ROTORRENT_STACKTRACE` (ON). It compiles `app/stacktrace.cpp` (boost::stacktrace,
  was unreferenced until now) and the new `app/crashhandler.*` into `rotorrent_app_core`, defines `STACKTRACE`, and sets the platform
  flags as upstream (`-rdynamic` + dl on Linux, `/Zi` + `/DEBUG` + `/PDBALTPATH` on MSVC, `--export-dynamic` on MinGW).
  `registerCrashHandlers()` (SIGSEGV, SIGABRT, SIGFPE, SIGILL) is called from `main()`: it writes the report to stderr, shows the Crash
  info dialog in the GUI build (skipped when `ROTORRENT_NO_CRASH_DIALOG` is set), then re-raises the signal with the default action.
- `RoDialog::run()` now centres on the screen when it has no visible owner (first-run notice, a window hidden in the tray, a crash).
- `rotorrent_window --dialog exit|shutdown|suspend|hibernate|reboot|legal|crash [--theme light] [--shot png]` opens one dialog alone.
- Because `src/app` is compiled headless, the GUI branches of `legalnotice.cpp` and `crashhandler.cpp` are built and tested through a
  small library `rotorrent_app_gui_glue` (test/CMakeLists.txt). Slice 7 moves the real executable onto it.
- Tests (real Qt 6.7.3, offscreen; about 110 checks): all five texts, titles and buttons; default button and focus; always on top;
  countdown ticks and confirms at zero and stops on Cancel; Confirm, Esc, X and Enter; "Don't show again" stored only by Exit Now and only for
  Exit; `askForConfirmation` modal paths; the real `showLegalNotice(true)` shows the styled dialog with the existing wording; parentless
  centring; crash dialog content (escaping of `<`, `&`, version block, fence, no "qBittorrent"); `getStacktrace()` returns frames
  naming the executable; and a **child process that really raises SIGSEGV** twice: with the dialog (the dialog opens, carries the signal and
  trace, is closed by Close and the process then dies by SIGSEGV) and with the dialog disabled (report on stderr only).
  Screenshots in `docs/phase6/` (exit_confirm, shutdown_confirm, legal_notice, crash_info; dark and light).

Deviations and assumptions to veto:
- "Don't show again" is shown for **Exit only**. The kit's Shutdown screenshot also shows it, but the preferences can remember only the
  auto-exit choice (as upstream), so a box on the other actions would do nothing. Upstream's `accept()` also wrote `false` for the other
  actions; that is not copied.
- The crash text is richer than the kit's placeholder (version block, signal, fence) and follows upstream's content; the bug address is
  the placeholder `bugs.rotorrent.example` (handoff 11, item 6).
- No Copy button on the crash dialog (the kit has only Close); the text is selectable.
- Build check used Qt 6.7.3 (conda-forge) with the system libtorrent 2.0.10 / Boost 1.83 / GCC 13, Linux, offscreen.
- Not verified: Windows / MSVC; that boost::stacktrace resolves names through the PDB on a release build; a real crash on Windows
  (the handler uses `std::signal`, which the MSVC CRT supports for SIGSEGV / SIGABRT / SIGFPE / SIGILL, but some SEH exceptions may
  not reach it: a `SetUnhandledExceptionFilter` hook is an option for Phase 9);
  the countdown over its real 15 s (tests use a 30 ms tick); the dialog staying on top of a full-screen game.
- The dialogs are not reachable from the running application yet: the shutdown dialog is raised by `Application::allTorrentsFinished`
  and the legal notice by `main()` once slice 7 moves the real executable to the GUI build.

### Phase 6 slice 7: integration and close-out (test/test_dragdrop and test/run_instances.sh, both in run_all.sh)
Built and tested on Linux (GCC 13, Qt 6.7.3 from conda-forge, libtorrent 2.0.10, offscreen). `run_all.sh` passes end to end.
- **The real `rotorrent` executable is now the GUI build** (`rotorrent_gui_app` in src/app/CMakeLists.txt: the same application
  sources compiled without `DISABLE_GUI`). The headless build is `rotorrent_headless` when the GUI is built (smoke test and
  hosts with no display). `Application` is a QApplication; `main()` starts `RoTheme` right after the profile and settings exist,
  so the legal notice, the Windows help box and error boxes are styled. Splash screen and the upstream-GUI branches are gone.
- **`EngineWindow::create`** (src/gui/EngineWindow.*) is the one place that builds the window on a live session with every
  dialog hooked in; `rotorrent_window --engine` uses it too. The window is created when the session reports `restored`, shown, and
  deleted before the session is freed (it owns the adapter, controller and details source).
- **Adds from outside the menus** all use `AddTorrentFlow::addSources(..., Options)`: command line, second instance, drag and drop.
  `Options` carries the `AddTorrentParams` from the command line and `skipDialog`. `--skip-dialog=true` adds with those params and
  no dialog; `false` forces the dialog even when "Never show again" is on; unset follows that setting. When the dialog shows,
  `--save-path`, `--category`, `--add-paused`, `--sequential`, `--first-and-last` and the other options pre-fill it
  (`AddNewTorrentDialog::run(..., initial)`), and fields it has no widget for (tags, limits) pass through to the result.
- **Second instance**: with arguments they are forwarded to the first instance and the second exits 0; with none the first window
  is brought to the front (`MainWindow::activate`, which also restores a minimized window). Arguments that arrive before the
  session has restored are queued and processed once the window is up.
- **Drag and drop** (`MainWindow::sourcesFromMimeData`, `canAcceptDrag`, `handleDrop`): `.torrent` files, magnet links and
  http(s)/ftp addresses, in drop order and without duplicates; folders and other files are ignored. The hook runs on the next
  event-loop turn, so the dialog does not keep Explorer waiting. In demo mode (no engine) drops are refused.
- Small gaps closed: sliders in Speed limits (0 to 10,000 KiB/s, or more when the current limit is higher; the spin box takes
  anything), the 70 px RO logo centred on the About tab. Screenshots: `docs/phase6/about_dark.png`, `speed_limits_dark.png`.
- Command-line wording: `--add-stopped` is now `--add-paused` (Appendix C), the help text says "paused", the splash line is gone
  (`--no-splash` is still parsed and does nothing, so old scripts do not break).

Deviations and assumptions to veto:
- Watched folders still add through the base `AddTorrentManager` (no dialog, session defaults). Upstream shows the dialog there.
- No startup progress dialog: nothing is shown until the session has restored its torrents (about a second for a small profile;
  a very large one will look idle at launch). A loading state in the window is the fix.
- (Phase 8 changed this: closing the window hides it to the tray while the icon is visible; see the Phase 8 section.)
- Dropped from upstream for now: the recursive-download prompt for a downloaded .torrent, the instance-name title suffix, and the
  Windows listener for the system colour mode. All three belong with Options (Phase 7) or Phase 8.
- Not every dialog has a Light screenshot: About and Speed limits are Dark only (the test grabs the active theme).

Not verified: Windows / MSVC (the new exe, `WIN32_EXECUTABLE`, the .rc and manifest, the shutdown-block shell window); a real drag
from Explorer (the test calls the handlers directly because a synthetic QDropEvent never reaches the widget offscreen); the Add
dialog pre-fill by eye (covered by the code path only, not by a test); the 15 s shutdown countdown on a real session end;
`rotorrent` started by double-clicking a .torrent (file association is an installer job, Phase 9).


## Phase 7 (Options and Advanced): built, verified headless on Linux only

Built and tested (`test/test_options`, 87 checks, part of `run_all.sh`; the whole suite passes, including `chrome` at all seven scales):
- **Options dialog** (`src/gui/options/`): upstream's `optionsdialog.{ui,cpp}` ported and trimmed, hosted in the `RoDialog` shell (880 px), body named
  `QDialog#roUi`. Six pages in a 150 px painted list (Behavior, Downloads, Connection, Speed, BitTorrent, Advanced; `OptionsCategoryDelegate`: 38 px rows,
  18 px icons, 12 px gap, pills). OK is the primary button; **Apply** keeps the dialog open and disables itself; **Cancel** discards; the last page is remembered.
  Opens from Tools > Options, Alt+O and the toolbar Settings button (`OptionsLauncher`, installed by `EngineWindow::create(session, app)`).
- **Advanced page** (`AdvancedPage`): upstream's rows, defaults, ranges and order, as two cards (ROTORRENT, LIBTORRENT) with Documentation links in accent,
  54 px rows, 16 px padding, one control fixed at 272 px, 1 px inset dividers, "(?)" turned into a 14 px info icon with the tooltip "More information".
  72 rows in this Linux build; Windows adds the memory-limit, memory-priority and Mark-of-the-Web rows, and libtorrent-version gates add or drop a few more.
- **Sub-dialogs** in the shell (`SubDialogShell.h`): watched folder options (per-folder add-torrent parameters, share limits; scrolls when taller than the
  screen allows) and banned IP addresses.
- **Live effects**: Torrent Queueing shows / hides every queue control at once; Color scheme and the toolbar toggle are one preference and follow each other;
  the Refresh interval reaches the details panel (the session loop re-reads it by itself); the instance name becomes the window title suffix; the start-up
  window state is applied at launch; **double-click on a torrent** runs the Behavior action (pause / resume, open folder, preview, options, nothing).
- **Audits**: `check_labels.sh` now also scans every `.ui` string and every Options literal, tooltips included (it found a leftover "WebUI" list entry and a
  "qBittorrent" tooltip); `check_base_clean.sh` asserts that no Search, RSS or Web UI remains in Preferences or Options. Both were shown to fail on a planted violation.

Fixes found on the way (outside Options):
- **`RoTheme` now sets the application palette** from the tokens (handoff 8.2 says "swap the stylesheet and palette"). Qt auto-fills scroll-area viewports from the
  palette before QSS paints, so a `.ui` form on the old light palette showed pale panels and unreadable labels in the dark theme.
- A `.ui` root must be named `roUi` **before** `setupUi` (its buttons are polished there), and a dynamic `primary` property needs a re-polish
  (`markPrimary`). Both are in `SubDialogShell.h`.
- The theme's steppers need room: spin boxes get 128 px (`widenSpinBoxes`), and a `.ui` body is grown to its polished layout minimum.
- `AddNewTorrentDialog::isEnabled()` now uses the preference (one setting, not two). `RoMessageBox::warning()` added; no `QMessageBox` is left in Options.

### Controls and rows removed (feature absent in RoTorrent; veto any of these)
- **Pages**: Search, RSS, Web UI. **Interface**: Style, custom UI theme and its button, system icons, "changing interface settings requires restart".
- **Transfer list**: alternate row colours, state colours, progress-bar text colour, hide-zero values (colours and columns are fixed by the design).
  **Sidebar**: auto-hide zero filters, separate tracker status filter. **Content tab**: drag content out. **Status bar**: External IP.
  **Desktop**: splash screen, tray icon style, the macOS-only controls. **Proxy**: "Use proxy for RSS". **Add dialog**: "Bring to front".
- **Recursive download dialog**: upstream's preference has no consumer anywhere in the snapshot, so it would do nothing.
- **Check for program updates**: no feed is configured (decision 5); the Phase 8 plumbing is in (see below).
- **Advanced rows** (7): Confirm removal of all tags, Download tracker's favicon, Enable speed graphs, Enable icons in menus, Attach "Add new torrent" dialog,
  Python executable path, Save path history length. The first, third, fourth and fifth are the handoff's four; favicon, Python and history have nothing in RoTorrent
  that reads them.
- **Kept but English only**: the Language combo shows English, disabled. **Window state "Hidden"** opens minimized until the tray exists.

### Deviations from the handoff
- `ipsubnetwhitelistoptionsdialog` is **not** ported although 8.12 lists it: it edits `WebUI/AuthSubnetWhitelist`, and its only entry point was the Web UI page.
- `uithemedialog` is not ported (8.12 lists it and also says "not carried over"; decision 13 wins).
- Wording: "Run RoTorrent when Windows starts", "Add torrents in paused state", "Pause condition", "Pause torrent", "Begin the BitTorrent session in paused state"
  (Appendix E says "Start ..."), "Final tracker announce timeout" (upstream: "Stop tracker timeout"), "Time the schedule begins / ends".
- Documentation links point at `docs.rotorrent.example` (placeholder, decision 6).

### Not done / not verified (Phase 7)
- **Nothing was built with MSVC or run on Windows.** The `#ifdef Q_OS_WIN` code (start with Windows, Default apps button, memory limit and priority rows,
  Mark-of-the-Web) was ported unchanged and never compiled here.
- Tray options, notifications and "Show free disk space" were stored in Phase 7 and are used from Phase 8.
- The Options hook is not installed in the `rotorrent_window` preview (it has no `IApplication`); the real `rotorrent` executable installs it.
- Screenshots are Linux / offscreen / Qt 6.7.3 at 100%. The Options dialog was not checked at 125-300% scaling or against the kit's screenshots side by side.
- Watched-folder rows and ban list were exercised through the dialogs, not through the real folder watcher picking up a file.
- Dialog height on screens under about 900 px: the Options pages scroll; the watched-folder parameters scroll; nothing else was checked on small screens.

## Phase 8 (tray, notifications, status bar, system integration): built, verified headless on Linux only

Run `test/run_all.sh <build-dir>` (363 checks, 0 failures on Linux / GCC 13 / Qt 6.7.3 / libtorrent 2.0.10, offscreen). New tests:
`test_statusbar`, `test_tray`, `test_power`, `test_updatecheck` (no engine) and `test_statusfeed` (real session). Manual script: `docs/QA.md`, "Phase 8".

### What exists now
- **Status bar** (`gui/widgets/StatusBar.*`, `RoStatusBar`): 26 px `QFrame#statusBar`, right-aligned: connection chip (Connected / Firewalled / Offline, coloured
  dot, click opens Options > Connection), speed-limit chip (only while a limit is set, click opens Speed limits), "N DHT nodes", "Free space: X"
  (hidden by Options > Behavior > "Show free disk space"). The row keeps its natural width and is clipped, never squeezed. View > Status Bar now works.
- **Offline banner** (`gui/widgets/NetBanner.*`): under the toolbar, inside the right column, with a Retry button.
- **StatusFeed** (`gui/StatusFeed.*`): session to bar. Offline = the session is not listening (upstream's definition); Firewalled = listening with no incoming
  connections. The banner appears only after 3 s offline: a session that has just started is not listening yet (the test saw exactly that), so no
  flash at every launch. "Network connection lost / restored" go to the Log (no "lost" message if the session never got online).
- **Tray** (`gui/tray/RoTray.*`): RO mark on a rounded dark tile in 8 sizes; left click toggles the window (DoubleClick ignored, Windows sends it after Trigger);
  menu Show/Hide RoTorrent, Pause/Resume Session, Exit RoTorrent. Hide, minimize and quit rules live in `MainWindow` (`closeEvent`, `changeEvent`,
  `showForStartup`): X hides to the tray while the icon is visible and "Close to notification area" is on, otherwise it asks as before and quits; minimize hides
  when "Minimize to notification area" is on; start-up "Hidden" falls back to minimized when there is no tray, so the window is never unreachable.
  `Application` now sets `setQuitOnLastWindowClosed(false)`.
- **Toasts** (`gui/tray/Toast.*`, `NotificationFeed.*`): themed card bottom-right, newest at the bottom, hover keeps it open (2 s grace), X dismisses, click opens
  the app. Fed by the session: Download completed, I/O Error, Torrent added (own switch). Native balloon in addition when `ROTORRENT_NATIVE_TOASTS=1` or
  `Gui/Notifications/Native=true`. The engine already writes the matching Log lines, so none are duplicated.
- **On Downloads Done** is now connected to the engine. Before, the menu stored `Gui/AutoAction` and `Application::allTorrentsFinished` read other
  Preferences flags, so the choice did nothing. The menu now reads and writes those flags, and follows them when the engine resets them.
- **Power** (`gui/power/PowerController.*`): upstream's rule (downloading / seeding options, moving files always) as a pure function, polled every 60 s and on session
  events; Windows `SetThreadExecutionState`, released on exit. A no-op elsewhere.
- **Update check** (`gui/update/UpdateChecker.*`): JSON feed over https from the setting `Gui/Update/FeedUrl`; reports not configured / up to date / available / failed;
  opens the download page, never downloads or installs anything (so no signing scheme is needed yet). Without a feed the message is unchanged.
- **File associations**: `dist/windows/rotorrent-associations.reg.in` registers RoTorrent for .torrent and magnet: under the user's Default apps list (Windows does not
  let a program make itself the default). Installer job (Phase 9).
- Backend: `Session::reconfigureListening()` added for Retry (three lines, marked, listed in `docs/UPSTREAM.md`).

### Deviations from the handoff (veto any)
- Tray menu uses the kit's wording "Show / Hide RoTorrent" and "Exit RoTorrent" (spec text says "Show / Hide ... Exit").
- Toasts and the native balloon follow the notification-area setting, as the kit does (no icon, no notifications).
- The Offline tooltip says "RoTorrent is not listening for connections" rather than "no network connection": that is what the engine can tell. The banner text is the kit's.
- The tray-closed reminder balloon qBittorrent shows once is not built (not in the spec).
- Free space reads "N/A" until the engine's disk checker has run.

### Corrections to earlier notes
- "Watched folders add without showing the dialog" is **upstream behaviour**, not a gap: `TorrentFilesWatcher::onTorrentFound` calls `Session::addTorrent` directly
  and only string sources go through the Add dialog. Checked against `guiaddtorrentmanager.cpp`.

### Not done / not verified (Phase 8)
- **Nothing was compiled with MSVC or run on Windows.** Untested: the real notification area and its icon at 100-300%, `QSystemTrayIcon::activated` behaviour,
  taskbar restore from a minimized-to-tray window, toasts on multiple monitors or with a bottom/side taskbar, `SetThreadExecutionState` (`powercfg /requests`),
  the registry file.
- **Shutdown, suspend, hibernate and reboot were not run.** Only the menu to Preferences link is tested; `Application::allTorrentsFinished` and the countdown
  dialog were not exercised here. Use a VM (spec Phase 8).
- The update check was tested without the network (parsing, ordering, not-configured path); a real https fetch was not.
- Toast look was not compared pixel-wise with a design screenshot (the kit has none); the status bar and banner were not compared with
  `screenshots/new/*` side by side. Offscreen screenshots were not taken for these.
- Phase 9 was untouched when this section was written; see the Phase 9 section below.

## Phase 9 (hardening, packaging, release): partly done, nothing run on Windows

### Done and checked on Linux
- **Translations:** `translations/rotorrent_en.ts` (1,681 strings). Found and fixed 37 strings `lupdate` could not see: file-scope
  `QT_TR_NOOP` tables in `GeneralTab`, `DetailsPanel`, `LogTab` (now `QT_TRANSLATE_NOOP` with their class as context), the sidebar status
  and speed-card tables, and one untranslated Advanced tooltip. Every extracted string passes the label dictionary.
- **New checks** (in `test/run_all.sh`): `check_translations.sh`, `check_tr_literals.sh`, `check_licences.sh`, `check_placeholders.sh`
  (`RELEASE=1` turns it into a gate). The tr() check was shown to fail when a tooltip loses its `tr()`.
- **Licences:** SPDX marker added to 144 RoTorrent-original files (none to upstream files); Nunito Sans OFL verified and added;
  upstream commit verified against GitHub (sample of 7 files byte-identical); `docs/COMPLIANCE.md`.
- **Windows resources:** `rotorrent.ico` (made by `tools/make_ico.py`), `rotorrent.rc.in` with icon, VERSIONINFO and manifest; the
  version is now single-sourced from `project(VERSION)` (`version.h.in` and the resource are configured from it). CMake configures on Linux.
- **Packaging files:** `dist/windows/package.ps1`, `portable/` (launcher, README, source offer, opt-in Register / Unregister scripts),
  `rotorrent.nsi` (compiles with `makensis` against a dummy folder, no warnings).
- **Docs:** `RELEASE.md`, `COMPLIANCE.md`, `PLACEHOLDERS.md`, Phase 9 section of `QA.md`.

### Corrections found during Phase 9
- `--no-legal` does not exist; the option is `--confirm-legal-notice` (used by the packaging smoke test).
- Options > Behavior > "Open default apps" only opens Windows Settings; it registers nothing. The portable zip therefore ships the optional
  Register / Unregister scripts and the installer writes the same keys.
- The executable had no icon or version block before this phase.

### Not done / not verified (Phase 9)
- **Nothing was compiled after the Phase 9 source edits** (Qt in the sandbox is 6.4.2, the code needs 6.5+; one CPU core). The edits are small
  (`QT_TRANSLATE_NOOP` / `QT_TR_NOOP` in table initialisers, one `tr()`, SPDX comment lines, CMake and resource changes) but are unproven by a build.
  The earlier 363 checks were not re-run.
- **`package.ps1`, the `.bat` files, the `.rc` and the NSIS installer have not run on Windows.** `package.ps1` was not even parsed by PowerShell.
- The Appendix D matrix, Snap, tray, toasts, power actions and the clean-VM run are open (`docs/QA.md`).
- 10 placeholder lines remain by design (`docs/PLACEHOLDERS.md`); the update feed, signing certificate and the Qt / vcpkg licence texts are open.
- The peer-ID code is still `qB` (owner decision); see `docs/COMPLIANCE.md`.
- `docs/reference/screenshots` comparison for the Phase 9 screens was not possible without Windows.

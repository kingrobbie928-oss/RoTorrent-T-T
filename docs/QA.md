# RoTorrent manual QA script

Automated coverage: `test_actionrules` (enable rules), `test_commands` (real session + real window: commands,
queue visibility, categories, Pause/Resume Session, shortcuts, label audit), `check_labels.sh` (source wording).
Everything below must still be exercised by hand on Windows 10 and 11, in Dark and Light, at 100% and 125%.

## Phase 4 - toolbar, menus, actions

Setup: a profile with at least one finished torrent, two unfinished ones, one Paused, one errored / missing-files.

### Menus (handoff 8.4)
| Menu | Item | Check |
|---|---|---|
| File | Add Torrent File... (Ctrl+O) | opens the picker (placeholder dialog until phase 6) |
| File | Add Torrent Link... (Ctrl+U), Create Torrent... | placeholder dialogs until phase 6 |
| File | Exit (Ctrl+Q) | quits; with an unfinished transfer asks No / Yes / Always Yes; "Always Yes" stops asking |
| Edit | Resume (Ctrl+S) / Pause (Ctrl+P) | enabled per the selected torrent; row status follows |
| Edit | Remove | opens the Remove dialog; Cancel keeps the torrent; "Also permanently delete the files" deletes data |
| Edit | Top / Move Up / Move Down / Bottom of Queue | visible only with Torrent Queueing on; enabled only for an unfinished selection |
| Edit | Pause Session (Ctrl+Shift+P) | stops every active torrent; label, icon and shortcut become Resume Session (Ctrl+Shift+S); resume restores each previous state (forced stays forced) |
| View | Top Toolbar, Status Bar, Filters Sidebar | checkbox column painted; choice survives restart; Statistics (Ctrl+I) is a placeholder |
| Tools | Torrent Creator, Manage Cookies..., Options... (Alt+O) | placeholders until phases 6 / 7 |
| Tools | On Downloads Done | six radio items, one checked, choice survives restart (action performed in phase 8) |
| Help | Documentation (F1) | opens the placeholder documentation URL |
| Help | Check for Updates | "Update checking is not configured." |
| Help | About | placeholder until phase 6; there is no Donate item |

### Toolbar (8.5)
Create | Pause, Resume | Delete (red hover) | queue group (hides with queueing off) | Settings | search box | link, Add torrent | theme + Vibrant toggles.
Resize the window: nothing wraps, only the search box shrinks. Delete goes through the same Remove dialog.

### Torrent context menu (8.8) - right-click a row of each kind
- running unfinished: Pause, Force Resume, ..., sequential / first-last checkable, Force reannounce enabled
- Paused: Resume, Force Resume; Force reannounce disabled with the tooltip
- forced: Resume and Pause, no Force Resume
- error / missing files: Resume, Force Resume
- finished: Super seeding mode shown; no sequential / first-last; no Queue submenu
- Category submenu: New... (dialog), Reset, one checkable entry per category (current one checked)
- Automatic Torrent Management: confirms when enabling, tooltip shown
- Force recheck: confirms unless "Confirm torrent recheck" is off
- Open destination folder opens Explorer; Copy submenu (Name, Info hash v1/v2, Magnet link, Torrent ID, Comment, Content Path) fills the clipboard
- Export .torrent...: save dialog; a failure shows the styled error box
- Queue submenu: only with queueing on and an unfinished torrent
- Rename...: dialog "Rename torrent" / "New name:"; Set location...: folder picker, data moves

### Sidebar menus (8.6)
Status row: Resume / Pause / Remove torrents act on every torrent in the filter. Tracker row: Copy tracker URL first. Category row:
Add / Edit / Remove category, Remove unused categories. Empty space or Categories header: Add category..., Remove unused categories.
An empty category still appears in the sidebar.

### Label audit
No "Start", "Stop", "Stopped", "Force Start", "Finished" or "qBittorrent" anywhere in the UI (About credit excepted).

## Phase 5 - details panel and tabs

Automated: `test_details` (offscreen, demo data) and `test_details_engine` (live session). By hand, on Windows 10 / 11,
Dark and Light, 100% and 125%:

| Area | Check |
|---|---|
| Panel | Starts collapsed with only the tab row. Click any tab: opens at 216 px on that tab. Chevron opens General / collapses and rotates. |
| Handle | Hover the top edge: red pill appears, cursor is vertical resize. Drag up: taller, never above 75%. Drag below 90 px: snaps collapsed. Press on the collapsed handle: expands first. Height survives restart. |
| Selection | Select another torrent: the open tab refreshes. Nothing selected: every tab except Log says "Select a torrent to see its details." |
| General | Values refresh at the list interval and match the real torrent (sizes, ratio, limits, hashes, dates). Magnet still fetching metadata: no crash, Pieces shows a dash. |
| Trackers | DHT / PeX / LSD first. Add trackers..., Edit tracker URL..., Remove tracker, Copy tracker URL, both reannounce items. Pseudo rows cannot be edited / removed. Selection survives refresh. |
| Peers | Rows while active; empty text when paused. Add peers... (valid and invalid lines), Copy IP:port, Ban peer permanently (confirmation), banned IP appears in Options > Connection. |
| HTTP Sources | Empty text; right-click on the empty text offers Add HTTP source...; add, copy, remove. |
| Content | Tree expanded; check / uncheck files and folders (tri-state); priority submenu; Rename file and folder; filter; Select All / None; Open and Open containing folder on a downloaded file. |
| Log | Four chips filter immediately; colours; scrolls to newest; choices survive restart and theme switch. |
| Theme | Switch Dark / Light / Vibrant with a tab open: nothing lost, header and chip colours follow. |
| Labels | No "Start", "Stop", "Stopped", "Finished" or "qBittorrent" visible in any tab or menu. |

## Phase 6 - Add torrent dialog (slice 3)

By hand on Windows 10 / 11, Dark and Light, 100% and 125%.

| Area | Check |
|---|---|
| File > Add Torrent File | Picker first; one dialog per chosen file; all fields below present |
| Torrent Management Mode | Automatic greys out Save at and the incomplete path; the torrent lands in the category's folder |
| Incomplete path | Ticking enables the path field; the torrent downloads to it until finished |
| Pause condition | Metadata received / Files checked: the torrent shows Paused at that point |
| Skip hash check | Only on a complete data set: the torrent shows Seeding without a recheck |
| Save as .torrent file... | Disabled until the metadata is known; writes a valid file |
| Magnet link | "Retrieving metadata..." with the busy bar; the tree appears; unticked files stay unticked after adding |
| Magnet, press OK early | Torrent is added; Content tab gets its files later |
| Magnet, Cancel | No leftover hidden torrent in the list |
| Do not delete .torrent file | With Options > delete mode "IfAdded": file removed after adding unless ticked |
| Never show again | Next adds skip the dialog and use the defaults (turn it back on is phase 7) |
| http(s) link | Add torrent links with a web address downloads, then shows the dialog; a bad address shows an error box |


## Phase 6 - Torrent Creator (slice 4)

By hand on Windows 10 / 11, Dark and Light, 100% and 125%. Compare with `screenshots/dialogs/{dark,light}_creator.png`.

| Area | Check |
|---|---|
| Entry points | File > Create Torrent..., Tools > Torrent Creator and the toolbar button all open the dialog |
| Drop zone | Drag a folder from Explorer over it: accent outline and red wash; drop fills Path; dragging away clears it |
| Select file / folder | Both pickers fill Path; a shortcut (.lnk) can be picked |
| Create Torrent | Disabled with no path; asks where to save (default name = folder name + .torrent) |
| Progress | A multi-GB folder shows the bar moving; the form is locked meanwhile |
| Cancel | Mid-creation: stops, bar back to 0%, no .torrent written, dialog stays open; Cancel again closes |
| Other client | Open the new .torrent in a second client (qBittorrent, Transmission): same name, size, trackers; a recheck on the data passes; hybrid shows both hashes |
| Private / trackers | Private torrent with two tiers (blank line between): the second client lists both tiers |
| Seed immediately | The torrent appears in the list as Seeding; with "Ignore share ratio limits" its share limits show as unlimited |
| Failures | Unreadable folder, read-only target folder: styled error box, no half-written file |
| Settings | Reopen the dialog: the last format, piece size, flags, trackers, comment, source and path are back |

## Phase 6 slice 5 (manual)
| Step | Expect |
|---|---|
| Right-click a torrent > Torrent options... | Dialog matches `docs/phase6/options_single_*.png`; OK applies limits, category, share limits |
| Select two torrents with different limits > Torrent options... | Inequality sign / partial checks; untouched fields keep each torrent's value |
| Tools > Manage Cookies... | Table, + adds a row, trash deletes, OK keeps the changes across restarts |
| Right-click > Edit trackers... | Tracker text box; blank line starts a new tier; OK replaces the trackers |
| Right-click > Preview file... on a multi-video torrent | Picker lists only previewable files; Preview opens the default player |
| Same on a torrent with one video | Opens straight away |
| Open the preview of a file that is not downloaded yet | "Preview impossible" box |

## Phase 6 slice 6 (manual)

By hand on Windows 10 / 11, Dark and Light, 100% and 125%. Compare with `docs/phase6/{exit_confirm,shutdown_confirm,legal_notice,crash_info}_*.png`.

| Step | Expect |
|---|---|
| `rotorrent_window --dialog shutdown` (and exit, suspend, hibernate, reboot) | Right title, message and "<Action> Now" button; the counter runs 15, 14, ... and the dialog accepts itself at 0 |
| Same, press Enter at once | Nothing happens to the computer: Cancel has the focus and the default |
| Same, Cancel / Esc / X | Closes, the counter stops, the action is abandoned |
| `--dialog exit`, tick "Don't show again", Exit Now | Next time the app exits without asking; no other action shows the box |
| `rotorrent_window --dialog legal` | Heading, notice, grey closing line, one OK; centred on the screen |
| First launch of the real app (slice 7) | The legal notice appears before the window and is not shown again after OK |
| `rotorrent_window --dialog crash` | Red headline, link in the accent colour (readable in Dark), version block, fenced trace, outlined Close |
| Force a crash in a Release build with its .pdb next to the exe | Report on stderr and the Crash info dialog with named frames; Close, then the process ends |
| Same with `ROTORRENT_NO_CRASH_DIALOG=1` | No dialog, report on stderr only |

## Phase 6 slice 7 (manual)

Automated: `test_dragdrop` (what a drag carries, the hook), `run_instances.sh` (real exe: second instance forwards a .torrent,
a magnet, and a no-argument start). By hand on Windows 10 / 11, Dark and Light, 100% and 125%:

| Step | Expect |
|---|---|
| Start `rotorrent.exe` on a fresh profile | Legal notice first, then the main window; the notice is not shown again after OK |
| Drag a .torrent from Explorer onto the window (list, sidebar, details, empty state) | Add New Torrent opens for it; the cursor shows a copy, not a "no" sign |
| Drag a magnet link from a browser; drag two .torrent files at once | Dialog for the magnet; one dialog per file, one after the other |
| Drag a folder or a .txt | The cursor shows "no", nothing opens |
| `rotorrent.exe C:\\x.torrent` while it is running | No second window; the first opens the dialog for it |
| Same with `--skip-dialog=true --add-paused=true --save-path=D:\\Data` | Torrent appears Paused in the list, saved under D:\\Data, no dialog |
| Same with `--category=Movies --skip-dialog=false` | Dialog opens with Movies selected and its save path |
| `rotorrent.exe` with no arguments while running (and while minimized) | The existing window comes to the front, restored |
| `rotorrent.exe --help` | A styled message box (no console), nothing says "stopped" or "splash" |
| Speed Limits dialog | A slider beside each box; drag moves the box, typing moves the slider; OK applies |
| About dialog, About tab | RO logo about 70 px tall, centred above the text, sharp at 125% |
| Close the main window | Confirmation as set in options, then the program exits and `rotorrent.exe` is gone from Task Manager |

## Phase 7 (manual) - Options and Advanced
`test/test_options` checks structure, binding, Apply / OK / Cancel and the live effects headless. It cannot judge how things
look or feel, and nothing here has run on Windows. Compare each page with `docs/phase7/` and the kit's screenshots.
| Step | Expect |
|---|---|
| Tools > Options, Alt+O, toolbar Settings, **and** the status-bar chips (Phase 8) | The same dialog; it reopens on the page you last used |
| Category list | Six rows (Behavior, Downloads, Connection, Speed, BitTorrent, Advanced), 38 px, icons go muted to text on hover; selected = surface3 pill |
| Every page at 100% / 125% / 150% / 175% in Dark and Light | No pale panels, no clipped numbers (ports, schedule times), no overlapping rows, labels readable |
| Change a value, press **Apply** | Dialog stays open, Apply greys out, the change is live; **Cancel** afterwards keeps it |
| Change a value, press **Cancel** | Reopen: the old value is shown |
| Behavior > Color scheme, then the toolbar Dark / Light toggle | They stay in step, with the dialog open too; no flicker |
| Behavior > Action on double-click, both combos | Double-click an unfinished / finished torrent does what is chosen (pause / resume, open folder, preview, options, nothing) |
| Behavior > Window state on start up = Minimized, restart | Starts minimized (to the tray when "Minimize to notification area" is on); Hidden stays in the tray |
| Behavior > Run RoTorrent when Windows starts (Windows) | Registry Run key appears / disappears; the Default apps button opens Windows Settings |
| Downloads > Display torrent content and some options, off then on | The Add dialog stops / starts appearing (one setting, shared with its "Never show again") |
| Downloads > Watched folders > Add, Edit, Remove | Folder dialog in the shell; options kept per folder; a .torrent dropped in the folder is added |
| Connection > Banned IP addresses | Dialog in the shell; Ban, Delete, OK; the address shows up as banned in the Peers tab |
| BitTorrent > Torrent Queueing off / on | The "#" column, toolbar queue group, Edit-menu items and context Queue submenu hide / show **at once** |
| Advanced | Two cards, ROTORRENT and LIBTORRENT, Documentation links in accent; i icon tooltip "More information"; rows never scroll sideways |
| Advanced > Customize application instance name | The window title (taskbar) becomes "RoTorrent - name" |
| Search for Search, RSS, Web UI anywhere in Options | Not there |

## Phase 8 (manual) - tray, toasts, status bar, power, updates

`test_statusbar`, `test_tray`, `test_power`, `test_updatecheck` and `test_statusfeed` check the logic headless on Linux. None of it has run on
Windows: the items below are the ones only a Windows machine (use a VM for the power rows) can settle.

| Step | Expect |
|---|---|
| Start RoTorrent | Notification-area icon: RO mark on a rounded dark tile, crisp at 100% and 150% |
| Left-click the icon, repeatedly | Window hides / shows, one toggle per click (a double-click must not flip it twice) |
| Right-click the icon | Show or Hide RoTorrent, Pause or Resume Session, Exit RoTorrent, styled like the other menus, with icons |
| Tray > Pause Session, then the menu again | Label is Resume Session; every active torrent paused; Edit menu agrees |
| Click X on the window | Hides to the tray, Log says "Main window hidden: RoTorrent keeps running in the notification area" |
| Options > Behavior > Close to notification area off, click X | Asks to confirm if transfers are running, then really exits |
| Options > Behavior > Minimize to notification area on, minimize | Window disappears from the taskbar; tray click restores it un-minimized |
| Options > Behavior > Show RoTorrent in notification area off, Apply | Icon disappears at once; X now exits; start-up "Hidden" opens minimized instead |
| Switch Dark / Light / Vibrant while hidden in the tray, then restore | Window comes back with the new theme, nothing lost |
| Tray > Exit RoTorrent | Process ends (confirmation only if transfers are running) |
| Let a download finish | Toast "Download completed" bottom-right, themed; the Log has "Torrent download finished"; click opens the window |
| Force an I/O error (read-only save folder) | Red toast "I/O Error" with the reason |
| Options > Advanced > Display notifications for added torrents on, add a torrent | Blue "Torrent added" toast; off: none |
| Hover a toast | It stays; leave it and it fades after about 2 s; the X dismisses; several toasts stack upward |
| ROTORRENT_NATIVE_TOASTS=1 | A native Windows balloon appears **in addition** (needs the installed build's shortcut) |
| Status bar | 26 px, right-aligned: connection chip, speed-limit chip (only while a limit is set), "N DHT nodes", "Free space: X" |
| Connected / Firewalled / Offline | Green / amber / red dot; tooltips explain; click opens Options > Connection |
| Set a global limit in the Speed limits dialog | Chip shows "↓ 500 KiB/s ↑ ∞" within about 2 s; clicking it opens the dialog; alternative limits show instead while active |
| Disable the network adapter for 10 s | After about 3 s the banner "No network connection..." appears under the toolbar and the Log says "Network connection lost"; Retry works; re-enable: banner goes, "Network connection restored" |
| Options > Behavior > Show free disk space off | The free-space item disappears |
| View > Status Bar off / on | Row hides / shows |
| Narrow the window to 500 px | Status bar clips on the right, labels never squeeze or overlap |
| Tools > On Downloads Done > Exit RoTorrent, finish a download | After the countdown dialog the program exits; choose Cancel and it stays |
| Same with Suspend / Hibernate / Reboot / Shutdown | **VM only.** Countdown dialog, then the action; afterwards the menu shows Do nothing again |
| Options > Behavior > prevent sleep while downloading, start a download | `powercfg /requests` lists RoTorrent under SYSTEM; pausing everything clears it within a minute |
| Help > Check for Updates (no feed) | "Update checking is not configured." |
| Set `Gui/Update/FeedUrl` to an https JSON feed with a newer version | "RoTorrent X is available." with notes, Download opens the page; same version: "You are running the latest version of RoTorrent (1.0.0)." |
| Import `dist/windows/rotorrent-associations.reg.in` (paths filled in) | RoTorrent appears under Settings > Apps > Default apps for .torrent and magnet; opening a .torrent starts or reuses the running instance |

## Phase 9 - release matrix (handoff Appendix D)

Run on **Windows 10 22H2 and Windows 11**, x64, once on a fresh VM and once on a machine that also has qBittorrent installed
(its profile must be untouched afterwards). Record pass / fail and a screenshot per cell in the release report; a failure is P1 (crash,
data loss, cannot start), P2 (feature broken, visible mismatch with the design) or P3 (cosmetic).

| Dimension | Values | Result |
|---|---|---|
| Display scaling | 100, 125, 150, 175, 225, 250, 300 %; window dragged between a 100 % and a 150 % monitor | |
| Theme | Dark, Light, Dark + Vibrant, Light + Vibrant (switch live, no state lost) | |
| Window width | 1280, 900, 700, 500, 360 px, and the 300 x 160 minimum (nothing wraps; only the search box shrinks) | |
| Data | empty list; 9 torrents; 200+ torrents (scroll without lag, idle CPU low); every status incl. Error and Missing Files | |
| Network | normal, firewalled, offline (banner and chip) | |
| Input | keyboard only (Tab, Enter, Esc, all shortcuts), mouse, touch pad | |
| Screens | every item of the menus (8.4), torrent menu (8.8), tabs (8.9), dialogs (8.12), empty states (8.14), in both themes, against `docs/reference/screenshots` | |
| Restart | kill the process mid-download, relaunch: torrents, queue order, categories, settings return | |

### Windows-only behaviour that was never run (must be checked here)
| Area | Check |
|---|---|
| Snap | drag to top / left / right edge, drag a maximized window down, Win+Arrow, double-click title row; one and two monitors |
| Close-button hover | red fill never covers the divider at every scaling above |
| Tray | icon crisp at 100-300 %, left click toggles, right-click menu, close to tray, minimize to tray, restore from taskbar |
| Toasts | finished, I/O error, added; bottom-right on every taskbar position and monitor; click opens the window |
| Power | On Downloads Done: Exit, Suspend, Hibernate, Reboot, Shutdown (VM) with the countdown and Cancel; sleep prevented while downloading (`powercfg /requests`) |
| Single instance | second launch with a .torrent / magnet forwards to the first and exits |
| Crash dialog | force a crash; the dialog shows a stack trace (needs the `.pdb`) |

### Packaging checks
| Check | Expected |
|---|---|
| Unzip the portable zip, run `RoTorrent-portable.bat` | starts, creates `profile\`, nothing written to `%APPDATA%\RoTorrent` |
| Run `rotorrent.exe` directly | uses `%APPDATA%\RoTorrent` / `%LOCALAPPDATA%\RoTorrent` |
| `Register-RoTorrent.bat`, then `Unregister-RoTorrent.bat` | RoTorrent appears in, then disappears from, Default apps; no stray keys left |
| File properties of `rotorrent.exe` | icon visible in Explorer and taskbar; version and "RoTorrent" product name correct |
| Installer: install, start, uninstall | per-user, no UAC; shortcut and Apps & Features entry appear and go; `%APPDATA%\RoTorrent` is kept |
| `licenses\` folder | COPYING*, AUTHORS, Nunito OFL, THIRD_PARTY, Qt notice, libtorrent / Boost / OpenSSL / zlib texts |
| Wire identity | capture one tracker announce: User-Agent `RoTorrent/<ver>`; peer ID prefix as documented in `docs/IDENTITY.md` |

# Placeholders that must not ship

`RELEASE=1 sh test/check_placeholders.sh` fails while any of these remain. They all use the reserved domain `rotorrent.example`.

| Placeholder | Where | Replace with |
|---|---|---|
| `https://rotorrent.example` (Home Page) | `src/gui/dialogs/AboutDialog.cpp` (lines 87-91) | the real website |
| `https://forum.rotorrent.example` | `AboutDialog.cpp` | the forum, or remove the row |
| `https://bugs.rotorrent.example` | `AboutDialog.cpp`, `src/gui/dialogs/StacktraceDialog.cpp` (`TRACKER_URL`), `src/app/crashhandler.cpp` | the issue tracker (three places) |
| `team@rotorrent.example`, "RoTorrent Team" | `AboutDialog.cpp` (Authors tab) | real maintainer name and address |
| `https://rotorrent.example/docs` | `src/gui/MainWindow.cpp` (`DOCS_URL`, Help > Documentation) | the documentation site |
| `https://docs.rotorrent.example/advanced` | `src/gui/options/AdvancedPage.cpp` | the Advanced-settings page |
| `https://docs.rotorrent.example/anonymous-mode` | `src/gui/options/optionsdialog.ui` | the anonymous-mode help page |
| `https://rotorrent.example/source` | `dist/windows/package.ps1` default for `-SourceUrl` | the public source repository |
| `Copyright (C) 2026 The RoTorrent project`, publisher "The RoTorrent project" | About, `rotorrent.rc.in`, `rotorrent.nsi` | the legal owner's name |

The example feed URL in the comment of `src/gui/update/UpdateChecker.h` already uses `example.org` and is not a placeholder.

Also open (not text, so the script cannot catch them): the update-feed URL (`Gui/Update/FeedUrl`, until set the app says
"Update checking is not configured."), the code-signing certificate, and the peer-ID code (`docs/IDENTITY.md`).
Update `docs/PLACEHOLDERS.md` if a placeholder is added or moved.

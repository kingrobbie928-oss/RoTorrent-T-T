#!/usr/bin/env python3
"""Derives src/gui/options/optionsdialog.ui from qBittorrent's src/gui/optionsdialog.ui (Phase 7).

Usage: trim_options_ui.py <upstream optionsdialog.ui> <output .ui>
       trim_options_ui.py --other <upstream .ui> <output .ui>   (the dialogs and widgets Options hosts)

The result is committed; this script is kept so the cut can be audited and repeated against another
upstream snapshot (docs/UPSTREAM.md). Every removal below has a reason in docs/STATUS.md (Phase 7).
Licensed GPL-2.0-or-later (derived from qBittorrent's optionsdialog.ui).
"""
import re
import sys

from lxml import etree

# Pages of removed features (handoff 8.13: Search, RSS, Web UI).
REMOVE_PAGES = ["tabSearchPage", "tabRSSPage", "tabWebUIPage"]
REMOVE_LIST_ITEMS = {"Search", "RSS", "Web UI", "WebUI"}   # upstream spells the last one "WebUI"

# Controls whose feature does not exist in RoTorrent (see STATUS.md, Phase 7, "controls removed").
REMOVE_WIDGETS = [
    # Interface: RoTorrent has its own theme engine (handoff section 11, items 7 and 13).
    "labelRestartRequired", "labelStyle", "comboStyle", "labelStyleHint",
    "checkUseCustomTheme", "checkUseSystemIcon", "buttonCustomizeUITheme",
    # Transfer list: colours, alternate rows and optional columns are fixed by the design (8.7).
    "checkAltRowColors", "checkUseTorrentStatesColors", "checkProgressBarFollowsTextColor",
    "checkHideZero", "comboHideZero",
    # Sidebar: nine fixed status rows, no separate tracker-status filter (8.6).
    "checkBoxHideZeroStatusFilters", "checkBoxUseSeparateTrackerStatusFilter",
    # Content tab: no drag-out of content (8.9).
    "boxTorrentContent",
    # Status bar: the external IP label is removed (8.10).
    "checkBoxExternalIPStatusBar",
    # Desktop: no splash screen (removed in Phase 6), one tray icon design (8.11), macOS-only items.
    "checkShowSplash", "labelTrayIconStyle", "comboTrayIcon",
    "checkAssociateTorrents", "checkAssociateMagnetLinks",
    "checkShowSpeedInDock", "checkShowMenuBarIcon",
    # Update checking is not configured in 1.0 (handoff section 11, item 5); it returns with the
    # Phase 8 update plumbing and a release feed.
    "checkProgramUpdates",
    # Proxy: RSS no longer exists.
    "checkProxyRSS",
    # Add dialog: always a frontmost modal here; upstream's recursive-download preference has no consumer.
    "checkAdditionDialogFront", "checkRecursiveDownload",
]

# Plain text replacements applied to every <string> (order matters).
REWORD = [
    ("Start / stop torrent", "Pause / resume torrent"),
    ("Torrent stop condition:", "Torrent pause condition:"),
    ("Start qBittorrent on Windows start up", "Run RoTorrent when Windows starts"),
    ("Do not start the download automatically", "Add torrents in paused state"),
    ("Stop torrent", "Pause torrent"),
    ("Append .!qB extension to incomplete files", "Append .!rt extension to incomplete files"),
    ("Drag content from qBittorrent", "Drag content from RoTorrent"),
    ("When torrent finished move backup to:", "When torrent completed move backup to:"),
    ("Run on torrent finished:", "Run on torrent completed:"),
    ("https://github.com/qbittorrent/qBittorrent/wiki/Anonymous-Mode", "https://docs.rotorrent.example/anonymous-mode"),
    ("Start time", "Time the schedule begins"),
    ("End time", "Time the schedule ends"),
    ("Search engine, software updates or anything else will use proxy", "Software updates or anything else will use proxy"),
    ("&qBittorrent", "RoTorrent"),   # no mnemonic on the product name
    ("qBittorrent", "RoTorrent"),
]

FORBIDDEN = re.compile(r"\bStart\b|\bStop\b|\bStopped\b|Force Start|\bFinished\b|qBittorrent")


def drop(widget):
    """Removes a widget with its layout <item> wrapper, if it has one."""
    parent = widget.getparent()
    if parent is not None and parent.tag == "item":
        grand = parent.getparent()
        grand.remove(parent)
    else:
        parent.remove(widget)


# The other .ui files Options uses: tags editor out, "Start/Stop" wording replaced.
OTHER_REMOVE = ["tagsLabel", "tagsLineEdit", "tagsEditButton"]
OTHER_REWORD = [
    ("Start torrent:", "Resume after adding:"),
    ("Stop condition:", "Pause condition:"),
    ("Stop torrent", "Pause torrent"),
    ("qBittorrent", "RoTorrent"),
]


def other(src, dst):
    tree = etree.parse(src)
    root = tree.getroot()
    for name in OTHER_REMOVE:
        for w in root.xpath(f".//widget[@name='{name}']"):
            drop(w)
    for s in root.iter("string"):
        if s.text:
            for old, new in OTHER_REWORD:
                s.text = s.text.replace(old, new)
    for h in root.xpath("//customwidget/header"):
        h.text = h.text.replace("gui/", "options/") if h.text.startswith("gui/") else "options/" + h.text
    tree.write(dst, encoding="UTF-8", xml_declaration=True)
    for s in root.iter("string"):
        if s.text and FORBIDDEN.search(s.text):
            print("FORBIDDEN:", repr(s.text[:110]))
    print(f"wrote {dst}")


def compact_grid_rows(root, name):
    """Removing widgets from a grid leaves their rows behind as empty space; renumber the rows that remain."""
    layout = root.xpath(f".//layout[@name='{name}']")[0]
    items = layout.findall("item")
    rows = sorted({int(i.get("row")) for i in items})
    mapping = {old: new for new, old in enumerate(rows)}
    for i in items:
        i.set("row", str(mapping[int(i.get("row"))]))


def group_to_checkbox(root, name):
    """A checkable group box whose only content was removed becomes a plain check box."""
    widget = root.xpath(f".//widget[@name='{name}']")[0]
    widget.set("class", "QCheckBox")
    for child in list(widget):
        if child.tag == "layout":
            widget.remove(child)
        elif child.tag == "property" and child.get("name") == "checkable":
            widget.remove(child)
        elif child.tag == "property" and child.get("name") == "title":
            child.set("name", "text")


def main():
    if sys.argv[1] == "--other":
        other(sys.argv[2], sys.argv[3])
        return
    src, dst = sys.argv[1], sys.argv[2]
    tree = etree.parse(src)
    root = tree.getroot()

    removed = []
    for name in REMOVE_PAGES + REMOVE_WIDGETS:
        found = root.xpath(f".//widget[@name='{name}']")
        if not found:
            sys.exit(f"widget not found: {name}")
        drop(found[0])
        removed.append(name)

    group_to_checkbox(root, "checkAdditionDialog")
    compact_grid_rows(root, "layoutStyle")

    lst = root.xpath(".//widget[@name='tabSelection']")[0]
    for item in lst.findall("item"):
        text = item.findtext("property[@name='text']/string")
        if text in REMOVE_LIST_ITEMS:
            lst.remove(item)
            removed.append(f"tab:{text}")

    for s in root.iter("string"):
        if s.text:
            for old, new in REWORD:
                s.text = s.text.replace(old, new)
            # Links in labels: accent colour (the palette's link colour) and no underline (handoff 8.12).
            s.text = re.sub(r'<a href="([^"]+)"(?! style=)', r'<a href="\1" style="text-decoration:none;"', s.text)

    # The dialog is embedded in RoDialog (880 px wide, 1 px border).
    custom = root.xpath("//customwidget/header")
    for h in custom:
        h.text = "options/fspathedit.h"

    tree.write(dst, encoding="UTF-8", xml_declaration=True)

    # Report what still reads wrong, for manual rewording.
    for s in root.iter("string"):
        if s.text and FORBIDDEN.search(s.text):
            print("FORBIDDEN:", repr(s.text[:110]))
    print(f"removed {len(removed)} elements; wrote {dst}")


if __name__ == "__main__":
    main()

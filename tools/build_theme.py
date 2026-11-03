#!/usr/bin/env python3
"""Generates dark.qss / light.qss, themed SVG icons, logos, and Palette.h
from the palette copied 1:1 from ro-torrent-merged.html."""
import re, os, json, base64

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # repo root (this file lives in tools/)
HERE = os.path.join(ROOT, "src", "resources", "ro")                # theme/, icons/, assets/ are written here
SRC  = os.environ.get("RO_HTML", "/mnt/user-data/uploads/ro-torrent-merged__29_.html")

P = {
 "dark": dict(bg="#0c0c0e", surface="#17171b", surface2="#1e1e23", surface3="#26262c", border="#2a2a30",
   text="#f1f1f3", muted="#8c8c96", faint="#55555e", accent="#e8323b", danger="#a01d2b", accent_a=(232,50,59,.14),
   seed="#34c77b", seed_a=(52,199,123,.14), info="#4c8dff", info_a=(76,141,255,.14),
   queued="#a78bfa", queued_a=(167,139,250,.14), check="#f0b429", check_a=(240,180,41,.14),
   stall="#9a9aa2", stall_a=(154,154,162,.14), rowerr=(140,58,42,.20), focus="#ffffff", sel_outline="#ffffff"),
 "light": dict(bg="#f4f4f6", surface="#ffffff", surface2="#f7f7f9", surface3="#ececef", border="#e3e3e8",
   text="#17171a", muted="#6b6b74", faint="#a4a4ad", accent="#e0242c", danger="#a8182a", accent_a=(224,36,44,.08),
   seed="#1fa863", seed_a=(31,168,99,.10), info="#2f7fe0", info_a=(47,127,224,.10),
   queued="#8b5cf6", queued_a=(139,92,246,.10), check="#c98a0e", check_a=(201,138,14,.10),
   stall="#8a8a92", stall_a=(138,138,146,.10), rowerr=(180,74,48,.09), focus="#3a3a3f", sel_outline="#444444"),
}
# light + vibrant ON boosts the washes (from the html)
VIBRANT_LIGHT = dict(seed_a=(31,168,99,.24), info_a=(47,127,224,.24), queued_a=(139,92,246,.24),
   check_a=(201,138,14,.26), stall_a=(138,138,146,.22), accent_a=(224,36,44,.20), rowerr=(180,74,48,.18))

def rgba(t): return "rgba(%d,%d,%d,%d)" % (t[0],t[1],t[2],round(t[3]*255))

# ---------------- icons (24x24 grid, currentColor) ----------------
A='stroke-linecap="round" stroke-linejoin="round"'
ICONS = {
 "create":   (1.9,"",'<rect x="4" y="4" width="16" height="16" rx="2"/><path d="M12 8v8M8 12h8"/>'),
 "pause":    (1.9,"",'<rect x="7.5" y="5" width="3" height="14" rx="1" fill="currentColor" stroke="none"/><rect x="13.5" y="5" width="3" height="14" rx="1" fill="currentColor" stroke="none"/>'),   # solid bars
 "resume":   (1.9,'stroke-linejoin="round"','<path d="M7 5l12 7-12 7V5z"/>'),
 "delete":   (1.9,A,'<path d="M5 7h14M9 7V5a1 1 0 011-1h4a1 1 0 011 1v2m-8 0l1 12a1 1 0 001 1h6a1 1 0 001-1l1-12"/>'),
 "settings": (1.8,"",'<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 00.33 1.82l.06.06a2 2 0 11-2.83 2.83l-.06-.06a1.65 1.65 0 00-1.82-.33 1.65 1.65 0 00-1 1.51V21a2 2 0 01-4 0v-.09A1.65 1.65 0 009 19.4a1.65 1.65 0 00-1.82.33l-.06.06a2 2 0 11-2.83-2.83l.06-.06A1.65 1.65 0 004.68 15a1.65 1.65 0 00-1.51-1H3a2 2 0 010-4h.09A1.65 1.65 0 004.6 9a1.65 1.65 0 00-.33-1.82l-.06-.06a2 2 0 112.83-2.83l.06.06A1.65 1.65 0 009 4.6a1.65 1.65 0 001-1.51V3a2 2 0 014 0v.09a1.65 1.65 0 001 1.51 1.65 1.65 0 001.82-.33l.06-.06a2 2 0 112.83 2.83l-.06.06A1.65 1.65 0 0019.4 9a1.65 1.65 0 001.51 1H21a2 2 0 010 4h-.09a1.65 1.65 0 00-1.51 1z"/>'),
 "filter":   (1.9,A,'<path d="M4 5h16l-6 8v5l-4 2v-7L4 5z"/>'),
 "search":   (2,'stroke-linecap="round"','<circle cx="11" cy="11" r="7"/><path d="M21 21l-4.3-4.3"/>'),
 "link":     (1.9,A,'<path d="M10 13a5 5 0 007 0l3-3a5 5 0 00-7-7l-1.5 1.5"/><path d="M14 11a5 5 0 00-7 0l-3 3a5 5 0 007 7l1.5-1.5"/>'),
 "minus":    (2.2,'stroke-linecap="round"','<path d="M5 12h14"/>'),
 "step_plus": (2.2,'stroke-linecap="round"','<path d="M12 6v12M6 12h12"/>'),
 "plus":     (2.4,'stroke-linecap="round"','<path d="M12 5v14M5 12h14"/>'),
 "chev":     (2,A,'<path d="M6 9l6 6 6-6"/>'),
 "all":      (2,"",'<path d="M12 3v12m0 0l-4-4m4 4l4-4M4 21h16"/>'),
 "dl":       (2.1,"",'<path d="M12 3.5v12M6.5 10.5L12 16l5.5-5.5"/><path d="M5 20.5h14"/>'),   # arrow fills the grid (was a small arrow in the upper-middle)
 "sd":       (2.1,"",'<path d="M12 20.5v-12M6.5 13.5L12 8l5.5 5.5"/><path d="M5 3.5h14"/>'),
 "st":       (2,"",'<rect x="7.5" y="5" width="3" height="14" rx="1" fill="currentColor" stroke="none"/><rect x="13.5" y="5" width="3" height="14" rx="1" fill="currentColor" stroke="none"/>'),   # solid bars (Paused)
 "qu":       (2,"",'<circle cx="12" cy="12" r="8"/><path d="M12 8v4l2.5 2.5"/>'),
 "ck":       (2,"",'<path d="M20 6L9 17l-5-5"/>'),
 "pa":       (2,"",'<circle cx="12" cy="12" r="8"/><path d="M9 9l6 6M15 9l-6 6"/>'),
 "er":       (2,"",'<circle cx="12" cy="12" r="8"/><path d="M12 8v5"/><circle cx="12" cy="16" r="0.6" fill="currentColor" stroke="none"/>'),
 "mf":       (2,"",'<path d="M12 3l9 16H3z"/><path d="M12 10v4"/><circle cx="12" cy="17" r="0.6" fill="currentColor" stroke="none"/>'),
 "fn":       (2,"",'<path d="M4 12l5 5L20 6"/>'),
 "folder":   (2,"",'<path d="M3 7l2-3h5l2 3h9v11a2 2 0 01-2 2H5a2 2 0 01-2-2V7z"/>'),
 "globe":    (2,"",'<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3a15 15 0 010 18M12 3a15 15 0 000 18"/>'),
 "file":     (1.8,"",'<path d="M14 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V8z"/><path d="M14 2v6h6"/>'),
 "info":     (2,A,'<circle cx="12" cy="12" r="9"/><path d="M12 11v5"/><circle cx="12" cy="8" r="0.6" fill="currentColor" stroke="none"/>'),
 "peers":    (2,A,'<path d="M17 21v-2a4 4 0 00-4-4H5a4 4 0 00-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M23 21v-2a4 4 0 00-3-3.87"/><path d="M16 3.13a4 4 0 010 7.75"/>'),
 "http":     (2,A,'<circle cx="12" cy="12" r="9"/><path d="M3 12h18"/><path d="M12 3a15.3 15.3 0 014 9 15.3 15.3 0 01-4 9 15.3 15.3 0 01-4-9 15.3 15.3 0 014-9z"/>'),
 "content":  (2,A,'<path d="M14 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V8z"/><path d="M14 2v6h6"/><path d="M9 13h6M9 17h6"/>'),
 "dht":      (1.8,"",'<circle cx="12" cy="5" r="2"/><circle cx="5" cy="19" r="2"/><circle cx="19" cy="19" r="2"/><path d="M12 7v6m0 0l-5.5 4M12 13l5.5 4"/>'),
}
def svg(name, color="currentColor"):
    w, extra, inner = ICONS[name]
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="{color}" '
            f'stroke-width="{w}" {extra}>{inner}</svg>').replace('fill="currentColor"', f'fill="{color}"')

def toggle_svg(pal, kind, on):
    """40x20 pill toggle drawn exactly like .toggle.mini (track 1.5px muted border, knob 16px)."""
    cx = 29.5 if on else 10.5; cy = 10.5
    knob = (f'<circle cx="{cx}" cy="{cy+1}" r="8" fill="#000" opacity=".28"/>'
            f'<circle cx="{cx}" cy="{cy}" r="7.5" fill="{pal["surface"]}" stroke="{pal["border"]}" stroke-width="1"/>')
    s = .325; g = f'<g transform="translate({cx-6.5},{cy-6.5}) scale({s})">'
    if kind == "theme":
        if not on:  # dark state: moon
            g += (f'<circle cx="20" cy="20" r="8" fill="{pal["accent"]}"/>'
                  f'<circle cx="23" cy="17" r="7.2" fill="{pal["surface"]}"/>')
        else:       # light state: sun + rays
            rays = [(20,4,20,8),(20,32,20,36),(4,20,8,20),(32,20,36,20),(8.9,8.9,11.7,11.7),
                    (28.3,28.3,31.1,31.1),(8.9,31.1,11.7,28.3),(28.3,11.7,31.1,8.9)]
            g += "".join(f'<line x1="{a}" y1="{b}" x2="{c}" y2="{d}" stroke="{pal["accent"]}" stroke-width="2.4" stroke-linecap="round"/>' for a,b,c,d in rays)
            g += f'<circle cx="20" cy="20" r="8" fill="{pal["accent"]}"/>'
    else:  # vibrant
        if not on: g += f'<circle cx="20" cy="20" r="6" fill="{pal["muted"]}"/>'
        else:
            g += (f'<circle cx="12" cy="12" r="5" fill="{pal["seed"]}"/><circle cx="28" cy="12" r="5" fill="{pal["info"]}"/>'
                  f'<circle cx="12" cy="28" r="5" fill="{pal["check"]}"/><circle cx="28" cy="28" r="5" fill="{pal["queued"]}"/>')
    g += '</g>'
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 40 20" width="40" height="20">'
            f'<rect x=".75" y=".75" width="38.5" height="18.5" rx="9.25" fill="{pal["surface3"]}" stroke="{pal["muted"]}" stroke-width="1.5"/>'
            f'{knob}{g}</svg>')

# ---------------- QSS template ----------------
QSS = r"""
/* RoTorrent — @THEME theme. Generated by build_theme.py — edit the template, not this file.
   Object names below are what you set with setObjectName() in the C++ UI code. */

* { font-family: "Nunito Sans"; outline: 0; }   /* base size (13px) is set on the application font, not here: a font-size on * would override every setFont() in code */
QMainWindow, QDialog { background: @bg; color: @text; }
QWidget#central { background: @bg; }
QLabel { background: transparent; color: @text; }
QToolTip { background: @surface3; color: @text; border: 1px solid @border; border-radius: 0px; padding: 1px; font-size: 12px; }   /* square + compact. Qt applies one padding value to all four sides of a tooltip, so 1px keeps it tight vertically while leaving ~5px at the sides */

/* ================= MENU BAR ================= */
QWidget#menuWrap { background: @surface; }
QFrame#hairline { background: @border; border: 0; }
QMenuBar { background: transparent; color: @muted; border: 0; min-height: 0px; padding: 0; font-size: 12.5px; spacing: 2px; }
QMenuBar::item { background: transparent; color: @muted; padding: 5px 10px; border-radius: 6px; margin: 8px 0; }   /* 28px pill centred in the 44px row (was 40px, nearly touching the top and bottom edges) */
QMenuBar::item:selected, QMenuBar::item:pressed { background: @surface3; color: @text; }
QMenu { background: @surface; color: @text; border: 1px solid @border; border-radius: 9px; padding: 4px; font-size: 12.5px; }
QMenu::item { padding: 7px 10px 7px 8px; min-width: 132px; border-radius: 6px; background: transparent; }   /* row = 12px inset | 16px icon | 10px gap | label ... shortcut | 12px inset */
QMenu::icon { margin-left: 6px; margin-right: 8px; }
QMenu::item:selected { background: @surface3; color: @text; }
QMenu::item:disabled { color: @faint; }
QMenu::separator { height: 1px; background: @border; margin: 4px 2px; }
QMenu::indicator { width: 13px; height: 13px; left: 10px; }
QMenu::indicator:checked { image: url("@ICONS/ck_accent.svg"); }
QMenu::right-arrow { image: url("@ICONS/chev_right.svg"); width: 10px; height: 10px; margin-right: 6px; }
/* menus that contain checkable items: the icon area is 34px = [checkbox 13px | gap | glyph 16px] so the box and the logo sit side by side (Qt hides its own indicator when an item has an icon) */
QMenu[checks="true"] { icon-size: 34px; }
QMenu[checks="true"]::item { icon-size: 34px; padding: 0px 10px 0px 6px; }
QMenu[checks="true"]::icon { margin-left: 0px; margin-right: 6px; }

/* Log tab (bottom panel) */
QPushButton#logChip { background: transparent; color: @faint; border: 1px solid @border; border-radius: 6px; padding: 0 10px; min-height: 24px; font-size: 11.5px; font-weight: 600; }
QPushButton#logChip:hover { color: @text; background: @surface2; }
QPushButton#logChip:checked { color: @text; background: @surface3; }
QTextEdit#logText { background: transparent; color: @text; border: 0; font-size: 12px; selection-background-color: @surface3; selection-color: @text; }

/* dialogs (frameless, same chrome as the main window) */
QDialog#appDialog { background: @surface; border: 1px solid @border; }
QWidget#dlgHead { background: @surface; }
QLabel#dlgTitle { color: @text; font-size: 13px; font-weight: 600; }
QLabel#dlgHeading { color: @muted; font-size: 10.5px; font-weight: 600; }
QLabel#dlgKey { color: @muted; font-size: 12.5px; }
QLabel#dlgVal { color: @text; font-size: 12.5px; font-weight: 600; }
QLabel#dlgBodyText { color: @muted; font-size: 12.5px; }
QLabel#dlgName { color: @text; font-size: 17px; font-weight: 700; }
QPushButton#dlgBtn { background: transparent; color: @accent; border: 1px solid @accent; border-radius: 8px; padding: 0 18px; min-height: 30px; font-weight: 600; font-size: 12.5px; }
QPushButton#dlgBtn:hover { background: @accent; color: #ffffff; }

/* Details-panel tables (Trackers, Peers, HTTP Sources, Content) */
QTableWidget#dTable, QTreeWidget#dTable { background: transparent; border: 0; outline: 0; color: @text; font-size: 12.5px; selection-background-color: @surface3; selection-color: @text; }
QTableWidget#dTable::item, QTreeWidget#dTable::item { background: transparent; padding: 0 6px; border-bottom: 1px solid @border; }
QTableWidget#dTable::item:hover, QTreeWidget#dTable::item:hover { background: @surface; }
QTableWidget#dTable::item:selected, QTreeWidget#dTable::item:selected { background: @surface3; color: @text; }
QTableWidget#dTable QHeaderView, QTreeWidget#dTable QHeaderView { background: transparent; border: 0; }
QTableWidget#dTable QHeaderView::section, QTreeWidget#dTable QHeaderView::section { background: transparent; color: @muted; padding: 8px 10px; border: 0; border-bottom: 1px solid @border; font-size: 10.5px; font-weight: 600; }
QTreeWidget#dTable::indicator { width: 14px; height: 14px; }
QTreeWidget#dTable::indicator:unchecked { image: url("@ICONS/cb_off.svg"); }
QTreeWidget#dTable::indicator:checked { image: url("@ICONS/cb_on.svg"); }
QTreeWidget#dTable::indicator:indeterminate { image: url("@ICONS/cb_part.svg"); }
QTreeWidget#dTable::branch { background: transparent; }
QLabel#dHeading { color: @faint; }
QPushButton#dBtn { background: @surface3; color: @text; border: 1px solid @border; border-radius: 8px; padding: 0 14px; font-size: 12px; font-weight: 600; }
QPushButton#dBtn:hover { border-color: @muted; }
QPushButton#dBtn:pressed { background: @surface2; }
/* ===================== forms: layouts loaded from qBittorrent's .ui files (embedded root = QDialog#roUi) ===================== */
QDialog#roUi { background: @surface; }
QDialog#roUi QLabel { color: @text; background: transparent; }
QDialog#roUi QLabel:disabled, QDialog#roUi QCheckBox:disabled, QDialog#roUi QRadioButton:disabled, QDialog#roUi QGroupBox:disabled { color: @faint; }
QDialog#roUi QScrollArea, QDialog#roUi QStackedWidget { background: transparent; border: 0; }
QDialog#roUi QScrollArea > QWidget > QWidget { background: transparent; }
QDialog#roUi QGroupBox { background: transparent; border: 1px solid @border; border-radius: 10px; margin-top: 14px; padding: 16px 10px 8px 10px; color: @text; font-weight: 600; }
QDialog#roUi QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 12px; padding: 0 6px; color: @muted; }
QDialog#roUi QGroupBox::indicator { width: 16px; height: 16px; }
QDialog#roUi QGroupBox::indicator:unchecked { image: url("@ICONS/cb_off.svg"); }
QDialog#roUi QGroupBox::indicator:checked { image: url("@ICONS/cb_on.svg"); }
QDialog#roUi QCheckBox, QDialog#roUi QRadioButton { color: @text; background: transparent; spacing: 9px; }
QDialog#roUi QCheckBox::indicator, QDialog#roUi QRadioButton::indicator { width: 16px; height: 16px; }
QDialog#roUi QCheckBox::indicator:unchecked { image: url("@ICONS/cb_off.svg"); }
QDialog#roUi QCheckBox::indicator:unchecked:hover { image: url("@ICONS/cb_off_hover.svg"); }
QDialog#roUi QCheckBox::indicator:checked { image: url("@ICONS/cb_on.svg"); }
QDialog#roUi QCheckBox::indicator:checked:hover { image: url("@ICONS/cb_on_hover.svg"); }
QDialog#roUi QCheckBox::indicator:indeterminate { image: url("@ICONS/cb_part.svg"); }
QDialog#roUi QCheckBox::indicator:unchecked:disabled { image: url("@ICONS/cb_dis_off.svg"); }
QDialog#roUi QCheckBox::indicator:checked:disabled { image: url("@ICONS/cb_dis_on.svg"); }
QDialog#roUi QRadioButton::indicator:unchecked { image: url("@ICONS/rb_off.svg"); }
QDialog#roUi QRadioButton::indicator:unchecked:hover { image: url("@ICONS/rb_off_hover.svg"); }
QDialog#roUi QRadioButton::indicator:checked { image: url("@ICONS/rb_on.svg"); }
QDialog#roUi QRadioButton::indicator:checked:hover { image: url("@ICONS/rb_on_hover.svg"); }
QDialog#roUi QRadioButton::indicator:unchecked:disabled { image: url("@ICONS/rb_dis_off.svg"); }
QDialog#roUi QRadioButton::indicator:checked:disabled { image: url("@ICONS/rb_dis_on.svg"); }
QDialog#roUi QLineEdit, QDialog#roUi QAbstractSpinBox, QDialog#roUi QComboBox, QDialog#roUi QDateTimeEdit { background: @surface2; color: @text; border: 1px solid @border; border-radius: 8px; padding: 5px 10px; min-height: 20px; selection-background-color: @surface3; selection-color: @text; }
QDialog#roUi QLineEdit:focus, QDialog#roUi QAbstractSpinBox:focus, QDialog#roUi QComboBox:focus, QDialog#roUi QComboBox:on { border-color: @focus; }
QDialog#roUi QLineEdit:disabled, QDialog#roUi QAbstractSpinBox:disabled, QDialog#roUi QComboBox:disabled { background: transparent; color: @faint; border-color: @border; }
QDialog#roUi QLineEdit:read-only { color: @muted; }
/* number fields: a clean [ value  − | + ] stepper instead of two tiny stacked chevrons */
QDialog#roUi QAbstractSpinBox { padding-right: 4px; }   /* Qt already reserves room for the stepper buttons; extra padding clipped the text */
QDialog#roUi QAbstractSpinBox::up-button, QDialog#roUi QAbstractSpinBox::down-button { subcontrol-origin: border; subcontrol-position: center right; width: 26px; height: 24px; border: 0; border-radius: 6px; background: transparent; }
QDialog#roUi QAbstractSpinBox::up-button { margin-right: 4px; }
QDialog#roUi QAbstractSpinBox::down-button { margin-right: 33px; }
QDialog#roUi QAbstractSpinBox::up-button:hover, QDialog#roUi QAbstractSpinBox::down-button:hover { background: @surface3; }
QDialog#roUi QAbstractSpinBox::up-button:pressed, QDialog#roUi QAbstractSpinBox::down-button:pressed { background: @border; }
QDialog#roUi QAbstractSpinBox::up-arrow { image: url("@ICONS/step_plus_muted.svg"); width: 12px; height: 12px; }
QDialog#roUi QAbstractSpinBox::down-arrow { image: url("@ICONS/minus_muted.svg"); width: 12px; height: 12px; }
QDialog#roUi QAbstractSpinBox::up-arrow:hover, QDialog#roUi QAbstractSpinBox::down-arrow:hover { width: 12px; height: 12px; }
QDialog#roUi QAbstractSpinBox::up-arrow:disabled, QDialog#roUi QAbstractSpinBox::up-arrow:off { image: url("@ICONS/step_plus_faint.svg"); }
QDialog#roUi QAbstractSpinBox::down-arrow:disabled, QDialog#roUi QAbstractSpinBox::down-arrow:off { image: url("@ICONS/minus_faint.svg"); }
QDialog#roUi QComboBox { padding-right: 26px; }
QDialog#roUi QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 26px; border: 0; background: transparent; }
QDialog#roUi QComboBox::down-arrow { image: url("@ICONS/chev_muted.svg"); width: 10px; height: 10px; }
QDialog#roUi QComboBox QAbstractItemView { background: @surface; color: @text; border: 1px solid @border; border-radius: 8px; padding: 4px; outline: 0; selection-background-color: @surface3; selection-color: @text; }
QDialog#roUi QPushButton { background: @surface3; color: @text; border: 1px solid @border; border-radius: 8px; padding: 0 16px; min-height: 30px; font-weight: 600; }
QDialog#roUi QPushButton:hover { border-color: @muted; }
QDialog#roUi QPushButton:pressed { background: @surface2; }
QDialog#roUi QPushButton:disabled { background: transparent; color: @faint; }
QDialog#roUi QPushButton[primary="true"] { background: @accent; border-color: @accent; color: #ffffff; }
QDialog#roUi QPushButton[primary="true"]:hover { border-color: @text; }
QDialog#roUi QPushButton[primary="true"]:disabled { background: transparent; color: @faint; border-color: @border; }
QDialog#roUi QToolButton { background: transparent; color: @text; border: 1px solid transparent; border-radius: 8px; padding: 5px; }
QDialog#roUi QToolButton:hover { background: @surface3; }
QDialog#roUi QToolButton:disabled { color: @faint; }
QDialog#roUi QTabWidget::pane { border: 1px solid @border; border-radius: 10px; top: -1px; background: transparent; }
QDialog#roUi QTabBar { background: transparent; }
QDialog#roUi QTabBar::tab { background: transparent; color: @muted; padding: 8px 14px; margin-right: 2px; border: 0; border-bottom: 2px solid transparent; }
QDialog#roUi QTabBar::tab:hover { color: @text; }
QDialog#roUi QTabBar::tab:selected { color: @accent; border-bottom: 2px solid @accent; font-weight: 600; }
QDialog#roUi QListWidget#tabSelection { background: transparent; border: 0; outline: 0; padding: 4px 12px 4px 0px; }   /* 12px of air each side of the pill: dialog edge on the left, divider on the right */
QDialog#roUi QListWidget#tabSelection::item { color: @muted; padding: 0 10px; margin: 1px 0; border-radius: 8px; }
QDialog#roUi QListWidget#tabSelection::item:hover { background: @surface2; color: @text; }
QDialog#roUi QListWidget#tabSelection::item:selected { background: @surface3; color: @text; }
QDialog#roUi QListView, QDialog#roUi QTreeView, QDialog#roUi QTableView, QDialog#roUi QTextEdit, QDialog#roUi QTextBrowser, QDialog#roUi QPlainTextEdit { background: @surface2; color: @text; border: 1px solid @border; border-radius: 8px; selection-background-color: @surface3; selection-color: @text; outline: 0; }
QDialog#roUi QTextEdit, QDialog#roUi QTextBrowser, QDialog#roUi QPlainTextEdit { padding: 7px 10px; }
QDialog#roUi QListView, QDialog#roUi QTreeView, QDialog#roUi QTableView { alternate-background-color: @surface; }
QDialog#roUi QListView::item, QDialog#roUi QTreeView::item { padding: 4px 6px; background: transparent; }
QDialog#roUi QListView::item:selected, QDialog#roUi QTreeView::item:selected, QDialog#roUi QTableView::item:selected { background: @surface3; color: @text; }
QDialog#roUi QHeaderView { background: transparent; border: 0; }
QDialog#roUi QHeaderView::section { background: @surface2; color: @muted; padding: 7px 10px; border: 0; border-bottom: 1px solid @border; font-size: 10.5px; font-weight: 600; text-align: left; }
QDialog#roUi QHeaderView::section:hover { color: @text; }
QDialog#roUi QProgressBar { background: @surface2; border: 1px solid @border; border-radius: 9px; min-height: 18px; max-height: 18px; text-align: center; color: @muted; font-size: 11px; font-weight: 600; }
QDialog#roUi QProgressBar::chunk { background: @accent; border-radius: 8px; margin: 1px; }
QDialog#roUi QSlider::groove:horizontal { height: 4px; background: @surface3; border-radius: 2px; }
QDialog#roUi QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; background: @accent; }
QDialog#roUi QSplitter::handle { background: @border; }
QDialog#roUi QFrame[frameShape="4"], QDialog#roUi QFrame[frameShape="5"] { color: @border; background: @border; border: 0; max-height: 1px; }
QDialog#roUi QFrame[frameShape="5"] { max-height: 16777215px; max-width: 1px; }
QDialog#roUi QDialogButtonBox { background: transparent; }
/* drop zone (Torrent Creator) */
QDialog#roUi QFrame#dropZone { background: transparent; border: 2px dashed @border; border-radius: 12px; }
QDialog#roUi QFrame#dropZone[drag="true"] { border-color: @accent; background: @accent_a; }
QDialog#roUi QLabel#dropTitle { color: @text; font-size: 13px; font-weight: 600; }
QDialog#roUi QLabel#dropSub { color: @faint; font-size: 11.5px; }
/* Options > Advanced (rebuilt as two cards of rows) */
QDialog#roUi QScrollArea#advScroll, QDialog#roUi QWidget#advHost { background: transparent; border: 0; }
QDialog#roUi QLabel#advHeading { color: @muted; font-size: 10.5px; font-weight: 600; }
QDialog#roUi QFrame#advCard { background: transparent; border: 1px solid @border; border-radius: 10px; }
QDialog#roUi QWidget#advRow { background: transparent; }
QDialog#roUi QFrame#advSep { background: @border; border: 0; }

/* ================= TOOLBAR ================= */
QFrame#mainToolbar { background: @surface; border: 0; border-bottom: 1px solid @border; }
QWidget#toolRow { background: transparent; }
QFrame#tbSep { background: @border; border: 0; min-width: 1px; max-width: 1px; }
QToolButton#tbIcon { background: transparent; border: 0; border-radius: 8px; padding: 0; }
QToolButton#tbIcon:hover { background: @surface3; }
QToolButton#tbIcon[danger="true"]:hover { background: @danger; }   /* delete: deep crimson, clearly darker than the bright accent red of Add torrent / magnet link */

/* window controls: Windows-style — flat, full-height, edge-to-edge rectangles (no rounded chip) */
QToolButton#capBtn { background: transparent; border: 0; border-radius: 0; padding: 0; margin: 0; }
QToolButton#capBtn:hover { background: @surface3; }
QToolButton#capBtn:pressed { background: @border; }
QToolButton#capBtn[danger="true"]:hover, QToolButton#capBtn[danger="true"]:pressed { background: @accent; }

QLineEdit#searchBox { background: @surface2; color: @text; border: 1px solid @border; border-radius: 8px; padding: 0 12px 0 32px; font-size: 13px; selection-background-color: @surface3; selection-color: @text; placeholder-text-color: @faint; }
QLineEdit#searchBox:focus { border-color: @focus; }

QPushButton#addBtn, QPushButton#linkBtn { background: transparent; color: @accent; border: 1.5px solid @accent; border-radius: 8px; padding: 0 12px; font-size: 12.5px; font-weight: 600; }
QPushButton#linkBtn { padding: 0 9px; }
QPushButton#addBtn:hover, QPushButton#linkBtn:hover { background: @accent; color: #ffffff; }
QPushButton#addBtn:pressed, QPushButton#linkBtn:pressed { background: @accent; color: #ffffff; padding-top: 1px; }

QCheckBox#themeToggle, QCheckBox#vibrantToggle { spacing: 0; background: transparent; }
QCheckBox#themeToggle::indicator, QCheckBox#vibrantToggle::indicator { width: 40px; height: 20px; }
QCheckBox#themeToggle::indicator:unchecked   { image: url("@ICONS/toggle_theme_dark.svg"); }
QCheckBox#themeToggle::indicator:checked     { image: url("@ICONS/toggle_theme_light.svg"); }
QCheckBox#vibrantToggle::indicator:unchecked { image: url("@ICONS/toggle_vibrant_off.svg"); }
QCheckBox#vibrantToggle::indicator:checked   { image: url("@ICONS/toggle_vibrant_on.svg"); }

/* ================= SIDEBAR ================= */
QFrame#sidebar { background: @surface; border: 0; border-right: 1px solid @border; }
QTreeWidget#sideTree { background: transparent; border: 0; padding: 14px 0px 14px 10px; show-decoration-selected: 0; }
QTreeWidget#sideTree::item { background: transparent; }   /* rows are painted by SideDelegate */
QTreeWidget#sideTree::branch { background: transparent; image: none; border-image: none; }
QFrame#graphBox { background: @surface2; border: 0; border-top: 1px solid @border; border-top-left-radius: 12px; border-top-right-radius: 12px; }
QFrame#graphBox QLabel { font-size: 11px; }
QLabel#graphKey { color: @muted; }
QLabel#graphVal { color: @text; }
QSplitter::handle { background: @border; }
QSplitter::handle:horizontal { width: 1px; }
QSplitter::handle:vertical { height: 1px; }
QSplitter::handle:hover { background: @accent; }

/* ================= TORRENT TABLE ================= */
QTableView#torrentTable { background: @bg; color: @text; border: 0; gridline-color: transparent; selection-background-color: transparent; selection-color: @text; }
QTableView#torrentTable QTableCornerButton::section { background: @bg; border: 0; }
QTableView#torrentTable QHeaderView { background: @bg; border: 0; }
QTableView#torrentTable QHeaderView::section { background: @bg; color: @muted; border: 0; border-bottom: 1px solid @border; padding: 12px 16px 9px 16px; font-size: 10.5px; font-weight: 600; text-align: left; }
QTableView#torrentTable QHeaderView::section:hover { color: @text; }
QTableView#torrentTable QHeaderView::down-arrow, QTableView#torrentTable QHeaderView::up-arrow { width: 9px; height: 9px; }
QTableView#torrentTable QHeaderView::down-arrow { image: url("@ICONS/chev_muted.svg"); }
QTableView#torrentTable QHeaderView::up-arrow   { image: url("@ICONS/chev_up_muted.svg"); }

/* ================= DETAILS PANEL ================= */
QFrame#detailPanel { background: @surface; border: 0; border-top: 1px solid @border; }
QFrame#detailTabsRow { background: transparent; border: 0; border-bottom: 1px solid @border; }
QPushButton#detailTab { background: transparent; color: @muted; padding: 9px 13px; margin: 0; border: 0; border-bottom: 2px solid transparent; border-radius: 0; font-size: 12px; text-align: left; }
QPushButton#detailTab:hover { color: @accent; }
QPushButton#detailTab[active="true"] { color: @accent; border-bottom: 2px solid @accent; font-weight: 600; }
QToolButton#detailToggle { background: transparent; border: 0; border-radius: 7px; padding: 8px; }
QToolButton#detailToggle:hover { background: @surface2; }
QScrollArea#detailScroll { background: transparent; border: 0; }
QScrollArea#detailScroll > QWidget > QWidget { background: transparent; }
QLabel#dName  { font-size: 13px; font-weight: 600; }
QLabel#dLabel { font-size: 10px; color: @faint; font-weight: 400; }
QLabel#dValue { font-size: 13.5px; color: @text; }
QLabel#dEmpty { font-size: 12.5px; color: @faint; }
QLabel#dBadge { font-size: 11px; font-weight: 600; padding: 2px 9px; border-radius: 99px; }

/* status badges (QLabel[status="..."]) — same tints as .badge.* */
QLabel[status="dl"] { background: @seed_a;   color: @seed; }
QLabel[status="sd"] { background: @info_a;   color: @info; }
QLabel[status="pa"] { background: @surface3; color: @muted; }
QLabel[status="qu"] { background: @queued_a; color: @queued; }
QLabel[status="ck"] { background: @check_a;  color: @check; }
QLabel[status="st"] { background: @stall_a;  color: @stall; }
QLabel[status="er"], QLabel[status="mf"] { background: @accent_a; color: @accent; }
QLabel[status="fn"] { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 @seed_65, stop:1 @seed_35); color: #ffffff; }

/* ================= STATUS BAR ================= */
QFrame#statusBar { background: @surface; color: @muted; border: 0; border-top: 1px solid @border; font-size: 11.5px; }
QFrame#statusBar QLabel { color: @muted; font-size: 11.5px; background: transparent; }
QFrame#statusSep { background: @border; max-width: 1px; min-width: 1px; max-height: 12px; min-height: 12px; }

/* ================= SCROLLBARS ================= */
QScrollBar:vertical   { background: transparent; width: 10px;  margin: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:vertical   { background: @surface3; min-height: 24px; border-radius: 3px; margin: 0 2px; }
QScrollBar::handle:horizontal { background: @surface3; min-width: 24px;  border-radius: 3px; margin: 2px 0; }
QScrollBar::handle:hover { background: @faint; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; background: none; border: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QAbstractScrollArea::corner { background: transparent; }
"""

def mix(hexc, other, pct):
    a = [int(hexc[i:i+2],16) for i in (1,3,5)]; b = [int(other[i:i+2],16) for i in (1,3,5)]
    return "#%02x%02x%02x" % tuple(round(a[i]*pct/100 + b[i]*(100-pct)/100) for i in range(3))

def tokens(pal):
    t = {}
    for k,v in pal.items():
        t[k] = rgba(v) if k.endswith("_a") or k=="rowerr" else v
    t["seed_65"] = mix(pal["seed"], "#000000", 65); t["seed_35"] = mix(pal["seed"], "#000000", 35)
    return t

def control_svgs(pal, d):
    """checkbox / radio indicators used by the dialog forms (QSS can't draw a rounded checkbox on its own)."""
    a, m, f, t, s2, bg = pal["accent"], pal["muted"], pal["faint"], pal["text"], pal["surface2"], pal["bg"]
    hd = 'xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16"'
    box = lambda fill, stroke, inner="": f'<svg {hd}><rect x="0.75" y="0.75" width="14.5" height="14.5" rx="4.25" fill="{fill}" stroke="{stroke}" stroke-width="1.5"/>{inner}</svg>'
    ck  = '<path d="M4.6 8.3l2.4 2.4 4.4-5.1" fill="none" stroke="#ffffff" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>'
    dash = '<path d="M4.8 8h6.4" fill="none" stroke="#ffffff" stroke-width="1.8" stroke-linecap="round"/>'
    rad = lambda stroke, dot=None: f'<svg {hd}><circle cx="8" cy="8" r="7.25" fill="{s2}" stroke="{stroke}" stroke-width="1.5"/>' + (f'<circle cx="8" cy="8" r="3.5" fill="{dot}"/>' if dot else '') + '</svg>'
    dim = mix(a, bg, 38)
    files = {"cb_off": box(s2, m), "cb_off_hover": box(s2, t), "cb_on": box(a, a, ck), "cb_on_hover": box(mix(a, "#ffffff", 86), a, ck), "cb_part": box(a, a, dash),
             "cb_dis_off": box("none", f), "cb_dis_on": box(dim, dim, ck.replace('stroke="#ffffff"', 'stroke="#ffffff" opacity=".6"')),
             "rb_off": rad(m), "rb_off_hover": rad(t), "rb_on": rad(a, a), "rb_on_hover": rad(mix(a, "#ffffff", 86), mix(a, "#ffffff", 86)), "rb_dis_off": rad(f), "rb_dis_on": rad(dim, dim)}
    for n, v in files.items(): open(f"{d}/{n}.svg", "w").write(v)

def build():
    os.makedirs(f"{HERE}/theme", exist_ok=True)
    os.makedirs(f"{HERE}/icons/src", exist_ok=True)
    for n in ICONS: open(f"{HERE}/icons/src/{n}.svg","w").write(svg(n))          # currentColor originals
    for th, pal in P.items():
        d = f"{HERE}/icons/{th}"; os.makedirs(d, exist_ok=True)
        # pre-coloured copies used directly by QSS url(...)
        open(f"{d}/ck_accent.svg","w").write(svg("ck", pal["accent"]))
        open(f"{d}/chev_faint.svg","w").write(svg("chev", pal["faint"]))
        for n_ in ("minus", "step_plus"): open(f"{d}/{n_}_faint.svg","w").write(svg(n_, pal["faint"]))
        open(f"{d}/chev_up_faint.svg","w").write(svg("chev", pal["faint"]).replace("<path","<path transform='rotate(180 12 12)'",1))
        open(f"{d}/chev_up_muted.svg","w").write(svg("chev", pal["muted"]).replace("<path","<path transform='rotate(180 12 12)'",1))
        open(f"{d}/chev_right.svg","w").write(svg("chev", pal["muted"]).replace("<path","<path transform='rotate(-90 12 12)'",1))
        for n in ICONS:                       # muted set for quick use
            open(f"{d}/{n}_muted.svg","w").write(svg(n, pal["muted"]))
        for kind, states in (("theme",(("dark",False),("light",True))),("vibrant",(("off",False),("on",True)))):
            for nm,on in states: open(f"{d}/toggle_{kind}_{nm}.svg","w").write(toggle_svg(pal, kind, on))
        control_svgs(pal, d)
        t = tokens(pal)
        q = QSS.replace("@THEME", th)
        for k in sorted(t, key=len, reverse=True): q = q.replace("@"+k, t[k])
        assert "@" not in q.replace("@ICONS","").replace("@THEME",""), re.findall(r"@\w+", q)
        open(f"{HERE}/theme/{th}.qss","w").write(q)
    # palette for delegates
    def c(h): return "0x%s" % h[1:].upper()
    def qc(t): return "QColor(%d,%d,%d,%d)" % (t[0],t[1],t[2],round(t[3]*255))
    hdr = ["// GENERATED by build_theme.py — colours copied 1:1 from ro-torrent-merged.html","#pragma once","#include <QColor>","",
           "struct RoPalette {","  QColor bg,surface,surface2,surface3,border,text,muted,faint,accent,seed,info,queued,check,stall,focus,selOutline;",
           "  QColor accentA,seedA,infoA,queuedA,checkA,stallA,rowError;","};","",
           "namespace RoPal {"]
    for th, pal in P.items():
        v = dict(pal); vv = dict(pal); vv.update(VIBRANT_LIGHT) if th=="light" else None
        f = lambda k: 'QColor("%s")' % v[k]
        hdr.append(f"inline const RoPalette {th.upper()} = {{ " + ", ".join(f(k) for k in
           ["bg","surface","surface2","surface3","border","text","muted","faint","accent","seed","info","queued","check","stall","focus"]) +
           f', QColor("{v["sel_outline"]}"), ' + ", ".join(qc(v[k]) for k in ["accent_a","seed_a","info_a","queued_a","check_a","stall_a","rowerr"]) + " };")
        if th=="light":
            hdr.append("// light theme + vibrant ON uses stronger washes:")
            hdr.append("inline const RoPalette LIGHT_VIBRANT = { " + ", ".join(f(k) for k in
               ["bg","surface","surface2","surface3","border","text","muted","faint","accent","seed","info","queued","check","stall","focus"]) +
               f', QColor("{v["sel_outline"]}"), ' + ", ".join(qc(VIBRANT_LIGHT[k]) for k in ["accent_a","seed_a","info_a","queued_a","check_a","stall_a","rowerr"]) + " };")
    hdr.append("}")
    open(f"{ROOT}/src/gui/theme/Palette.h","w").write("\n".join(hdr)+"\n")
    json.dump(P, open(f"{HERE}/theme/tokens.json","w"), indent=1)
    # logos (optional: only re-extracted when the source html is available; otherwise the logos already in assets/ are kept)
    if os.path.isfile(SRC):
        s = open(SRC, encoding="utf-8").read()
        os.makedirs(f"{HERE}/assets", exist_ok=True)
        for name,fn in (("LIGHT_LOGO","logo_light.png"),("DARK_LOGO","logo_dark.png")):
            m = re.search(name+r' = "data:image/png;base64,([A-Za-z0-9+/=]+)"', s)
            if m: open(f"{HERE}/assets/{fn}","wb").write(base64.b64decode(m.group(1)))
    else:
        print(f"note: {SRC} not found - kept existing logos in assets/ (set RO_HTML to re-extract them)")
    print("ok")

if __name__ == "__main__":
    build()

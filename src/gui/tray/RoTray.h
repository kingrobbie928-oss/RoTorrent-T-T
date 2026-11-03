// SPDX-License-Identifier: GPL-2.0-or-later
// Notification-area icon, its menu and the toast stack (handoff 8.11). Behaviour follows qBittorrent's defaults:
//   * one icon for the whole app; left-click toggles show / hide of the main window;
//   * right-click: Show / Hide RoTorrent, Pause / Resume Session, Exit RoTorrent;
//   * close and minimize to the tray are decided by MainWindow, which asks closeToTray() / minimizeToTray();
//   * notifications are themed toasts (Toast), plus the native balloon when native toasts are switched on.
// Where the OS has no notification area every method is a harmless no-op and the window closes normally.
// Licensed GPL-2.0-or-later.
#pragma once

#include <optional>

#include <QList>
#include <QObject>
#include <QPointer>

#include "Toast.h"

class MainWindow;
class QAction;
class QMenu;
class QSystemTrayIcon;

class RoTray final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RoTray)

public:
    // `available` overrides the OS answer (unit tests run on a platform without a notification area).
    explicit RoTray(MainWindow *window, std::optional<bool> available = std::nullopt);
    ~RoTray() override;

    // The OS offers a notification area.
    bool available() const { return m_available; }
    // Our icon is on screen: available and Options > Behavior > "Show RoTorrent in notification area".
    bool visible() const;
    // Re-reads the Options > Behavior settings (called after Apply / OK).
    void applySettings();

    // Effective behaviours: the setting AND a visible icon.
    bool closeToTray() const;
    bool minimizeToTray() const;

    // Shows a toast. Needs the notification-area setting on and, for Options > Advanced > "Display notifications"
    // off, shows nothing; `added` marks the "torrent added" kind, which has its own switch.
    bool notify(const QString &title, const QString &text, NoticeKind kind = NoticeKind::Info, bool added = false);

    void showWindow();
    void hideWindow();
    void toggleWindow();
    bool windowShown() const;

    // Native balloon in addition to the toast (needs an AppUserModelID shortcut, i.e. the installed build):
    // the environment flag ROTORRENT_NATIVE_TOASTS=1 or the setting Gui/Notifications/Native.
    static bool nativeToastsEnabled();

    QSystemTrayIcon *icon() const { return m_icon; }
    QMenu *menu() const { return m_menu; }
    QList<Toast *> toasts() const;
    // Every notification asked for, newest last (title, text, kind) - handy for tests and the Log.
    struct Notice
    {
        QString title;
        QString text;
        NoticeKind kind;
    };
    const QList<Notice> &history() const { return m_history; }

    // The icon drawn for the tray: the RO mark on a rounded dark tile, several sizes so Windows picks a crisp one.
    static QIcon makeIcon();

private:
    void refreshMenu();
    void onActivated(int reason);
    void layoutToasts();
    void toastGone(Toast *toast);

    MainWindow *m_window;
    QSystemTrayIcon *m_icon;
    QMenu *m_menu;
    QAction *m_toggleAction;
    QAction *m_sessionAction;
    QAction *m_exitAction;
    QList<QPointer<Toast>> m_toasts;
    QList<Notice> m_history;
    bool m_available;
};

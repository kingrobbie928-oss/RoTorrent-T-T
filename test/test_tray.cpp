// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 8: tray icon, close / minimize to tray, toasts, startup window state, On Downloads Done <-> Preferences
// (offscreen, no engine). Handoff 8.11 and 8.4.
#include <cstdio>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QTemporaryDir>
#include <QTimer>

#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "gui/GuiSettings.h"
#include "gui/MainWindow.h"
#include "gui/theme/RoTheme.h"
#include "gui/tray/RoTray.h"
#include "gui/tray/Toast.h"

namespace
{
    int failures = 0;

    void check(const bool ok, const QString &what)
    {
        std::printf("%s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
        if (!ok)
            ++failures;
    }

    void settle()
    {
        for (int i = 0; i < 5; ++i)
            QApplication::processEvents();
    }

    bool closeAccepted(MainWindow &w)
    {
        QCloseEvent e;
        QApplication::sendEvent(&w, &e);
        return e.isAccepted();
    }
}

int main(int argc, char **argv)
{
    RoTheme::configureHighDpi();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RoTorrent"));
    QApplication::setQuitOnLastWindowClosed(false);
    QTemporaryDir dir;
    Profile::initInstance(Path(dir.path()), QString(), false);
    SettingsStorage::initInstance();
    Preferences::initInstance();
    RoTheme::instance().init();
    Preferences *pref = Preferences::instance();

    MainWindow window;
    auto *tray = new RoTray(&window, true);   // pretend the OS has a notification area
    window.setTray(tray);
    window.show();
    settle();

    // ---- icon and menu ----
    pref->setSystemTrayEnabled(true);
    tray->applySettings();
    check(tray->available() && tray->visible(), QStringLiteral("tray icon visible when the setting is on"));
    check(!tray->icon()->icon().isNull() && tray->icon()->icon().availableSizes().size() >= 4, QStringLiteral("tray icon rendered at several sizes"));
    emit tray->menu()->aboutToShow();   // what a right-click does
    const QList<QAction *> acts = tray->menu()->actions();
    QStringList texts;
    for (const QAction *a : acts)
    {
        if (!a->isSeparator())
            texts << a->text();
    }
    check(texts == QStringList({QStringLiteral("Hide RoTorrent"), QStringLiteral("Pause Session"), QStringLiteral("Exit RoTorrent")})
          , QStringLiteral("menu: Hide RoTorrent / Pause Session / Exit RoTorrent, got: ") + texts.join(u'|'));
    check(!tray->menu()->actions().at(2)->isEnabled(), QStringLiteral("Pause Session is disabled without a controller"));

    // ---- left click toggles the window ----
    QMetaObject::invokeMethod(tray->icon(), "activated", Q_ARG(QSystemTrayIcon::ActivationReason, QSystemTrayIcon::Trigger));
    check(!window.isVisible(), QStringLiteral("left click hides a shown window"));
    emit tray->menu()->aboutToShow();
    check(tray->menu()->actions().first()->text() == u"Show RoTorrent", QStringLiteral("menu offers Show RoTorrent while hidden"));
    QMetaObject::invokeMethod(tray->icon(), "activated", Q_ARG(QSystemTrayIcon::ActivationReason, QSystemTrayIcon::Trigger));
    check(window.isVisible(), QStringLiteral("left click shows it again"));
    QMetaObject::invokeMethod(tray->icon(), "activated", Q_ARG(QSystemTrayIcon::ActivationReason, QSystemTrayIcon::DoubleClick));
    check(window.isVisible(), QStringLiteral("a DoubleClick (which Windows sends after Trigger) does not toggle twice"));

    // ---- close to tray ----
    pref->setCloseToTray(true);
    check(window.closeGoesToTray(), QStringLiteral("closeGoesToTray with a visible icon and the setting on"));
    check(!closeAccepted(window) && !window.isVisible(), QStringLiteral("X hides to the tray instead of closing"));
    // handoff Phase 8: closing to the tray and restoring works with a theme switch in between
    const bool wasDark = RoTheme::instance().isDark();
    RoTheme::instance().setDark(!wasDark);
    RoTheme::instance().setVibrant(true);
    settle();
    emit tray->menu()->aboutToShow();
    check(tray->menu()->actions().first()->text() == u"Show RoTorrent" && tray->visible(), QStringLiteral("theme switch while in the tray: icon and menu intact"));
    tray->showWindow();
    settle();
    check(window.isVisible(), QStringLiteral("restored from the tray after a theme switch"));
    RoTheme::instance().setVibrant(false);
    RoTheme::instance().setDark(wasDark);
    settle();

    pref->setCloseToTray(false);
    check(!window.closeGoesToTray(), QStringLiteral("close-to-tray off: the window really closes"));
    pref->setCloseToTray(true);
    pref->setSystemTrayEnabled(false);
    tray->applySettings();
    check(!tray->visible() && !window.closeGoesToTray(), QStringLiteral("no icon: closing never hides to a tray that is not there"));
    pref->setSystemTrayEnabled(true);
    tray->applySettings();

    // ---- minimize to tray ----
    pref->setMinimizeToTray(false);
    window.setWindowState(Qt::WindowMinimized);
    settle();
    check(window.isVisible() || window.isMinimized(), QStringLiteral("minimize with the option off stays on the taskbar"));
    window.setWindowState(Qt::WindowNoState);
    window.show();
    settle();
    pref->setMinimizeToTray(true);
    window.setWindowState(Qt::WindowMinimized);
    settle();
    check(window.isHidden(), QStringLiteral("minimize with 'Minimize to notification area' hides the window"));
    tray->showWindow();
    settle();
    check(window.isVisible() && !window.isMinimized(), QStringLiteral("restoring clears the minimized state"));
    pref->setMinimizeToTray(false);

    // ---- startup window state ----
    window.hide();
    window.showForStartup(GuiSettings::WindowState::Hidden);
    settle();
    check(window.isHidden(), QStringLiteral("start 'Hidden' with a tray icon keeps the window in the tray"));
    pref->setSystemTrayEnabled(false);
    tray->applySettings();
    window.showForStartup(GuiSettings::WindowState::Hidden);
    settle();
    check(!window.isHidden(), QStringLiteral("start 'Hidden' without a tray icon falls back to minimized (never unreachable)"));
    window.hide();
    window.setWindowState(Qt::WindowNoState);
    pref->setSystemTrayEnabled(true);
    tray->applySettings();
    window.showForStartup(GuiSettings::WindowState::Normal);
    settle();
    check(window.isVisible() && !window.isMinimized(), QStringLiteral("start 'Normal' shows the window"));

    // ---- toasts ----
    GuiSettings::setNotificationsEnabled(true);
    GuiSettings::setTorrentAddedNotificationsEnabled(false);
    check(tray->notify(QStringLiteral("Download completed"), QStringLiteral("'a.iso' has finished downloading."), NoticeKind::Ok)
          , QStringLiteral("a completion toast is shown"));
    check(tray->toasts().size() == 1 && tray->toasts().first()->title() == u"Download completed", QStringLiteral("one toast on screen"));
    check(!tray->notify(QStringLiteral("Torrent added"), QStringLiteral("x"), NoticeKind::Info, true), QStringLiteral("'torrent added' toasts have their own switch (off by default)"));
    GuiSettings::setTorrentAddedNotificationsEnabled(true);
    check(tray->notify(QStringLiteral("Torrent added"), QStringLiteral("'b.iso' was added."), NoticeKind::Info, true), QStringLiteral("...and show once it is on"));
    check(tray->toasts().size() == 2, QStringLiteral("two toasts stack"));
    check(tray->toasts().at(1)->y() > tray->toasts().at(0)->y(), QStringLiteral("the newest toast sits at the bottom, older ones above"));
    check(tray->toasts().at(0)->width() == Toast::WIDTH, QStringLiteral("toast width is %1").arg(Toast::WIDTH));

    GuiSettings::setNotificationsEnabled(false);
    check(!tray->notify(QStringLiteral("I/O Error"), QStringLiteral("x"), NoticeKind::Crit), QStringLiteral("Display notifications off: nothing shown"));
    GuiSettings::setNotificationsEnabled(true);
    pref->setSystemTrayEnabled(false);
    check(!tray->notify(QStringLiteral("I/O Error"), QStringLiteral("x"), NoticeKind::Crit), QStringLiteral("notification area off: nothing shown (as in qBittorrent)"));
    pref->setSystemTrayEnabled(true);
    check(tray->history().size() == 2, QStringLiteral("history records only what was shown"));

    // dismiss: leaves the stack at once, others close up
    Toast *first = tray->toasts().first();
    const int yBefore = tray->toasts().at(1)->y();
    first->dismiss();
    check(tray->toasts().size() == 1 && first->closing(), QStringLiteral("dismiss removes it from the stack"));
    check(tray->toasts().first()->y() >= yBefore, QStringLiteral("the remaining toast drops to the bottom slot"));

    // click on a toast opens the app
    window.hide();
    tray->toasts().first()->clicked();
    settle();
    check(window.isVisible(), QStringLiteral("a toast click shows the main window"));
    for (Toast *t : tray->toasts())
        t->dismiss();

    // ---- On Downloads Done <-> Preferences ----
    window.action(QStringLiteral("auto_hibernate"))->setChecked(true);
    check(pref->hibernateWhenDownloadsComplete() && !pref->suspendWhenDownloadsComplete(), QStringLiteral("menu choice reaches Preferences"));
    pref->setHibernateWhenDownloadsComplete(false);
    window.syncAutoAction();
    check(window.action(QStringLiteral("auto_nothing"))->isChecked(), QStringLiteral("menu follows Preferences back to 'Do nothing'"));

    std::printf(failures ? "\n%d FAILED\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}

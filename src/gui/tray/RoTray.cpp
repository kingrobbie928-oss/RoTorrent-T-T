// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "RoTray.h"

#include <QAction>
#include <QGuiApplication>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QWindow>

#include "actions/TorrentController.h"
#include "base/preferences.h"
#include "base/settingsstorage.h"
#include "GuiSettings.h"
#include "MainWindow.h"
#include "theme/RoTheme.h"
#include "widgets/MenuIcons.h"

namespace
{
    const QString NATIVE_TOASTS_KEY = QStringLiteral("Gui/Notifications/Native");
}

QIcon RoTray::makeIcon()
{
    const QPixmap logo(QStringLiteral(":/ro/assets/logo_dark.png"));
    QIcon icon;
    for (const int px : {16, 20, 24, 32, 40, 48, 64, 128})
    {
        QPixmap pm(px, px);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        QPainterPath path;
        const qreal r = px * 0.22;
        path.addRoundedRect(QRectF(0, 0, px, px), r, r);
        p.fillPath(path, QColor(0x17, 0x17, 0x1b));
        if (!logo.isNull())
        {
            const qreal w = px * 0.70;
            const qreal h = w * logo.height() / logo.width();
            p.drawPixmap(QRectF((px - w) / 2, (px - h) / 2, w, h), logo, QRectF(logo.rect()));
        }
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

RoTray::RoTray(MainWindow *window, const std::optional<bool> available)
    : QObject(window)
    , m_window(window)
    , m_icon(new QSystemTrayIcon(makeIcon(), this))
    , m_menu(new QMenu)   // parentless popup: it picks up the application stylesheet like every other menu
    , m_toggleAction(new QAction(m_menu))
    , m_sessionAction(new QAction(m_menu))
    , m_exitAction(new QAction(tr("Exit RoTorrent"), m_menu))
    , m_available(available.value_or(QSystemTrayIcon::isSystemTrayAvailable()))
{
    m_icon->setToolTip(QStringLiteral("RoTorrent"));
    m_menu->setObjectName(QStringLiteral("trayMenu"));
    m_menu->addAction(m_toggleAction);
    m_menu->addSeparator();
    m_menu->addAction(m_sessionAction);
    m_menu->addSeparator();
    m_menu->addAction(m_exitAction);
    connect(m_menu, &QMenu::aboutToShow, this, &RoTray::refreshMenu);
    connect(m_toggleAction, &QAction::triggered, this, &RoTray::toggleWindow);
    connect(m_sessionAction, &QAction::triggered, this, [this] { m_window->toggleSession(); });
    connect(m_exitAction, &QAction::triggered, this, [this] { m_window->requestQuit(); });
    m_icon->setContextMenu(m_menu);
    // Left click only: Windows also sends DoubleClick after Trigger, which would toggle the window twice.
    connect(m_icon, &QSystemTrayIcon::activated, this, [this](const QSystemTrayIcon::ActivationReason reason)
    {
        onActivated(static_cast<int>(reason));
    });
    connect(m_icon, &QSystemTrayIcon::messageClicked, this, &RoTray::showWindow);
    connect(&RoTheme::instance(), &RoTheme::changed, this, &RoTray::refreshMenu);

    refreshMenu();
    applySettings();
}

RoTray::~RoTray()
{
    m_icon->hide();
    for (const QPointer<Toast> &t : std::as_const(m_toasts))
        delete t.data();
    delete m_menu;
}

bool RoTray::visible() const
{
    return m_available && Preferences::instance()->systemTrayEnabled();
}

void RoTray::applySettings()
{
    const bool want = visible();
    if (want != m_icon->isVisible())
        m_icon->setVisible(want);
}

bool RoTray::closeToTray() const
{
    return visible() && Preferences::instance()->closeToTray();
}

bool RoTray::minimizeToTray() const
{
    return visible() && Preferences::instance()->minimizeToTray();
}

bool RoTray::windowShown() const
{
    return m_window->isVisible() && !m_window->isMinimized();
}

void RoTray::showWindow()
{
    m_window->activate();
}

void RoTray::hideWindow()
{
    m_window->hide();
}

void RoTray::toggleWindow()
{
    if (windowShown())
        hideWindow();
    else
        showWindow();
}

void RoTray::onActivated(const int reason)
{
    if (static_cast<QSystemTrayIcon::ActivationReason>(reason) == QSystemTrayIcon::Trigger)
        toggleWindow();
}

void RoTray::refreshMenu()
{
    m_toggleAction->setText(windowShown() ? tr("Hide RoTorrent") : tr("Show RoTorrent"));
    m_toggleAction->setIcon(RoMenu::icon(QStringLiteral("m-eye")));
    const bool paused = m_window->controller() && m_window->controller()->sessionPaused();
    m_sessionAction->setText(paused ? tr("Resume Session") : tr("Pause Session"));
    m_sessionAction->setIcon(RoMenu::icon(paused ? QStringLiteral("m-play-circle") : QStringLiteral("m-pause-circle")));
    m_sessionAction->setEnabled(m_window->controller() != nullptr);
    m_exitAction->setIcon(RoMenu::icon(QStringLiteral("m-exit")));
}

bool RoTray::nativeToastsEnabled()
{
    return (qEnvironmentVariable("ROTORRENT_NATIVE_TOASTS") == QLatin1String("1"))
            || SettingsStorage::instance()->loadValue<bool>(NATIVE_TOASTS_KEY, false);
}

bool RoTray::notify(const QString &title, const QString &text, const NoticeKind kind, const bool added)
{
    // Like qBittorrent, notifications belong to the notification-area setting and to Advanced > "Display notifications".
    if (!Preferences::instance()->systemTrayEnabled())
        return false;
    if (!GuiSettings::notificationsEnabled() || (added && !GuiSettings::torrentAddedNotificationsEnabled()))
        return false;

    m_history.append({title, text, kind});
    auto *toast = new Toast(title, text, kind);
    connect(toast, &Toast::clicked, this, &RoTray::showWindow);
    connect(toast, &Toast::dismissed, this, &RoTray::toastGone);
    m_toasts.append(toast);
    layoutToasts();
    toast->popup();

    if (m_icon->isVisible() && nativeToastsEnabled())
    {
        const auto icon = (kind == NoticeKind::Crit) ? QSystemTrayIcon::Critical
                : (kind == NoticeKind::Warn) ? QSystemTrayIcon::Warning : QSystemTrayIcon::Information;
        m_icon->showMessage(title, text, icon, 5000);
    }
    return true;
}

QList<Toast *> RoTray::toasts() const
{
    QList<Toast *> list;
    for (const QPointer<Toast> &t : m_toasts)
    {
        if (t)
            list.append(t.data());
    }
    return list;
}

void RoTray::toastGone(Toast *toast)
{
    m_toasts.removeAll(QPointer<Toast>(toast));
    layoutToasts();
}

void RoTray::layoutToasts()
{
    const QScreen *screen = (m_window->isVisible() ? m_window->screen() : nullptr);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect r = screen->availableGeometry();
    int y = r.bottom() - Toast::MARGIN + 1 - 8;
    for (auto it = m_toasts.crbegin(); it != m_toasts.crend(); ++it)   // newest at the bottom, older ones stack upward
    {
        Toast *t = it->data();
        if (!t)
            continue;
        t->move(r.right() - t->width() + Toast::MARGIN - 8 + 1, y - t->height() + Toast::MARGIN);
        y -= t->height() - 2 * Toast::MARGIN + Toast::GAP;
    }
}

// SPDX-License-Identifier: GPL-2.0-or-later
// RoTorrent main window. Phase 4: toolbar, menus, shortcuts, torrent and sidebar context menus and
// the Remove / Rename / Category dialogs, all driven by TorrentController. Later phases add the
// details panel (5), the remaining dialogs (6), options (7), status bar / tray (8).
#pragma once

#include <functional>

#include <QStringList>
#include <QTimer>

#include "chrome/RoFramelessWindow.h"
#include "GuiSettings.h"
#include "models/TorrentFilterProxy.h"

class DetailsPanel;
class DetailsSource;
class LogBuffer;
class MainToolbar;
class NetBanner;
class QAction;
class QActionGroup;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QMimeData;
class QMenu;
class RoSearchBox;
class RoStatusBar;
class RoTray;
class Sidebar;
class TorrentController;
class TransferListModel;
class TransferListView;
class QVBoxLayout;

class MainWindow final : public RoFramelessWindow
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainWindow)

public:
    // Dialogs and tabs that arrive in later phases plug in here. Until then each hook shows a
    // styled "not available yet" box, so every menu item of 8.4 / 8.8 is reachable today.
    struct Hooks
    {
        std::function<void()> addTorrentFile;
        std::function<void()> addTorrentLink;
        // Drag and drop of .torrent files, magnet links and web addresses (set by AddTorrentFlow::install).
        std::function<void(const QStringList &)> addSources;
        std::function<void()> createTorrent;
        std::function<void()> statistics;
        std::function<void()> options;
        // Opens Options on a page (MainWindow::OPTIONS_PAGE_*); the status bar's connection chip uses it.
        std::function<void(int)> optionsPage;
        std::function<void()> cookies;
        std::function<void()> about;
        std::function<void()> speedLimits;
        std::function<void(const QStringList &)> manageContent;
        std::function<void(const QStringList &)> editTrackers;
        std::function<void(const QStringList &)> torrentOptions;
        std::function<void(const QStringList &)> previewFile;
    };

    // Options pages the window opens directly; they equal OptionsDialog::Tabs (checked in OptionsLauncher.cpp).
    static constexpr int OPTIONS_PAGE_CONNECTION = 2;

    explicit MainWindow(QWidget *parent = nullptr);

    // Brings the window to the front, restoring it if it is minimized (second instance, notifications).
    void activate();

    // The torrent sources a drag carries: local .torrent files, magnet links and http(s) / ftp addresses,
    // in drop order and without duplicates. Folders and other files are ignored.
    static QStringList sourcesFromMimeData(const QMimeData *mime);

    // The drag-and-drop decisions behind dragEnterEvent / dropEvent: a drag is wanted when the hook is set
    // and it carries a source; a drop hands the sources to the hook on the next event-loop turn.
    bool canAcceptDrag(const QMimeData *mime) const;
    bool handleDrop(const QMimeData *mime);

    // First show at start-up, honouring Options > Behavior > "RoTorrent window state on start up": Normal shows
    // the window, Minimized minimizes it (to the tray when "Minimize to notification area" is on), Hidden keeps it
    // in the tray. Without a visible tray icon a hidden start falls back to minimized.
    void showForStartup(GuiSettings::WindowState state);

    // The notification-area icon (created by EngineWindow); null in the preview and in unit tests that skip it.
    void setTray(RoTray *tray);
    RoTray *tray() const { return m_tray; }
    // Re-reads the tray settings after Apply / OK.
    void applyTraySettings();
    // Closing the window hides it (instead of quitting) while the tray icon is visible and "Close to notification
    // area" is on; likewise for minimizing.
    bool closeGoesToTray() const;
    bool minimizeGoesToTray() const;

    RoStatusBar *statusBar() const { return m_statusBar; }
    NetBanner *banner() const { return m_banner; }

    // "On Downloads Done" radio group <-> the engine's Preferences (the flags Application::allTorrentsFinished
    // reads). syncAutoAction() re-reads them; the engine switches the one-shot actions off after performing them.
    void syncAutoAction();

    TransferListModel *transferModel() const { return m_model; }
    TorrentFilterProxy *filterProxy() const { return m_proxy; }
    TransferListView *transferView() const { return m_view; }
    Sidebar *sidebar() const { return m_sidebar; }
    RoSearchBox *searchBox() const;
    MainToolbar *toolbar() const { return m_toolbar; }
    Hooks &hooks() { return m_hooks; }

    // Connects the real engine. Without a controller (demo mode) commands are disabled.
    void setController(TorrentController *controller);
    TorrentController *controller() const { return m_controller; }

    // The details panel (handoff 8.9). Created when a data source is given: SessionDetailsSource on a
    // live session, DemoDetailsSource in the preview. refreshMs is the list's refresh interval.
    void setDetailsSource(DetailsSource *source, int refreshMs = 1500);
    DetailsPanel *detailsPanel() const { return m_details; }
    LogBuffer *logBuffer() const { return m_log; }
    // Selects the torrent in the list (clearing filters is not done) and opens the named tab.
    bool selectTorrent(const QString &id);
    void showDetails(const QString &id, int tab);   // DetailsPanel::Tab

    // "#" column, toolbar queue group, Edit-menu queue items and the context-menu Queue submenu
    // exist only while Options > BitTorrent > Torrent Queueing is on.
    void setQueueingEnabled(bool enabled);
    bool queueingEnabled() const { return m_queueing; }

    // Test and menu access.
    QAction *action(const QString &key) const { return m_actions.value(key); }
    QStringList selectedIds() const;
    // The torrent context menu for `ids` (caller owns it). Public so tests can inspect it.
    QMenu *buildTorrentMenu(const QStringList &ids);
    QMenu *buildSidebarMenu(const QString &group, const QVariant &payload);
    QStringList idsMatching(const RowFilter &filter) const;

    // Commands (also reachable from menus, toolbar and shortcuts).
    // Double-click on a torrent: the action chosen in Options > Behavior (one for unfinished, one for
    // finished torrents).
    void activateTorrent(const QString &id);
    // Options > Advanced > Refresh interval, applied to the open details panel.
    void setDetailsRefreshInterval(int ms);
    void resumeSelected();
    void pauseSelected();
    void forceResumeSelected();
    void removeIds(const QStringList &ids, bool askConfirmation = true);
    void removeSelected() { removeIds(selectedIds()); }
    void moveSelectedInQueue(int where);   // TorrentController::QueueMove
    void toggleSession();
    bool confirmQuit();
    void requestQuit();
    // Help > Check for Updates (UpdateChecker; "not configured" until a feed URL is set).
    void checkForUpdates();

signals:
    void viewToggled(const QString &what, bool visible);   // "toolbar" | "statusbar" | "sidebar"

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    // Catches drags aimed at child widgets (search box, log, tables) so a drop works anywhere on the window.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildMenus();
    void buildBody(QVBoxLayout *body);
    void onModelChanged();
    void updateCommandStates();
    void updateSessionAction();
    void notYet(const QString &feature);
    void showTorrentContextMenu(const QPoint &viewportPos);
    void renameTorrent(const QString &id);
    void setLocation(const QStringList &ids);
    void askNewCategory(const QStringList &ids);
    void addCategory();
    void editCategory(const QString &name);
    void exportTorrent(const QString &id);
    void persistView(const QString &key, bool on);
    QAction *makeAction(const QString &key, const QString &text, const QString &icon, bool wide = false
                        , const QKeySequence &shortcut = {});

    MainToolbar *m_toolbar = nullptr;
    TransferListModel *m_model = nullptr;
    TorrentFilterProxy *m_proxy = nullptr;
    TransferListView *m_view = nullptr;
    Sidebar *m_sidebar = nullptr;
    TorrentController *m_controller = nullptr;
    QVBoxLayout *m_rightColumn = nullptr;
    DetailsPanel *m_details = nullptr;
    LogBuffer *m_log = nullptr;
    QHash<QString, QAction *> m_actions;
    QList<QAction *> m_queueActions;
    QAction *m_queueSeparator = nullptr;
    QActionGroup *m_autoActionGroup = nullptr;
    RoStatusBar *m_statusBar = nullptr;
    NetBanner *m_banner = nullptr;
    RoTray *m_tray = nullptr;
    Hooks m_hooks;
    bool m_queueing = false;
    bool m_quitting = false;
    bool m_syncingAutoAction = false;
    QTimer m_countsTimer;
};

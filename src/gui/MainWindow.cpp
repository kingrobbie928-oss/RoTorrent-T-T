// SPDX-License-Identifier: GPL-2.0-or-later
#include "MainWindow.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QAbstractSpinBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QShortcut>
#include <QTextEdit>
#include <QClipboard>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "actions/TorrentController.h"
#include "details/DetailsPanel.h"
#include "details/DetailsSource.h"
#include "details/LogBuffer.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/version.h"
#include "base/preferences.h"
#include "base/settingsstorage.h"
#include "dialogs/RoMessageBox.h"
#include "GuiLog.h"
#include "models/TorrentFilterProxy.h"
#include "models/TransferListModel.h"
#include "theme/RoTheme.h"
#include "tray/RoTray.h"
#include "update/UpdateChecker.h"
#include "widgets/NetBanner.h"
#include "widgets/StatusBar.h"
#include "widgets/MainToolbar.h"
#include "widgets/MenuIcons.h"
#include "widgets/RoSearchBox.h"
#include "widgets/Sidebar.h"
#include "widgets/TransferListView.h"

namespace
{
    const QString DOCS_URL = QStringLiteral("https://rotorrent.example/docs");   // placeholder (handoff section 11, item 6)

    // On Downloads Done: the engine reads these five flags in Application::allTorrentsFinished (same precedence).
    QString autoActionKey()
    {
        const Preferences *p = Preferences::instance();
        if (!p)
            return SettingsStorage::instance()->loadValue<QString>(u"Gui/AutoAction"_s, u"nothing"_s);   // preview only
        if (p->suspendWhenDownloadsComplete())
            return u"suspend"_s;
        if (p->hibernateWhenDownloadsComplete())
            return u"hibernate"_s;
        if (p->shutdownWhenDownloadsComplete())
            return u"shutdown"_s;
        if (p->rebootWhenDownloadsComplete())
            return u"reboot"_s;
        if (p->shutdownqBTWhenDownloadsComplete())
            return u"exit"_s;
        return u"nothing"_s;
    }

    void storeAutoAction(const QString &key)
    {
        Preferences *p = Preferences::instance();
        if (!p)
        {
            SettingsStorage::instance()->storeValue(u"Gui/AutoAction"_s, key);
            return;
        }
        p->setShutdownqBTWhenDownloadsComplete(key == u"exit");
        p->setSuspendWhenDownloadsComplete(key == u"suspend");
        p->setHibernateWhenDownloadsComplete(key == u"hibernate");
        p->setShutdownWhenDownloadsComplete(key == u"shutdown");
        p->setRebootWhenDownloadsComplete(key == u"reboot");
        p->apply();
    }

    bool loadFlag(const QString &key, const bool defaultValue)
    {
        return SettingsStorage::instance()->loadValue<bool>(key, defaultValue);
    }
}

MainWindow::MainWindow(QWidget *parent)
    : RoFramelessWindow(parent)
{
    setAcceptDrops(true);   // .torrent files and magnet links dropped anywhere on the window
    qApp->installEventFilter(this);   // see eventFilter(): drags over child widgets

    // Ctrl+V anywhere in the window adds the links / info-hashes / .torrent files on the clipboard.
    auto *paste = new QShortcut(QKeySequence::Paste, this);
    paste->setContext(Qt::WindowShortcut);
    connect(paste, &QShortcut::activated, this, [this]
    {
        QWidget *focus = QApplication::focusWidget();
        const auto *textEdit = qobject_cast<QTextEdit *>(focus);
        const auto *plainEdit = qobject_cast<QPlainTextEdit *>(focus);
        const bool editable = qobject_cast<QLineEdit *>(focus) || qobject_cast<QAbstractSpinBox *>(focus)
                || (textEdit && !textEdit->isReadOnly()) || (plainEdit && !plainEdit->isReadOnly());
        if (editable)
        {
            QMetaObject::invokeMethod(focus, "paste");   // ordinary text paste (search box and the like)
            return;
        }
        handleDrop(QApplication::clipboard()->mimeData());
    });
    auto *central = new QWidget;
    central->setObjectName(QStringLiteral("central"));
    auto *body = new QVBoxLayout(central);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    setCentralWidget(central);
    // The search box must not grab focus at startup (it would show its white focus ring).
    central->setFocusPolicy(Qt::ClickFocus);
    QTimer::singleShot(0, central, qOverload<>(&QWidget::setFocus));

    m_model = new TransferListModel(this);
    m_proxy = new TorrentFilterProxy(this);
    m_proxy->setSourceModel(m_model);

    m_toolbar = new MainToolbar;
    body->addWidget(m_toolbar);
    buildBody(body);
    buildMenus();

    // Hooks for dialogs that later phases provide.
    m_hooks.addTorrentFile = [this] { notYet(tr("Add Torrent File")); };
    m_hooks.addTorrentLink = [this] { notYet(tr("Add Torrent Link")); };
    m_hooks.createTorrent = [this] { notYet(tr("Torrent Creator")); };
    m_hooks.statistics = [this] { notYet(tr("Statistics")); };
    m_hooks.options = [this] { notYet(tr("Options")); };
    m_hooks.optionsPage = [this](int) { m_hooks.options(); };
    m_hooks.cookies = [this] { notYet(tr("Manage Cookies")); };
    m_hooks.about = [this] { notYet(tr("About")); };
    m_hooks.speedLimits = [this] { notYet(tr("Speed limits")); };
    m_hooks.manageContent = [this](const QStringList &) { notYet(tr("Content tab")); };
    m_hooks.editTrackers = [this](const QStringList &) { notYet(tr("Tracker list")); };
    m_hooks.torrentOptions = [this](const QStringList &) { notYet(tr("Torrent options")); };
    m_hooks.previewFile = [this](const QStringList &) { notYet(tr("Preview")); };

    connect(m_toolbar, &MainToolbar::createRequested, this, [this] { m_hooks.createTorrent(); });
    connect(m_toolbar, &MainToolbar::addTorrentRequested, this, [this] { m_hooks.addTorrentFile(); });
    connect(m_toolbar, &MainToolbar::addLinkRequested, this, [this] { m_hooks.addTorrentLink(); });
    connect(m_toolbar, &MainToolbar::settingsRequested, this, [this] { m_hooks.options(); });
    connect(m_toolbar, &MainToolbar::pauseRequested, this, &MainWindow::pauseSelected);
    connect(m_toolbar, &MainToolbar::resumeRequested, this, &MainWindow::resumeSelected);
    connect(m_toolbar, &MainToolbar::deleteRequested, this, &MainWindow::removeSelected);
    connect(m_toolbar, &MainToolbar::queueRequested, this, [this](const MainToolbar::QueueButton which)
    {
        moveSelectedInQueue(static_cast<int>(which));
    });
    connect(m_sidebar, &Sidebar::speedCardClicked, this, [this] { m_hooks.speedLimits(); });
    connect(m_statusBar, &RoStatusBar::limitsClicked, this, [this] { m_hooks.speedLimits(); });
    connect(m_statusBar, &RoStatusBar::connectionClicked, this, [this] { m_hooks.optionsPage(OPTIONS_PAGE_CONNECTION); });
    connect(m_view, &TransferListView::torrentActivated, this, &MainWindow::activateTorrent);
    connect(m_view, &TransferListView::addTorrentRequested, this, [this] { m_hooks.addTorrentFile(); });
    connect(m_view, &TransferListView::addLinkRequested, this, [this] { m_hooks.addTorrentLink(); });

    updateCommandStates();
    updateSessionAction();
}

RoSearchBox *MainWindow::searchBox() const
{
    return m_toolbar->searchBox();
}

QAction *MainWindow::makeAction(const QString &key, const QString &text, const QString &icon, const bool wide
                                , const QKeySequence &shortcut)
{
    auto *a = new QAction(text, this);
    if (!icon.isEmpty())
        RoMenu::bind(a, icon, wide);
    if (!shortcut.isEmpty())
        a->setShortcut(shortcut);
    m_actions.insert(key, a);
    return a;
}

void MainWindow::buildMenus()
{
    // No & mnemonics in menu labels (handoff 8.3).
    QMenuBar *bar = titleMenuBar();

    // ---- File ----
    QMenu *file = bar->addMenu(tr("File"));
    file->addAction(makeAction(QStringLiteral("addFile"), tr("Add Torrent File..."), QStringLiteral("m-file-plus"), false, QKeySequence(QStringLiteral("Ctrl+O"))));
    file->addAction(makeAction(QStringLiteral("addLink"), tr("Add Torrent Link..."), QStringLiteral("link"), false, QKeySequence(QStringLiteral("Ctrl+U"))));
    file->addAction(makeAction(QStringLiteral("createTorrentFile"), tr("Create Torrent..."), QStringLiteral("m-package")));
    file->addSeparator();
    file->addAction(makeAction(QStringLiteral("exit"), tr("Exit"), QStringLiteral("m-exit"), false, QKeySequence(QStringLiteral("Ctrl+Q"))));
    connect(action(QStringLiteral("addFile")), &QAction::triggered, this, [this] { m_hooks.addTorrentFile(); });
    connect(action(QStringLiteral("addLink")), &QAction::triggered, this, [this] { m_hooks.addTorrentLink(); });
    connect(action(QStringLiteral("createTorrentFile")), &QAction::triggered, this, [this] { m_hooks.createTorrent(); });
    connect(action(QStringLiteral("exit")), &QAction::triggered, this, &MainWindow::requestQuit);

    // ---- Edit ----
    QMenu *edit = bar->addMenu(tr("Edit"));
    edit->addAction(makeAction(QStringLiteral("resume"), tr("Resume"), QStringLiteral("resume"), false, QKeySequence(QStringLiteral("Ctrl+S"))));
    edit->addAction(makeAction(QStringLiteral("pause"), tr("Pause"), QStringLiteral("pause"), false, QKeySequence(QStringLiteral("Ctrl+P"))));
    edit->addSeparator();
    edit->addAction(makeAction(QStringLiteral("remove"), tr("Remove"), QStringLiteral("delete")));
    m_queueSeparator = edit->addSeparator();   // queue items only while Torrent Queueing is on
    const struct { const char *key; const char *text; const char *icon; int where; } queue[] =
    {
        {"queueTop", QT_TR_NOOP("Top of Queue"), "q-top", 0}, {"queueUp", QT_TR_NOOP("Move Up Queue"), "q-up", 1}
        , {"queueDown", QT_TR_NOOP("Move Down Queue"), "q-down", 2}, {"queueBottom", QT_TR_NOOP("Bottom of Queue"), "q-bottom", 3}
    };
    for (const auto &q : queue)
    {
        QAction *a = makeAction(QString::fromLatin1(q.key), tr(q.text), QString::fromLatin1(q.icon));
        const int where = q.where;
        connect(a, &QAction::triggered, this, [this, where] { moveSelectedInQueue(where); });
        edit->addAction(a);
        m_queueActions.append(a);
    }
    edit->addSeparator();
    edit->addAction(makeAction(QStringLiteral("session"), tr("Pause Session"), QStringLiteral("m-pause-circle")));
    connect(action(QStringLiteral("resume")), &QAction::triggered, this, &MainWindow::resumeSelected);
    connect(action(QStringLiteral("pause")), &QAction::triggered, this, &MainWindow::pauseSelected);
    connect(action(QStringLiteral("remove")), &QAction::triggered, this, &MainWindow::removeSelected);
    connect(action(QStringLiteral("session")), &QAction::triggered, this, &MainWindow::toggleSession);
    connect(edit, &QMenu::aboutToShow, this, &MainWindow::updateCommandStates);
    for (QAction *a : std::as_const(m_queueActions))
        a->setVisible(false);
    m_queueSeparator->setVisible(false);

    // ---- View (checkable items get the checkbox column) ----
    QMenu *view = bar->addMenu(tr("View"));
    view->setProperty("checks", true);
    const struct { const char *key; const char *text; const char *icon; const char *setting; const char *what; } toggles[] =
    {
        {"viewToolbar", QT_TR_NOOP("Top Toolbar"), "m-toolbar", "Gui/ShowToolbar", "toolbar"}
        , {"viewStatusBar", QT_TR_NOOP("Status Bar"), "m-statusbar", "Gui/ShowStatusBar", "statusbar"}
        , {"viewSidebar", QT_TR_NOOP("Filters Sidebar"), "m-sidebar", "Gui/ShowSidebar", "sidebar"}
    };
    for (const auto &t : toggles)
    {
        QAction *a = makeAction(QString::fromLatin1(t.key), tr(t.text), QString::fromLatin1(t.icon), true);
        a->setCheckable(true);
        a->setChecked(loadFlag(QString::fromLatin1(t.setting), true));
        RoMenu::bind(a, QString::fromLatin1(t.icon), true);   // re-render now that the checked state is known
        const QString setting = QString::fromLatin1(t.setting);
        const QString what = QString::fromLatin1(t.what);
        connect(a, &QAction::toggled, this, [this, setting, what](const bool on)
        {
            persistView(setting, on);
            if (what == u"toolbar")
                m_toolbar->setVisible(on);
            else if (what == u"sidebar")
                m_sidebar->setVisible(on);
            else if (what == u"statusbar")
                m_statusBar->setVisible(on);
            emit viewToggled(what, on);
        });
        view->addAction(a);
    }
    view->addSeparator();
    view->addAction(makeAction(QStringLiteral("stats"), tr("Statistics"), QStringLiteral("m-stats"), true, QKeySequence(QStringLiteral("Ctrl+I"))));
    connect(action(QStringLiteral("stats")), &QAction::triggered, this, [this] { m_hooks.statistics(); });
    m_toolbar->setVisible(action(QStringLiteral("viewToolbar"))->isChecked());
    m_sidebar->setVisible(action(QStringLiteral("viewSidebar"))->isChecked());
    m_statusBar->setVisible(action(QStringLiteral("viewStatusBar"))->isChecked());

    // ---- Tools ----
    QMenu *tools = bar->addMenu(tr("Tools"));
    tools->addAction(makeAction(QStringLiteral("creator"), tr("Torrent Creator"), QStringLiteral("create")));
    tools->addSeparator();
    tools->addAction(makeAction(QStringLiteral("cookies"), tr("Manage Cookies..."), QStringLiteral("m-cookie")));
    tools->addAction(makeAction(QStringLiteral("options"), tr("Options..."), QStringLiteral("settings"), false, QKeySequence(QStringLiteral("Alt+O"))));
    tools->addSeparator();
    connect(action(QStringLiteral("creator")), &QAction::triggered, this, [this] { m_hooks.createTorrent(); });
    connect(action(QStringLiteral("cookies")), &QAction::triggered, this, [this] { m_hooks.cookies(); });
    connect(action(QStringLiteral("options")), &QAction::triggered, this, [this] { m_hooks.options(); });

    auto *done = new QMenu(tr("On Downloads Done"), tools);
    done->setProperty("checks", true);
    RoMenu::bind(done->menuAction(), QStringLiteral("m-power"), false);
    tools->addMenu(done);
    m_autoActionGroup = new QActionGroup(this);
    m_autoActionGroup->setExclusive(true);
    const struct { const char *key; const char *text; const char *icon; } autoActions[] =
    {
        {"nothing", QT_TR_NOOP("Do nothing"), "m-ban"}, {"exit", QT_TR_NOOP("Exit RoTorrent"), "m-exit"}
        , {"suspend", QT_TR_NOOP("Suspend System"), "m-moon"}, {"hibernate", QT_TR_NOOP("Hibernate System"), "m-snow"}
        , {"reboot", QT_TR_NOOP("Reboot System"), "m-update"}, {"shutdown", QT_TR_NOOP("Shutdown System"), "m-power"}
    };
    for (const auto &item : autoActions)
    {
        const QString key = QString::fromLatin1(item.key);
        auto *a = new QAction(tr(item.text), this);
        a->setCheckable(true);
        a->setActionGroup(m_autoActionGroup);
        a->setData(key);
        RoMenu::bind(a, QString::fromLatin1(item.icon), true);
        connect(a, &QAction::toggled, this, [this, key](const bool on)
        {
            if (on && !m_syncingAutoAction)
                storeAutoAction(key);
        });
        done->addAction(a);
        m_actions.insert(QStringLiteral("auto_") + key, a);
    }
    syncAutoAction();
    if (Preferences *pref = Preferences::instance())
        connect(pref, &Preferences::changed, this, &MainWindow::syncAutoAction);

    // ---- Help ----
    QMenu *help = bar->addMenu(tr("Help"));
    help->addAction(makeAction(QStringLiteral("docs"), tr("Documentation"), QStringLiteral("m-book"), false, QKeySequence(Qt::Key_F1)));
    help->addAction(makeAction(QStringLiteral("updates"), tr("Check for Updates"), QStringLiteral("m-update")));
    help->addSeparator();
    help->addAction(makeAction(QStringLiteral("about"), tr("About"), QStringLiteral("info")));
    connect(action(QStringLiteral("docs")), &QAction::triggered, this, [] { QDesktopServices::openUrl(QUrl(DOCS_URL)); });
    connect(action(QStringLiteral("updates")), &QAction::triggered, this, &MainWindow::checkForUpdates);
    connect(action(QStringLiteral("about")), &QAction::triggered, this, [this] { m_hooks.about(); });
}

void MainWindow::buildBody(QVBoxLayout *body)
{
    auto *content = new QWidget;
    auto *row = new QHBoxLayout(content);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    m_sidebar = new Sidebar(m_model);
    m_view = new TransferListView(m_model, m_proxy);
    // Right column: the table with the details panel under it.
    auto *right = new QWidget;
    m_rightColumn = new QVBoxLayout(right);
    m_rightColumn->setContentsMargins(0, 0, 0, 0);
    m_rightColumn->setSpacing(0);
    m_rightColumn->addWidget(m_view, 1);
    row->addWidget(m_sidebar);
    row->addWidget(right, 1);
    body->addWidget(content, 1);
    m_statusBar = new RoStatusBar;
    body->addWidget(m_statusBar);
    // The offline banner sits above the table, inside the right column (handoff 8.10).
    m_banner = new NetBanner;
    m_rightColumn->insertWidget(0, m_banner);
    m_log = new LogBuffer(this);
    // Ignored = "may be smaller than my contents": overflow is clipped by the parent, never forces the window wider.
    for (QWidget *w : {content, right, static_cast<QWidget *>(m_view)})
        w->setSizePolicy(QSizePolicy::Ignored, w->sizePolicy().verticalPolicy());

    // Sidebar row -> filter. The search text stays part of the filter.
    connect(m_sidebar, &Sidebar::filterChanged, this, [this](const RowFilter &f, const QString &label)
    {
        RowFilter merged = f;
        merged.search = m_proxy->filter().search;
        m_proxy->setFilter(merged);
        m_view->updateEmptyState(label);
    });
    connect(m_toolbar->searchBox(), &RoSearchBox::searchChanged, this, [this](const QString &text)
    {
        m_proxy->setSearch(text);
        m_view->updateEmptyState(m_sidebar->currentLabel());
    });
    connect(m_view, &TransferListView::clearSearchRequested, m_toolbar->searchBox(), &QLineEdit::clear);
    connect(m_view, &TransferListView::showAllRequested, m_sidebar, &Sidebar::resetFilter);

    // Context menus.
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos)
    {
        // For a QAbstractScrollArea the signal's position is already in viewport coordinates.
        showTorrentContextMenu(pos);
    });
    connect(m_sidebar, &Sidebar::menuRequested, this, [this](const QPoint &global, const QString &group, const QVariant &payload)
    {
        if (QMenu *menu = buildSidebarMenu(group, payload))
            menu->popup(global);
    });
    connect(m_view, &TransferListView::selectedIdChanged, this, &MainWindow::updateCommandStates);

    // Counts, empty state and command states follow the data. Coalesced: a live session reports every second.
    m_countsTimer.setSingleShot(true);
    m_countsTimer.setInterval(120);
    connect(&m_countsTimer, &QTimer::timeout, this, &MainWindow::onModelChanged);
    connect(m_model, &TransferListModel::rowsChanged, &m_countsTimer, qOverload<>(&QTimer::start));
    onModelChanged();
}

void MainWindow::setDetailsRefreshInterval(const int ms)
{
    if (m_details)
        m_details->setRefreshInterval(ms);
}

void MainWindow::setDetailsSource(DetailsSource *source, const int refreshMs)
{
    if (m_details || !source)
        return;
    m_details = new DetailsPanel(source, m_log);
    m_details->setRefreshInterval(refreshMs);
    m_rightColumn->addWidget(m_details);
    connect(m_view, &TransferListView::selectedIdChanged, m_details, &DetailsPanel::setTorrent);
    m_details->setTorrent(m_view->selectedId());

    // Context menu "Manage content..." opens the Content tab, "Edit trackers..." the Trackers tab
    // (the full tracker-list dialog is Phase 6).
    m_hooks.manageContent = [this](const QStringList &ids)
    {
        if (!ids.isEmpty())
            showDetails(ids.first(), static_cast<int>(DetailsPanel::Tab::Content));
    };
    m_hooks.editTrackers = [this](const QStringList &ids)
    {
        if (!ids.isEmpty())
            showDetails(ids.first(), static_cast<int>(DetailsPanel::Tab::Trackers));
    };
}

bool MainWindow::selectTorrent(const QString &id)
{
    const int source = m_model->indexOfId(id);
    if (source < 0)
        return false;
    const QModelIndex proxy = m_proxy->mapFromSource(m_model->index(source, 0));
    if (!proxy.isValid())
        return false;   // filtered out
    m_view->selectRow(proxy.row());
    return true;
}

void MainWindow::showDetails(const QString &id, const int tab)
{
    if (!m_details)
        return;
    selectTorrent(id);
    m_details->setTorrent(id);
    m_details->expand(static_cast<DetailsPanel::Tab>(tab));
}

void MainWindow::setController(TorrentController *controller)
{
    m_controller = controller;
    if (!controller)
    {
        updateCommandStates();
        return;
    }
    connect(controller, &TorrentController::sessionPausedChanged, this, &MainWindow::updateSessionAction);
    connect(controller, &TorrentController::categoriesChanged, this, &MainWindow::onModelChanged);
    connect(controller, &TorrentController::queueingChanged, this, &MainWindow::setQueueingEnabled);
    setQueueingEnabled(controller->queueingEnabled());
    onModelChanged();
    updateSessionAction();
}

void MainWindow::onModelChanged()
{
    if (m_controller)
        m_sidebar->setKnownCategories(m_controller->categories());
    m_sidebar->refreshCounts();
    m_view->updateEmptyState(m_sidebar->currentLabel());
    updateCommandStates();
}

void MainWindow::setQueueingEnabled(const bool enabled)
{
    m_queueing = enabled;
    m_view->setQueueColumnVisible(enabled);
    m_toolbar->setQueueGroupVisible(enabled);
    m_queueSeparator->setVisible(enabled);
    for (QAction *a : std::as_const(m_queueActions))
        a->setVisible(enabled);
    updateCommandStates();
}

QStringList MainWindow::selectedIds() const
{
    return m_view->selectedIds();
}

void MainWindow::updateCommandStates()
{
    if (m_actions.isEmpty() || !m_toolbar)
        return;
    const QStringList ids = selectedIds();
    const CommandPlan plan = m_controller ? m_controller->planFor(ids) : CommandPlan {};
    const bool queue = ActionRules::queueCommandsEnabled(plan, m_queueing);
    action(QStringLiteral("resume"))->setEnabled(plan.resume);
    action(QStringLiteral("pause"))->setEnabled(plan.pause);
    action(QStringLiteral("remove"))->setEnabled(plan.any);
    for (QAction *a : std::as_const(m_queueActions))
        a->setEnabled(queue);
    m_toolbar->setCommandStates(plan.resume, plan.pause, plan.any, queue);
    action(QStringLiteral("session"))->setEnabled(m_controller != nullptr);
}

void MainWindow::updateSessionAction()
{
    QAction *a = action(QStringLiteral("session"));
    if (!a)
        return;
    const bool paused = m_controller && m_controller->sessionPaused();
    a->setText(paused ? tr("Resume Session") : tr("Pause Session"));
    RoMenu::bind(a, paused ? QStringLiteral("m-play-circle") : QStringLiteral("m-pause-circle"), false);
    a->setShortcut(QKeySequence(paused ? QStringLiteral("Ctrl+Shift+S") : QStringLiteral("Ctrl+Shift+P")));
}

void MainWindow::toggleSession()
{
    if (!m_controller)
        return;
    if (m_controller->sessionPaused())
        m_controller->resumeSession();
    else
        m_controller->pauseSession();
}

void MainWindow::notYet(const QString &feature)
{
    RoMessageBox::info(this, feature, tr("%1 is not available in this build yet.").arg(feature));
}

void MainWindow::persistView(const QString &key, const bool on)
{
    SettingsStorage::instance()->storeValue(key, on);
}

bool MainWindow::confirmQuit()
{
    if (!m_controller)
        return true;
    Preferences *pref = Preferences::instance();
    if (!pref->confirmOnExit())
        return true;
    bool transferring = false;
    for (int i = 0; i < m_model->rowCount() && !transferring; ++i)
    {
        const TorrentRow &r = m_model->row(i);
        transferring = !r.finished() && (r.status == RoStatus::Downloading || r.status == RoStatus::Stalled
                                         || r.status == RoStatus::Checking || r.status == RoStatus::Queued);
    }
    if (!transferring)
        return true;

    RoMessageBox::Options o;
    o.kind = RoMessageBox::Kind::Question;
    o.buttons = {tr("No"), tr("Yes"), tr("Always Yes")};
    o.primary = tr("Yes");
    o.defaultButton = tr("No");
    o.detail = tr("Torrents are still downloading.");
    const auto result = RoMessageBox::show(this, tr("Exit RoTorrent"), tr("Are you sure you want to quit RoTorrent?"), o);
    if (result.label == tr("Always Yes"))
    {
        pref->setConfirmOnExit(false);
        return true;
    }
    return result.label == tr("Yes");
}

void MainWindow::requestQuit()
{
    if (!confirmQuit())
        return;
    m_quitting = true;
    QApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_quitting && closeGoesToTray())
    {
        // X = hide to the tray (qBittorrent default); File > Exit and the tray's Exit really quit.
        event->ignore();
        hide();
        GuiLog::add(tr("Main window hidden: RoTorrent keeps running in the notification area"));
        return;
    }
    if (!m_quitting && !confirmQuit())
    {
        event->ignore();
        return;
    }
    m_quitting = true;
    RoFramelessWindow::closeEvent(event);
    // With no tray icon, closing the window ends the program.
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
}

void MainWindow::changeEvent(QEvent *event)
{
    RoFramelessWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && isMinimized() && minimizeGoesToTray())
        QTimer::singleShot(0, this, &QWidget::hide);   // Options > Behavior > "Minimize to notification area"
}

void MainWindow::checkForUpdates()
{
    if (UpdateChecker::feedUrl().isEmpty())
    {
        RoMessageBox::info(this, tr("Check for Updates"), tr("Update checking is not configured."));   // section 11, item 5
        return;
    }
    auto *checker = new UpdateChecker(this);
    connect(checker, &UpdateChecker::finished, this, [this, checker](const UpdateChecker::Outcome &o)
    {
        checker->deleteLater();
        const QString title = tr("Check for Updates");
        switch (o.result)
        {
        case UpdateChecker::Result::NotConfigured:
            RoMessageBox::info(this, title, tr("Update checking is not configured."));
            break;
        case UpdateChecker::Result::UpToDate:
            RoMessageBox::info(this, title, tr("You are running the latest version of RoTorrent (%1).").arg(QStringLiteral(QBT_VERSION_2)));
            break;
        case UpdateChecker::Result::Failed:
            RoMessageBox::error(this, title, tr("Could not check for updates.\n%1").arg(o.error));
            break;
        case UpdateChecker::Result::Available:
        {
            RoMessageBox::Options opts;
            opts.kind = RoMessageBox::Kind::Info;
            opts.detail = o.notes;
            if (!o.url.isEmpty())
            {
                opts.buttons = {tr("Later"), tr("Download")};
                opts.primary = tr("Download");
            }
            const auto r = RoMessageBox::show(this, title, tr("RoTorrent %1 is available.").arg(o.version), opts);
            if (!o.url.isEmpty() && (r.label == tr("Download")))
                QDesktopServices::openUrl(QUrl(o.url));
            break;
        }
        }
    });
    checker->check();
}

void MainWindow::syncAutoAction()
{
    if (!m_autoActionGroup)
        return;
    QAction *a = action(QStringLiteral("auto_") + autoActionKey());
    if (!a)
        a = action(QStringLiteral("auto_nothing"));
    if (a && !a->isChecked())
    {
        // The group unchecks the others; their toggled() signals still run, which repaints the radio dots.
        m_syncingAutoAction = true;
        a->setChecked(true);
        m_syncingAutoAction = false;
    }
}

void MainWindow::setTray(RoTray *tray)
{
    m_tray = tray;
}

void MainWindow::applyTraySettings()
{
    if (m_tray)
        m_tray->applySettings();
}

bool MainWindow::closeGoesToTray() const
{
    return m_tray && m_tray->closeToTray();
}

bool MainWindow::minimizeGoesToTray() const
{
    return m_tray && m_tray->minimizeToTray();
}

void MainWindow::showForStartup(const GuiSettings::WindowState state)
{
    const bool trayVisible = m_tray && m_tray->visible();
    switch (state)
    {
    case GuiSettings::WindowState::Normal:
        show();
        break;
    case GuiSettings::WindowState::Minimized:
        if (minimizeGoesToTray())
            break;   // lives in the notification area
        showMinimized();
        break;
    case GuiSettings::WindowState::Hidden:
        if (!trayVisible)
            showMinimized();   // nothing to restore it from: keep it reachable on the taskbar
        break;
    }
}

void MainWindow::activate()
{
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    show();
    raise();
    activateWindow();
}

QStringList MainWindow::sourcesFromMimeData(const QMimeData *mime)
{
    QStringList sources;
    if (!mime)
        return sources;
    const auto add = [&sources](const QString &source)
    {
        if (!source.isEmpty() && !sources.contains(source))
            sources.append(source);
    };
    const auto isWebOrMagnet = [](const QString &s)
    {
        return s.startsWith(QLatin1String("magnet:"), Qt::CaseInsensitive)
                || s.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
                || s.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)
                || s.startsWith(QLatin1String("ftp://"), Qt::CaseInsensitive);
    };

    if (mime->hasUrls())
    {
        for (const QUrl &url : mime->urls())
        {
            if (url.isLocalFile())
            {
                const QString path = url.toLocalFile();
                if (path.endsWith(QLatin1String(".torrent"), Qt::CaseInsensitive) && QFileInfo(path).isFile())
                    add(path);
            }
            else
            {
                // toEncoded() keeps a magnet link's percent-escapes exactly as the browser sent them.
                const QString text = QString::fromUtf8(url.toEncoded());
                if (isWebOrMagnet(text))
                    add(text);
            }
        }
    }
    if (sources.isEmpty() && mime->hasText())
    {
        const QStringList lines = mime->text().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines)
        {
            const QString text = line.trimmed();
            static const QRegularExpression infoHash(QStringLiteral("^(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64}|[A-Za-z2-7]{32})$"));
            if (isWebOrMagnet(text) || infoHash.match(text).hasMatch())
                add(text);
        }
    }
    return sources;
}

bool MainWindow::canAcceptDrag(const QMimeData *mime) const
{
    return m_hooks.addSources && !sourcesFromMimeData(mime).isEmpty();
}

bool MainWindow::handleDrop(const QMimeData *mime)
{
    const QStringList sources = sourcesFromMimeData(mime);
    if (sources.isEmpty() || !m_hooks.addSources)
        return false;
    // Run the hook after the drop is answered: the add dialog is modal and would otherwise keep the drag
    // source (Explorer) waiting.
    QTimer::singleShot(0, this, [this, sources]
    {
        if (m_hooks.addSources)
            m_hooks.addSources(sources);
    });
    return true;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (canAcceptDrag(event->mimeData()))
        event->acceptProposedAction();
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    if (canAcceptDrag(event->mimeData()))
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (handleDrop(event->mimeData()))
        event->acceptProposedAction();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type type = event->type();
    if ((type == QEvent::DragEnter) || (type == QEvent::DragMove) || (type == QEvent::Drop))
    {
        const auto *widget = qobject_cast<QWidget *>(watched);
        if (widget && (widget->window() == this))
        {
            auto *drag = static_cast<QDropEvent *>(event);   // QDragEnterEvent / QDragMoveEvent derive from it
            if (type == QEvent::Drop)
            {
                if (handleDrop(drag->mimeData()))
                {
                    drag->acceptProposedAction();
                    return true;
                }
            }
            else if (canAcceptDrag(drag->mimeData()))
            {
                drag->acceptProposedAction();
                return true;
            }
        }
    }
    return RoFramelessWindow::eventFilter(watched, event);
}

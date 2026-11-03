// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "EngineWindow.h"

#include "actions/TorrentController.h"
#include "base/bittorrent/session.h"
#include "details/LogBuffer.h"
#include "details/SessionDetailsSource.h"
#include "dialogs/AddTorrentFlow.h"
#include "dialogs/InfoDialogs.h"
#include "dialogs/ManagementDialogs.h"
#include "dialogs/TorrentCreatorDialog.h"
#include "models/SessionAdapter.h"
#include "models/TransferListModel.h"
#include "power/PowerController.h"
#include "StatusFeed.h"
#include "tray/NotificationFeed.h"
#include "tray/RoTray.h"
#include "options/OptionsLauncher.h"
#include "widgets/Sidebar.h"
#include "MainWindow.h"

MainWindow *EngineWindow::create(BitTorrent::Session *session, IApplication *app)
{
    auto *window = new MainWindow;

    TorrentCreatorDialog::install(*window, session);
    auto *adapter = new SessionAdapter(session, window->transferModel(), window);
    window->setController(new TorrentController(session, window));
    AddTorrentFlow::install(*window, session);
    InfoDialogs::install(*window, session);
    ManagementDialogs::install(*window, session);
    if (app)
    {
        OptionsLauncher::install(*window, app);
        OptionsLauncher::applyLiveEffects(*window, app);
    }
    QObject::connect(adapter, &SessionAdapter::ratesChanged, window->sidebar(), &Sidebar::setRates);
    if (adapter->queueingEnabled())
        window->setQueueingEnabled(true);

    // Notification area, toasts, status bar, offline banner and the "do not sleep" inhibitor (Phase 8).
    auto *tray = new RoTray(window);
    window->setTray(tray);
    new NotificationFeed(session, tray);
    new StatusFeed(session, window);
    new PowerController(session, window);

    window->setDetailsSource(new SessionDetailsSource(session, window), session->refreshInterval());
    window->logBuffer()->attachLogger();
    window->sidebar()->refreshCounts();
    return window;
}

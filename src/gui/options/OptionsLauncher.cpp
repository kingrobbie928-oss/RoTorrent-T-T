// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "OptionsLauncher.h"

#include <QCoreApplication>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/interfaces/iapplication.h"
#include "MainWindow.h"
#include "optionsdialog.h"

void OptionsLauncher::install(MainWindow &window, IApplication *app)
{
    static_assert(MainWindow::OPTIONS_PAGE_CONNECTION == OptionsDialog::TAB_CONNECTION);
    MainWindow *w = &window;
    window.hooks().options = [w, app]
    {
        OptionsDialog::run(app, w, -1, [w, app] { applyLiveEffects(*w, app); });
    };
    window.hooks().optionsPage = [w, app](const int page)
    {
        OptionsDialog::run(app, w, page, [w, app] { applyLiveEffects(*w, app); });
    };
}

void OptionsLauncher::applyLiveEffects(MainWindow &window, const IApplication *app)
{
    const auto *session = BitTorrent::Session::instance();
    window.setQueueingEnabled(session->isQueueingSystemEnabled());
    window.setDetailsRefreshInterval(session->refreshInterval());
    window.applyTraySettings();   // "Show RoTorrent in notification area"

    const QString name = app->instanceName();
    const QString base = QCoreApplication::applicationName();
    window.setWindowTitle(name.isEmpty() ? base : (base + u" - "_s + name));
}

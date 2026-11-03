// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "StatusFeed.h"

#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/preferences.h"
#include "GuiLog.h"
#include "MainWindow.h"
#include "widgets/NetBanner.h"
#include "widgets/StatusBar.h"

StatusFeed::StatusFeed(BitTorrent::Session *session, MainWindow *window)
    : QObject(window)
    , m_session(session)
    , m_window(window)
{
    m_offlineTimer.setSingleShot(true);
    m_offlineTimer.setInterval(OFFLINE_GRACE_MS);
    connect(&m_offlineTimer, &QTimer::timeout, this, &StatusFeed::onOfflineGraceOver);

    connect(session, &BitTorrent::Session::statsUpdated, this, &StatusFeed::refresh);
    connect(session, &BitTorrent::Session::speedLimitModeChanged, this, &StatusFeed::refresh);
    connect(session, &BitTorrent::Session::freeDiskSpaceChecked, this, &StatusFeed::refresh);
    connect(Preferences::instance(), &Preferences::changed, this, &StatusFeed::refresh);
    connect(window->banner(), &NetBanner::retryRequested, this, &StatusFeed::retry);
    refresh();
}

void StatusFeed::refresh()
{
    RoStatusBar *bar = m_window->statusBar();

    RoStatusBar::Connection state = RoStatusBar::Connection::Offline;
    if (m_session->isListening())
    {
        state = m_session->status().hasIncomingConnections ? RoStatusBar::Connection::Connected
                                                           : RoStatusBar::Connection::Firewalled;
    }
    bar->setConnection(state);

    // downloadSpeedLimit() / uploadSpeedLimit() already follow the alternative-speed mode.
    bar->setSpeedLimits(m_session->downloadSpeedLimit(), m_session->uploadSpeedLimit());
    bar->setDhtNodes(static_cast<int>(m_session->status().dhtNodes), m_session->isDHTEnabled());
    bar->setFreeSpace(m_session->freeDiskSpace(), Preferences::instance()->isStatusbarFreeDiskSpaceDisplayed());

    const bool offline = (state == RoStatusBar::Connection::Offline);
    if (!offline)
        m_seenOnline = true;
    if (offline == m_offline)
        return;
    m_offline = offline;
    if (offline)
    {
        m_offlineTimer.start();
    }
    else
    {
        m_offlineTimer.stop();
        if (m_bannerShown)
        {
            m_bannerShown = false;
            m_window->banner()->hide();
            GuiLog::add(tr("Network connection restored"), Log::INFO);
        }
    }
}

void StatusFeed::onOfflineGraceOver()
{
    if (!m_offline || m_bannerShown)
        return;
    m_bannerShown = true;
    m_window->banner()->show();
    if (m_seenOnline)   // a start-up that never got online is not a "lost" connection
        GuiLog::add(tr("Network connection lost"), Log::WARNING);
}

void StatusFeed::retry()
{
    GuiLog::add(tr("Retrying to listen for incoming connections"));
    m_session->reconfigureListening();
    QTimer::singleShot(1500, this, &StatusFeed::refresh);
}

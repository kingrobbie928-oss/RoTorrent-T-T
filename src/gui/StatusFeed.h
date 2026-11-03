// SPDX-License-Identifier: GPL-2.0-or-later
// Feeds the status bar and the offline banner from the live session (handoff 8.10, section 9): connection state
// (listening, firewalled, offline), active speed limits, DHT node count and free disk space. Licensed
// GPL-2.0-or-later.
#pragma once

#include <QObject>
#include <QTimer>

class MainWindow;

namespace BitTorrent
{
    class Session;
}

class StatusFeed final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(StatusFeed)

public:
    // How long the session must stay offline before the banner appears. A session that has just started is not
    // listening yet, and a banner flashing at every launch would be wrong.
    static constexpr int OFFLINE_GRACE_MS = 3000;

    StatusFeed(BitTorrent::Session *session, MainWindow *window);

    // Reads everything from the session now (also runs on every statistics update).
    void refresh();

    bool bannerShown() const { return m_bannerShown; }
    void setOfflineGrace(int ms) { m_offlineTimer.setInterval(ms); }

private:
    void onOfflineGraceOver();
    void retry();

    BitTorrent::Session *m_session;
    MainWindow *m_window;
    QTimer m_offlineTimer;
    bool m_offline = false;
    bool m_bannerShown = false;
    bool m_seenOnline = false;
};

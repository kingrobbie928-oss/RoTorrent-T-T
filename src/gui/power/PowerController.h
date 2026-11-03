// SPDX-License-Identifier: GPL-2.0-or-later
// Keeps Windows from suspending while torrents are working (handoff Phase 8, "power inhibitor while
// downloading"). Ported from qBittorrent's PowerManagement / InhibitorWindows and MainWindow::
// updatePowerManagementState: the same rule, split into a pure decision (PowerPolicy) that unit tests can run
// and a controller that polls the session. Options > Behavior > "Inhibit system sleep when torrents are
// downloading / seeding". Licensed GPL-2.0-or-later.
#pragma once

#include <functional>
#include <memory>

#include <QList>
#include <QObject>
#include <QTimer>

namespace BitTorrent
{
    class Session;
}

// The operating system side: ask for "busy" (do not sleep) or "idle".
class Inhibitor
{
public:
    virtual ~Inhibitor() = default;
    virtual bool requestBusy() { return true; }
    virtual bool requestIdle() { return true; }
};

namespace PowerPolicy
{
    struct TorrentFacts
    {
        bool finished = false;
        bool stopped = false;
        bool errored = false;
        bool hasMetadata = true;
        bool moving = false;
    };

    // True when sleep must be prevented: an unfinished, running, healthy torrent while "downloading" is on; a
    // finished, running one while "seeding" is on; any torrent that is moving its files.
    bool shouldInhibit(const QList<TorrentFacts> &torrents, bool whenDownloading, bool whenSeeding);
}

class PowerController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PowerController)

public:
    enum class State { Idle, Busy };

    // `inhibitor` defaults to the Windows one (a no-op elsewhere); tests pass a fake.
    explicit PowerController(BitTorrent::Session *session, QObject *parent = nullptr
                             , std::unique_ptr<Inhibitor> inhibitor = {});
    ~PowerController() override;

    // Replaces the session as the source of torrent facts (unit tests).
    void setFactsSource(std::function<QList<PowerPolicy::TorrentFacts>()> source);

    State state() const { return m_state; }
    // Re-evaluates now (also runs on session changes, preference changes and every minute).
    void update();

    static constexpr int INTERVAL_MS = 60 * 1000;

private:
    void setState(State state);

    BitTorrent::Session *m_session;
    std::unique_ptr<Inhibitor> m_inhibitor;
    std::function<QList<PowerPolicy::TorrentFacts>()> m_factsSource;
    State m_state = State::Idle;
    QTimer m_poll;
    QTimer m_coalesce;
};

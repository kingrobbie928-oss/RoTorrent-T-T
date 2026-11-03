// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "PowerController.h"

#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/preferences.h"

namespace
{
#ifdef Q_OS_WIN
    class InhibitorWindows final : public Inhibitor
    {
    public:
        // SetThreadExecutionState is per thread: the controller only ever runs on the GUI thread.
        bool requestBusy() override { return ::SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED) != 0; }
        bool requestIdle() override { return ::SetThreadExecutionState(ES_CONTINUOUS) != 0; }
    };
#endif

    std::unique_ptr<Inhibitor> makeDefaultInhibitor()
    {
#ifdef Q_OS_WIN
        return std::make_unique<InhibitorWindows>();
#else
        return std::make_unique<Inhibitor>();
#endif
    }
}

bool PowerPolicy::shouldInhibit(const QList<TorrentFacts> &torrents, const bool whenDownloading, const bool whenSeeding)
{
    return std::ranges::any_of(torrents, [=](const TorrentFacts &t)
    {
        if (whenDownloading && !t.finished && !t.stopped && !t.errored && t.hasMetadata)
            return true;
        if (whenSeeding && t.finished && !t.stopped)
            return true;
        return t.moving;
    });
}

PowerController::PowerController(BitTorrent::Session *session, QObject *parent, std::unique_ptr<Inhibitor> inhibitor)
    : QObject(parent)
    , m_session(session)
    , m_inhibitor(inhibitor ? std::move(inhibitor) : makeDefaultInhibitor())
{
    m_poll.setInterval(INTERVAL_MS);
    connect(&m_poll, &QTimer::timeout, this, &PowerController::update);
    m_coalesce.setSingleShot(true);
    m_coalesce.setInterval(250);
    connect(&m_coalesce, &QTimer::timeout, this, &PowerController::update);

    if (m_session)
    {
        using BitTorrent::Session;
        for (const auto signal : {&Session::torrentAdded, &Session::torrentFinished, &Session::torrentStopped
                                  , &Session::torrentStarted, &Session::torrentAboutToBeRemoved})
            connect(m_session, signal, &m_coalesce, qOverload<>(&QTimer::start));
    }
    connect(Preferences::instance(), &Preferences::changed, &m_coalesce, qOverload<>(&QTimer::start));

    update();
    m_poll.start();
}

PowerController::~PowerController()
{
    setState(State::Idle);   // never leave the machine held awake
}

void PowerController::setFactsSource(std::function<QList<PowerPolicy::TorrentFacts>()> source)
{
    m_factsSource = std::move(source);
    update();
}

void PowerController::update()
{
    const Preferences *pref = Preferences::instance();
    QList<PowerPolicy::TorrentFacts> facts;
    if (m_factsSource)
    {
        facts = m_factsSource();
    }
    else if (m_session)
    {
        const QList<BitTorrent::Torrent *> torrents = m_session->torrents();
        facts.reserve(torrents.size());
        for (const BitTorrent::Torrent *t : torrents)
        {
            facts.append({t->isFinished(), t->isStopped(), t->isErrored(), t->hasMetadata(), t->isMoving()});
        }
    }
    setState(PowerPolicy::shouldInhibit(facts, pref->preventFromSuspendWhenDownloading()
                                        , pref->preventFromSuspendWhenSeeding()) ? State::Busy : State::Idle);
}

void PowerController::setState(const State state)
{
    if (state == m_state)
        return;
    const bool ok = (state == State::Busy) ? m_inhibitor->requestBusy() : m_inhibitor->requestIdle();
    if (ok)
        m_state = state;
}

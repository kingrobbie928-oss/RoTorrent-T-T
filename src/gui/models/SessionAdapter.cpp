// SPDX-License-Identifier: GPL-2.0-or-later
#include "SessionAdapter.h"

#include <QUrl>

#include "TransferListModel.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "base/utils/misc.h"

namespace
{
    constexpr qlonglong MAX_ETA = 8640000;   // the engine's "infinite"
    constexpr qreal MAX_RATIO = 9999;
}

void SessionAdapter::fillTexts(TorrentRow &row)
{
    row.sizeText = (row.totalSize < 0) ? QObject::tr("Unknown") : Utils::Misc::friendlyUnit(row.totalSize);
    row.downText = Utils::Misc::friendlyUnit(row.downRate, true);
    row.upText = Utils::Misc::friendlyUnit(row.upRate, true);
    row.etaText = (row.etaSeconds < 0) ? QStringLiteral("\u2014") : Utils::Misc::userFriendlyDuration(row.etaSeconds, MAX_ETA);
    row.ratioText = (row.ratio >= MAX_RATIO) ? QStringLiteral("\u221e") : QString::number(row.ratio, 'f', 2);
}

TorrentRow SessionAdapter::makeRow(const BitTorrent::Torrent *t, const QString &trackerHost)
{
    TorrentRow r;
    r.id = t->id().toString();
    r.name = t->name();
    r.totalSize = t->totalSize();
    r.progress = t->progress() * 100.0;
    r.status = statusOf(t);   // the single place a status is decided
    r.downRate = t->downloadPayloadRate();
    r.upRate = t->uploadPayloadRate();
    r.etaSeconds = ((t->eta() <= 0) || (t->eta() >= MAX_ETA)) ? -1 : t->eta();
    r.ratio = t->realRatio();
    r.queuePos = t->queuePosition();
    r.category = t->category();
    r.trackerHost = trackerHost;
    fillTexts(r);
    return r;
}

SessionAdapter::SessionAdapter(BitTorrent::Session *session, TransferListModel *model, QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_model(model)
{
    using BitTorrent::Session;
    connect(session, &Session::torrentsLoaded, this, &SessionAdapter::refreshMany);
    connect(session, &Session::torrentAdded, this, [this](BitTorrent::Torrent *t) { refresh(t); });
    connect(session, &Session::torrentsUpdated, this, &SessionAdapter::refreshMany);
    connect(session, &Session::torrentStopped, this, [this](BitTorrent::Torrent *t) { refresh(t); });
    connect(session, &Session::torrentStarted, this, [this](BitTorrent::Torrent *t) { refresh(t); });
    connect(session, &Session::torrentFinished, this, [this](BitTorrent::Torrent *t) { refresh(t); });
    connect(session, &Session::torrentCategoryChanged, this, [this](BitTorrent::Torrent *t, const QString &) { refresh(t); });
    connect(session, &Session::torrentAboutToBeRemoved, this, [this](BitTorrent::Torrent *t)
    {
        const QString id = t->id().toString();
        m_hosts.remove(id);
        m_model->removeId(id);
    });
    connect(session, &Session::trackersAdded, this, [this](BitTorrent::Torrent *t, const QList<BitTorrent::TrackerEntry> &)
    {
        m_hosts.remove(t->id().toString());
        refresh(t);
    });
    connect(session, &Session::trackersRemoved, this, [this](BitTorrent::Torrent *t, const QStringList &)
    {
        m_hosts.remove(t->id().toString());
        refresh(t);
    });
    connect(session, &Session::statsUpdated, this, [this]
    {
        const BitTorrent::SessionStatus &s = m_session->status();
        emit ratesChanged(s.payloadDownloadRate, s.payloadUploadRate);
    });

    if (session->isRestored())
        loadAll();
    else
        connect(session, &Session::restored, this, &SessionAdapter::loadAll);
}

bool SessionAdapter::queueingEnabled() const
{
    return m_session->isQueueingSystemEnabled();
}

QString SessionAdapter::hostOf(const BitTorrent::Torrent *t)
{
    const QString id = t->id().toString();
    if (const auto it = m_hosts.constFind(id); it != m_hosts.constEnd())
        return *it;

    QString host;
    for (const BitTorrent::TrackerEntryStatus &entry : t->trackers())
    {
        const QUrl url(entry.url);
        if ((url.scheme() == u"http") || (url.scheme() == u"https") || (url.scheme() == u"udp"))
        {
            host = url.host();
            break;
        }
    }
    m_hosts.insert(id, host);
    return host;
}

void SessionAdapter::loadAll()
{
    refreshMany(m_session->torrents());
    emit queueingChanged(queueingEnabled());
}

void SessionAdapter::refresh(const BitTorrent::Torrent *t)
{
    if (t)
        m_model->upsert(makeRow(t, hostOf(t)));
}

void SessionAdapter::refreshMany(const QList<BitTorrent::Torrent *> &torrents)
{
    QList<TorrentRow> rows;
    rows.reserve(torrents.size());
    for (const BitTorrent::Torrent *t : torrents)
        rows.append(makeRow(t, hostOf(t)));
    m_model->upsertMany(rows);
}

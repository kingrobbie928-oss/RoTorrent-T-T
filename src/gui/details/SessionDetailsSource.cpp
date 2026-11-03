// SPDX-License-Identifier: GPL-2.0-or-later
#include "SessionDetailsSource.h"

#include <chrono>

#include <QFutureWatcher>
#include <QUrl>

#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/peeraddress.h"
#include "base/bittorrent/peerinfo.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/trackerentry.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/net/geoipmanager.h"
#include "base/path.h"
#include "models/rostatus.h"

namespace
{
    using Clock = std::chrono::high_resolution_clock;

    qint64 secondsUntil(const BitTorrent::AnnounceTimePoint &when)
    {
        if (when == BitTorrent::AnnounceTimePoint {})
            return -1;
        const auto diff = std::chrono::duration_cast<std::chrono::seconds>(when - Clock::now()).count();
        return (diff > 0) ? diff : 0;
    }

    Details::TrackerState toState(const BitTorrent::TrackerEndpointState state, const bool updating)
    {
        if (updating)
            return Details::TrackerState::Updating;
        switch (state)
        {
        case BitTorrent::TrackerEndpointState::Working: return Details::TrackerState::Working;
        case BitTorrent::TrackerEndpointState::NotContacted: return Details::TrackerState::NotContacted;
        default: return Details::TrackerState::NotWorking;   // NotWorking, TrackerError, Unreachable
        }
    }

    Details::Priority toPriority(const BitTorrent::DownloadPriority p)
    {
        switch (p)
        {
        case BitTorrent::DownloadPriority::Ignored: return Details::Skip;
        case BitTorrent::DownloadPriority::High: return Details::High;
        case BitTorrent::DownloadPriority::Maximum: return Details::Maximum;
        default: return Details::Normal;
        }
    }

    BitTorrent::DownloadPriority toEngine(const Details::Priority p)
    {
        switch (p)
        {
        case Details::Skip: return BitTorrent::DownloadPriority::Ignored;
        case Details::High: return BitTorrent::DownloadPriority::High;
        case Details::Maximum: return BitTorrent::DownloadPriority::Maximum;
        default: return BitTorrent::DownloadPriority::Normal;
        }
    }

    QString btVersionText(const BitTorrent::TrackerEntryStatus &status)
    {
        bool v1 = false, v2 = false;
        for (auto it = status.endpoints.cbegin(); it != status.endpoints.cend(); ++it)
        {
            (it.value().btVersion == 2 ? v2 : v1) = true;
        }
        if (v1 && v2)
            return QStringLiteral("v1 & v2");
        if (v2)
            return QStringLiteral("v2");
        return v1 ? QStringLiteral("v1") : QString();
    }
}

SessionDetailsSource::SessionDetailsSource(BitTorrent::Session *session, QObject *parent)
    : DetailsSource(parent)
    , m_session(session)
{
    qRegisterMetaType<QList<Details::Peer>>("QList<Details::Peer>");
}

BitTorrent::Torrent *SessionDetailsSource::torrent(const QString &id) const
{
    const auto tid = BitTorrent::TorrentID::fromString(id);
    return tid.isValid() ? m_session->getTorrent(tid) : nullptr;
}

bool SessionDetailsSource::contains(const QString &id) const
{
    return torrent(id) != nullptr;
}

Details::General SessionDetailsSource::general(const QString &id) const
{
    Details::General g;
    const BitTorrent::Torrent *t = torrent(id);
    if (!t)
        return g;

    g.valid = true;
    g.id = id;
    g.name = t->name();
    g.status = statusOf(t);
    g.hasMetadata = t->hasMetadata();
    g.timeActiveSecs = t->activeTime();
    g.etaSecs = t->eta();
    g.connections = t->connectionsCount();
    g.connectionsLimit = t->connectionsLimit();
    g.downloaded = t->totalDownload();
    g.uploaded = t->totalUpload();
    g.downloadedSession = t->totalPayloadDownload();
    g.uploadedSession = t->totalPayloadUpload();
    g.seeds = t->seedsCount();
    g.seedsTotal = t->totalSeedsCount();
    g.peers = t->leechsCount();
    g.peersTotal = t->totalLeechersCount();
    g.downSpeed = t->downloadPayloadRate();
    g.upSpeed = t->uploadPayloadRate();
    g.downLimit = t->downloadLimit();
    g.upLimit = t->uploadLimit();
    g.wasted = t->wastedSize();
    g.ratio = t->realRatio();
    g.reannounceInSecs = t->nextAnnounce();
    g.lastSeenComplete = t->lastSeenComplete();
    g.popularity = t->popularity();

    g.totalSize = t->totalSize();
    g.piecesCount = t->piecesCount();
    g.pieceLength = t->pieceLength();
    g.piecesHave = t->piecesHave();
    g.createdBy = t->creator();
    g.addedOn = t->addedTime();
    g.completedOn = t->completedTime();
    g.createdOn = t->creationDate();
    g.isPrivate = t->isPrivate();
    const BitTorrent::InfoHash hash = t->infoHash();
    g.hashV1 = hash.v1().isValid() ? hash.v1().toString() : QString();
    g.hashV2 = hash.v2().isValid() ? hash.v2().toString() : QString();
    g.savePath = t->savePath().toString();
    g.comment = t->comment();
    g.pieces = t->pieces();
    g.progress = t->progress();
    return g;
}

QList<Details::Tracker> SessionDetailsSource::trackers(const QString &id) const
{
    QList<Details::Tracker> rows;
    const BitTorrent::Torrent *t = torrent(id);
    if (!t)
        return rows;

    const auto pseudo = [&](const QString &name, const bool enabled, const QHash<QString, int> &counts)
    {
        Details::Tracker row;
        row.url = name;
        row.pseudo = true;
        row.state = enabled ? Details::TrackerState::Working : Details::TrackerState::Disabled;
        if (enabled && !t->isPrivate() && counts.contains(id))
            row.peers = counts.value(id);
        if (t->isPrivate())
            row.message = QObject::tr("This torrent is private");
        rows.append(row);
    };
    pseudo(QStringLiteral("** [DHT] **"), m_session->isDHTEnabled() && !t->isPrivate(), m_peerCountDHT);
    pseudo(QStringLiteral("** [PeX] **"), m_session->isPeXEnabled() && !t->isPrivate(), m_peerCountPeX);
    pseudo(QStringLiteral("** [LSD] **"), m_session->isLSDEnabled() && !t->isPrivate(), m_peerCountLSD);

    for (const BitTorrent::TrackerEntryStatus &status : asConst(t->trackers()))
    {
        Details::Tracker row;
        row.url = status.url;
        row.tier = status.tier;
        row.btProtocol = btVersionText(status);
        row.state = toState(status.state, status.isUpdating);
        row.peers = status.numPeers;
        row.seeds = status.numSeeds;
        row.leeches = status.numLeeches;
        row.downloaded = status.numDownloaded;
        row.message = status.message;
        row.nextAnnounceSecs = secondsUntil(status.nextAnnounceTime);
        row.minAnnounceSecs = secondsUntil(status.minAnnounceTime);
        rows.append(row);
    }
    return rows;
}

QList<Details::WebSeed> SessionDetailsSource::webSeeds(const QString &id) const
{
    QList<Details::WebSeed> rows;
    if (const BitTorrent::Torrent *t = torrent(id))
    {
        for (const QUrl &url : asConst(t->urlSeeds()))
            rows.append({url.toString()});
    }
    return rows;
}

QList<Details::File> SessionDetailsSource::files(const QString &id) const
{
    QList<Details::File> rows;
    const BitTorrent::Torrent *t = torrent(id);
    if (!t || !t->hasMetadata())
        return rows;

    const int count = t->filesCount();
    const QList<BitTorrent::DownloadPriority> priorities = t->filePriorities();
    const QList<qreal> progress = t->filesProgress();
    rows.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        Details::File f;
        f.index = i;
        f.path = t->filePath(i).data();
        f.size = t->fileSize(i);
        f.progress = (i < progress.size()) ? progress.at(i) : 0;
        f.priority = (i < priorities.size()) ? toPriority(priorities.at(i)) : Details::Normal;
        rows.append(f);
    }
    return rows;
}

void SessionDetailsSource::requestPeers(const QString &id)
{
    BitTorrent::Torrent *t = torrent(id);
    if (!t)
        return;

    auto *watcher = new QFutureWatcher<QList<BitTorrent::PeerInfo>>(this);
    connect(watcher, &QFutureWatcher<QList<BitTorrent::PeerInfo>>::finished, this, [this, watcher, id]
    {
        const QList<BitTorrent::PeerInfo> infos = watcher->result();
        watcher->deleteLater();

        QList<Details::Peer> rows;
        rows.reserve(infos.size());
        int dht = 0, pex = 0, lsd = 0;
        for (const BitTorrent::PeerInfo &info : infos)
        {
            const BitTorrent::PeerAddress address = info.address();
            Details::Peer p;
            p.ip = address.ip.toString();
            p.port = address.port;
            p.countryCode = info.country();
            p.country = p.countryCode.isEmpty() ? QString() : Net::GeoIPManager::CountryName(p.countryCode);
            p.connection = info.connectionType();
            p.flags = info.flags();
            p.flagsDescription = info.flagsDescription();
            p.client = info.client();
            p.progress = info.progress();
            p.downSpeed = info.payloadDownSpeed();
            p.upSpeed = info.payloadUpSpeed();
            p.downloaded = info.totalDownload();
            p.uploaded = info.totalUpload();
            p.relevance = info.relevance();
            rows.append(p);
            dht += info.fromDHT();
            pex += info.fromPeX();
            lsd += info.fromLSD();
        }
        m_peerCountDHT[id] = dht;
        m_peerCountPeX[id] = pex;
        m_peerCountLSD[id] = lsd;
        emit peersReady(id, rows);
    });
    watcher->setFuture(t->fetchPeerInfo());
}

void SessionDetailsSource::requestAvailability(const QString &id)
{
    BitTorrent::Torrent *t = torrent(id);
    if (!t || !t->hasMetadata())
        return;

    auto *pieces = new QFutureWatcher<QList<int>>(this);
    connect(pieces, &QFutureWatcher<QList<int>>::finished, this, [this, pieces, id]
    {
        const QList<int> availability = pieces->result();
        pieces->deleteLater();

        // File fractions are requested second so one signal carries both.
        BitTorrent::Torrent *t2 = torrent(id);
        if (!t2)
            return;
        auto *fractions = new QFutureWatcher<QList<qreal>>(this);
        connect(fractions, &QFutureWatcher<QList<qreal>>::finished, this, [this, fractions, id, availability]
        {
            const QList<qreal> result = fractions->result();
            fractions->deleteLater();
            emit availabilityReady(id, availability, QList<double>(result.cbegin(), result.cend()));
        });
        fractions->setFuture(t2->fetchAvailableFileFractions());
    });
    pieces->setFuture(t->fetchPieceAvailability());
}

void SessionDetailsSource::addTrackers(const QString &id, const QStringList &urls)
{
    BitTorrent::Torrent *t = torrent(id);
    if (!t)
        return;
    QList<BitTorrent::TrackerEntry> entries;
    for (const QString &line : urls)
    {
        const QString url = line.trimmed();
        if (!url.isEmpty())
            entries.append({url, 0});
    }
    if (!entries.isEmpty())
        t->addTrackers(entries);
}

bool SessionDetailsSource::editTracker(const QString &id, const QString &oldUrl, const QString &newUrl)
{
    BitTorrent::Torrent *t = torrent(id);
    const QString url = newUrl.trimmed();
    if (!t || url.isEmpty())
        return false;

    QList<BitTorrent::TrackerEntry> entries;
    bool replaced = false;
    for (const BitTorrent::TrackerEntryStatus &status : asConst(t->trackers()))
    {
        if (status.url == oldUrl)
        {
            entries.append({url, status.tier});
            replaced = true;
        }
        else
        {
            entries.append({status.url, status.tier});
        }
    }
    if (!replaced)
        return false;
    t->replaceTrackers(entries);
    return true;
}

void SessionDetailsSource::removeTrackers(const QString &id, const QStringList &urls)
{
    if (BitTorrent::Torrent *t = torrent(id))
        t->removeTrackers(urls);
}

void SessionDetailsSource::reannounce(const QString &id, const QStringList &urls)
{
    BitTorrent::Torrent *t = torrent(id);
    if (!t)
        return;
    if (urls.isEmpty())
    {
        t->forceReannounce();
        return;
    }
    // forceReannounce(index) takes the position in trackers() (as upstream's TrackerListWidget does).
    const QList<BitTorrent::TrackerEntryStatus> all = t->trackers();
    for (int i = 0; i < all.size(); ++i)
    {
        if (urls.contains(all.at(i).url))
            t->forceReannounce(i);
    }
}

int SessionDetailsSource::addPeers(const QString &id, const QStringList &endpoints)
{
    BitTorrent::Torrent *t = torrent(id);
    if (!t)
        return 0;
    int accepted = 0;
    for (const QString &text : endpoints)
    {
        const BitTorrent::PeerAddress address = BitTorrent::PeerAddress::parse(text.trimmed());
        if (!address.ip.isNull() && (address.port > 0) && t->connectPeer(address))
            ++accepted;
    }
    return accepted;
}

void SessionDetailsSource::banPeers(const QStringList &endpoints)
{
    for (const QString &text : endpoints)
    {
        const BitTorrent::PeerAddress address = BitTorrent::PeerAddress::parse(text.trimmed());
        if (!address.ip.isNull())
            m_session->banIP(address.ip.toString());
    }
}

void SessionDetailsSource::addWebSeeds(const QString &id, const QStringList &urls)
{
    BitTorrent::Torrent *t = torrent(id);
    if (!t)
        return;
    QList<QUrl> parsed;
    for (const QString &text : urls)
    {
        const QUrl url = QUrl::fromUserInput(text.trimmed());
        if (url.isValid() && !url.host().isEmpty())
            parsed.append(url);
    }
    if (!parsed.isEmpty())
        t->addUrlSeeds(parsed);
}

void SessionDetailsSource::removeWebSeeds(const QString &id, const QStringList &urls)
{
    BitTorrent::Torrent *t = torrent(id);
    if (!t)
        return;
    QList<QUrl> parsed;
    for (const QString &text : urls)
        parsed.append(QUrl(text));
    t->removeUrlSeeds(parsed);
}

void SessionDetailsSource::setFilePriority(const QString &id, const QList<int> &fileIndexes, const Details::Priority priority)
{
    BitTorrent::Torrent *t = torrent(id);
    if (!t || !t->hasMetadata())
        return;
    QList<BitTorrent::DownloadPriority> all = t->filePriorities();
    for (const int index : fileIndexes)
    {
        if (index >= 0 && index < all.size())
            all[index] = toEngine(priority);
    }
    t->prioritizeFiles(all);
}

bool SessionDetailsSource::renameFile(const QString &id, const QString &oldPath, const QString &newPath
                                      , const bool isFolder, QString *error)
{
    BitTorrent::Torrent *t = torrent(id);
    if (!t || !t->hasMetadata())
        return false;
    try
    {
        if (isFolder)
            t->renameFolder(Path(oldPath), Path(newPath));
        else
            t->renameFile(Path(oldPath), Path(newPath));
        return true;
    }
    catch (const RuntimeError &e)
    {
        if (error)
            *error = e.message();
        return false;
    }
}

QString SessionDetailsSource::absolutePath(const QString &id, const QString &relativePath) const
{
    const BitTorrent::Torrent *t = torrent(id);
    if (!t || !t->hasMetadata())
        return {};
    for (int i = 0; i < t->filesCount(); ++i)
    {
        if (t->filePath(i).data() == relativePath)
            return (t->actualStorageLocation() / t->actualFilePath(i)).toString();
    }
    return (t->actualStorageLocation() / Path(relativePath)).toString();   // a folder
}

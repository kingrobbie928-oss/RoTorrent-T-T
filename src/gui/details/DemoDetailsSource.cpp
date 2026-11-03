// SPDX-License-Identifier: GPL-2.0-or-later
#include "DemoDetailsSource.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QTimer>

#include "models/TransferListModel.h"

namespace
{
    quint32 seedOf(const QString &text, const QString &salt = {})
    {
        const QByteArray h = QCryptographicHash::hash((text + salt).toUtf8(), QCryptographicHash::Sha1);
        quint32 v = 0;
        for (int i = 0; i < 4; ++i)
            v = (v << 8) | static_cast<quint8>(h.at(i));
        return v;
    }

    const QStringList TRACKER_URLS = {
        QStringLiteral("https://tracker.linuxtracker.org:443/announce"), QStringLiteral("udp://tracker.opentrackr.org:1337/announce")
        , QStringLiteral("udp://open.stealth.si:80/announce"), QStringLiteral("udp://tracker.torrent.eu.org:451/announce")
        , QStringLiteral("udp://exodus.desync.com:6969/announce")
    };
    const QStringList CLIENTS = {QStringLiteral("qBittorrent 5.0.4"), QStringLiteral("Transmission 4.0.6"), QStringLiteral("Deluge 2.1.1")
                                 , QStringLiteral("libtorrent 2.0.10"), QStringLiteral("µTorrent 3.6.0"), QStringLiteral("BiglyBT 3.8.0.0")};
    const QStringList COUNTRIES = {QStringLiteral("US"), QStringLiteral("DE"), QStringLiteral("NL"), QStringLiteral("FR"), QStringLiteral("GB")
                                   , QStringLiteral("CA"), QStringLiteral("SE"), QStringLiteral("JP")};
    const QStringList FLAGS = {QStringLiteral("D K E I"), QStringLiteral("U E"), QStringLiteral("d H X P"), QStringLiteral("D K"), QStringLiteral("u E")};
}

DemoDetailsSource::DemoDetailsSource(const TransferListModel *model, QObject *parent)
    : DetailsSource(parent)
    , m_model(model)
{
    qRegisterMetaType<QList<Details::Peer>>("QList<Details::Peer>");
}

bool DemoDetailsSource::contains(const QString &id) const
{
    return m_model->indexOfId(id) >= 0;
}

DemoDetailsSource::State &DemoDetailsSource::state(const QString &id) const
{
    State &s = m_state[id];
    if (s.init)
        return s;
    s.init = true;

    const TorrentRow &row = m_model->row(m_model->indexOfId(id));
    QRandomGenerator rng(seedOf(row.name, QStringLiteral("trk")));

    Details::Tracker dht, pex, lsd;
    dht.url = QStringLiteral("** [DHT] **");
    pex.url = QStringLiteral("** [PeX] **");
    lsd.url = QStringLiteral("** [LSD] **");
    for (Details::Tracker *t : {&dht, &pex, &lsd})
    {
        t->pseudo = true;
        t->state = Details::TrackerState::Working;
    }
    lsd.state = Details::TrackerState::NotContacted;
    s.trackers = {dht, pex, lsd};
    QStringList pool = TRACKER_URLS;
    for (int k = 0; k < 3; ++k)
    {
        Details::Tracker t;
        t.url = pool.takeAt(static_cast<int>(rng.bounded(static_cast<quint32>(pool.size()))));
        t.tier = k;
        t.btProtocol = QStringLiteral("v1");
        const Details::TrackerState states[] = {Details::TrackerState::Working, Details::TrackerState::Working
                                                , Details::TrackerState::Updating, Details::TrackerState::NotContacted
                                                , Details::TrackerState::NotWorking};
        t.state = states[rng.bounded(5u)];
        t.peers = static_cast<int>(rng.bounded(41u));
        t.seeds = static_cast<int>(rng.bounded(401u));
        t.leeches = static_cast<int>(rng.bounded(41u));
        t.downloaded = static_cast<int>(rng.bounded(901u));
        t.message = (t.state == Details::TrackerState::NotWorking) ? QStringLiteral("Timed out") : QString();
        t.nextAnnounceSecs = 60 + static_cast<qint64>(rng.bounded(1700u));
        t.minAnnounceSecs = 300;
        s.trackers.append(t);
    }

    if (row.name.startsWith(QLatin1String("debian")))
        s.webSeeds = {{QStringLiteral("https://cdimage.debian.org/debian-cd/current/amd64/iso-cd/")}, {QStringLiteral("https://mirror.example.net/debian-cd/")}};
    else if (row.name.startsWith(QLatin1String("fedora")))
        s.webSeeds = {{QStringLiteral("https://download.fedoraproject.org/pub/fedora/linux/releases/42/")}};

    const qint64 total = row.totalSize;
    const auto file = [&](const int index, const QString &path, const qint64 size)
    {
        Details::File f;
        f.index = index;
        f.path = path;
        f.size = size;
        f.progress = row.progress / 100.0;
        f.availability = (row.progress < 100) ? 0.8 + (seedOf(path) % 20) / 100.0 : -1;
        return f;
    };
    if (row.name.startsWith(QLatin1String("blender")))
    {
        const QString root = QStringLiteral("blender-studio-open-movies-4K/");
        s.files = {file(0, root + QStringLiteral("blender-studio-open-movies-4K.mkv"), total * 93 / 100)
                   , file(1, root + QStringLiteral("subtitles/en.srt"), 96 * 1024), file(2, root + QStringLiteral("subtitles/de.srt"), 91 * 1024)
                   , file(3, root + QStringLiteral("README.txt"), 3 * 1024), file(4, root + QStringLiteral("poster.jpg"), total / 500)};
    }
    else if (row.name.startsWith(QLatin1String("debian")))
    {
        s.files = {file(0, row.name, qMax<qint64>(1, total - 2048)), file(1, QStringLiteral("SHA256SUMS"), 1200), file(2, QStringLiteral("SHA256SUMS.sign"), 833)};
    }
    else
    {
        s.files = {file(0, row.name, qMax<qint64>(1, total))};
    }
    return s;
}

Details::General DemoDetailsSource::general(const QString &id) const
{
    Details::General g;
    const int i = m_model->indexOfId(id);
    if (i < 0)
        return g;
    const TorrentRow &row = m_model->row(i);
    QRandomGenerator rng(seedOf(row.name, QStringLiteral("gen")));

    g.valid = true;
    g.id = id;
    g.name = row.name;
    g.status = row.status;
    g.progress = row.progress / 100.0;
    g.totalSize = row.totalSize;
    g.pieceLength = 4 * 1024 * 1024;
    g.piecesCount = static_cast<int>(qMax<qint64>(1, row.totalSize / g.pieceLength));
    g.piecesHave = static_cast<int>(g.piecesCount * g.progress);
    g.timeActiveSecs = 3600 + static_cast<qint64>(rng.bounded(200000u));
    g.etaSecs = row.etaSeconds;
    g.connections = row.downRate + row.upRate > 0 ? 6 : 0;
    g.connectionsLimit = 48;
    g.downloaded = static_cast<qint64>(row.totalSize * g.progress);
    g.uploaded = static_cast<qint64>(g.downloaded * row.ratio);
    g.downloadedSession = g.downloaded / 2;
    g.uploadedSession = g.uploaded / 2;
    g.seeds = g.connections ? 3 : 0;
    g.seedsTotal = 120;
    g.peers = g.connections ? 4 : 0;
    g.peersTotal = 18;
    g.downSpeed = row.downRate;
    g.upSpeed = row.upRate;
    g.wasted = static_cast<qint64>(rng.bounded(5u)) * 1024 * 1024;
    g.ratio = row.ratio;
    g.reannounceInSecs = 600;
    if (row.progress >= 100 || g.seeds)
        g.lastSeenComplete = QDateTime::currentDateTime();
    g.popularity = 0.2 + (rng.bounded(320u) / 100.0);
    g.createdBy = QStringLiteral("libtorrent 2.0.9");
    g.addedOn = QDateTime(QDate(2026, 8, 14), QTime(9, 22));
    if (row.progress >= 100)
        g.completedOn = g.addedOn.addSecs(5400);
    g.createdOn = QDateTime(QDate(2025, 8, 14), QTime(9, 22));
    g.hashV1 = QString::fromLatin1(QCryptographicHash::hash(row.name.toUtf8(), QCryptographicHash::Sha1).toHex());
    g.hashV2 = QString::fromLatin1(QCryptographicHash::hash(row.name.toUtf8(), QCryptographicHash::Sha256).toHex());
    g.savePath = QStringLiteral("C:/Users/you/Downloads");
    g.pieces = QBitArray(g.piecesCount);
    QRandomGenerator pr(seedOf(row.name, QStringLiteral("pc")));
    for (int p = 0; p < g.piecesCount; ++p)
        g.pieces.setBit(p, (p < g.piecesHave) || (row.progress < 99 && pr.bounded(100u) < 12));
    return g;
}

QList<Details::Tracker> DemoDetailsSource::trackers(const QString &id) const
{
    return contains(id) ? state(id).trackers : QList<Details::Tracker> {};
}

QList<Details::WebSeed> DemoDetailsSource::webSeeds(const QString &id) const
{
    return contains(id) ? state(id).webSeeds : QList<Details::WebSeed> {};
}

QList<Details::File> DemoDetailsSource::files(const QString &id) const
{
    return contains(id) ? state(id).files : QList<Details::File> {};
}

void DemoDetailsSource::requestPeers(const QString &id)
{
    const int i = m_model->indexOfId(id);
    if (i < 0)
        return;
    const TorrentRow &row = m_model->row(i);
    QList<Details::Peer> peers;
    const bool active = (row.status == RoStatus::Downloading) || (row.status == RoStatus::Seeding) || (row.status == RoStatus::Stalled);
    if (active)
    {
        QRandomGenerator rng(seedOf(row.name, QStringLiteral("peer")));
        const int n = 5 + static_cast<int>(rng.bounded(5u));
        for (int k = 0; k < n; ++k)
        {
            Details::Peer p;
            p.ip = QStringLiteral("%1.%2.%3.%4").arg(11 + rng.bounded(200u)).arg(rng.bounded(256u)).arg(rng.bounded(256u)).arg(1 + rng.bounded(254u));
            p.port = static_cast<int>(10000 + rng.bounded(55000u));
            p.countryCode = COUNTRIES.at(static_cast<int>(rng.bounded(static_cast<quint32>(COUNTRIES.size()))));
            p.country = p.countryCode;
            p.connection = (rng.bounded(2u) ? QStringLiteral("µTP") : QStringLiteral("BT"));
            p.flags = FLAGS.at(static_cast<int>(rng.bounded(static_cast<quint32>(FLAGS.size()))));
            p.flagsDescription = QStringLiteral("demo flags");
            p.client = CLIENTS.at(static_cast<int>(rng.bounded(static_cast<quint32>(CLIENTS.size()))));
            p.progress = rng.bounded(101u) / 100.0;
            p.downSpeed = rng.bounded(3u) ? static_cast<qint64>(rng.bounded(900u)) * 1024 : 0;
            p.upSpeed = rng.bounded(2u) ? static_cast<qint64>(rng.bounded(300u)) * 1024 : 0;
            p.downloaded = static_cast<qint64>(rng.bounded(3000u)) * 1024 * 1024;
            p.uploaded = static_cast<qint64>(rng.bounded(800u)) * 1024 * 1024;
            p.relevance = rng.bounded(100u) / 100.0;
            peers.append(p);
        }
        for (const QString &endpoint : state(id).extraPeers)
        {
            Details::Peer p;
            p.ip = endpoint.section(u':', 0, -2);
            p.port = endpoint.section(u':', -1).toInt();
            p.connection = QStringLiteral("BT");
            p.client = QStringLiteral("added manually");
            peers.append(p);
        }
    }
    QTimer::singleShot(0, this, [this, id, peers] { emit peersReady(id, peers); });
}

void DemoDetailsSource::requestAvailability(const QString &id)
{
    const int i = m_model->indexOfId(id);
    if (i < 0)
        return;
    const TorrentRow &row = m_model->row(i);
    const Details::General g = general(id);
    QRandomGenerator rng(seedOf(row.name, QStringLiteral("av")));
    QList<int> pieces;
    for (int p = 0; p < g.piecesCount; ++p)
        pieces.append(static_cast<int>(rng.bounded(7u)));
    QList<double> fractions;
    for (const Details::File &f : state(id).files)
        fractions.append(f.availability);
    QTimer::singleShot(0, this, [this, id, pieces, fractions] { emit availabilityReady(id, pieces, fractions); });
}

void DemoDetailsSource::addTrackers(const QString &id, const QStringList &urls)
{
    if (!contains(id))
        return;
    State &s = state(id);
    for (const QString &raw : urls)
    {
        const QString url = raw.trimmed();
        if (url.isEmpty())
            continue;
        Details::Tracker t;
        t.url = url;
        t.tier = 0;
        t.btProtocol = QStringLiteral("v1");
        s.trackers.append(t);
    }
    m_actionLog << QStringLiteral("addTrackers %1").arg(urls.join(u','));
}

bool DemoDetailsSource::editTracker(const QString &id, const QString &oldUrl, const QString &newUrl)
{
    if (!contains(id) || newUrl.trimmed().isEmpty())
        return false;
    for (Details::Tracker &t : state(id).trackers)
    {
        if (t.url == oldUrl)
        {
            t.url = newUrl.trimmed();
            m_actionLog << QStringLiteral("editTracker %1 -> %2").arg(oldUrl, t.url);
            return true;
        }
    }
    return false;
}

void DemoDetailsSource::removeTrackers(const QString &id, const QStringList &urls)
{
    if (!contains(id))
        return;
    QList<Details::Tracker> &list = state(id).trackers;
    list.removeIf([&urls](const Details::Tracker &t) { return !t.pseudo && urls.contains(t.url); });
    m_actionLog << QStringLiteral("removeTrackers %1").arg(urls.join(u','));
}

void DemoDetailsSource::reannounce(const QString &id, const QStringList &urls)
{
    Q_UNUSED(id);
    m_actionLog << QStringLiteral("reannounce %1").arg(urls.isEmpty() ? QStringLiteral("all") : urls.join(u','));
}

int DemoDetailsSource::addPeers(const QString &id, const QStringList &endpoints)
{
    if (!contains(id))
        return 0;
    int n = 0;
    for (const QString &e : endpoints)
    {
        if (e.contains(u':') && e.section(u':', -1).toInt() > 0)
        {
            state(id).extraPeers << e.trimmed();
            ++n;
        }
    }
    m_actionLog << QStringLiteral("addPeers %1").arg(n);
    return n;
}

void DemoDetailsSource::banPeers(const QStringList &endpoints)
{
    for (const QString &e : endpoints)
        m_banned << e.section(u':', 0, -2);
    m_actionLog << QStringLiteral("ban %1").arg(endpoints.join(u','));
}

void DemoDetailsSource::addWebSeeds(const QString &id, const QStringList &urls)
{
    if (!contains(id))
        return;
    for (const QString &u : urls)
    {
        if (!u.trimmed().isEmpty())
            state(id).webSeeds.append({u.trimmed()});
    }
    m_actionLog << QStringLiteral("addWebSeeds %1").arg(urls.join(u','));
}

void DemoDetailsSource::removeWebSeeds(const QString &id, const QStringList &urls)
{
    if (!contains(id))
        return;
    state(id).webSeeds.removeIf([&urls](const Details::WebSeed &w) { return urls.contains(w.url); });
    m_actionLog << QStringLiteral("removeWebSeeds %1").arg(urls.join(u','));
}

void DemoDetailsSource::setFilePriority(const QString &id, const QList<int> &fileIndexes, const Details::Priority priority)
{
    if (!contains(id))
        return;
    for (Details::File &f : state(id).files)
    {
        if (fileIndexes.contains(f.index))
            f.priority = priority;
    }
    m_actionLog << QStringLiteral("priority %1 -> %2").arg(fileIndexes.size()).arg(static_cast<int>(priority));
}

bool DemoDetailsSource::renameFile(const QString &id, const QString &oldPath, const QString &newPath, const bool isFolder, QString *error)
{
    if (!contains(id) || newPath.isEmpty() || (oldPath == newPath))
        return false;
    State &s = state(id);
    for (const Details::File &f : s.files)
    {
        if (f.path == newPath)
        {
            if (error)
                *error = QStringLiteral("The file already exists: '%1'.").arg(newPath);
            return false;
        }
    }
    bool any = false;
    for (Details::File &f : s.files)
    {
        if (!isFolder && (f.path == oldPath))
        {
            f.path = newPath;
            any = true;
        }
        else if (isFolder && f.path.startsWith(oldPath + u'/'))
        {
            f.path = newPath + f.path.mid(oldPath.size());
            any = true;
        }
    }
    if (!any && error)
        *error = QStringLiteral("No such file: '%1'.").arg(oldPath);
    m_actionLog << QStringLiteral("rename %1 -> %2").arg(oldPath, newPath);
    return any;
}

QString DemoDetailsSource::absolutePath(const QString &id, const QString &relativePath) const
{
    return contains(id) ? QStringLiteral("C:/Users/you/Downloads/") + relativePath : QString();
}

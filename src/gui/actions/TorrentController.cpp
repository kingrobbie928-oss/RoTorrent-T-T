// SPDX-License-Identifier: GPL-2.0-or-later
#include "TorrentController.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>

#include "base/bittorrent/infohash.h"
#include "base/global.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/logger.h"
#include "base/path.h"

#include <QSet>

TorrentController::TorrentController(BitTorrent::Session *session, QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_queueing(session->isQueueingSystemEnabled())
{
    connect(session, &BitTorrent::Session::statsUpdated, this, &TorrentController::refreshQueueing);
}

void TorrentController::refreshQueueing()
{
    const bool now = m_session->isQueueingSystemEnabled();
    if (now == m_queueing)
        return;
    m_queueing = now;
    emit queueingChanged(now);
}

bool TorrentController::queueingEnabled() const
{
    return m_session->isQueueingSystemEnabled();
}

BitTorrent::Torrent *TorrentController::torrent(const QString &id) const
{
    const auto tid = BitTorrent::TorrentID::fromString(id);
    return tid.isValid() ? m_session->getTorrent(tid) : nullptr;
}

TorrentFacts TorrentController::facts(const QString &id) const
{
    TorrentFacts f;
    f.id = id;
    const BitTorrent::Torrent *t = torrent(id);
    if (!t)
        return f;
    f.category = t->category();
    f.hasMetadata = t->hasMetadata();
    f.finished = t->isFinished();
    f.stopped = t->isStopped();
    f.forced = t->isForced();
    f.errored = t->isErrored();
    f.missingFiles = t->hasMissingFiles();
    f.checking = t->isChecking();
    f.queued = t->isQueued();
    f.autoTMM = t->isAutoTMMEnabled();
    f.sequential = t->isSequentialDownload();
    f.firstLast = t->hasFirstLastPiecePriority();
    f.superSeeding = t->superSeeding();
    f.hasInfoHashV1 = t->infoHash().v1().isValid();
    f.hasInfoHashV2 = t->infoHash().v2().isValid();
    return f;
}

QList<TorrentFacts> TorrentController::factsFor(const QStringList &ids) const
{
    QList<TorrentFacts> out;
    for (const QString &id : ids)
        if (torrent(id))
            out.append(facts(id));
    return out;
}

void TorrentController::resume(const QStringList &ids)
{
    for (const QString &id : ids)
        if (auto *t = torrent(id))
            t->start();
}

void TorrentController::pause(const QStringList &ids)
{
    for (const QString &id : ids)
        if (auto *t = torrent(id))
            t->stop();
}

void TorrentController::forceResume(const QStringList &ids)
{
    for (const QString &id : ids)
        if (auto *t = torrent(id))
            t->start(BitTorrent::TorrentOperatingMode::Forced);
}

void TorrentController::remove(const QStringList &ids, const bool removeContent)
{
    const auto option = removeContent ? BitTorrent::TorrentRemoveOption::RemoveContent
                                      : BitTorrent::TorrentRemoveOption::KeepContent;
    for (const QString &id : ids)
    {
        const auto tid = BitTorrent::TorrentID::fromString(id);
        if (tid.isValid())
            m_session->removeTorrent(tid, option);
    }
}

void TorrentController::moveInQueue(const QStringList &ids, const QueueMove where)
{
    if (!queueingEnabled())
        return;
    QList<BitTorrent::TorrentID> tids;
    for (const QString &id : ids)
    {
        const auto tid = BitTorrent::TorrentID::fromString(id);
        if (tid.isValid())
            tids.append(tid);
    }
    switch (where)
    {
    case QueueMove::Top: m_session->topTorrentsQueuePos(tids); break;
    case QueueMove::Up: m_session->increaseTorrentsQueuePos(tids); break;
    case QueueMove::Down: m_session->decreaseTorrentsQueuePos(tids); break;
    case QueueMove::Bottom: m_session->bottomTorrentsQueuePos(tids); break;
    }
}

void TorrentController::setLocation(const QStringList &ids, const QString &folder)
{
    const Path newLocation {folder};
    if (!newLocation.exists())
        return;
    for (const QString &id : ids)
    {
        if (auto *t = torrent(id))
        {
            LogMsg(tr("Move torrent: \"%1\". From \"%2\" to \"%3\"")
                   .arg(t->name(), t->savePath().toString(), newLocation.toString()));
            t->setAutoTMMEnabled(false);
            t->setSavePath(newLocation);
        }
    }
}

bool TorrentController::rename(const QString &id, const QString &name)
{
    auto *t = torrent(id);
    if (!t || name.trimmed().isEmpty())
        return false;
    t->setName(name.trimmed());
    return true;
}

bool TorrentController::setCategory(const QStringList &ids, const QString &category)
{
    bool ok = true;
    for (const QString &id : ids)
        if (auto *t = torrent(id))
            ok = t->setCategory(category) && ok;
    return ok;
}

void TorrentController::setAutoTMM(const QStringList &ids, const bool on)
{
    for (const QString &id : ids)
        if (auto *t = torrent(id))
            t->setAutoTMMEnabled(on);
}

void TorrentController::setSequential(const QStringList &ids, const bool on)
{
    for (const QString &id : ids)
        if (auto *t = torrent(id); t && !t->isFinished())
            t->setSequentialDownload(on);
}

void TorrentController::setFirstLast(const QStringList &ids, const bool on)
{
    for (const QString &id : ids)
        if (auto *t = torrent(id); t && !t->isFinished())
            t->setFirstLastPiecePriority(on);
}

void TorrentController::setSuperSeeding(const QStringList &ids, const bool on)
{
    for (const QString &id : ids)
        if (auto *t = torrent(id); t && t->isFinished() && t->hasMetadata())
            t->setSuperSeeding(on);
}

void TorrentController::recheck(const QStringList &ids)
{
    for (const QString &id : ids)
        if (auto *t = torrent(id); t && t->hasMetadata())
            t->forceRecheck();
}

void TorrentController::reannounce(const QStringList &ids)
{
    for (const QString &id : ids)
        if (auto *t = torrent(id))
            t->forceReannounce();
}

QString TorrentController::copyText(const QStringList &ids, const CopyKind kind) const
{
    QStringList out;
    for (const QString &id : ids)
    {
        const BitTorrent::Torrent *t = torrent(id);
        if (!t)
            continue;
        switch (kind)
        {
        case CopyKind::Name: out << t->name(); break;
        case CopyKind::InfoHashV1:
            if (t->infoHash().v1().isValid())
                out << t->infoHash().v1().toString();
            break;
        case CopyKind::InfoHashV2:
            if (t->infoHash().v2().isValid())
                out << t->infoHash().v2().toString();
            break;
        case CopyKind::MagnetLink: out << t->createMagnetURI(); break;
        case CopyKind::TorrentId: out << t->id().toString(); break;
        case CopyKind::Comment: out << t->comment(); break;
        case CopyKind::ContentPath: out << t->contentPath().toString(); break;
        }
    }
    return out.join(QLatin1Char('\n'));
}

QString TorrentController::trackerUrlsForHost(const QStringList &ids, const QString &host) const
{
    QStringList out;
    for (const QString &id : ids)
    {
        const BitTorrent::Torrent *t = torrent(id);
        if (!t)
            continue;
        for (const BitTorrent::TrackerEntryStatus &entry : t->trackers())
        {
            const QUrl url(entry.url);
            const bool real = (url.scheme() == u"http") || (url.scheme() == u"https") || (url.scheme() == u"udp");
            if (real && (url.host() == host) && !out.contains(entry.url))
                out << entry.url;
        }
    }
    return out.join(QLatin1Char('\n'));
}

QString TorrentController::destinationFolder(const QString &id) const
{
    const BitTorrent::Torrent *t = torrent(id);
    if (!t)
        return {};
    const Path content = t->contentPath();
    return (content.exists() ? content.parentPath() : t->savePath()).toString();
}

void TorrentController::openDestination(const QStringList &ids) const
{
    for (const QString &id : ids)
    {
        const QString folder = destinationFolder(id);
        if (!folder.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    }
}

QString TorrentController::suggestedExportName(const QString &id) const
{
    const BitTorrent::Torrent *t = torrent(id);
    return t ? (t->name() + u".torrent"_s) : QString();
}

bool TorrentController::exportTorrent(const QString &id, const QString &filePath, QString *error) const
{
    const BitTorrent::Torrent *t = torrent(id);
    if (!t)
        return false;
    const auto result = t->exportToFile(Path(filePath));
    if (!result)
    {
        if (error)
            *error = result.error();
        return false;
    }
    return true;
}

QStringList TorrentController::categories() const
{
    QStringList list = m_session->categories();
    list.sort(Qt::CaseInsensitive);
    return list;
}

QString TorrentController::categorySavePath(const QString &name) const
{
    return m_session->categoryOptions(name).savePath.toString();
}

bool TorrentController::addCategory(const QString &name, const QString &savePath)
{
    BitTorrent::CategoryOptions options;
    options.savePath = Path(savePath);
    const bool ok = m_session->addCategory(name, options);
    if (ok)
        emit categoriesChanged();
    return ok;
}

bool TorrentController::editCategory(const QString &name, const QString &savePath)
{
    BitTorrent::CategoryOptions options = m_session->categoryOptions(name);
    options.savePath = Path(savePath);
    const bool ok = m_session->setCategoryOptions(name, options);
    if (ok)
        emit categoriesChanged();
    return ok;
}

bool TorrentController::removeCategory(const QString &name)
{
    const bool ok = m_session->removeCategory(name);
    if (ok)
        emit categoriesChanged();
    return ok;
}

int TorrentController::removeUnusedCategories()
{
    QSet<QString> used;
    for (const BitTorrent::Torrent *t : m_session->torrents())
        used.insert(t->category());
    int removed = 0;
    for (const QString &name : m_session->categories())
        if (!used.contains(name) && m_session->removeCategory(name))
            ++removed;
    if (removed > 0)
        emit categoriesChanged();
    return removed;
}

void TorrentController::pauseSession()
{
    if (m_sessionPaused)
        return;
    m_previous.clear();
    for (BitTorrent::Torrent *t : m_session->torrents())
    {
        if (t->isStopped())
            continue;
        m_previous.insert(t->id().toString(), t->isForced());
        t->stop();
    }
    m_sessionPaused = true;
    emit sessionPausedChanged(true);
}

void TorrentController::resumeSession()
{
    if (!m_sessionPaused)
        return;
    for (auto it = m_previous.cbegin(); it != m_previous.cend(); ++it)
        if (auto *t = torrent(it.key()))
            t->start(it.value() ? BitTorrent::TorrentOperatingMode::Forced : BitTorrent::TorrentOperatingMode::AutoManaged);
    m_previous.clear();
    m_sessionPaused = false;
    emit sessionPausedChanged(false);
}

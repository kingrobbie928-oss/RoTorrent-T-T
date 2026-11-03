// SPDX-License-Identifier: GPL-2.0-or-later
// Engine-facing commands for the GUI (handoff 9: Resume/Pause/Force Resume, Remove, queue, categories,
// ATM, location, rename, options, copy, export, Pause/Resume Session). Everything takes torrent ids
// (info-hash text, as TorrentRow::id) so views never hold engine objects.
// Licensed GPL-2.0-or-later (derived from qBittorrent's TransferListWidget).
#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>

#include "ActionRules.h"

namespace BitTorrent
{
    class Session;
    class Torrent;
}

class Path;

class TorrentController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentController)

public:
    enum class QueueMove { Top, Up, Down, Bottom };
    enum class CopyKind { Name, InfoHashV1, InfoHashV2, MagnetLink, TorrentId, Comment, ContentPath };

    explicit TorrentController(BitTorrent::Session *session, QObject *parent = nullptr);

    BitTorrent::Session *session() const { return m_session; }
    bool queueingEnabled() const;
    // The engine has no signal for this setting; it is checked on every session stats tick, and the
    // Options dialog calls this right after Apply so the UI follows immediately.
    void refreshQueueing();

    TorrentFacts facts(const QString &id) const;
    QList<TorrentFacts> factsFor(const QStringList &ids) const;
    CommandPlan planFor(const QStringList &ids) const { return ActionRules::plan(factsFor(ids)); }

    void resume(const QStringList &ids);          // start(AutoManaged): upstream "Start"
    void pause(const QStringList &ids);           // stop(): upstream "Stop"
    void forceResume(const QStringList &ids);
    void remove(const QStringList &ids, bool removeContent);
    void moveInQueue(const QStringList &ids, QueueMove where);

    void setLocation(const QStringList &ids, const QString &folder);
    bool rename(const QString &id, const QString &name);
    bool setCategory(const QStringList &ids, const QString &category);
    void setAutoTMM(const QStringList &ids, bool on);
    void setSequential(const QStringList &ids, bool on);
    void setFirstLast(const QStringList &ids, bool on);
    void setSuperSeeding(const QStringList &ids, bool on);
    void recheck(const QStringList &ids);
    void reannounce(const QStringList &ids);

    QString copyText(const QStringList &ids, CopyKind kind) const;
    // Full announce URLs of the torrents in `ids` whose tracker host is `host` (the sidebar's Trackers
    // row). Distinct, in first-seen order, one per line. DHT / PeX / LSD pseudo-entries are skipped.
    QString trackerUrlsForHost(const QStringList &ids, const QString &host) const;
    QString destinationFolder(const QString &id) const;   // folder to open in Explorer
    void openDestination(const QStringList &ids) const;
    bool exportTorrent(const QString &id, const QString &filePath, QString *error = nullptr) const;
    QString suggestedExportName(const QString &id) const;

    // Categories (engine side of the sidebar menus).
    QStringList categories() const;
    bool addCategory(const QString &name, const QString &savePath);
    bool editCategory(const QString &name, const QString &savePath);
    QString categorySavePath(const QString &name) const;
    bool removeCategory(const QString &name);
    int removeUnusedCategories();

    // Pause Session / Resume Session (handoff 8.4): stop every active torrent, remember each one's
    // mode, restore on resume.
    bool sessionPaused() const { return m_sessionPaused; }
    void pauseSession();
    void resumeSession();

signals:
    void sessionPausedChanged(bool paused);
    void categoriesChanged();
    void queueingChanged(bool enabled);

private:
    BitTorrent::Torrent *torrent(const QString &id) const;

    BitTorrent::Session *m_session;
    QHash<QString, bool> m_previous;    // id -> was forced, for torrents stopped by pauseSession()
    bool m_sessionPaused = false;
    bool m_queueing = false;
};

// SPDX-License-Identifier: GPL-2.0-or-later
// DetailsSource on a live BitTorrent::Session. Reference: upstream's PropertiesWidget,
// TrackerListModel, PeerListWidget and TorrentContentModel. Licensed GPL-2.0-or-later.
#pragma once

#include <QHash>

#include "DetailsSource.h"

namespace BitTorrent
{
    class Session;
    class Torrent;
}

class SessionDetailsSource final : public DetailsSource
{
    Q_OBJECT

public:
    explicit SessionDetailsSource(BitTorrent::Session *session, QObject *parent = nullptr);

    bool contains(const QString &id) const override;
    Details::General general(const QString &id) const override;
    QList<Details::Tracker> trackers(const QString &id) const override;
    QList<Details::WebSeed> webSeeds(const QString &id) const override;
    QList<Details::File> files(const QString &id) const override;
    void requestPeers(const QString &id) override;
    void requestAvailability(const QString &id) override;

    void addTrackers(const QString &id, const QStringList &urls) override;
    bool editTracker(const QString &id, const QString &oldUrl, const QString &newUrl) override;
    void removeTrackers(const QString &id, const QStringList &urls) override;
    void reannounce(const QString &id, const QStringList &urls) override;
    int addPeers(const QString &id, const QStringList &endpoints) override;
    void banPeers(const QStringList &endpoints) override;
    void addWebSeeds(const QString &id, const QStringList &urls) override;
    void removeWebSeeds(const QString &id, const QStringList &urls) override;
    void setFilePriority(const QString &id, const QList<int> &fileIndexes, Details::Priority priority) override;
    bool renameFile(const QString &id, const QString &oldPath, const QString &newPath, bool isFolder, QString *error = nullptr) override;
    QString absolutePath(const QString &id, const QString &relativePath) const override;

private:
    BitTorrent::Torrent *torrent(const QString &id) const;

    BitTorrent::Session *m_session;
    mutable QHash<QString, int> m_peerCountDHT, m_peerCountPeX, m_peerCountLSD;   // from the last peers snapshot
};

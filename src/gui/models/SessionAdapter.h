// SPDX-License-Identifier: GPL-2.0-or-later
// Feeds TransferListModel from the live BitTorrent::Session (handoff Phase 3, "live data").
// statusOf() is the only place a status is decided. Rows are plain data (TorrentRow), so the
// model and everything above it never touch engine objects.
#pragma once

#include <QHash>
#include <QObject>

#include "TorrentRow.h"

namespace BitTorrent
{
    class Session;
    class Torrent;
}

class TransferListModel;

class SessionAdapter final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SessionAdapter)

public:
    SessionAdapter(BitTorrent::Session *session, TransferListModel *model, QObject *parent = nullptr);

    static TorrentRow makeRow(const BitTorrent::Torrent *torrent, const QString &trackerHost);
    // Formatting shared with the demo source so both show identical text.
    static void fillTexts(TorrentRow &row);

    bool queueingEnabled() const;

signals:
    void ratesChanged(qint64 downBytesPerSecond, qint64 upBytesPerSecond);
    void queueingChanged(bool enabled);

private:
    void loadAll();
    void refresh(const BitTorrent::Torrent *torrent);
    void refreshMany(const QList<BitTorrent::Torrent *> &torrents);
    QString hostOf(const BitTorrent::Torrent *torrent);

    BitTorrent::Session *m_session;
    TransferListModel *m_model;
    QHash<QString, QString> m_hosts;   // torrent id -> tracker host, refreshed when trackers change
};

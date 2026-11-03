// SPDX-License-Identifier: GPL-2.0-or-later
// Interface between the details panel and whatever supplies the data (handoff 9: "Details tabs").
// Snapshots are synchronous where the engine allows it; peers and availability are asynchronous
// in libtorrent, so they are requested and delivered through signals.
// Licensed GPL-2.0-or-later.
#pragma once

#include <QObject>
#include <QStringList>

#include "DetailsData.h"

class DetailsSource : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DetailsSource)

public:
    using QObject::QObject;

    virtual bool contains(const QString &id) const = 0;
    virtual Details::General general(const QString &id) const = 0;
    virtual QList<Details::Tracker> trackers(const QString &id) const = 0;
    virtual QList<Details::WebSeed> webSeeds(const QString &id) const = 0;
    virtual QList<Details::File> files(const QString &id) const = 0;

    // Asynchronous data. The answer arrives through peersReady / availabilityReady.
    virtual void requestPeers(const QString &id) = 0;
    virtual void requestAvailability(const QString &id) = 0;

    // ---- actions (return false / 0 when the engine refuses) ----
    virtual void addTrackers(const QString &id, const QStringList &urls) = 0;
    virtual bool editTracker(const QString &id, const QString &oldUrl, const QString &newUrl) = 0;
    virtual void removeTrackers(const QString &id, const QStringList &urls) = 0;
    virtual void reannounce(const QString &id, const QStringList &urls) = 0;   // empty list = all trackers
    virtual int addPeers(const QString &id, const QStringList &endpoints) = 0; // number accepted
    virtual void banPeers(const QStringList &endpoints) = 0;
    virtual void addWebSeeds(const QString &id, const QStringList &urls) = 0;
    virtual void removeWebSeeds(const QString &id, const QStringList &urls) = 0;
    virtual void setFilePriority(const QString &id, const QList<int> &fileIndexes, Details::Priority priority) = 0;
    virtual bool renameFile(const QString &id, const QString &oldPath, const QString &newPath, bool isFolder, QString *error = nullptr) = 0;
    // Absolute path on disk of a file (or of a folder when `path` names one); empty when unknown.
    virtual QString absolutePath(const QString &id, const QString &relativePath) const = 0;

signals:
    void peersReady(const QString &id, const QList<Details::Peer> &peers);
    void availabilityReady(const QString &id, const QList<int> &pieceAvailability, const QList<double> &fileFractions);
};

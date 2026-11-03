// SPDX-License-Identifier: GPL-2.0-or-later
// DetailsSource that invents stable data from the transfer list's rows (preview, screenshots,
// tests). Actions edit the in-memory data so the wiring can be tested without an engine.
// Licensed GPL-2.0-or-later.
#pragma once

#include <QHash>

#include "DetailsSource.h"

class TransferListModel;

class DemoDetailsSource final : public DetailsSource
{
    Q_OBJECT

public:
    explicit DemoDetailsSource(const TransferListModel *model, QObject *parent = nullptr);

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

    // What the actions did, for tests.
    QStringList banned() const { return m_banned; }
    QStringList actionLog() const { return m_actionLog; }

private:
    struct State
    {
        bool init = false;
        QList<Details::Tracker> trackers;
        QList<Details::WebSeed> webSeeds;
        QList<Details::File> files;
        QStringList extraPeers;
    };
    State &state(const QString &id) const;

    const TransferListModel *m_model;
    mutable QHash<QString, State> m_state;
    QStringList m_banned;
    QStringList m_actionLog;
};

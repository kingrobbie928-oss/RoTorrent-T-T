// SPDX-License-Identifier: GPL-2.0-or-later
// Flat table model over TorrentRow. Columns keep the demo's logical order (Name first, "#" last);
// the view moves "#" to the front. Updates are by row id so scrolling and selection survive them.
#pragma once

#include <QAbstractTableModel>
#include <QHash>
#include <QList>

#include "TorrentRow.h"

class TransferListModel final : public QAbstractTableModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TransferListModel)

public:
    enum Column { Name = 0, Progress, Status, Down, Up, Eta, Ratio, QueuePos, ColumnCount };

    enum Role
    {
        RowRole = Qt::UserRole + 1,   // const TorrentRow *
        SortRole                      // QVariant sort key for the column
    };

    explicit TransferListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    const TorrentRow &row(int i) const { return m_rows.at(i); }
    int indexOfId(const QString &id) const { return m_index.value(id, -1); }

    void setRows(QList<TorrentRow> rows);          // replaces everything
    void upsert(const TorrentRow &row);            // adds or updates one
    void upsertMany(const QList<TorrentRow> &rows);
    void removeId(const QString &id);
    void clear();

    // Displayed text of the "#" column for a row: "*" for finished torrents, otherwise 1-based.
    static QString queueText(const TorrentRow &row);

signals:
    // Emitted (coalesced by the receiver) whenever rows were added, removed or changed.
    void rowsChanged();

private:
    void reindex(int from = 0);

    QList<TorrentRow> m_rows;
    QHash<QString, int> m_index;
};

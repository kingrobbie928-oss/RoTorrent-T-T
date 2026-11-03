// SPDX-License-Identifier: GPL-2.0-or-later
#include "TransferListModel.h"

#include <QSet>

TransferListModel::TransferListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int TransferListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int TransferListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QString TransferListModel::queueText(const TorrentRow &row)
{
    return (row.finished() || (row.queuePos < 0)) ? QStringLiteral("*") : QString::number(row.queuePos + 1);
}

QVariant TransferListModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || (index.row() < 0) || (index.row() >= m_rows.size()))
        return {};
    const TorrentRow &r = m_rows.at(index.row());

    if (role == RowRole)
        return QVariant::fromValue(&r);

    if (role == SortRole)
    {
        switch (index.column())
        {
        case Name: return r.name.toLower();
        case Progress: return r.progress;
        case Status: return static_cast<int>(r.status);
        case Down: return r.downRate;
        case Up: return r.upRate;
        case Eta: return (r.etaSeconds < 0) ? std::numeric_limits<qint64>::max() : r.etaSeconds;
        case Ratio: return r.ratio;
        case QueuePos: return r.finished() ? std::numeric_limits<int>::max() : r.queuePos;
        }
    }
    else if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case Name: return r.name;
        case Progress: return QString(QString::number(static_cast<int>(r.progress)) + QLatin1Char('%'));
        case Status: return QString::fromLatin1(RoStatusInfo::code(r.status));
        case Down: return r.downText;
        case Up: return r.upText;
        case Eta: return r.etaText;
        case Ratio: return r.ratioText;
        case QueuePos: return queueText(r);
        }
    }
    return {};
}

QVariant TransferListModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if ((orientation != Qt::Horizontal) || (role != Qt::DisplayRole))
        return {};
    static const QStringList names = {tr("NAME"), tr("PROGRESS"), tr("STATUS"), tr("DOWN"), tr("UP"), tr("ETA"), tr("RATIO"), tr("#")};
    return names.value(section);
}

void TransferListModel::reindex(const int from)
{
    for (int i = from; i < m_rows.size(); ++i)
        m_index.insert(m_rows.at(i).id, i);
}

void TransferListModel::setRows(QList<TorrentRow> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    m_index.clear();
    reindex();
    endResetModel();
    emit rowsChanged();
}

void TransferListModel::upsert(const TorrentRow &row)
{
    upsertMany({row});
}

void TransferListModel::upsertMany(const QList<TorrentRow> &rows)
{
    if (rows.isEmpty())
        return;

    QList<TorrentRow> added;
    for (const TorrentRow &r : rows)
    {
        const int i = m_index.value(r.id, -1);
        if (i < 0)
        {
            added.append(r);
            continue;
        }
        m_rows[i] = r;
        emit dataChanged(index(i, 0), index(i, ColumnCount - 1));
    }
    if (!added.isEmpty())
    {
        const int first = static_cast<int>(m_rows.size());
        beginInsertRows({}, first, first + static_cast<int>(added.size()) - 1);
        m_rows.append(added);
        reindex(first);
        endInsertRows();
    }
    emit rowsChanged();
}

void TransferListModel::removeId(const QString &id)
{
    const int i = m_index.value(id, -1);
    if (i < 0)
        return;
    beginRemoveRows({}, i, i);
    m_rows.removeAt(i);
    m_index.remove(id);
    reindex(i);
    endRemoveRows();
    emit rowsChanged();
}

void TransferListModel::clear()
{
    setRows({});
}

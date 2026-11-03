// SPDX-License-Identifier: GPL-2.0-or-later
#include "TorrentFilterProxy.h"

#include "TransferListModel.h"

bool RowFilter::statusMatches(const StatusKind kind, const RoStatus s)
{
    switch (kind)
    {
    case StatusKind::All: return true;
    case StatusKind::Downloading: return s == RoStatus::Downloading;
    case StatusKind::Seeding: return s == RoStatus::Seeding;
    case StatusKind::Completed: return s == RoStatus::Completed;
    case StatusKind::Paused: return s == RoStatus::Paused;
    case StatusKind::Queued: return s == RoStatus::Queued;
    case StatusKind::Checking: return s == RoStatus::Checking;
    case StatusKind::Stalled: return s == RoStatus::Stalled;
    case StatusKind::Error: return RoStatusInfo::countsAsErrorInSidebar(s);
    }
    return false;
}

bool RowFilter::matches(const TorrentRow &row) const
{
    if (!statusMatches(status, row.status))
        return false;
    if (category && (row.category != *category))
        return false;
    if (tracker && (row.trackerHost != *tracker))
        return false;
    if (!search.isEmpty())
    {
        const QStringList terms = search.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QString &term : terms)
        {
            if (!row.name.contains(term, Qt::CaseInsensitive))
                return false;
        }
    }
    return true;
}

TorrentFilterProxy::TorrentFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSortRole(TransferListModel::SortRole);
    setDynamicSortFilter(true);
}

void TorrentFilterProxy::setFilter(const RowFilter &filter)
{
    if (m_filter == filter)
        return;
    m_filter = filter;
    invalidateFilter();   // beginFilterChange() is Qt 6.9+; the minimum is 6.6
}

void TorrentFilterProxy::setSearch(const QString &text)
{
    RowFilter f = m_filter;
    f.search = text.trimmed();
    setFilter(f);
}

bool TorrentFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex &sourceParent) const
{
    const auto *src = qobject_cast<const TransferListModel *>(sourceModel());
    return src && m_filter.matches(src->row(sourceRow)) && !sourceParent.isValid();
}

bool TorrentFilterProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const QVariant a = left.data(TransferListModel::SortRole);
    const QVariant b = right.data(TransferListModel::SortRole);
    if (a.metaType().id() == QMetaType::QString)
        return QString::localeAwareCompare(a.toString(), b.toString()) < 0;
    return a.toDouble() < b.toDouble();
}

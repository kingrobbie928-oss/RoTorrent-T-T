// SPDX-License-Identifier: GPL-2.0-or-later
// Filter + sort for the transfer list. ONE predicate (matches) is used for the table AND for the
// sidebar counts, so a sidebar number always equals the rows that filter shows (handoff Phase 3).
#pragma once

#include <optional>

#include <QSortFilterProxyModel>

#include "TorrentRow.h"

struct RowFilter
{
    enum class StatusKind { All, Downloading, Seeding, Completed, Paused, Queued, Checking, Stalled, Error };

    StatusKind status = StatusKind::All;
    std::optional<QString> category;     // set + empty string = "Uncategorized"
    std::optional<QString> tracker;      // set + empty string = "Trackerless"
    QString search;                      // whitespace separated terms, all must match the name

    bool operator==(const RowFilter &) const = default;

    // statusOf() is the only place a status is decided; this maps the sidebar row to it.
    static bool statusMatches(StatusKind kind, RoStatus s);
    bool matches(const TorrentRow &row) const;
};

class TorrentFilterProxy final : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentFilterProxy)

public:
    explicit TorrentFilterProxy(QObject *parent = nullptr);

    void setFilter(const RowFilter &filter);
    const RowFilter &filter() const { return m_filter; }
    void setSearch(const QString &text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    RowFilter m_filter;
};

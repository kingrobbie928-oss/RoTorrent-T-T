// SPDX-License-Identifier: GPL-2.0-or-later
// Paints a transfer-list row exactly like the demo's html <tr> (handoff 8.7): wash, progress bar,
// status badge, name tile, Vibrant washes, Checking stripes, selection outline.
#pragma once

#include <QBrush>

#include "models/TransferListModel.h"
#include <QStyledItemDelegate>

#include "models/TorrentRow.h"
#include "theme/Palette.h"

class TorrentDelegate final : public QStyledItemDelegate
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentDelegate)

public:
    explicit TorrentDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // Live animation clock in seconds, advanced by the view's timer (shine 1.6 s, wave 2.2 s, stripes 24 px/s).
    void setTime(const double seconds) { m_time = seconds; }
    // Freezes every animation at one phase (0..1). Used for screenshots and tests; the demo's
    // reference images were rendered at 0.35.
    void setFixedPhase(const double phase) { m_fixed = phase; m_hasFixed = true; }
    void clearFixedPhase() { m_hasFixed = false; }

    // Does this row animate right now (Downloading shine, Vibrant wave, Checking stripes)?
    static bool needsAnimation(const TorrentRow &row, bool vibrant);

    static constexpr int BADGE_ICON = 14;
    static QString statusLabel(RoStatus status);
    // Row wash / solid gradient for a status (also the sidebar's Completed pill).
    static QBrush washBrush(const RoPalette &pal, bool light, RoStatus status, const QRectF &rect);

    // Minimum content width of a column for this row (used by the view's responsive columns).
    static qreal minContentWidth(const TorrentRow &row, int column, bool vibrant);

private:
    qreal cellHeight(const TorrentRow &row, int column, qreal columnWidth, bool vibrant) const;
    static bool rowHovered(const QStyleOptionViewItem &option, const QModelIndex &index);
    struct Tint { QColor wash; QColor fg; bool valid = false; };
    static Tint tint(const RoPalette &pal, RoStatus status);

    double stripePhase() const { return m_hasFixed ? m_fixed : m_time; }
    double shinePhase() const { return m_hasFixed ? m_fixed : m_time / 1.6; }
    double wavePhase() const { return m_hasFixed ? m_fixed : m_time / 2.2; }

    double m_time = 0;
    double m_fixed = 0;
    bool m_hasFixed = false;
};

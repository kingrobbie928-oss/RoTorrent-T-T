// SPDX-License-Identifier: GPL-2.0-or-later
// Small widgets and formatters shared by the details tabs (handoff 8.9): the pieces / availability
// bars, the slim progress-bar cell delegate, the heading and label factories, the table factory
// and the value formatters. Ports of PiecesBar, BarDelegate, heading() and table() in tabs_ui.py.
// Licensed GPL-2.0-or-later.
#pragma once

#include <QBitArray>
#include <QLabel>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QWidget>

#include "DetailsData.h"

namespace DetailsFormat
{
    QString bytes(qint64 byteCount);                 // "1.5 GiB" (engine's own friendlyUnit)
    QString speed(qint64 bytesPerSecond);       // "340 KiB/s"
    QString limit(qint64 bytesPerSecond);       // "∞" when unlimited, else speed()
    QString duration(qint64 seconds);           // "2h 5m" or "∞" when unknown (seconds < 0)
    QString dateTime(const QDateTime &when);    // "2026-08-14 09:22" or "—"
    QString ratio(double value);                // two decimals, "—" when unknown
    QString percent(double fraction);           // "42.5%"
    QString statusText(RoStatus status);        // table / sidebar / details all use the same words
}

// "PROGRESS" bar (downloaded pieces, green) or "AVAILABILITY" bar (copies per piece, blue).
class PiecesBar final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PiecesBar)

public:
    enum class Kind { Progress, Availability };

    explicit PiecesBar(Kind kind, QWidget *parent = nullptr);

    void setPieces(const QBitArray &have);                 // Progress
    void setAvailability(const QList<int> &copies);        // Availability
    void clear();

    // Test access: fraction (0..1) of the pieces drawn in pixel column `x` of a bar `width` wide.
    static double columnValue(const QBitArray &have, int x, int width);
    static double columnAvailability(const QList<int> &copies, int maxCopies, int x, int width);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Kind m_kind;
    QBitArray m_have;
    QList<int> m_copies;
    int m_maxCopies = 0;
};

// Slim 6 px bar plus percentage; the cell's Qt::UserRole holds 0..100 (-1 = draw nothing).
class BarDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit BarDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

namespace DetailsUi
{
    QLabel *heading(const QString &text);                       // objectName dHeading, uppercase, spaced
    QLabel *fieldLabel(const QString &text);                    // objectName dLabel, uppercase, spaced
    QLabel *emptyLabel(const QString &text);                    // objectName dEmpty
    // objectName dTable; headers are shown uppercase; widths are the base widths from the spec.
    QTableWidget *makeTable(const QStringList &headers, const QList<int> &widths);
    QTableWidgetItem *cell(const QString &text, const QColor &colour = {}, Qt::Alignment align = Qt::AlignLeft);
}

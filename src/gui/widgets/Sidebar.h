// SPDX-License-Identifier: GPL-2.0-or-later
// Sidebar (handoff 8.6): collapsible STATUS / CATEGORIES / TRACKERS tree, live counts, the speed
// card with sparkline, and a drag-resizable right edge (160-420 px).
#pragma once

#include <optional>

#include <QFrame>
#include <QStyledItemDelegate>
#include <QTreeWidget>

#include "models/TorrentFilterProxy.h"

class QLabel;
class QTreeWidgetItem;
class SparkLine;
class TransferListModel;

class SideTree final : public QTreeWidget
{
    Q_OBJECT
public:
    using QTreeWidget::QTreeWidget;

protected:
    // Qt only repaints the hovered row's rectangle when the hover moves; the pill's anti-aliased edge
    // can land outside it at fractional DPI and stay as a stray line. Repaint the whole (small) viewport.
    bool viewportEvent(QEvent *event) override;
};

class SideDelegate final : public QStyledItemDelegate
{
    Q_OBJECT
public:
    enum Role
    {
        KindRole = Qt::UserRole,      // "section" | "item"
        ColourRole = Qt::UserRole + 1,    // palette member name for status rows ("seed", "info", ...)
        WashRole = Qt::UserRole + 2,      // wash palette member name, or "completed"
        CollapsedRole = Qt::UserRole + 3,
        IconRole = Qt::UserRole + 4,
        CountRole = Qt::UserRole + 5,
        ActiveRole = Qt::UserRole + 6,
        GroupRole = Qt::UserRole + 7,     // "status" | "category" | "tracker"
        PayloadRole = Qt::UserRole + 8    // StatusKind (int) or category / tracker name
    };

    explicit SideDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

class Sidebar final : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Sidebar)

public:
    explicit Sidebar(TransferListModel *source, QWidget *parent = nullptr);

    SideTree *tree() const { return m_tree; }
    // Filter chosen in the sidebar (search text is not part of it).
    RowFilter currentFilter() const { return m_filter; }
    QString currentLabel() const { return m_label; }

    // Categories known to the engine; rows are shown even when no torrent uses them yet.
    void setKnownCategories(const QStringList &categories);
    void resetFilter();                         // back to "All torrents", no category, no tracker
    void refreshCounts();                       // recompute everything from the model now
    void setRates(qint64 downBytes, qint64 upBytes);   // live speed card values + one sparkline sample
    // Fixed card contents for screenshots and the demo (the kit's 24-point series, "4.2 MB/s" / "1.1 MB/s").
    void showDemoSpeedCard();
    SparkLine *spark() const { return m_spark; }

    static constexpr int DEFAULT_WIDTH = 210;
    static constexpr int MIN_WIDTH = 160;
    static constexpr int MAX_WIDTH = 420;
    static constexpr int HANDLE_WIDTH = 10;

signals:
    void filterChanged(const RowFilter &filter, const QString &label);
    void speedCardClicked();
    void widthDragged();
    // Right-click: group is "status", "category", "tracker", "categoryHeader" or "empty";
    // payload is the StatusKind (int) or the category / tracker name.
    void menuRequested(const QPoint &globalPos, const QString &group, const QVariant &payload);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Entry { QString name; int count = 0; };

    void rebuild();                             // rebuilds category and tracker rows if their sets changed
    void onItemClicked(QTreeWidgetItem *item);
    void updateActiveMarks();
    QTreeWidgetItem *addSection(const QString &name, bool collapsed);
    QTreeWidgetItem *addItem(QTreeWidgetItem *parent, const QString &name, const QString &icon, int count
                             , const QString &group, const QVariant &payload
                             , const QString &colour = {}, const QString &wash = {});

    TransferListModel *m_source;
    SideTree *m_tree = nullptr;
    QFrame *m_card = nullptr;
    QLabel *m_downValue = nullptr;
    QLabel *m_upValue = nullptr;
    SparkLine *m_spark = nullptr;
    QWidget *m_handle = nullptr;
    QTreeWidgetItem *m_statusSection = nullptr;
    QTreeWidgetItem *m_categorySection = nullptr;
    QTreeWidgetItem *m_trackerSection = nullptr;
    RowFilter m_filter;
    QString m_label;
    QStringList m_knownCategories;
    QStringList m_shownCategories;
    QStringList m_shownTrackers;
    bool m_categoryCollapsed = false;
    bool m_trackerCollapsed = true;
};

class SparkLine final : public QWidget
{
    Q_OBJECT
public:
    explicit SparkLine(QWidget *parent = nullptr);
    void addSample(qint64 downBytes, qint64 upBytes);
    void setSamples(const QList<double> &down, const QList<double> &up);   // already normalised 0..100
    int sampleCount() const { return static_cast<int>(m_down.size()); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<double> m_down;
    QList<double> m_up;
    QList<qint64> m_rawDown;
    QList<qint64> m_rawUp;
};

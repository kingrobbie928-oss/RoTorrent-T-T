// SPDX-License-Identifier: GPL-2.0-or-later
// The six tabs of the details panel (handoff 8.9): General, Trackers, Peers, HTTP Sources, Content, Log.
// Every tab reads a DetailsSource and repaints on refresh(); none of them touches the engine.
// Licensed GPL-2.0-or-later.
#pragma once

#include <QHash>
#include <QSet>
#include <QTableWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QWidget>

#include "DetailsData.h"

class DetailsSource;
class LogBuffer;
class PiecesBar;
class QLabel;
class QPushButton;
class RoSearchBox;
class QStackedLayout;

class DetailsTab : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DetailsTab)

public:
    DetailsTab(DetailsSource *source, QWidget *parent);

    QString torrentId() const { return m_id; }
    // The selection changed: forget per-torrent state and load the new torrent.
    virtual void setTorrent(const QString &id);
    // Timer tick (the list's refresh interval) or an explicit request: re-read and repaint.
    virtual void refresh() = 0;

protected:
    DetailsSource *m_source;
    QString m_id;
};

// ---------------------------------------------------------------------------------------------
class GeneralTab final : public DetailsTab
{
    Q_OBJECT

public:
    explicit GeneralTab(DetailsSource *source, QWidget *parent = nullptr);
    void setTorrent(const QString &id) override;
    void refresh() override;

    // Test access.
    QString value(const QString &key) const;
    QLabel *badge() const { return m_badge; }
    PiecesBar *progressBar() const { return m_progressBar; }
    PiecesBar *availabilityBar() const { return m_availabilityBar; }
    QStringList keys() const { return m_order; }

private:
    void addField(QLayout *grid, int row, int col, const QString &key, bool wide);
    void setValue(const QString &key, const QString &text);
    void onAvailability(const QString &id, const QList<int> &pieces);

    QLabel *m_name = nullptr;
    QLabel *m_badge = nullptr;
    PiecesBar *m_progressBar = nullptr;
    PiecesBar *m_availabilityBar = nullptr;
    QHash<QString, QLabel *> m_values;
    QStringList m_order;
    int m_tick = 0;
};

// ---------------------------------------------------------------------------------------------
class TrackersTab final : public DetailsTab
{
    Q_OBJECT

public:
    static constexpr int ColumnCount = 11;
    explicit TrackersTab(DetailsSource *source, QWidget *parent = nullptr);
    void refresh() override;

    QTableWidget *table() const { return m_table; }
    QStringList selectedUrls() const;           // real trackers only
    // Menu actions, public so tests and the main window can drive them.
    void addTrackersInteractive();
    void editSelectedInteractive();
    void removeSelected();
    void reannounceSelected();
    void reannounceAll();
    void copySelectedUrl();
    QMenu *buildMenu();

private:
    QTableWidget *m_table;
};

class PeersTab final : public DetailsTab
{
    Q_OBJECT

public:
    static constexpr int ColumnCount = 12;
    explicit PeersTab(DetailsSource *source, QWidget *parent = nullptr);
    void setTorrent(const QString &id) override;
    void refresh() override;

    QTableWidget *table() const { return m_table; }
    QStringList selectedEndpoints() const;
    bool showingEmptyState() const;
    void addPeersInteractive();
    void banSelectedInteractive(bool confirm = true);
    void copySelectedEndpoint();
    QMenu *buildMenu();

private:
    void onPeers(const QString &id, const QList<Details::Peer> &peers);

    QTableWidget *m_table;
    QLabel *m_empty;
    QStackedLayout *m_stack;
};

class HttpTab final : public DetailsTab
{
    Q_OBJECT

public:
    explicit HttpTab(DetailsSource *source, QWidget *parent = nullptr);
    void refresh() override;

    QTableWidget *table() const { return m_table; }
    bool showingEmptyState() const;
    QStringList selectedUrls() const;
    void addInteractive();
    void removeSelected();
    void copySelected();
    QMenu *buildMenu();

private:
    QTableWidget *m_table;
    QLabel *m_empty;
    QStackedLayout *m_stack;
};

// ---------------------------------------------------------------------------------------------
class ContentTab final : public DetailsTab
{
    Q_OBJECT

public:
    explicit ContentTab(DetailsSource *source, QWidget *parent = nullptr);
    void setTorrent(const QString &id) override;
    void refresh() override;

    QTreeWidget *tree() const { return m_tree; }
    RoSearchBox *filterBox() const { return m_filter; }
    void selectAll();
    void selectNone();
    void applyFilter(const QString &text);
    void setPriorityOfSelection(Details::Priority priority);
    void renameSelectedInteractive();
    void openSelected();
    void openContainingFolder();
    QMenu *buildMenu();

    // Test access: the file indexes below an item (itself when it is a file).
    static QList<int> fileIndexesUnder(const QTreeWidgetItem *item);
    QString pathOf(const QTreeWidgetItem *item) const;

private:
    void rebuild(const QList<Details::File> &files);
    void updateValues(const QList<Details::File> &files);
    void recalcFolders();
    void onItemChanged(QTreeWidgetItem *item);
    void onAvailability(const QString &id, const QList<double> &fractions);

    QTreeWidget *m_tree;
    RoSearchBox *m_filter;
    QString m_signature;                       // joined paths: when it changes the tree is rebuilt
    QHash<int, QTreeWidgetItem *> m_fileItems;
    QList<QTreeWidgetItem *> m_folders;
    bool m_updating = false;
    QList<double> m_availability;
};

// ---------------------------------------------------------------------------------------------
class LogTab final : public DetailsTab
{
    Q_OBJECT

public:
    LogTab(DetailsSource *source, LogBuffer *buffer, QWidget *parent = nullptr);
    void refresh() override;                    // re-renders (theme change)

    int mask() const { return m_mask; }
    void setMask(int mask);                     // chips + view + persistence
    QPushButton *chip(int kind) const { return m_chips.value(kind); }
    QString plainText() const;
    QTextEdit *view() const { return m_view; }
    static QString lineHtml(const QDateTime &time, int kind, const QString &text);

private:
    void render();
    void updateChipIcons();

    LogBuffer *m_buffer;
    QTextEdit *m_view;
    QHash<int, QPushButton *> m_chips;
    int m_mask = 15;
};

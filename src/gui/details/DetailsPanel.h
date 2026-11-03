// SPDX-License-Identifier: GPL-2.0-or-later
// The details panel at the bottom of the window (handoff 8.9): a 36 px tab row that is always visible,
// and a body that opens into the clicked tab. Starts collapsed with no active tab. A 10 px strip on
// the top edge resizes it (up to 75% of the parent, snapping collapsed at 90 px). Port of DetailPanel,
// DetailTab and ResizeHandle in preview.py. Licensed GPL-2.0-or-later.
#pragma once

#include <QFrame>
#include <QTimer>

#include "DetailsTabs.h"

class DetailsSource;
class LogBuffer;
class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QToolButton;

class DetailsPanel final : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DetailsPanel)

public:
    enum class Tab { General, Trackers, Peers, Http, Content, Log };

    static constexpr int MinHeight = 90;
    static constexpr int DefaultHeight = 216;
    static constexpr int CollapsedHeight = 38;   // 1 px panel border + 36 px tab row + 1 px row border
    static constexpr int HandleHeight = 10;

    DetailsPanel(DetailsSource *source, LogBuffer *log, QWidget *parent = nullptr);

    bool isCollapsed() const { return m_collapsed; }
    bool hasActiveTab() const { return m_hasTab; }
    Tab activeTab() const { return m_tab; }
    int expandedHeight() const { return m_height; }
    QString torrentId() const { return m_id; }

    void expand(Tab tab = Tab::General);
    void collapse();
    void toggle();                          // chevron: opens on General, or collapses
    void setTorrent(const QString &id);     // empty = nothing selected
    void setRefreshInterval(int ms);        // the list's interval (Preferences "Refresh interval")
    void refreshNow();

    // Resize handling (also used by tests): applies the 90 px / 75% rules and snap-to-collapsed.
    void setPanelHeight(int height, int parentHeight);
    void finishResize();

    // Test and wiring access.
    DetailsTab *tabWidget(Tab tab) const;
    QPushButton *tabButton(Tab tab) const { return m_buttons.value(static_cast<int>(tab)); }
    QToolButton *toggleButton() const { return m_toggle; }
    QWidget *handle() const { return m_handle; }
    QStackedWidget *stack() const { return m_stack; }
    QLabel *emptyLabel() const { return m_empty; }
    GeneralTab *general() const { return m_general; }
    TrackersTab *trackers() const { return m_trackers; }
    PeersTab *peers() const { return m_peers; }
    HttpTab *http() const { return m_http; }
    ContentTab *content() const { return m_content; }
    LogTab *log() const { return m_log; }
    bool refreshTimerActive() const { return m_timer.isActive(); }

signals:
    void expandedChanged(bool expanded);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void apply();
    void showCurrentPage();
    void updateTabIcons();
    void updateToggle();

    DetailsSource *m_source;
    QFrame *m_row = nullptr;
    QToolButton *m_toggle = nullptr;
    QWidget *m_handle = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_empty = nullptr;
    QScrollArea *m_generalScroll = nullptr;
    QHash<int, QPushButton *> m_buttons;
    GeneralTab *m_general = nullptr;
    TrackersTab *m_trackers = nullptr;
    PeersTab *m_peers = nullptr;
    HttpTab *m_http = nullptr;
    ContentTab *m_content = nullptr;
    LogTab *m_log = nullptr;
    QTimer m_timer;
    QString m_id;
    Tab m_tab = Tab::General;
    bool m_hasTab = false;
    bool m_collapsed = true;
    int m_height = DefaultHeight;
};

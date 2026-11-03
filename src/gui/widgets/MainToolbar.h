// SPDX-License-Identifier: GPL-2.0-or-later
// Top toolbar (handoff 8.5): a 51 px QFrame#mainToolbar (never a QToolBar) holding a row widget
// toolRow with padding 0 14 px and 8 px gaps. Create | Pause Resume | Delete | [queue group] |
// Settings | search box | stretch | link button, Add torrent | theme + Vibrant toggles.
// Licensed GPL-2.0-or-later.
#pragma once

#include <QFrame>

class QCheckBox;
class QWidget;
class RoHoverIcon;
class RoSearchBox;

class MainToolbar final : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainToolbar)

public:
    enum class QueueButton { Top, Up, Down, Bottom };

    explicit MainToolbar(QWidget *parent = nullptr);

    RoSearchBox *searchBox() const { return m_search; }

    // Queue group (separator + 4 arrows) hides together when queueing is off.
    void setQueueGroupVisible(bool visible);
    // Enable states follow the selection (upstream's rules, see ActionRules).
    void setCommandStates(bool resume, bool pause, bool remove, bool queue);

signals:
    void createRequested();
    void pauseRequested();
    void resumeRequested();
    void deleteRequested();
    void queueRequested(QueueButton which);
    void settingsRequested();
    void addLinkRequested();
    void addTorrentRequested();

private:
    void syncToggles();

    RoSearchBox *m_search = nullptr;
    RoHoverIcon *m_pause = nullptr;
    RoHoverIcon *m_resume = nullptr;
    RoHoverIcon *m_delete = nullptr;
    QWidget *m_queueGroup = nullptr;
    QList<RoHoverIcon *> m_queueButtons;
    QCheckBox *m_themeToggle = nullptr;
    QCheckBox *m_vibrantToggle = nullptr;
};

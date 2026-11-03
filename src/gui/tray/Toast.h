// SPDX-License-Identifier: GPL-2.0-or-later
// The themed in-app toast (handoff 8.11). Windows only shows native balloons for apps that registered an
// AppUserModelID, so RoTorrent draws its own card at the bottom-right of the screen; it works while the main
// window is hidden in the tray. Licensed GPL-2.0-or-later.
#pragma once

#include <QPointer>
#include <QTimer>
#include <QWidget>

enum class NoticeKind
{
    Ok,     // green   - download completed
    Info,   // blue    - torrent added
    Warn,   // amber
    Crit    // red     - I/O error
};

class Toast final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Toast)

public:
    static constexpr int WIDTH = 436;     // card plus shadow margin
    static constexpr int MARGIN = 16;     // room for the drop shadow
    static constexpr int GAP = 12;        // between stacked toasts
    static constexpr int LIFE_MS = 7000;
    static constexpr int HOVER_GRACE_MS = 2000;

    Toast(const QString &title, const QString &text, NoticeKind kind, QWidget *parent = nullptr);

    QString title() const { return m_title; }
    QString text() const { return m_text; }
    NoticeKind kind() const { return m_kind; }
    bool closing() const { return m_closing; }

    void popup();      // show, fade in, start the life timer
    void dismiss();    // fade out and close (once)

signals:
    void clicked();               // the card was clicked: open the app
    void dismissed(Toast *self);  // emitted once, when it starts to go away

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void fadeTo(qreal opacity, int ms, bool thenClose);

    QString m_title;
    QString m_text;
    NoticeKind m_kind;
    QTimer m_life;
    bool m_closing = false;
};

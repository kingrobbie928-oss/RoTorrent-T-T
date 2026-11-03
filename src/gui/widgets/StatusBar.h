// SPDX-License-Identifier: GPL-2.0-or-later
// RoTorrent status bar (handoff 8.10): a plain 26 px QFrame#statusBar (never QStatusBar, whose internal margins
// ignore the design). Left to right, content right-aligned:
//   [connection chip] | [speed-limit chip] | [DHT nodes] | [free space]
// It is pure view: whoever owns a session pushes values in (see StatusFeed). Licensed GPL-2.0-or-later.
#pragma once

#include <QFrame>

class QLabel;

// A clickable icon + text pair.
class StatusChip final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(StatusChip)

public:
    explicit StatusChip(QWidget *parent = nullptr, bool clickable = true);
    QLabel *icon() const { return m_icon; }
    QLabel *text() const { return m_text; }

signals:
    void clicked();

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QLabel *m_icon;
    QLabel *m_text;
    bool m_clickable;
};

class RoStatusBar final : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RoStatusBar)

public:
    enum class Connection
    {
        Connected,    // listening and reachable from outside
        Firewalled,   // listening, no incoming connections seen
        Offline       // not listening at all
    };

    explicit RoStatusBar(QWidget *parent = nullptr);

    void setConnection(Connection state);
    Connection connection() const { return m_connection; }

    // Global (or alternative) limits in bytes per second; <= 0 means unlimited. The chip is shown only while at
    // least one of them is set.
    void setSpeedLimits(qint64 downBytesPerSecond, qint64 upBytesPerSecond);

    void setDhtNodes(int nodes, bool visible = true);

    // bytes < 0 means unknown ("N/A").
    void setFreeSpace(qint64 bytes, bool visible = true);

    // What the chips currently say (tests, accessibility).
    QString connectionText() const;
    QString connectionToolTip() const;
    QString limitText() const;
    QString dhtText() const;
    QString freeSpaceText() const;
    bool limitChipVisible() const;
    bool dhtVisible() const;
    bool freeSpaceVisible() const;
    StatusChip *connectionChip() const { return m_conn; }
    StatusChip *limitChip() const { return m_limit; }

    // "↓ 500 KiB/s   ↑ ∞"
    static QString limitsText(qint64 downBytesPerSecond, qint64 upBytesPerSecond);

    static constexpr int HEIGHT = 26;

signals:
    void connectionClicked();
    void limitsClicked();

private:
    void refreshIcons();
    void updateSeparators();
    void pinMinimumWidth();

    StatusChip *m_conn;
    StatusChip *m_limit;
    StatusChip *m_dht;
    QLabel *m_free;
    QFrame *m_connSep;
    QFrame *m_limitSep;
    QFrame *m_freeSep;
    Connection m_connection = Connection::Offline;
    bool m_limitOn = false;
    bool m_dhtOn = true;
    bool m_freeOn = true;
};

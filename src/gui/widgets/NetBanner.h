// SPDX-License-Identifier: GPL-2.0-or-later
// The "No network connection" banner under the toolbar (handoff 8.10, screenshots/new/*offline_banner.png).
// Shown while the connection state is Offline. Licensed GPL-2.0-or-later.
#pragma once

#include <QFrame>

class QLabel;
class QPushButton;

class NetBanner final : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(NetBanner)

public:
    explicit NetBanner(QWidget *parent = nullptr);

    QPushButton *retryButton() const { return m_retry; }
    QString text() const;

signals:
    void retryRequested();

private:
    void restyle();

    QLabel *m_icon;
    QLabel *m_text;
    QPushButton *m_retry;
};

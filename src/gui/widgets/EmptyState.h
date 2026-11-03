// SPDX-License-Identifier: GPL-2.0-or-later
// Overlay shown over the transfer list when there is nothing to list (handoff 8.14):
// icon, one-line headline, quiet sub-line and optional buttons (objectName dlgBtn).
#pragma once

#include <QList>
#include <QWidget>

#include <functional>

class QHBoxLayout;
class QLabel;

class EmptyState final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(EmptyState)

public:
    struct Action
    {
        QString text;
        std::function<void()> run;
    };

    explicit EmptyState(QWidget *viewport);

    void present(const QString &iconName, const QString &title, const QString &subtitle, const QList<Action> &actions);
    void dismiss() { hide(); }

    QString titleText() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void restyle();

    QLabel *m_icon = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_sub = nullptr;
    QHBoxLayout *m_row = nullptr;
    QString m_iconName;
};

// SPDX-License-Identifier: GPL-2.0-or-later
// Search field with the leading magnifier glyph (handoff 8.5, objectName searchBox). The text change
// is debounced so typing in a 500-row list stays smooth.
#pragma once

#include <QLineEdit>
#include <QTimer>

class QLabel;

class RoSearchBox final : public QLineEdit
{
    Q_OBJECT
public:
    explicit RoSearchBox(QWidget *parent = nullptr);

signals:
    void searchChanged(const QString &text);   // debounced

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void refreshIcon();

    QLabel *m_icon = nullptr;
    QTimer m_debounce;
};

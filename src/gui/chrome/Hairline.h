// SPDX-License-Identifier: GPL-2.0-or-later
// 1 px divider drawn as its own widget ON TOP of its parent's bottom edge (handoff 8.1 crispness rule 6).
// Full-height buttons would otherwise paint their hover fill over a border drawn by the row itself,
// and at fractional scaling a margin trick leaves half-covered pixels. A sibling raised above the
// buttons can never be covered. objectName "hairline" is what the QSS paints.
#pragma once

#include <QEvent>
#include <QFrame>

class Hairline final : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Hairline)

public:
    explicit Hairline(QWidget *parent)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("hairline"));
        setFixedHeight(1);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        parent->installEventFilter(this);
        place();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == parentWidget())
        {
            switch (event->type())
            {
            case QEvent::Resize:
            case QEvent::Show:
            case QEvent::PolishRequest:
            case QEvent::LayoutRequest:
                place();
                break;
            default:
                break;
            }
        }
        return false;
    }

private:
    void place()
    {
        QWidget *p = parentWidget();
        setGeometry(0, p->height() - 1, p->width(), 1);
        raise();
    }
};

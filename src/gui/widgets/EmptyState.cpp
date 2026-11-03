// SPDX-License-Identifier: GPL-2.0-or-later
#include "EmptyState.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "theme/RoTheme.h"

EmptyState::EmptyState(QWidget *viewport)
    : QWidget(viewport)
{
    setAttribute(Qt::WA_TranslucentBackground);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    v->setAlignment(Qt::AlignCenter);

    m_icon = new QLabel;
    m_icon->setAlignment(Qt::AlignCenter);
    v->addWidget(m_icon);
    v->addSpacing(6);

    m_title = new QLabel;
    m_title->setAlignment(Qt::AlignCenter);
    v->addWidget(m_title);

    m_sub = new QLabel;
    m_sub->setAlignment(Qt::AlignCenter);
    m_sub->setWordWrap(true);
    m_sub->setFixedWidth(340);
    v->addWidget(m_sub, 0, Qt::AlignHCenter);

    v->addSpacing(8);
    m_row = new QHBoxLayout;
    m_row->setSpacing(8);
    m_row->setAlignment(Qt::AlignCenter);
    v->addLayout(m_row);

    viewport->installEventFilter(this);
    connect(&RoTheme::instance(), &RoTheme::changed, this, &EmptyState::restyle);
    hide();
}

QString EmptyState::titleText() const
{
    return m_title->text();
}

void EmptyState::restyle()
{
    const RoPalette &pal = RoTheme::instance().pal();
    m_title->setStyleSheet(QStringLiteral("color:%1; font-size:15px; font-weight:600;").arg(pal.text.name()));
    m_sub->setStyleSheet(QStringLiteral("color:%1; font-size:12.5px;").arg(pal.muted.name()));
    if (!m_iconName.isEmpty())
        m_icon->setPixmap(RoTheme::instance().iconPx(m_iconName, pal.faint, 44));
}

bool EmptyState::eventFilter(QObject *watched, QEvent *event)
{
    if ((event->type() == QEvent::Resize) || (event->type() == QEvent::Show))
    {
        if (auto *w = qobject_cast<QWidget *>(watched))
            setGeometry(0, 0, w->width(), w->height());
    }
    return false;
}

void EmptyState::present(const QString &iconName, const QString &title, const QString &subtitle, const QList<Action> &actions)
{
    m_iconName = iconName;
    m_title->setText(title);
    m_sub->setText(subtitle);
    restyle();

    while (QLayoutItem *item = m_row->takeAt(0))
    {
        if (QWidget *w = item->widget())
        {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    for (const Action &a : actions)
    {
        auto *b = new QPushButton(a.text);
        b->setObjectName(QStringLiteral("dlgBtn"));
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [run = a.run] { if (run) run(); });
        m_row->addWidget(b);
    }
    setGeometry(0, 0, parentWidget()->width(), parentWidget()->height());
    raise();
    show();
}

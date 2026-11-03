// SPDX-License-Identifier: GPL-2.0-or-later
#include "RoSearchBox.h"

#include <QLabel>

#include "theme/RoTheme.h"

RoSearchBox::RoSearchBox(QWidget *parent)
    : QLineEdit(parent)
{
    setObjectName(QStringLiteral("searchBox"));
    setPlaceholderText(tr("Search torrents..."));
    setClearButtonEnabled(false);

    m_icon = new QLabel(this);
    m_icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_icon->setFixedSize(13, 13);
    refreshIcon();
    connect(&RoTheme::instance(), &RoTheme::changed, this, &RoSearchBox::refreshIcon);

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, [this] { emit searchChanged(text()); });
    connect(this, &QLineEdit::textChanged, this, [this](const QString &t)
    {
        if (t.isEmpty())
            emit searchChanged(t);   // clearing applies at once
        else
            m_debounce.start();
    });
}

void RoSearchBox::refreshIcon()
{
    m_icon->setPixmap(RoTheme::instance().iconPx(QStringLiteral("search"), RoTheme::instance().pal().faint, 13));
}

void RoSearchBox::resizeEvent(QResizeEvent *event)
{
    QLineEdit::resizeEvent(event);
    m_icon->move(9, (height() - 13) / 2);
}

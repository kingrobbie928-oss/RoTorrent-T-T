// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "NetBanner.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include "theme/RoTheme.h"

NetBanner::NetBanner(QWidget *parent)
    : QFrame(parent)
    , m_icon(new QLabel)
    , m_text(new QLabel(tr("No network connection. Transfers will resume automatically when you are back online.")))
    , m_retry(new QPushButton(tr("Retry")))
{
    setObjectName(QStringLiteral("netBanner"));
    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(16, 8, 12, 8);
    l->setSpacing(10);
    l->addWidget(m_icon);
    l->addWidget(m_text, 1);

    m_retry->setObjectName(QStringLiteral("dlgBtn"));
    m_retry->setCursor(Qt::PointingHandCursor);
    m_retry->setMinimumHeight(26);
    m_retry->setStyleSheet(QStringLiteral("QPushButton#dlgBtn { min-height: 24px; padding: 0 14px; }"));
    l->addWidget(m_retry);
    connect(m_retry, &QPushButton::clicked, this, &NetBanner::retryRequested);

    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    restyle();
    connect(&RoTheme::instance(), &RoTheme::changed, this, &NetBanner::restyle);
    hide();
}

QString NetBanner::text() const
{
    return m_text->text();
}

void NetBanner::restyle()
{
    const RoPalette &p = RoTheme::instance().pal();
    setStyleSheet(QStringLiteral("QFrame#netBanner { background: %1; border: 0; border-bottom: 1px solid %2; }"
                                 " QFrame#netBanner QLabel { color: %3; font-size: 12.5px; background: transparent; }")
                  .arg(p.surface2.name(), p.border.name(), p.text.name()));
    m_icon->setPixmap(RoTheme::instance().iconPx(QStringLiteral("m-warn"), p.check, 16));
}

// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "LegalNoticeDialog.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "theme/RoTheme.h"

void LegalNoticeDialog::showNotice(QWidget *parent, const QString &title, const QString &body, const QString &footer)
{
    LegalNoticeDialog dialog(parent, title, body, footer);
    dialog.run();
}

LegalNoticeDialog::LegalNoticeDialog(QWidget *parent, const QString &title, const QString &body, const QString &footer)
    : RoDialog(parent, title, 460)
    , m_heading {tr("Before you start")}
{
    const RoPalette &P = RoTheme::instance().pal();

    auto *host = new QDialog;
    host->setObjectName(QStringLiteral("roUi"));
    host->setWindowFlags(Qt::Widget);
    auto *v = new QVBoxLayout(host);
    v->setContentsMargins(22, 20, 22, 18);
    v->setSpacing(12);

    auto *heading = new QLabel(m_heading);
    heading->setObjectName(QStringLiteral("legalHeading"));
    heading->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600;"));
    v->addWidget(heading);

    auto *text = new QLabel(body);
    text->setObjectName(QStringLiteral("legalBody"));
    text->setWordWrap(true);
    v->addWidget(text);

    auto *end = new QLabel(footer);
    end->setObjectName(QStringLiteral("legalFooter"));
    end->setWordWrap(true);
    end->setStyleSheet(QStringLiteral("color:%1;").arg(P.muted.name()));
    v->addWidget(end);

    auto *row = new QHBoxLayout;
    row->addStretch(1);
    m_ok = new QPushButton(tr("OK"));
    m_ok->setProperty("primary", true);
    m_ok->setCursor(Qt::PointingHandCursor);
    m_ok->setMinimumWidth(96);
    m_ok->setDefault(true);
    row->addWidget(m_ok);
    v->addSpacing(4);
    v->addLayout(row);
    setContent(host);

    connect(m_ok, &QPushButton::clicked, this, &QDialog::accept);
}

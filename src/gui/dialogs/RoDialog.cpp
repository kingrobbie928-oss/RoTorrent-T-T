// SPDX-License-Identifier: GPL-2.0-or-later
#include "RoDialog.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QScreen>
#include <QVBoxLayout>
#include <QWindow>

#include "chrome/Hairline.h"
#include "chrome/RoHoverIcon.h"
#include "theme/RoTheme.h"

RoDialog::RoDialog(QWidget *parent, const QString &title, const int width)
    : QDialog(parent)
    , m_owner(parent ? parent->window() : nullptr)
{
    setObjectName(QStringLiteral("appDialog"));
    setWindowTitle(title);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);
    setFixedWidth(width);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(1, 1, 1, 1);
    root->setSpacing(0);

    auto *head = new QWidget;
    head->setObjectName(QStringLiteral("dlgHead"));
    head->setFixedHeight(41);   // 40 px + the 1 px Hairline (whole device pixels at every 25% step)
    auto *row = new QHBoxLayout(head);
    row->setContentsMargins(11, 0, 0, 1);   // 11 + 1 px border = 12 px, the same edge as the content
    row->setSpacing(0);
    new Hairline(head);

    m_logo = new QLabel;
    row->addWidget(m_logo);
    row->addSpacing(10);
    auto *label = new QLabel(title);
    label->setObjectName(QStringLiteral("dlgTitle"));
    row->addWidget(label, 1);

    auto *close = new RoHoverIcon(QStringLiteral("win-close"), &RoPalette::muted, &RoPalette::text, true, 16);
    close->setObjectName(QStringLiteral("capBtn"));
    close->setProperty("danger", true);
    close->setFixedWidth(46);
    close->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    close->setToolTip(tr("Close"));
    connect(close, &QToolButton::clicked, this, &QDialog::reject);
    row->addWidget(close);
    root->addWidget(head);

    m_body = new QVBoxLayout;
    m_body->setContentsMargins(0, 0, 0, 0);
    m_body->setSpacing(0);
    root->addLayout(m_body);

    rebuildLogo();
    connect(&RoTheme::instance(), &RoTheme::changed, this, &RoDialog::rebuildLogo);
}

void RoDialog::rebuildLogo()
{
    const qreal dpr = devicePixelRatioF();
    QPixmap src(RoTheme::instance().logoPath());
    if (src.isNull())
        return;
    QPixmap scaled = src.scaledToHeight(qRound(16 * dpr), Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    m_logo->setPixmap(scaled);
}

void RoDialog::setContent(QWidget *content)
{
    m_body->addWidget(content);
}

int RoDialog::run()
{
    adjustSize();
    if (m_owner && m_owner->isVisible())
    {
        move(m_owner->geometry().center() - QPoint(width() / 2, height() / 2));
    }
    else if (const QScreen *screen = QGuiApplication::primaryScreen())
    {
        // No visible owner (first-run notice, a window hidden in the tray, a crash): centre on the screen.
        move(screen->availableGeometry().center() - QPoint(width() / 2, height() / 2));
    }
    return exec();
}

void RoDialog::mousePressEvent(QMouseEvent *event)
{
    if ((event->button() == Qt::LeftButton) && (event->position().y() < 41) && windowHandle())
        windowHandle()->startSystemMove();
    else
        QDialog::mousePressEvent(event);
}

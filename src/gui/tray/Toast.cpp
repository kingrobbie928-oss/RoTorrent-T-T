// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "Toast.h"

#include <QEasingCurve>
#include <QEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QToolButton>
#include <QVBoxLayout>

#include "theme/RoTheme.h"

namespace
{
    struct Look
    {
        const char *icon;
        QColor RoPalette::*colour;
        QColor RoPalette::*wash;
    };

    Look lookOf(const NoticeKind kind)
    {
        switch (kind)
        {
        case NoticeKind::Ok: return {"fn", &RoPalette::seed, &RoPalette::seedA};
        case NoticeKind::Warn: return {"m-warn", &RoPalette::check, &RoPalette::checkA};
        case NoticeKind::Crit: return {"m-crit", &RoPalette::accent, &RoPalette::accentA};
        case NoticeKind::Info: break;
        }
        return {"m-file-plus", &RoPalette::info, &RoPalette::infoA};
    }

    QString rgba(const QColor &c)
    {
        return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alphaF(), 0, 'f', 3);
    }
}

Toast::Toast(const QString &title, const QString &text, const NoticeKind kind, QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus)
    , m_title(title)
    , m_text(text)
    , m_kind(kind)
{
    setObjectName(QStringLiteral("toast"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose);

    const RoTheme &theme = RoTheme::instance();
    const RoPalette &p = theme.pal();
    const Look look = lookOf(kind);
    const QColor colour = p.*look.colour;
    const QColor wash = p.*look.wash;

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("toastCard"));
    card->setFixedWidth(WIDTH - 2 * MARGIN);
    outer->addWidget(card);
    card->setStyleSheet(QStringLiteral("QFrame#toastCard { background: %1; border: 1px solid %2; border-radius: 14px; }"
                                       " QLabel { background: transparent; border: 0; }")
                        .arg(p.surface.name(), p.border.name()));
    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(32);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, theme.isDark() ? 110 : 60));
    card->setGraphicsEffect(shadow);

    auto *row = new QHBoxLayout(card);
    row->setContentsMargins(16, 16, 12, 16);
    row->setSpacing(14);

    auto *chip = new QLabel;
    chip->setFixedSize(40, 40);
    chip->setAlignment(Qt::AlignCenter);
    chip->setPixmap(theme.iconPx(QString::fromLatin1(look.icon), colour, 21));
    chip->setStyleSheet(QStringLiteral("QLabel { background: %1; border-radius: 20px; }")
                        .arg(rgba(wash.alpha() < 255 ? wash : colour)));
    row->addWidget(chip, 0, Qt::AlignTop);

    auto *col = new QVBoxLayout;
    col->setSpacing(3);
    auto *titleLabel = new QLabel(title);
    QFont tf = titleLabel->font();
    tf.setPixelSize(15);
    tf.setWeight(QFont::DemiBold);
    titleLabel->setFont(tf);
    titleLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(p.text.name()));
    auto *body = new QLabel(text);
    QFont bf = body->font();
    bf.setPixelSize(13);
    body->setFont(bf);
    body->setWordWrap(true);
    body->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(p.muted.name()));
    col->addWidget(titleLabel);
    col->addWidget(body);
    row->addLayout(col, 1);

    auto *close = new QToolButton;
    close->setObjectName(QStringLiteral("toastClose"));
    close->setFixedSize(26, 26);
    close->setCursor(Qt::PointingHandCursor);
    close->setIcon(theme.icon(QStringLiteral("win-close"), p.muted, 14));
    close->setAutoRaise(true);
    close->setToolTip(tr("Dismiss"));
    close->setStyleSheet(QStringLiteral("QToolButton { background: transparent; border: 0; border-radius: 8px; }"
                                        " QToolButton:hover { background: %1; }").arg(p.surface3.name()));
    connect(close, &QToolButton::clicked, this, &Toast::dismiss);
    row->addWidget(close, 0, Qt::AlignTop);

    card->setCursor(Qt::PointingHandCursor);
    card->installEventFilter(this);

    adjustSize();
    setWindowOpacity(0.0);
    m_life.setSingleShot(true);
    connect(&m_life, &QTimer::timeout, this, &Toast::dismiss);
}

bool Toast::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);
    switch (event->type())
    {
    case QEvent::MouseButtonRelease:
        if (static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton)
        {
            emit clicked();   // a click on a toast opens the app, like a native notification
            dismiss();
            return true;
        }
        break;
    case QEvent::Enter:
        m_life.stop();   // hovering keeps it open
        break;
    case QEvent::Leave:
        if (!m_closing)
            m_life.start(HOVER_GRACE_MS);
        break;
    default:
        break;
    }
    return false;
}

void Toast::popup()
{
    show();
    fadeTo(1.0, 170, false);
    m_life.start(LIFE_MS);
}

void Toast::dismiss()
{
    if (m_closing)
        return;
    m_closing = true;
    m_life.stop();
    emit dismissed(this);
    fadeTo(0.0, 140, true);
}

void Toast::fadeTo(const qreal opacity, const int ms, const bool thenClose)
{
    auto *a = new QPropertyAnimation(this, "windowOpacity", this);
    a->setDuration(ms);
    a->setStartValue(windowOpacity());
    a->setEndValue(opacity);
    a->setEasingCurve(QEasingCurve::OutCubic);
    if (thenClose)
        connect(a, &QPropertyAnimation::finished, this, &QWidget::close);
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

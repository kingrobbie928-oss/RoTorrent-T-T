// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "StatusBar.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>

#include "base/utils/misc.h"
#include "theme/RoTheme.h"

namespace
{
    QPixmap dotPixmap(const QColor &colour)
    {
        const qreal dpr = qApp->devicePixelRatio();
        QPixmap pm(QSize(10, 10) * dpr);
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(colour);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(1, 1, 8, 8));
        return pm;
    }

    QFrame *makeSeparator()
    {
        auto *f = new QFrame;
        f->setObjectName(QStringLiteral("statusSep"));
        return f;
    }
}

// ---- StatusChip --------------------------------------------------------------------------------

StatusChip::StatusChip(QWidget *parent, const bool clickable)
    : QWidget(parent)
    , m_icon(new QLabel)
    , m_text(new QLabel)
    , m_clickable(clickable)
{
    if (clickable)
        setCursor(Qt::PointingHandCursor);
    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(6);   // html: .statusbar .item { gap: 6px }
    l->addWidget(m_icon);
    l->addWidget(m_text);
}

void StatusChip::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_clickable && (event->button() == Qt::LeftButton) && rect().contains(event->position().toPoint()))
        emit clicked();
    QWidget::mouseReleaseEvent(event);
}

// ---- RoStatusBar -------------------------------------------------------------------------------

RoStatusBar::RoStatusBar(QWidget *parent)
    : QFrame(parent)
    , m_conn(new StatusChip)
    , m_limit(new StatusChip)
    , m_dht(new StatusChip(nullptr, false))
    , m_free(new QLabel)
    , m_connSep(makeSeparator())
    , m_limitSep(makeSeparator())
    , m_freeSep(makeSeparator())
{
    setObjectName(QStringLiteral("statusBar"));
    setFixedHeight(HEIGHT);
    // The whole row may be narrower than its contents; the parent clips it (handoff 8.3, "no breakpoints").
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(16, 6, 16, 6);
    l->setSpacing(20);
    l->addStretch(1);
    l->addWidget(m_conn);
    l->addWidget(m_connSep);
    l->addWidget(m_limit);
    l->addWidget(m_limitSep);
    l->addWidget(m_dht);
    l->addWidget(m_freeSep);
    l->addWidget(m_free);

    connect(m_conn, &StatusChip::clicked, this, &RoStatusBar::connectionClicked);
    connect(m_limit, &StatusChip::clicked, this, &RoStatusBar::limitsClicked);
    m_limit->setToolTip(tr("Global speed limits are active.\nClick to change them."));

    m_dht->text()->setText(tr("%1 DHT nodes").arg(0));
    m_free->setText(tr("Free space: N/A"));
    m_limit->text()->setText(limitsText(0, 0));
    m_limit->setVisible(false);

    setConnection(Connection::Offline);
    updateSeparators();
    connect(&RoTheme::instance(), &RoTheme::changed, this, &RoStatusBar::refreshIcons);
}

QString RoStatusBar::limitsText(const qint64 down, const qint64 up)
{
    const auto one = [](const qint64 v)
    {
        if (v <= 0)
            return QString(QChar(0x221E));
        // "500.0 KiB/s" -> "500 KiB/s" (the design shows whole numbers; fractions stay: "1.5 MiB/s")
        static const QRegularExpression trailingZero(QStringLiteral(R"(\.0+(?!\d))"));
        return Utils::Misc::friendlyUnit(v, true).remove(trailingZero);
    };
    return QString(QChar(0x2193)) + u' ' + one(down) + u"   " + QChar(0x2191) + u' ' + one(up);
}

void RoStatusBar::setConnection(const Connection state)
{
    m_connection = state;
    QString label;
    QString tip;
    switch (state)
    {
    case Connection::Connected:
        label = tr("Connected");
        tip = tr("Connected: incoming connections are working.");
        break;
    case Connection::Firewalled:
        label = tr("Firewalled");
        tip = tr("Firewalled: incoming connections are blocked. Downloads still work but can be slower."
                 " Check your router’s port forwarding.");
        break;
    case Connection::Offline:
        label = tr("Offline");
        tip = tr("Offline: RoTorrent is not listening for connections."
                 " Check your network connection and the listening port.");
        break;
    }
    m_conn->text()->setText(label);
    m_conn->setToolTip(tip + QStringLiteral("\n") + tr("Click to open the connection settings."));
    refreshIcons();
}

void RoStatusBar::setSpeedLimits(const qint64 down, const qint64 up)
{
    m_limitOn = (down > 0) || (up > 0);
    m_limit->text()->setText(limitsText(down, up));
    m_limit->setVisible(m_limitOn);
    updateSeparators();
    pinMinimumWidth();
}

void RoStatusBar::setDhtNodes(const int nodes, const bool visible)
{
    m_dhtOn = visible;
    m_dht->text()->setText(tr("%1 DHT nodes").arg(nodes));
    m_dht->setVisible(visible);
    updateSeparators();
    pinMinimumWidth();
}

void RoStatusBar::setFreeSpace(const qint64 bytes, const bool visible)
{
    m_freeOn = visible;
    m_free->setText(tr("Free space: %1").arg((bytes < 0) ? tr("N/A") : Utils::Misc::friendlyUnit(bytes)));
    m_free->setVisible(visible);
    updateSeparators();
    pinMinimumWidth();
}

void RoStatusBar::refreshIcons()
{
    const RoPalette &p = RoTheme::instance().pal();
    QColor dot = p.seed;
    if (m_connection == Connection::Firewalled)
        dot = p.check;
    else if (m_connection == Connection::Offline)
        dot = p.accent;
    m_conn->icon()->setPixmap(dotPixmap(dot));
    m_limit->icon()->setPixmap(RoTheme::instance().iconPx(QStringLiteral("m-speed"), p.muted, 13));
    m_dht->icon()->setPixmap(RoTheme::instance().iconPx(QStringLiteral("dht"), p.muted, 13));
}

void RoStatusBar::updateSeparators()
{
    // A divider only where something stands on both sides of it.
    const bool anyAfterConn = m_limitOn || m_dhtOn || m_freeOn;
    m_connSep->setVisible(anyAfterConn);
    m_limitSep->setVisible(m_limitOn && (m_dhtOn || m_freeOn));
    m_freeSep->setVisible(m_dhtOn && m_freeOn);
}

void RoStatusBar::pinMinimumWidth()
{
    // Keep the natural width of the content so a narrow window clips the row instead of squeezing the labels.
    layout()->activate();
    const int w = layout()->minimumSize().width();
    if (minimumWidth() != w)
        setMinimumWidth(w);
}

QString RoStatusBar::connectionText() const { return m_conn->text()->text(); }
QString RoStatusBar::connectionToolTip() const { return m_conn->toolTip(); }
QString RoStatusBar::limitText() const { return m_limit->text()->text(); }
QString RoStatusBar::dhtText() const { return m_dht->text()->text(); }
QString RoStatusBar::freeSpaceText() const { return m_free->text(); }
bool RoStatusBar::limitChipVisible() const { return m_limit->isVisibleTo(this); }
bool RoStatusBar::dhtVisible() const { return m_dht->isVisibleTo(this); }
bool RoStatusBar::freeSpaceVisible() const { return m_free->isVisibleTo(this); }

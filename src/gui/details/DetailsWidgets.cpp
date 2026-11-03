// SPDX-License-Identifier: GPL-2.0-or-later
#include "DetailsWidgets.h"

#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>

#include "base/utils/misc.h"
#include "theme/RoTheme.h"
#include "widgets/RoPaint.h"
#include "widgets/TorrentDelegate.h"

namespace DetailsFormat
{
    QString bytes(const qint64 byteCount) { return Utils::Misc::friendlyUnit(byteCount); }
    QString speed(const qint64 bps) { return Utils::Misc::friendlyUnit(bps, true); }

    QString limit(const qint64 bps)
    {
        return (bps <= 0) ? QStringLiteral("∞") : speed(bps);
    }

    QString duration(const qint64 seconds)
    {
        return Utils::Misc::userFriendlyDuration(seconds);
    }

    QString dateTime(const QDateTime &when)
    {
        return when.isValid() ? when.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("—");
    }

    QString ratio(const double value)
    {
        return (value < 0) ? QStringLiteral("—") : QString::number(value, 'f', 2);
    }

    QString percent(const double fraction)
    {
        return QString::number(fraction * 100.0, 'f', 1) + u'%';
    }

    QString statusText(const RoStatus status)
    {
        return TorrentDelegate::statusLabel(status);
    }
}

// ------------------------------------------------------------------ PiecesBar

PiecesBar::PiecesBar(const Kind kind, QWidget *parent)
    : QWidget(parent)
    , m_kind(kind)
{
    setFixedHeight(10);
    connect(&RoTheme::instance(), &RoTheme::changed, this, qOverload<>(&QWidget::update));
}

void PiecesBar::setPieces(const QBitArray &have)
{
    m_have = have;
    update();
}

void PiecesBar::setAvailability(const QList<int> &copies)
{
    m_copies = copies;
    m_maxCopies = 0;
    for (const int c : copies)
        m_maxCopies = qMax(m_maxCopies, c);
    update();
}

void PiecesBar::clear()
{
    m_have.clear();
    m_copies.clear();
    m_maxCopies = 0;
    update();
}

double PiecesBar::columnValue(const QBitArray &have, const int x, const int width)
{
    const int n = static_cast<int>(have.size());
    if (n <= 0 || width <= 0)
        return 0;
    const int from = static_cast<int>(static_cast<qint64>(x) * n / width);
    const int to = qMax(from + 1, static_cast<int>(static_cast<qint64>(x + 1) * n / width));
    int count = 0;
    for (int i = from; i < qMin(to, n); ++i)
        count += have.testBit(i);
    return static_cast<double>(count) / (qMin(to, n) - from);
}

double PiecesBar::columnAvailability(const QList<int> &copies, const int maxCopies, const int x, const int width)
{
    const int n = static_cast<int>(copies.size());
    if (n <= 0 || width <= 0 || maxCopies <= 0)
        return 0;
    const int from = static_cast<int>(static_cast<qint64>(x) * n / width);
    const int to = qMin(n, qMax(from + 1, static_cast<int>(static_cast<qint64>(x + 1) * n / width)));
    qint64 sum = 0;
    for (int i = from; i < to; ++i)
        sum += qMax(0, copies.at(i));
    return static_cast<double>(sum) / ((to - from) * maxCopies);
}

void PiecesBar::paintEvent(QPaintEvent *)
{
    const RoPalette &pal = RoTheme::instance().pal();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(rect());
    QPainterPath clip;
    clip.addRoundedRect(r, 5, 5);
    p.setClipPath(clip);
    p.fillRect(r, pal.surface3);

    // One column per device pixel: the bar is a downsampled view of however many pieces there are.
    const qreal dpr = devicePixelRatioF();
    const int columns = qMax(1, qRound(width() * dpr));
    const qreal colWidth = r.width() / columns;
    const QColor base = (m_kind == Kind::Progress) ? pal.seed : pal.info;
    const bool progress = (m_kind == Kind::Progress);
    for (int x = 0; x < columns; ++x)
    {
        const double v = progress ? columnValue(m_have, x, columns) : columnAvailability(m_copies, m_maxCopies, x, columns);
        if (v <= 0)
            continue;
        QColor c = base;
        c.setAlphaF(progress ? (0.35 + 0.65 * v) : (0.35 + 0.65 * v));
        p.fillRect(QRectF(r.left() + x * colWidth, 0, colWidth + 0.6, r.height()), c);
    }
}

// ------------------------------------------------------------------ BarDelegate

BarDelegate::BarDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize BarDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return {130, 34};
}

void BarDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const RoPalette &pal = RoTheme::instance().pal();
    const QVariant data = index.data(Qt::UserRole);
    const double v = data.isValid() ? data.toDouble() : -1;

    if (option.state & QStyle::State_Selected)
        p->fillRect(option.rect, pal.surface3);
    else if (option.state & QStyle::State_MouseOver)
        p->fillRect(option.rect, pal.surface);
    if (v < 0)
        return;

    const QRectF r(option.rect);
    const qreal cy = r.center().y();
    const QRectF bar(r.left() + 10, cy - 3, qMin<qreal>(70, r.width() - 56), 6);
    p->save();
    p->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    p->setPen(Qt::NoPen);
    p->setBrush(pal.surface3);
    p->drawRoundedRect(bar, 3, 3);
    if (v > 0)
    {
        p->setBrush(pal.seed);
        p->drawRoundedRect(QRectF(bar.left(), bar.top(), qMax<qreal>(6, bar.width() * v / 100.0), 6), 3, 3);
    }
    p->setFont(RoPaint::font(11.5));
    p->setPen(pal.muted);
    p->drawText(QRectF(bar.right() + 8, r.top(), 44, r.height()), Qt::AlignVCenter | Qt::AlignLeft
                , QString::number(qRound(v)) + u'%');
    p->restore();
}

// ------------------------------------------------------------------ factories

namespace DetailsUi
{
    QLabel *heading(const QString &text)
    {
        auto *l = new QLabel(text.toUpper());
        l->setObjectName(QStringLiteral("dHeading"));
        QFont f = RoPaint::font(10, 700);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        l->setFont(f);
        return l;
    }

    QLabel *fieldLabel(const QString &text)
    {
        auto *l = new QLabel(text.toUpper());
        l->setObjectName(QStringLiteral("dLabel"));
        QFont f = RoPaint::font(10);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
        l->setFont(f);
        return l;
    }

    QLabel *emptyLabel(const QString &text)
    {
        auto *l = new QLabel(text);
        l->setObjectName(QStringLiteral("dEmpty"));
        l->setWordWrap(true);
        l->setContentsMargins(22, 14, 0, 0);
        return l;
    }

    QTableWidget *makeTable(const QStringList &headers, const QList<int> &widths)
    {
        auto *t = new QTableWidget(0, static_cast<int>(headers.size()));
        t->setObjectName(QStringLiteral("dTable"));
        QStringList upper;
        for (const QString &h : headers)
            upper << h.toUpper();
        t->setHorizontalHeaderLabels(upper);
        t->verticalHeader()->setVisible(false);
        t->verticalHeader()->setDefaultSectionSize(34);
        t->setShowGrid(false);
        t->setFrameShape(QFrame::NoFrame);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setSelectionBehavior(QAbstractItemView::SelectRows);
        t->setSelectionMode(QAbstractItemView::ExtendedSelection);
        QHeaderView *h = t->horizontalHeader();
        h->setHighlightSections(false);
        h->setStretchLastSection(true);
        h->setMinimumSectionSize(40);
        h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        QFont hf = RoPaint::font(10.5, 600);
        hf.setLetterSpacing(QFont::AbsoluteSpacing, 0.63);
        h->setFont(hf);
        t->setMouseTracking(true);
        t->setFocusPolicy(Qt::NoFocus);
        t->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        t->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        for (int i = 0; i < widths.size(); ++i)
            t->setColumnWidth(i, widths.at(i));
        return t;
    }

    QTableWidgetItem *cell(const QString &text, const QColor &colour, const Qt::Alignment align)
    {
        auto *it = new QTableWidgetItem(text);
        it->setTextAlignment(align | Qt::AlignVCenter);
        if (colour.isValid())
            it->setForeground(QBrush(colour));
        return it;
    }
}

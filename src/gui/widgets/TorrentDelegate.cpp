// SPDX-License-Identifier: GPL-2.0-or-later
#include "TorrentDelegate.h"

#include <cmath>

#include <QAbstractItemView>
#include <QCursor>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTableView>

#include "RoPaint.h"
#include "theme/RoTheme.h"

using RoPaint::font;
using RoPaint::mix;
using RoPaint::wrapHeight;

namespace
{
    constexpr int BI = TorrentDelegate::BADGE_ICON;

    const TorrentRow *rowOf(const QModelIndex &index)
    {
        return index.data(TransferListModel::RowRole).value<const TorrentRow *>();
    }

    QString statusIconName(const RoStatus s)
    {
        switch (s)
        {
        case RoStatus::MissingFiles: return QStringLiteral("mf");
        default: return QString::fromLatin1(RoStatusInfo::code(s));
        }
    }

    // The demo draws an "fn" icon for Completed; RoStatusInfo::code(Completed) is "fn" as well.
}

TorrentDelegate::TorrentDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QString TorrentDelegate::statusLabel(const RoStatus status)
{
    switch (status)
    {
    case RoStatus::Downloading: return tr("Downloading");
    case RoStatus::Seeding: return tr("Seeding");
    case RoStatus::Completed: return tr("Completed");
    case RoStatus::Paused: return tr("Paused");
    case RoStatus::Queued: return tr("Queued");
    case RoStatus::Checking: return tr("Checking");
    case RoStatus::Stalled: return tr("Stalled");
    case RoStatus::Error: return tr("Error");
    case RoStatus::MissingFiles: return tr("Missing Files");
    }
    return {};
}

bool TorrentDelegate::needsAnimation(const TorrentRow &row, const bool vibrant)
{
    // Checking: stripes. Downloading: the bar's moving shine (and, in Vibrant, the row wave).
    Q_UNUSED(vibrant);
    return (row.status == RoStatus::Checking) || (row.status == RoStatus::Downloading);
}

qreal TorrentDelegate::cellHeight(const TorrentRow &row, const int column, const qreal colw, const bool vibrant) const
{
    const qreal inner = colw - 32;
    const bool chk = (row.status == RoStatus::Checking);
    switch (column)
    {
    case TransferListModel::Name:
    {
        const qreal nw = inner - 28 - 10;
        const qreal h = wrapHeight(font(13, 500), row.name, nw);
        return chk ? h : (h + 2 + QFontMetricsF(font(11)).height());
    }
    case TransferListModel::Status:
    {
        const bool flat = vibrant && !chk;
        const QFont f = font(vibrant ? 12.5 : 11, 600);
        const int padx = flat ? 0 : 9;
        return wrapHeight(f, statusLabel(row.status), inner - padx * 2 - BI - 5) + (flat ? 0 : 8);
    }
    case TransferListModel::Down:
    case TransferListModel::Up:
    case TransferListModel::Eta:
    case TransferListModel::Ratio:
    {
        const QString t = chk ? QStringLiteral("\u2014")
                              : (column == TransferListModel::Down ? row.downText
                                 : column == TransferListModel::Up ? row.upText
                                 : column == TransferListModel::Eta ? row.etaText : row.ratioText);
        return wrapHeight(font(12.5), t, inner);
    }
    default:
        return 0;
    }
}

QSize TorrentDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const auto *view = qobject_cast<const QTableView *>(option.widget);
    const int w = view ? view->columnWidth(index.column()) : 100;
    const TorrentRow *row = rowOf(index);
    if (!row)
        return {w, 53};
    return {w, qMax(53, static_cast<int>(cellHeight(*row, index.column(), w, RoTheme::instance().vibrant())) + 19)};
}

bool TorrentDelegate::rowHovered(const QStyleOptionViewItem &option, const QModelIndex &index)
{
    const auto *view = qobject_cast<const QAbstractItemView *>(option.widget);
    if (!view || !view->viewport()->underMouse())
        return false;
    return view->indexAt(view->viewport()->mapFromGlobal(QCursor::pos())).row() == index.row();
}

TorrentDelegate::Tint TorrentDelegate::tint(const RoPalette &p, const RoStatus s)
{
    switch (s)
    {
    case RoStatus::Downloading: return {p.seedA, p.seed, true};
    case RoStatus::Seeding: return {p.infoA, p.info, true};
    case RoStatus::Stalled: return {p.surface3, p.muted, true};
    case RoStatus::Queued: return {p.queuedA, p.queued, true};
    case RoStatus::Checking: return {p.checkA, p.check, true};
    case RoStatus::Paused: return {p.stallA, p.stall, true};
    case RoStatus::Error:
    case RoStatus::MissingFiles: return {p.accentA, p.accent, true};
    case RoStatus::Completed: break;
    }
    return {};
}

QBrush TorrentDelegate::washBrush(const RoPalette &p, const bool light, const RoStatus s, const QRectF &rect)
{
    QLinearGradient g(rect.topLeft(), rect.bottomRight());
    if (s == RoStatus::Completed)
    {
        const int a = light ? 68 : 42;
        const int b = light ? 42 : 24;
        const QColor base = light ? QColor(Qt::black) : p.surface2;
        g.setColorAt(0, mix(p.seed, base, a));
        g.setColorAt(1, mix(p.seed, base, b));
        return QBrush(g);
    }
    if (!light)
    {
        switch (s)
        {
        case RoStatus::Downloading: return QBrush(p.seedA);
        case RoStatus::Checking: return QBrush(p.checkA);
        case RoStatus::Seeding: return QBrush(p.infoA);
        case RoStatus::Queued: return QBrush(p.queuedA);
        case RoStatus::Stalled:
        case RoStatus::Paused: return QBrush(p.stallA);
        default: return QBrush(Qt::NoBrush);
        }
    }
    struct M { QColor c; int a; int b; QColor base; };
    M m;
    switch (s)
    {
    case RoStatus::Downloading: m = {p.seed, 62, 34, p.surface2}; break;
    case RoStatus::Checking: m = {p.check, 58, 32, p.surface2}; break;
    case RoStatus::Seeding: m = {p.info, 62, 34, p.surface2}; break;
    case RoStatus::Queued: m = {p.queued, 62, 34, p.surface2}; break;
    case RoStatus::Stalled:
    case RoStatus::Paused: m = {p.stall, 40, 22, p.surface3}; break;
    default: return QBrush(Qt::NoBrush);
    }
    g.setColorAt(0, mix(m.c, m.base, m.a));
    g.setColorAt(1, mix(m.c, m.base, m.b));
    return QBrush(g);
}

qreal TorrentDelegate::minContentWidth(const TorrentRow &row, const int column, const bool vibrant)
{
    const auto fw = [](const QFont &f, const QString &t) { return QFontMetricsF(f).horizontalAdvance(t); };
    // Browsers break at spaces and after a hyphen that is followed by a letter (not a digit).
    static const QRegularExpression splitter(QStringLiteral("(?<=-)(?=[A-Za-z])|\\s+"));
    const auto widest = [&](const QFont &f, const QString &t)
    {
        qreal w = 0;
        for (const QString &chunk : t.split(splitter))
            w = qMax(w, fw(f, chunk));
        return w;
    };
    switch (column)
    {
    case TransferListModel::Name:
        return 32 + 28 + 10 + qMax(widest(font(13, 500), row.name), fw(font(11), row.sizeText));
    case TransferListModel::Status:
    {
        const bool flat = vibrant && (row.status != RoStatus::Checking);
        return 32 + (flat ? 0 : 18) + BI + 5 + widest(font(vibrant ? 12.5 : 11, 600), statusLabel(row.status));
    }
    case TransferListModel::Down: return 32 + widest(font(12.5), row.downText);
    case TransferListModel::Up: return 32 + widest(font(12.5), row.upText);
    case TransferListModel::Eta: return 32 + widest(font(12.5), row.etaText);
    case TransferListModel::Ratio: return 32 + widest(font(12.5), row.ratioText);
    default: return 0;
    }
}

void TorrentDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const
{
    const TorrentRow *rowPtr = rowOf(idx);
    const auto *V = qobject_cast<const QTableView *>(opt.widget);
    if (!rowPtr || !V)
        return;
    const TorrentRow &row = *rowPtr;

    RoTheme &theme = RoTheme::instance();
    const RoPalette &P = theme.pal();
    const bool vib = theme.vibrant();
    const bool light = theme.isLight();
    const RoStatus st = row.status;
    const int pct = static_cast<int>(row.progress);
    const bool chk = (st == RoStatus::Checking);

    const QRectF r(opt.rect);
    const int col = idx.column();
    const int hs = V->horizontalScrollBar()->value();
    int W = 0;
    for (int c = 0; c < TransferListModel::ColumnCount; ++c)
        W += V->columnWidth(c);   // full row width (the html <tr>)
    const qreal x0 = -hs;

    p->save();
    p->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    p->setClipRect(r);
    if (vib && (st == RoStatus::Paused) && !light)
        p->setOpacity(0.85);

    const bool selected = opt.state & QStyle::State_Selected;

    // Row-wide layers (background, wash, stripes, separator, selection outline) are painted ONCE, by
    // the first visible column, across the whole row. Painting them per cell left a hairline seam
    // between columns at fractional DPI.
    const int firstVisible = V->columnAt(0);
    if ((col == firstVisible) || ((firstVisible < 0) && (col == 0)))
    {
        const QRectF rr(x0, r.top(), W, r.height());
        p->setClipRect(rr.intersected(QRectF(0, r.top(), V->viewport()->width(), r.height())));
        if (selected)
            p->fillRect(rr, P.surface3);
        else if (rowHovered(opt, idx))
            p->fillRect(rr, P.surface);
        if ((st == RoStatus::Error) || (st == RoStatus::MissingFiles))
            p->fillRect(rr, P.rowError);

        const bool washed = vib && ((st == RoStatus::Downloading) || (st == RoStatus::Seeding) || (st == RoStatus::Queued)
                                    || (st == RoStatus::Checking) || (st == RoStatus::Stalled) || (st == RoStatus::Paused)
                                    || (st == RoStatus::Completed));
        if (washed)
        {
            const qreal w = W * ((chk || (st == RoStatus::Queued)) ? 100 : pct) / 100.0;
            const QRectF wr(x0, rr.top(), w, rr.height());
            const QBrush b = washBrush(P, light, st, wr);
            if (b.style() != Qt::NoBrush)
                p->fillRect(wr.intersected(rr), b);

            if (chk)   // animated diagonal stripes (45deg, 6 px / 6 px)
            {
                p->save();
                p->setClipRect(wr.intersected(rr));
                QColor a1 = P.check;
                a1.setAlpha(static_cast<int>(255 * (light ? 0.65 : 0.55)));
                QColor a2 = P.check;
                a2.setAlpha(static_cast<int>(255 * (light ? 0.35 : 0.22)));
                const qreal hw = 6 * 1.4142;
                const qreal off = std::fmod(stripePhase() * 24, 2 * hw);
                qreal x = wr.left() - 80 - off;
                p->setPen(Qt::NoPen);
                while (x < wr.right() + 80)
                {
                    for (int k = 0; k < 2; ++k)
                    {
                        p->setBrush(k == 0 ? a1 : a2);
                        p->drawPolygon(QPolygonF({QPointF(x + k * hw, rr.bottom()), QPointF(x + k * hw + hw, rr.bottom())
                                                  , QPointF(x + k * hw + hw + rr.height(), rr.top())
                                                  , QPointF(x + k * hw + rr.height(), rr.top())}));
                    }
                    x += 2 * hw;
                }
                p->restore();
            }
            if (st == RoStatus::Downloading)   // scrolling wave overlay (svg pattern, 40 px tile, 2.2 s loop)
            {
                p->save();
                p->setClipRect(wr.intersected(rr));
                const qreal sc = rr.height() / 40;
                const qreal off = -std::fmod(wavePhase(), 1.0) * 40 * sc;
                const struct { qreal y0, y1, a; } waves[] = {{22, 10, 0.16}, {28, 18, 0.10}};
                for (const auto &wv : waves)
                {
                    qreal x = wr.left() + off - 40 * sc;
                    while (x < wr.right())
                    {
                        QPainterPath path(QPointF(x, rr.top() + wv.y0 * sc));
                        path.quadTo(x + 10 * sc, rr.top() + wv.y1 * sc, x + 20 * sc, rr.top() + wv.y0 * sc);
                        path.quadTo(x + 30 * sc, rr.top() + (2 * wv.y0 - wv.y1) * sc, x + 40 * sc, rr.top() + wv.y0 * sc);
                        path.lineTo(x + 40 * sc, rr.bottom());
                        path.lineTo(x, rr.bottom());
                        path.closeSubpath();
                        QColor c(255, 255, 255);
                        c.setAlphaF(wv.a);
                        p->fillPath(path, c);
                        x += 40 * sc;
                    }
                }
                p->restore();
            }
        }

        // Separators / selection outline. The grey 1 px separator used to sit between the outline and
        // the rows above and below it (a visible gap): the outline takes its place, so there is no
        // separator under a selected row nor under the row just above it.
        const QAbstractItemModel *m = V->model();
        const bool nextSelected = (idx.row() + 1 < m->rowCount())
                                  && V->selectionModel()->isSelected(m->index(idx.row() + 1, 0));
        if (!selected && !nextSelected)
            p->fillRect(QRectF(rr.left(), rr.bottom() - 1, rr.width(), 1), P.border);
        if (selected)
        {
            p->setPen(QPen(P.selOutline, 1));
            p->drawLine(QPointF(rr.left(), rr.top() + .5), QPointF(rr.right(), rr.top() + .5));
            p->drawLine(QPointF(rr.left(), rr.bottom() - .5), QPointF(rr.right(), rr.bottom() - .5));
            p->drawLine(QPointF(rr.left() + .5, rr.top()), QPointF(rr.left() + .5, rr.bottom()));
            p->drawLine(QPointF(rr.right() - .5, rr.top()), QPointF(rr.right() - .5, rr.bottom()));
        }
        p->setClipRect(r);
    }

    // Text colours (per vibrant / light rules in the css).
    const bool fnVib = vib && (st == RoStatus::Completed);
    const bool ltVib = vib && light && ((st == RoStatus::Downloading) || (st == RoStatus::Seeding) || (st == RoStatus::Queued)
                                        || (st == RoStatus::Checking) || (st == RoStatus::Stalled) || (st == RoStatus::Paused));
    const QColor cName = fnVib ? QColor(Qt::white) : P.text;
    QColor cSub = cName;
    cSub.setAlphaF(0.7);
    const QColor cNum = fnVib ? QColor(Qt::white) : (!ltVib ? P.muted : P.text);
    const qreal cy = r.center().y() - .5;
    const qreal L = r.left() + 16;

    const auto text = [&](qreal x, qreal y, const QString &t, const QFont &f, const QColor &color)
    {
        p->setFont(f);
        p->setPen(color);
        const QFontMetricsF fm(f);
        p->drawText(QRectF(x, y - fm.height() / 2, r.right() - x - 8, fm.height()), Qt::AlignVCenter | Qt::AlignLeft, t);
    };
    const auto wtext = [&](qreal x, qreal y, qreal w, qreal h, const QString &t, const QFont &f, const QColor &color)
    {
        p->setFont(f);
        p->setPen(color);
        p->drawText(QRectF(x, y, w, h), static_cast<int>(Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop), t);
    };

    if (col == TransferListModel::Name)
    {
        const QRectF ic(L, cy - 14, 28, 28);
        p->setPen(Qt::NoPen);
        p->setBrush(fnVib ? QColor(255, 255, 255, 56) : P.surface3);
        p->drawRoundedRect(ic, 7, 7);
        const QPixmap pm = theme.iconPx(QStringLiteral("file"), fnVib ? QColor(Qt::white) : P.muted, 14);
        p->drawPixmap(roSnap(p, QPointF(ic.center().x() - 7, ic.center().y() - 7)), pm);

        const qreal x = L + 28 + 10;
        const qreal nw = r.width() - 32 - 28 - 10;
        const QFont nf = font(13, 500);
        const qreal hn = wrapHeight(nf, row.name, nw);
        const qreal hsub = QFontMetricsF(font(11)).height();
        const qreal blk = hn + (chk ? 0 : 2 + hsub);
        const qreal top = cy - blk / 2;

        if (vib && chk && !light)   // dark Vibrant Checking only: a flat plate hugs the name so the stripes do not fight the text
        {
            const qreal tw = QFontMetricsF(nf).boundingRect(QRectF(0, 0, nw, 10000)
                                                            , static_cast<int>(Qt::TextWordWrap | Qt::AlignLeft), row.name).width();
            const auto over = [](const QColor &topC, const qreal a, const QColor &bot)
            {
                return QColor(qRound(topC.red() * a + bot.red() * (1 - a)), qRound(topC.green() * a + bot.green() * (1 - a))
                              , qRound(topC.blue() * a + bot.blue() * (1 - a)));
            };
            const QColor wash = P.checkA;
            const QColor base1 = over(wash, wash.alphaF(), P.bg);
            const QColor s1 = over(P.check, 0.55, base1);
            const QColor s2 = over(P.check, 0.22, base1);
            QColor plate((s1.red() + s2.red()) / 2, (s1.green() + s2.green()) / 2, (s1.blue() + s2.blue()) / 2);
            plate.setAlphaF(0.58);
            p->setPen(Qt::NoPen);
            p->setBrush(plate);
            p->drawRoundedRect(QRectF(x - 6, top - 2, tw + 12, hn + 4), 5, 5);
        }
        wtext(x, top, nw, hn, row.name, nf, cName);
        if (!chk)
            wtext(x, top + hn + 2, nw, hsub, row.sizeText, font(11), cSub);
    }
    else if (col == TransferListModel::Progress)
    {
        if (vib)
        {
            if (!chk)
                text(L, cy, QString::number(pct) + QLatin1Char('%'), font(12.5, 600)
                     , ((st == RoStatus::Downloading) && !ltVib) ? P.seed : cNum);
        }
        else
        {
            const qreal top = cy - 13.5;
            const QRectF bar(L, top, 110, 8);
            p->setPen(Qt::NoPen);
            p->setBrush(P.surface3);
            p->drawRoundedRect(bar, 4, 4);
            QLinearGradient inset(bar.topLeft(), bar.bottomLeft());   // soft inner shadow at the top
            inset.setColorAt(0, QColor(0, 0, 0, 76));
            inset.setColorAt(2.0 / 8, QColor(0, 0, 0, 0));
            p->setBrush(inset);
            p->drawRoundedRect(bar, 4, 4);

            const qreal w = chk ? 110 : 110.0 * pct / 100;
            if (w > 0)
            {
                const QRectF fill(L, top, qMax<qreal>(w, 8), 8);
                p->save();
                QPainterPath clip;
                clip.addRoundedRect(bar, 4, 4);
                p->setClipPath(clip);
                const auto grad = [&](const QColor &c)
                {
                    QLinearGradient g(fill.topLeft(), fill.topRight());
                    g.setColorAt(0, mix(c, Qt::black, 75));
                    g.setColorAt(1, c);
                    return QBrush(g);
                };
                QBrush br(Qt::NoBrush);
                switch (st)
                {
                case RoStatus::Downloading:
                case RoStatus::Completed: br = grad(P.seed); break;
                case RoStatus::Seeding: br = grad(P.info); break;
                case RoStatus::Queued: br = grad(P.queued); break;
                case RoStatus::Error:
                case RoStatus::MissingFiles: { QColor c = P.accent; c.setAlphaF(0.85); br = QBrush(c); break; }
                case RoStatus::Stalled: br = QBrush(P.stall); break;
                case RoStatus::Paused: { QColor c = P.stall; c.setAlphaF(0.6); br = QBrush(c); break; }
                case RoStatus::Checking: break;
                }
                p->setPen(Qt::NoPen);
                p->setBrush(br);
                p->drawRoundedRect(fill, 4, 4);

                if (chk)
                {
                    QColor c1 = P.check;
                    QColor c2 = P.check;
                    c2.setAlphaF(0.4);
                    const qreal hw = 6 * 1.4142;
                    const qreal off = std::fmod(stripePhase() * 24, 2 * hw);
                    qreal x = fill.left() - 2 * hw - off;
                    while (x < fill.right() + 2 * hw)
                    {
                        for (int k = 0; k < 2; ++k)
                        {
                            p->setBrush(k == 0 ? c1 : c2);
                            const qreal xa = x + k * hw;
                            p->drawPolygon(QPolygonF({QPointF(xa, fill.bottom()), QPointF(xa + hw, fill.bottom())
                                                      , QPointF(xa + hw + 8, fill.top()), QPointF(xa + 8, fill.top())}));
                        }
                        x += 2 * hw;
                    }
                }
                if (st == RoStatus::Downloading)   // moving shine sweep
                {
                    const qreal ph = std::fmod(shinePhase(), 1.0);
                    QLinearGradient sh(fill.left() + ph * fill.width() * 1.4 - fill.width() * 0.2, 0
                                       , fill.left() + ph * fill.width() * 1.4 + fill.width() * 0.35, 0);
                    sh.setColorAt(0, QColor(255, 255, 255, 0));
                    sh.setColorAt(0.5, QColor(255, 255, 255, 89));
                    sh.setColorAt(1, QColor(255, 255, 255, 0));
                    p->setBrush(sh);
                    p->drawRoundedRect(fill, 4, 4);
                }
                p->restore();
            }
            if (!chk)
                text(L, top + 8 + 5 + 7, QString::number(pct) + QLatin1Char('%'), font(11)
                     , (st == RoStatus::Downloading) ? P.seed : P.muted);
        }
    }
    else if (col == TransferListModel::Status)
    {
        QColor bg;
        QColor fg;
        bool hasBg = false;
        const Tint t = tint(P, st);
        if (st == RoStatus::Completed)
            fg = Qt::white;
        else if (t.valid)
        {
            bg = t.wash;
            fg = t.fg;
            hasBg = true;
        }
        if (vib)
            hasBg = false;
        if (vib && chk)
        {
            bg = P.surface2;
            hasBg = true;
        }
        const QFont f = font(vib ? 12.5 : 11, 600);
        const QString label = statusLabel(st);
        const QFontMetricsF fm(f);
        const bool flat = vib && !chk;
        const int padx = flat ? 0 : 9;
        const qreal tw = qMin(fm.horizontalAdvance(label), r.width() - 32 - padx * 2 - BI - 5);
        const qreal lh = wrapHeight(f, label, tw);   // the label wraps at spaces when the column is narrow
        const qreal w = padx * 2 + BI + 5 + tw;
        const qreal h = flat ? qMax<qreal>(16, lh) : (lh + 8);
        const QRectF bx(L, cy - h / 2, w, h);
        const qreal rad = qMin<qreal>(10, h / 2);
        if (vib && chk)
        {
            p->setPen(Qt::NoPen);
            p->setBrush(P.surface2);
            p->drawRoundedRect(bx, rad, rad);
            p->setBrush(P.checkA);
            p->drawRoundedRect(bx, rad, rad);
        }
        else if (hasBg && (st != RoStatus::Completed))
        {
            p->setPen(Qt::NoPen);
            p->setBrush(bg);
            p->drawRoundedRect(bx, rad, rad);
        }
        else if ((st == RoStatus::Completed) && !vib)
        {
            QLinearGradient g(bx.topLeft(), bx.bottomRight());
            g.setColorAt(0, mix(P.seed, Qt::black, 65));
            g.setColorAt(1, mix(P.seed, Qt::black, 35));
            p->setPen(Qt::NoPen);
            p->setBrush(g);
            p->drawRoundedRect(bx, rad, rad);
        }
        const QColor fgc = fnVib ? QColor(Qt::white) : ((ltVib && !chk) ? P.text : (fg.isValid() ? fg : P.muted));
        p->drawPixmap(roSnap(p, QPointF(bx.left() + padx, cy - BI / 2.0)), theme.iconPx(statusIconName(st), fgc, BI));
        wtext(bx.left() + padx + BI + 5, cy - lh / 2, tw + 1, lh, label, f, fgc);
    }
    else if (col == TransferListModel::QueuePos)
    {
        const QString qn = idx.data(Qt::DisplayRole).toString();
        const QFont nf = font(12.5);
        const qreal hn = wrapHeight(nf, qn, r.width() - 32);
        wtext(L, cy - hn / 2, r.width() - 32, hn, qn, nf, fnVib ? QColor(Qt::white) : cNum);
    }
    else
    {
        QString t = (col == TransferListModel::Down) ? row.downText : (col == TransferListModel::Up) ? row.upText
                  : (col == TransferListModel::Eta) ? row.etaText : row.ratioText;
        if (chk)
            t = vib ? QString() : QStringLiteral("\u2014");
        QColor color = ((col == TransferListModel::Down) && (st == RoStatus::Downloading) && !ltVib) ? P.seed
                       : ((col == TransferListModel::Eta) ? P.text : cNum);
        if (fnVib)
            color = Qt::white;
        const QFont nf = font(12.5);
        const qreal hn = wrapHeight(nf, t, r.width() - 32);
        wtext(L, cy - hn / 2, r.width() - 32, hn, t, nf, color);
    }
    p->restore();
}

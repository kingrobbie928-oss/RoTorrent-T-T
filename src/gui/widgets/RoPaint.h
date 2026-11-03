// SPDX-License-Identifier: GPL-2.0-or-later
// Small painting helpers shared by the delegates (ports of F, mix and wrap_h from the demo).
#pragma once

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QRectF>
#include <QString>

namespace RoPaint
{
    // Nunito Sans at a CSS pixel size and weight. Fractional sizes (12.5, 10.5) go through point
    // sizes because QFont::setPixelSize takes an int.
    inline QFont font(const qreal px, const int weight = 400)
    {
        QFont f(QStringLiteral("Nunito Sans"));
        if (px == static_cast<int>(px))
            f.setPixelSize(static_cast<int>(px));
        else
            f.setPointSizeF(px * 0.75);
        f.setWeight(static_cast<QFont::Weight>(weight));
        f.setHintingPreference(QFont::PreferNoHinting);
        f.setStyleStrategy(static_cast<QFont::StyleStrategy>(QFont::PreferAntialias | QFont::PreferQuality));
        return f;
    }

    // CSS color-mix(in srgb, a pct%, b)
    inline QColor mix(const QColor &a, const QColor &b, const int pct)
    {
        const qreal f = pct / 100.0;
        return QColor(qRound(a.red() * f + b.red() * (1 - f))
                      , qRound(a.green() * f + b.green() * (1 - f))
                      , qRound(a.blue() * f + b.blue() * (1 - f)));
    }

    // Height of `text` word-wrapped to `width`; Qt breaks at spaces and after hyphens like a browser.
    inline qreal wrapHeight(const QFont &f, const QString &text, const qreal width)
    {
        return QFontMetricsF(f).boundingRect(QRectF(0, 0, qMax<qreal>(1, width), 10000)
                                             , static_cast<int>(Qt::TextWordWrap | Qt::AlignLeft), text).height();
    }
}

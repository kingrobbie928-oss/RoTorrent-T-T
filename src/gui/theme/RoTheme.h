// SPDX-License-Identifier: GPL-2.0-or-later
// RoTheme: fonts, dark/light/vibrant state, QSS loader and the icon renderer (handoff 8.1, 8.2).
// Derived from the ro-qss kit. Licensed GPL-2.0-or-later.
#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QIcon>
#include <QObject>
#include <QPainter>
#include <QPixmap>
#include <QTransform>

#include "Palette.h"

class RoTheme final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RoTheme)

public:
    static RoTheme &instance();

    // Must run before the QApplication object exists: exact fractional display scaling.
    static void configureHighDpi();

    // Loads fonts, sets the Fusion style and the 13 px application font, restores the saved
    // theme (Gui/Theme, Gui/Vibrant in settings) and applies the stylesheet.
    // resDir is the resource prefix that holds theme/, icons/, fonts/ and assets/ (":/ro").
    // Needs SettingsStorage to be initialised.
    void init(const QString &resDir = QStringLiteral(":/ro"));

    void setDark(bool dark);     // swaps stylesheet and palette, saves, emits changed()
    void setVibrant(bool on);    // row washes on or off, saves, emits changed()
    bool isLight() const { return !m_dark; }
    bool isDark() const { return m_dark; }
    bool vibrant() const { return m_vibrant; }

    // Light + Vibrant uses the stronger washes automatically.
    const RoPalette &pal() const;
    QString logoPath() const;

    // SVG -> pixmap at the real device-pixel ratio (dpr <= 0: the primary screen's). Cached per
    // (name, colour, size, dpr).
    QPixmap iconPx(const QString &name, const QColor &colour, int size, qreal dpr = 0) const;
    QIcon icon(const QString &name, const QColor &colour, int size = 16) const
    {
        return QIcon(iconPx(name, colour, size));
    }

    qint64 ms() const { return m_clock.elapsed(); }   // animation clock for delegates

signals:
    void changed();

private:
    RoTheme() = default;
    void apply();
    void applyPalette() const;
    void loadFonts();

    QString m_dir;
    bool m_dark = true;
    bool m_vibrant = false;
    bool m_initialised = false;
    QElapsedTimer m_clock;
};

// Snap a logical point onto the painter's device pixel grid. An icon at a half device pixel is
// smeared over two rows or columns by the bilinear sampler; on the grid it maps 1:1 and is crisp.
inline QPointF roSnap(const QPainter *painter, const QPointF &point)
{
    const QTransform t = painter->deviceTransform();
    bool ok = false;
    const QTransform inv = t.inverted(&ok);
    if (!ok)
        return point;
    const QPointF d = t.map(point);
    return inv.map(QPointF(qRound(d.x()), qRound(d.y())));
}

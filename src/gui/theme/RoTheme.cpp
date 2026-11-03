// SPDX-License-Identifier: GPL-2.0-or-later
#include "RoTheme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPalette>
#include <QHash>
#include <QScreen>
#include <QStyleFactory>
#include <QSvgRenderer>

#include "base/global.h"
#include "base/settingsstorage.h"

namespace
{
    const QString THEME_KEY = u"Gui/Theme"_s;       // "dark" | "light"
    const QString VIBRANT_KEY = u"Gui/Vibrant"_s;   // bool
}

RoTheme &RoTheme::instance()
{
    static RoTheme theme;
    return theme;
}

void RoTheme::configureHighDpi()
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
}

void RoTheme::init(const QString &resDir)
{
    m_dir = resDir;
    m_clock.start();

    m_dark = (SettingsStorage::instance()->loadValue<QString>(THEME_KEY, u"dark"_s) != u"light"_s);
    m_vibrant = SettingsStorage::instance()->loadValue<bool>(VIBRANT_KEY, false);

    QApplication::setStyle(QStyleFactory::create(u"Fusion"_s));
    loadFonts();
    m_initialised = true;
    apply();
}

void RoTheme::loadFonts()
{
    for (const char *weight : {"Regular", "Medium", "SemiBold", "Bold"})
        QFontDatabase::addApplicationFont(u"%1/fonts/NunitoSans-%2.ttf"_s.arg(m_dir, QString::fromLatin1(weight)));

    // The 13 px base size lives on the application font, not in QSS: a QSS font-size on * would
    // override every setFont() call made in code.
    QFont base = qApp->font();
    base.setFamily(u"Nunito Sans"_s);
    base.setPixelSize(13);
    base.setHintingPreference(QFont::PreferNoHinting);
    base.setStyleStrategy(static_cast<QFont::StyleStrategy>(QFont::PreferAntialias | QFont::PreferQuality));
    qApp->setFont(base);
}

void RoTheme::setDark(const bool dark)
{
    if (m_dark == dark)
        return;
    m_dark = dark;
    SettingsStorage::instance()->storeValue(THEME_KEY, dark ? u"dark"_s : u"light"_s);
    apply();
}

void RoTheme::setVibrant(const bool on)
{
    if (m_vibrant == on)
        return;
    m_vibrant = on;
    SettingsStorage::instance()->storeValue(VIBRANT_KEY, on);
    emit changed();
}

const RoPalette &RoTheme::pal() const
{
    if (m_dark)
        return RoPal::DARK;
    return m_vibrant ? RoPal::LIGHT_VIBRANT : RoPal::LIGHT;
}

QString RoTheme::logoPath() const
{
    return m_dir + (m_dark ? u"/assets/logo_dark.png"_s : u"/assets/logo_light.png"_s);
}

void RoTheme::apply()
{
    if (!m_initialised)
        return;

    const QString name = m_dark ? u"dark"_s : u"light"_s;
    QFile file(u"%1/theme/%2.qss"_s.arg(m_dir, name));
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning("RoTheme: cannot open %s", qUtf8Printable(file.fileName()));
        return;
    }

    QString qss = QString::fromUtf8(file.readAll());
    // Backslashes in url() are CSS escapes, so the prefix must use forward slashes.
    qss.replace(u"@ICONS"_s, QDir::fromNativeSeparators(u"%1/icons/%2"_s.arg(m_dir, name)));
    applyPalette();
    qApp->setStyleSheet(qss);
    emit changed();
}

// QSS does the painting, but Qt still fills some backgrounds from the application palette first (scroll
// area viewports and their contents, frames, native-style widgets in forms loaded from .ui files). A
// light palette under a dark stylesheet shows as pale panels and unreadable labels, so the palette
// follows the tokens too (handoff 8.2: "swap the stylesheet and palette").
void RoTheme::applyPalette() const
{
    const RoPalette &P = pal();
    QPalette p;
    p.setColor(QPalette::Window, P.surface);
    p.setColor(QPalette::WindowText, P.text);
    p.setColor(QPalette::Base, P.surface2);
    p.setColor(QPalette::AlternateBase, P.surface);
    p.setColor(QPalette::Text, P.text);
    p.setColor(QPalette::Button, P.surface3);
    p.setColor(QPalette::ButtonText, P.text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, P.accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase, P.surface2);
    p.setColor(QPalette::ToolTipText, P.text);
    p.setColor(QPalette::PlaceholderText, P.faint);
    p.setColor(QPalette::Link, P.accent);
    p.setColor(QPalette::LinkVisited, P.accent);
    p.setColor(QPalette::Light, P.surface3);
    p.setColor(QPalette::Midlight, P.surface3);
    p.setColor(QPalette::Mid, P.border);
    p.setColor(QPalette::Dark, P.border);
    p.setColor(QPalette::Shadow, P.bg);
    for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, P.faint);
    qApp->setPalette(p);
}

QPixmap RoTheme::iconPx(const QString &name, const QColor &colour, const int size, qreal dpr) const
{
    if (dpr <= 0)
    {
        const QScreen *screen = QGuiApplication::primaryScreen();
        dpr = screen ? qMax<qreal>(1.0, screen->devicePixelRatio()) : 1.0;
    }

    static QHash<QString, QPixmap> cache;
    const QString key = u"%1|%2|%3|%4"_s.arg(name, colour.name(QColor::HexArgb)).arg(size).arg(dpr);
    if (const auto it = cache.constFind(key); it != cache.constEnd())
        return *it;

    QFile file(u"%1/icons/src/%2.svg"_s.arg(m_dir, name));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QByteArray svg = file.readAll();
    svg.replace("currentColor", colour.name().toUtf8());

    QPixmap pixmap((QSize(size, size) * dpr).boundedTo(QSize(4096, 4096)));
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(dpr);
    {
        QPainter painter(&pixmap);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        QSvgRenderer(svg).render(&painter, QRectF(0, 0, size, size));
    }
    cache.insert(key, pixmap);
    return pixmap;
}

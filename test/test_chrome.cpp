// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 2 chrome test (offscreen). Checks the geometry rules of handoff 8.3 and the live
// theme / Vibrant switch. Run with QT_QPA_PLATFORM=offscreen and, for the scaling rule,
// QT_SCALE_FACTOR=1 / 1.25 / 1.5 / 1.75 / 2.25 / 2.5 / 3 (see run_chrome.sh).
#include <cstdio>

#include <QApplication>
#include <QDir>
#include <QMenuBar>
#include <QStyle>
#include <QStyleFactory>
#include <QTemporaryDir>

#include "MainWindow.h"
#include "base/path.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "theme/RoTheme.h"

namespace
{
    int failures = 0;
    void check(const bool ok, const char *what)
    {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok)
            ++failures;
    }
}

int main(int argc, char **argv)
{
    RoTheme::configureHighDpi();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RoTorrent"));

    QTemporaryDir dir;
    Profile::initInstance(Path(dir.path()), QString(), false);
    SettingsStorage::initInstance();
    RoTheme &theme = RoTheme::instance();
    theme.init();
    check(theme.isDark() && !theme.vibrant(), "defaults: dark, no Vibrant");
    check(qApp->font().pixelSize() == 13, "application font is 13 px");
    check(qApp->font().family() == QStringLiteral("Nunito Sans"), "application font is Nunito Sans (bundled)");
    // With a stylesheet active Qt wraps the base style in a QStyleSheetStyle proxy that reports no name,
    // so "base style is Fusion" cannot be read back here; RoTheme::init sets Fusion before applying QSS.
    check(QStyleFactory::keys().contains(QStringLiteral("Fusion"), Qt::CaseInsensitive), "Fusion style is available");
    check(QByteArray(qApp->style()->metaObject()->className()) == "QStyleSheetStyle", "stylesheet proxy is active");
    check(qApp->styleSheet().contains(QStringLiteral(":/ro/icons/dark")), "@ICONS replaced with the dark resource prefix");

    MainWindow window;
    window.show();
    QApplication::processEvents();

    check(window.minimumSize() == QSize(300, 160), "minimum size 300 x 160");
    check(window.windowFlags().testFlag(Qt::FramelessWindowHint), "frameless");

    auto *wrap = window.findChild<QWidget *>(QStringLiteral("menuWrap"));
    check(wrap && wrap->height() == 45, "title row is 45 px");
    const auto buttons = window.findChildren<QWidget *>(QStringLiteral("capBtn"));
    check(buttons.size() == 3, "three caption buttons");
    bool sized = !buttons.isEmpty();
    bool flush = !buttons.isEmpty();
    int hairlineY = -1;
    if (auto *h = wrap ? wrap->findChild<QWidget *>(QStringLiteral("hairline")) : nullptr)
        hairlineY = h->y();
    for (QWidget *b : buttons)
    {
        sized = sized && (b->width() == 46) && (b->height() == 44);
        flush = flush && (b->y() + b->height() == hairlineY);   // hover fill ends exactly where the divider starts
    }
    check(sized, "caption buttons are 46 x 44");
    check(hairlineY == 44, "hairline sits on the row's last pixel");
    check(flush, "buttons end exactly at the hairline (red hover cannot cover the divider)");
    if (!buttons.isEmpty())
    {
        QWidget *close = buttons.last();
        check(close->mapTo(&window, QPoint(close->width(), 0)).x() == window.width(), "Close is flush with the right edge");
        check(close->mapTo(&window, QPoint(0, 0)).y() == 0, "Close is flush with the top edge");
    }
    // 44 px must be whole device pixels at every spec'd scale (otherwise hover bleeds into the line).
    const qreal dpr = window.devicePixelRatioF();
    check(qAbs(44 * dpr - qRound(44 * dpr)) < 1e-9, "44 px row is a whole number of device pixels at this scale");

    check(window.titleMenuBar()->actions().size() == 5, "five menus: File Edit View Tools Help");
    bool noMnemonics = true;
    for (const QAction *a : window.titleMenuBar()->actions())
        noMnemonics = noMnemonics && !a->text().contains(u'&');
    check(noMnemonics, "no & mnemonics in menu labels");

    // Live theme switch: same window object, same size, no rebuild.
    const QSize before = window.size();
    int changes = 0;
    QObject::connect(&theme, &RoTheme::changed, &window, [&changes] { ++changes; });
    theme.setDark(false);
    QApplication::processEvents();
    check(!theme.isDark() && qApp->styleSheet().contains(QStringLiteral(":/ro/icons/light")), "setDark(false) swaps to the light stylesheet");
    theme.setVibrant(true);
    check(theme.vibrant() && (theme.pal().seedA.alpha() == 61), "Light + Vibrant uses the stronger washes");
    check(changes == 2, "changed() emitted once per switch");
    check(window.size() == before && window.isVisible(), "window kept its size and stayed visible through the switch");
    theme.setDark(true);
    check(theme.pal().seedA.alpha() == 36, "dark washes are 14%");

    // Persistence.
    theme.setDark(false);
    SettingsStorage::instance()->save();
    check(SettingsStorage::instance()->loadValue<QString>(QStringLiteral("Gui/Theme")) == QStringLiteral("light"), "theme persisted");
    check(SettingsStorage::instance()->loadValue<bool>(QStringLiteral("Gui/Vibrant")), "Vibrant persisted");

    // Icon renderer: cached, correct size at the requested dpr.
    const QPixmap a = theme.iconPx(QStringLiteral("win-close"), Qt::white, 16, 2.0);
    const QPixmap b = theme.iconPx(QStringLiteral("win-close"), Qt::white, 16, 2.0);
    check(a.size() == QSize(32, 32) && qFuzzyCompare(a.devicePixelRatio(), 2.0), "icon renders at 16 px x dpr 2 = 32 px");
    check(a.cacheKey() == b.cacheKey(), "icon pixmap is cached per (name, colour, size, dpr)");
    check(theme.iconPx(QStringLiteral("does-not-exist"), Qt::white, 16).isNull(), "unknown icon returns a null pixmap");

    window.close();
    SettingsStorage::freeInstance();
    Profile::freeInstance();
    std::printf(failures ? "%d FAILED\n" : "chrome OK\n", failures);
    return failures ? 1 : 0;
}

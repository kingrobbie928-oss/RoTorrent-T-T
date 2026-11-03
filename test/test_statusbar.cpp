// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 8: status bar and offline banner (offscreen, no engine). Handoff 8.10.
#include <cstdio>

#include <QApplication>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>

#include "base/path.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "gui/theme/RoTheme.h"
#include "gui/widgets/NetBanner.h"
#include "gui/widgets/StatusBar.h"

namespace
{
    int failures = 0;

    void check(const bool ok, const QString &what)
    {
        std::printf("%s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
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
    RoTheme::instance().init();

    RoStatusBar bar;
    bar.resize(900, RoStatusBar::HEIGHT);
    bar.show();

    check(bar.objectName() == u"statusBar", QStringLiteral("objectName is statusBar (the QSS keys off it)"));
    check(bar.height() == 26, QStringLiteral("26 px high"));

    // ---- connection chip ----
    bar.setConnection(RoStatusBar::Connection::Connected);
    check(bar.connectionText() == u"Connected", QStringLiteral("Connected label"));
    bar.setConnection(RoStatusBar::Connection::Firewalled);
    check(bar.connectionText() == u"Firewalled" && bar.connectionToolTip().contains(u"port forwarding"), QStringLiteral("Firewalled label and tooltip"));
    bar.setConnection(RoStatusBar::Connection::Offline);
    check(bar.connectionText() == u"Offline" && bar.connectionToolTip().contains(u"not listening"), QStringLiteral("Offline label and tooltip"));
    check(bar.connectionToolTip().contains(u"Click"), QStringLiteral("tooltip says the chip is clickable"));

    int connClicks = 0;
    QObject::connect(&bar, &RoStatusBar::connectionClicked, &bar, [&connClicks] { ++connClicks; });
    {
        // a left-button release inside the chip is a click
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(2, 2), QPointF(2, 2), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(2, 2), QPointF(2, 2), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(bar.connectionChip(), &press);
        QApplication::sendEvent(bar.connectionChip(), &release);
    }
    check(connClicks == 1, QStringLiteral("clicking the connection chip emits connectionClicked"));

    // ---- speed-limit chip ----
    check(!bar.limitChipVisible(), QStringLiteral("limit chip hidden while nothing is limited"));
    bar.setSpeedLimits(500 * 1024, 0);
    check(bar.limitChipVisible(), QStringLiteral("limit chip shown once a limit is set"));
    check(bar.limitText().startsWith(QChar(0x2193)) && bar.limitText().contains(u"500") && bar.limitText().contains(u"KiB/s")
          && bar.limitText().endsWith(QString(QChar(0x221E))), QStringLiteral("limit text: down 500 KiB/s, up unlimited: ") + bar.limitText());
    check(bar.limitText().simplified() == QString(QChar(0x2193)) + u" 500 KiB/s " + QChar(0x2191) + u" " + QChar(0x221E), QStringLiteral("exact text as designed: ") + bar.limitText());
    check(RoStatusBar::limitsText(1536 * 1024, 0).simplified().contains(u"1.5 MiB/s"), QStringLiteral("fractions are kept: ") + RoStatusBar::limitsText(1536 * 1024, 0));
    check(RoStatusBar::limitsText(0, 2 * 1024 * 1024).contains(u"MiB/s"), QStringLiteral("MiB/s for larger limits"));
    bar.setSpeedLimits(0, 0);
    check(!bar.limitChipVisible(), QStringLiteral("limit chip hides again when both limits are off"));
    bar.setSpeedLimits(0, 1024);
    int limClicks = 0;
    QObject::connect(&bar, &RoStatusBar::limitsClicked, &bar, [&limClicks] { ++limClicks; });
    {
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(2, 2), QPointF(2, 2), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(bar.limitChip(), &release);
    }
    check(limClicks == 1, QStringLiteral("clicking the limit chip emits limitsClicked"));

    // ---- DHT and free space ----
    bar.setDhtNodes(312);
    check(bar.dhtText() == u"312 DHT nodes" && bar.dhtVisible(), QStringLiteral("DHT chip: 312 DHT nodes"));
    bar.setDhtNodes(0, false);
    check(!bar.dhtVisible(), QStringLiteral("DHT chip hidden when DHT is off"));
    bar.setFreeSpace(-1);
    check(bar.freeSpaceText() == u"Free space: N/A", QStringLiteral("unknown free space reads N/A"));
    bar.setFreeSpace(842LL * 1024 * 1024 * 1024);
    check(bar.freeSpaceText().startsWith(u"Free space: 842") && bar.freeSpaceText().contains(u"GiB"), QStringLiteral("free space text: ") + bar.freeSpaceText());
    bar.setFreeSpace(1, false);
    check(!bar.freeSpaceVisible(), QStringLiteral("free space hidden by the preference"));

    // ---- narrow window: the row keeps its natural width and is clipped, never squeezed ----
    bar.setDhtNodes(312);
    bar.setFreeSpace(842LL * 1024 * 1024 * 1024);
    bar.setSpeedLimits(500 * 1024, 0);
    QApplication::processEvents();
    check(bar.minimumWidth() > 300, QStringLiteral("minimum width follows the content: %1").arg(bar.minimumWidth()));
    check(bar.sizePolicy().horizontalPolicy() == QSizePolicy::Ignored, QStringLiteral("horizontal policy is Ignored (clipped by the parent)"));

    // ---- theme switch must keep everything alive ----
    RoTheme::instance().setDark(false);
    RoTheme::instance().setDark(true);
    check(bar.connectionText() == u"Offline", QStringLiteral("survives a theme switch"));

    // ---- banner ----
    NetBanner banner;
    check(!banner.isVisible(), QStringLiteral("banner starts hidden"));
    check(banner.text().startsWith(u"No network connection.") && banner.retryButton()->text() == u"Retry", QStringLiteral("banner text and Retry button"));
    int retries = 0;
    QObject::connect(&banner, &NetBanner::retryRequested, &banner, [&retries] { ++retries; });
    banner.retryButton()->click();
    check(retries == 1, QStringLiteral("Retry emits retryRequested"));

    std::printf(failures ? "\n%d FAILED\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}

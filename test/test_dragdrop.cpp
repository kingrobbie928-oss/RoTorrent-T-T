// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 6 slice 7: drag and drop onto the main window (offscreen, no engine).
#include <cstdio>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include "base/path.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "gui/MainWindow.h"
#include "gui/theme/RoTheme.h"

namespace
{
    int failures = 0;

    void check(const bool ok, const QString &what)
    {
        std::printf("%s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
        if (!ok)
            ++failures;
    }

    QString touch(const QString &path)
    {
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write("x");
        return path;
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

    const QString torrent = touch(dir.filePath(QStringLiteral("a.TORRENT")));
    const QString other = touch(dir.filePath(QStringLiteral("notes.txt")));
    QDir().mkpath(dir.filePath(QStringLiteral("folder.torrent")));   // a folder named like a torrent
    const QString magnet = QStringLiteral("magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567&dn=a%20b&tr=http%3A%2F%2Fx.org%2Fann");

    // ---- what a drag carries ----------------------------------------------------------------
    {
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(torrent), QUrl::fromLocalFile(other)
                      , QUrl::fromLocalFile(dir.filePath(QStringLiteral("folder.torrent"))), QUrl::fromLocalFile(torrent)});
        const QStringList s = MainWindow::sourcesFromMimeData(&mime);
        check(s == QStringList {torrent}, QStringLiteral("only .torrent files, no folders, no duplicates: ") + s.join(u'|'));
    }
    {
        QMimeData mime;
        mime.setUrls({QUrl::fromEncoded(magnet.toUtf8())});
        const QStringList s = MainWindow::sourcesFromMimeData(&mime);
        check(s == QStringList {magnet}, QStringLiteral("a magnet URL keeps its percent-escapes"));
    }
    {
        QMimeData mime;
        mime.setText(QStringLiteral("  %1 \r\nnot a link\nhttps://example.org/x.torrent\n%1\n").arg(magnet));
        const QStringList s = MainWindow::sourcesFromMimeData(&mime);
        check(s == QStringList({magnet, QStringLiteral("https://example.org/x.torrent")}), QStringLiteral("text lines: magnet and https, deduplicated, junk ignored"));
    }
    {
        QMimeData mime;
        mime.setText(QStringLiteral("just some words"));
        check(MainWindow::sourcesFromMimeData(&mime).isEmpty(), QStringLiteral("plain words are not a source"));
        check(MainWindow::sourcesFromMimeData(nullptr).isEmpty(), QStringLiteral("no mime data is not a source"));
    }

    // ---- the window ---------------------------------------------------------------------------
    MainWindow window;
    window.show();
    QStringList received;
    int calls = 0;

    // (A synthetic QDropEvent never reaches the widget offscreen, so the handlers' logic is tested directly.)
    auto drop = [&window](const QMimeData &mime)
    {
        return window.canAcceptDrag(&mime) && window.handleDrop(&mime);
    };
    auto pump = []
    {
        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();
    };

    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(torrent)});

    check(window.acceptDrops(), QStringLiteral("the main window accepts drops"));
    check(!drop(mime), QStringLiteral("without the hook (demo mode) a drop is refused"));

    window.hooks().addSources = [&](const QStringList &sources) { received = sources; ++calls; };
    check(drop(mime), QStringLiteral("with the hook a .torrent drop is accepted"));
    check(calls == 0, QStringLiteral("the hook runs after the drop is answered, not inside it"));
    pump();
    check(calls == 1 && received == QStringList {torrent}, QStringLiteral("the hook then gets the dropped path"));

    QMimeData junk;
    junk.setUrls({QUrl::fromLocalFile(other)});
    check(!drop(junk), QStringLiteral("a drop with nothing usable is refused"));
    pump();
    check(calls == 1, QStringLiteral("and does not call the hook"));

    // ---- second instance / notification: bring the window forward -----------------------------
    window.showMinimized();
    window.activate();
    check(!window.isMinimized() && window.isVisible(), QStringLiteral("activate() restores a minimized window"));

    std::printf("%s\n", failures == 0 ? "DRAGDROP OK" : "DRAGDROP FAILED");
    return failures == 0 ? 0 : 1;
}

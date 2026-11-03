// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 2 development entry point: shows the RoTorrent window without starting the torrent
// engine. The real `rotorrent` executable takes over from Phase 3.
//
//   rotorrent_window [--profile <dir>] [--theme dark|light] [--vibrant] [--shot <png>]
//                    [--demo <torrents.json> | --generate <n>] [--select <row>] [--phase <0..1>]
//                    [--engine]   run on a real BitTorrent session using the profile directory
//                    [--dialog exit|shutdown|suspend|hibernate|reboot|legal|crash]   open one app-level dialog alone
//                    [--filter <status>] [--search <text>] [--queueing] [--width <px>] [--height <px>]
//
// --shot renders the window to a PNG and exits (use with QT_QPA_PLATFORM=offscreen).
// Licensed GPL-2.0-or-later.

#include <memory>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QMenu>
#include <QMenuBar>
#include <QTimer>

#include "EngineWindow.h"
#include "MainWindow.h"
#include "app/firstrun.h"
#include "base/bittorrent/session.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/preferences.h"
#include "actions/TorrentController.h"
#include "dialogs/AddTorrentFlow.h"
#include "dialogs/TorrentCreatorDialog.h"
#include "dialogs/InfoDialogs.h"
#include "dialogs/LegalNoticeDialog.h"
#include "dialogs/ManagementDialogs.h"
#include "dialogs/ShutdownConfirmDialog.h"
#include "dialogs/StacktraceDialog.h"
#include "details/DemoDetailsSource.h"
#include "details/DetailsPanel.h"
#include "details/LogBuffer.h"
#include "details/SessionDetailsSource.h"
#include "models/SessionAdapter.h"
#include "models/DemoSource.h"
#include "models/TorrentFilterProxy.h"
#include "models/TransferListModel.h"
#include "widgets/RoSearchBox.h"
#include "widgets/Sidebar.h"
#include "widgets/TorrentDelegate.h"
#include "widgets/TransferListView.h"
#include "base/path.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "theme/RoTheme.h"

namespace
{
    // Opens one slice-6 dialog on its own (with --shot: renders it to a PNG and exits).
    int showAppDialog(const QString &name, const QString &shot)
    {
        Preferences::initInstance();
        std::unique_ptr<RoDialog> dialog;
        const QHash<QString, ShutdownDialogAction> actions = {
            {QStringLiteral("exit"), ShutdownDialogAction::Exit}, {QStringLiteral("shutdown"), ShutdownDialogAction::Shutdown}
            , {QStringLiteral("suspend"), ShutdownDialogAction::Suspend}, {QStringLiteral("hibernate"), ShutdownDialogAction::Hibernate}
            , {QStringLiteral("reboot"), ShutdownDialogAction::Reboot}};
        if (actions.contains(name))
        {
            dialog = std::make_unique<ShutdownConfirmDialog>(nullptr, actions.value(name));
        }
        else if (name == QStringLiteral("legal"))
        {
            dialog = std::make_unique<LegalNoticeDialog>(nullptr, QObject::tr("Legal Notice")
                , QObject::tr("RoTorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.")
                , QObject::tr("No further notices will be issued."));
        }
        else if (name == QStringLiteral("crash"))
        {
            auto crash = std::make_unique<StacktraceDialog>();
            crash->setText(QStringLiteral("SIGSEGV"), QStringLiteral("0# 0x00007ff6c1f2a3b0 in rotorrent\n1# 0x00007ff6c1e9d4a2 in rotorrent\n2# 0x00007ff6c2013f10 in rotorrent"));
            dialog = std::move(crash);
        }
        else
        {
            qWarning("Unknown dialog '%s'", qUtf8Printable(name));
            return 2;
        }
        if (shot.isEmpty())
            return dialog->run();
        dialog->adjustSize();
        dialog->show();
        QApplication::processEvents();
        return dialog->grab().save(shot) ? 0 : 1;
    }
}

int main(int argc, char *argv[])
{
    RoTheme::configureHighDpi();   // must precede the QApplication object
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RoTorrent"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("profile"), QStringLiteral("Profile directory"), QStringLiteral("dir")
                      , QDir::temp().filePath(QStringLiteral("rotorrent-window-profile"))});
    parser.addOption({QStringLiteral("theme"), QStringLiteral("dark or light"), QStringLiteral("name")});
    parser.addOption({QStringLiteral("vibrant"), QStringLiteral("Enable Vibrant rows")});
    parser.addOption({QStringLiteral("shot"), QStringLiteral("Save a screenshot and exit"), QStringLiteral("png")});
    parser.addOption({QStringLiteral("menu"), QStringLiteral("With --shot: open this menu (0-4) first"), QStringLiteral("index")});
    parser.addOption({QStringLiteral("details"), QStringLiteral("Open the details panel on a tab (general, trackers, peers, http, content, log)"), QStringLiteral("tab")});
    parser.addOption({QStringLiteral("engine"), QStringLiteral("Run on a real BitTorrent session (profile dir)")});
    parser.addOption({QStringLiteral("demo"), QStringLiteral("Load rows from the kit's torrents.json"), QStringLiteral("file")});
    parser.addOption({QStringLiteral("generate"), QStringLiteral("Generate n demo rows covering every status"), QStringLiteral("n")});
    parser.addOption({QStringLiteral("select"), QStringLiteral("Select this row (0-based) after loading"), QStringLiteral("row")});
    parser.addOption({QStringLiteral("phase"), QStringLiteral("Freeze animations at this phase (0..1)"), QStringLiteral("p")});
    parser.addOption({QStringLiteral("filter"), QStringLiteral("Click a sidebar status row by label"), QStringLiteral("label")});
    parser.addOption({QStringLiteral("search"), QStringLiteral("Type this into the search box"), QStringLiteral("text")});
    parser.addOption({QStringLiteral("dialog"), QStringLiteral("Show only this app dialog (exit, shutdown, suspend, hibernate, reboot, legal, crash)"), QStringLiteral("name")});
    parser.addOption({QStringLiteral("queueing"), QStringLiteral("Show the # queue-position column")});
    parser.addOption({QStringLiteral("width"), QStringLiteral("Window width"), QStringLiteral("px")});
    parser.addOption({QStringLiteral("height"), QStringLiteral("Window height"), QStringLiteral("px")});
    parser.process(app);

    const bool engine = parser.isSet(QStringLiteral("engine"));
    if (engine)
        Logger::initInstance();
    QDir().mkpath(parser.value(QStringLiteral("profile")));
    Profile::initInstance(Path(parser.value(QStringLiteral("profile"))), QString(), false);
    SettingsStorage::initInstance();
    if (engine)
    {
        Preferences::initInstance();
        if (SettingsStorage::instance()->isEmpty())
            applyFirstRunDefaults();
        Net::ProxyConfigurationManager::initInstance();
        Net::DownloadManager::initInstance();
        BitTorrent::Session::initInstance();
    }

    RoTheme &theme = RoTheme::instance();
    theme.init();
    if (parser.isSet(QStringLiteral("theme")))
        theme.setDark(parser.value(QStringLiteral("theme")) != QStringLiteral("light"));
    if (parser.isSet(QStringLiteral("vibrant")))
        theme.setVibrant(true);

    if (parser.isSet(QStringLiteral("dialog")))
        return showAppDialog(parser.value(QStringLiteral("dialog")), parser.value(QStringLiteral("shot")));

    // On a real session the window comes from EngineWindow::create, the same wiring the rotorrent executable uses.
    std::unique_ptr<MainWindow> windowOwner(engine ? EngineWindow::create(BitTorrent::Session::instance()) : new MainWindow);
    MainWindow &window = *windowOwner;
    if (parser.isSet(QStringLiteral("width")) || parser.isSet(QStringLiteral("height")))
        window.resize(parser.value(QStringLiteral("width")).toInt() ? parser.value(QStringLiteral("width")).toInt() : 1280
                      , parser.value(QStringLiteral("height")).toInt() ? parser.value(QStringLiteral("height")).toInt() : 800);

    QString error;
    QList<TorrentRow> rows;
    if (parser.isSet(QStringLiteral("demo")))
        rows = DemoSource::fromJson(parser.value(QStringLiteral("demo")), &error);
    else if (parser.isSet(QStringLiteral("generate")))
        rows = DemoSource::generate(parser.value(QStringLiteral("generate")).toInt());
    if (!error.isEmpty())
        qWarning("%s", qUtf8Printable(error));
    if (!engine)
    {
        // The Creator works without an engine (it only cannot seed the new torrent), so it is wired in demo mode too.
        TorrentCreatorDialog::install(window, nullptr);
        window.transferModel()->setRows(rows);
        window.setDetailsSource(new DemoDetailsSource(window.transferModel(), &window), 1500);
        const QDateTime now = QDateTime::currentDateTime();
        window.logBuffer()->append(LogBuffer::Normal, QStringLiteral("RoTorrent v1.0 started"), now.addSecs(-95));
        window.logBuffer()->append(LogBuffer::Info, QStringLiteral("Peer ID: -RO0100-"), now.addSecs(-94));
        window.logBuffer()->append(LogBuffer::Warning, QStringLiteral("UPnP / NAT-PMP: port mapping failed (no gateway found)."), now.addSecs(-60));
        window.logBuffer()->append(LogBuffer::Critical, QStringLiteral("'kde-neon-user-edition.iso' errored. Reason: files are missing."), now.addSecs(-41));
    }
    if (parser.isSet(QStringLiteral("queueing")))
        window.setQueueingEnabled(true);
    if (parser.isSet(QStringLiteral("phase")))
        window.transferView()->rowDelegate()->setFixedPhase(parser.value(QStringLiteral("phase")).toDouble());
    window.show();
    QApplication::processEvents();
    if (parser.isSet(QStringLiteral("details")))
    {
        const QStringList names = {QStringLiteral("general"), QStringLiteral("trackers"), QStringLiteral("peers")
                                   , QStringLiteral("http"), QStringLiteral("content"), QStringLiteral("log")};
        const int tab = qMax(0, static_cast<int>(names.indexOf(parser.value(QStringLiteral("details")))));
        const int n = parser.isSet(QStringLiteral("select")) ? parser.value(QStringLiteral("select")).toInt() : 0;
        if (n >= 0 && n < window.transferModel()->rowCount())
            window.showDetails(window.transferModel()->row(n).id, tab);
        else
            window.detailsPanel()->expand(static_cast<DetailsPanel::Tab>(tab));
        QApplication::processEvents();
    }
    window.sidebar()->refreshCounts();
    if (parser.isSet(QStringLiteral("demo")))
        window.sidebar()->showDemoSpeedCard();
    if (parser.isSet(QStringLiteral("filter")))
    {
        const QString label = parser.value(QStringLiteral("filter"));
        const auto items = window.sidebar()->tree()->findItems(label, Qt::MatchExactly | Qt::MatchRecursive);
        if (!items.isEmpty())
            emit window.sidebar()->tree()->itemClicked(items.first(), 0);
    }
    if (parser.isSet(QStringLiteral("search")))
        window.searchBox()->setText(parser.value(QStringLiteral("search")));
    if (parser.isSet(QStringLiteral("select")))
        window.transferView()->selectRow(parser.value(QStringLiteral("select")).toInt());

    if (parser.isSet(QStringLiteral("shot")))
    {
        const QString out = parser.value(QStringLiteral("shot"));
        const QString menu = parser.value(QStringLiteral("menu"));
        QTimer::singleShot(engine ? 4000 : 700, &app, [&window, &app, out, menu]
        {
            if (!menu.isEmpty())
            {
                QMenuBar *bar = window.titleMenuBar();
                const auto actions = bar->actions();
                const int idx = menu.toInt();
                if (idx >= 0 && idx < actions.size() && actions[idx]->menu())
                    actions[idx]->menu()->popup(bar->mapToGlobal(bar->actionGeometry(actions[idx]).bottomLeft()));
            }
            QTimer::singleShot(200, &app, [&window, &app, out]
            {
                window.grab().save(out);
                app.quit();
            });
        });
    }

    const int rc = app.exec();
    if (engine)
    {
        windowOwner.reset();   // owns the adapter and the details source, which use the session
        BitTorrent::Session::freeInstance();
        Net::DownloadManager::freeInstance();
        Net::ProxyConfigurationManager::freeInstance();
        Preferences::freeInstance();
        Logger::freeInstance();
    }
    SettingsStorage::freeInstance();
    Profile::freeInstance();
    return rc;
}

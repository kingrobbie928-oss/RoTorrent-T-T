/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2026  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "application.h"

#include <algorithm>
#include <memory>
#include <utility>

#ifdef DISABLE_GUI
#include <cstdio>
#endif

#ifdef Q_OS_WIN
#include <memory>
#include <windows.h>
#include <shellapi.h>
#elif defined(Q_OS_UNIX)
#include <sys/resource.h>
#endif

#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QLibraryInfo>
#include <QMetaObject>
#include <QProcess>

#ifndef DISABLE_GUI
#include <QPixmapCache>
#include <QWidget>
#ifdef Q_OS_WIN
#include <QSessionManager>
#endif // Q_OS_WIN
#endif

#include "base/addtorrentmanager.h"
#include "base/bittorrent/addtorrentparams.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/net/geoipmanager.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/net/reverseresolution.h"
#include "base/net/smtpclient.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "base/torrentfileswatcher.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/os.h"
#include "base/utils/string.h"
#include "base/version.h"
#include "applicationinstancemanager.h"
#include "filelogger.h"
#include "firstrun.h"

#ifndef DISABLE_GUI
#include "gui/EngineWindow.h"
#include "gui/GuiSettings.h"
#include "gui/MainWindow.h"
#include "gui/dialogs/AddTorrentFlow.h"
#include "gui/dialogs/ShutdownConfirmDialog.h"
#endif // DISABLE_GUI

namespace
{
#define SETTINGS_KEY(name) u"Application/" name
#define FILELOGGER_SETTINGS_KEY(name) (SETTINGS_KEY(u"FileLogger/") name)

    const QString LOG_FOLDER = u"logs"_s;
    const QChar PARAMS_SEPARATOR = u'|';

    const Path DEFAULT_PORTABLE_MODE_PROFILE_DIR {u"profile"_s};

    const int MIN_FILELOG_SIZE = 1024; // 1KiB
    const int MAX_FILELOG_SIZE = 1000 * 1024 * 1024; // 1000MiB
    const int DEFAULT_FILELOG_SIZE = 65 * 1024; // 65KiB

#ifndef DISABLE_GUI
    const int PIXMAP_CACHE_SIZE = 64 * 1024 * 1024;  // 64MiB
#endif

    const QString PARAM_ADDSTOPPED = u"@addStopped"_s;
    const QString PARAM_CATEGORY = u"@category"_s;
    const QString PARAM_FIRSTLASTPIECEPRIORITY = u"@firstLastPiecePriority"_s;
    const QString PARAM_SAVEPATH = u"@savePath"_s;
    const QString PARAM_SEQUENTIAL = u"@sequential"_s;
    const QString PARAM_SEEDMODE = u"@seedMode"_s;
    const QString PARAM_SKIPDIALOG = u"@skipDialog"_s;

    QString bindParamValue(const QStringView paramName, const QStringView paramValue)
    {
        return paramName + u'=' + paramValue;
    }

    std::pair<QStringView, QStringView> parseParam(const QStringView param)
    {
        const qsizetype sepIndex = param.indexOf(u'=');
        if (sepIndex >= 0)
            return {param.first(sepIndex), param.sliced(sepIndex + 1)};

        return {param, {}};
    }

    QString serializeParams(const QBtCommandLineParameters &params)
    {
        QStringList result;
        // Because we're passing a string list to the currently running
        // qBittorrent process, we need some way of passing along the options
        // the user has specified. Here we place special strings that are
        // almost certainly not going to collide with a file path or URL
        // specified by the user, and placing them at the beginning of the
        // string list so that they will be processed before the list of
        // torrent paths or URLs.

        const BitTorrent::AddTorrentParams &addTorrentParams = params.addTorrentParams;

        if (!addTorrentParams.savePath.isEmpty())
            result.append(bindParamValue(PARAM_SAVEPATH, addTorrentParams.savePath.data()));

        if (addTorrentParams.addStopped.has_value())
            result.append(bindParamValue(PARAM_ADDSTOPPED, (*addTorrentParams.addStopped ? u"1" : u"0")));

        if (addTorrentParams.seedMode)
            result.append(PARAM_SEEDMODE);

        if (!addTorrentParams.category.isEmpty())
            result.append(bindParamValue(PARAM_CATEGORY, addTorrentParams.category));

        if (addTorrentParams.sequential)
            result.append(PARAM_SEQUENTIAL);

        if (addTorrentParams.firstLastPiecePriority)
            result.append(PARAM_FIRSTLASTPIECEPRIORITY);

        if (params.skipDialog.has_value())
            result.append(bindParamValue(PARAM_SKIPDIALOG, (*params.skipDialog ? u"1" : u"0")));

        result += params.torrentSources;

        return result.join(PARAMS_SEPARATOR);
    }

    QBtCommandLineParameters parseParams(const QStringView str)
    {
        QBtCommandLineParameters parsedParams;
        BitTorrent::AddTorrentParams &addTorrentParams = parsedParams.addTorrentParams;

        for (QStringView param : asConst(str.split(PARAMS_SEPARATOR, Qt::SkipEmptyParts)))
        {
            param = param.trimmed();
            const auto [paramName, paramValue] = parseParam(param);

            // Process strings indicating options specified by the user.

            if (paramName == PARAM_SAVEPATH)
            {
                addTorrentParams.savePath = Path(paramValue.toString());
                continue;
            }

            if (paramName == PARAM_ADDSTOPPED)
            {
                addTorrentParams.addStopped = (paramValue.toInt() != 0);
                continue;
            }

            if (paramName == PARAM_SEEDMODE)
            {
                addTorrentParams.seedMode = true;
                continue;
            }

            if (paramName == PARAM_CATEGORY)
            {
                addTorrentParams.category = paramValue.toString();
                continue;
            }

            if (paramName == PARAM_SEQUENTIAL)
            {
                addTorrentParams.sequential = true;
                continue;
            }

            if (paramName == PARAM_FIRSTLASTPIECEPRIORITY)
            {
                addTorrentParams.firstLastPiecePriority = true;
                continue;
            }

            if (paramName == PARAM_SKIPDIALOG)
            {
                parsedParams.skipDialog = (paramValue.toInt() != 0);
                continue;
            }

            parsedParams.torrentSources.append(param.toString());
        }

        return parsedParams;
    }
}

Application::Application(int &argc, char **argv)
    : BaseApplication(argc, argv)
    , m_commandLineArgs(parseCommandLine(Application::arguments()))
    , m_storeInstanceName(SETTINGS_KEY(u"InstanceName"_s))
    , m_storeFileLoggerEnabled(FILELOGGER_SETTINGS_KEY(u"Enabled"_s))
    , m_storeFileLoggerBackup(FILELOGGER_SETTINGS_KEY(u"Backup"_s))
    , m_storeFileLoggerDeleteOld(FILELOGGER_SETTINGS_KEY(u"DeleteOld"_s))
    , m_storeFileLoggerMaxSize(FILELOGGER_SETTINGS_KEY(u"MaxSizeBytes"_s))
    , m_storeFileLoggerAge(FILELOGGER_SETTINGS_KEY(u"Age"_s))
    , m_storeFileLoggerAgeType(FILELOGGER_SETTINGS_KEY(u"AgeType"_s))
    , m_storeFileLoggerPath(FILELOGGER_SETTINGS_KEY(u"Path"_s))
    , m_storeMemoryWorkingSetLimit(SETTINGS_KEY(u"MemoryWorkingSetLimit"_s))
#ifdef Q_OS_WIN
    , m_processMemoryPriority(SETTINGS_KEY(u"ProcessMemoryPriority"_s))
#endif
{
    qRegisterMetaType<Log::Msg>("Log::Msg");
    qRegisterMetaType<Log::Peer>("Log::Peer");

    setApplicationName(u"RoTorrent"_s);
    setOrganizationDomain(u"rotorrent.invalid"_s);  // PLACEHOLDER domain, see docs/STATUS.md
#if !defined(DISABLE_GUI)
    setDesktopFileName(u"RoTorrent"_s);
    // Hiding the main window to the notification area must not end the program: quitting is always explicit
    // (File > Exit, the tray's Exit, or MainWindow::closeEvent when there is no tray icon).
    setQuitOnLastWindowClosed(false);
    setQuitLockEnabled(false);
    QPixmapCache::setCacheLimit(PIXMAP_CACHE_SIZE);
#endif

    m_launchTimeSecsSinceEpoch = QDateTime::currentSecsSinceEpoch();

    Logger::initInstance();

    const auto portableProfilePath = Path(QCoreApplication::applicationDirPath()) / DEFAULT_PORTABLE_MODE_PROFILE_DIR;
    const bool portableModeEnabled = m_commandLineArgs.profileDir.isEmpty() && Utils::Fs::isDir(portableProfilePath);
    const Path profileDir = portableModeEnabled ? portableProfilePath : m_commandLineArgs.profileDir;
    Profile::initInstance(profileDir, m_commandLineArgs.configurationName,
                        (m_commandLineArgs.relativeFastresumePaths || portableModeEnabled));

    m_instanceManager = new ApplicationInstanceManager(Profile::instance()->location(SpecialFolder::Config), this);

    SettingsStorage::initInstance();
    Preferences::initInstance();

    if (SettingsStorage::instance()->isEmpty())
        applyFirstRunDefaults();

    initializeTranslation();

    connect(this, &QCoreApplication::aboutToQuit, this, &Application::cleanup);
    connect(m_instanceManager, &ApplicationInstanceManager::messageReceived, this, &Application::processMessage);
#if defined(Q_OS_WIN) && !defined(DISABLE_GUI)
    connect(this, &QGuiApplication::commitDataRequest, this, &Application::shutdownCleanup, Qt::DirectConnection);
#endif

    LogMsg(tr("RoTorrent %1 started. Process ID: %2", "RoTorrent v1.0.0 started")
        .arg(QStringLiteral(QBT_VERSION), QString::number(QCoreApplication::applicationPid())));
    if (portableModeEnabled)
    {
        LogMsg(tr("Running in portable mode. Auto detected profile folder at: %1").arg(profileDir.toString()));
        if (m_commandLineArgs.relativeFastresumePaths)
            LogMsg(tr("Redundant command line flag detected: \"%1\". Portable mode implies relative fastresume.").arg(u"--relative-fastresume"_s), Log::WARNING); // to avoid translating the `--relative-fastresume` string
    }
    else
    {
        LogMsg(tr("Using config directory: %1").arg(Profile::instance()->location(SpecialFolder::Config).toString()));
    }

    if (isFileLoggerEnabled())
        m_fileLogger = new FileLogger(fileLoggerPath(), isFileLoggerBackup(), fileLoggerMaxSize(), isFileLoggerDeleteOld(), fileLoggerAge(), static_cast<FileLogger::FileLogAgeType>(fileLoggerAgeType()));

    if (m_commandLineArgs.torrentingPort > 0) // it will be -1 when user did not set any value
    {
        SettingValue<int> port {u"BitTorrent/Session/Port"_s};
        port = m_commandLineArgs.torrentingPort;
    }
}

Application::~Application()
{
    // we still need to call cleanup()
    // in case the App failed to start
    cleanup();
}

const QBtCommandLineParameters &Application::commandLineArgs() const
{
    return m_commandLineArgs;
}

QString Application::instanceName() const
{
    return m_storeInstanceName;
}

void Application::setInstanceName(const QString &name)
{
    if (name == instanceName())
        return;

    m_storeInstanceName = name;
}

int Application::memoryWorkingSetLimit() const
{
    return m_storeMemoryWorkingSetLimit.get(512);
}

void Application::setMemoryWorkingSetLimit(const int size)
{
    if (size == memoryWorkingSetLimit())
        return;

    m_storeMemoryWorkingSetLimit = size;
#if defined(QBT_USES_LIBTORRENT2) && !defined(Q_OS_LINUX) && !defined(Q_OS_MACOS)
    applyMemoryWorkingSetLimit();
#endif
}

bool Application::isFileLoggerEnabled() const
{
    return m_storeFileLoggerEnabled.get(true);
}

void Application::setFileLoggerEnabled(const bool value)
{
    if (value && !m_fileLogger)
        m_fileLogger = new FileLogger(fileLoggerPath(), isFileLoggerBackup(), fileLoggerMaxSize(), isFileLoggerDeleteOld(), fileLoggerAge(), static_cast<FileLogger::FileLogAgeType>(fileLoggerAgeType()));
    else if (!value)
        delete m_fileLogger;
    m_storeFileLoggerEnabled = value;
}

Path Application::fileLoggerPath() const
{
    return m_storeFileLoggerPath.get(Path(LOG_FOLDER));
}

void Application::setFileLoggerPath(const Path &path)
{
    if (m_fileLogger)
        m_fileLogger->setPath(path);
    m_storeFileLoggerPath = path;
}

bool Application::isFileLoggerBackup() const
{
    return m_storeFileLoggerBackup.get(true);
}

void Application::setFileLoggerBackup(const bool value)
{
    if (m_fileLogger)
        m_fileLogger->setBackup(value);
    m_storeFileLoggerBackup = value;
}

bool Application::isFileLoggerDeleteOld() const
{
    return m_storeFileLoggerDeleteOld.get(true);
}

void Application::setFileLoggerDeleteOld(const bool value)
{
    if (value && m_fileLogger)
        m_fileLogger->deleteOld(fileLoggerAge(), static_cast<FileLogger::FileLogAgeType>(fileLoggerAgeType()));
    m_storeFileLoggerDeleteOld = value;
}

int Application::fileLoggerMaxSize() const
{
    const int val = m_storeFileLoggerMaxSize.get(DEFAULT_FILELOG_SIZE);
    return std::clamp(val, MIN_FILELOG_SIZE, MAX_FILELOG_SIZE);
}

void Application::setFileLoggerMaxSize(const int bytes)
{
    const int clampedValue = std::clamp(bytes, MIN_FILELOG_SIZE, MAX_FILELOG_SIZE);
    if (m_fileLogger)
        m_fileLogger->setMaxSize(clampedValue);
    m_storeFileLoggerMaxSize = clampedValue;
}

int Application::fileLoggerAge() const
{
    const int val = m_storeFileLoggerAge.get(1);
    return std::clamp(val, 1, 365);
}

void Application::setFileLoggerAge(const int value)
{
    m_storeFileLoggerAge = std::clamp(value, 1, 365);
}

int Application::fileLoggerAgeType() const
{
    const int val = m_storeFileLoggerAgeType.get(1);
    return ((val < 0) || (val > 2)) ? 1 : val;
}

void Application::setFileLoggerAgeType(const int value)
{
    m_storeFileLoggerAgeType = ((value < 0) || (value > 2)) ? 1 : value;
}

void Application::processMessage(const QString &message)
{
#ifndef DISABLE_GUI
    if (message.isEmpty())
    {
        // A second instance was started without arguments: bring this window to the front.
        if (m_window)
            m_window->activate();
        return;
    }
#endif

    const QBtCommandLineParameters params = parseParams(message);
    // If Application is not allowed to process params immediately
    // (i.e., other components are not ready) store params
    if (m_isProcessingParamsAllowed)
        processParams(params);
    else
        m_paramsQueue.append(params);
}

void Application::runExternalProgram(const QString &programTemplate, const BitTorrent::Torrent *torrent) const
{
    // Cannot give users shell environment by default, as doing so could
    // enable command injection via torrent name and other arguments
    // (especially when some automated download mechanism has been setup).
    // See: https://github.com/qbittorrent/qBittorrent/issues/10925

    const auto replaceVariables = [torrent](QString str) -> QString
    {
        for (qsizetype i = (str.length() - 2); i >= 0; --i)
        {
            if (str[i] != u'%')
                continue;

            const ushort specifier = str[i + 1].unicode();
            switch (specifier)
            {
            case u'C':
                str.replace(i, 2, QString::number(torrent->filesCount()));
                break;
            case u'D':
                str.replace(i, 2, torrent->savePath().toString());
                break;
            case u'F':
                str.replace(i, 2, torrent->contentPath().toString());
                break;
            case u'G':
                str.replace(i, 2, Utils::String::joinIntoString(torrent->tags(), u","_s));
                break;
            case u'I':
                str.replace(i, 2, (torrent->infoHash().v1().isValid() ? torrent->infoHash().v1().toString() : u"-"_s));
                break;
            case u'J':
                str.replace(i, 2, (torrent->infoHash().v2().isValid() ? torrent->infoHash().v2().toString() : u"-"_s));
                break;
            case u'K':
                str.replace(i, 2, torrent->id().toString());
                break;
            case u'L':
                str.replace(i, 2, torrent->category());
                break;
            case u'M':
                str.replace(i, 2, torrent->comment());
                break;
            case u'N':
                str.replace(i, 2, torrent->name());
                break;
            case u'R':
                str.replace(i, 2, torrent->rootPath().toString());
                break;
            case u'T':
                str.replace(i, 2, torrent->currentTracker());
                break;
            case u'Z':
                str.replace(i, 2, QString::number(torrent->totalSize()));
                break;
            default:
                // do nothing
                break;
            }

            // decrement `i` to avoid unwanted replacement, example pattern: "%%N"
            --i;
        }

        return str;
    };

    const QString logMsg = tr("Running external program. Torrent: \"%1\". Command: `%2`");
    const QString logMsgError = tr("Failed to run external program. Torrent: \"%1\". Command: `%2`");

    // The processing sequence is different for Windows and other OS, this is intentional
#if defined(Q_OS_WIN)
    const QString program = replaceVariables(programTemplate);
    const std::wstring programWStr = program.toStdWString();

    // Need to split arguments manually because QProcess::startDetached(QString)
    // will strip off empty parameters.
    // E.g. `python.exe "1" "" "3"` will become `python.exe "1" "3"`
    int argCount = 0;
    std::unique_ptr<LPWSTR[], decltype(&::LocalFree)> args {::CommandLineToArgvW(programWStr.c_str(), &argCount), ::LocalFree};

    if (argCount <= 0)
        return;

    QStringList argList;
    for (int i = 1; i < argCount; ++i)
        argList += QString::fromWCharArray(args[i]);

    QProcess proc;
    proc.setProgram(QString::fromWCharArray(args[0]));
    proc.setArguments(argList);
    proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args)
    {
        if (Preferences::instance()->isAutoRunConsoleEnabled())
        {
            args->flags |= CREATE_NEW_CONSOLE;
            args->flags &= ~(CREATE_NO_WINDOW | DETACHED_PROCESS);
        }
        else
        {
            args->flags |= CREATE_NO_WINDOW;
            args->flags &= ~(CREATE_NEW_CONSOLE | DETACHED_PROCESS);
        }
        args->inheritHandles = false;
        args->startupInfo->dwFlags &= ~STARTF_USESTDHANDLES;
        ::CloseHandle(args->startupInfo->hStdInput);
        ::CloseHandle(args->startupInfo->hStdOutput);
        ::CloseHandle(args->startupInfo->hStdError);
        args->startupInfo->hStdInput = nullptr;
        args->startupInfo->hStdOutput = nullptr;
        args->startupInfo->hStdError = nullptr;
    });

    if (proc.startDetached())
        LogMsg(logMsg.arg(torrent->name(), program));
    else
        LogMsg(logMsgError.arg(torrent->name(), program));
#else // Q_OS_WIN
    QStringList args = Utils::String::splitCommand(programTemplate);

    if (args.isEmpty())
        return;

    for (QString &arg : args)
    {
        // strip redundant quotes
        if (arg.startsWith(u'"') && arg.endsWith(u'"'))
        {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
            arg.slice(1, (arg.size() - 2));
#else
            arg.removeLast().removeFirst();
#endif
        }

        arg = replaceVariables(arg);
    }

    const QString command = args.takeFirst();
    QProcess proc;
    proc.setProgram(command);
    proc.setArguments(args);
    proc.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors);

    if (proc.startDetached())
    {
        // show intended command in log
        LogMsg(logMsg.arg(torrent->name(), replaceVariables(programTemplate)));
    }
    else
    {
        // show intended command in log
        LogMsg(logMsgError.arg(torrent->name(), replaceVariables(programTemplate)));
    }
#endif
}

void Application::sendNotificationEmail(const BitTorrent::Torrent *torrent)
{
    // Prepare mail content
    const QString content = tr("Torrent name: %1").arg(torrent->name()) + u'\n'
        + tr("Torrent size: %1").arg(Utils::Misc::friendlyUnit(torrent->wantedSize())) + u'\n'
        + tr("Save path: %1").arg(torrent->savePath().toString()) + u"\n\n"
        + tr("The torrent was downloaded in %1.", "The torrent was downloaded in 1 hour and 20 seconds")
            .arg(Utils::Misc::userFriendlyDuration(torrent->activeTime())) + u"\n\n\n"
        + tr("Thank you for using RoTorrent.") + u'\n';

    // Send the notification email
    const Preferences *pref = Preferences::instance();
    Net::SMTPClient::sendMail(pref->getMailNotificationSender(), pref->getMailNotificationEmail()
            , tr("Torrent \"%1\" has finished downloading").arg(torrent->name()), content, this);
}

void Application::sendTestEmail()
{
    const Preferences *pref = Preferences::instance();
    if (pref->isMailNotificationEnabled())
    {
        // Prepare mail content
        const QString content = tr("This is a test email.") + u'\n'
            + tr("Thank you for using RoTorrent.") + u'\n';

        // Send the notification email
        Net::SMTPClient::sendMail(pref->getMailNotificationSender(), pref->getMailNotificationEmail()
            , tr("Test email"), content, this);
    }
}

void Application::torrentAdded(const BitTorrent::Torrent *torrent) const
{
    const Preferences *pref = Preferences::instance();

    // AutoRun program
    if (pref->isAutoRunOnTorrentAddedEnabled())
        runExternalProgram(pref->getAutoRunOnTorrentAddedProgram().trimmed(), torrent);
}

void Application::torrentFinished(const BitTorrent::Torrent *torrent)
{
    const Preferences *pref = Preferences::instance();

    // AutoRun program
    if (pref->isAutoRunOnTorrentFinishedEnabled())
        runExternalProgram(pref->getAutoRunOnTorrentFinishedProgram().trimmed(), torrent);

    // Mail notification
    if (pref->isMailNotificationEnabled())
    {
        LogMsg(tr("Torrent: %1, sending mail notification").arg(torrent->name()));
        sendNotificationEmail(torrent);
    }

}

void Application::allTorrentsFinished()
{
    Preferences *const pref = Preferences::instance();
    bool isExit = pref->shutdownqBTWhenDownloadsComplete();
    bool isShutdown = pref->shutdownWhenDownloadsComplete();
    bool isSuspend = pref->suspendWhenDownloadsComplete();
    bool isHibernate = pref->hibernateWhenDownloadsComplete();
    bool isReboot = pref->rebootWhenDownloadsComplete();

    const bool haveAction = isExit || isShutdown || isSuspend || isHibernate || isReboot;
    if (!haveAction) return;

    ShutdownDialogAction action = ShutdownDialogAction::Exit;
    if (isSuspend)
        action = ShutdownDialogAction::Suspend;
    else if (isHibernate)
        action = ShutdownDialogAction::Hibernate;
    else if (isShutdown)
        action = ShutdownDialogAction::Shutdown;
    else if (isReboot)
        action = ShutdownDialogAction::Reboot;

#ifndef DISABLE_GUI
    // ask confirm
    if ((action == ShutdownDialogAction::Exit) && (pref->dontConfirmAutoExit()))
    {
        // do nothing & skip confirm
    }
    else
    {
        if (!ShutdownConfirmDialog::askForConfirmation(m_window, action)) return;
    }
#endif // DISABLE_GUI

    // Actually shut down
    if (action != ShutdownDialogAction::Exit)
    {
        qDebug("Preparing for auto-shutdown because all downloads are complete!");
        // Disabling it for next time
        pref->setShutdownWhenDownloadsComplete(false);
        pref->setSuspendWhenDownloadsComplete(false);
        pref->setHibernateWhenDownloadsComplete(false);
        pref->setRebootWhenDownloadsComplete(false);
        // Make sure preferences are synced before exiting
        m_shutdownAct = action;
    }

    qDebug("Exiting the application");
    exit();
}

bool Application::callMainInstance()
{
    return m_instanceManager->sendMessage(serializeParams(commandLineArgs()));
}

void Application::processParams(const QBtCommandLineParameters &params)
{
#ifndef DISABLE_GUI
    // Command-line, second-instance and file-association adds go through the same flow as File > Add Torrent:
    // a .torrent path, a magnet link, an info-hash or a web address each end in the Add New Torrent dialog,
    // unless --skip-dialog says otherwise or "Never show again" is on. --save-path, --category,
    // --add-paused, --sequential and the other options pre-fill that dialog.
    if (!m_window || params.torrentSources.isEmpty())
        return;
    AddTorrentFlow::Options options;
    options.params = params.addTorrentParams;
    options.skipDialog = params.skipDialog;
    AddTorrentFlow::addSources(*m_window, BitTorrent::Session::instance(), params.torrentSources, options);
#else
    for (const QString &torrentSource : params.torrentSources)
        m_addTorrentManager->addTorrent(torrentSource, params.addTorrentParams);
#endif
}

int Application::exec()
{
#if defined(QBT_USES_LIBTORRENT2) && !defined(Q_OS_LINUX) && !defined(Q_OS_MACOS)
    applyMemoryWorkingSetLimit();
#endif

#ifdef Q_OS_WIN
    applyMemoryPriority();
    adjustThreadPriority();
#endif

    Net::ProxyConfigurationManager::initInstance();
    Net::DownloadManager::initInstance();

    BitTorrent::Session::initInstance();

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::restored, this, [this]
    {
        connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentAdded, this, &Application::torrentAdded);
        connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentFinished, this, &Application::torrentFinished);
        connect(BitTorrent::Session::instance(), &BitTorrent::Session::allTorrentsFinished, this, &Application::allTorrentsFinished, Qt::QueuedConnection);

        m_addTorrentManager = new AddTorrentManagerImpl(this, BitTorrent::Session::instance(), this);

        Net::GeoIPManager::initInstance();
        Net::ReverseResolution::initInstance();
        TorrentFilesWatcher::initInstance();

#ifndef DISABLE_GUI
        // The window is created once the session has restored its torrents, so it opens with the full list.
        m_window = EngineWindow::create(BitTorrent::Session::instance(), this);
        // Options > Behavior > window state on start up ("Hidden" lives in the notification area).
        m_window->showForStartup(GuiSettings::startUpWindowState());
#endif // DISABLE_GUI

        m_isProcessingParamsAllowed = true;
#ifndef DISABLE_GUI
        // Let the window paint before a queued add opens its (modal) dialog.
        QMetaObject::invokeMethod(this, [this]
        {
            const QList<QBtCommandLineParameters> queued = std::exchange(m_paramsQueue, {});
            for (const QBtCommandLineParameters &params : queued)
                processParams(params);
        }, Qt::QueuedConnection);
#else
        for (const QBtCommandLineParameters &params : asConst(m_paramsQueue))
            processParams(params);
        m_paramsQueue.clear();
#endif
    });

    const QBtCommandLineParameters params = commandLineArgs();
    if (!params.torrentSources.isEmpty())
        m_paramsQueue.append(params);

    return BaseApplication::exec();
}

bool Application::hasAnotherInstance() const
{
    return !m_instanceManager->isFirstInstance();
}

void Application::initializeTranslation()
{
    QCoreApplication::installTranslator(&m_qtTranslator);
    QCoreApplication::installTranslator(&m_translator);

    const auto *pref = Preferences::instance();
    const QString localeStr = pref->getLocale();

    loadTranslation(localeStr);
#ifndef DISABLE_GUI
    if (localeStr.startsWith(u"ar") || localeStr.startsWith(u"he"))
    {
        qDebug("Right to Left mode");
        setLayoutDirection(Qt::RightToLeft);
    }
    else
    {
        setLayoutDirection(Qt::LeftToRight);
    }
#endif
}

#if (!defined(DISABLE_GUI) && defined(Q_OS_WIN))
void Application::shutdownCleanup([[maybe_unused]] QSessionManager &manager)
{
    // This is only needed for a special case on Windows XP.
    // (but is called for every Windows version)
    // If a process takes too much time to exit during OS
    // shutdown, the OS presents a dialog to the user.
    // That dialog tells the user that qbt is blocking the
    // shutdown, it shows a progress bar and it offers
    // a "Terminate Now" button for the user. However,
    // after the progress bar has reached 100% another button
    // is offered to the user reading "Cancel". With this the
    // user can cancel the **OS** shutdown. If we don't do
    // the cleanup by handling the commitDataRequest() signal
    // and the user clicks "Cancel", it will result in qbt being
    // killed and the shutdown proceeding instead. Apparently
    // aboutToQuit() is emitted too late in the shutdown process.
    cleanup();

    // According to the qt docs we shouldn't call quit() inside a slot.
    // aboutToQuit() is never emitted if the user hits "Cancel" in
    // the above dialog.
    QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
}
#endif

#if defined(QBT_USES_LIBTORRENT2) && !defined(Q_OS_LINUX) && !defined(Q_OS_MACOS)
void Application::applyMemoryWorkingSetLimit() const
{
    const size_t MiB = 1024 * 1024;
    const QString logMessage = tr("Failed to set physical memory (RAM) usage limit. Error code: %1. Error message: \"%2\"");

#ifdef Q_OS_WIN
    const SIZE_T maxSize = memoryWorkingSetLimit() * MiB;
    const auto minSize = std::min<SIZE_T>((64 * MiB), (maxSize / 2));
    if (!::SetProcessWorkingSetSizeEx(::GetCurrentProcess(), minSize, maxSize, QUOTA_LIMITS_HARDWS_MAX_ENABLE))
    {
        const DWORD errorCode = ::GetLastError();
        QString message;
        LPVOID lpMsgBuf = nullptr;
        const DWORD msgLength = ::FormatMessageW((FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS)
            , nullptr, errorCode, LANG_USER_DEFAULT, reinterpret_cast<LPWSTR>(&lpMsgBuf), 0, nullptr);
        if (msgLength > 0)
        {
            message = QString::fromWCharArray(reinterpret_cast<LPWSTR>(lpMsgBuf)).trimmed();
            ::LocalFree(lpMsgBuf);
        }
        LogMsg(logMessage.arg(QString::number(errorCode), message), Log::WARNING);
    }
#elif defined(Q_OS_UNIX)
    // has no effect on linux but it might be meaningful for other OS
    rlimit limit {};

    if (::getrlimit(RLIMIT_RSS, &limit) != 0)
        return;

    const size_t newSize = memoryWorkingSetLimit() * MiB;
    if (newSize > limit.rlim_max)
    {
        // try to raise the hard limit
        rlimit newLimit = limit;
        newLimit.rlim_max = newSize;
        if (::setrlimit(RLIMIT_RSS, &newLimit) != 0)
        {
            const auto message = QString::fromLocal8Bit(strerror(errno));
            LogMsg(tr("Failed to set physical memory (RAM) usage hard limit. Requested size: %1. System hard limit: %2. Error code: %3. Error message: \"%4\"")
                .arg(QString::number(newSize), QString::number(limit.rlim_max), QString::number(errno), message), Log::WARNING);
            return;
        }
    }

    limit.rlim_cur = newSize;
    if (::setrlimit(RLIMIT_RSS, &limit) != 0)
    {
        const auto message = QString::fromLocal8Bit(strerror(errno));
        LogMsg(logMessage.arg(QString::number(errno), message), Log::WARNING);
    }
#endif
}
#endif

#ifdef Q_OS_WIN
MemoryPriority Application::processMemoryPriority() const
{
    return m_processMemoryPriority.get(MemoryPriority::BelowNormal);
}

void Application::setProcessMemoryPriority(const MemoryPriority priority)
{
    if (processMemoryPriority() == priority)
        return;

    m_processMemoryPriority = priority;
    applyMemoryPriority();
}

void Application::applyMemoryPriority() const
{
    using SETPROCESSINFORMATION = BOOL (WINAPI *)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
    const auto setProcessInformation = Utils::OS::loadWinAPI<SETPROCESSINFORMATION>(u"Kernel32.dll"_s, "SetProcessInformation");
    if (!setProcessInformation)  // only available on Windows >= 8
        return;

    using SETTHREADINFORMATION = BOOL (WINAPI *)(HANDLE, THREAD_INFORMATION_CLASS, LPVOID, DWORD);
    const auto setThreadInformation = Utils::OS::loadWinAPI<SETTHREADINFORMATION>(u"Kernel32.dll"_s, "SetThreadInformation");
    if (!setThreadInformation)  // only available on Windows >= 8
        return;

#if (_WIN32_WINNT < _WIN32_WINNT_WIN8)
    // this dummy struct is required to compile successfully when targeting older Windows version
    struct MEMORY_PRIORITY_INFORMATION
    {
        ULONG MemoryPriority;
    };

#define MEMORY_PRIORITY_LOWEST 0
#define MEMORY_PRIORITY_VERY_LOW 1
#define MEMORY_PRIORITY_LOW 2
#define MEMORY_PRIORITY_MEDIUM 3
#define MEMORY_PRIORITY_BELOW_NORMAL 4
#define MEMORY_PRIORITY_NORMAL 5
#endif

    MEMORY_PRIORITY_INFORMATION prioInfo {};
    switch (processMemoryPriority())
    {
    case MemoryPriority::Normal:
    default:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_NORMAL;
        break;
    case MemoryPriority::BelowNormal:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_BELOW_NORMAL;
        break;
    case MemoryPriority::Medium:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_MEDIUM;
        break;
    case MemoryPriority::Low:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_LOW;
        break;
    case MemoryPriority::VeryLow:
        prioInfo.MemoryPriority = MEMORY_PRIORITY_VERY_LOW;
        break;
    }
    setProcessInformation(::GetCurrentProcess(), ProcessMemoryPriority, &prioInfo, sizeof(prioInfo));

    // To avoid thrashing/sluggishness of the app, set "main event loop" thread to normal memory priority
    // which is higher/equal than other threads
    prioInfo.MemoryPriority = MEMORY_PRIORITY_NORMAL;
    setThreadInformation(::GetCurrentThread(), ThreadMemoryPriority, &prioInfo, sizeof(prioInfo));
}

void Application::adjustThreadPriority() const
{
    // Workaround for improving responsiveness of qbt when CPU resources are scarce.
    // Raise main event loop thread to be just one level higher than libtorrent threads.
    // Also note that on *nix platforms there is no easy way to achieve it,
    // so implementation is omitted.

    ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
}
#endif

bool Application::loadTranslation(const QString &locale)
{
    // Load Qt translation
    const QString trPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (m_qtTranslator.load((u"qtbase_" + locale), trPath) || m_qtTranslator.load((u"qt_" + locale), trPath))
    {
        LogMsg(tr("Load Qt translation successful. Locale: %1.").arg(locale));
    }
    else
    {
        LogMsg(tr("Load Qt translation failed. Temporarily falling back to English. Locale not found: %1.")
            .arg(locale), Log::WARNING);
    }

    // Load qbt translation
    const bool success = m_translator.load(u":/lang/qbittorrent_" + locale);
    if (success)
    {
        LogMsg(tr("Load RoTorrent translation successful. Locale: %1.").arg(locale));
    }
    else
    {
        LogMsg(tr("Load RoTorrent translation failed. Temporarily falling back to English. Locale not found: %1.")
            .arg(locale), Log::WARNING);
    }

    return success;
}

qint64 Application::launchTimeSecsSinceEpoch() const
{
    return m_launchTimeSecsSinceEpoch;
}

void Application::cleanup()
{
    // cleanup() can be called multiple times during shutdown. We only need it once.
    if (m_isCleanupRun.exchange(true, std::memory_order_acquire))
        return;

    LogMsg(tr("RoTorrent termination initiated"));

#ifndef DISABLE_GUI
    // The add-torrent manager goes first, so no dialog it opened is left, then the window: it owns the list
    // adapter, the controller and the details source, and all of them talk to the session.
    delete m_addTorrentManager;
    m_addTorrentManager = nullptr;

#ifdef Q_OS_WIN
    // Windows needs a top-level window handle to attach the "RoTorrent is saving" shutdown block reason to.
    // The main window is deleted below, so a hidden shell window carries it until the very end.
    std::unique_ptr<QWidget> shutdownShell;
    if (m_window)
    {
        shutdownShell = std::make_unique<QWidget>();
        shutdownShell->setWindowFlag(Qt::Tool);
        const std::wstring msg = tr("Saving torrent progress...").toStdWString();
        ::ShutdownBlockReasonCreate(reinterpret_cast<HWND>(shutdownShell->winId()), msg.c_str());
    }
#endif // Q_OS_WIN

    if (m_window)
    {
        // Hide it first so it is not left on screen as an unresponsive window while the session saves.
        m_window->hide();
        delete m_window;
        m_window = nullptr;
    }
#endif // DISABLE_GUI

    TorrentFilesWatcher::freeInstance();
#ifdef DISABLE_GUI
    delete m_addTorrentManager;
#endif
    BitTorrent::Session::freeInstance();
    Net::ReverseResolution::freeInstance();
    Net::GeoIPManager::freeInstance();
    Net::DownloadManager::freeInstance();
    Net::ProxyConfigurationManager::freeInstance();
    Preferences::freeInstance();
    SettingsStorage::freeInstance();
    Utils::Fs::removeDirRecursively(Utils::Fs::tempPath());

    LogMsg(tr("RoTorrent is now ready to exit"));
    Logger::freeInstance();
    delete m_fileLogger;

#if defined(Q_OS_WIN) && !defined(DISABLE_GUI)
    if (shutdownShell)
    {
        ::ShutdownBlockReasonDestroy(reinterpret_cast<HWND>(shutdownShell->winId()));
        shutdownShell.reset();
    }
#endif

    Profile::freeInstance();

    if (m_shutdownAct != ShutdownDialogAction::Exit)
    {
        qDebug() << "Sending computer shutdown/suspend/hibernate signal...";
        Utils::OS::shutdownComputer(m_shutdownAct);
    }
}

AddTorrentManagerImpl *Application::addTorrentManager() const
{
    return m_addTorrentManager;
}


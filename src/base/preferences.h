/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  sledgehammer999 <hammered999@gmail.com>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#pragma once

#include <QtContainerFwd>
#include <QtSystemDetection>
#include <QObject>

#include "base/net/smtpencryptiontype.h"
#include "base/pathfwd.h"
#include "base/utils/net.h"

class QDateTime;
class QNetworkCookie;
class QTime;

namespace Scheduler
{
    Q_NAMESPACE

    enum class Days : int
    {
        EveryDay = 0,
        Weekday = 1,
        Weekend = 2,
        Monday = 3,
        Tuesday = 4,
        Wednesday = 5,
        Thursday = 6,
        Friday = 7,
        Saturday = 8,
        Sunday = 9
    };
    Q_ENUM_NS(Days)
}

class Preferences final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Preferences)

    Preferences();

public:
    static void initInstance();
    static void freeInstance();
    static Preferences *instance();

    // General options
    QString getLocale() const;
    void setLocale(const QString &locale);
    bool useCustomUITheme() const;
    void setUseCustomUITheme(bool use);
    Path customUIThemePath() const;
    void setCustomUIThemePath(const Path &path);
    bool removeTorrentContent() const;
    void setRemoveTorrentContent(bool remove);
    bool confirmOnExit() const;
    void setConfirmOnExit(bool confirm);
    bool speedInTitleBar() const;
    void showSpeedInTitleBar(bool show);
    bool useAlternatingRowColors() const;
    void setAlternatingRowColors(bool b);
    bool useTorrentStatesColors() const;
    void setUseTorrentStatesColors(bool value);
    bool getProgressBarFollowsTextColor() const;
    void setProgressBarFollowsTextColor(bool value);
    bool getHideZeroValues() const;
    void setHideZeroValues(bool b);
    int getHideZeroComboValues() const;
    void setHideZeroComboValues(int n);
    bool isStatusbarDisplayed() const;
    void setStatusbarDisplayed(bool displayed);
    bool isStatusbarFreeDiskSpaceDisplayed() const;
    void setStatusbarFreeDiskSpaceDisplayed(bool displayed);
    bool isStatusbarExternalIPDisplayed() const;
    void setStatusbarExternalIPDisplayed(bool displayed);
    bool isToolbarDisplayed() const;
    void setToolbarDisplayed(bool displayed);
    bool isTorrentContentDragEnabled() const;
    void setTorrentContentDragEnabled(bool enabled);
    bool isSplashScreenDisabled() const;
    void setSplashScreenDisabled(bool b);
    bool preventFromSuspendWhenDownloading() const;
    void setPreventFromSuspendWhenDownloading(bool b);
    bool preventFromSuspendWhenSeeding() const;
    void setPreventFromSuspendWhenSeeding(bool b);
    QString getStyle() const;
    void setStyle(const QString &styleName);
#ifdef Q_OS_WIN
    bool WinStartup() const;
    void setWinStartup(bool b);
#endif

    // Downloads
    Path getScanDirsLastPath() const;
    void setScanDirsLastPath(const Path &path);
    bool isMailNotificationEnabled() const;
    void setMailNotificationEnabled(bool enabled);
    QString getMailNotificationSender() const;
    void setMailNotificationSender(const QString &mail);
    QString getMailNotificationEmail() const;
    void setMailNotificationEmail(const QString &mail);
    QString getMailNotificationSMTP() const;
    void setMailNotificationSMTP(const QString &smtpServer);
    Net::SMTPEncryptionType getMailNotificationSMTPEncryptionType() const;
    void setMailNotificationSMTPEncryptionType(Net::SMTPEncryptionType mailEncryptionType);
    bool getMailNotificationSMTPAuth() const;
    void setMailNotificationSMTPAuth(bool use);
    QString getMailNotificationSMTPUsername() const;
    void setMailNotificationSMTPUsername(const QString &username);
    QString getMailNotificationSMTPPassword() const;
    void setMailNotificationSMTPPassword(const QString &password);
    int getActionOnDblClOnTorrentDl() const;
    void setActionOnDblClOnTorrentDl(int act);
    int getActionOnDblClOnTorrentFn() const;
    void setActionOnDblClOnTorrentFn(int act);

    // Connection options
    QTime getSchedulerStartTime() const;
    void setSchedulerStartTime(const QTime &time);
    QTime getSchedulerEndTime() const;
    void setSchedulerEndTime(const QTime &time);
    Scheduler::Days getSchedulerDays() const;
    void setSchedulerDays(Scheduler::Days days);




    // Advanced settings

    bool isAutoRunOnTorrentAddedEnabled() const;
    void setAutoRunOnTorrentAddedEnabled(bool enabled);
    QString getAutoRunOnTorrentAddedProgram() const;
    void setAutoRunOnTorrentAddedProgram(const QString &program);
    bool isAutoRunOnTorrentFinishedEnabled() const;
    void setAutoRunOnTorrentFinishedEnabled(bool enabled);
    QString getAutoRunOnTorrentFinishedProgram() const;
    void setAutoRunOnTorrentFinishedProgram(const QString &program);
#if defined(Q_OS_WIN)
    bool isAutoRunConsoleEnabled() const;
    void setAutoRunConsoleEnabled(bool enabled);
#endif

    bool shutdownWhenDownloadsComplete() const;
    void setShutdownWhenDownloadsComplete(bool shutdown);
    bool rebootWhenDownloadsComplete() const;
    void setRebootWhenDownloadsComplete(bool reboot);
    bool suspendWhenDownloadsComplete() const;
    void setSuspendWhenDownloadsComplete(bool suspend);
    bool hibernateWhenDownloadsComplete() const;
    void setHibernateWhenDownloadsComplete(bool hibernate);
    bool shutdownqBTWhenDownloadsComplete() const;
    void setShutdownqBTWhenDownloadsComplete(bool shutdown);
    bool dontConfirmAutoExit() const;
    void setDontConfirmAutoExit(bool dontConfirmAutoExit);
    bool recheckTorrentsOnCompletion() const;
    void recheckTorrentsOnCompletion(bool recheck);
    bool resolvePeerCountries() const;
    void resolvePeerCountries(bool resolve);
    bool resolvePeerHostNames() const;
    void resolvePeerHostNames(bool resolve);
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    bool useSystemIcons() const;
    void useSystemIcons(bool enabled);
#endif
    bool isRecursiveDownloadEnabled() const;
    void setRecursiveDownloadEnabled(bool enable);
    int getTrackerPort() const;
    void setTrackerPort(int port);
    bool isTrackerPortForwardingEnabled() const;
    void setTrackerPortForwardingEnabled(bool enabled);
    bool isMarkOfTheWebEnabled() const;
    void setMarkOfTheWebEnabled(bool enabled);
    bool isIgnoreSSLErrors() const;
    void setIgnoreSSLErrors(bool enabled);
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    bool isUpdateCheckEnabled() const;
    void setUpdateCheckEnabled(bool enabled);
#endif
#ifdef Q_OS_MACOS
    bool isSpeedInDockEnabled() const;
    void setSpeedInDockEnabled(bool enabled);
    bool isMacOSMenuBarIconEnabled() const;
    void setMacOSMenuBarIconEnabled(bool enabled);
#endif
    bool confirmTorrentDeletion() const;
    void setConfirmTorrentDeletion(bool enabled);
    bool confirmTorrentRecheck() const;
    void setConfirmTorrentRecheck(bool enabled);
    bool confirmRemoveAllTags() const;
    void setConfirmRemoveAllTags(bool enabled);
    bool confirmMergeTrackers() const;
    void setConfirmMergeTrackers(bool enabled);
    bool confirmRemoveTrackerFromAllTorrents() const;
    void setConfirmRemoveTrackerFromAllTorrents(bool enabled);
#ifndef Q_OS_MACOS
    bool systemTrayEnabled() const;
    void setSystemTrayEnabled(bool enabled);
    bool minimizeToTrayNotified() const;
    void setMinimizeToTrayNotified(bool b);
    bool minimizeToTray() const;
    void setMinimizeToTray(bool b);
    bool closeToTray() const;
    void setCloseToTray(bool b);
    bool closeToTrayNotified() const;
    void setCloseToTrayNotified(bool b);
    bool iconsInMenusEnabled() const;
    void setIconsInMenusEnabled(bool enable);
#endif // Q_OS_MACOS
    qint64 getTorrentFileSizeLimit() const;
    void setTorrentFileSizeLimit(qint64 value);
    int getBdecodeDepthLimit() const;
    void setBdecodeDepthLimit(int value);
    int getBdecodeTokenLimit() const;
    void setBdecodeTokenLimit(int value);

    // Stuff that don't appear in the Options GUI but are saved
    // in the same file.
    QByteArray getMainGeometry() const;
    void setMainGeometry(const QByteArray &geometry);
    bool isFiltersSidebarVisible() const;
    void setFiltersSidebarVisible(bool value);
    int getFiltersSidebarWidth() const;
    void setFiltersSidebarWidth(int value);
    Path getMainLastDir() const;
    void setMainLastDir(const Path &path);
    QByteArray getPeerListState() const;
    void setPeerListState(const QByteArray &state);
    QString getPropSplitterSizes() const;
    void setPropSplitterSizes(const QString &sizes);
    QByteArray getPropFileListState() const;
    void setPropFileListState(const QByteArray &state);
    int getPropCurTab() const;
    void setPropCurTab(int tab);
    bool getPropVisible() const;
    void setPropVisible(bool visible);
    QByteArray getTrackerListState() const;
    void setTrackerListState(const QByteArray &state);
    QString getTorImportLastContentDir() const;
    void setTorImportLastContentDir(const QString &path);
    QByteArray getTorImportGeometry() const;
    void setTorImportGeometry(const QByteArray &geometry);
    bool getStatusFilterState() const;
    bool getCategoryFilterState() const;
    bool getTagFilterState() const;
    bool getTrackerFilterState() const;
    bool getTrackerStatusFilterState() const;
    bool useSeparateTrackerStatusFilter() const;
    void setUseSeparateTrackerStatusFilter(bool value);
    int getTransSelFilter() const;
    void setTransSelFilter(int index);
    bool getHideZeroStatusFilters() const;
    void setHideZeroStatusFilters(bool hide);
    QByteArray getTransHeaderState() const;
    void setTransHeaderState(const QByteArray &state);
    bool getRegexAsFilteringPatternForTransferList() const;
    void setRegexAsFilteringPatternForTransferList(bool checked);
    int getToolbarTextPosition() const;
    void setToolbarTextPosition(int position);

    // Network
    QList<QNetworkCookie> getNetworkCookies() const;
    void setNetworkCookies(const QList<QNetworkCookie> &cookies);

    bool useProxyForBT() const;
    void setUseProxyForBT(bool value);
    bool useProxyForGeneralPurposes() const;
    void setUseProxyForGeneralPurposes(bool value);

    // SpeedWidget
    bool isSpeedWidgetEnabled() const;
    void setSpeedWidgetEnabled(bool enabled);
    int getSpeedWidgetPeriod() const;
    void setSpeedWidgetPeriod(int period);
    bool getSpeedWidgetGraphEnable(int id) const;
    void setSpeedWidgetGraphEnable(int id, bool enable);

    // AddNewTorrentDialog
    bool isAddNewTorrentDialogEnabled() const;
    void setAddNewTorrentDialogEnabled(bool value);
    bool isAddNewTorrentDialogTopLevel() const;
    void setAddNewTorrentDialogTopLevel(bool value);
    int addNewTorrentDialogSavePathHistoryLength() const;
    void setAddNewTorrentDialogSavePathHistoryLength(int value);
    bool isAddNewTorrentDialogAttached() const;
    void setAddNewTorrentDialogAttached(bool attached);

public slots:
    void setStatusFilterState(bool checked);
    void setCategoryFilterState(bool checked);
    void setTagFilterState(bool checked);
    void setTrackerFilterState(bool checked);
    void setTrackerStatusFilterState(bool checked);

    void apply();

signals:
    void changed();

private:
    static Preferences *m_instance;
};

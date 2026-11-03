/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023-2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
 * Copyright (C) 2026  The RoTorrent project (modifications for RoTorrent, see docs/UPSTREAM.md)
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

#include <functional>

#include <QDialog>

#include "base/net/proxytype.h"
#include "base/pathfwd.h"
#include "base/settingvalue.h"
#include "GuiSettings.h"

class QListWidgetItem;

class IApplication;
class AdvancedPage;

namespace Ui
{
    class OptionsDialog;
}

// Tools > Options (handoff 8.13). This is the body of the dialog (a QDialog named roUi, built from
// optionsdialog.ui); OptionsDialog::run() hosts it in the RoDialog shell. Ported from qBittorrent's
// optionsdialog.cpp: the Search, RSS and Web UI pages and the style / custom-theme controls are gone.
class OptionsDialog final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OptionsDialog)

    using ThisType = OptionsDialog;

public:
    // Order of the category list (handoff 8.13).
    enum Tabs
    {
        TAB_UI,
        TAB_DOWNLOADS,
        TAB_CONNECTION,
        TAB_SPEED,
        TAB_BITTORRENT,
        TAB_ADVANCED
    };

    explicit OptionsDialog(IApplication *app, QWidget *parent = nullptr);
    ~OptionsDialog() override;

    // Opens the dialog in its RoDialog shell and returns the dialog result. `page` < 0 reopens the
    // page that was last viewed. Emits settingsApplied() on every Apply and on OK.
    static int run(IApplication *app, QWidget *owner, int page = -1,
                   const std::function<void()> &onApplied = {});

    void showPage(int page);
    QPushButton *applyButton() const { return m_applyButton; }
    bool applySettings();
    AdvancedPage *advancedPage() const { return m_advancedPage; }

signals:
    void settingsApplied();

private slots:
    void adjustProxyOptions();
    void changeSMTPEncryptionPortInfoLabel();
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();
    void enableApplyButton();
    void toggleComboRatioLimitAct();
    void changePage(QListWidgetItem *, QListWidgetItem *);
    void handleWatchedFolderViewSelectionChanged();
    void editWatchedFolderOptions(const QModelIndex &index);
    void on_IpFilterRefreshBtn_clicked();
    void handleIPFilterParsed(bool error, int ruleCount);
    void on_banListButton_clicked();
    void on_randomButton_clicked();
    void on_addWatchedFolderButton_clicked();
    void on_editWatchedFolderButton_clicked();
    void on_removeWatchedFolderButton_clicked();
    void setLocale(const QString &localeStr);

private:
    // Methods
    void saveOptions() const;

    void loadBehaviorTabOptions();
    void saveBehaviorTabOptions() const;

    void loadDownloadsTabOptions();
    void saveDownloadsTabOptions() const;

    void loadConnectionTabOptions();
    void saveConnectionTabOptions() const;

    void loadSpeedTabOptions();
    void saveSpeedTabOptions() const;

    void loadBittorrentTabOptions();
    void saveBittorrentTabOptions() const;

    // General options
    void initializeLanguageCombo();
    void initializeColorSchemeOptions();
    QString getLocale() const;
#ifdef Q_OS_WIN
    bool WinStartup() const;
#endif
    // Downloads
    bool preAllocateAllFiles() const;
    bool useAdditionDialog() const;
    bool addTorrentsStopped() const;
    // Connection options
    int getPort() const;
    bool isUPnPEnabled() const;
    // Bittorrent options
    int getMaxConnections() const;
    int getMaxConnectionsPerTorrent() const;
    int getMaxUploads() const;
    int getMaxUploadsPerTorrent() const;
    bool isDHTEnabled() const;
    bool isLSDEnabled() const;
    int getEncryptionSetting() const;
    qreal getMaxRatio() const;
    int getMaxSeedingMinutes() const;
    int getMaxInactiveSeedingMinutes() const;
    // Proxy options
    bool isProxyEnabled() const;
    QString getProxyIp() const;
    unsigned short getProxyPort() const;
    QString getProxyUsername() const;
    QString getProxyPassword() const;
    Net::ProxyType getProxyType() const;
    // IP Filter
    bool isIPFilteringEnabled() const;
    Path getFilter() const;
    // Queueing system
    bool isQueueingSystemEnabled() const;
    int getMaxActiveDownloads() const;
    int getMaxActiveUploads() const;
    int getMaxActiveTorrents() const;

    bool schedTimesOk();

    IApplication *m_app = nullptr;
    Ui::OptionsDialog *m_ui = nullptr;
    SettingValue<int> m_storeLastViewedPage;

    QPushButton *m_applyButton = nullptr;

    AdvancedPage *m_advancedPage = nullptr;

    bool m_refreshingIpFilter = false;
};

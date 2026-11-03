/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2024 qBittorrent project
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

#include <libtorrent/config.hpp>
#include <libtorrent/version.hpp>

#include <QtSystemDetection>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>

#include <map>
class QFrame;
class QVBoxLayout;

class IApplication;
// Options > Advanced (handoff 8.13): the settings qBittorrent shows in its two-column table, in the same
// order, laid out as two cards ("ROTORRENT" and "LIBTORRENT") of rows with one 272 px control each.
class AdvancedPage final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AdvancedPage)

public:
    explicit AdvancedPage(IApplication *app, QWidget *parent = nullptr);

public slots:
    void saveAdvancedSettings() const;

signals:
    void settingsChanged();

private slots:
    void updateInterfaceAddressCombo();



private:
    void loadAdvancedSettings();
    template <typename T> void addRow(int row, const QString &text, T *widget);
    void buildCards();

    struct Row
    {
        QString text;
        QWidget *widget = nullptr;
    };

    IApplication *m_app = nullptr;
    std::map<int, Row> m_rows;   // keyed by the row id, so the order is qBittorrent's
    QVBoxLayout *m_rootLayout = nullptr;

    QSpinBox m_spinBoxSaveResumeDataInterval, m_spinBoxSaveStatisticsInterval, m_spinBoxTorrentFileSizeLimit, m_spinBoxBdecodeDepthLimit, m_spinBoxBdecodeTokenLimit,
             m_spinBoxAsyncIOThreads, m_spinBoxFilePoolSize, m_spinBoxCheckingMemUsage, m_spinBoxDiskQueueSize,
             m_spinBoxOutgoingPortsMin, m_spinBoxOutgoingPortsMax, m_spinBoxUPnPLeaseDuration, m_spinBoxPeerDSCP, m_spinBoxHostnameCacheTTL,
             m_spinBoxListRefresh, m_spinBoxTrackerPort, m_spinBoxSendBufferWatermark, m_spinBoxSendBufferLowWatermark,
             m_spinBoxSendBufferWatermarkFactor, m_spinBoxConnectionSpeed, m_spinBoxSocketSendBufferSize, m_spinBoxSocketReceiveBufferSize, m_spinBoxSocketBacklogSize,
             m_spinBoxAnnouncePort, m_spinBoxMaxConcurrentHTTPAnnounces, m_spinBoxStopTrackerTimeout, m_spinBoxSessionShutdownTimeout,
             m_spinBoxSavePathHistoryLength, m_spinBoxPeerTurnover, m_spinBoxPeerTurnoverCutoff, m_spinBoxPeerTurnoverInterval, m_spinBoxRequestQueueSize, m_spinBoxMaxOutstandingBlockRequests;
    QCheckBox m_checkBoxOsCache, m_checkBoxRecheckCompleted, m_checkBoxResolveCountries, m_checkBoxResolveHosts,
              m_checkBoxProgramNotifications, m_checkBoxTorrentAddedNotifications, m_checkBoxReannounceWhenAddressChanged, m_checkBoxTrackerStatus,
              m_checkBoxTrackerPortForwarding, m_checkBoxIgnoreSSLErrors, m_checkBoxConfirmTorrentRecheck, m_checkBoxAnnounceAllTrackers,
              m_checkBoxAnnounceAllTiers, m_checkBoxMultiConnectionsPerIp, m_checkBoxMultiConnectionsPerPeerID, m_checkBoxValidateHTTPSTrackerCertificate, m_checkBoxSSRFMitigation, m_checkBoxBlockPeersOnPrivilegedPorts,
              m_checkBoxPieceExtentAffinity, m_checkBoxSuggestMode, m_checkBoxSeedingOutgoingConnections, m_checkBoxIDNSupport,
              m_checkBoxConfirmRemoveTrackerFromAllTorrents, m_checkBoxStartSessionPaused;
    QComboBox m_comboBoxInterface, m_comboBoxInterfaceAddress, m_comboBoxDiskIOReadMode, m_comboBoxDiskIOWriteMode, m_comboBoxUtpMixedMode, m_comboBoxChokingAlgorithm,
              m_comboBoxSeedChokingAlgorithm, m_comboBoxResumeDataStorage, m_comboBoxTorrentContentRemoveOption;
    QLineEdit m_lineEditAppInstanceName, m_lineEditAnnounceIP, m_lineEditDHTBootstrapNodes, m_lineEditWebTorrentSTUNServer;

    QComboBox m_comboBoxDiskIOType;
    QSpinBox m_spinBoxHashingThreads;

#if defined(QBT_USES_LIBTORRENT2) && !defined(Q_OS_LINUX) && !defined(Q_OS_MACOS)
    QSpinBox m_spinBoxMemoryWorkingSetLimit;
#endif

#if defined(QBT_USES_LIBTORRENT2) && TORRENT_USE_I2P
    QSpinBox m_spinBoxI2PInboundQuantity, m_spinBoxI2POutboundQuantity, m_spinBoxI2PInboundLength, m_spinBoxI2POutboundLength;
#if LIBTORRENT_VERSION_NUM >= 20012
    QSpinBox m_spinBoxI2PInboundLengthVariance, m_spinBoxI2POutboundLengthVariance;
#endif // LIBTORRENT_VERSION_NUM >= 20012
#endif

    // OS dependent settings
#ifdef Q_OS_WIN
    QComboBox m_comboBoxOSMemoryPriority;
#endif

#ifdef Q_OS_WIN
    QCheckBox m_checkBoxMarkOfTheWeb;
#endif

};

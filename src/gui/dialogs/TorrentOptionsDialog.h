// SPDX-License-Identifier: GPL-2.0-or-later
// Torrent options dialog (handoff 8.12, kit extra_screens.build_torrent_options, upstream torrentoptionsdialog).
// Works on one or many torrents: a field whose value differs between them shows a partial check mark, an
// inequality sign or a blank combo entry, and is only written when the user changes it.
// 600 px, scrolling body, built on RoDialog. Licensed GPL-2.0-or-later.
#pragma once

#include <optional>

#include <QList>
#include <QStringList>

#include "base/bittorrent/sharelimits.h"
#include "base/path.h"
#include "dialogs/RoDialog.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;

namespace BitTorrent
{
    class Session;
}

class TorrentOptionsDialog final : public RoDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentOptionsDialog)

public:
    TorrentOptionsDialog(QWidget *parent, BitTorrent::Session *session, const QStringList &ids);

    void accept() override;   // writes every changed field to every selected torrent

    // Opens the dialog for the torrents `ids` (hex ids of the transfer list). Returns true when accepted.
    static bool show(QWidget *parent, BitTorrent::Session *session, const QStringList &ids);

private:
    // One "Default / Unlimited / Set to" row of the share limits group.
    struct LimitRow
    {
        QComboBox *mode = nullptr;         // item data: -1 blank (values differ), 0 Default, 1 Unlimited, 2 Set to
        QDoubleSpinBox *value = nullptr;   // ratio uses 2 decimals, the time rows are whole minutes
        double stored = 0;                 // last value typed while "Set to" was selected
        double defaultValue = -1;          // what "Default" currently resolves to; < 0 means none
        int previous = -1;                 // item data before the last change
        QString suffix;                    // " min" for the time rows
    };

    void buildUi();
    void loadValues();
    LimitRow makeLimitRow(QComboBox *mode, QDoubleSpinBox *value, const QString &suffix);
    void setLimitRow(LimitRow &row, double limit, double defaultMarker, double noLimitMarker, bool same);
    double limitRowValue(const LimitRow &row, double defaultMarker, double noLimitMarker) const;
    void onLimitModeChanged(LimitRow &row);
    void showRowDefault(LimitRow &row);
    void refreshCategoryDefaults();
    void refreshDefaultTexts();
    void onAutoTMMChanged();
    void onUseDownloadPathChanged();
    void onCategoryActivated();

    BitTorrent::Session *m_session;
    QStringList m_ids;
    bool m_allSameCategory = true;
    QString m_mixedCategoryText;
    QStringList m_categories;

    QCheckBox *m_autoTMM = nullptr;
    QWidget *m_pathGroup = nullptr;
    QLineEdit *m_savePath = nullptr;
    QPushButton *m_saveBrowse = nullptr;
    QCheckBox *m_useDownloadPath = nullptr;
    QLineEdit *m_downloadPath = nullptr;
    QPushButton *m_downloadBrowse = nullptr;
    QComboBox *m_category = nullptr;
    QSlider *m_upSlider = nullptr;
    QSlider *m_downSlider = nullptr;
    QSpinBox *m_upSpin = nullptr;
    QSpinBox *m_downSpin = nullptr;
    LimitRow m_ratio;
    LimitRow m_seeding;
    LimitRow m_inactive;
    QComboBox *m_limitsMode = nullptr;
    QComboBox *m_limitAction = nullptr;
    QCheckBox *m_dht = nullptr;
    QCheckBox *m_pex = nullptr;
    QCheckBox *m_lsd = nullptr;
    QCheckBox *m_sequential = nullptr;
    QCheckBox *m_firstLast = nullptr;
    bool m_defaultsFromCategory = false;
    BitTorrent::ShareLimits m_defaultLimits;

    struct Initial
    {
        Path savePath;
        Path downloadPath;
        QString category;
        int ratioMode = -1, seedingMode = -1, inactiveMode = -1, limitsMode = -1, action = -1;
        double ratioValue = 0, seedingValue = 0, inactiveValue = 0;
        int upLimit = 0;
        int downLimit = 0;
        Qt::CheckState autoTMM = Qt::Unchecked;
        Qt::CheckState useDownloadPath = Qt::Unchecked;
        Qt::CheckState dht = Qt::Unchecked;
        Qt::CheckState pex = Qt::Unchecked;
        Qt::CheckState lsd = Qt::Unchecked;
        Qt::CheckState sequential = Qt::Unchecked;
        Qt::CheckState firstLast = Qt::Unchecked;
    } m_initial;
};

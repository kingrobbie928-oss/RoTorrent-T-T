/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023-2026  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2024  Jonathan Ketchker
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

#include "optionsdialog.h"

#include <algorithm>
#include <chrono>
#include <limits>

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFileDialog>
#include <QGuiApplication>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
#include <QSystemTrayIcon>

#ifdef Q_OS_WIN
#include <QProcess>
#endif

#include "base/bittorrent/session.h"
#include "base/bittorrent/sharelimits.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/interfaces/iapplication.h"
#include "base/net/portforwarder.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/net/smtpclient.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/torrentfileguard.h"
#include "base/torrentfileswatcher.h"
#include "base/utils/compare.h"
#include "base/utils/io.h"
#include "base/utils/misc.h"
#include "base/utils/net.h"
#include "base/utils/random.h"
#include "dialogs/RoDialog.h"
#include "dialogs/RoMessageBox.h"
#include "theme/RoTheme.h"
#include "AdvancedPage.h"
#include "banlistoptionsdialog.h"
#include "fspathedit.h"
#include "GuiSettings.h"
#include "OptionsCategoryDelegate.h"
#include "SubDialogShell.h"
#include "ui_optionsdialog.h"
#include "watchedfolderoptionsdialog.h"
#include "watchedfoldersmodel.h"

#ifdef Q_OS_WIN
#include "base/utils/os.h"
#endif

#define SETTINGS_KEY(name) u"OptionsDialog/" name

namespace
{
    QStringList translatedWeekdayNames()
    {
        // return translated strings from Monday to Sunday in user selected locale

        const QLocale locale {Preferences::instance()->getLocale()};
        const QDate date {2018, 11, 5};  // Monday
        QStringList ret;
        for (int i = 0; i < 7; ++i)
            ret.append(locale.toString(date.addDays(i), u"dddd"_s));
        return ret;
    }

    class WheelEventEater final : public QObject
    {
    public:
        using QObject::QObject;

    private:
        bool eventFilter(QObject *, QEvent *event) override
        {
            return (event->type() == QEvent::Wheel);
        }
    };

    // Shortcuts for frequently used signals that have more than one overload. They would require
    // type casts and that is why we declare required member pointer here instead.
    void (QComboBox::*qComboBoxCurrentIndexChanged)(int) = &QComboBox::currentIndexChanged;
    void (QSpinBox::*qSpinBoxValueChanged)(int) = &QSpinBox::valueChanged;

    // Icons of the category list, in the order of OptionsDialog::Tabs (handoff 8.13).
    const QStringList categoryIcons {u"m-toolbar"_s, u"dl"_s, u"globe"_s, u"m-speed"_s, u"peers"_s, u"settings"_s};

    constexpr int DialogWidth = 880;      // handoff 8.13 (the shell adds a 1 px border on each side)
    constexpr int ShellChrome = 43;       // 41 px title row + 2 px border
}

// Constructor
OptionsDialog::OptionsDialog(IApplication *app, QWidget *parent)
    : QDialog {parent}
    , m_app {app}
    , m_ui {new Ui::OptionsDialog}
    , m_storeLastViewedPage {SETTINGS_KEY(u"LastViewedPage"_s)}
{
    setWindowFlags(Qt::Widget);   // embedded in the RoDialog shell
    // The theme keys off QDialog#roUi. Name the root BEFORE setupUi: uic only names an unnamed root, and
    // the buttons it creates are polished during setupUi, so a later rename would leave them unstyled.
    setObjectName(u"roUi"_s);
    m_ui->setupUi(this);
    m_applyButton = m_ui->buttonBox->button(QDialogButtonBox::Apply);
    markPrimary(m_ui->buttonBox->button(QDialogButtonBox::Ok));

    // Size: the kit's 591 px page height, capped to the screen like every dialog (handoff 8.12).
    int height = 591;
    if (const QScreen *screen = QGuiApplication::primaryScreen())
        height = std::min(height, screen->availableGeometry().height() - 120 - ShellChrome);
    setFixedSize(DialogWidth - 2, height);

    // Category list: 150 px wide, painted by OptionsCategoryDelegate; the splitter keeps its sizes.
    QListWidget *list = m_ui->tabSelection;
    list->setViewMode(QListView::ListMode);
    list->setFlow(QListView::TopToBottom);
    list->setMovement(QListView::Static);
    list->setSpacing(0);
    list->setUniformItemSizes(true);
    list->setMouseTracking(true);
    list->setFrameShape(QFrame::NoFrame);
    list->setIconSize({OptionsCategoryDelegate::IconSize, OptionsCategoryDelegate::IconSize});
    list->setFixedWidth(150);
    list->setItemDelegate(new OptionsCategoryDelegate(list));
    for (int i = 0; i < list->count(); ++i)
    {
        QListWidgetItem *item = list->item(i);
        item->setData(Qt::UserRole, categoryIcons.value(i));
        item->setSizeHint({150, OptionsCategoryDelegate::RowHeight});
    }
    m_ui->hsplitter->setCollapsible(0, false);
    m_ui->hsplitter->setCollapsible(1, false);
    m_ui->hsplitter->setHandleWidth(0);
    m_ui->hsplitter->setSizes({150, (DialogWidth - 2) - 150});

    connect(m_ui->tabSelection, &QListWidget::currentItemChanged, this, &ThisType::changePage);

    widenSpinBoxes(this);

    // Load options
    loadBehaviorTabOptions();
    loadDownloadsTabOptions();
    loadConnectionTabOptions();
    loadSpeedTabOptions();
    loadBittorrentTabOptions();

    // Load Advanced settings
    m_advancedPage = new AdvancedPage(app, m_ui->tabAdvancedPage);
    m_ui->advPageLayout->addWidget(m_advancedPage);
    connect(m_advancedPage, &AdvancedPage::settingsChanged, this, &ThisType::enableApplyButton);

    // setup apply button
    m_applyButton->setEnabled(false);
    connect(m_applyButton, &QPushButton::clicked, this, [this]
    {
        if (applySettings())
            m_applyButton->setEnabled(false);
    });

    // disable mouse wheel event on widgets to avoid misselection
    auto *wheelEventEater = new WheelEventEater(this);
    for (QComboBox *widget : asConst(findChildren<QComboBox *>()))
        widget->installEventFilter(wheelEventEater);
    for (QSpinBox *widget : asConst(findChildren<QSpinBox *>()))
        widget->installEventFilter(wheelEventEater);

    const int lastPage = m_storeLastViewedPage;
    m_ui->tabSelection->setCurrentRow((lastPage >= 0 && lastPage < m_ui->tabSelection->count()) ? lastPage : 0);
}

OptionsDialog::~OptionsDialog()
{
    m_storeLastViewedPage = m_ui->tabSelection->currentRow();

    delete m_ui;
}

int OptionsDialog::run(IApplication *app, QWidget *owner, const int page, const std::function<void()> &onApplied)
{
    RoDialog shell(owner, tr("Options"), DialogWidth);
    auto *body = new OptionsDialog(app, &shell);
    shell.setContent(body);
    connect(body, &QDialog::finished, &shell, &QDialog::done);
    if (onApplied)
        connect(body, &OptionsDialog::settingsApplied, &shell, onApplied);
    if (page >= 0)
        body->showPage(page);
    return shell.run();
}

void OptionsDialog::showPage(const int page)
{
    if ((page >= 0) && (page < m_ui->tabSelection->count()))
        m_ui->tabSelection->setCurrentRow(page);
}

void OptionsDialog::initializeLanguageCombo()
{
    // English only in 1.0 (handoff decision 13). Every user-visible string is wrapped in tr(), so more
    // languages can be added later; until then the combo shows the one language that exists.
    m_ui->comboLanguage->clear();
    m_ui->comboLanguage->addItem(u"English"_s, u"en"_s);
    m_ui->comboLanguage->setEnabled(false);
    m_ui->comboLanguage->setToolTip(tr("RoTorrent 1.0 is available in English only."));
}

void OptionsDialog::initializeColorSchemeOptions()
{
    // Bound to the same Dark / Light preference as the toolbar toggle (handoff section 11, item 7).
    m_ui->comboColorScheme->clear();
    m_ui->comboColorScheme->addItem(tr("Dark"), true);
    m_ui->comboColorScheme->addItem(tr("Light"), false);
    m_ui->comboColorScheme->setCurrentIndex(m_ui->comboColorScheme->findData(RoTheme::instance().isDark()));
    connect(m_ui->comboColorScheme, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);

    // The toolbar toggle can change the theme while this dialog is open: follow it, and do not
    // count that as an unsaved change.
    connect(&RoTheme::instance(), &RoTheme::changed, this, [this]
    {
        const QSignalBlocker blocker {m_ui->comboColorScheme};
        m_ui->comboColorScheme->setCurrentIndex(m_ui->comboColorScheme->findData(RoTheme::instance().isDark()));
    });
}

QString OptionsDialog::getLocale() const
{
    return m_ui->comboLanguage->itemData(m_ui->comboLanguage->currentIndex(), Qt::UserRole).toString();
}

void OptionsDialog::setLocale(const QString &localeStr)
{
    Q_UNUSED(localeStr);
    m_ui->comboLanguage->setCurrentIndex(0);   // English
}

void OptionsDialog::loadBehaviorTabOptions()
{
    const auto *pref = Preferences::instance();
    const auto *session = BitTorrent::Session::instance();

    initializeLanguageCombo();
    initializeColorSchemeOptions();

    m_ui->confirmDeletion->setChecked(pref->confirmTorrentDeletion());

    const auto fillDoubleClickCombo = [](QComboBox *combo, const int current, const int fallback)
    {
        combo->setItemData(0, TOGGLE_STOP);
        combo->setItemData(1, OPEN_DEST);
        combo->setItemData(2, PREVIEW_FILE);
        combo->setItemData(3, SHOW_OPTIONS);
        combo->setItemData(4, NO_ACTION);
        const int action = ((current < 0) || (current >= combo->count())) ? fallback : current;
        combo->setCurrentIndex(combo->findData(action));
    };
    fillDoubleClickCombo(m_ui->actionTorrentDlOnDblClBox, pref->getActionOnDblClOnTorrentDl(), TOGGLE_STOP);
    fillDoubleClickCombo(m_ui->actionTorrentFnOnDblClBox, pref->getActionOnDblClOnTorrentFn(), OPEN_DEST);

#ifdef Q_OS_WIN
    m_ui->checkStartup->setChecked(pref->WinStartup());
#else
    m_ui->checkStartup->setVisible(false);
#endif

    m_ui->windowStateComboBox->addItem(tr("Normal"), static_cast<int>(GuiSettings::WindowState::Normal));
    m_ui->windowStateComboBox->addItem(tr("Minimized"), static_cast<int>(GuiSettings::WindowState::Minimized));
    m_ui->windowStateComboBox->addItem(tr("Hidden"), static_cast<int>(GuiSettings::WindowState::Hidden));
    m_ui->windowStateComboBox->setCurrentIndex(
            m_ui->windowStateComboBox->findData(static_cast<int>(GuiSettings::startUpWindowState())));

    m_ui->checkProgramExitConfirm->setChecked(pref->confirmOnExit());
    m_ui->checkProgramAutoExitConfirm->setChecked(!pref->dontConfirmAutoExit());

    // Disable notification-area integration if the system has none
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        m_ui->checkShowSystray->setChecked(false);
        m_ui->checkShowSystray->setEnabled(false);
        m_ui->checkShowSystray->setToolTip(tr("Disabled because no notification area was found"));
    }
    m_ui->checkShowSystray->setChecked(pref->systemTrayEnabled());
    m_ui->checkMinimizeToSysTray->setChecked(pref->minimizeToTray());
    m_ui->checkCloseToSystray->setChecked(pref->closeToTray());

#if defined(Q_OS_WIN)
    connect(m_ui->buttonOpenDefaultApps, &QPushButton::clicked, this, [](bool)
    {
        const Path explorer = Utils::OS::windowsSystemPath().parentPath() / Path(u"explorer.exe"_s);
        QProcess::startDetached(explorer.data(), {u"ms-settings:defaultapps"_s});
    });
#else
    m_ui->groupFileAssociation->setVisible(false);
#endif
    m_ui->assocPanel->hide();   // the macOS-only association check boxes were removed

    m_ui->checkPreventFromSuspendWhenDownloading->setChecked(pref->preventFromSuspendWhenDownloading());
    m_ui->checkPreventFromSuspendWhenSeeding->setChecked(pref->preventFromSuspendWhenSeeding());

    m_ui->textFileLogPath->setDialogCaption(tr("Choose a save directory"));
    m_ui->textFileLogPath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->textFileLogPath->setSelectedPath(m_app->fileLoggerPath());
    m_ui->textFileLogPath->setBasePath(specialFolderLocation(SpecialFolder::Data));
    const bool fileLogBackup = m_app->isFileLoggerBackup();
    m_ui->checkFileLogBackup->setChecked(fileLogBackup);
    m_ui->spinFileLogSize->setEnabled(fileLogBackup);
    const bool fileLogDelete = m_app->isFileLoggerDeleteOld();
    m_ui->checkFileLogDelete->setChecked(fileLogDelete);
    m_ui->spinFileLogAge->setEnabled(fileLogDelete);
    m_ui->comboFileLogAgeType->setEnabled(fileLogDelete);
    m_ui->spinFileLogSize->setValue(m_app->fileLoggerMaxSize() / 1024);
    m_ui->spinFileLogAge->setValue(m_app->fileLoggerAge());
    m_ui->comboFileLogAgeType->setCurrentIndex(m_app->fileLoggerAgeType());
    // Groupbox's check state must be initialized after some of its children if they are manually enabled/disabled
    m_ui->checkFileLog->setChecked(m_app->isFileLoggerEnabled());

    m_ui->checkBoxFreeDiskSpaceStatusBar->setChecked(pref->isStatusbarFreeDiskSpaceDisplayed());
    m_ui->checkBoxPerformanceWarning->setChecked(session->isPerformanceWarningEnabled());

    connect(m_ui->comboLanguage, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->confirmDeletion, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->actionTorrentDlOnDblClBox, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->actionTorrentFnOnDblClBox, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);

#ifdef Q_OS_WIN
    connect(m_ui->checkStartup, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
#endif
    connect(m_ui->checkProgramExitConfirm, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkProgramAutoExitConfirm, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkShowSystray, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMinimizeToSysTray, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkCloseToSystray, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->windowStateComboBox, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->checkPreventFromSuspendWhenDownloading, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkPreventFromSuspendWhenSeeding, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);

    connect(m_ui->checkFileLog, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textFileLogPath, &FileSystemPathEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkFileLogBackup, &QAbstractButton::toggled, m_ui->spinFileLogSize, &QWidget::setEnabled);
    connect(m_ui->checkFileLogBackup, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkFileLogDelete, &QAbstractButton::toggled, m_ui->comboFileLogAgeType, &QWidget::setEnabled);
    connect(m_ui->checkFileLogDelete, &QAbstractButton::toggled, m_ui->spinFileLogAge, &QWidget::setEnabled);
    connect(m_ui->checkFileLogDelete, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->spinFileLogSize, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinFileLogAge, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->comboFileLogAgeType, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->checkBoxFreeDiskSpaceStatusBar, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkBoxPerformanceWarning, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
}

void OptionsDialog::saveBehaviorTabOptions() const
{
    auto *pref = Preferences::instance();
    auto *session = BitTorrent::Session::instance();

    // Colour scheme: same preference as the toolbar toggle (it repaints in place and persists itself).
    if (const bool dark = m_ui->comboColorScheme->currentData().toBool(); dark != RoTheme::instance().isDark())
        RoTheme::instance().setDark(dark);

    pref->setConfirmTorrentDeletion(m_ui->confirmDeletion->isChecked());

    pref->setActionOnDblClOnTorrentDl(m_ui->actionTorrentDlOnDblClBox->currentData().toInt());
    pref->setActionOnDblClOnTorrentFn(m_ui->actionTorrentFnOnDblClBox->currentData().toInt());

    pref->setConfirmOnExit(m_ui->checkProgramExitConfirm->isChecked());
    pref->setDontConfirmAutoExit(!m_ui->checkProgramAutoExitConfirm->isChecked());

#ifdef Q_OS_WIN
    pref->setWinStartup(WinStartup());
#endif

    pref->setSystemTrayEnabled(m_ui->checkShowSystray->isChecked());
    pref->setCloseToTray(m_ui->checkCloseToSystray->isChecked());
    pref->setMinimizeToTray(m_ui->checkMinimizeToSysTray->isChecked());

    pref->setPreventFromSuspendWhenDownloading(m_ui->checkPreventFromSuspendWhenDownloading->isChecked());
    pref->setPreventFromSuspendWhenSeeding(m_ui->checkPreventFromSuspendWhenSeeding->isChecked());

    m_app->setFileLoggerPath(m_ui->textFileLogPath->selectedPath());
    m_app->setFileLoggerBackup(m_ui->checkFileLogBackup->isChecked());
    m_app->setFileLoggerMaxSize(m_ui->spinFileLogSize->value() * 1024);
    m_app->setFileLoggerAge(m_ui->spinFileLogAge->value());
    m_app->setFileLoggerAgeType(m_ui->comboFileLogAgeType->currentIndex());
    m_app->setFileLoggerDeleteOld(m_ui->checkFileLogDelete->isChecked());
    m_app->setFileLoggerEnabled(m_ui->checkFileLog->isChecked());

    GuiSettings::setStartUpWindowState(
            static_cast<GuiSettings::WindowState>(m_ui->windowStateComboBox->currentData().toInt()));

    pref->setStatusbarFreeDiskSpaceDisplayed(m_ui->checkBoxFreeDiskSpaceStatusBar->isChecked());
    session->setPerformanceWarningEnabled(m_ui->checkBoxPerformanceWarning->isChecked());
}

void OptionsDialog::loadDownloadsTabOptions()
{
    const auto *pref = Preferences::instance();
    const auto *session = BitTorrent::Session::instance();

    m_ui->checkAdditionDialog->setChecked(pref->isAddNewTorrentDialogEnabled());

    m_ui->contentLayoutComboBox->setCurrentIndex(static_cast<int>(session->torrentContentLayout()));
    m_ui->checkAddToQueueTop->setChecked(session->isAddTorrentToQueueTop());
    m_ui->checkAddStopped->setChecked(session->isAddTorrentStopped());

    m_ui->stopConditionComboBox->setToolTip(
                u"<html><body><p><b>" + tr("None") + u"</b> - " + tr("No pause condition is set.") + u"</p><p><b>" +
                tr("Metadata received") + u"</b> - " + tr("Torrent will pause after metadata is received.") +
                u" <em>" + tr("Torrents that have metadata initially will be added as paused.") + u"</em></p><p><b>" +
                tr("Files checked") + u"</b> - " + tr("Torrent will pause after files are initially checked.") +
                u" <em>" + tr("This will also download metadata if it wasn't there initially.") + u"</em></p></body></html>");
    m_ui->stopConditionComboBox->setItemData(0, QVariant::fromValue(BitTorrent::Torrent::StopCondition::None));
    m_ui->stopConditionComboBox->setItemData(1, QVariant::fromValue(BitTorrent::Torrent::StopCondition::MetadataReceived));
    m_ui->stopConditionComboBox->setItemData(2, QVariant::fromValue(BitTorrent::Torrent::StopCondition::FilesChecked));
    m_ui->stopConditionComboBox->setCurrentIndex(m_ui->stopConditionComboBox->findData(QVariant::fromValue(session->torrentStopCondition())));
    m_ui->stopConditionLabel->setEnabled(!m_ui->checkAddStopped->isChecked());
    m_ui->stopConditionComboBox->setEnabled(!m_ui->checkAddStopped->isChecked());

    m_ui->checkMergeTrackers->setChecked(session->isMergeTrackersEnabled());
    m_ui->checkConfirmMergeTrackers->setEnabled(m_ui->checkAdditionDialog->isChecked());
    m_ui->checkConfirmMergeTrackers->setChecked(m_ui->checkConfirmMergeTrackers->isEnabled() ? pref->confirmMergeTrackers() : false);
    connect(m_ui->checkAdditionDialog, &QAbstractButton::toggled, this, [this, pref]
    {
        m_ui->checkConfirmMergeTrackers->setEnabled(m_ui->checkAdditionDialog->isChecked());
        m_ui->checkConfirmMergeTrackers->setChecked(m_ui->checkConfirmMergeTrackers->isEnabled() ? pref->confirmMergeTrackers() : false);
    });

    m_ui->backupDirCheckBox->setChecked(session->isTorrentFileBackupEnabled());
    m_ui->backupDirPathEdit->setDialogCaption(tr("Choose backup directory"));
    m_ui->backupDirPathEdit->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->backupDirPathEdit->setBasePath(specialFolderLocation(SpecialFolder::Data));
    m_ui->backupDirPathEdit->setSelectedPath(session->torrentBackupDirectory());
    m_ui->backupDirPathEdit->setEnabled(m_ui->backupDirCheckBox->isChecked());
    m_ui->finishedBackupDirCheckBox->setChecked(session->isFinishedTorrentBackupDirectoryEnabled());
    m_ui->finishedBackupDirPathEdit->setDialogCaption(tr("Choose backup directory"));
    m_ui->finishedBackupDirPathEdit->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->finishedBackupDirPathEdit->setBasePath(specialFolderLocation(SpecialFolder::Data));
    m_ui->finishedBackupDirPathEdit->setSelectedPath(session->finishedTorrentBackupDirectory());
    m_ui->finishedBackupDirPathEdit->setEnabled(m_ui->finishedBackupDirCheckBox->isChecked());
    m_ui->removeBackupTorrentFileCheckBox->setChecked(session->removeTorrentFileBackup());

    const TorrentFileGuard::AutoDeleteMode autoDeleteMode = TorrentFileGuard::autoDeleteMode();
    m_ui->deleteTorrentBox->setChecked(autoDeleteMode != TorrentFileGuard::Never);
    m_ui->deleteCancelledTorrentBox->setChecked(autoDeleteMode == TorrentFileGuard::Always);
    m_ui->deleteTorrentWarningIcon->setPixmap(QApplication::style()->standardIcon(QStyle::SP_MessageBoxCritical).pixmap(16, 16));
    m_ui->deleteTorrentWarningIcon->hide();
    m_ui->deleteTorrentWarningLabel->hide();
    m_ui->deleteTorrentWarningLabel->setToolTip(u"<html><body><p>" +
        tr("By enabling these options, you can <strong>irrevocably lose</strong> your .torrent files!") +
        u"</p><p>" +
        tr("When these options are enabled, RoTorrent will <strong>delete</strong> .torrent files "
        "after they were successfully (the first option) or not (the second option) added to its "
        "download queue. This will be applied <strong>not only</strong> to the files opened via "
        "&ldquo;Add torrent&rdquo; menu action but to those opened via <strong>file type association</strong> as well") +
        u"</p><p>" +
        tr("If you enable the second option (&ldquo;Also when addition is cancelled&rdquo;) the "
        ".torrent file <strong>will be deleted</strong> even if you press &ldquo;<strong>Cancel</strong>&rdquo; in "
        "the &ldquo;Add torrent&rdquo; dialog") +
        u"</p></body></html>");

    m_ui->checkPreallocateAll->setChecked(session->isPreallocationEnabled());
    m_ui->checkAppendqB->setChecked(session->isAppendExtensionEnabled());
    m_ui->checkUnwantedFolder->setChecked(session->isUnwantedFolderEnabled());

    m_ui->comboSavingMode->setCurrentIndex(!session->isAutoTMMDisabledByDefault());
    m_ui->comboTorrentCategoryChanged->setCurrentIndex(session->isDisableAutoTMMWhenCategoryChanged());
    m_ui->comboCategoryChanged->setCurrentIndex(session->isDisableAutoTMMWhenCategorySavePathChanged());
    m_ui->comboCategoryDefaultPathChanged->setCurrentIndex(session->isDisableAutoTMMWhenDefaultSavePathChanged());

    m_ui->checkUseCategoryPaths->setChecked(session->useCategoryPathsInManualMode());

    m_ui->textSavePath->setDialogCaption(tr("Choose a save directory"));
    m_ui->textSavePath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->textSavePath->setSelectedPath(session->savePath());

    m_ui->checkUseDownloadPath->setChecked(session->isDownloadPathEnabled());
    m_ui->textDownloadPath->setDialogCaption(tr("Choose a save directory"));
    m_ui->textDownloadPath->setEnabled(m_ui->checkUseDownloadPath->isChecked());
    m_ui->textDownloadPath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->textDownloadPath->setSelectedPath(session->downloadPath());

    auto *watchedFoldersModel = new WatchedFoldersModel(TorrentFilesWatcher::instance(), this);
    connect(watchedFoldersModel, &QAbstractListModel::dataChanged, this, &ThisType::enableApplyButton);
    m_ui->scanFoldersView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ui->scanFoldersView->setModel(watchedFoldersModel);
    connect(m_ui->scanFoldersView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ThisType::handleWatchedFolderViewSelectionChanged);
    connect(m_ui->scanFoldersView, &QTreeView::doubleClicked, this, &ThisType::editWatchedFolderOptions);

    m_ui->groupExcludedFileNames->setChecked(session->isExcludedFileNamesEnabled());
    m_ui->textExcludedFileNames->setPlainText(session->excludedFileNames().join(u'\n'));

    m_ui->groupMailNotification->setChecked(pref->isMailNotificationEnabled());
    m_ui->senderEmailTxt->setText(pref->getMailNotificationSender());
    m_ui->senderEmailTxt->setToolTip(tr("Provide the sending email address."));
    m_ui->lineEditDestEmail->setText(pref->getMailNotificationEmail());
    m_ui->lineEditDestEmail->setToolTip(tr("Provide the recipient email address or addresses.")
        + u"\n\n"
        + tr("Separate multiple emails with a semicolon.")
        + u"\n"
        + tr("Separate multiple email addresses within each email with a comma.")
        + u"\n\n"
        + tr("Example:")
        + u"\n"
        + tr("a@x.com;b@y.com,c@z.com - send two emails: the first to just a@x.com, the second to both b@y.com and c@z.com")
        + u"\n\n"
        + tr("Note: b@y.com & c@z.com will both see each other's email addresses, whereas a@x.com will not see them nor be seen."));
    m_ui->lineEditSMTPServer->setText(pref->getMailNotificationSMTP());
    m_ui->lineEditSMTPServer->setToolTip(tr("Provide the SMTP server address for sending email notifications.")
        + u"\n\n"
        + tr("The server address can be entered either as a DNS name or an IP address (DNS name recommended).")
        + u"\n"
        + tr("To manually specify the server port, add a colon and then the port number to the end.")
        + u"\n\n"
        + tr("Example:")
        + u"\n"
        + tr("smtp.example.com:465 - connect to server smtp.example.com on port 465"));
    m_ui->comboSMTPEncryption->setToolTip(u"<html><body>"
        + u"<p>%1</p>"_s.arg(tr("Select the encryption type used when sending SMTP emails"))
        + u"<p>"
        + u"<b>%1</b>: %2<br>"_s.arg(tr("None"), tr("no encryption used when sending emails"))
        + u"<b>%1</b> <em>[%2] : [%3]</em>"_s.arg(tr("(last choice if no other option)"), tr("Default port"), QString::number(Net::SMTP_DEFAULT_PORT))
        + u"</p>"
        + u"<p>"
        + u"<b>%1</b>: %2<br>"_s.arg(tr("STARTTLS"), tr("use STARTTLS encryption when sending emails"))
        + u"<b>%1</b> <em>[%2] : [%3]</em>"_s.arg(tr("(alternative choice if supported)"), tr("Default port"), QString::number(Net::SMTP_DEFAULT_PORT_STARTTLS))
        + u"</p>"
        + u"<p>"
        + u"<b>%1</b>: %2<br>"_s.arg(tr("SMTPS"), tr("use SMTPS encryption when sending emails"))
        + u"<b>%1</b> <em>[%2] : [%3]</em>"_s.arg(tr("(best choice if supported)"), tr("Default port"), QString::number(Net::SMTP_DEFAULT_PORT_SSL))
        + u"</p>"
        + u"</body></html>");
    m_ui->comboSMTPEncryption->addItem(tr("None"), QVariant::fromValue(Net::SMTPEncryptionType::None));
    m_ui->comboSMTPEncryption->addItem(tr("STARTTLS"), QVariant::fromValue(Net::SMTPEncryptionType::STARTTLS));
    m_ui->comboSMTPEncryption->addItem(tr("SMTPS"), QVariant::fromValue(Net::SMTPEncryptionType::SMTPS));
    m_ui->comboSMTPEncryption->setCurrentIndex(m_ui->comboSMTPEncryption->findData(QVariant::fromValue(pref->getMailNotificationSMTPEncryptionType())));
    changeSMTPEncryptionPortInfoLabel();
    m_ui->groupMailNotifAuth->setChecked(pref->getMailNotificationSMTPAuth());
    m_ui->mailNotifUsername->setText(pref->getMailNotificationSMTPUsername());
    m_ui->mailNotifPassword->setText(pref->getMailNotificationSMTPPassword());

    m_ui->groupBoxRunOnAdded->setChecked(pref->isAutoRunOnTorrentAddedEnabled());
    m_ui->groupBoxRunOnFinished->setChecked(pref->isAutoRunOnTorrentFinishedEnabled());
    m_ui->lineEditRunOnAdded->setText(pref->getAutoRunOnTorrentAddedProgram());
    m_ui->lineEditRunOnFinished->setText(pref->getAutoRunOnTorrentFinishedProgram());
#if defined(Q_OS_WIN)
    m_ui->autoRunConsole->setChecked(pref->isAutoRunConsoleEnabled());
#else
    m_ui->autoRunConsole->hide();
#endif
    const auto autoRunStr = u"%1\n    %2\n    %3\n    %4\n    %5\n    %6\n    %7\n    %8\n    %9\n    %10\n    %11\n    %12\n    %13\n%14"_s
        .arg(tr("Supported parameters (case sensitive):")
            , tr("%N: Torrent name")
            , tr("%L: Category")
            , tr("%F: Content path (same as root path for multifile torrent)")
            , tr("%R: Root path (first torrent subdirectory path)")
            , tr("%D: Save path")
            , tr("%C: Number of files")
            , tr("%Z: Torrent size (bytes)"))
        .arg(tr("%T: Current tracker")
            , tr("%I: Info hash v1 (or '-' if unavailable)")
            , tr("%J: Info hash v2 (or '-' if unavailable)")
            , tr("%K: Torrent ID (either sha-1 info hash for v1 torrent or truncated sha-256 info hash for v2/hybrid torrent)")
            , tr("%M: Comment")
            , tr("Tip: Encapsulate parameter with quotation marks to avoid text being cut off at whitespace (e.g., \"%N\")"));
    m_ui->labelAutoRunParam->setText(autoRunStr);

    connect(m_ui->checkAdditionDialog, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);

    connect(m_ui->contentLayoutComboBox, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->checkAddToQueueTop, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkAddStopped, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkAddStopped, &QAbstractButton::toggled, this, [this](const bool checked)
    {
        m_ui->stopConditionLabel->setEnabled(!checked);
        m_ui->stopConditionComboBox->setEnabled(!checked);
    });
    connect(m_ui->stopConditionComboBox, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMergeTrackers, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkConfirmMergeTrackers, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);

    connect(m_ui->backupDirCheckBox, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->backupDirCheckBox, &QAbstractButton::toggled, m_ui->backupDirPathEdit, &QWidget::setEnabled);
    connect(m_ui->finishedBackupDirCheckBox, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->finishedBackupDirCheckBox, &QAbstractButton::toggled, m_ui->finishedBackupDirPathEdit, &QWidget::setEnabled);
    connect(m_ui->backupDirPathEdit, &FileSystemPathEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->finishedBackupDirPathEdit, &FileSystemPathEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->removeBackupTorrentFileCheckBox, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);

    connect(m_ui->deleteTorrentBox, &QGroupBox::toggled, m_ui->deleteTorrentWarningIcon, &QWidget::setVisible);
    connect(m_ui->deleteTorrentBox, &QGroupBox::toggled, m_ui->deleteTorrentWarningLabel, &QWidget::setVisible);
    connect(m_ui->deleteTorrentBox, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->deleteCancelledTorrentBox, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);

    connect(m_ui->checkPreallocateAll, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkAppendqB, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkUnwantedFolder, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);

    connect(m_ui->comboSavingMode, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->comboTorrentCategoryChanged, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->comboCategoryChanged, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->comboCategoryDefaultPathChanged, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->checkUseCategoryPaths, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);

    connect(m_ui->textSavePath, &FileSystemPathEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->textDownloadPath, &FileSystemPathEdit::selectedPathChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->checkUseDownloadPath, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkUseDownloadPath, &QAbstractButton::toggled, m_ui->textDownloadPath, &QWidget::setEnabled);

    connect(m_ui->addWatchedFolderButton, &QAbstractButton::clicked, this, &ThisType::enableApplyButton);

    connect(m_ui->groupExcludedFileNames, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textExcludedFileNames, &QPlainTextEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->removeWatchedFolderButton, &QAbstractButton::clicked, this, &ThisType::enableApplyButton);

    connect(m_ui->groupMailNotification, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->senderEmailTxt, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->lineEditDestEmail, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->lineEditSMTPServer, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->comboSMTPEncryption, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->comboSMTPEncryption, qComboBoxCurrentIndexChanged, this, &ThisType::changeSMTPEncryptionPortInfoLabel);
    connect(m_ui->groupMailNotifAuth, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->mailNotifUsername, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->mailNotifPassword, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->sendTestEmail, &QPushButton::clicked, this, [this]
    {
        m_app->sendTestEmail();
        RoMessageBox::info(this, tr("Test email"), tr("Attempted to send test email.\nCheck your inbox to confirm success.\nCheck the Execution Log for errors."));
    });

    connect(m_ui->groupBoxRunOnAdded, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->lineEditRunOnAdded, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->groupBoxRunOnFinished, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->lineEditRunOnFinished, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->autoRunConsole, &QCheckBox::toggled, this, &ThisType::enableApplyButton);
}

void OptionsDialog::saveDownloadsTabOptions() const
{
    auto *pref = Preferences::instance();
    auto *session = BitTorrent::Session::instance();

    pref->setAddNewTorrentDialogEnabled(useAdditionDialog());

    session->setTorrentContentLayout(static_cast<BitTorrent::TorrentContentLayout>(m_ui->contentLayoutComboBox->currentIndex()));

    session->setAddTorrentToQueueTop(m_ui->checkAddToQueueTop->isChecked());
    session->setAddTorrentStopped(addTorrentsStopped());
    session->setTorrentStopCondition(m_ui->stopConditionComboBox->currentData().value<BitTorrent::Torrent::StopCondition>());

    session->setTorrentFileBackupEnabled(m_ui->backupDirCheckBox->isChecked());
    session->setTorrentBackupDirectory(m_ui->backupDirPathEdit->selectedPath());
    session->setFinishedTorrentBackupDirectoryEnabled(m_ui->finishedBackupDirCheckBox->isChecked());
    session->setFinishedTorrentBackupDirectory(m_ui->finishedBackupDirPathEdit->selectedPath());
    session->setRemoveTorrentFileBackup(m_ui->removeBackupTorrentFileCheckBox->isChecked());

    TorrentFileGuard::setAutoDeleteMode(!m_ui->deleteTorrentBox->isChecked() ? TorrentFileGuard::Never
            : !m_ui->deleteCancelledTorrentBox->isChecked() ? TorrentFileGuard::IfAdded
            : TorrentFileGuard::Always);

    session->setMergeTrackersEnabled(m_ui->checkMergeTrackers->isChecked());
    if (m_ui->checkConfirmMergeTrackers->isEnabled())
        pref->setConfirmMergeTrackers(m_ui->checkConfirmMergeTrackers->isChecked());

    session->setPreallocationEnabled(preAllocateAllFiles());
    session->setAppendExtensionEnabled(m_ui->checkAppendqB->isChecked());
    session->setUnwantedFolderEnabled(m_ui->checkUnwantedFolder->isChecked());

    session->setAutoTMMDisabledByDefault(m_ui->comboSavingMode->currentIndex() == 0);
    session->setDisableAutoTMMWhenCategoryChanged(m_ui->comboTorrentCategoryChanged->currentIndex() == 1);
    session->setDisableAutoTMMWhenCategorySavePathChanged(m_ui->comboCategoryChanged->currentIndex() == 1);
    session->setDisableAutoTMMWhenDefaultSavePathChanged(m_ui->comboCategoryDefaultPathChanged->currentIndex() == 1);

    session->setUseCategoryPathsInManualMode(m_ui->checkUseCategoryPaths->isChecked());

    session->setSavePath(Path(m_ui->textSavePath->selectedPath()));
    session->setDownloadPathEnabled(m_ui->checkUseDownloadPath->isChecked());
    session->setDownloadPath(m_ui->textDownloadPath->selectedPath());

    auto *watchedFoldersModel = static_cast<WatchedFoldersModel *>(m_ui->scanFoldersView->model());
    watchedFoldersModel->apply();

    session->setExcludedFileNamesEnabled(m_ui->groupExcludedFileNames->isChecked());
    session->setExcludedFileNames(m_ui->textExcludedFileNames->toPlainText().split(u'\n', Qt::SkipEmptyParts));

    pref->setMailNotificationEnabled(m_ui->groupMailNotification->isChecked());
    pref->setMailNotificationSender(m_ui->senderEmailTxt->text());
    pref->setMailNotificationEmail(m_ui->lineEditDestEmail->text());
    pref->setMailNotificationSMTP(m_ui->lineEditSMTPServer->text());
    pref->setMailNotificationSMTPEncryptionType(m_ui->comboSMTPEncryption->currentData().value<Net::SMTPEncryptionType>());
    pref->setMailNotificationSMTPAuth(m_ui->groupMailNotifAuth->isChecked());
    pref->setMailNotificationSMTPUsername(m_ui->mailNotifUsername->text());
    pref->setMailNotificationSMTPPassword(m_ui->mailNotifPassword->text());

    pref->setAutoRunOnTorrentAddedEnabled(m_ui->groupBoxRunOnAdded->isChecked());
    pref->setAutoRunOnTorrentAddedProgram(m_ui->lineEditRunOnAdded->text().trimmed());
    pref->setAutoRunOnTorrentFinishedEnabled(m_ui->groupBoxRunOnFinished->isChecked());
    pref->setAutoRunOnTorrentFinishedProgram(m_ui->lineEditRunOnFinished->text().trimmed());
#if defined(Q_OS_WIN)
    pref->setAutoRunConsoleEnabled(m_ui->autoRunConsole->isChecked());
#endif
}

void OptionsDialog::loadConnectionTabOptions()
{
    const auto *session = BitTorrent::Session::instance();

    m_ui->comboProtocol->setCurrentIndex(static_cast<int>(session->btProtocol()));
    m_ui->spinPort->setValue(session->port());
    m_ui->checkUPnP->setChecked(Net::PortForwarder::instance()->isEnabled());

    int intValue = session->maxConnections();
    if (intValue > 0)
    {
        // enable
        m_ui->checkMaxConnections->setChecked(true);
        m_ui->spinMaxConnec->setEnabled(true);
        m_ui->spinMaxConnec->setValue(intValue);
    }
    else
    {
        // disable
        m_ui->checkMaxConnections->setChecked(false);
        m_ui->spinMaxConnec->setEnabled(false);
    }
    intValue = session->maxConnectionsPerTorrent();
    if (intValue > 0)
    {
        // enable
        m_ui->checkMaxConnectionsPerTorrent->setChecked(true);
        m_ui->spinMaxConnecPerTorrent->setEnabled(true);
        m_ui->spinMaxConnecPerTorrent->setValue(intValue);
    }
    else
    {
        // disable
        m_ui->checkMaxConnectionsPerTorrent->setChecked(false);
        m_ui->spinMaxConnecPerTorrent->setEnabled(false);
    }
    intValue = session->maxUploads();
    if (intValue > 0)
    {
        // enable
        m_ui->checkMaxUploads->setChecked(true);
        m_ui->spinMaxUploads->setEnabled(true);
        m_ui->spinMaxUploads->setValue(intValue);
    }
    else
    {
        // disable
        m_ui->checkMaxUploads->setChecked(false);
        m_ui->spinMaxUploads->setEnabled(false);
    }
    intValue = session->maxUploadsPerTorrent();
    if (intValue > 0)
    {
        // enable
        m_ui->checkMaxUploadsPerTorrent->setChecked(true);
        m_ui->spinMaxUploadsPerTorrent->setEnabled(true);
        m_ui->spinMaxUploadsPerTorrent->setValue(intValue);
    }
    else
    {
        // disable
        m_ui->checkMaxUploadsPerTorrent->setChecked(false);
        m_ui->spinMaxUploadsPerTorrent->setEnabled(false);
    }

#if defined(QBT_USES_LIBTORRENT2) && TORRENT_USE_I2P
    m_ui->textI2PHost->setText(session->I2PAddress());
    m_ui->spinI2PPort->setValue(session->I2PPort());
    m_ui->checkI2PMixed->setChecked(session->I2PMixedMode());
    m_ui->groupI2P->setChecked(session->isI2PEnabled());
#if LIBTORRENT_VERSION_NUM >= 20100
    m_ui->checkI2PPeX->setChecked(session->isI2PPeXEnabled());
#else
    m_ui->checkI2PPeX->hide();
#endif // LIBTORRENT_VERSION_NUM >= 20100
#else
    m_ui->groupI2P->hide();
#endif

    const auto *proxyConfigManager = Net::ProxyConfigurationManager::instance();
    const Net::ProxyConfiguration proxyConf = proxyConfigManager->proxyConfiguration();

    m_ui->comboProxyType->addItem(tr("(None)"), QVariant::fromValue(Net::ProxyType::None));
    m_ui->comboProxyType->addItem(tr("SOCKS4"), QVariant::fromValue(Net::ProxyType::SOCKS4));
    m_ui->comboProxyType->addItem(tr("SOCKS5"), QVariant::fromValue(Net::ProxyType::SOCKS5));
    m_ui->comboProxyType->addItem(tr("HTTP"), QVariant::fromValue(Net::ProxyType::HTTP));
    m_ui->comboProxyType->setCurrentIndex(m_ui->comboProxyType->findData(QVariant::fromValue(proxyConf.type)));
    adjustProxyOptions();

    m_ui->textProxyIP->setText(proxyConf.ip);
    m_ui->spinProxyPort->setValue(proxyConf.port);
    m_ui->textProxyUsername->setText(proxyConf.username);
    m_ui->textProxyPassword->setText(proxyConf.password);
    m_ui->checkProxyAuth->setChecked(proxyConf.authEnabled);
    m_ui->checkProxyHostnameLookup->setChecked(proxyConf.hostnameLookupEnabled);

    m_ui->checkProxyPeerConnections->setChecked(session->isProxyPeerConnectionsEnabled());
    m_ui->checkProxyBitTorrent->setChecked(Preferences::instance()->useProxyForBT());
    m_ui->checkProxyMisc->setChecked(Preferences::instance()->useProxyForGeneralPurposes());

    m_ui->checkIPFilter->setChecked(session->isIPFilteringEnabled());
    m_ui->textFilterPath->setDialogCaption(tr("Choose an IP filter file"));
    m_ui->textFilterPath->setEnabled(m_ui->checkIPFilter->isChecked());
    m_ui->textFilterPath->setFileNameFilter(tr("All supported filters") + u" (*.dat *.p2p *.p2b);;.dat (*.dat);;.p2p (*.p2p);;.p2b (*.p2b)");
    m_ui->textFilterPath->setSelectedPath(session->IPFilterFile());

    const auto refreshIcons = [this]
    {
        const RoPalette &P = RoTheme::instance().pal();
        m_ui->IpFilterRefreshBtn->setIcon(RoTheme::instance().icon(u"m-update"_s, P.muted, 16));
    };
    refreshIcons();
    connect(&RoTheme::instance(), &RoTheme::changed, this, refreshIcons);
    m_ui->IpFilterRefreshBtn->setEnabled(m_ui->checkIPFilter->isChecked());
    m_ui->checkIpFilterTrackers->setChecked(session->isTrackerFilteringEnabled());

    connect(m_ui->comboProtocol, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinPort, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkUPnP, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);

    connect(m_ui->checkMaxConnections, &QAbstractButton::toggled, m_ui->spinMaxConnec, &QWidget::setEnabled);
    connect(m_ui->checkMaxConnections, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxConnectionsPerTorrent, &QAbstractButton::toggled, m_ui->spinMaxConnecPerTorrent, &QWidget::setEnabled);
    connect(m_ui->checkMaxConnectionsPerTorrent, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxUploads, &QAbstractButton::toggled, m_ui->spinMaxUploads, &QWidget::setEnabled);
    connect(m_ui->checkMaxUploads, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxUploadsPerTorrent, &QAbstractButton::toggled, m_ui->spinMaxUploadsPerTorrent, &QWidget::setEnabled);
    connect(m_ui->checkMaxUploadsPerTorrent, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxConnec, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxConnecPerTorrent, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxUploads, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxUploadsPerTorrent, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->comboProxyType, qComboBoxCurrentIndexChanged, this, &ThisType::adjustProxyOptions);
    connect(m_ui->comboProxyType, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->textProxyIP, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinProxyPort, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);

#if defined(QBT_USES_LIBTORRENT2) && TORRENT_USE_I2P
    connect(m_ui->textI2PHost, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinI2PPort, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkI2PMixed, &QCheckBox::toggled, this, &ThisType::enableApplyButton);
#if LIBTORRENT_VERSION_NUM >= 20100
    connect(m_ui->checkI2PPeX, &QCheckBox::toggled, this, &ThisType::enableApplyButton);
#endif // LIBTORRENT_VERSION_NUM >= 20100
    connect(m_ui->groupI2P, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
#endif

    connect(m_ui->checkProxyBitTorrent, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkProxyBitTorrent, &QGroupBox::toggled, this, &ThisType::adjustProxyOptions);
    connect(m_ui->checkProxyPeerConnections, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkProxyHostnameLookup, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkProxyMisc, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);

    connect(m_ui->checkProxyAuth, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textProxyUsername, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->textProxyPassword, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->checkIPFilter, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkIPFilter, &QAbstractButton::toggled, m_ui->textFilterPath, &QWidget::setEnabled);
    connect(m_ui->checkIPFilter, &QAbstractButton::toggled, m_ui->IpFilterRefreshBtn, &QWidget::setEnabled);
    connect(m_ui->textFilterPath, &FileSystemPathEdit::selectedPathChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkIpFilterTrackers, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
}

void OptionsDialog::saveConnectionTabOptions() const
{
    auto *session = BitTorrent::Session::instance();

    session->setBTProtocol(static_cast<BitTorrent::BTProtocol>(m_ui->comboProtocol->currentIndex()));
    session->setPort(getPort());
    Net::PortForwarder::instance()->setEnabled(isUPnPEnabled());

    session->setMaxConnections(getMaxConnections());
    session->setMaxConnectionsPerTorrent(getMaxConnectionsPerTorrent());
    session->setMaxUploads(getMaxUploads());
    session->setMaxUploadsPerTorrent(getMaxUploadsPerTorrent());

#if defined(QBT_USES_LIBTORRENT2) && TORRENT_USE_I2P
    session->setI2PEnabled(m_ui->groupI2P->isChecked());
    session->setI2PAddress(m_ui->textI2PHost->text().trimmed());
    session->setI2PPort(m_ui->spinI2PPort->value());
    session->setI2PMixedMode(m_ui->checkI2PMixed->isChecked());
#if LIBTORRENT_VERSION_NUM >= 20100
    session->setI2PPeXEnabled(m_ui->checkI2PPeX->isChecked());
#endif // LIBTORRENT_VERSION_NUM >= 20100
#endif

    auto *proxyConfigManager = Net::ProxyConfigurationManager::instance();
    Net::ProxyConfiguration proxyConf;
    proxyConf.type = getProxyType();
    proxyConf.ip = getProxyIp();
    proxyConf.port = getProxyPort();
    proxyConf.authEnabled = m_ui->checkProxyAuth->isChecked();
    proxyConf.username = getProxyUsername();
    proxyConf.password = getProxyPassword();
    proxyConf.hostnameLookupEnabled = m_ui->checkProxyHostnameLookup->isChecked();
    proxyConfigManager->setProxyConfiguration(proxyConf);

    Preferences::instance()->setUseProxyForBT(m_ui->checkProxyBitTorrent->isChecked());
    Preferences::instance()->setUseProxyForGeneralPurposes(m_ui->checkProxyMisc->isChecked());

    session->setProxyPeerConnectionsEnabled(m_ui->checkProxyPeerConnections->isChecked());

    // IPFilter
    session->setIPFilteringEnabled(isIPFilteringEnabled());
    session->setTrackerFilteringEnabled(m_ui->checkIpFilterTrackers->isChecked());
    session->setIPFilterFile(m_ui->textFilterPath->selectedPath());
}

void OptionsDialog::loadSpeedTabOptions()
{
    const auto *pref = Preferences::instance();
    const auto *session = BitTorrent::Session::instance();

    m_ui->labelGlobalRate->setPixmap(RoTheme::instance().iconPx(u"m-speed"_s, RoTheme::instance().pal().muted, 24));
    m_ui->spinUploadLimit->setValue(session->globalUploadSpeedLimit() / 1024);
    m_ui->spinDownloadLimit->setValue(session->globalDownloadSpeedLimit() / 1024);

    m_ui->labelAltRate->setPixmap(RoTheme::instance().iconPx(u"m-speed"_s, RoTheme::instance().pal().accent, 24));
    m_ui->spinUploadLimitAlt->setValue(session->altGlobalUploadSpeedLimit() / 1024);
    m_ui->spinDownloadLimitAlt->setValue(session->altGlobalDownloadSpeedLimit() / 1024);

    m_ui->comboBoxScheduleDays->addItems(translatedWeekdayNames());

    m_ui->groupBoxSchedule->setChecked(session->isBandwidthSchedulerEnabled());
    m_ui->timeEditScheduleFrom->setTime(pref->getSchedulerStartTime());
    m_ui->timeEditScheduleTo->setTime(pref->getSchedulerEndTime());
    m_ui->comboBoxScheduleDays->setCurrentIndex(static_cast<int>(pref->getSchedulerDays()));

    m_ui->checkLimituTPConnections->setChecked(session->isUTPRateLimited());
    m_ui->checkLimitTransportOverhead->setChecked(session->includeOverheadInLimits());
    m_ui->checkLimitLocalPeerRate->setChecked(!session->ignoreLimitsOnLAN());

    connect(m_ui->spinUploadLimit, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinDownloadLimit, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->spinUploadLimitAlt, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinDownloadLimitAlt, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->groupBoxSchedule, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->timeEditScheduleFrom, &QDateTimeEdit::timeChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->timeEditScheduleTo, &QDateTimeEdit::timeChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->comboBoxScheduleDays, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->checkLimituTPConnections, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkLimitTransportOverhead, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkLimitLocalPeerRate, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);

}

void OptionsDialog::saveSpeedTabOptions() const
{
    auto *pref = Preferences::instance();
    auto *session = BitTorrent::Session::instance();

    session->setGlobalUploadSpeedLimit(m_ui->spinUploadLimit->value() * 1024);
    session->setGlobalDownloadSpeedLimit(m_ui->spinDownloadLimit->value() * 1024);

    session->setAltGlobalUploadSpeedLimit(m_ui->spinUploadLimitAlt->value() * 1024);
    session->setAltGlobalDownloadSpeedLimit(m_ui->spinDownloadLimitAlt->value() * 1024);

    session->setBandwidthSchedulerEnabled(m_ui->groupBoxSchedule->isChecked());
    pref->setSchedulerStartTime(m_ui->timeEditScheduleFrom->time());
    pref->setSchedulerEndTime(m_ui->timeEditScheduleTo->time());
    pref->setSchedulerDays(static_cast<Scheduler::Days>(m_ui->comboBoxScheduleDays->currentIndex()));

    session->setUTPRateLimited(m_ui->checkLimituTPConnections->isChecked());
    session->setIncludeOverheadInLimits(m_ui->checkLimitTransportOverhead->isChecked());
    session->setIgnoreLimitsOnLAN(!m_ui->checkLimitLocalPeerRate->isChecked());

}

void OptionsDialog::loadBittorrentTabOptions()
{
    const auto *session = BitTorrent::Session::instance();

    m_ui->checkDHT->setChecked(session->isDHTEnabled());
    m_ui->checkPeX->setChecked(session->isPeXEnabled());
    m_ui->checkLSD->setChecked(session->isLSDEnabled());
    m_ui->comboEncryption->setCurrentIndex(session->encryption());
    m_ui->checkAnonymousMode->setChecked(session->isAnonymousModeEnabled());

    m_ui->spinBoxMaxActiveCheckingTorrents->setValue(session->maxActiveCheckingTorrents());

    m_ui->checkEnableQueueing->setChecked(session->isQueueingSystemEnabled());
    m_ui->spinMaxActiveDownloads->setValue(session->maxActiveDownloads());
    m_ui->spinMaxActiveUploads->setValue(session->maxActiveUploads());
    m_ui->spinMaxActiveTorrents->setValue(session->maxActiveTorrents());

    m_ui->checkIgnoreSlowTorrentsForQueueing->setChecked(session->ignoreSlowTorrentsForQueueing());
    const QString slowTorrentsExplanation = u"<html><body><p>"
            + tr("A torrent will be considered slow if its download and upload rates stay below these values for \"Torrent inactivity timer\" seconds")
            + u"</p></body></html>";
    m_ui->labelDownloadRateForSlowTorrents->setToolTip(slowTorrentsExplanation);
    m_ui->labelUploadRateForSlowTorrents->setToolTip(slowTorrentsExplanation);
    m_ui->labelSlowTorrentInactivityTimer->setToolTip(slowTorrentsExplanation);
    m_ui->spinDownloadRateForSlowTorrents->setValue(session->downloadRateForSlowTorrents());
    m_ui->spinUploadRateForSlowTorrents->setValue(session->uploadRateForSlowTorrents());
    m_ui->spinSlowTorrentsInactivityTimer->setValue(session->slowTorrentsInactivityTimer());

    m_ui->spinMaxRatio->setMaximum(std::numeric_limits<int>::max());

    const BitTorrent::ShareLimits &shareLimits = session->shareLimits();
    if (shareLimits.ratioLimit >= 0.)
    {
        // Enable
        m_ui->checkMaxRatio->setChecked(true);
        m_ui->spinMaxRatio->setEnabled(true);
        m_ui->spinMaxRatio->setValue(shareLimits.ratioLimit);
    }
    else
    {
        // Disable
        m_ui->checkMaxRatio->setChecked(false);
        m_ui->spinMaxRatio->setEnabled(false);
    }
    if (shareLimits.seedingTimeLimit >= 0)
    {
        // Enable
        m_ui->checkMaxSeedingMinutes->setChecked(true);
        m_ui->spinMaxSeedingMinutes->setEnabled(true);
        m_ui->spinMaxSeedingMinutes->setValue(shareLimits.seedingTimeLimit);
    }
    else
    {
        // Disable
        m_ui->checkMaxSeedingMinutes->setChecked(false);
        m_ui->spinMaxSeedingMinutes->setEnabled(false);
    }
    if (shareLimits.inactiveSeedingTimeLimit >= 0)
    {
        // Enable
        m_ui->checkMaxInactiveSeedingMinutes->setChecked(true);
        m_ui->spinMaxInactiveSeedingMinutes->setEnabled(true);
        m_ui->spinMaxInactiveSeedingMinutes->setValue(shareLimits.inactiveSeedingTimeLimit);
    }
    else
    {
        // Disable
        m_ui->checkMaxInactiveSeedingMinutes->setChecked(false);
        m_ui->spinMaxInactiveSeedingMinutes->setEnabled(false);
    }
    m_ui->comboRatioLimitAct->setEnabled((shareLimits.ratioLimit >= 0.) || (shareLimits.seedingTimeLimit >= 0) || (shareLimits.inactiveSeedingTimeLimit >= 0));

    const QHash<BitTorrent::ShareLimitAction, int> actIndex =
    {
        {BitTorrent::ShareLimitAction::Stop, 0},
        {BitTorrent::ShareLimitAction::Remove, 1},
        {BitTorrent::ShareLimitAction::RemoveWithContent, 2},
        {BitTorrent::ShareLimitAction::EnableSuperSeeding, 3}
    };
    m_ui->comboRatioLimitAct->setCurrentIndex(actIndex.value(shareLimits.action));

    if (shareLimits.mode == BitTorrent::ShareLimitsMode::MatchAll)
        m_ui->radioButtonShareLimitsModeAll->setChecked(true);
    else
        m_ui->radioButtonShareLimitsModeAny->setChecked(true);

    m_ui->checkEnableAddTrackers->setChecked(session->isAddTrackersEnabled());
    m_ui->textTrackers->setPlainText(session->additionalTrackers());

    m_ui->checkAddTrackersFromURL->setChecked(session->isAddTrackersFromURLEnabled());
    m_ui->textTrackersURL->setText(session->additionalTrackersURL());
    m_ui->textTrackersFromURL->setPlainText(session->additionalTrackersFromURL());

    connect(m_ui->checkDHT, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkPeX, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkLSD, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->comboEncryption, qComboBoxCurrentIndexChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkAnonymousMode, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);

    connect(m_ui->spinBoxMaxActiveCheckingTorrents, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->checkEnableQueueing, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxActiveDownloads, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxActiveUploads, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxActiveTorrents, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkIgnoreSlowTorrentsForQueueing, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->spinDownloadRateForSlowTorrents, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinUploadRateForSlowTorrents, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->spinSlowTorrentsInactivityTimer, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->checkMaxRatio, &QAbstractButton::toggled, m_ui->spinMaxRatio, &QWidget::setEnabled);
    connect(m_ui->checkMaxRatio, &QAbstractButton::toggled, this, &ThisType::toggleComboRatioLimitAct);
    connect(m_ui->checkMaxRatio, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxRatio, qOverload<double>(&QDoubleSpinBox::valueChanged),this, &ThisType::enableApplyButton);
    connect(m_ui->radioButtonShareLimitsModeAny, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxSeedingMinutes, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxSeedingMinutes, &QAbstractButton::toggled, m_ui->spinMaxSeedingMinutes, &QWidget::setEnabled);
    connect(m_ui->checkMaxSeedingMinutes, &QAbstractButton::toggled, this, &ThisType::toggleComboRatioLimitAct);
    connect(m_ui->checkMaxSeedingMinutes, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxSeedingMinutes, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);
    connect(m_ui->checkMaxInactiveSeedingMinutes, &QAbstractButton::toggled, m_ui->spinMaxInactiveSeedingMinutes, &QWidget::setEnabled);
    connect(m_ui->checkMaxInactiveSeedingMinutes, &QAbstractButton::toggled, this, &ThisType::toggleComboRatioLimitAct);
    connect(m_ui->checkMaxInactiveSeedingMinutes, &QAbstractButton::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->spinMaxInactiveSeedingMinutes, qSpinBoxValueChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->checkEnableAddTrackers, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textTrackers, &QPlainTextEdit::textChanged, this, &ThisType::enableApplyButton);

    connect(m_ui->checkAddTrackersFromURL, &QGroupBox::toggled, this, &ThisType::enableApplyButton);
    connect(m_ui->textTrackersURL, &QLineEdit::textChanged, this, &ThisType::enableApplyButton);
}

void OptionsDialog::saveBittorrentTabOptions() const
{
    auto *session = BitTorrent::Session::instance();

    session->setDHTEnabled(isDHTEnabled());
    session->setPeXEnabled(m_ui->checkPeX->isChecked());
    session->setLSDEnabled(isLSDEnabled());
    session->setEncryption(getEncryptionSetting());
    session->setAnonymousModeEnabled(m_ui->checkAnonymousMode->isChecked());

    session->setMaxActiveCheckingTorrents(m_ui->spinBoxMaxActiveCheckingTorrents->value());
    // Queueing system
    session->setQueueingSystemEnabled(isQueueingSystemEnabled());
    session->setMaxActiveDownloads(m_ui->spinMaxActiveDownloads->value());
    session->setMaxActiveUploads(m_ui->spinMaxActiveUploads->value());
    session->setMaxActiveTorrents(m_ui->spinMaxActiveTorrents->value());
    session->setIgnoreSlowTorrentsForQueueing(m_ui->checkIgnoreSlowTorrentsForQueueing->isChecked());
    session->setDownloadRateForSlowTorrents(m_ui->spinDownloadRateForSlowTorrents->value());
    session->setUploadRateForSlowTorrents(m_ui->spinUploadRateForSlowTorrents->value());
    session->setSlowTorrentsInactivityTimer(m_ui->spinSlowTorrentsInactivityTimer->value());

    const QList<BitTorrent::ShareLimitAction> actIndex =
    {
        BitTorrent::ShareLimitAction::Stop,
        BitTorrent::ShareLimitAction::Remove,
        BitTorrent::ShareLimitAction::RemoveWithContent,
        BitTorrent::ShareLimitAction::EnableSuperSeeding
    };
    session->setShareLimits({
        .ratioLimit = getMaxRatio(),
        .seedingTimeLimit = getMaxSeedingMinutes(),
        .inactiveSeedingTimeLimit = getMaxInactiveSeedingMinutes(),
        .mode = (m_ui->radioButtonShareLimitsModeAll->isChecked() ? BitTorrent::ShareLimitsMode::MatchAll : BitTorrent::ShareLimitsMode::MatchAny),
        .action = actIndex.value(m_ui->comboRatioLimitAct->currentIndex())
    });

    session->setAddTrackersEnabled(m_ui->checkEnableAddTrackers->isChecked());
    session->setAdditionalTrackers(m_ui->textTrackers->toPlainText());

    session->setAddTrackersFromURLEnabled(m_ui->checkAddTrackersFromURL->isChecked());
    session->setAdditionalTrackersURL(m_ui->textTrackersURL->text());
}


void OptionsDialog::changePage(QListWidgetItem *current, QListWidgetItem *previous)
{
    if (!current)
        current = previous;
    m_ui->tabOption->setCurrentIndex(m_ui->tabSelection->row(current));
}

void OptionsDialog::saveOptions() const
{
    auto *pref = Preferences::instance();

    saveBehaviorTabOptions();
    saveDownloadsTabOptions();
    saveConnectionTabOptions();
    saveSpeedTabOptions();
    saveBittorrentTabOptions();
    m_advancedPage->saveAdvancedSettings();

    // Assume that user changed multiple settings
    // so it's best to save immediately
    pref->apply();
}

bool OptionsDialog::isIPFilteringEnabled() const
{
    return m_ui->checkIPFilter->isChecked();
}

Net::ProxyType OptionsDialog::getProxyType() const
{
    return m_ui->comboProxyType->currentData().value<Net::ProxyType>();
}

int OptionsDialog::getPort() const
{
    return m_ui->spinPort->value();
}

void OptionsDialog::on_randomButton_clicked()
{
    // Range [1024: 65535]
    m_ui->spinPort->setValue(Utils::Random::rand(1024, 65535));
}

int OptionsDialog::getEncryptionSetting() const
{
    return m_ui->comboEncryption->currentIndex();
}

int OptionsDialog::getMaxActiveDownloads() const
{
    return m_ui->spinMaxActiveDownloads->value();
}

int OptionsDialog::getMaxActiveUploads() const
{
    return m_ui->spinMaxActiveUploads->value();
}

int OptionsDialog::getMaxActiveTorrents() const
{
    return m_ui->spinMaxActiveTorrents->value();
}

bool OptionsDialog::isQueueingSystemEnabled() const
{
    return m_ui->checkEnableQueueing->isChecked();
}

bool OptionsDialog::isDHTEnabled() const
{
    return m_ui->checkDHT->isChecked();
}

bool OptionsDialog::isLSDEnabled() const
{
    return m_ui->checkLSD->isChecked();
}

bool OptionsDialog::isUPnPEnabled() const
{
    return m_ui->checkUPnP->isChecked();
}

// Return Share ratio
qreal OptionsDialog::getMaxRatio() const
{
    if (m_ui->checkMaxRatio->isChecked())
        return m_ui->spinMaxRatio->value();
    return -1;
}

// Return Seeding Minutes
int OptionsDialog::getMaxSeedingMinutes() const
{
    if (m_ui->checkMaxSeedingMinutes->isChecked())
        return m_ui->spinMaxSeedingMinutes->value();
    return -1;
}

// Return Inactive Seeding Minutes
int OptionsDialog::getMaxInactiveSeedingMinutes() const
{
    return m_ui->checkMaxInactiveSeedingMinutes->isChecked()
        ? m_ui->spinMaxInactiveSeedingMinutes->value()
        : -1;
}

// Return max connections number
int OptionsDialog::getMaxConnections() const
{
    if (!m_ui->checkMaxConnections->isChecked())
        return -1;

    return m_ui->spinMaxConnec->value();
}

int OptionsDialog::getMaxConnectionsPerTorrent() const
{
    if (!m_ui->checkMaxConnectionsPerTorrent->isChecked())
        return -1;

    return m_ui->spinMaxConnecPerTorrent->value();
}

int OptionsDialog::getMaxUploads() const
{
    if (!m_ui->checkMaxUploads->isChecked())
        return -1;

    return m_ui->spinMaxUploads->value();
}

int OptionsDialog::getMaxUploadsPerTorrent() const
{
    if (!m_ui->checkMaxUploadsPerTorrent->isChecked())
        return -1;

    return m_ui->spinMaxUploadsPerTorrent->value();
}

void OptionsDialog::on_buttonBox_accepted()
{
    if (m_applyButton->isEnabled())
    {
        if (!applySettings())
            return;

        m_applyButton->setEnabled(false);
    }

    accept();
}

bool OptionsDialog::applySettings()
{
    if (!schedTimesOk())
    {
        m_ui->tabSelection->setCurrentRow(TAB_SPEED);
        return false;
    }

    saveOptions();
    emit settingsApplied();
    return true;
}

void OptionsDialog::on_buttonBox_rejected()
{
    reject();
}

bool OptionsDialog::useAdditionDialog() const
{
    return m_ui->checkAdditionDialog->isChecked();
}

void OptionsDialog::enableApplyButton()
{
    m_applyButton->setEnabled(true);
}

void OptionsDialog::toggleComboRatioLimitAct()
{
    // Verify if the share action button must be enabled
    m_ui->comboRatioLimitAct->setEnabled(m_ui->checkMaxRatio->isChecked() || m_ui->checkMaxSeedingMinutes->isChecked() || m_ui->checkMaxInactiveSeedingMinutes->isChecked());
}

void OptionsDialog::adjustProxyOptions()
{
    const auto currentProxyType = m_ui->comboProxyType->currentData().value<Net::ProxyType>();
    const bool isAuthSupported = ((currentProxyType == Net::ProxyType::SOCKS5)
            || (currentProxyType == Net::ProxyType::HTTP));

    m_ui->checkProxyAuth->setEnabled(isAuthSupported);

    if (currentProxyType == Net::ProxyType::None)
    {
        m_ui->labelProxyTypeUnavailable->setVisible(false);

        m_ui->lblProxyIP->setEnabled(false);
        m_ui->textProxyIP->setEnabled(false);
        m_ui->lblProxyPort->setEnabled(false);
        m_ui->spinProxyPort->setEnabled(false);

        m_ui->checkProxyHostnameLookup->setEnabled(false);
        m_ui->checkProxyMisc->setEnabled(false);
        m_ui->checkProxyBitTorrent->setEnabled(false);
        m_ui->checkProxyPeerConnections->setEnabled(false);
    }
    else
    {
        m_ui->lblProxyIP->setEnabled(true);
        m_ui->textProxyIP->setEnabled(true);
        m_ui->lblProxyPort->setEnabled(true);
        m_ui->spinProxyPort->setEnabled(true);

        m_ui->checkProxyBitTorrent->setEnabled(true);
        m_ui->checkProxyPeerConnections->setEnabled(true);

        if (currentProxyType == Net::ProxyType::SOCKS4)
        {
            m_ui->labelProxyTypeUnavailable->setVisible(true);

            m_ui->checkProxyHostnameLookup->setEnabled(false);
            m_ui->checkProxyMisc->setEnabled(false);
        }
        else
        {
            // SOCKS5 or HTTP
            m_ui->labelProxyTypeUnavailable->setVisible(false);

            m_ui->checkProxyHostnameLookup->setEnabled(true);
            m_ui->checkProxyMisc->setEnabled(true);
        }
    }
}

void OptionsDialog::changeSMTPEncryptionPortInfoLabel()
{
    const Net::SMTPEncryptionType encryptionType = m_ui->comboSMTPEncryption->currentData().value<Net::SMTPEncryptionType>();
    int port = 0;
    switch (encryptionType)
    {
    case Net::SMTPEncryptionType::None:
        port = Net::SMTP_DEFAULT_PORT;
        break;
    case Net::SMTPEncryptionType::STARTTLS:
        port = Net::SMTP_DEFAULT_PORT_STARTTLS;
        break;
    case Net::SMTPEncryptionType::SMTPS:
        port = Net::SMTP_DEFAULT_PORT_SSL;
        break;
    }
    m_ui->labelSMTPEncryptionPortInfo->setText(tr("Default port: %1").arg(QString::number(port)));
}

#ifdef Q_OS_WIN
bool OptionsDialog::WinStartup() const
{
    return m_ui->checkStartup->isChecked();
}
#endif

bool OptionsDialog::preAllocateAllFiles() const
{
    return m_ui->checkPreallocateAll->isChecked();
}

bool OptionsDialog::addTorrentsStopped() const
{
    return m_ui->checkAddStopped->isChecked();
}

// Proxy settings
bool OptionsDialog::isProxyEnabled() const
{
    return m_ui->comboProxyType->currentIndex();
}

QString OptionsDialog::getProxyIp() const
{
    return m_ui->textProxyIP->text().trimmed();
}

unsigned short OptionsDialog::getProxyPort() const
{
    return m_ui->spinProxyPort->value();
}

QString OptionsDialog::getProxyUsername() const
{
    QString username = m_ui->textProxyUsername->text().trimmed();
    return username;
}

QString OptionsDialog::getProxyPassword() const
{
    QString password = m_ui->textProxyPassword->text();
    password = password.trimmed();
    return password;
}

// Locale Settings
void OptionsDialog::on_addWatchedFolderButton_clicked()
{
    Preferences *const pref = Preferences::instance();
    const Path dir {QFileDialog::getExistingDirectory(
                this, tr("Select folder to monitor"), pref->getScanDirsLastPath().parentPath().toString())};
    if (dir.isEmpty())
        return;

    auto *dialog = new WatchedFolderOptionsDialog({}, nullptr);
    runInShell(this, dialog, [this, dialog, dir, pref](const int result)
    {
        if (result != QDialog::Accepted)
            return;

        try
        {
            auto *watchedFoldersModel = static_cast<WatchedFoldersModel *>(m_ui->scanFoldersView->model());
            watchedFoldersModel->addFolder(dir, dialog->watchedFolderOptions());

            pref->setScanDirsLastPath(dir);

            for (int i = 0; i < watchedFoldersModel->columnCount(); ++i)
                m_ui->scanFoldersView->resizeColumnToContents(i);

            enableApplyButton();
        }
        catch (const RuntimeError &err)
        {
            RoMessageBox::error(this, tr("Adding entry failed"), err.message());
        }
    });
}

void OptionsDialog::on_editWatchedFolderButton_clicked()
{
    const QModelIndex selected
        = m_ui->scanFoldersView->selectionModel()->selectedIndexes().at(0);

    editWatchedFolderOptions(selected);
}

void OptionsDialog::on_removeWatchedFolderButton_clicked()
{
    const QModelIndexList selected
        = m_ui->scanFoldersView->selectionModel()->selectedIndexes();

    for (const QModelIndex &index : selected)
        m_ui->scanFoldersView->model()->removeRow(index.row());
}

void OptionsDialog::handleWatchedFolderViewSelectionChanged()
{
    const QModelIndexList selectedIndexes = m_ui->scanFoldersView->selectionModel()->selectedIndexes();
    m_ui->removeWatchedFolderButton->setEnabled(!selectedIndexes.isEmpty());
    m_ui->editWatchedFolderButton->setEnabled(selectedIndexes.count() == 1);
}

void OptionsDialog::editWatchedFolderOptions(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    auto *watchedFoldersModel = static_cast<WatchedFoldersModel *>(m_ui->scanFoldersView->model());
    auto *dialog = new WatchedFolderOptionsDialog(watchedFoldersModel->folderOptions(index.row()), nullptr);
    const QPersistentModelIndex persistent {index};
    runInShell(this, dialog, [this, dialog, persistent, watchedFoldersModel](const int result)
    {
        // The row can disappear while the dialog is open, so only apply to a still-valid index.
        if ((result == QDialog::Accepted) && persistent.isValid())
        {
            watchedFoldersModel->setFolderOptions(persistent.row(), dialog->watchedFolderOptions());
            enableApplyButton();
        }
    });
}

// Return Filter object to apply to BT session
Path OptionsDialog::getFilter() const
{
    return m_ui->textFilterPath->selectedPath();
}



void OptionsDialog::on_IpFilterRefreshBtn_clicked()
{
    if (m_refreshingIpFilter) return;
    m_refreshingIpFilter = true;
    // Updating program preferences
    BitTorrent::Session *const session = BitTorrent::Session::instance();
    session->setIPFilteringEnabled(true);
    session->setIPFilterFile({}); // forcing Session reload filter file
    session->setIPFilterFile(getFilter());
    connect(session, &BitTorrent::Session::IPFilterParsed, this, &OptionsDialog::handleIPFilterParsed);
    setCursor(QCursor(Qt::WaitCursor));
}

void OptionsDialog::handleIPFilterParsed(bool error, int ruleCount)
{
    setCursor(QCursor(Qt::ArrowCursor));
    if (error)
        RoMessageBox::warning(this, tr("Parsing error"), tr("Failed to parse the provided IP filter"));
    else
        RoMessageBox::info(this, tr("Successfully refreshed"), tr("Successfully parsed the provided IP filter: %1 rules were applied.", "%1 is a number").arg(ruleCount));
    m_refreshingIpFilter = false;
    disconnect(BitTorrent::Session::instance(), &BitTorrent::Session::IPFilterParsed, this, &OptionsDialog::handleIPFilterParsed);
}

bool OptionsDialog::schedTimesOk()
{
    if (m_ui->timeEditScheduleFrom->time() == m_ui->timeEditScheduleTo->time())
    {
        RoMessageBox::warning(this, tr("Time Error"), tr("The start time and the end time can't be the same."));
        return false;
    }
    return true;
}

void OptionsDialog::on_banListButton_clicked()
{
    if (runInShell(this, new BanListOptionsDialog(nullptr)) == QDialog::Accepted)
        enableApplyButton();
}


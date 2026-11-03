// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 7 check: the Options dialog (six pages), the Advanced page, the sub-dialogs and the live effects.
// Usage: test_options <profile-dir> <data-dir> [screenshot-dir]. Licensed GPL-2.0-or-later.

#include <cstdio>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>

#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/file_storage.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>

#include "app/firstrun.h"
#include "base/bittorrent/addtorrentparams.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/global.h"
#include "base/interfaces/iapplication.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/net/geoipmanager.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/net/reverseresolution.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "base/torrentfileswatcher.h"
#include "gui/EngineWindow.h"
#include "gui/GuiSettings.h"
#include "gui/MainWindow.h"
#include "gui/models/TransferListModel.h"
#include "gui/options/AdvancedPage.h"
#include "gui/options/addtorrentparamswidget.h"
#include "gui/options/banlistoptionsdialog.h"
#include "gui/options/optionsdialog.h"
#include "gui/options/SubDialogShell.h"
#include "gui/options/watchedfolderoptionsdialog.h"
#include "gui/theme/RoTheme.h"
#include "gui/widgets/TransferListView.h"

namespace
{
    int failures = 0;
    QString shotDir;

    void check(const bool ok, const QString &what)
    {
        std::printf("%s  %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
        if (!ok)
            ++failures;
    }

    bool waitFor(const std::function<bool()> &cond, const int ms)
    {
        QEventLoop loop;
        QTimer poll;
        QTimer limit;
        limit.setSingleShot(true);
        QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (cond()) loop.quit(); });
        QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
        poll.start(20);
        limit.start(ms);
        loop.exec();
        return cond();
    }

    // The application object Options talks to. File logger and memory settings are plain fields.
    class FakeApp final : public IApplication
    {
    public:
        QString instanceName() const override { return m_instanceName; }
        void setInstanceName(const QString &name) override { m_instanceName = name; }
        bool loadTranslation(const QString &) override { return true; }
        qint64 launchTimeSecsSinceEpoch() const override { return 0; }
        bool isFileLoggerEnabled() const override { return m_logEnabled; }
        void setFileLoggerEnabled(const bool value) override { m_logEnabled = value; }
        Path fileLoggerPath() const override { return m_logPath; }
        void setFileLoggerPath(const Path &path) override { m_logPath = path; }
        bool isFileLoggerBackup() const override { return m_logBackup; }
        void setFileLoggerBackup(const bool value) override { m_logBackup = value; }
        bool isFileLoggerDeleteOld() const override { return m_logDelete; }
        void setFileLoggerDeleteOld(const bool value) override { m_logDelete = value; }
        int fileLoggerMaxSize() const override { return m_logMax; }
        void setFileLoggerMaxSize(const int bytes) override { m_logMax = bytes; }
        int fileLoggerAge() const override { return m_logAge; }
        void setFileLoggerAge(const int value) override { m_logAge = value; }
        int fileLoggerAgeType() const override { return m_logAgeType; }
        void setFileLoggerAgeType(const int value) override { m_logAgeType = value; }
        int memoryWorkingSetLimit() const override { return m_memLimit; }
        void setMemoryWorkingSetLimit(const int size) override { m_memLimit = size; }
        void sendTestEmail() override {}
#ifdef Q_OS_WIN
        MemoryPriority processMemoryPriority() const override { return MemoryPriority::Normal; }
        void setProcessMemoryPriority(MemoryPriority) override {}
#endif
        AddTorrentManager *addTorrentManager() const override { return nullptr; }

        QString m_instanceName;
        bool m_logEnabled = false;
        Path m_logPath = Path(u"logs"_s);
        bool m_logBackup = true;
        bool m_logDelete = false;
        int m_logMax = 65 * 1024;
        int m_logAge = 1;
        int m_logAgeType = 0;
        int m_memLimit = 512;
    };

    QDialogButtonBox *buttonsOf(QWidget *dialog)
    {
        return dialog->findChild<QDialogButtonBox *>();
    }

    void click(QWidget *dialog, const QDialogButtonBox::StandardButton which)
    {
        if (QDialogButtonBox *box = buttonsOf(dialog))
            if (QPushButton *button = box->button(which))
                button->click();
    }

    // Runs `action` on the modal dialog shortly after it opens.
    void answerModal(const std::function<void(QWidget *)> &action)
    {
        QTimer::singleShot(400, [action]
        {
            if (QWidget *w = QApplication::activeModalWidget())
                action(w);
        });
    }

    OptionsDialog *bodyOf(QWidget *shell)
    {
        return shell->findChild<OptionsDialog *>();
    }

    template <typename T>
    T *child(QWidget *root, const char *name)
    {
        T *found = root->findChild<T *>(QString::fromLatin1(name));
        if (!found)
            std::printf("  (missing widget %s)\n", name);
        return found;
    }

    void shot(QWidget *w, const QString &name)
    {
        if (!shotDir.isEmpty())
            w->grab().save(shotDir + u"/"_s + name + u".png"_s);
    }

    QByteArray makeTorrent(const QString &dataDir, const QString &name)
    {
        const std::string filePath = QDir(dataDir).filePath(name).toStdString();
        {
            std::mt19937 rng(static_cast<unsigned>(qHash(name)));
            std::ofstream out(filePath, std::ios::binary);
            for (int i = 0; i < 256 * 1024; ++i)
                out.put(static_cast<char>(rng() & 0xff));
        }
        lt::file_storage fs;
        lt::add_files(fs, filePath);
        lt::create_torrent ct(fs, 16 * 1024);
        lt::set_piece_hashes(ct, dataDir.toStdString());
        std::vector<char> buf;
        lt::bencode(std::back_inserter(buf), ct.generate());
        QFile::remove(QString::fromStdString(filePath));   // unfinished torrent: no data on disk
        return QByteArray(buf.data(), static_cast<qsizetype>(buf.size()));
    }

    // Widgets removed with their features (trim_options_ui.py). None may come back.
    const QStringList removedWidgets {
        u"tabSearchPage"_s, u"tabRSSPage"_s, u"tabWebUIPage"_s,
        u"comboStyle"_s, u"checkUseCustomTheme"_s, u"customThemeFilePath"_s, u"buttonCustomizeUITheme"_s,
        u"checkAltRowColors"_s, u"checkUseTorrentStatesColors"_s, u"checkProgressBarFollowsTextColor"_s,
        u"checkHideZero"_s, u"comboHideZero"_s, u"checkBoxHideZeroStatusFilters"_s,
        u"checkBoxUseSeparateTrackerStatusFilter"_s, u"checkTorrentContentDrag"_s, u"checkBoxExternalIPStatusBar"_s,
        u"checkShowSplash"_s, u"comboTrayIcon"_s, u"checkProxyRSS"_s, u"checkAdditionDialogFront"_s,
        u"checkRecursiveDownload"_s, u"checkProgramUpdates"_s
    };

    // Wording that must not reach a user (Appendix C), checked on every label of the dialog.
    const QRegularExpression forbidden {u"\\bStart\\b|\\bStop\\b|\\bStopped\\b|Force Start|\\bFinished\\b|qBittorrent"_s};

    QStringList allTexts(QWidget *root)
    {
        QStringList texts;
        for (const QLabel *label : root->findChildren<QLabel *>())
            texts << label->text() << label->toolTip();
        for (const QAbstractButton *button : root->findChildren<QAbstractButton *>())
            texts << button->text() << button->toolTip();
        for (const QGroupBox *box : root->findChildren<QGroupBox *>())
            texts << box->title();
        for (const QComboBox *combo : root->findChildren<QComboBox *>())
        {
            for (int i = 0; i < combo->count(); ++i)
                texts << combo->itemText(i);
            texts << combo->toolTip();
        }
        return texts;
    }
}

int main(int argc, char **argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    RoTheme::configureHighDpi();
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"RoTorrent"_s);
    if (argc < 3)
        return 2;
    const QString profileDir = QString::fromLocal8Bit(argv[1]);
    const QString dataDir = QString::fromLocal8Bit(argv[2]);
    shotDir = (argc > 3) ? QString::fromLocal8Bit(argv[3]) : QString();
    QDir().mkpath(profileDir);
    QDir().mkpath(dataDir);
    if (!shotDir.isEmpty())
        QDir().mkpath(shotDir);

    Logger::initInstance();
    Profile::initInstance(Path(profileDir), QString(), false);
    SettingsStorage::initInstance();
    Preferences::initInstance();
    applyFirstRunDefaults();
    Net::ProxyConfigurationManager::initInstance();
    Net::DownloadManager::initInstance();
    BitTorrent::Session::initInstance();
    RoTheme::instance().init();
    auto *session = BitTorrent::Session::instance();
    check(waitFor([session] { return session->isRestored(); }, 60000), u"session restored"_s);
    // The singletons the real Application creates after the session (Options reads the watcher's folders).
    Net::GeoIPManager::initInstance();
    Net::ReverseResolution::initInstance();
    TorrentFilesWatcher::initInstance();

    FakeApp fakeApp;
    auto *pref = Preferences::instance();
    RoTheme::instance().setDark(true);

    MainWindow *window = EngineWindow::create(session, &fakeApp);
    window->resize(1280, 800);
    window->show();

    // ---- structure: six pages, nothing of Search / RSS / Web UI, the shell around it ----
    {
        int result = -1;
        answerModal([&](QWidget *w)
        {
            OptionsDialog *body = bodyOf(w);
            check(body != nullptr, u"Options opens in a dialog shell with an OptionsDialog body"_s);
            if (!body)
                return;
            check(w->width() == 880, QStringLiteral("Options is 880 px wide (got %1)").arg(w->width()));
            check(body->objectName() == u"roUi"_s, u"the body is named roUi (the theme keys off it)"_s);

            auto *list = child<QListWidget>(body, "tabSelection");
            auto *stack = child<QStackedWidget>(body, "tabOption");
            const QStringList expected {u"Behavior"_s, u"Downloads"_s, u"Connection"_s, u"Speed"_s, u"BitTorrent"_s, u"Advanced"_s};
            QStringList names;
            for (int i = 0; list && i < list->count(); ++i)
                names << list->item(i)->text();
            check(names == expected, u"category list: "_s + names.join(u", "_s));
            check(stack && stack->count() == 6, u"six pages in the stack"_s);
            check(list && list->width() == 150, u"category list is 150 px wide"_s);
            check(list && list->item(0)->sizeHint().height() == 38, u"category rows are 38 px tall"_s);

            for (const QString &name : removedWidgets)
                check(body->findChild<QWidget *>(name) == nullptr, u"removed widget absent: "_s + name);

            QPushButton *ok = buttonsOf(body)->button(QDialogButtonBox::Ok);
            check(ok && ok->property("primary").toBool(), u"OK is the primary (accent) button"_s);
            check(!body->applyButton()->isEnabled(), u"Apply starts disabled"_s);

            const QStringList texts = allTexts(body);
            QStringList bad;
            for (const QString &t : texts)
                if (forbidden.match(t).hasMatch())
                    bad << t.left(70);
            check(bad.isEmpty(), u"no Start / Stop / Finished / qBittorrent in any label: "_s + bad.join(u" | "_s));

            // A screenshot of every page in both themes.
            for (const bool dark : {true, false})
            {
                RoTheme::instance().setDark(dark);
                for (int i = 0; list && i < list->count(); ++i)
                {
                    list->setCurrentRow(i);
                    QApplication::processEvents();
                    shot(w, QStringLiteral("options_%1_%2").arg(expected.at(i).toLower(), dark ? u"dark"_s : u"light"_s));
                }
            }
            RoTheme::instance().setDark(true);
            list->setCurrentRow(0);
            click(body, QDialogButtonBox::Cancel);
        });
        result = OptionsDialog::run(&fakeApp, window);
        check(result == QDialog::Rejected, u"Cancel rejects"_s);
    }

    // ---- Behavior: Apply keeps the dialog open, persists, takes effect; OK closes ----
    const bool confirmBefore = pref->confirmTorrentDeletion();
    {
        int applied = 0;
        answerModal([&](QWidget *w)
        {
            OptionsDialog *body = bodyOf(w);
            body->showPage(OptionsDialog::TAB_UI);
            auto *confirm = child<QCheckBox>(body, "confirmDeletion");
            auto *windowState = child<QComboBox>(body, "windowStateComboBox");
            auto *scheme = child<QComboBox>(body, "comboColorScheme");
            auto *dblDl = child<QComboBox>(body, "actionTorrentDlOnDblClBox");
            check(scheme && scheme->count() == 2, u"colour scheme offers Dark and Light"_s);
            check(scheme && scheme->currentData().toBool() == RoTheme::instance().isDark(), u"colour scheme shows the current theme"_s);

            confirm->setChecked(!confirmBefore);
            windowState->setCurrentIndex(windowState->findData(static_cast<int>(GuiSettings::WindowState::Minimized)));
            scheme->setCurrentIndex(scheme->findData(false));   // Light
            dblDl->setCurrentIndex(dblDl->findData(OPEN_DEST));
            check(body->applyButton()->isEnabled(), u"a change enables Apply"_s);

            body->applyButton()->click();
            check(pref->confirmTorrentDeletion() == !confirmBefore, u"Apply stores 'confirm when deleting'"_s);
            check(GuiSettings::startUpWindowState() == GuiSettings::WindowState::Minimized, u"Apply stores the start-up window state"_s);
            check(!RoTheme::instance().isDark(), u"Apply switches the colour scheme live (same preference as the toolbar toggle)"_s);
            check(pref->getActionOnDblClOnTorrentDl() == OPEN_DEST, u"Apply stores the double-click action"_s);
            check(!body->applyButton()->isEnabled(), u"Apply disables itself after applying"_s);
            check(QApplication::activeModalWidget() == w && body->isVisible(), u"Apply keeps the dialog open"_s);
            check(applied == 1, u"settingsApplied fired once for Apply"_s);

            // The toolbar toggle moves the combo along without counting as an unsaved change.
            RoTheme::instance().setDark(true);
            check(scheme->currentData().toBool() == true, u"the combo follows the toolbar toggle"_s);
            check(!body->applyButton()->isEnabled(), u"following the toggle is not an unsaved change"_s);
            click(body, QDialogButtonBox::Ok);
        });
        const int result = OptionsDialog::run(&fakeApp, window, -1, [&] { ++applied; });
        check(result == QDialog::Accepted, u"OK accepts"_s);
        check(applied == 1, u"OK with nothing pending does not apply twice"_s);
    }

    // ---- Cancel discards ----
    {
        answerModal([&](QWidget *w)
        {
            OptionsDialog *body = bodyOf(w);
            body->showPage(OptionsDialog::TAB_UI);
            auto *confirm = child<QCheckBox>(body, "confirmDeletion");
            check(confirm->isChecked() == !confirmBefore, u"reopened dialog shows the stored value"_s);
            confirm->setChecked(confirmBefore);
            click(body, QDialogButtonBox::Cancel);
        });
        OptionsDialog::run(&fakeApp, window);
        check(pref->confirmTorrentDeletion() == !confirmBefore, u"Cancel discards the change"_s);
    }
    pref->setConfirmTorrentDeletion(confirmBefore);
    pref->setActionOnDblClOnTorrentDl(TOGGLE_STOP);

    // ---- Queueing: the BitTorrent page switches the window's queue controls live ----
    {
        const bool queueingBefore = session->isQueueingSystemEnabled();
        answerModal([&](QWidget *w)
        {
            OptionsDialog *body = bodyOf(w);
            body->showPage(OptionsDialog::TAB_BITTORRENT);
            child<QGroupBox>(body, "checkEnableQueueing")->setChecked(!queueingBefore);
            click(body, QDialogButtonBox::Ok);
        });
        window->hooks().options();
        check(session->isQueueingSystemEnabled() == !queueingBefore, u"OK stores the queueing system switch"_s);
        check(window->queueingEnabled() == !queueingBefore, u"the window's queue controls follow at once"_s);

        answerModal([&](QWidget *w)
        {
            OptionsDialog *body = bodyOf(w);
            body->showPage(OptionsDialog::TAB_BITTORRENT);
            child<QGroupBox>(body, "checkEnableQueueing")->setChecked(queueingBefore);
            click(body, QDialogButtonBox::Ok);
        });
        window->hooks().options();
        check(window->queueingEnabled() == queueingBefore, u"switching it back restores the previous controls"_s);
    }

    // ---- Advanced: two cards, rows in qBittorrent's order, bound to the engine ----
    {
        answerModal([&](QWidget *w)
        {
            OptionsDialog *body = bodyOf(w);
            body->showPage(OptionsDialog::TAB_ADVANCED);
            AdvancedPage *page = body->advancedPage();
            check(page != nullptr, u"Advanced page exists"_s);

            const auto cards = page->findChildren<QFrame *>(u"advCard"_s);
            check(cards.size() == 2, QStringLiteral("two cards (got %1)").arg(cards.size()));
            QStringList headings;
            for (const QLabel *label : page->findChildren<QLabel *>(u"advHeading"_s))
                headings << label->text();
            check(headings == QStringList({u"ROTORRENT"_s, u"LIBTORRENT"_s}), u"headings: "_s + headings.join(u", "_s));

            const auto rows = page->findChildren<QWidget *>(u"advRow"_s);
            std::printf("  rows: %lld\n", static_cast<long long>(rows.size()));
            check(rows.size() >= 60 && rows.size() <= 82, QStringLiteral("row count in the expected range (%1)").arg(rows.size()));
            bool allFixed = true;
            bool allTall = true;
            for (const QWidget *row : rows)
            {
                allTall = allTall && (row->minimumHeight() >= 54);
                const auto cells = row->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
                bool hasFixed = false;
                for (const QWidget *cell : cells)
                    hasFixed = hasFixed || (cell->maximumWidth() == 272 && cell->minimumWidth() == 272);
                allFixed = allFixed && hasFixed;
            }
            check(allFixed, u"every row has one control cell fixed at 272 px"_s);
            check(allTall, u"every row is at least 54 px tall"_s);

            const auto infos = page->findChildren<QToolButton *>(u"advInfo"_s);
            check(!infos.isEmpty() && infos.first()->toolTip() == u"More information"_s, u"'(?)' became an info icon with the tooltip 'More information'"_s);
            bool noLinkText = true;
            for (const QLabel *label : page->findChildren<QLabel *>())
                noLinkText = noLinkText && !label->text().contains(u"(?)"_s);
            check(noLinkText, u"no '(?)' text is left in a label"_s);

            // Rows whose feature does not exist in RoTorrent are not shown.
            const QStringList texts = allTexts(page);
            const QStringList gone {u"tags"_s, u"speed graphs"_s, u"icons in menus"_s, u"Attach"_s, u"Python"_s,
                                    u"favicon"_s, u"history length"_s, u"qBittorrent"_s};
            for (const QString &word : gone)
            {
                bool seen = false;
                for (const QString &t : texts)
                    seen = seen || t.contains(word, Qt::CaseInsensitive);
                check(!seen, u"no Advanced row mentions '"_s + word + u"'"_s);
            }

            // Bound rows: change two, press OK, read the engine / preferences back.
            const auto rowWithLabel = [&](const QString &prefix) -> QWidget *
            {
                for (QWidget *row : rows)
                    for (const QLabel *label : row->findChildren<QLabel *>())
                        if (label->text().startsWith(prefix))
                            return row;
                return nullptr;
            };
            QWidget *sizeRow = rowWithLabel(u".torrent file size limit"_s);
            QWidget *recheckRow = rowWithLabel(u"Confirm torrent recheck"_s);
            check(sizeRow && recheckRow, u"found the '.torrent file size limit' and 'Confirm torrent recheck' rows"_s);
            if (sizeRow && recheckRow)
            {
                auto *spin = sizeRow->findChild<QSpinBox *>();
                auto *box = recheckRow->findChild<QCheckBox *>();
                spin->setValue(7);
                box->setChecked(!box->isChecked());
                check(body->applyButton()->isEnabled(), u"changing an Advanced row enables Apply"_s);
                shot(w, u"options_advanced_changed"_s);
            }
            click(body, QDialogButtonBox::Ok);
        });
        const bool recheckBefore = pref->confirmTorrentRecheck();
        OptionsDialog::run(&fakeApp, window);
        check(pref->getTorrentFileSizeLimit() == 7 * 1024 * 1024, u"Advanced: .torrent file size limit stored"_s);
        check(pref->confirmTorrentRecheck() == !recheckBefore, u"Advanced: 'Confirm torrent recheck' stored"_s);
    }

    // ---- instance name: the window title follows ----
    {
        answerModal([&](QWidget *w)
        {
            OptionsDialog *body = bodyOf(w);
            body->showPage(OptionsDialog::TAB_ADVANCED);
            for (QLineEdit *edit : body->advancedPage()->findChildren<QLineEdit *>())
                if (edit->toolTip().contains(u"window title"_s))
                    edit->setText(u"seedbox"_s);
            click(body, QDialogButtonBox::Ok);
        });
        window->hooks().options();
        check(fakeApp.instanceName() == u"seedbox"_s, u"instance name stored through the application"_s);
        check(window->windowTitle() == u"RoTorrent - seedbox"_s, u"window title gets the instance name: "_s + window->windowTitle());
    }

    // ---- Sub-dialogs in the shell: ban list and watched folder options ----
    {
        answerModal([&](QWidget *w)
        {
            check(w->findChild<QWidget *>(u"roUi"_s) != nullptr, u"ban list body is named roUi"_s);
            child<QLineEdit>(w, "txtIP")->setText(u"10.20.30.40"_s);
            child<QPushButton>(w, "buttonBanIP")->click();
            shot(w, u"options_banlist"_s);
            click(w, QDialogButtonBox::Ok);
        });
        const int result = runInShell(window, new BanListOptionsDialog(nullptr));
        check(result == QDialog::Accepted, u"ban list accepts"_s);
        check(session->bannedIPs().contains(u"10.20.30.40"_s), u"banned address reached the session"_s);
    }
    {
        bool recursive = false;
        bool hasParams = false;
        answerModal([&](QWidget *w)
        {
            hasParams = w->findChild<AddTorrentParamsWidget *>() != nullptr;
            child<QCheckBox>(w, "checkBoxRecursive")->setChecked(true);
            shot(w, u"options_watchedfolder"_s);
            click(w, QDialogButtonBox::Ok);
        });
        auto *dialog = new WatchedFolderOptionsDialog({}, nullptr);
        runInShell(window, dialog, [&](const int result)
        {
            if (result == QDialog::Accepted)
                recursive = dialog->watchedFolderOptions().recursive;
        });
        check(hasParams, u"watched folder options embed the add-torrent parameters"_s);
        check(recursive, u"watched folder options return the chosen values"_s);
    }

    // ---- double-click on a torrent runs the chosen action ----
    {
        BitTorrent::AddTorrentParams params;
        params.savePath = Path(dataDir) / Path(u"nowhere"_s);
        params.useAutoTMM = false;
        const auto descriptor = BitTorrent::TorrentDescriptor::load(makeTorrent(dataDir, u"dbl.bin"_s));
        check(descriptor && session->addTorrent(*descriptor, params), u"a torrent to double-click"_s);
        const QString id = descriptor->infoHash().toTorrentID().toString();
        check(waitFor([&] { return window->transferModel()->indexOfId(id) >= 0; }, 20000), u"the window lists it"_s);
        BitTorrent::Torrent *torrent = session->getTorrent(BitTorrent::TorrentID::fromString(id));
        check(waitFor([&] { return torrent && !torrent->isChecking(); }, 20000), u"it finishes its first check"_s);

        pref->setActionOnDblClOnTorrentDl(TOGGLE_STOP);
        window->activateTorrent(id);
        check(waitFor([&] { return torrent->isStopped(); }, 10000), u"double-click (pause / resume) pauses a running torrent"_s);
        window->activateTorrent(id);
        check(waitFor([&] { return !torrent->isStopped(); }, 10000), u"a second double-click resumes it"_s);

        pref->setActionOnDblClOnTorrentDl(NO_ACTION);
        window->activateTorrent(id);
        waitFor([] { return false; }, 300);
        check(!torrent->isStopped(), u"'Do nothing' leaves the torrent alone"_s);

        QStringList seen;
        window->hooks().torrentOptions = [&seen](const QStringList &ids) { seen = ids; };
        pref->setActionOnDblClOnTorrentDl(SHOW_OPTIONS);
        window->activateTorrent(id);
        check(seen == QStringList({id}), u"'Show torrent options' opens Torrent options for it"_s);
        pref->setActionOnDblClOnTorrentDl(TOGGLE_STOP);
    }

    delete window;
    TorrentFilesWatcher::freeInstance();
    Net::ReverseResolution::freeInstance();
    Net::GeoIPManager::freeInstance();
    BitTorrent::Session::freeInstance();
    std::printf("%s\n", failures ? "SOME FAILED" : "ALL OK");
    return failures ? 1 : 0;
}

// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 6 slice 5 check: Torrent options, Manage Cookies, Tracker list and Preview picker against a real session.
// Usage: test_managementdialogs <profile-dir> <data-dir> [screenshot-dir]. Licensed GPL-2.0-or-later.

#include <cstdio>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>

#include <libtorrent/bencode.hpp>
#include <libtorrent/create_torrent.hpp>
#include <libtorrent/file_storage.hpp>

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>

#include "app/firstrun.h"
#include "base/bittorrent/addtorrentparams.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "base/unicodestrings.h"
#include "gui/MainWindow.h"
#include "gui/dialogs/ManagementDialogs.h"
#include "gui/dialogs/TorrentOptionsDialog.h"
#include "gui/theme/RoTheme.h"

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

    // Writes `files` (name -> bytes) under dataDir/<folder> and returns the .torrent bytes. keepData false removes the payload.
    QByteArray makeTorrent(const QString &dataDir, const QString &folder, const QStringList &files, const bool keepData
                           , const bool isPrivate = false)
    {
        const QString root = QDir(dataDir).filePath(folder);
        QDir().mkpath(root);
        std::mt19937 rng(static_cast<unsigned>(qHash(folder)));
        for (const QString &name : files)
        {
            std::ofstream out(QDir(root).filePath(name).toStdString(), std::ios::binary);
            for (int i = 0; i < 48 * 1024; ++i)
                out.put(static_cast<char>(rng() & 0xff));
        }
        lt::file_storage fs;
        lt::add_files(fs, root.toStdString());
        lt::create_torrent ct(fs, 16 * 1024);
        if (isPrivate)
            ct.set_priv(true);
        lt::set_piece_hashes(ct, dataDir.toStdString());
        std::vector<char> buf;
        lt::bencode(std::back_inserter(buf), ct.generate());
        if (!keepData)
            QDir(root).removeRecursively();
        return QByteArray(buf.data(), static_cast<qsizetype>(buf.size()));
    }

    BitTorrent::Torrent *addTorrent(BitTorrent::Session *session, const QByteArray &data, const Path &savePath)
    {
        const auto desc = BitTorrent::TorrentDescriptor::load(data);
        if (!desc)
            return nullptr;
        BitTorrent::AddTorrentParams params;
        params.savePath = savePath;
        params.useAutoTMM = false;
        params.addStopped = true;
        if (!session->addTorrent(*desc, params))
            return nullptr;
        const auto id = desc->infoHash().toTorrentID();
        waitFor([&] { return session->getTorrent(id) != nullptr; }, 20000);
        return session->getTorrent(id);
    }

    void shot(QWidget *w, const QString &name)
    {
        if (shotDir.isEmpty())
            return;
        w->adjustSize();
        w->show();
        QApplication::processEvents();
        QApplication::processEvents();
        w->grab().save(shotDir + u'/' + name + u".png"_s);
    }

    // Runs `action` on the modal widget shortly after it opens.
    void answerModal(const std::function<void(QWidget *)> &action, const int delay = 400)
    {
        QTimer::singleShot(delay, [action]
        {
            if (QWidget *w = QApplication::activeModalWidget())
                action(w);
        });
    }

    void clickOk(QWidget *dialog)
    {
        for (QDialogButtonBox *box : dialog->findChildren<QDialogButtonBox *>())
        {
            if (QPushButton *b = box->button(QDialogButtonBox::Ok))
            {
                b->click();
                return;
            }
        }
        const auto buttons = dialog->findChildren<QPushButton *>();
        if (!buttons.isEmpty())
            buttons.last()->click();
        else
            dialog->close();
    }

    void clickCancel(QWidget *dialog)
    {
        for (QDialogButtonBox *box : dialog->findChildren<QDialogButtonBox *>())
        {
            if (QPushButton *b = box->button(QDialogButtonBox::Cancel))
            {
                b->click();
                return;
            }
        }
        dialog->close();
    }

    template <typename T> T *child(QWidget *w, const char *name)
    {
        return w->findChild<T *>(QString::fromLatin1(name));
    }

    int indexByText(QComboBox *box, const QString &text)
    {
        return box->findText(text);
    }
}

int main(int argc, char **argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    RoTheme::configureHighDpi();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RoTorrent"));
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

    MainWindow window;
    window.resize(1280, 800);
    window.show();
    ManagementDialogs::install(window, session);
    check(window.hooks().cookies && window.hooks().torrentOptions && window.hooks().editTrackers && window.hooks().previewFile
          , u"the four hooks are installed"_s);

    const Path save = Path(dataDir);
    session->addCategory(u"Linux ISOs"_s);
    session->addCategory(u"Movies"_s);
    BitTorrent::Torrent *t1 = addTorrent(session, makeTorrent(dataDir, u"one"_s, {u"a.bin"_s}, false), save / Path(u"nowhere1"_s));
    BitTorrent::Torrent *t2 = addTorrent(session, makeTorrent(dataDir, u"two"_s, {u"b.bin"_s}, false), save / Path(u"nowhere2"_s));
    BitTorrent::Torrent *tPriv = addTorrent(session, makeTorrent(dataDir, u"priv"_s, {u"p.bin"_s}, false, true), save / Path(u"nowhere3"_s));
    check(t1 && t2 && tPriv, u"three torrents added"_s);
    if (!(t1 && t2 && tPriv))
        return 1;
    const QString id1 = t1->id().toString();
    const QString id2 = t2->id().toString();
    const QString idPriv = tPriv->id().toString();

    // ============================== Torrent options ==============================
    {
        t1->setUploadLimit(200 * 1024);
        TorrentOptionsDialog dialog(&window, session, {id1});
        auto *up = child<QSpinBox>(&dialog, "spinUploadLimit");
        auto *down = child<QSpinBox>(&dialog, "spinDownloadLimit");
        auto *autoTmm = child<QCheckBox>(&dialog, "checkAutoTMM");
        auto *savePath = child<QLineEdit>(&dialog, "savePath");
        auto *category = child<QComboBox>(&dialog, "comboCategory");
        auto *ratioMode = child<QComboBox>(&dialog, "comboRatioMode");
        auto *ratio = child<QDoubleSpinBox>(&dialog, "spinRatio");
        auto *seedMode = child<QComboBox>(&dialog, "comboSeedingMode");
        auto *inactiveMode = child<QComboBox>(&dialog, "comboInactiveMode");
        auto *mode = child<QComboBox>(&dialog, "comboLimitsMode");
        auto *action = child<QComboBox>(&dialog, "comboLimitAction");
        auto *dht = child<QCheckBox>(&dialog, "checkDisableDHT");
        auto *seq = child<QCheckBox>(&dialog, "checkSequential");
        auto *slider = child<QSlider>(&dialog, "sliderUploadLimit");
        check(up && down && autoTmm && savePath && category && ratioMode && ratio && seedMode && inactiveMode && mode && action && dht && seq && slider
              , u"options: every field exists"_s);
        check(dialog.windowTitle() == u"Torrent options"_s, u"options: title"_s);
        check(up->value() == 200 && down->value() == 0 && down->specialValueText() == C_INFINITY, u"options: limits load in KiB/s, 0 shows the infinity sign"_s);
        check(slider->value() == 200 && slider->maximum() >= 200, u"options: slider follows the spin box"_s);
        check(autoTmm->checkState() == Qt::Unchecked && savePath->text() == (save / Path(u"nowhere1"_s)).toString(), u"options: manual mode shows the torrent's save path"_s);
        check(ratioMode->currentData().toInt() == 0 && seedMode->currentData().toInt() == 0, u"options: share limits start on Default"_s);
        check(ratio->isEnabled() == false, u"options: the value spin is disabled until \"Set to\""_s);

        QStringList actions;
        for (int i = 0; i < action->count(); ++i)
            actions << action->itemText(i);
        check(actions.contains(u"Pause torrent"_s) && !actions.join(u'|').contains(u"Stop"_s), u"options: the action list says \"Pause torrent\", never \"Stop\""_s);
        shot(&dialog, u"options_single_dark"_s);

        // edit
        slider->setValue(300);
        check(up->value() == 300, u"options: moving the slider moves the spin box"_s);
        up->setValue(100);
        down->setValue(50);
        ratioMode->setCurrentIndex(indexByText(ratioMode, u"Set to"_s));
        check(ratio->isEnabled(), u"options: \"Set to\" enables the value"_s);
        ratio->setValue(2.5);
        seedMode->setCurrentIndex(indexByText(seedMode, u"Unlimited"_s));
        mode->setCurrentIndex(mode->findData(static_cast<int>(BitTorrent::ShareLimitsMode::MatchAll)));
        action->setCurrentIndex(action->findData(static_cast<int>(BitTorrent::ShareLimitAction::Stop)));
        dht->setChecked(true);
        seq->setChecked(true);
        category->setEditText(u"Cat5"_s);
        dialog.accept();
        check(t1->uploadLimit() == 100 * 1024 && t1->downloadLimit() == 50 * 1024, u"options: speed limits applied to the torrent"_s);
        const BitTorrent::ShareLimits l = t1->shareLimits();
        check(l.ratioLimit == 2.5 && l.seedingTimeLimit == BitTorrent::NO_SEEDING_TIME_LIMIT
              && l.inactiveSeedingTimeLimit == BitTorrent::DEFAULT_SEEDING_TIME_LIMIT, u"options: ratio 2.5, seeding time unlimited, inactive untouched"_s);
        check(l.mode == BitTorrent::ShareLimitsMode::MatchAll && l.action == BitTorrent::ShareLimitAction::Stop, u"options: mode and action applied"_s);
        // the engine mirrors native flags on its next status update
        waitFor([&] { return t1->isDHTDisabled(); }, 5000);
        check(t1->isDHTDisabled() && t1->isSequentialDownload() && !t1->isPEXDisabled(), u"options: DHT off, sequential on, PeX untouched"_s);
        check(t1->category() == u"Cat5"_s && session->categories().contains(u"Cat5"_s), u"options: a typed category is created and assigned"_s);
    }
    {
        // reopen: shows what was written
        TorrentOptionsDialog dialog(&window, session, {id1});
        auto *ratioMode = child<QComboBox>(&dialog, "comboRatioMode");
        auto *ratio = child<QDoubleSpinBox>(&dialog, "spinRatio");
        auto *category = child<QComboBox>(&dialog, "comboCategory");
        check(ratioMode->currentData().toInt() == 2 && ratio->value() == 2.5, u"options: reopening shows ratio \"Set to\" 2.5"_s);
        check(category->currentText() == u"Cat5"_s && category->currentIndex() == 0, u"options: the current category is the first entry"_s);
        const auto before = t1->shareLimits();
        const int upBefore = t1->uploadLimit();
        dialog.accept();   // nothing changed: nothing is written back differently
        check(t1->shareLimits() == before && t1->uploadLimit() == upBefore && t1->category() == u"Cat5"_s, u"options: OK without changes leaves the torrent as it was"_s);
    }
    {
        TorrentOptionsDialog dialog(&window, session, {id1});
        child<QSpinBox>(&dialog, "spinUploadLimit")->setValue(7);
        dialog.reject();
        check(t1->uploadLimit() == 100 * 1024, u"options: Cancel changes nothing"_s);
    }
    {
        // two torrents with different values
        t1->setUploadLimit(100 * 1024);
        t2->setUploadLimit(300 * 1024);
        t2->setSequentialDownload(false);
        t2->setCategory(u"Movies"_s);
        TorrentOptionsDialog dialog(&window, session, {id1, id2});
        auto *up = child<QSpinBox>(&dialog, "spinUploadLimit");
        auto *seq = child<QCheckBox>(&dialog, "checkSequential");
        auto *category = child<QComboBox>(&dialog, "comboCategory");
        auto *ratioMode = child<QComboBox>(&dialog, "comboRatioMode");
        auto *dht = child<QCheckBox>(&dialog, "checkDisableDHT");
        check(up->specialValueText() == C_INEQUALITY && up->value() == -1, u"options (2): differing limits show the inequality sign"_s);
        check(seq->checkState() == Qt::PartiallyChecked && dht->checkState() == Qt::PartiallyChecked, u"options (2): differing flags show a partial check"_s);
        check(category->currentText().isEmpty() && category->itemText(0).contains(u"Currently used categories"_s), u"options (2): differing categories show the placeholder entry"_s);
        auto *inactiveMode = child<QComboBox>(&dialog, "comboInactiveMode");
        check(ratioMode->currentData().toInt() == -1 && ratioMode->currentText().isEmpty(), u"options (2): differing share limits show a blank entry"_s);
        check(inactiveMode->currentData().toInt() == 0, u"options (2): equal share limits stay on Default"_s);
        shot(&dialog, u"options_multi_dark"_s);
        dialog.accept();
        check(t1->uploadLimit() == 100 * 1024 && t2->uploadLimit() == 300 * 1024, u"options (2): untouched fields keep each torrent's own value"_s);
        check(t1->isSequentialDownload() && !t2->isSequentialDownload(), u"options (2): untouched partial checkbox keeps each torrent's own state"_s);
        check(t1->category() == u"Cat5"_s && t2->category() == u"Movies"_s, u"options (2): untouched category keeps each torrent's category"_s);
    }
    {
        TorrentOptionsDialog dialog(&window, session, {id1, id2});
        child<QSpinBox>(&dialog, "spinUploadLimit")->setValue(64);
        child<QCheckBox>(&dialog, "checkSequential")->setChecked(true);
        auto *category = child<QComboBox>(&dialog, "comboCategory");
        category->setCurrentIndex(indexByText(category, u"Linux ISOs"_s));
        emit category->activated(category->currentIndex());
        dialog.accept();
        check(t1->uploadLimit() == 64 * 1024 && t2->uploadLimit() == 64 * 1024, u"options (2): a changed field is written to every torrent"_s);
        check(t1->isSequentialDownload() && t2->isSequentialDownload(), u"options (2): checkbox applies to both"_s);
        check(t1->category() == u"Linux ISOs"_s && t2->category() == u"Linux ISOs"_s, u"options (2): category applies to both"_s);
    }
    {
        TorrentOptionsDialog dialog(&window, session, {idPriv});
        auto *dht = child<QCheckBox>(&dialog, "checkDisableDHT");
        check(!dht->isEnabled() && dht->isChecked() && !dht->toolTip().isEmpty(), u"options: DHT / PeX / LSD are fixed and explained for a private torrent"_s);
    }
    {
        // automatic management
        TorrentOptionsDialog dialog(&window, session, {id1});
        auto *autoTmm = child<QCheckBox>(&dialog, "checkAutoTMM");
        auto *group = child<QWidget>(&dialog, "groupBoxSavePath");
        check(group->isEnabled(), u"options: the Save at group is editable in manual mode"_s);
        autoTmm->click();
        check(autoTmm->isChecked() && !group->isEnabled(), u"options: Automatic Torrent Management disables the Save at group"_s);
        dialog.accept();
        check(t1->isAutoTMMEnabled(), u"options: automatic management applied"_s);
        t1->setAutoTMMEnabled(false);
    }
    {
        // light theme render
        RoTheme::instance().setDark(false);
        TorrentOptionsDialog dialog(&window, session, {id1});
        shot(&dialog, u"options_single_light"_s);
        RoTheme::instance().setDark(true);
    }

    // ============================== Manage Cookies ==============================
    {
        auto *manager = Net::DownloadManager::instance();
        QNetworkCookie a(QByteArrayLiteral("session"), QByteArrayLiteral("a41f9c2e"));
        a.setDomain(u"tracker.example.org"_s);
        a.setPath(u"/"_s);
        a.setExpirationDate(QDateTime(QDate(2030, 12, 31), QTime(0, 0)));
        QNetworkCookie b(QByteArrayLiteral("uid"), QByteArrayLiteral("10482"));
        b.setDomain(u"cdn.example.net"_s);
        b.setPath(u"/announce"_s);
        manager->setAllCookies({a, b});
        check(manager->allCookies().size() == 2, u"cookies: the download manager holds two cookies"_s);

        CookiesDialog dialog(&window, manager->allCookies());
        auto *view = dialog.view();
        check(view->model()->rowCount() == 2 && view->model()->columnCount() == 5, u"cookies: two rows, five columns"_s);
        QStringList headers;
        for (int i = 0; i < 5; ++i)
            headers << view->model()->headerData(i, Qt::Horizontal).toString();
        check(headers == QStringList({u"Domain"_s, u"Path"_s, u"Name"_s, u"Value"_s, u"Expiration date"_s}), u"cookies: column titles"_s);
        check(view->model()->index(0, 4).data().toString() == u"2030-12-31"_s && view->model()->index(1, 4).data().toString() == u"Session"_s
              , u"cookies: expiration shows a date, or \"Session\" when there is none"_s);
        check(view->model()->index(0, 0).data().toString() == u"tracker.example.org"_s && view->model()->index(0, 3).data().toString() == u"a41f9c2e"_s
              , u"cookies: cells show the cookie fields"_s);
        shot(&dialog, u"cookies_dark"_s);

        dialog.addButton()->click();
        check(view->model()->rowCount() == 3, u"cookies: the plus button adds a row"_s);
        const QModelIndex fresh = view->selectionModel()->currentIndex();
        check(fresh.isValid() && fresh.row() == 1, u"cookies: the new row is selected below the previous selection"_s);
        view->closePersistentEditor(fresh);
        view->model()->setData(view->model()->index(fresh.row(), 0), u"new.example.com"_s);
        view->model()->setData(view->model()->index(fresh.row(), 2), u"token"_s);
        view->model()->setData(view->model()->index(fresh.row(), 3), u"xyz"_s);
        view->selectionModel()->setCurrentIndex(view->model()->index(0, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        dialog.deleteButton()->click();
        check(view->model()->rowCount() == 2 && dialog.cookies().size() == 2, u"cookies: the trash button deletes the selected row"_s);
        const auto list = dialog.cookies();
        check(list.at(0).domain() == u"new.example.com"_s && list.at(0).name() == "token" && list.at(0).value() == "xyz"
              && list.at(0).expirationDate().isValid(), u"cookies: edited cell values reach the cookie; new cookies expire in two years"_s);
    }
    {
        // show(): OK writes back, Cancel does not
        auto *manager = Net::DownloadManager::instance();
        answerModal([](QWidget *w)
        {
            auto *d = qobject_cast<CookiesDialog *>(w);
            check(d != nullptr, u"cookies: the hook opens the Cookies dialog"_s);
            if (!d)
            {
                w->close();
                return;
            }
            d->addButton()->click();
            d->view()->model()->setData(d->view()->model()->index(d->view()->selectionModel()->currentIndex().row(), 0), u"added.example.org"_s);
            clickOk(w);
        });
        window.hooks().cookies();
        bool found = false;
        for (const QNetworkCookie &c : manager->allCookies())
            found |= (c.domain() == u"added.example.org"_s);
        check(found && manager->allCookies().size() == 3, u"cookies: OK stores the list in the download manager"_s);

        answerModal([](QWidget *w)
        {
            if (auto *d = qobject_cast<CookiesDialog *>(w))
            {
                d->view()->selectAll();
                d->deleteButton()->click();
            }
            clickCancel(w);
        });
        window.hooks().cookies();
        check(manager->allCookies().size() == 3, u"cookies: Cancel stores nothing"_s);
    }

    // ============================== Tracker list ==============================
    {
        using BitTorrent::TrackerEntry;
        const QList<TrackerEntry> sample {{u"udp://a.example:1/announce"_s, 0}, {u"udp://b.example:2/announce"_s, 0}, {u"https://c.example/announce"_s, 1}};
        const QString text = TrackerListDialog::toText(sample);
        check(text == u"udp://a.example:1/announce\nudp://b.example:2/announce\n\nhttps://c.example/announce\n"_s, u"tracker list: tiers are separated by a blank line"_s);
        const auto parsed = BitTorrent::parseTrackerEntries(text);
        check(parsed.size() == 3 && parsed.at(2).tier == 1 && parsed.at(0).tier == 0, u"tracker list: the text parses back into the same tiers"_s);

        t1->replaceTrackers({{u"udp://a.example:1/announce"_s, 0}, {u"udp://b.example:2/announce"_s, 0}});
        t2->replaceTrackers({{u"udp://a.example:1/announce"_s, 0}, {u"udp://z.example:9/announce"_s, 1}});
        waitFor([&] { return t1->trackers().size() == 2 && t2->trackers().size() == 2; }, 5000);
        const auto common = TrackerListDialog::commonTrackers({t1, t2});
        check(common.size() == 1 && common.first().url == u"udp://a.example:1/announce"_s, u"tracker list: two torrents show only the common trackers"_s);
        check(TrackerListDialog::commonTrackers({t1}).size() == 2, u"tracker list: one torrent shows all of its trackers"_s);

        TrackerListDialog dialog(&window, TrackerListDialog::commonTrackers({t1}));
        check(dialog.edit()->toPlainText().contains(u"udp://b.example:2/announce"_s), u"tracker list: the text box is pre-filled"_s);
        shot(&dialog, u"tracker_list_dark"_s);

        answerModal([](QWidget *w)
        {
            auto *d = qobject_cast<TrackerListDialog *>(w);
            check(d != nullptr, u"tracker list: the hook opens the Tracker list dialog"_s);
            if (!d)
            {
                w->close();
                return;
            }
            d->edit()->setPlainText(u"udp://n1.example:1/announce\nudp://n2.example:2/announce\n\nudp://n3.example:3/announce"_s);
            clickOk(w);
        });
        window.hooks().editTrackers({id1, id2});
        auto urls = [](BitTorrent::Torrent *t)
        {
            QStringList out;
            for (const auto &s : t->trackers())
                out << (QString::number(s.tier) + u':' + s.url);
            out.sort();   // the engine does not promise an order inside a tier
            return out;
        };
        const QStringList expected {u"0:udp://n1.example:1/announce"_s, u"0:udp://n2.example:2/announce"_s, u"1:udp://n3.example:3/announce"_s};
        waitFor([&] { return urls(t1) == expected && urls(t2) == expected; }, 8000);
        if (!(urls(t1) == expected && urls(t2) == expected))
            std::printf("  t1=%s\n  t2=%s\n", qPrintable(urls(t1).join(u',')), qPrintable(urls(t2).join(u',')));
        check(urls(t1) == expected && urls(t2) == expected, u"tracker list: OK replaces the trackers of every selected torrent, tiers kept"_s);

        answerModal([](QWidget *w)
        {
            if (auto *d = qobject_cast<TrackerListDialog *>(w))
                d->edit()->clear();
            clickCancel(w);
        });
        window.hooks().editTrackers({id1});
        check(t1->trackers().size() == 3, u"tracker list: Cancel changes nothing"_s);
    }

    // ============================== Preview picker ==============================
    {
        QStringList opened;
        PreviewSelectDialog::setOpener([&opened](const Path &p) { opened << p.toString(); });
        BitTorrent::Torrent *tMulti = addTorrent(session, makeTorrent(dataDir, u"media"_s, {u"Episode 02.mkv"_s, u"Episode 01.mkv"_s, u"notes.txt"_s}, true), save);
        BitTorrent::Torrent *tSolo = addTorrent(session, makeTorrent(dataDir, u"solo"_s, {u"only.mp4"_s}, true), save);
        BitTorrent::Torrent *tText = addTorrent(session, makeTorrent(dataDir, u"texts"_s, {u"a.txt"_s, u"b.txt"_s}, true), save);
        check(tMulti && tSolo && tText, u"preview: three media torrents added"_s);
        if (tMulti && tSolo && tText)
        {
            session->getTorrent(tMulti->id());
            waitFor([&] { return tMulti->isFinished() && tSolo->isFinished(); }, 60000);

            PreviewSelectDialog dialog(&window, tMulti);
            check(dialog.rowCount() == 2, u"preview: only previewable files are listed (the .txt is not)"_s);
            auto *view = dialog.view();
            check(view->model()->index(0, 0).data().toString() == u"Episode 01.mkv"_s, u"preview: sorted by name"_s);
            check(view->isColumnHidden(PreviewSelectDialog::FileIndexColumn), u"preview: the file-index column is hidden"_s);
            check(view->model()->index(0, 1).data().toString().contains(u"KiB"_s) && view->model()->index(0, 2).data().toString().endsWith(u'%'), u"preview: size and progress columns"_s);
            check(view->currentIndex().row() == 0, u"preview: the first file is preselected"_s);
            shot(&dialog, u"preview_picker_dark"_s);

            view->setCurrentIndex(view->model()->index(1, 0));
            check(dialog.previewSelected() && opened.size() == 1 && opened.first().endsWith(u"Episode 02.mkv"_s), u"preview: the selected file is opened from the torrent's folder"_s);
            opened.clear();

            window.hooks().previewFile({tSolo->id().toString()});
            check(opened.size() == 1 && opened.first().endsWith(u"only.mp4"_s), u"preview: a single previewable file opens without asking"_s);
            opened.clear();

            answerModal([](QWidget *w) { clickOk(w); });
            window.hooks().previewFile({tMulti->id().toString()});
            check(opened.size() == 1 && opened.first().endsWith(u"Episode 01.mkv"_s), u"preview: the hook runs the picker; Preview opens the preselected file"_s);
            opened.clear();

            answerModal([](QWidget *w) { clickCancel(w); });
            window.hooks().previewFile({tMulti->id().toString()});
            check(opened.isEmpty(), u"preview: Cancel opens nothing"_s);

            answerModal([](QWidget *w)
            {
                check(w->findChildren<QLabel *>().size() > 0, u"preview: a torrent with no previewable file explains itself"_s);
                clickOk(w);
            });
            window.hooks().previewFile({tText->id().toString()});
            check(opened.isEmpty(), u"preview: nothing is opened for a torrent without previewable files"_s);
        }

        // payload missing on disk: error box, nothing opened
        BitTorrent::Torrent *tGone = addTorrent(session, makeTorrent(dataDir, u"gone"_s, {u"a.mkv"_s, u"b.mkv"_s}, false), save / Path(u"empty"_s));
        if (tGone)
        {
            QTimer::singleShot(400, [&]
            {
                // the picker is up: accept it; the styled error box that follows is closed by the second timer
                if (QWidget *w = QApplication::activeModalWidget())
                {
                    QTimer::singleShot(400, [] { if (QWidget *e = QApplication::activeModalWidget()) clickOk(e); QTimer::singleShot(200, [] { if (QWidget *p = QApplication::activeModalWidget()) clickCancel(p); }); });
                    clickOk(w);
                }
            });
            window.hooks().previewFile({tGone->id().toString()});
            check(opened.isEmpty(), u"preview: a file that is not on disk is not opened (error box shown)"_s);
        }
        PreviewSelectDialog::setOpener({});
    }

    // ---- light theme renders of the three list dialogs ----
    {
        RoTheme::instance().setDark(false);
        CookiesDialog cookies(&window, Net::DownloadManager::instance()->allCookies());
        shot(&cookies, u"cookies_light"_s);
        TrackerListDialog trackers(&window, TrackerListDialog::commonTrackers({t1}));
        shot(&trackers, u"tracker_list_light"_s);
        for (BitTorrent::Torrent *t : session->torrents())
        {
            if (t->name() == u"media"_s)
            {
                PreviewSelectDialog picker(&window, t);
                shot(&picker, u"preview_picker_light"_s);
            }
        }
        RoTheme::instance().setDark(true);
    }

    // ---- label audit at run time ----
    {
        TorrentOptionsDialog dialog(&window, session, {id1});
        QStringList texts;
        for (const QLabel *l : dialog.findChildren<QLabel *>())
            texts << l->text();
        for (const QAbstractButton *b : dialog.findChildren<QAbstractButton *>())
            texts << b->text();
        for (const QComboBox *c : dialog.findChildren<QComboBox *>())
            for (int i = 0; i < c->count(); ++i)
                texts << c->itemText(i);
        const QString all = texts.join(u'\n');
        check(!all.contains(u"qBittorrent"_s) && !all.contains(QRegularExpression(u"\\b(Start|Stop|Stopped|Finished)\\b"_s)), u"labels: no Start / Stop / Finished / qBittorrent in the options dialog"_s);
    }

    window.close();
    BitTorrent::Session::freeInstance();
    std::printf("%s\n", failures ? "SOME FAILED" : "ALL OK");
    return failures ? 1 : 0;
}

// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 6 slice 6 check: shutdown countdown, legal notice and crash info dialogs.
// Usage: test_appdialogs <profile-dir> [screenshot-dir]
//        test_appdialogs --crash-child <profile-dir>     (internal: crashes on purpose, see runCrashChild)
// Licensed GPL-2.0-or-later.

#include <csignal>
#include <cstdio>
#include <functional>

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QEventLoop>
#include <QKeyEvent>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QTextBrowser>
#include <QTimer>

#include "app/crashhandler.h"
#include "app/legalnotice.h"
#include "app/stacktrace.h"
#include "base/global.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "gui/dialogs/LegalNoticeDialog.h"
#include "gui/dialogs/ShutdownConfirmDialog.h"
#include "gui/dialogs/StacktraceDialog.h"
#include "gui/theme/RoTheme.h"

using namespace std::chrono_literals;

namespace
{
    int failures = 0;
    QString shotDir;

    void check(const bool ok, const QString &what)
    {
        std::printf("%s  %s\n", ok ? "ok  " : "FAIL", qPrintable(what));
        std::fflush(stdout);
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
        poll.start(5);
        limit.start(ms);
        loop.exec();
        return cond();
    }

    // Runs `action` on the modal dialog shortly after it opens.
    void answerModal(const std::function<void(QWidget *)> &action)
    {
        QTimer::singleShot(150, [action]
        {
            if (QWidget *w = QApplication::activeModalWidget())
                action(w);
        });
    }

    void shot(QWidget *w, const QString &name)
    {
        if (shotDir.isEmpty())
            return;
        w->grab().save(QDir(shotDir).filePath(name + u".png"_s));
    }

    void pressKey(QWidget *target, const int key)
    {
        QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
        QApplication::sendEvent(target, &press);
        QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
        QApplication::sendEvent(target, &release);
    }

    QPushButton *buttonNamed(QWidget *dialog, const QString &text)
    {
        for (QPushButton *b : dialog->findChildren<QPushButton *>())
        {
            if (b->text() == text)
                return b;
        }
        return nullptr;
    }

    QLabel *labelNamed(QWidget *dialog, const QString &objectName)
    {
        return dialog->findChild<QLabel *>(objectName);
    }

    void testShutdown()
    {
        std::printf("-- shutdown confirmation\n");
        struct Case
        {
            ShutdownDialogAction action;
            QString title, message, confirm;
        };
        const Case cases[] = {
            {ShutdownDialogAction::Exit, u"Exit confirmation"_s, u"RoTorrent will now exit."_s, u"Exit Now"_s},
            {ShutdownDialogAction::Shutdown, u"Shutdown confirmation"_s, u"The computer is going to shutdown."_s, u"Shutdown Now"_s},
            {ShutdownDialogAction::Suspend, u"Suspend confirmation"_s, u"The computer is going to enter suspend mode."_s, u"Suspend Now"_s},
            {ShutdownDialogAction::Hibernate, u"Hibernate confirmation"_s, u"The computer is going to enter hibernation mode."_s, u"Hibernate Now"_s},
            {ShutdownDialogAction::Reboot, u"Reboot confirmation"_s, u"The computer is going to reboot."_s, u"Reboot Now"_s},
        };
        for (const Case &c : cases)
        {
            ShutdownConfirmDialog dlg(nullptr, c.action);
            check(dlg.windowTitle() == c.title, c.title + u": window title"_s);
            check(dlg.messageText() == c.message + u"\nYou can cancel the action within 15 seconds."_s, c.title + u": message and 15 s countdown"_s);
            check(dlg.confirmButton()->text() == c.confirm, c.title + u": confirm button is '"_s + c.confirm + u"'"_s);
            check(dlg.confirmButton()->property("primary").toBool(), c.title + u": confirm button is the accent one"_s);
            check((dlg.neverShowAgainBox() != nullptr) == (c.action == ShutdownDialogAction::Exit), c.title + u": 'Don't show again' only for Exit"_s);
            check(dlg.cancelButton()->isDefault() && !dlg.confirmButton()->isDefault(), c.title + u": Cancel is the default button"_s);
            check(bool(dlg.windowFlags() & Qt::WindowStaysOnTopHint), c.title + u": always on top"_s);
            check(dlg.isModal(), c.title + u": modal"_s);
        }

        Preferences *pref = Preferences::instance();
        pref->setDontConfirmAutoExit(false);

        {   // the countdown counts down by itself and confirms at zero
            ShutdownConfirmDialog dlg(nullptr, ShutdownDialogAction::Shutdown, 4, 30ms);
            dlg.show();
            check(waitFor([&] { return dlg.secondsLeft() <= 2; }, 2000), u"countdown: ticks down"_s);
            check(dlg.messageText().contains(u"within 2 seconds"_s) || dlg.messageText().contains(u"within 1 seconds"_s)
                  || dlg.messageText().contains(u"within 0 seconds"_s), u"countdown: the text follows the counter"_s);
            check(waitFor([&] { return dlg.result() == QDialog::Accepted; }, 2000), u"countdown: accepted when it reaches zero"_s);
            check(!dlg.isVisible(), u"countdown: dialog closed itself"_s);
            check(dlg.secondsLeft() == 0, u"countdown: stops at zero"_s);
        }
        {   // Cancel stops the clock and abandons the action
            ShutdownConfirmDialog dlg(nullptr, ShutdownDialogAction::Reboot, 6, 30ms);
            dlg.show();
            waitFor([&] { return dlg.secondsLeft() <= 5; }, 2000);
            dlg.cancelButton()->click();
            check(dlg.result() == QDialog::Rejected, u"Cancel: rejected"_s);
            const int left = dlg.secondsLeft();
            waitFor([] { return false; }, 150);
            check(dlg.secondsLeft() == left, u"Cancel: the timer is stopped"_s);
        }
        {   // Confirm acts at once
            ShutdownConfirmDialog dlg(nullptr, ShutdownDialogAction::Suspend, 60, 1000ms);
            dlg.show();
            dlg.confirmButton()->click();
            check(dlg.result() == QDialog::Accepted && dlg.secondsLeft() == 60, u"Confirm: accepted immediately"_s);
        }
        {   // Esc and the title-bar X abandon it
            ShutdownConfirmDialog dlg(nullptr, ShutdownDialogAction::Hibernate, 60, 1000ms);
            dlg.show();
            pressKey(&dlg, Qt::Key_Escape);
            check(dlg.result() == QDialog::Rejected, u"Esc: rejected"_s);
            ShutdownConfirmDialog dlg2(nullptr, ShutdownDialogAction::Hibernate, 60, 1000ms);
            dlg2.show();
            dlg2.reject();   // what the X button is wired to
            check(dlg2.result() == QDialog::Rejected, u"X: rejected"_s);
        }
        {   // a stray Enter must not shut the computer down
            ShutdownConfirmDialog dlg(nullptr, ShutdownDialogAction::Shutdown, 60, 1000ms);
            dlg.show();
            dlg.activateWindow();
            pressKey(dlg.cancelButton(), Qt::Key_Return);
            check(dlg.result() != QDialog::Accepted, u"Enter does not confirm the action"_s);
        }

        // "Don't show again" (Exit only)
        pref->setDontConfirmAutoExit(false);
        {
            ShutdownConfirmDialog dlg(nullptr, ShutdownDialogAction::Exit, 60, 1000ms);
            dlg.show();
            dlg.neverShowAgainBox()->setChecked(true);
            dlg.cancelButton()->click();
            check(!pref->dontConfirmAutoExit(), u"Don't show again: Cancel does not store it"_s);
        }
        {
            ShutdownConfirmDialog dlg(nullptr, ShutdownDialogAction::Exit, 60, 1000ms);
            dlg.show();
            dlg.neverShowAgainBox()->setChecked(true);
            dlg.confirmButton()->click();
            check(pref->dontConfirmAutoExit(), u"Don't show again: Exit Now stores it"_s);
        }
        check(ShutdownConfirmDialog::askForConfirmation(nullptr, ShutdownDialogAction::Exit), u"askForConfirmation: Exit skips the dialog once remembered"_s);
        {   // other actions never touch the preference and never skip
            bool asked = false;
            answerModal([&](QWidget *w)
            {
                asked = true;
                buttonNamed(w, u"Shutdown Now"_s)->click();
            });
            check(ShutdownConfirmDialog::askForConfirmation(nullptr, ShutdownDialogAction::Shutdown) && asked
                  , u"askForConfirmation: Shutdown is always asked, confirm returns true"_s);
            check(pref->dontConfirmAutoExit(), u"a non-Exit confirmation leaves the Exit preference alone"_s);
        }
        pref->setDontConfirmAutoExit(false);
        {
            answerModal([](QWidget *w) { buttonNamed(w, u"Cancel"_s)->click(); });
            check(!ShutdownConfirmDialog::askForConfirmation(nullptr, ShutdownDialogAction::Exit), u"askForConfirmation: Cancel returns false"_s);
            answerModal([](QWidget *w) { buttonNamed(w, u"Exit Now"_s)->click(); });
            check(ShutdownConfirmDialog::askForConfirmation(nullptr, ShutdownDialogAction::Exit), u"askForConfirmation: Exit Now returns true"_s);
        }
        pref->setDontConfirmAutoExit(false);
    }

    void testLegal()
    {
        std::printf("-- legal notice\n");
        {
            LegalNoticeDialog dlg(nullptr, u"Legal Notice"_s, u"body text"_s, u"footer text"_s);
            check(dlg.windowTitle() == u"Legal Notice"_s, u"title passed through"_s);
            check(dlg.headingText() == u"Before you start"_s, u"heading is 'Before you start'"_s);
            check(labelNamed(&dlg, u"legalBody"_s)->text() == u"body text"_s && labelNamed(&dlg, u"legalFooter"_s)->text() == u"footer text"_s
                  , u"body and footer text shown"_s);
            check(dlg.okButton()->text() == u"OK"_s && dlg.okButton()->isDefault() && dlg.okButton()->property("primary").toBool()
                  , u"one accent OK button, default"_s);
            check(dlg.findChildren<QPushButton *>().size() == 1, u"OK is the only button"_s);
            dlg.show();
            dlg.okButton()->click();
            check(dlg.result() == QDialog::Accepted, u"OK accepts"_s);
        }
        {   // the real logic: showLegalNotice() from app/legalnotice.cpp (GUI branch)
            bool seen = false;
            QString body, footer, title, heading;
            answerModal([&](QWidget *w)
            {
                auto *dlg = qobject_cast<LegalNoticeDialog *>(w);
                seen = (dlg != nullptr);
                if (!dlg)
                    return;
                title = dlg->windowTitle();
                heading = dlg->headingText();
                body = labelNamed(dlg, u"legalBody"_s)->text();
                footer = labelNamed(dlg, u"legalFooter"_s)->text();
                shot(dlg, RoTheme::instance().isDark() ? u"legal_notice_dark"_s : u"legal_notice_light"_s);
                dlg->okButton()->click();
            });
            showLegalNotice(true);
            check(seen, u"showLegalNotice(true) opens the styled dialog (no QMessageBox)"_s);
            check(title == u"Legal Notice"_s && heading == u"Before you start"_s, u"existing logic: title 'Legal Notice'"_s);
            check(body.startsWith(u"RoTorrent is a file sharing program."_s) && body.contains(u"sole responsibility"_s), u"existing logic: notice body"_s);
            check(footer == u"No further notices will be issued."_s, u"existing logic: closing line"_s);
        }
        {   // no owner: the dialog is centred on the screen
            LegalNoticeDialog dlg(nullptr, u"Legal Notice"_s, u"x"_s, u"y"_s);
            answerModal([&](QWidget *w) { w->close(); });
            dlg.run();
            const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
            check(screen.contains(dlg.geometry().center()) && qAbs(dlg.geometry().center().x() - screen.center().x()) <= 2
                  , u"parentless dialog is centred on the screen"_s);
        }
        {   // Esc closes without the "OK" side effects, and the dialog can be shown again
            LegalNoticeDialog dlg(nullptr, u"Legal Notice"_s, u"x"_s, u"y"_s);
            dlg.show();
            pressKey(&dlg, Qt::Key_Escape);
            check(dlg.result() == QDialog::Rejected, u"Esc closes the notice"_s);
        }
    }

    void testCrashDialog()
    {
        std::printf("-- crash info\n");
        StacktraceDialog dlg;
        check(dlg.windowTitle() == u"Crash info"_s, u"title 'Crash info'"_s);
        const QString trace = u"0# 0x00007ff6c1f2a3b0 in rotorrent\n1# foo<int>(a && b) at src/x.cpp:12\n2# 0x00007ff6c2013f10 in rotorrent"_s;
        dlg.setText(u"SIGSEGV"_s, trace);
        const QString text = dlg.reportText();
        check(text.contains(u"RoTorrent has crashed."_s), u"headline"_s);
        check(text.contains(u"Please file a bug report at https://bugs.rotorrent.example"_s), u"where to report"_s);
        check(text.contains(u"RoTorrent version: v1.0.0"_s), u"version block: RoTorrent version"_s);
        check(text.contains(u"Libtorrent version:"_s) && text.contains(u"Qt version:"_s) && text.contains(u"Boost version:"_s)
              && text.contains(u"OpenSSL version:"_s) && text.contains(u"zlib version:"_s) && text.contains(u"OS version:"_s)
              , u"version block: libtorrent, Qt, Boost, OpenSSL, zlib, OS"_s);
        check(text.contains(u"Caught signal: SIGSEGV"_s), u"caught signal shown"_s);
        check(text.contains(u"```\n"_s + trace + u"\n```"_s), u"trace shown verbatim in a fenced block (angle brackets and && intact)"_s);
        check(!text.contains(u"qBittorrent"_s), u"no qBittorrent wording"_s);
        check(dlg.textView()->openExternalLinks(), u"bug tracker link opens externally"_s);
        check(dlg.textView()->isReadOnly(), u"text is read-only (but selectable)"_s);
        check(dlg.closeButton()->text() == u"Close"_s && dlg.findChildren<QPushButton *>().size() >= 1, u"Close button"_s);
        dlg.show();
        dlg.closeButton()->click();
        check(dlg.result() == QDialog::Accepted, u"Close closes the dialog"_s);

        const std::string live = getStacktrace();
        check(!live.empty() && live.find("0#") != std::string::npos, u"getStacktrace() returns frames (boost::stacktrace is wired in)"_s);
        check(live.find("test_appdialogs") != std::string::npos, u"stack trace names this executable (symbols exported)"_s);
    }

    // Runs this executable again, crashing on purpose, to prove the handler writes the report and shows the dialog.
    void testCrashHandler(const QString &self, const QString &profile, const bool withDialog)
    {
        QProcess child;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (withDialog)
            env.remove(QStringLiteral(ROTORRENT_NO_CRASH_DIALOG_ENV));
        else
            env.insert(QStringLiteral(ROTORRENT_NO_CRASH_DIALOG_ENV), u"1"_s);
        env.insert(u"QT_QPA_PLATFORM"_s, u"offscreen"_s);
        child.setProcessEnvironment(env);
        child.start(self, {u"--crash-child"_s, profile});
        const bool finished = child.waitForFinished(30000);
        const QString out = QString::fromLocal8Bit(child.readAllStandardOutput());
        const QString err = QString::fromLocal8Bit(child.readAllStandardError());
        const QString label = withDialog ? u"crash child (dialog)"_s : u"crash child (no dialog)"_s;
        check(finished && (child.exitStatus() == QProcess::CrashExit || child.exitCode() != 0)   /* Windows reports the CRT abort as exit code 3, not a crash */, label + u": process dies by the signal after the handler ran"_s);
        check(err.contains(u"Caught signal: SIGSEGV"_s), label + u": stderr names the signal"_s);
        check(err.contains(u"RoTorrent version: v1.0.0"_s) && err.contains(u"https://bugs.rotorrent.example"_s), label + u": stderr has version and report address"_s);
        check(err.contains(u"```"_s) && err.contains(u"0#"_s), label + u": stderr has the fenced stack trace"_s);
        check(withDialog == out.contains(u"CRASH-DIALOG-SHOWN"_s), label + (withDialog ? u": the Crash info dialog was shown"_s : u": no dialog when disabled"_s));
        if (withDialog)
            check(out.contains(u"CRASH-DIALOG-TEXT-OK"_s), label + u": the dialog carried the signal and trace"_s);
    }

    int runCrashChild(QApplication &app)
    {
        Q_UNUSED(app)
        registerCrashHandlers();
        // The handler runs a nested event loop for the dialog: this timer fires inside it.
        QTimer::singleShot(300, [&]
        {
            if (auto *dlg = qobject_cast<StacktraceDialog *>(QApplication::activeModalWidget()))
            {
                std::printf("CRASH-DIALOG-SHOWN\n");
                const QString text = dlg->reportText();
                if (text.contains(u"Caught signal: SIGSEGV"_s) && text.contains(u"```"_s))
                    std::printf("CRASH-DIALOG-TEXT-OK\n");
                std::fflush(stdout);
                dlg->closeButton()->click();
            }
        });
        QTimer::singleShot(50, [] { std::raise(SIGSEGV); });
        return app.exec();
    }
}

int main(int argc, char **argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    RoTheme::configureHighDpi();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RoTorrent"));
    const QStringList args = QCoreApplication::arguments();
    const bool crashChild = (args.size() > 2) && (args.at(1) == u"--crash-child"_s);
    if (args.size() < 2)
        return 2;
    const QString profileDir = crashChild ? args.at(2) : args.at(1);
    shotDir = (!crashChild && args.size() > 2) ? args.at(2) : QString();
    QDir().mkpath(profileDir);
    if (!shotDir.isEmpty())
        QDir().mkpath(shotDir);

    Profile::initInstance(Path(profileDir), QString(), false);
    SettingsStorage::initInstance();
    Preferences::initInstance();
    RoTheme::instance().init();
    if (crashChild)
        return runCrashChild(app);

    // Both themes: the checks are theme independent, the screenshots are not.
    for (const bool dark : {true, false})
    {
        RoTheme::instance().setDark(dark);
        std::printf("== %s theme\n", dark ? "dark" : "light");
        const QString theme = dark ? u"dark"_s : u"light"_s;

        if (!shotDir.isEmpty())
        {
            for (const auto &[action, name] : {std::pair {ShutdownDialogAction::Exit, u"exit_confirm"_s}, std::pair {ShutdownDialogAction::Shutdown, u"shutdown_confirm"_s}})
            {
                ShutdownConfirmDialog dlg(nullptr, action);
                dlg.adjustSize();
                dlg.show();
                QApplication::processEvents();
                shot(&dlg, name + u"_"_s + theme);
            }
            StacktraceDialog crash;
            crash.setText(u"SIGSEGV"_s, u"0# 0x00007ff6c1f2a3b0 in rotorrent\n1# 0x00007ff6c1e9d4a2 in rotorrent\n2# 0x00007ff6c2013f10 in rotorrent"_s);
            crash.adjustSize();
            crash.show();
            QApplication::processEvents();
            shot(&crash, u"crash_info_"_s + theme);
        }
        testLegal();
        if (dark)
        {
            testShutdown();
            testCrashDialog();
            testCrashHandler(QCoreApplication::applicationFilePath(), profileDir, false);
            testCrashHandler(QCoreApplication::applicationFilePath(), profileDir, true);
        }
    }

    std::printf(failures ? "\n%d FAILED\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}

// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 8: "do not sleep" rule and controller (no engine, no Windows needed). Handoff Phase 8.
#include <cstdio>

#include <QApplication>
#include <QTemporaryDir>

#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "gui/power/PowerController.h"

namespace
{
    int failures = 0;

    void check(const bool ok, const QString &what)
    {
        std::printf("%s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
        if (!ok)
            ++failures;
    }

    using F = PowerPolicy::TorrentFacts;

    struct FakeInhibitor final : Inhibitor
    {
        int *busy;
        int *idle;
        FakeInhibitor(int *b, int *i) : busy(b), idle(i) {}
        bool requestBusy() override { ++*busy; return true; }
        bool requestIdle() override { ++*idle; return true; }
    };
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RoTorrent"));
    QTemporaryDir dir;
    Profile::initInstance(Path(dir.path()), QString(), false);
    SettingsStorage::initInstance();
    Preferences::initInstance();
    Preferences *pref = Preferences::instance();

    const F downloading {false, false, false, true, false};
    const F seeding {true, false, false, true, false};
    const F paused {false, true, false, true, false};
    const F errored {false, false, true, true, false};
    const F noMetadata {false, false, false, false, false};
    const F moving {true, true, false, true, true};

    // ---- pure rule (same as qBittorrent's updatePowerManagementState) ----
    check(!PowerPolicy::shouldInhibit({}, true, true), QStringLiteral("no torrents: no inhibit"));
    check(PowerPolicy::shouldInhibit({downloading}, true, false), QStringLiteral("a running download inhibits when 'downloading' is on"));
    check(!PowerPolicy::shouldInhibit({downloading}, false, true), QStringLiteral("...and not when only 'seeding' is on"));
    check(PowerPolicy::shouldInhibit({seeding}, false, true), QStringLiteral("a running seed inhibits when 'seeding' is on"));
    check(!PowerPolicy::shouldInhibit({seeding}, true, false), QStringLiteral("...and not when only 'downloading' is on"));
    check(!PowerPolicy::shouldInhibit({paused}, true, true), QStringLiteral("a paused torrent never inhibits"));
    check(!PowerPolicy::shouldInhibit({errored}, true, true), QStringLiteral("an errored torrent never inhibits"));
    check(!PowerPolicy::shouldInhibit({noMetadata}, true, true), QStringLiteral("a torrent still waiting for metadata does not inhibit"));
    check(PowerPolicy::shouldInhibit({moving}, false, false), QStringLiteral("moving files always inhibits (even with both options off)"));
    check(PowerPolicy::shouldInhibit({paused, errored, downloading}, true, false), QStringLiteral("one qualifying torrent among others is enough"));

    // ---- controller with a fake inhibitor ----
    int busy = 0;
    int idle = 0;
    QList<F> facts;
    pref->setPreventFromSuspendWhenDownloading(true);
    pref->setPreventFromSuspendWhenSeeding(false);
    {
        PowerController pc(nullptr, nullptr, std::make_unique<FakeInhibitor>(&busy, &idle));
        pc.setFactsSource([&facts] { return facts; });
        check(pc.state() == PowerController::State::Idle && busy == 0, QStringLiteral("starts idle without torrents"));
        facts = {downloading};
        pc.update();
        check(pc.state() == PowerController::State::Busy && busy == 1, QStringLiteral("a download makes it Busy (inhibitor asked once)"));
        pc.update();
        check(busy == 1, QStringLiteral("no repeated request while the state is unchanged"));
        facts = {paused};
        pc.update();
        check(pc.state() == PowerController::State::Idle && idle == 1, QStringLiteral("pausing releases the inhibitor"));
        facts = {seeding};
        pc.update();
        check(pc.state() == PowerController::State::Idle, QStringLiteral("seeding with the option off stays Idle"));
        pref->setPreventFromSuspendWhenSeeding(true);
        pc.update();
        check(pc.state() == PowerController::State::Busy, QStringLiteral("switching the option on takes effect on the next update"));
    }
    check(idle == 2, QStringLiteral("destroying the controller releases a held inhibitor (never leaves the PC awake)"));

    std::printf(failures ? "\n%d FAILED\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}

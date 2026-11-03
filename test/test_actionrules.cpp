// SPDX-License-Identifier: GPL-2.0-or-later
// Unit test for ActionRules::plan: qBittorrent's enable / visibility rules for torrent commands.
#include <cstdio>

#include <QString>

#include "gui/actions/ActionRules.h"

namespace
{
    int failures = 0;
    void check(const bool ok, const char *what)
    {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok)
            ++failures;
    }

    TorrentFacts active()    { TorrentFacts f; f.id = QStringLiteral("a"); return f; }
}

int main()
{
    check(!ActionRules::plan({}).any, "empty selection: nothing is available");

    {   // running, unfinished, not forced
        const CommandPlan p = ActionRules::plan({active()});
        check(!p.resume && p.pause && p.forceResume, "running torrent: Pause and Force Resume, no Resume");
        check(p.anyUnfinished && p.single && p.canReannounce, "running unfinished torrent: queue / rename / reannounce available");
    }
    {   // Paused (stopped, unfinished)
        TorrentFacts f = active();
        f.stopped = true;
        const CommandPlan p = ActionRules::plan({f});
        check(p.resume && !p.pause && p.forceResume, "paused torrent: Resume and Force Resume, no Pause");
        check(!p.canReannounce, "paused torrent cannot reannounce");
    }
    {   // forced
        TorrentFacts f = active();
        f.forced = true;
        const CommandPlan p = ActionRules::plan({f});
        check(p.resume && p.pause && !p.forceResume, "forced torrent: Resume (leave forced) and Pause, no Force Resume");
    }
    {   // errored / missing files can only be restarted
        TorrentFacts f = active();
        f.stopped = true;
        f.missingFiles = true;
        const CommandPlan p = ActionRules::plan({f});
        check(p.resume && p.forceResume && !p.pause, "missing-files torrent: Resume / Force Resume only");
        TorrentFacts e = active();
        e.errored = true;
        const CommandPlan pe = ActionRules::plan({e});
        check(pe.resume && pe.forceResume && !pe.canReannounce, "errored torrent: restartable, cannot reannounce");
    }
    {   // rechecking: both Resume and Pause
        TorrentFacts f = active();
        f.checking = true;
        const CommandPlan p = ActionRules::plan({f});
        check(p.resume && p.pause && !p.canReannounce, "checking torrent: Resume and Pause, no reannounce");
    }
    {   // queued cannot reannounce
        TorrentFacts f = active();
        f.queued = true;
        check(!ActionRules::plan({f}).canReannounce, "queued torrent cannot reannounce");
    }
    {   // finished torrents: no queue commands, super seeding offered
        TorrentFacts f = active();
        f.finished = true;
        f.superSeeding = true;
        const CommandPlan p = ActionRules::plan({f});
        check(!p.anyUnfinished && p.showSuperSeeding && p.superSeeding == Qt::Checked, "finished torrent: super seeding offered and reflects state");
        check(!ActionRules::queueCommandsEnabled(p, true), "finished torrent: queue commands disabled");
        check(!ActionRules::queueCommandsEnabled(ActionRules::plan({active()}), false), "queueing off: queue commands disabled");
        check(ActionRules::queueCommandsEnabled(ActionRules::plan({active()}), true), "queueing on + unfinished: queue commands enabled");
        TorrentFacts u = active();
        check(!ActionRules::plan({f, u}).showSuperSeeding, "mixed selection: no super seeding");
    }
    {   // tri-states and categories over a selection
        TorrentFacts a = active(), b = active();
        a.sequential = true;
        b.sequential = false;
        a.category = b.category = QStringLiteral("Docs");
        a.autoTMM = true;
        const CommandPlan p = ActionRules::plan({a, b});
        check(p.sequential == Qt::PartiallyChecked && p.autoTMM == Qt::PartiallyChecked, "mixed values give a partial check state");
        check(p.allSameCategory && p.commonCategory == QStringLiteral("Docs") && !p.single, "same category is detected; Rename needs a single torrent");
        b.category = QStringLiteral("Other");
        check(!ActionRules::plan({a, b}).allSameCategory, "different categories are detected");
        b.sequential = true;
        check(ActionRules::plan({a, b}).sequential == Qt::Checked, "all sequential gives Checked");
    }
    {   // info hashes
        TorrentFacts f = active();
        f.hasInfoHashV1 = true;
        f.hasInfoHashV2 = false;
        const CommandPlan p = ActionRules::plan({f});
        check(p.copyInfoHashV1 && !p.copyInfoHashV2, "info hash v2 copy is disabled for v1-only torrents");
    }
    std::printf(failures ? "%d FAILED\n" : "actionrules OK\n", failures);
    return failures ? 1 : 0;
}

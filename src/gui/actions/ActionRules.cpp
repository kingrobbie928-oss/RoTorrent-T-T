// SPDX-License-Identifier: GPL-2.0-or-later
#include "ActionRules.h"

namespace
{
    Qt::CheckState tri(const bool allSame, const bool value)
    {
        return allSame ? (value ? Qt::Checked : Qt::Unchecked) : Qt::PartiallyChecked;
    }
}

CommandPlan ActionRules::plan(const QList<TorrentFacts> &selection)
{
    CommandPlan p;
    if (selection.isEmpty())
        return p;
    p.any = true;
    p.single = (selection.size() == 1);

    bool needsStart = false, needsStop = false, needsForce = false;
    bool allSameSuper = true, superMode = false;
    bool allSameSeq = true, allSameFirstLast = true, seqMode = false, firstLastMode = false;
    bool allSameAuto = true, firstAuto = false;
    bool oneNotFinished = false, oneHasMetadata = false;
    bool first = true;
    QString firstCategory;

    for (const TorrentFacts &t : selection)
    {
        if (first)
        {
            firstCategory = t.category;
            firstAuto = t.autoTMM;
        }
        if (t.category != firstCategory)
            p.allSameCategory = false;
        if (t.autoTMM != firstAuto)
            allSameAuto = false;

        if (t.hasMetadata)
            oneHasMetadata = true;

        if (!t.finished)
        {
            oneNotFinished = true;
            if (first)
            {
                seqMode = t.sequential;
                firstLastMode = t.firstLast;
            }
            else
            {
                if (seqMode != t.sequential)
                    allSameSeq = false;
                if (firstLastMode != t.firstLast)
                    allSameFirstLast = false;
            }
        }
        else if (!oneNotFinished && allSameSuper && t.hasMetadata)
        {
            if (first)
                superMode = t.superSeeding;
            else if (superMode != t.superSeeding)
                allSameSuper = false;
        }

        // Same bookkeeping as upstream: a forced torrent can be resumed (to leave forced mode),
        // an unforced one can be forced.
        if (!t.forced)
            needsForce = true;
        else
            needsStart = true;

        if (t.stopped)
            needsStart = true;
        else
            needsStop = true;

        if (t.errored || t.missingFiles)
        {
            // It cannot keep processing until it is restarted.
            needsStart = true;
            needsForce = true;
        }

        if (t.checking)
        {
            needsStart = true;
            needsStop = true;
        }

        if (!t.stopped && !t.checking && !t.queued && !t.errored && !t.missingFiles)
            p.canReannounce = true;
        if (t.hasInfoHashV1)
            p.copyInfoHashV1 = true;
        if (t.hasInfoHashV2)
            p.copyInfoHashV2 = true;
        first = false;
    }

    p.resume = needsStart;
    p.pause = needsStop;
    p.forceResume = needsForce;
    p.anyUnfinished = oneNotFinished;
    p.anyMetadata = oneHasMetadata;
    p.commonCategory = p.allSameCategory ? firstCategory : QString();
    p.autoTMM = tri(allSameAuto, firstAuto);
    p.showSuperSeeding = !oneNotFinished && oneHasMetadata;
    p.superSeeding = tri(allSameSuper, superMode);
    p.sequential = tri(allSameSeq, seqMode);
    p.firstLast = tri(allSameFirstLast, firstLastMode);
    return p;
}

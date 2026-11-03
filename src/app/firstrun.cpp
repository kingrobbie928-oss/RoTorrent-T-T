// SPDX-License-Identifier: GPL-2.0-or-later
#include "firstrun.h"

#include <QString>
#include <QVariant>

#include "base/global.h"
#include "base/settingsstorage.h"

void applyFirstRunDefaults()
{
    struct DefaultValue
    {
        QString name;
        QVariant value;
    };

    // Same "current" values upstream stores for new users.
    const DefaultValue defaults[] =
    {
        {u"BitTorrent/Session/QueueingSystemEnabled"_s, false},
        {u"Meta/MigrationVersion"_s, 12}  // marks the settings layout; kept for upstream compatibility
    };

    auto *settingsStorage = SettingsStorage::instance();
    for (const DefaultValue &value : defaults)
    {
        if (!settingsStorage->hasKey(value.name))
            settingsStorage->storeValue(value.name, value.value);
    }
}

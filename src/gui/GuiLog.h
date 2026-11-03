// SPDX-License-Identifier: GPL-2.0-or-later
// Writes a line to the engine's log (and so to the Log tab and the optional log file). A no-op when no Logger
// exists (the preview executable, unit tests). Licensed GPL-2.0-or-later.
#pragma once

#include <QString>

#include "base/logger.h"

namespace GuiLog
{
    inline void add(const QString &message, const Log::MsgType type = Log::NORMAL)
    {
        if (Logger::instance())
            Logger::instance()->addMessage(message, type);
    }
}

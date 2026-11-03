// SPDX-License-Identifier: GPL-2.0-or-later
#include "LogBuffer.h"

#include "base/global.h"
#include "base/logger.h"

namespace
{
    LogBuffer::Kind kindOf(const Log::MsgType type)
    {
        switch (type)
        {
        case Log::INFO: return LogBuffer::Info;
        case Log::WARNING: return LogBuffer::Warning;
        case Log::CRITICAL: return LogBuffer::Critical;
        default: return LogBuffer::Normal;
        }
    }
}

LogBuffer::LogBuffer(QObject *parent)
    : QObject(parent)
{
}

void LogBuffer::attachLogger()
{
    Logger *logger = Logger::instance();
    if (!logger)
        return;

    const auto add = [this](const Log::Msg &msg)
    {
        append(kindOf(msg.type), msg.message, QDateTime::fromSecsSinceEpoch(msg.timestamp));
    };
    for (const Log::Msg &msg : asConst(logger->getMessages()))
        add(msg);
    connect(logger, &Logger::newLogMessage, this, add);
}

void LogBuffer::append(const Kind kind, const QString &text, const QDateTime &time)
{
    if (m_entries.size() >= MaxEntries)
        m_entries.remove(0, m_entries.size() - MaxEntries + 1);
    m_entries.append({time, kind, text});
    emit added();
}

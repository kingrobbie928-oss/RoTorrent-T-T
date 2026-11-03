// SPDX-License-Identifier: GPL-2.0-or-later
// Bounded buffer behind the Log tab (handoff 8.9). Fed by the engine's Logger (same four message
// types) or, in the preview, by append(). Upstream keeps a bounded list too (MAX_LOG_MESSAGES).
// Licensed GPL-2.0-or-later.
#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

class LogBuffer final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LogBuffer)

public:
    // Message types, the values of Log::MsgType so Logger messages pass straight through.
    enum Kind { Normal = 1, Info = 2, Warning = 4, Critical = 8 };
    static constexpr int MaxEntries = 20000;

    struct Entry
    {
        QDateTime time;
        Kind kind = Normal;
        QString text;
    };

    explicit LogBuffer(QObject *parent = nullptr);

    // Reads the messages the Logger already holds and follows new ones.
    void attachLogger();
    void append(Kind kind, const QString &text, const QDateTime &time = QDateTime::currentDateTime());

    const QList<Entry> &entries() const { return m_entries; }
    int size() const { return static_cast<int>(m_entries.size()); }

signals:
    void added();   // one signal per append, the tab re-renders on it

private:
    QList<Entry> m_entries;
};

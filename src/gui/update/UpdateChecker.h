// SPDX-License-Identifier: GPL-2.0-or-later
// Update-check plumbing (handoff Phase 8, section 11 item 5). There is no release feed yet: until the owner
// supplies a URL (setting Gui/Update/FeedUrl) the check reports "not configured". The feed is a small JSON
// document fetched over HTTPS:
//     {"version": "1.0.1", "url": "https://example.org/download", "notes": "What changed"}
// RoTorrent only tells the user and opens the download page; it never downloads or installs a binary, so no
// signing scheme is needed for 1.0 (an installer-side decision for the owner). Licensed GPL-2.0-or-later.
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class UpdateChecker final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(UpdateChecker)

public:
    enum class Result
    {
        NotConfigured,   // no feed URL
        UpToDate,
        Available,
        Failed           // network error or a feed that cannot be read
    };

    struct Outcome
    {
        Result result = Result::NotConfigured;
        QString version;    // the newer version (Available)
        QString url;        // where to get it (Available)
        QString notes;
        QString error;      // Failed
    };

    explicit UpdateChecker(QObject *parent = nullptr);

    // The configured feed address; empty = not configured. Only https:// addresses are accepted.
    static QString feedUrl();
    static void setFeedUrl(const QString &url);
    static bool isValidFeedUrl(const QString &url);

    // Dotted numeric comparison ("1.0.10" > "1.0.9"; a missing part counts as 0). <0, 0, >0.
    static int compareVersions(const QString &a, const QString &b);

    // Pure: reads a feed document and compares it with the running version.
    static Outcome evaluate(const QByteArray &feed, const QString &currentVersion);

    // Fetches the feed through the application's download manager. Emits finished() exactly once.
    void check();

signals:
    void finished(const UpdateChecker::Outcome &outcome);
};

// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 8: update-check plumbing (no network). Handoff section 11 item 5.
#include <cstdio>

#include <QApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>

#include "base/global.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "gui/update/UpdateChecker.h"

namespace
{
    int failures = 0;

    void check(const bool ok, const QString &what)
    {
        std::printf("%s  %s\n", ok ? "PASS" : "FAIL", qPrintable(what));
        if (!ok)
            ++failures;
    }
    using R = UpdateChecker::Result;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RoTorrent"));
    QTemporaryDir dir;
    Profile::initInstance(Path(dir.path()), QString(), false);
    SettingsStorage::initInstance();
    Preferences::initInstance();

    // ---- version order ----
    check(UpdateChecker::compareVersions(u"1.0.10"_s, u"1.0.9"_s) > 0, QStringLiteral("1.0.10 is newer than 1.0.9 (numeric, not text)"));
    check(UpdateChecker::compareVersions(u"1.0"_s, u"1.0.0"_s) == 0, QStringLiteral("a missing part counts as 0"));
    check(UpdateChecker::compareVersions(u"v1.2.0"_s, u"1.1.9"_s) > 0, QStringLiteral("a leading v is ignored"));
    check(UpdateChecker::compareVersions(u"1.0.0"_s, u"1.0.1-beta"_s) < 0, QStringLiteral("suffixes after the number are ignored"));
    check(UpdateChecker::compareVersions(u"2"_s, u"1.9.9"_s) > 0, QStringLiteral("major wins"));

    // ---- feed evaluation ----
    const auto feed = [](const char *json) { return UpdateChecker::evaluate(QByteArray(json), QStringLiteral("1.0.0")); };
    const auto newer = feed(R"j({"version":"1.1.0","url":"https://rotorrent.example/get","notes":"Faster."})j");
    check(newer.result == R::Available && newer.version == u"1.1.0" && newer.url == u"https://rotorrent.example/get" && newer.notes == u"Faster."
          , QStringLiteral("a newer version is reported with its page and notes"));
    check(feed(R"j({"version":"1.0.0"})j").result == R::UpToDate, QStringLiteral("same version: up to date"));
    check(feed(R"j({"version":"0.9.0"})j").result == R::UpToDate, QStringLiteral("older feed: up to date"));
    check(feed("not json").result == R::Failed && !feed("not json").error.isEmpty(), QStringLiteral("garbage is a failure with a message"));
    check(feed(R"j([1,2])j").result == R::Failed, QStringLiteral("a JSON array is not a feed"));
    check(feed(R"j({"notes":"x"})j").result == R::Failed, QStringLiteral("a feed without a version is a failure"));
    check(feed(R"j({"version":"1.2.0","url":"http://evil.example/x"})j").url.isEmpty(), QStringLiteral("a non-https download page is never offered"));
    check(feed(R"j({"version":"1.2.0","url":"javascript:alert(1)"})j").url.isEmpty(), QStringLiteral("neither is a non-web scheme"));

    // ---- feed address ----
    check(!UpdateChecker::isValidFeedUrl(QString()) && !UpdateChecker::isValidFeedUrl(u"http://x.example/f.json"_s)
          && !UpdateChecker::isValidFeedUrl(u"https://"_s)  && UpdateChecker::isValidFeedUrl(u"https://rotorrent.example/update.json"_s)
          , QStringLiteral("only https feed addresses are valid"));
    check(UpdateChecker::feedUrl().isEmpty(), QStringLiteral("no feed configured by default"));
    UpdateChecker::setFeedUrl(QStringLiteral("http://insecure.example/f.json"));
    check(UpdateChecker::feedUrl().isEmpty(), QStringLiteral("an http address in the settings is ignored"));
    UpdateChecker::setFeedUrl(QStringLiteral("https://rotorrent.example/update.json"));
    check(UpdateChecker::feedUrl() == u"https://rotorrent.example/update.json", QStringLiteral("a valid address is returned"));
    UpdateChecker::setFeedUrl(QString());

    // ---- not configured: check() answers asynchronously, exactly once ----
    {
        UpdateChecker checker;
        int answers = 0;
        R got = R::Failed;
        QEventLoop loop;
        QObject::connect(&checker, &UpdateChecker::finished, &loop, [&](const UpdateChecker::Outcome &o)
        {
            ++answers;
            got = o.result;
            loop.quit();
        });
        checker.check();
        check(answers == 0, QStringLiteral("the answer is not delivered synchronously"));
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        loop.exec();
        QApplication::processEvents();
        check(answers == 1 && got == R::NotConfigured, QStringLiteral("no feed: finished(NotConfigured) exactly once"));
    }

    std::printf(failures ? "\n%d FAILED\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}

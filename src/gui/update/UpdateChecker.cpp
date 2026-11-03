// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "UpdateChecker.h"

#include <algorithm>

#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/settingsstorage.h"
#include "base/version.h"

namespace
{
    const QString FEED_KEY = QStringLiteral("Gui/Update/FeedUrl");
    constexpr qint64 MAX_FEED_BYTES = 64 * 1024;   // a feed is a few hundred bytes; refuse anything absurd

    QList<int> partsOf(const QString &version)
    {
        QString v = version.trimmed();
        if (v.startsWith(u'v', Qt::CaseInsensitive))
            v.remove(0, 1);
        QList<int> parts;
        for (const QString &piece : v.split(u'.'))
        {
            // "1.0.1-beta" -> 1 0 1: the numeric prefix of each piece
            int i = 0;
            while ((i < piece.size()) && piece.at(i).isDigit())
                ++i;
            parts.append(piece.left(i).toInt());
        }
        return parts;
    }
}

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<UpdateChecker::Outcome>("UpdateChecker::Outcome");
}

QString UpdateChecker::feedUrl()
{
    const QString url = SettingsStorage::instance()->loadValue<QString>(FEED_KEY, QString()).trimmed();
    return isValidFeedUrl(url) ? url : QString();
}

void UpdateChecker::setFeedUrl(const QString &url)
{
    SettingsStorage::instance()->storeValue(FEED_KEY, url.trimmed());
}

bool UpdateChecker::isValidFeedUrl(const QString &url)
{
    const QUrl u(url.trimmed(), QUrl::StrictMode);
    return u.isValid() && (u.scheme() == u"https") && !u.host().isEmpty();
}

int UpdateChecker::compareVersions(const QString &a, const QString &b)
{
    const QList<int> pa = partsOf(a);
    const QList<int> pb = partsOf(b);
    const qsizetype n = std::max(pa.size(), pb.size());
    for (qsizetype i = 0; i < n; ++i)
    {
        const int x = (i < pa.size()) ? pa.at(i) : 0;
        const int y = (i < pb.size()) ? pb.at(i) : 0;
        if (x != y)
            return (x < y) ? -1 : 1;
    }
    return 0;
}

UpdateChecker::Outcome UpdateChecker::evaluate(const QByteArray &feed, const QString &currentVersion)
{
    Outcome out;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(feed, &err);
    if ((err.error != QJsonParseError::NoError) || !doc.isObject())
    {
        out.result = Result::Failed;
        out.error = tr("The update information could not be read.");
        return out;
    }
    const QJsonObject o = doc.object();
    const QString version = o.value(u"version"_s).toString().trimmed();
    if (version.isEmpty() || partsOf(version).isEmpty())
    {
        out.result = Result::Failed;
        out.error = tr("The update information has no version number.");
        return out;
    }
    if (compareVersions(version, currentVersion) <= 0)
    {
        out.result = Result::UpToDate;
        return out;
    }
    out.result = Result::Available;
    out.version = version;
    out.notes = o.value(u"notes"_s).toString().trimmed();
    // Only an https address is ever opened for the user.
    const QString url = o.value(u"url"_s).toString().trimmed();
    out.url = isValidFeedUrl(url) ? url : QString();
    return out;
}

void UpdateChecker::check()
{
    const QString url = feedUrl();
    if (url.isEmpty())
    {
        Outcome out;
        out.result = Result::NotConfigured;
        QTimer::singleShot(0, this, [this, out] { emit finished(out); });
        return;
    }
    Net::DownloadManager::instance()->download(Net::DownloadRequest(url).limit(MAX_FEED_BYTES)
                                               , Preferences::instance()->useProxyForGeneralPurposes(), this
                                               , [this](const Net::DownloadResult &result)
    {
        Outcome out;
        if (result.status != Net::DownloadStatus::Success)
        {
            out.result = Result::Failed;
            out.error = result.errorString;
        }
        else
        {
            out = evaluate(result.data, QStringLiteral(QBT_VERSION_2));
        }
        emit finished(out);
    });
}

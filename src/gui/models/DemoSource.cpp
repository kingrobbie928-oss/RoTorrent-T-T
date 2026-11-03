// SPDX-License-Identifier: GPL-2.0-or-later
#include "DemoSource.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QRandomGenerator>

#include "SessionAdapter.h"

namespace
{
    RoStatus statusFromCode(const QString &code)
    {
        static const QHash<QString, RoStatus> map = {
            {QStringLiteral("dl"), RoStatus::Downloading}, {QStringLiteral("sd"), RoStatus::Seeding}
            , {QStringLiteral("fn"), RoStatus::Completed}, {QStringLiteral("st"), RoStatus::Paused}
            , {QStringLiteral("qu"), RoStatus::Queued}, {QStringLiteral("ck"), RoStatus::Checking}
            , {QStringLiteral("pa"), RoStatus::Stalled}, {QStringLiteral("er"), RoStatus::Error}
            , {QStringLiteral("mf"), RoStatus::MissingFiles}};
        return map.value(code, RoStatus::Paused);
    }

    // "659 MB", "3.1 MB/s", "820 KB/s" -> bytes (only used as a sort key for demo rows)
    qint64 parseBytes(const QString &text)
    {
        static const QRegularExpression re(QStringLiteral("([0-9.]+)\\s*([KMGT]?)B"));
        const auto m = re.match(text);
        if (!m.hasMatch())
            return 0;
        double v = m.captured(1).toDouble();
        const QString u = m.captured(2);
        for (const QString &unit : {QStringLiteral("K"), QStringLiteral("M"), QStringLiteral("G"), QStringLiteral("T")})
        {
            v *= 1024;
            if (u == unit)
                break;
        }
        return u.isEmpty() ? static_cast<qint64>(m.captured(1).toDouble()) : static_cast<qint64>(v);
    }

    // "6m 12s", "2d 14h", "1h 5m" -> seconds, "\u2014" -> -1
    qint64 parseEta(const QString &text)
    {
        static const QRegularExpression re(QStringLiteral("([0-9]+)\\s*([dhms])"));
        qint64 total = 0;
        bool any = false;
        auto it = re.globalMatch(text);
        while (it.hasNext())
        {
            const auto m = it.next();
            any = true;
            const qint64 n = m.captured(1).toLongLong();
            switch (m.captured(2).at(0).toLatin1())
            {
            case 'd': total += n * 86400; break;
            case 'h': total += n * 3600; break;
            case 'm': total += n * 60; break;
            default: total += n; break;
            }
        }
        return any ? total : -1;
    }

    QString categoryFor(const QString &name)
    {
        static const QList<QPair<QString, QString>> map = {
            {QStringLiteral("debian"), QStringLiteral("Linux ISOs")}, {QStringLiteral("fedora"), QStringLiteral("Linux ISOs")}
            , {QStringLiteral("archlinux"), QStringLiteral("Linux ISOs")}, {QStringLiteral("opensuse"), QStringLiteral("Linux ISOs")}
            , {QStringLiteral("raspios"), QStringLiteral("Linux ISOs")}, {QStringLiteral("kde-neon"), QStringLiteral("Linux ISOs")}
            , {QStringLiteral("blender"), QStringLiteral("Open Media")}, {QStringLiteral("public-domain"), QStringLiteral("Open Media")}
            , {QStringLiteral("nasa"), QStringLiteral("Datasets")}};
        for (const auto &p : map)
        {
            if (name.startsWith(p.first))
                return p.second;
        }
        return {};
    }

    const QStringList &trackerHosts()
    {
        static const QStringList hosts = {
            QStringLiteral("tracker.linuxtracker.org"), QStringLiteral("opentrackr.org"), QStringLiteral("tracker.opentrackr.org")
            , QStringLiteral("tracker.torrent.eu.org"), QStringLiteral("open.stealth.si"), QStringLiteral("exodus.desync.com")};
        return hosts;
    }

    void finish(QList<TorrentRow> &rows)
    {
        int queue = 0;
        for (TorrentRow &r : rows)
            r.queuePos = r.finished() ? -1 : queue++;
    }
}

QList<TorrentRow> DemoSource::fromJson(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error)
            *error = QStringLiteral("cannot open %1").arg(path);
        return {};
    }
    const QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
    QList<TorrentRow> rows;
    int i = 0;
    for (const QJsonValue &v : arr)
    {
        const QJsonObject o = v.toObject();
        TorrentRow r;
        r.name = o[u"name"].toString();
        r.id = QString::number(i, 16).rightJustified(40, u'0');
        r.progress = o[u"pct"].toDouble();
        r.status = statusFromCode(o[u"status"].toString());
        r.sizeText = o[u"size"].toString();
        r.downText = o[u"down"].toString();
        r.upText = o[u"up"].toString();
        r.etaText = o[u"eta"].toString();
        r.ratioText = o[u"ratio"].toString();
        r.totalSize = parseBytes(r.sizeText);
        r.downRate = parseBytes(r.downText);
        r.upRate = parseBytes(r.upText);
        r.etaSeconds = parseEta(r.etaText);
        r.ratio = r.ratioText.toDouble();
        r.category = categoryFor(r.name);
        r.trackerHost = trackerHosts().at(i % trackerHosts().size());
        rows.append(r);
        ++i;
    }
    finish(rows);
    return rows;
}

QList<TorrentRow> DemoSource::generate(const int count, const unsigned seed)
{
    static const RoStatus cycle[] = {RoStatus::Seeding, RoStatus::Downloading, RoStatus::Completed, RoStatus::Paused
                                     , RoStatus::Queued, RoStatus::Checking, RoStatus::Stalled, RoStatus::Error
                                     , RoStatus::MissingFiles, RoStatus::Downloading};
    static const QStringList cats = {QStringLiteral("Linux ISOs"), QStringLiteral("Open Media"), QStringLiteral("Datasets"), QString()};
    QRandomGenerator rng(seed);

    QList<TorrentRow> rows;
    rows.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        TorrentRow r;
        r.id = QString::number(i + 1, 16).rightJustified(40, u'0');
        r.status = cycle[i % std::size(cycle)];
        r.name = QStringLiteral("distro-%1-%2.%3-amd64-netinst.iso").arg(i + 1).arg(rng.bounded(1, 20)).arg(rng.bounded(0, 9));
        r.totalSize = static_cast<qint64>(rng.bounded(50, 8000)) * 1024 * 1024;
        switch (r.status)
        {
        case RoStatus::Seeding:
        case RoStatus::Completed: r.progress = 100; break;
        case RoStatus::Checking: r.progress = rng.bounded(10, 90); break;
        case RoStatus::Queued: r.progress = rng.bounded(0, 30); break;
        default: r.progress = rng.bounded(0, 99); break;
        }
        if (r.status == RoStatus::Downloading)
        {
            r.downRate = static_cast<qint64>(rng.bounded(100, 9000)) * 1024;
            r.etaSeconds = rng.bounded(30, 7200);
        }
        if (r.status == RoStatus::Seeding)
            r.upRate = static_cast<qint64>(rng.bounded(0, 2000)) * 1024;
        r.ratio = rng.bounded(0, 500) / 100.0;
        r.category = cats.at(i % cats.size());
        r.trackerHost = (i % 7 == 6) ? QString() : trackerHosts().at(i % trackerHosts().size());
        SessionAdapter::fillTexts(r);
        rows.append(r);
    }
    finish(rows);
    return rows;
}

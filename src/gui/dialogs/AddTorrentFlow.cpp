// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "AddTorrentFlow.h"

#include <QFile>
#include <QFileDialog>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/net/downloadmanager.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/torrentfileguard.h"
#include "AddLinksDialog.h"
#include "AddNewTorrentDialog.h"
#include "MainWindow.h"
#include "RoMessageBox.h"

namespace
{
    // Adds one parsed torrent: dialog (unless "Never show again"), engine call, .torrent file guard.
    // `sourcePath` is the .torrent file the descriptor came from, empty for magnets and info-hashes.
    void addDescriptor(MainWindow &window, BitTorrent::Session *session, const BitTorrent::TorrentDescriptor &descriptor
                       , const QString &sourcePath, const AddTorrentFlow::Options &options)
    {
        if (session->isKnownTorrent(descriptor.infoHash()))
        {
            RoMessageBox::info(&window, QObject::tr("Add New Torrent")
                    , QObject::tr("This torrent is already in the transfer list."));
            return;
        }

        // The guard removes the source .torrent after a successful add, as the Options say; the dialog's
        // "Do not delete .torrent file" overrides that for this torrent.
        std::optional<TorrentFileGuard> guard;
        if (!sourcePath.isEmpty())
            guard.emplace(Path(sourcePath));

        BitTorrent::TorrentDescriptor toAdd = descriptor;
        BitTorrent::AddTorrentParams params = options.params;   // unset fields take the session default
        const bool showDialog = options.skipDialog.has_value() ? !*options.skipDialog : AddNewTorrentDialog::isEnabled();
        if (showDialog)
        {
            const auto result = AddNewTorrentDialog::run(&window, session, descriptor, sourcePath, options.params);
            if (!result)
                return;
            toAdd = result->descriptor;
            params = result->params;
            if (guard && result->keepTorrentFile)
                guard->setAutoRemove(false);
        }

        if (!session->addTorrent(toAdd, params))
        {
            RoMessageBox::error(&window, QObject::tr("Add New Torrent")
                    , QObject::tr("Failed to add the torrent. See the Log tab for details."));
            return;
        }
        if (guard)
            guard->markAsAddedToSession();
    }

    // Downloads a torrent from a web address with the engine's download manager (cookies, proxy, size
    // limit from the preferences), then runs the same add flow as for a file.
    void downloadAndAdd(MainWindow &window, BitTorrent::Session *session, const QString &url
                        , const AddTorrentFlow::Options &options)
    {
        MainWindow *w = &window;
        Net::DownloadManager::instance()->download(
                Net::DownloadRequest(url).limit(Preferences::instance()->getTorrentFileSizeLimit())
                , Preferences::instance()->useProxyForGeneralPurposes(), w
                , [w, session, url, options](const Net::DownloadResult &result)
        {
            std::optional<BitTorrent::TorrentDescriptor> descriptor;
            QString error = result.errorString;
            if (result.status == Net::DownloadStatus::Success)
            {
                if (auto loaded = BitTorrent::TorrentDescriptor::load(result.data))
                    descriptor = loaded.value();
                else
                    error = loaded.error();
            }
            else if (result.status == Net::DownloadStatus::RedirectedToMagnet)
            {
                if (auto parsed = BitTorrent::TorrentDescriptor::parse(result.magnetURI))
                    descriptor = parsed.value();
                else
                    error = parsed.error();
            }
            if (!descriptor)
            {
                RoMessageBox::error(w, QObject::tr("Add torrent links")
                        , QObject::tr("Could not download a torrent from this address.")
                        + QLatin1Char('\n') + url + QLatin1Char('\n') + error);
                return;
            }
            addDescriptor(*w, session, *descriptor, QString(), options);
        });
    }
}

void AddTorrentFlow::addSources(MainWindow &window, BitTorrent::Session *session, const QStringList &sources
                                , const UrlHandler &urlHandler)
{
    addSources(window, session, sources, Options {}, urlHandler);
}

void AddTorrentFlow::addSources(MainWindow &window, BitTorrent::Session *session, const QStringList &sources
                                , const Options &options, const UrlHandler &urlHandler)
{
    for (const QString &source : sources)
    {
        const QString s = source.trimmed();
        if (s.isEmpty())
            continue;

        const bool isUrl = s.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
                || s.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)
                || s.startsWith(QStringLiteral("ftp://"), Qt::CaseInsensitive);
        if (isUrl)
        {
            // A handler passed by the full application wins; otherwise download here.
            if (!urlHandler || !urlHandler(s))
                downloadAndAdd(window, session, s, options);
            continue;
        }

        const bool isMagnetOrHash = s.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive)
                || !QFile::exists(s);
        const auto result = isMagnetOrHash
                ? BitTorrent::TorrentDescriptor::parse(s)
                : BitTorrent::TorrentDescriptor::loadFromFile(Path(s));
        if (!result)
        {
            RoMessageBox::error(&window, QObject::tr("Add New Torrent")
                    , isMagnetOrHash ? QObject::tr("This link is not a valid magnet link or info-hash.")
                                     : QObject::tr("This file is not a valid .torrent file.")
                    + QLatin1Char('\n') + s);
            continue;
        }
        addDescriptor(window, session, result.value(), isMagnetOrHash ? QString() : s, options);
    }
}

void AddTorrentFlow::install(MainWindow &window, BitTorrent::Session *session, UrlHandler urlHandler)
{
    MainWindow *w = &window;
    window.hooks().addTorrentFile = [w, session, urlHandler]
    {
        const QStringList files = QFileDialog::getOpenFileNames(w, QObject::tr("Open Torrent Files")
                , {}, QObject::tr("Torrent Files (*.torrent);;All Files (*)"));
        if (!files.isEmpty())
            addSources(*w, session, files, urlHandler);
    };
    window.hooks().addSources = [w, session, urlHandler](const QStringList &sources)
    {
        addSources(*w, session, sources, urlHandler);
    };
    window.hooks().addTorrentLink = [w, session, urlHandler]
    {
        if (const auto links = AddLinksDialog::getLinks(w))
            addSources(*w, session, *links, urlHandler);
    };
}

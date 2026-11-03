// SPDX-License-Identifier: GPL-2.0-or-later
// Cookies manager, Tracker list and Preview picker dialogs (handoff 8.12; upstream cookiesdialog,
// trackerentriesdialog, previewselectdialog) and the hooks that open them together with the Torrent options
// dialog. Built on RoDialog. Licensed GPL-2.0-or-later.
#pragma once

#include <functional>

#include <QList>
#include <QNetworkCookie>
#include <QStringList>

#include "base/bittorrent/trackerentry.h"
#include "base/path.h"
#include "dialogs/RoDialog.h"

class QPlainTextEdit;
class QToolButton;
class QTreeView;
class MainWindow;

namespace BitTorrent
{
    class Session;
    class Torrent;
}

// "Manage Cookies": a table of the download manager's cookies (domain, path, name, value, expiration), plus add
// and delete. OK writes the whole list back, Cancel changes nothing.
class CookiesDialog final : public RoDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CookiesDialog)

public:
    CookiesDialog(QWidget *parent, const QList<QNetworkCookie> &cookies);

    QList<QNetworkCookie> cookies() const;
    QTreeView *view() const { return m_view; }
    QToolButton *addButton() const { return m_add; }
    QToolButton *deleteButton() const { return m_delete; }

    // Opens the dialog on Net::DownloadManager's cookies and stores them on OK. Returns true when accepted.
    static bool show(QWidget *parent);

private:
    class Model;
    Model *m_model = nullptr;
    QTreeView *m_view = nullptr;
    QToolButton *m_add = nullptr;
    QToolButton *m_delete = nullptr;
};

// "Tracker list": one URL per line, blank lines split tiers. Shows the trackers common to the selected torrents;
// OK replaces the trackers of every selected torrent with the edited list (upstream behaviour).
class TrackerListDialog final : public RoDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TrackerListDialog)

public:
    TrackerListDialog(QWidget *parent, const QList<BitTorrent::TrackerEntry> &trackers);

    QList<BitTorrent::TrackerEntry> trackers() const;
    QPlainTextEdit *edit() const { return m_edit; }

    static QString toText(const QList<BitTorrent::TrackerEntry> &trackers);
    static QList<BitTorrent::TrackerEntry> commonTrackers(const QList<BitTorrent::Torrent *> &torrents);
    static bool show(QWidget *parent, BitTorrent::Session *session, const QStringList &ids);

private:
    QPlainTextEdit *m_edit = nullptr;
};

// "Preview file": lists the previewable files of a torrent (name, size, progress). Double click or Preview opens the
// chosen file; a torrent with a single previewable file skips the dialog.
class PreviewSelectDialog final : public RoDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PreviewSelectDialog)

public:
    enum Column { NameColumn, SizeColumn, ProgressColumn, FileIndexColumn, ColumnCount };

    PreviewSelectDialog(QWidget *parent, const BitTorrent::Torrent *torrent);

    int rowCount() const;
    QTreeView *view() const { return m_view; }
    // Opens the selected file; false (and an error box) when it is not on disk yet.
    bool previewSelected();

    // Runs the picker for every torrent in `ids` that has metadata.
    static void show(QWidget *parent, BitTorrent::Session *session, const QStringList &ids);
    // How a chosen file is opened (default: the system's default application). Tests replace it.
    static void setOpener(std::function<void(const Path &)> opener);

protected:
    void accept() override;

private:
    const BitTorrent::Torrent *m_torrent;
    QTreeView *m_view = nullptr;
};

namespace ManagementDialogs
{
    // Connects Manage Cookies, Torrent options, Tracker list and Preview file.
    void install(MainWindow &window, BitTorrent::Session *session);
}

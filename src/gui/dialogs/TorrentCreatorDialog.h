// SPDX-License-Identifier: GPL-2.0-or-later
// Torrent Creator (handoff 8.12, phase 6 slice 4). Three cards (Select file/folder, Settings, Fields) in a
// scroll area, a footer with real progress, Create Torrent and Cancel. The work is done by the ported
// BitTorrent::TorrentCreator on a worker thread; this dialog only shows its progress and honours Cancel.
// Licensed GPL-2.0-or-later.
#pragma once

#include <functional>

#include <QString>

#include "RoDialog.h"
#include "base/bittorrent/torrentcreator.h"
#include "base/path.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QThreadPool;
class QWidget;

class MainWindow;
class RoDropZone;

namespace BitTorrent
{
    class Session;
}

class TorrentCreatorDialog final : public RoDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentCreatorDialog)

public:
    // `session` receives the new torrent when "Seed immediately" is on; it may be null (nothing is added).
    TorrentCreatorDialog(QWidget *parent, BitTorrent::Session *session, const Path &defaultPath = {});
    ~TorrentCreatorDialog() override;

    // Opens the dialog on File > Create Torrent, Tools > Torrent Creator and the toolbar.
    static void install(MainWindow &window, BitTorrent::Session *session);

    // The piece sizes offered, in bytes (0 = Auto). 16 KiB to 128 MiB, as upstream.
    static QList<int> pieceSizes();
    // Persisted form values live under "TorrentCreator/" in the settings, like upstream.

    void setInputPath(const Path &path);
    Path inputPath() const;
    bool isCreating() const { return m_creating; }

    // The parameters the current form values give for a given output file.
    BitTorrent::TorrentCreatorParams buildParams(const Path &torrentFile) const;

    // Starts the worker now, writing `torrentFile`. This is what the Create Torrent button does after the
    // save-file picker. Returns false (with a message) when the input path is unreadable or already creating.
    bool startCreation(const Path &torrentFile);
    // Stops a running creation; the form comes back and nothing is written. No-op when idle.
    void cancelCreation();

    // Replaces the message boxes (tests). Arguments: is-error, title, text.
    using Notifier = std::function<void(bool, const QString &, const QString &)>;
    void setNotifier(Notifier notifier) { m_notifier = std::move(notifier); }

    void reject() override;   // while creating: cancel the creation, keep the dialog

signals:
    void created(const Path &torrentFile);
    void failed(const QString &reason);
    void cancelled();

private:
    void buildUi();
    void onCreateClicked();
    void onCalculatePieces();
    void handleSuccess(const BitTorrent::TorrentCreatorResult &result);
    void handleFailure(const QString &message);
    void setBusy(bool busy);
    void notify(bool error, const QString &title, const QString &text);
    void loadSettings();
    void saveSettings() const;
    BitTorrent::TorrentFormat torrentFormat() const;

    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    BitTorrent::Session *m_session;
    QThreadPool *m_pool;
    Notifier m_notifier;
    bool m_creating = false;
    bool m_cancelRequested = false;
    BitTorrent::TorrentCreator *m_creator = nullptr;
    bool m_seedAfter = false;
    bool m_ignoreLimitsAfter = false;

    QWidget *m_scroll = nullptr;
    QLineEdit *m_path = nullptr;
    RoDropZone *m_drop = nullptr;
    QPushButton *m_selectFile = nullptr;
    QPushButton *m_selectFolder = nullptr;
    QComboBox *m_format = nullptr;
    QComboBox *m_pieceSize = nullptr;
    QPushButton *m_calc = nullptr;
    QLabel *m_pieceCount = nullptr;
    QCheckBox *m_ignoreDotfiles = nullptr;
    QCheckBox *m_private = nullptr;
    QCheckBox *m_seed = nullptr;
    QCheckBox *m_ignoreLimits = nullptr;
    QPlainTextEdit *m_trackers = nullptr;
    QPlainTextEdit *m_webSeeds = nullptr;
    QPlainTextEdit *m_comment = nullptr;
    QLineEdit *m_source = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_create = nullptr;
    QPushButton *m_cancel = nullptr;
};

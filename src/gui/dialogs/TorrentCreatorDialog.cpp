// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "TorrentCreatorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QStyle>
#include <QThread>
#include <QThreadPool>
#include <QUrl>
#include <QVBoxLayout>

#include "RoMessageBox.h"
#include "MainWindow.h"
#include "base/bittorrent/addtorrentparams.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/sharelimits.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentdescriptor.h"
#include "base/global.h"
#include "base/settingsstorage.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "theme/RoTheme.h"

#ifndef QBT_USES_LIBTORRENT2
#error "RoTorrent is built on libtorrent 2 (handoff decision 7)"
#endif

namespace
{
#define CREATOR_KEY(name) QStringLiteral("TorrentCreator/" name)

    // When the root file or folder is a symlink the torrent should keep the link's name, so the picker must not
    // resolve it. On Windows that option would disable .lnk expansion, so it is left off there (as upstream).
#ifndef Q_OS_WIN
    const QFileDialog::Options FILE_DIALOG_OPTIONS {QFileDialog::DontResolveSymlinks};
#else
    const QFileDialog::Options FILE_DIALOG_OPTIONS {};
#endif

    // The piece count cannot be interrupted inside libtorrent, so the thread always runs to the end and is not
    // owned by the dialog; if the dialog is gone the result simply has no receiver.
    class PieceCalculationThread final : public QThread
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(PieceCalculationThread)

    public:
        using CalcFunc = std::function<int ()>;

        explicit PieceCalculationThread(CalcFunc calc)
            : QThread(nullptr)
            , m_calc {std::move(calc)}
        {
        }

        ~PieceCalculationThread() override
        {
            wait();
        }

    signals:
        void resultReady(int pieces);

    private:
        void run() override
        {
            emit resultReady(m_calc());
        }

        CalcFunc m_calc;
    };

    Path pathFromMime(const QMimeData *mime)
    {
        if (!mime || !mime->hasUrls())
            return {};
        // Only the first item counts.
        const QUrl first = mime->urls().constFirst();
        return Path((first.scheme().compare(QStringLiteral("file"), Qt::CaseInsensitive) == 0)
                    ? first.toLocalFile() : first.toString());
    }

    QStringList lines(const QPlainTextEdit *edit)
    {
        QStringList out;
        for (const QString &line : edit->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts))
        {
            const QString t = line.trimmed();
            if (!t.isEmpty())
                out << t;
        }
        return out;
    }

    // Tracker list: one URL per line; a blank line starts the next tier (kept, as upstream). Runs of blank lines
    // collapse into one.
    QStringList trackerLines(const QPlainTextEdit *edit)
    {
        const QString text = edit->toPlainText().trimmed();
        if (text.isEmpty())
            return {};
        QString collapsed = text;
        collapsed.replace(QRegularExpression(QStringLiteral("\\n[ \\t\\r]*\\n[\\n \\t\\r]+")), QStringLiteral("\n\n"));
        QStringList out;
        for (const QString &line : collapsed.split(QLatin1Char('\n')))
            out << line.trimmed();
        return out;
    }
}

// The dashed drop target of the first card (spec 8.12): outline turns accent and a faint wash appears while a
// file or folder is dragged over it (QSS: QFrame#dropZone[drag="true"]).
class RoDropZone final : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RoDropZone)

public:
    explicit RoDropZone(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setObjectName(QStringLiteral("dropZone"));
        setAcceptDrops(true);
        setProperty("drag", false);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 18, 16, 18);
        layout->setSpacing(6);

        m_icon = new QLabel;
        m_icon->setAlignment(Qt::AlignCenter);
        layout->addWidget(m_icon);

        auto *title = new QLabel(tr("Drag & drop a file or folder here"));
        title->setObjectName(QStringLiteral("dropTitle"));
        title->setAlignment(Qt::AlignCenter);
        layout->addWidget(title);

        auto *sub = new QLabel(tr("or"));
        sub->setObjectName(QStringLiteral("dropSub"));
        sub->setAlignment(Qt::AlignCenter);
        layout->addWidget(sub);

        m_buttons = new QHBoxLayout;
        m_buttons->setSpacing(8);
        m_buttons->addStretch(1);
        m_buttons->addStretch(1);
        layout->addLayout(m_buttons);

        refreshIcon();
        connect(&RoTheme::instance(), &RoTheme::changed, this, &RoDropZone::refreshIcon);
    }

    // Puts a button between the two stretches that centre the row.
    void addButton(QPushButton *button)
    {
        m_buttons->insertWidget(m_buttons->count() - 1, button);
    }

signals:
    void pathDropped(const Path &path);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasUrls() || event->mimeData()->hasText())
        {
            event->acceptProposedAction();
            setDrag(true);
        }
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        setDrag(false);
        QFrame::dragLeaveEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        setDrag(false);
        event->acceptProposedAction();
        const Path path = pathFromMime(event->mimeData());
        if (!path.isEmpty())
            emit pathDropped(path);
    }

private:
    void setDrag(const bool on)
    {
        setProperty("drag", on);
        style()->unpolish(this);
        style()->polish(this);
        update();
    }

    void refreshIcon()
    {
        m_icon->setPixmap(RoTheme::instance().iconPx(QStringLiteral("m-upload"), RoTheme::instance().pal().muted, 30));
    }

    QLabel *m_icon;
    QHBoxLayout *m_buttons;
};

QList<int> TorrentCreatorDialog::pieceSizes()
{
    QList<int> sizes {0};
    for (int i = 4; i <= 17; ++i)
        sizes << (1024 << i);
    return sizes;
}

TorrentCreatorDialog::TorrentCreatorDialog(QWidget *parent, BitTorrent::Session *session, const Path &defaultPath)
    : RoDialog(parent, tr("Torrent Creator"), 594)
    , m_session {session}
    , m_pool {new QThreadPool(this)}
{
    m_pool->setMaxThreadCount(1);
    m_pool->setObjectName(QStringLiteral("TorrentCreatorDialog pool"));
    setAcceptDrops(true);
    buildUi();
    loadSettings();
    if (!defaultPath.isEmpty())
        setInputPath(defaultPath);
    setBusy(false);

    // Same height as the kit's dialog (774 px), but never taller than the screen allows.
    int height = 774;
    if (const QScreen *screen = QGuiApplication::primaryScreen())
        height = qMin(height, screen->availableGeometry().height() - 80);
    setMinimumHeight(qMax(420, height));
}

TorrentCreatorDialog::~TorrentCreatorDialog()
{
    if (m_creator)
        m_creator->requestInterruption();
    // The pool's destructor waits for the worker, which stops at the next checkpoint.
    delete m_pool;
    m_pool = nullptr;
    delete m_creator;   // the pool does not own it; the worker is finished
    m_creator = nullptr;
    saveSettings();
}

void TorrentCreatorDialog::install(MainWindow &window, BitTorrent::Session *session)
{
    MainWindow *w = &window;
    window.hooks().createTorrent = [w, session]
    {
        TorrentCreatorDialog dialog(w, session);
        dialog.run();
    };
}

void TorrentCreatorDialog::buildUi()
{
    auto *host = new QDialog;
    host->setObjectName(QStringLiteral("roUi"));
    host->setWindowFlags(Qt::Widget);
    auto *outer = new QVBoxLayout(host);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // --- scrolling cards ---
    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("tcScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *cards = new QWidget;
    cards->setObjectName(QStringLiteral("tcCards"));
    auto *cardsLayout = new QVBoxLayout(cards);
    cardsLayout->setContentsMargins(12, 12, 12, 12);
    cardsLayout->setSpacing(16);
    scroll->setWidget(cards);
    outer->addWidget(scroll, 1);
    m_scroll = scroll;

    constexpr int labelWidth = 104;

    // Card 1: Select file/folder to share
    {
        auto *box = new QGroupBox(tr("Select file/folder to share"));
        auto *v = new QVBoxLayout(box);
        v->setSpacing(14);

        auto *row = new QHBoxLayout;
        row->setSpacing(12);
        auto *label = new QLabel(tr("Path:"));
        row->addWidget(label);
        m_path = new QLineEdit;
        m_path->setObjectName(QStringLiteral("tcPath"));
        m_path->setReadOnly(true);
        m_path->setPlaceholderText(tr("Path to the file or folder to share"));
        m_path->setTextMargins(30, 0, 0, 0);
        // folder glyph inside the box, left (a plain label: it is decoration, not a button)
        auto *glyph = new QLabel(m_path);
        glyph->setAttribute(Qt::WA_TransparentForMouseEvents);
        glyph->setGeometry(2, 0, 26, 32);
        glyph->setAlignment(Qt::AlignCenter);
        const auto refreshGlyph = [glyph]
        {
            glyph->setPixmap(RoTheme::instance().iconPx(QStringLiteral("folder"), RoTheme::instance().pal().muted, 16));
        };
        refreshGlyph();
        connect(&RoTheme::instance(), &RoTheme::changed, glyph, refreshGlyph);
        row->addWidget(m_path, 1);
        v->addLayout(row);

        m_drop = new RoDropZone;
        m_drop->setObjectName(QStringLiteral("dropZone"));
        m_selectFile = new QPushButton(tr("Select file"));
        m_selectFile->setObjectName(QStringLiteral("tcSelectFile"));
        m_selectFolder = new QPushButton(tr("Select folder"));
        m_selectFolder->setObjectName(QStringLiteral("tcSelectFolder"));
        m_drop->addButton(m_selectFile);
        m_drop->addButton(m_selectFolder);
        v->addWidget(m_drop);
        cardsLayout->addWidget(box);

        connect(m_drop, &RoDropZone::pathDropped, this, &TorrentCreatorDialog::setInputPath);
        connect(m_selectFile, &QPushButton::clicked, this, [this]
        {
            const Path path {QFileDialog::getOpenFileName(this, tr("Select file"), inputPath().data()
                    , QString(), nullptr, FILE_DIALOG_OPTIONS)};
            if (!path.isEmpty())
                setInputPath(path);
        });
        connect(m_selectFolder, &QPushButton::clicked, this, [this]
        {
            const Path path {QFileDialog::getExistingDirectory(this, tr("Select folder"), inputPath().data()
                    , (QFileDialog::ShowDirsOnly | FILE_DIALOG_OPTIONS))};
            if (!path.isEmpty())
                setInputPath(path);
        });
    }

    // Card 2: Settings
    {
        auto *box = new QGroupBox(tr("Settings"));
        auto *v = new QVBoxLayout(box);
        v->setSpacing(10);

        auto *formatRow = new QHBoxLayout;
        formatRow->setSpacing(12);
        auto *formatLabel = new QLabel(tr("Torrent format:"));
        formatLabel->setFixedWidth(labelWidth);
        formatRow->addWidget(formatLabel);
        m_format = new QComboBox;
        m_format->setObjectName(QStringLiteral("tcFormat"));
        m_format->setFixedWidth(168);
        m_format->addItem(tr("V2"));        // index 0, as upstream
        m_format->addItem(tr("Hybrid"));    // index 1
        m_format->addItem(tr("V1"));        // index 2
        formatRow->addWidget(m_format);
        formatRow->addStretch(1);
        v->addLayout(formatRow);

        auto *pieceRow = new QHBoxLayout;
        pieceRow->setSpacing(12);
        auto *pieceLabel = new QLabel(tr("Piece size:"));
        pieceLabel->setFixedWidth(labelWidth);
        pieceRow->addWidget(pieceLabel);
        m_pieceSize = new QComboBox;
        m_pieceSize->setObjectName(QStringLiteral("tcPieceSize"));
        m_pieceSize->setFixedWidth(168);
        for (const int size : pieceSizes())
        {
            m_pieceSize->addItem((size == 0) ? tr("Auto") : Utils::Misc::friendlyUnit(size, false, 0), size);
        }
        pieceRow->addWidget(m_pieceSize);
        m_calc = new QPushButton(tr("Calculate number of pieces:"));
        m_calc->setObjectName(QStringLiteral("tcCalc"));
        pieceRow->addWidget(m_calc);
        m_pieceCount = new QLabel(QStringLiteral("0"));
        m_pieceCount->setObjectName(QStringLiteral("tcPieceCount"));
        pieceRow->addWidget(m_pieceCount);
        pieceRow->addStretch(1);
        v->addLayout(pieceRow);

        m_ignoreDotfiles = new QCheckBox(tr("Ignore dotfiles"));
        m_ignoreDotfiles->setObjectName(QStringLiteral("tcIgnoreDotfiles"));
        v->addWidget(m_ignoreDotfiles);
        m_private = new QCheckBox(tr("Private torrent (Won't distribute on DHT network)"));
        m_private->setObjectName(QStringLiteral("tcPrivate"));
        v->addWidget(m_private);
        // "Start seeding immediately" in the kit; the label dictionary (Appendix C) has no "Start".
        m_seed = new QCheckBox(tr("Seed immediately after creating"));
        m_seed->setObjectName(QStringLiteral("tcSeed"));
        v->addWidget(m_seed);
        m_ignoreLimits = new QCheckBox(tr("Ignore share ratio limits for this torrent"));
        m_ignoreLimits->setObjectName(QStringLiteral("tcIgnoreLimits"));
        v->addWidget(m_ignoreLimits);
        // The kit also shows an "Optimize alignment" group. It belongs to libtorrent 1 and does nothing with
        // libtorrent 2 (the engine RoTorrent is built on), so it is not offered.
        cardsLayout->addWidget(box);

        connect(m_calc, &QPushButton::clicked, this, &TorrentCreatorDialog::onCalculatePieces);
        connect(m_seed, &QCheckBox::toggled, m_ignoreLimits, &QWidget::setEnabled);
        // The old count no longer describes the form once an input to it changes.
        const auto resetCount = [this] { m_pieceCount->setText(QStringLiteral("0")); };
        connect(m_format, &QComboBox::currentIndexChanged, this, resetCount);
        connect(m_pieceSize, &QComboBox::currentIndexChanged, this, resetCount);
        connect(m_ignoreDotfiles, &QCheckBox::toggled, this, resetCount);
    }

    // Card 3: Fields
    {
        auto *box = new QGroupBox(tr("Fields"));
        auto *grid = new QGridLayout(box);
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(10);
        grid->setColumnMinimumWidth(0, 105);
        grid->setColumnStretch(1, 1);

        const auto addLabel = [grid](const QString &text, const int row, const bool top)
        {
            auto *label = new QLabel(text);
            label->setFixedWidth(105);
            grid->addWidget(label, row, 0, top ? (Qt::AlignTop | Qt::AlignLeft) : (Qt::AlignVCenter | Qt::AlignLeft));
            if (top)
                label->setContentsMargins(0, 7, 0, 0);
        };
        const auto makeBox = [](const QString &name, const QString &hint)
        {
            auto *edit = new QPlainTextEdit;
            edit->setObjectName(name);
            edit->setPlaceholderText(hint);
            edit->setFixedHeight(58);
            edit->setTabChangesFocus(true);
            return edit;
        };

        addLabel(tr("Tracker URLs:"), 0, true);
        m_trackers = makeBox(QStringLiteral("tcTrackers"), tr("One tracker URL per line"));
        grid->addWidget(m_trackers, 0, 1);
        addLabel(tr("Web seed URLs:"), 1, true);
        m_webSeeds = makeBox(QStringLiteral("tcWebSeeds"), tr("One web seed URL per line (optional)"));
        grid->addWidget(m_webSeeds, 1, 1);
        addLabel(tr("Comments:"), 2, true);
        m_comment = makeBox(QStringLiteral("tcComment"), tr("Comment (optional)"));
        grid->addWidget(m_comment, 2, 1);
        addLabel(tr("Source:"), 3, false);
        m_source = new QLineEdit;
        m_source->setObjectName(QStringLiteral("tcSource"));
        m_source->setPlaceholderText(tr("Source (optional)"));
        grid->addWidget(m_source, 3, 1);
        cardsLayout->addWidget(box);
    }
    cardsLayout->addStretch(1);

    // --- footer ---
    auto *footer = new QWidget;
    footer->setObjectName(QStringLiteral("tcFooter"));
    auto *fv = new QVBoxLayout(footer);
    fv->setContentsMargins(12, 8, 12, 12);
    fv->setSpacing(10);
    auto *progressRow = new QHBoxLayout;
    progressRow->setSpacing(12);
    progressRow->addWidget(new QLabel(tr("Progress:")));
    m_progress = new QProgressBar;
    m_progress->setObjectName(QStringLiteral("tcProgress"));
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    progressRow->addWidget(m_progress, 1);
    fv->addLayout(progressRow);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(8);
    buttonRow->addStretch(1);
    m_create = new QPushButton(tr("Create Torrent"));   // the accept button, renamed from OK
    m_create->setObjectName(QStringLiteral("tcCreate"));
    m_create->setProperty("primary", true);
    m_create->setDefault(true);
    buttonRow->addWidget(m_create);
    m_cancel = new QPushButton(tr("Cancel"));
    m_cancel->setObjectName(QStringLiteral("tcCancel"));
    buttonRow->addWidget(m_cancel);
    buttonRow->addStretch(1);
    fv->addLayout(buttonRow);
    outer->addWidget(footer);

    connect(m_create, &QPushButton::clicked, this, &TorrentCreatorDialog::onCreateClicked);
    connect(m_cancel, &QPushButton::clicked, this, &TorrentCreatorDialog::reject);

    setContent(host);
}

void TorrentCreatorDialog::loadSettings()
{
    auto *s = SettingsStorage::instance();
    const QString lastPath = s->loadValue<QString>(CREATOR_KEY("LastAddPath"), QString());
    if (!lastPath.isEmpty())
    {
        // Pre-fills the box only when the place still exists.
        if (Utils::Fs::isReadable(Path(lastPath)))
            m_path->setText(Path(lastPath).toString());
    }

    const int sizeIndex = m_pieceSize->findData(s->loadValue<int>(CREATOR_KEY("PieceSizeBytes"), 0));
    m_pieceSize->setCurrentIndex((sizeIndex >= 0) ? sizeIndex : 0);
    m_ignoreDotfiles->setChecked(s->loadValue<bool>(CREATOR_KEY("IgnoreDotfiles"), true));
    m_private->setChecked(s->loadValue<bool>(CREATOR_KEY("PrivateTorrent"), false));
    m_seed->setChecked(s->loadValue<bool>(CREATOR_KEY("StartSeeding"), true));
    m_ignoreLimits->setChecked(s->loadValue<bool>(CREATOR_KEY("IgnoreRatio"), false));
    m_ignoreLimits->setEnabled(m_seed->isChecked());
    // Hybrid by default (upstream): the widest compatibility with other clients.
    m_format->setCurrentIndex(qBound(0, s->loadValue<int>(CREATOR_KEY("TorrentFormat"), 1), 2));

    m_trackers->setPlainText(s->loadValue<QString>(CREATOR_KEY("TrackerList"), QString()));
    m_webSeeds->setPlainText(s->loadValue<QString>(CREATOR_KEY("WebSeedList"), QString()));
    m_comment->setPlainText(s->loadValue<QString>(CREATOR_KEY("Comments"), QString()));
    m_source->setText(s->loadValue<QString>(CREATOR_KEY("Source"), QString()));
    m_create->setEnabled(!m_path->text().isEmpty());
}

void TorrentCreatorDialog::saveSettings() const
{
    auto *s = SettingsStorage::instance();
    s->storeValue(CREATOR_KEY("LastAddPath"), m_path->text());
    s->storeValue(CREATOR_KEY("PieceSizeBytes"), m_pieceSize->currentData().toInt());
    s->storeValue(CREATOR_KEY("IgnoreDotfiles"), m_ignoreDotfiles->isChecked());
    s->storeValue(CREATOR_KEY("PrivateTorrent"), m_private->isChecked());
    s->storeValue(CREATOR_KEY("StartSeeding"), m_seed->isChecked());
    s->storeValue(CREATOR_KEY("IgnoreRatio"), m_ignoreLimits->isChecked());
    s->storeValue(CREATOR_KEY("TorrentFormat"), m_format->currentIndex());
    s->storeValue(CREATOR_KEY("TrackerList"), m_trackers->toPlainText());
    s->storeValue(CREATOR_KEY("WebSeedList"), m_webSeeds->toPlainText());
    s->storeValue(CREATOR_KEY("Comments"), m_comment->toPlainText());
    s->storeValue(CREATOR_KEY("Source"), m_source->text());
}

void TorrentCreatorDialog::setInputPath(const Path &path)
{
    if (path.isEmpty() || m_creating)
        return;
    m_path->setText(path.toString());
    m_path->setCursorPosition(0);   // show the start of a long path
    m_progress->setValue(0);
    m_pieceCount->setText(QStringLiteral("0"));
    m_create->setEnabled(true);
}

Path TorrentCreatorDialog::inputPath() const
{
    return Path(m_path->text());
}

BitTorrent::TorrentFormat TorrentCreatorDialog::torrentFormat() const
{
    switch (m_format->currentIndex())
    {
    case 0:
        return BitTorrent::TorrentFormat::V2;
    case 1:
        return BitTorrent::TorrentFormat::Hybrid;
    case 2:
        return BitTorrent::TorrentFormat::V1;
    }
    return BitTorrent::TorrentFormat::Hybrid;
}

BitTorrent::TorrentCreatorParams TorrentCreatorDialog::buildParams(const Path &torrentFile) const
{
    Path source = inputPath();
#ifdef Q_OS_WIN
    // Resolve a shortcut in the path, otherwise the checks that follow would see it as invalid.
    source = Utils::Fs::toCanonicalPath(source);
#endif
    return BitTorrent::TorrentCreatorParams {
        .ignoreDotfiles = m_ignoreDotfiles->isChecked(),
        .isPrivate = m_private->isChecked(),
        .torrentFormat = torrentFormat(),
        .pieceSize = m_pieceSize->currentData().toInt(),
        .sourcePath = source,
        .torrentFilePath = torrentFile,
        .comment = m_comment->toPlainText(),
        .source = m_source->text(),
        .trackers = trackerLines(m_trackers),
        .urlSeeds = lines(m_webSeeds)
    };
}

void TorrentCreatorDialog::notify(const bool error, const QString &title, const QString &text)
{
    if (m_notifier)
        m_notifier(error, title, text);
    else if (error)
        RoMessageBox::error(this, title, text);
    else
        RoMessageBox::info(this, title, text);
}

void TorrentCreatorDialog::setBusy(const bool busy)
{
    m_creating = busy;
    m_scroll->setEnabled(!busy);
    m_create->setEnabled(!busy && !m_path->text().isEmpty());
    // Cancel is always live: it stops a running creation, otherwise it closes the dialog.
    m_cancel->setEnabled(true);
    setCursor(busy ? Qt::BusyCursor : Qt::ArrowCursor);
}

void TorrentCreatorDialog::onCreateClicked()
{
    if (m_creating)
        return;
    const Path source = buildParams({}).sourcePath;
    if (!Utils::Fs::isReadable(source))
    {
        notify(true, tr("Torrent creation failed"), tr("Reason: Path to file/folder is not readable."));
        return;
    }

    auto *s = SettingsStorage::instance();
    const Path lastDir {s->loadValue<QString>(CREATOR_KEY("LastSavePath"), Utils::Fs::homePath().toString())};
    const Path suggestion = lastDir / Path(source.filename() + TORRENT_FILE_EXTENSION);
    Path dest {QFileDialog::getSaveFileName(this, tr("Select where to save the new torrent")
            , suggestion.data(), tr("Torrent Files (*.torrent)"))};
    if (dest.isEmpty())
        return;
    if (!dest.isValid())
    {
        notify(true, tr("Invalid file name"), tr("The name is invalid: \"%1\"").arg(dest.filename()));
        return;
    }
    if (!dest.hasExtension(TORRENT_FILE_EXTENSION))
        dest += TORRENT_FILE_EXTENSION;
    s->storeValue(CREATOR_KEY("LastSavePath"), dest.parentPath().toString());

    startCreation(dest);
}

bool TorrentCreatorDialog::startCreation(const Path &torrentFile)
{
    if (m_creating)
        return false;
    const BitTorrent::TorrentCreatorParams params = buildParams(torrentFile);
    if (!Utils::Fs::isReadable(params.sourcePath))
    {
        notify(true, tr("Torrent creation failed"), tr("Reason: Path to file/folder is not readable."));
        return false;
    }

    m_seedAfter = m_seed->isChecked();
    m_ignoreLimitsAfter = m_ignoreLimits->isChecked();
    m_cancelRequested = false;
    m_progress->setValue(0);
    setBusy(true);

    // The creator is owned here: it is a QObject whose signals are delivered on this thread while run() executes
    // in the pool, and it must not vanish while Cancel may still reach it.
    m_creator = new BitTorrent::TorrentCreator(params);
    m_creator->setAutoDelete(false);
    connect(m_creator, &BitTorrent::TorrentCreator::progressUpdated, this, [this](const int percent)
    {
        m_progress->setValue(percent);
    });
    connect(m_creator, &BitTorrent::TorrentCreator::creationSuccess, this, &TorrentCreatorDialog::handleSuccess);
    connect(m_creator, &BitTorrent::TorrentCreator::creationFailure, this, &TorrentCreatorDialog::handleFailure);
    m_pool->start(m_creator);
    return true;
}

void TorrentCreatorDialog::cancelCreation()
{
    if (!m_creating || m_cancelRequested)
        return;
    m_cancelRequested = true;
    if (m_creator)
        m_creator->requestInterruption();
}

void TorrentCreatorDialog::reject()
{
    if (m_creating)
    {
        cancelCreation();
        return;
    }
    saveSettings();
    RoDialog::reject();
}

void TorrentCreatorDialog::handleFailure(const QString &message)
{
    if (m_creator)
    {
        m_creator->deleteLater();
        m_creator = nullptr;
    }
    const bool wasCancel = m_cancelRequested;
    m_cancelRequested = false;
    m_progress->setValue(0);
    setBusy(false);

    if (wasCancel)
    {
        emit cancelled();
        return;
    }
    notify(true, tr("Torrent creation failed"), message);
    emit failed(message);
}

void TorrentCreatorDialog::handleSuccess(const BitTorrent::TorrentCreatorResult &result)
{
    if (m_creator)
    {
        m_creator->deleteLater();
        m_creator = nullptr;
    }
    // A cancel that arrives after the last checkpoint still produced a complete file; report it as done.
    m_cancelRequested = false;
    m_progress->setValue(100);
    setBusy(false);

    if (m_seedAfter && m_session)
    {
        const auto loaded = BitTorrent::TorrentDescriptor::loadFromFile(result.torrentFilePath);
        if (loaded)
        {
            BitTorrent::AddTorrentParams params;
            // No global defaults: set each value, so a changed default cannot make seeding fail unexpectedly.
            params.addStopped = false;
            params.contentLayout = BitTorrent::TorrentContentLayout::Original;
            params.savePath = result.savePath;
            params.seedMode = true;
            params.stopCondition = BitTorrent::Torrent::StopCondition::None;
            params.useAutoTMM = false;   // otherwise the default save path would replace savePath
            params.useDownloadPath = false;
            if (m_ignoreLimitsAfter)
            {
                params.shareLimits.ratioLimit = BitTorrent::NO_RATIO_LIMIT;
                params.shareLimits.seedingTimeLimit = BitTorrent::NO_SEEDING_TIME_LIMIT;
                params.shareLimits.inactiveSeedingTimeLimit = BitTorrent::NO_SEEDING_TIME_LIMIT;
            }
            if (!m_session->addTorrent(loaded.value(), params))
                notify(true, tr("Add torrent failed"), tr("The new torrent could not be added to the transfer list."));
        }
        else
        {
            notify(true, tr("Add torrent failed")
                    , tr("Add torrent to transfer list failed.") + QLatin1Char('\n')
                    + tr("Reason: \"%1\"").arg(loaded.error()));
        }
    }

    notify(false, tr("Torrent Creator"), tr("Torrent created:") + QLatin1Char('\n') + result.torrentFilePath.toString());
    emit created(result.torrentFilePath);
}

void TorrentCreatorDialog::onCalculatePieces()
{
    if (m_path->text().isEmpty())
    {
        m_pieceCount->setText(QStringLiteral("0"));
        return;
    }
    m_calc->setEnabled(false);
    m_pieceCount->setText(tr("Calculating..."));

    auto *thread = new PieceCalculationThread([path = inputPath()
            , pieceSize = m_pieceSize->currentData().toInt()
            , ignoreDotfiles = m_ignoreDotfiles->isChecked()
            , format = torrentFormat()]() -> int
    {
        return BitTorrent::TorrentCreator::calculateTotalPieces(path, pieceSize, ignoreDotfiles, format);
    });
    thread->setObjectName(QStringLiteral("PieceCalculationThread"));
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &PieceCalculationThread::resultReady, this, [this](const int pieces)
    {
        m_pieceCount->setText(QString::number(pieces));
        m_calc->setEnabled(true);
    });
    thread->start();
}

void TorrentCreatorDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (!m_creating && (event->mimeData()->hasUrls() || event->mimeData()->hasText()))
        event->acceptProposedAction();
}

void TorrentCreatorDialog::dropEvent(QDropEvent *event)
{
    event->acceptProposedAction();
    setInputPath(pathFromMime(event->mimeData()));
}

#include "TorrentCreatorDialog.moc"

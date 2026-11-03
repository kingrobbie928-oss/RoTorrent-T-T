// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "AddNewTorrentDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedLayout>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/torrentcontentlayout.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/path.h"
#include "base/settingsstorage.h"
#include "base/torrentfileguard.h"
#include "base/utils/misc.h"
#include "base/preferences.h"
#include "RoDialog.h"
#include "RoMessageBox.h"

namespace
{
    constexpr int FileIndexRole = Qt::UserRole + 1;

    // A magnet link may carry only the v1 or only the v2 hash of a hybrid torrent: either one matches.
    bool sameTorrent(const BitTorrent::InfoHash &a, const BitTorrent::InfoHash &b)
    {
        return (a.v1().isValid() && (a.v1() == b.v1())) || (a.v2().isValid() && (a.v2() == b.v2()));
    }

    QString sizeText(const qlonglong bytes)
    {
        return Utils::Misc::friendlyUnit(bytes);
    }

    QTreeWidgetItem *childNamed(QTreeWidgetItem *parent, QTreeWidget *tree, const QString &name)
    {
        const int count = parent ? parent->childCount() : tree->topLevelItemCount();
        for (int i = 0; i < count; ++i)
        {
            QTreeWidgetItem *item = parent ? parent->child(i) : tree->topLevelItem(i);
            if ((item->data(0, FileIndexRole).toInt() < 0) && (item->text(0) == name))
                return item;
        }
        return nullptr;
    }

    // Builds the folder / file tree. Folder sizes are the sum of their files.
    void fillTree(QTreeWidget *tree, const BitTorrent::TorrentInfo &info)
    {
        tree->setUpdatesEnabled(false);
        QHash<QTreeWidgetItem *, qlonglong> folderSizes;
        for (int i = 0; i < info.filesCount(); ++i)
        {
            const QStringList parts = info.filePath(i).data().split(QLatin1Char('/'), Qt::SkipEmptyParts);
            QTreeWidgetItem *parent = nullptr;
            for (int p = 0; p < (parts.size() - 1); ++p)
            {
                QTreeWidgetItem *folder = childNamed(parent, tree, parts[p]);
                if (!folder)
                {
                    folder = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
                    folder->setText(0, parts[p]);
                    folder->setData(0, FileIndexRole, -1);
                    folder->setFlags(folder->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
                    folder->setCheckState(0, Qt::Checked);
                }
                folderSizes[folder] += info.fileSize(i);
                parent = folder;
            }
            auto *file = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
            file->setText(0, parts.isEmpty() ? QString() : parts.last());
            file->setText(1, sizeText(info.fileSize(i)));
            file->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            file->setData(0, FileIndexRole, i);
            file->setFlags(file->flags() | Qt::ItemIsUserCheckable);
            file->setCheckState(0, Qt::Checked);
        }
        for (auto it = folderSizes.cbegin(); it != folderSizes.cend(); ++it)
        {
            it.key()->setText(1, sizeText(it.value()));
            it.key()->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        }
        tree->expandAll();
        tree->setUpdatesEnabled(true);
    }

    void collectPriorities(QTreeWidgetItem *item, QList<BitTorrent::DownloadPriority> &out)
    {
        const int index = item->data(0, FileIndexRole).toInt();
        if (index >= 0)
        {
            out[index] = (item->checkState(0) == Qt::Unchecked)
                    ? BitTorrent::DownloadPriority::Ignored : BitTorrent::DownloadPriority::Normal;
            return;
        }
        for (int i = 0; i < item->childCount(); ++i)
            collectPriorities(item->child(i), out);
    }
}

bool AddNewTorrentDialog::isEnabled()
{
    // One setting, shared with Options > Downloads > "Display torrent content and some options".
    return Preferences::instance()->isAddNewTorrentDialogEnabled();
}

void AddNewTorrentDialog::setEnabled(const bool enabled)
{
    Preferences::instance()->setAddNewTorrentDialogEnabled(enabled);
}

std::optional<AddNewTorrentDialog::Result> AddNewTorrentDialog::run(QWidget *parent, BitTorrent::Session *session
        , const BitTorrent::TorrentDescriptor &descriptor, const QString &sourcePath
        , const BitTorrent::AddTorrentParams &initial)
{
    BitTorrent::TorrentDescriptor current = descriptor;
    const BitTorrent::TorrentID torrentId = BitTorrent::TorrentID::fromInfoHash(descriptor.infoHash());
    bool hasMetadata = current.info().has_value();
    bool metadataRequested = false;

    RoDialog dialog(parent, QObject::tr("Add New Torrent"), 780);

    auto *host = new QDialog;
    host->setObjectName(QStringLiteral("roUi"));
    host->setWindowFlags(Qt::Widget);
    auto *outer = new QVBoxLayout(host);
    outer->setContentsMargins(18, 14, 18, 14);
    outer->setSpacing(12);

    // --- torrent summary ---
    auto *name = new QLabel(current.name().isEmpty() ? current.infoHash().toString() : current.name());
    name->setObjectName(QStringLiteral("dlgName"));
    name->setWordWrap(true);
    name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    outer->addWidget(name);
    auto *sub = new QLabel;
    sub->setObjectName(QStringLiteral("dlgBodyText"));
    outer->addWidget(sub);
    const auto setSummary = [&current, sub]
    {
        sub->setText(current.info().has_value()
                ? QObject::tr("%1 in %2 file(s)").arg(sizeText(current.info()->totalSize()))
                        .arg(current.info()->filesCount())
                : QObject::tr("Retrieving metadata..."));
    };
    setSummary();

    auto *columns = new QHBoxLayout;
    columns->setSpacing(18);
    outer->addLayout(columns, 1);

    // --- left: options ---
    auto *left = new QVBoxLayout;
    left->setSpacing(8);
    columns->addLayout(left, 0);

    left->addWidget(new QLabel(QObject::tr("Torrent Management Mode:")));
    auto *tmmBox = new QComboBox;
    tmmBox->addItem(QObject::tr("Manual"), false);
    tmmBox->addItem(QObject::tr("Automatic"), true);
    tmmBox->setCurrentIndex(session->isAutoTMMDisabledByDefault() ? 0 : 1);
    left->addWidget(tmmBox);

    left->addWidget(new QLabel(QObject::tr("Save at:")));
    auto *pathRow = new QHBoxLayout;
    auto *savePath = new QLineEdit(session->savePath().toString());
    auto *browse = new QPushButton(QObject::tr("Browse..."));
    pathRow->addWidget(savePath, 1);
    pathRow->addWidget(browse);
    left->addLayout(pathRow);
    QObject::connect(browse, &QPushButton::clicked, &dialog, [&dialog, savePath]
    {
        const QString dir = QFileDialog::getExistingDirectory(&dialog, QObject::tr("Choose save path"), savePath->text());
        if (!dir.isEmpty())
            savePath->setText(dir);
    });

    auto *useIncomplete = new QCheckBox(QObject::tr("Use another path for incomplete torrent"));
    useIncomplete->setChecked(session->isDownloadPathEnabled());
    left->addWidget(useIncomplete);
    auto *incompleteRow = new QHBoxLayout;
    auto *incompletePath = new QLineEdit(session->downloadPath().toString());
    auto *incompleteBrowse = new QPushButton(QObject::tr("Browse..."));
    incompleteRow->addWidget(incompletePath, 1);
    incompleteRow->addWidget(incompleteBrowse);
    left->addLayout(incompleteRow);
    QObject::connect(incompleteBrowse, &QPushButton::clicked, &dialog, [&dialog, incompletePath]
    {
        const QString dir = QFileDialog::getExistingDirectory(&dialog, QObject::tr("Choose incomplete torrent path")
                , incompletePath->text());
        if (!dir.isEmpty())
            incompletePath->setText(dir);
    });

    left->addWidget(new QLabel(QObject::tr("Category:")));
    auto *category = new QComboBox;
    category->setEditable(true);
    category->addItem(QString());
    category->addItems(session->categories());
    category->setCurrentIndex(0);
    category->setMinimumWidth(290);   // gives the left column room for the save path field
    left->addWidget(category);

    // Automatic mode: the category (or default) save path applies and the path fields are read-only.
    const auto refreshPaths = [=]
    {
        const bool automatic = tmmBox->currentData().toBool();
        const QString cat = category->currentText().trimmed();
        if (automatic)
        {
            const Path p = session->categorySavePath(cat);
            savePath->setText((p.isEmpty() ? session->savePath() : p).toString());
        }
        else if (!cat.isEmpty() && session->categories().contains(cat))
        {
            // Choosing an existing category fills its save path, as upstream does for Manual mode.
            const Path p = session->categorySavePath(cat);
            if (!p.isEmpty())
                savePath->setText(p.toString());
        }
        savePath->setEnabled(!automatic);
        browse->setEnabled(!automatic);
        useIncomplete->setEnabled(!automatic);
        incompletePath->setEnabled(!automatic && useIncomplete->isChecked());
        incompleteBrowse->setEnabled(!automatic && useIncomplete->isChecked());
    };
    QObject::connect(category, &QComboBox::currentTextChanged, savePath, refreshPaths);
    QObject::connect(tmmBox, &QComboBox::currentIndexChanged, savePath, refreshPaths);
    QObject::connect(useIncomplete, &QCheckBox::toggled, savePath, refreshPaths);
    refreshPaths();

    left->addWidget(new QLabel(QObject::tr("Content layout:")));
    auto *layoutBox = new QComboBox;
    layoutBox->addItem(QObject::tr("Original"), static_cast<int>(BitTorrent::TorrentContentLayout::Original));
    layoutBox->addItem(QObject::tr("Create subfolder"), static_cast<int>(BitTorrent::TorrentContentLayout::Subfolder));
    layoutBox->addItem(QObject::tr("Don't create subfolder"), static_cast<int>(BitTorrent::TorrentContentLayout::NoSubfolder));
    const int currentLayout = static_cast<int>(session->torrentContentLayout());
    layoutBox->setCurrentIndex(std::max(0, layoutBox->findData(currentLayout)));
    left->addWidget(layoutBox);

    left->addWidget(new QLabel(QObject::tr("Pause condition:")));
    auto *stopBox = new QComboBox;
    stopBox->addItem(QObject::tr("None"), static_cast<int>(BitTorrent::Torrent::StopCondition::None));
    stopBox->addItem(QObject::tr("Metadata received"), static_cast<int>(BitTorrent::Torrent::StopCondition::MetadataReceived));
    stopBox->addItem(QObject::tr("Files checked"), static_cast<int>(BitTorrent::Torrent::StopCondition::FilesChecked));
    stopBox->setCurrentIndex(std::max(0, stopBox->findData(static_cast<int>(session->torrentStopCondition()))));
    left->addWidget(stopBox);

    auto *start = new QCheckBox(QObject::tr("Resume torrent after adding"));
    start->setChecked(!session->isAddTorrentStopped());
    auto *top = new QCheckBox(QObject::tr("Add to top of queue"));
    top->setChecked(session->isAddTorrentToQueueTop());
    auto *skipCheck = new QCheckBox(QObject::tr("Skip hash check"));
    skipCheck->setEnabled(hasMetadata);   // needs the file list; follows the metadata
    auto *sequential = new QCheckBox(QObject::tr("Download in sequential order"));
    auto *firstLast = new QCheckBox(QObject::tr("Download first and last pieces first"));
    for (QCheckBox *box : {start, top, skipCheck, sequential, firstLast})
        left->addWidget(box);
    QCheckBox *keepFile = nullptr;
    if (!sourcePath.isEmpty())
    {
        keepFile = new QCheckBox(QObject::tr("Do not delete .torrent file"));
        keepFile->setChecked(TorrentFileGuard::autoDeleteMode() == TorrentFileGuard::Never);
        left->addWidget(keepFile);
    }
    // Values from the command line (or any other caller) pre-fill the fields. The save path goes last because
    // choosing the category or the management mode rewrites it.
    if (initial.useAutoTMM)
        tmmBox->setCurrentIndex(*initial.useAutoTMM ? 1 : 0);
    else if (!initial.savePath.isEmpty())
        tmmBox->setCurrentIndex(0);
    if (!initial.category.isEmpty())
        category->setCurrentText(initial.category);
    if (initial.contentLayout)
        layoutBox->setCurrentIndex(std::max(0, layoutBox->findData(static_cast<int>(*initial.contentLayout))));
    if (initial.stopCondition)
        stopBox->setCurrentIndex(std::max(0, stopBox->findData(static_cast<int>(*initial.stopCondition))));
    if (initial.addStopped)
        start->setChecked(!*initial.addStopped);
    if (initial.addToQueueTop)
        top->setChecked(*initial.addToQueueTop);
    sequential->setChecked(initial.sequential);
    firstLast->setChecked(initial.firstLastPiecePriority);
    if (initial.seedMode && skipCheck->isEnabled())
        skipCheck->setChecked(true);
    if (!initial.savePath.isEmpty() && !tmmBox->currentData().toBool())
        savePath->setText(initial.savePath.toString());
    if (initial.useDownloadPath)
        useIncomplete->setChecked(*initial.useDownloadPath);
    if (!initial.downloadPath.isEmpty())
        incompletePath->setText(initial.downloadPath.toString());
    left->addStretch(1);

    // --- right: files (or the metadata wait) ---
    auto *right = new QVBoxLayout;
    right->setSpacing(8);
    columns->addLayout(right, 1);

    auto *selectRow = new QHBoxLayout;
    selectRow->addStretch(1);
    auto *saveTorrent = new QPushButton(QObject::tr("Save as .torrent file..."));
    auto *all = new QPushButton(QObject::tr("Select All"));
    auto *none = new QPushButton(QObject::tr("Select None"));
    selectRow->addWidget(saveTorrent);
    selectRow->addWidget(all);
    selectRow->addWidget(none);
    right->addLayout(selectRow);

    auto *stack = new QStackedLayout;
    right->addLayout(stack, 1);

    // page 0: waiting for metadata
    auto *wait = new QWidget;
    auto *waitLayout = new QVBoxLayout(wait);
    waitLayout->setContentsMargins(0, 0, 0, 0);
    auto *note = new QLabel(QObject::tr("Retrieving metadata from the network. The file list appears here "
                                        "as soon as it arrives. You can also add the torrent now and pick "
                                        "files later in the Content tab."));
    note->setObjectName(QStringLiteral("dlgBodyText"));
    note->setWordWrap(true);
    note->setMinimumWidth(320);
    auto *bar = new QProgressBar;
    bar->setRange(0, 0);   // busy indicator: the engine reports no progress for metadata
    bar->setTextVisible(false);
    waitLayout->addWidget(note);
    waitLayout->addWidget(bar);
    waitLayout->addStretch(1);
    stack->addWidget(wait);

    // page 1: the file tree
    auto *tree = new QTreeWidget;
    tree->setObjectName(QStringLiteral("dTable"));
    tree->setColumnCount(2);
    tree->setHeaderLabels({QObject::tr("Name"), QObject::tr("Size")});
    tree->setRootIsDecorated(true);
    tree->setIndentation(18);
    tree->setMinimumSize(320, 220);
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    stack->addWidget(tree);

    auto setAll = [tree](const Qt::CheckState state)
    {
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            tree->topLevelItem(i)->setCheckState(0, state);
    };
    QObject::connect(all, &QPushButton::clicked, tree, [setAll] { setAll(Qt::Checked); });
    QObject::connect(none, &QPushButton::clicked, tree, [setAll] { setAll(Qt::Unchecked); });
    QObject::connect(saveTorrent, &QPushButton::clicked, &dialog, [&dialog, &current]
    {
        const QString suggested = QDir::home().filePath(current.name() + QStringLiteral(".torrent"));
        const QString file = QFileDialog::getSaveFileName(&dialog, QObject::tr("Save as .torrent file")
                , suggested, QObject::tr("Torrent Files (*.torrent)"));
        if (file.isEmpty())
            return;
        if (const auto result = current.saveToFile(Path(file)); !result)
            RoMessageBox::error(&dialog, QObject::tr("Add New Torrent"), result.error());
    });

    // Switches the right side to the file tree once the metadata is known.
    const auto showFiles = [&]
    {
        fillTree(tree, *current.info());
        stack->setCurrentWidget(tree);
        all->setEnabled(true);
        none->setEnabled(true);
        saveTorrent->setEnabled(true);
        skipCheck->setEnabled(true);
        setSummary();
    };
    if (hasMetadata)
    {
        showFiles();
    }
    else
    {
        stack->setCurrentWidget(wait);
        all->setEnabled(false);
        none->setEnabled(false);
        saveTorrent->setEnabled(false);
        metadataRequested = session->downloadMetadata(current);
        if (!metadataRequested)
            note->setText(QObject::tr("The metadata could not be requested. The torrent can still be added; "
                                      "its files appear in the Content tab once it is known."));
        QObject::connect(session, &BitTorrent::Session::metadataDownloaded, &dialog
                , [&](const BitTorrent::TorrentInfo &info)
        {
            if (hasMetadata || !sameTorrent(info.infoHash(), descriptor.infoHash()))
                return;
            current.setTorrentInfo(info);
            hasMetadata = true;
            showFiles();
        });
    }

    // --- bottom row ---
    auto *neverShow = new QCheckBox(QObject::tr("Never show again"));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setProperty("primary", true);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *bottom = new QHBoxLayout;
    bottom->addWidget(neverShow);
    bottom->addStretch(1);
    bottom->addWidget(buttons);
    outer->addLayout(bottom);

    dialog.setContent(host);
    const bool accepted = (dialog.run() == QDialog::Accepted);
    if (metadataRequested)
        session->cancelDownloadMetadata(torrentId);   // drop the hidden metadata torrent (no-op if already gone)
    if (!accepted)
        return std::nullopt;

    Result result;
    result.descriptor = current;
    result.params = initial;   // keeps what the dialog has no widget for (tags, limits, name)
    BitTorrent::AddTorrentParams &params = result.params;
    params.category = category->currentText().trimmed();
    params.useAutoTMM = tmmBox->currentData().toBool();
    if (!*params.useAutoTMM)
    {
        params.savePath = Path(savePath->text().trimmed());
        params.useDownloadPath = useIncomplete->isChecked();
        if (useIncomplete->isChecked())
            params.downloadPath = Path(incompletePath->text().trimmed());
    }
    params.addStopped = !start->isChecked();
    params.addToQueueTop = top->isChecked();
    params.stopCondition = static_cast<BitTorrent::Torrent::StopCondition>(stopBox->currentData().toInt());
    params.seedMode = hasMetadata && skipCheck->isChecked();
    params.sequential = sequential->isChecked();
    params.firstLastPiecePriority = firstLast->isChecked();
    params.contentLayout = static_cast<BitTorrent::TorrentContentLayout>(layoutBox->currentData().toInt());
    if (hasMetadata)
    {
        QList<BitTorrent::DownloadPriority> priorities(current.info()->filesCount(), BitTorrent::DownloadPriority::Normal);
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            collectPriorities(tree->topLevelItem(i), priorities);
        params.filePriorities = priorities;
    }
    result.keepTorrentFile = keepFile && keepFile->isChecked();
    if (neverShow->isChecked())
        setEnabled(false);
    return result;
}

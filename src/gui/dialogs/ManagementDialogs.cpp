// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "ManagementDialogs.h"

#include <algorithm>

#include <QAbstractTableModel>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QStandardItemModel>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/utils/misc.h"
#include "MainWindow.h"
#include "dialogs/RoMessageBox.h"
#include "dialogs/TorrentOptionsDialog.h"
#include "theme/RoTheme.h"

namespace
{
    QDialog *makeHost(QVBoxLayout *&layout)
    {
        auto *host = new QDialog;
        host->setObjectName(QStringLiteral("roUi"));
        host->setWindowFlags(Qt::Widget);
        layout = new QVBoxLayout(host);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(10);
        return host;
    }

    QDialogButtonBox *makeButtons(RoDialog &dialog, const QString &okText = {})
    {
        auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QPushButton *ok = box->button(QDialogButtonBox::Ok);
        ok->setProperty("primary", true);
        if (!okText.isEmpty())
            ok->setText(okText);
        QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        return box;
    }

    QList<BitTorrent::Torrent *> torrentsFor(BitTorrent::Session *session, const QStringList &ids)
    {
        QList<BitTorrent::Torrent *> out;
        if (!session)
            return out;
        for (const QString &id : ids)
        {
            const auto tid = BitTorrent::TorrentID::fromString(id);
            if (BitTorrent::Torrent *t = tid.isValid() ? session->getTorrent(tid) : nullptr)
                out.append(t);
        }
        return out;
    }

    std::function<void(const Path &)> &opener()
    {
        static std::function<void(const Path &)> fn = [](const Path &path)
        {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path.data()));
        };
        return fn;
    }
}

// ============================================================================================================================
//  Cookies
// ============================================================================================================================
class CookiesDialog::Model final : public QAbstractTableModel
{
public:
    enum Column { Domain, Path, Name, Value, Expiration, ColumnCount };

    explicit Model(const QList<QNetworkCookie> &cookies, QObject *parent)
        : QAbstractTableModel(parent), m_cookies(cookies) {}

    QList<QNetworkCookie> cookies() const { return m_cookies; }

    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : m_cookies.size(); }
    int columnCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : ColumnCount; }

    QVariant headerData(const int section, const Qt::Orientation orientation, const int role) const override
    {
        if ((role != Qt::DisplayRole) || (orientation != Qt::Horizontal))
            return {};
        switch (section)
        {
        case Domain: return CookiesDialog::tr("Domain");
        case Path: return CookiesDialog::tr("Path");
        case Name: return CookiesDialog::tr("Name");
        case Value: return CookiesDialog::tr("Value");
        case Expiration: return CookiesDialog::tr("Expiration date");
        }
        return {};
    }

    QVariant data(const QModelIndex &index, const int role) const override
    {
        if (!index.isValid() || (index.row() >= m_cookies.size()))
            return {};
        const QNetworkCookie &c = m_cookies.at(index.row());
        if (role == Qt::ToolTipRole)
            return (index.column() == Value) ? QVariant(QString::fromLatin1(c.value())) : QVariant();
        if ((role != Qt::DisplayRole) && (role != Qt::EditRole))
            return {};
        switch (index.column())
        {
        case Domain: return c.domain();
        case Path: return c.path();
        case Name: return QString::fromLatin1(c.name());
        case Value: return QString::fromLatin1(c.value());
        case Expiration:
            if (role == Qt::EditRole)
                return c.expirationDate().isValid() ? c.expirationDate() : QDateTime::currentDateTime().addYears(2);
            return c.expirationDate().isValid() ? QVariant(c.expirationDate().toString(u"yyyy-MM-dd"_s))
                                                : QVariant(CookiesDialog::tr("Session"));
        }
        return {};
    }

    bool setData(const QModelIndex &index, const QVariant &value, const int role) override
    {
        if ((role != Qt::EditRole) || !index.isValid() || (index.row() >= m_cookies.size()))
            return false;
        QNetworkCookie &c = m_cookies[index.row()];
        switch (index.column())
        {
        case Domain: c.setDomain(value.toString()); break;
        case Path: c.setPath(value.toString()); break;
        case Name: c.setName(value.toString().toLatin1()); break;
        case Value: c.setValue(value.toString().toLatin1()); break;
        case Expiration: c.setExpirationDate(value.toDateTime()); break;
        default: return false;
        }
        emit dataChanged(index, index);
        return true;
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        return index.isValid() ? (QAbstractTableModel::flags(index) | Qt::ItemIsEditable) : Qt::NoItemFlags;
    }

    bool insertRows(const int row, const int count, const QModelIndex &parent = {}) override
    {
        if ((row < 0) || (row > m_cookies.size()) || (count < 1))
            return false;
        QNetworkCookie fresh;
        fresh.setExpirationDate(QDateTime::currentDateTime().addYears(2));
        beginInsertRows(parent, row, row + count - 1);
        for (int i = 0; i < count; ++i)
            m_cookies.insert(row, fresh);
        endInsertRows();
        return true;
    }

    bool removeRows(const int row, const int count, const QModelIndex &parent = {}) override
    {
        if ((row < 0) || (count < 1) || ((row + count) > m_cookies.size()))
            return false;
        beginRemoveRows(parent, row, row + count - 1);
        m_cookies.remove(row, count);
        endRemoveRows();
        return true;
    }

private:
    QList<QNetworkCookie> m_cookies;
};

CookiesDialog::CookiesDialog(QWidget *parent, const QList<QNetworkCookie> &cookies)
    : RoDialog(parent, tr("Manage Cookies"), 620)
    , m_model(new Model(cookies, this))
{
    QVBoxLayout *layout = nullptr;
    QDialog *host = makeHost(layout);

    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    m_view = new QTreeView;
    m_view->setObjectName(QStringLiteral("cookiesView"));
    m_view->setAlternatingRowColors(true);
    m_view->setModel(m_model);
    m_view->setRootIsDecorated(false);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setTextElideMode(Qt::ElideMiddle);
    m_view->setMinimumHeight(300);
    m_view->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    m_view->header()->setStretchLastSection(true);
    m_view->setColumnWidth(Model::Domain, 140);
    m_view->setColumnWidth(Model::Path, 80);
    m_view->setColumnWidth(Model::Name, 80);
    m_view->setColumnWidth(Model::Value, 110);
    row->addWidget(m_view, 1);

    auto *side = new QVBoxLayout;
    side->setSpacing(6);
    const RoPalette &pal = RoTheme::instance().pal();
    auto makeTool = [&](const QString &name, const QString &icon, const QString &tip)
    {
        auto *button = new QToolButton;
        button->setObjectName(name);
        button->setIcon(RoTheme::instance().icon(icon, pal.muted, 16));
        button->setToolTip(tip);
        button->setAutoRaise(true);
        return button;
    };
    m_add = makeTool(QStringLiteral("buttonAdd"), QStringLiteral("plus"), tr("Add cookie"));
    m_delete = makeTool(QStringLiteral("buttonDelete"), QStringLiteral("delete"), tr("Delete selected cookies"));
    side->addWidget(m_add);
    side->addWidget(m_delete);
    side->addStretch(1);
    row->addLayout(side);
    layout->addLayout(row, 1);

    connect(m_add, &QToolButton::clicked, this, [this]
    {
        const int at = m_view->selectionModel()->currentIndex().isValid() ? (m_view->selectionModel()->currentIndex().row() + 1)
                                                                          : m_model->rowCount();
        m_model->insertRow(at);
        m_view->selectionModel()->setCurrentIndex(m_model->index(at, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        m_view->edit(m_model->index(at, 0));
    });
    connect(m_delete, &QToolButton::clicked, this, [this]
    {
        QModelIndexList rows = m_view->selectionModel()->selectedRows();
        std::ranges::sort(rows, [](const QModelIndex &a, const QModelIndex &b) { return a.row() > b.row(); });
        for (const QModelIndex &index : asConst(rows))
            m_model->removeRow(index.row());
    });

    layout->addWidget(makeButtons(*this));
    setContent(host);
    if (m_model->rowCount() > 0)
        m_view->selectionModel()->setCurrentIndex(m_model->index(0, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

QList<QNetworkCookie> CookiesDialog::cookies() const
{
    return m_model->cookies();
}

bool CookiesDialog::show(QWidget *parent)
{
    Net::DownloadManager *manager = Net::DownloadManager::instance();
    if (!manager)
        return false;
    CookiesDialog dialog(parent, manager->allCookies());
    if (dialog.run() != QDialog::Accepted)
        return false;
    manager->setAllCookies(dialog.cookies());
    return true;
}

// ============================================================================================================================
//  Tracker list
// ============================================================================================================================
TrackerListDialog::TrackerListDialog(QWidget *parent, const QList<BitTorrent::TrackerEntry> &trackers)
    : RoDialog(parent, tr("Tracker list"), 508)
{
    QVBoxLayout *layout = nullptr;
    QDialog *host = makeHost(layout);
    auto *help = new QLabel(tr("One tracker URL per line.\n\n"
                               "- You can split the trackers into groups by inserting blank lines.\n"
                               "- All trackers within the same group will belong to the same tier.\n"
                               "- The group on top will be tier 0, the next group tier 1 and so on.\n"
                               "- Below will show the common subset of trackers of the selected torrents."));
    help->setWordWrap(true);
    layout->addWidget(help);

    m_edit = new QPlainTextEdit;
    m_edit->setObjectName(QStringLiteral("plainTextEdit"));
    m_edit->setMinimumHeight(325);
    m_edit->setPlainText(toText(trackers));
    layout->addWidget(m_edit, 1);
    layout->addWidget(makeButtons(*this));
    setContent(host);
}

QString TrackerListDialog::toText(const QList<BitTorrent::TrackerEntry> &trackers)
{
    int maxTier = -1;
    QHash<int, QString> tiers;
    for (const BitTorrent::TrackerEntry &entry : trackers)
    {
        tiers[entry.tier] += (entry.url + u'\n');
        maxTier = std::max(maxTier, entry.tier);
    }
    QString text = tiers.value(0);
    for (int i = 1; i <= maxTier; ++i)
        text += (u'\n' + tiers.value(i));
    return text;
}

QList<BitTorrent::TrackerEntry> TrackerListDialog::trackers() const
{
    return BitTorrent::parseTrackerEntries(m_edit->toPlainText());
}

QList<BitTorrent::TrackerEntry> TrackerListDialog::commonTrackers(const QList<BitTorrent::Torrent *> &torrents)
{
    QList<BitTorrent::TrackerEntry> common;
    if (torrents.isEmpty())
        return common;
    for (const BitTorrent::TrackerEntryStatus &status : torrents.first()->trackers())
        common.append({status.url, status.tier});
    for (qsizetype i = 1; i < torrents.size(); ++i)
    {
        QSet<BitTorrent::TrackerEntry> set;
        for (const BitTorrent::TrackerEntryStatus &status : torrents.at(i)->trackers())
            set.insert({status.url, status.tier});
        common.erase(std::remove_if(common.begin(), common.end(), [&set](const BitTorrent::TrackerEntry &e) { return !set.contains(e); })
                     , common.end());
    }
    return common;
}

bool TrackerListDialog::show(QWidget *parent, BitTorrent::Session *session, const QStringList &ids)
{
    const QList<BitTorrent::Torrent *> torrents = torrentsFor(session, ids);
    if (torrents.isEmpty())
        return false;
    TrackerListDialog dialog(parent, commonTrackers(torrents));
    if (dialog.run() != QDialog::Accepted)
        return false;
    const QList<BitTorrent::TrackerEntry> edited = dialog.trackers();
    for (BitTorrent::Torrent *torrent : torrents)
        torrent->replaceTrackers(edited);
    return true;
}

// ============================================================================================================================
//  Preview picker
// ============================================================================================================================
namespace
{
    constexpr int SortRole = Qt::UserRole;
}

PreviewSelectDialog::PreviewSelectDialog(QWidget *parent, const BitTorrent::Torrent *torrent)
    : RoDialog(parent, tr("Preview file"), 464)
    , m_torrent(torrent)
{
    QVBoxLayout *layout = nullptr;
    QDialog *host = makeHost(layout);
    auto *label = new QLabel(tr("The following files from torrent \"%1\" support previewing, please select one of them:")
                             .arg(torrent->name()));
    label->setWordWrap(true);
    layout->addWidget(label);

    auto *model = new QStandardItemModel(0, ColumnCount, this);
    model->setHorizontalHeaderLabels({tr("Name"), tr("Size"), tr("Progress"), QString()});
    const QList<qreal> progress = torrent->filesProgress();
    for (int i = 0; i < torrent->filesCount(); ++i)
    {
        const Path filePath = torrent->filePath(i);
        if (!Utils::Misc::isPreviewable(filePath))
            continue;
        auto *name = new QStandardItem(filePath.filename());
        name->setData(filePath.filename().toLower(), SortRole);
        auto *size = new QStandardItem(Utils::Misc::friendlyUnit(torrent->fileSize(i)));
        size->setData(torrent->fileSize(i), SortRole);
        const qreal fraction = (i < progress.size()) ? progress.at(i) : 0;
        auto *prog = new QStandardItem(QString::number(qRound(fraction * 100)) + u'%');
        prog->setData(fraction, SortRole);
        auto *index = new QStandardItem(QString::number(i));
        index->setData(i, SortRole);
        for (QStandardItem *item : {name, size, prog, index})
            item->setEditable(false);
        model->appendRow({name, size, prog, index});
    }
    model->setSortRole(SortRole);

    m_view = new QTreeView;
    m_view->setObjectName(QStringLiteral("previewList"));
    m_view->setAlternatingRowColors(true);
    m_view->setModel(model);
    m_view->setRootIsDecorated(false);
    m_view->setUniformRowHeights(true);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(NameColumn, Qt::AscendingOrder);
    m_view->hideColumn(FileIndexColumn);
    m_view->setColumnWidth(NameColumn, 250);
    m_view->setColumnWidth(SizeColumn, 90);
    m_view->setMinimumHeight(220);
    if (model->rowCount() > 0)
        m_view->setCurrentIndex(model->index(0, NameColumn));
    layout->addWidget(m_view, 1);

    auto *box = makeButtons(*this, tr("Preview"));
    layout->addWidget(box);
    connect(m_view, &QAbstractItemView::doubleClicked, this, &PreviewSelectDialog::accept);
    setContent(host);
}

int PreviewSelectDialog::rowCount() const
{
    return m_view->model()->rowCount();
}

bool PreviewSelectDialog::previewSelected()
{
    const QModelIndexList selected = m_view->selectionModel()->selectedRows(FileIndexColumn);
    if (selected.isEmpty())
        return false;
    m_torrent->flushCache();
    const int fileIndex = selected.first().data(SortRole).toInt();
    const Path path = m_torrent->actualStorageLocation() / m_torrent->actualFilePath(fileIndex);
    if (!path.exists())
    {
        RoMessageBox::error(isVisible() ? static_cast<QWidget *>(this) : parentWidget(), tr("Preview impossible")
                            , tr("Sorry, we can't preview this file: \"%1\".").arg(path.toString()));
        return false;
    }
    opener()(path);
    return true;
}

void PreviewSelectDialog::accept()
{
    if (previewSelected())
        RoDialog::accept();
}

void PreviewSelectDialog::setOpener(std::function<void(const Path &)> fn)
{
    opener() = fn ? std::move(fn) : std::function<void(const Path &)>([](const Path &path)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path.data()));
    });
}

void PreviewSelectDialog::show(QWidget *parent, BitTorrent::Session *session, const QStringList &ids)
{
    for (BitTorrent::Torrent *torrent : torrentsFor(session, ids))
    {
        if (!torrent->hasMetadata())
            continue;
        PreviewSelectDialog dialog(parent, torrent);
        if (dialog.rowCount() == 0)
        {
            RoMessageBox::info(parent, tr("Preview impossible")
                               , tr("None of the files in \"%1\" can be previewed.").arg(torrent->name()));
            continue;
        }
        if (dialog.rowCount() == 1)
            dialog.previewSelected();   // no choice to make
        else
            dialog.run();
    }
}

// ============================================================================================================================
//  Hooks
// ============================================================================================================================
void ManagementDialogs::install(MainWindow &window, BitTorrent::Session *session)
{
    MainWindow *w = &window;
    window.hooks().cookies = [w] { CookiesDialog::show(w); };
    window.hooks().torrentOptions = [w, session](const QStringList &ids) { TorrentOptionsDialog::show(w, session, ids); };
    window.hooks().editTrackers = [w, session](const QStringList &ids) { TrackerListDialog::show(w, session, ids); };
    window.hooks().previewFile = [w, session](const QStringList &ids) { PreviewSelectDialog::show(w, session, ids); };
}

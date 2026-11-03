// SPDX-License-Identifier: GPL-2.0-or-later
// Trackers, Peers and HTTP Sources tabs (handoff 8.9).
#include "DetailsTabs.h"

#include <algorithm>
#include <functional>

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMenu>
#include <QScrollBar>
#include <QStackedLayout>
#include <QVBoxLayout>

#include "DetailsDialogs.h"
#include "DetailsSource.h"
#include "DetailsWidgets.h"
#include "base/utils/misc.h"
#include "dialogs/RoInputDialog.h"
#include "dialogs/RoMessageBox.h"
#include "theme/RoTheme.h"
#include "widgets/MenuIcons.h"

namespace
{
    QAction *menuItem(QMenu *menu, const QString &text, const QString &icon, std::function<void()> slot, const bool enabled = true)
    {
        QAction *a = menu->addAction(text);
        RoMenu::bind(a, icon, false);
        a->setEnabled(enabled);
        QObject::connect(a, &QAction::triggered, menu, std::move(slot));
        return a;
    }

    void copyLines(const QStringList &lines)
    {
        if (!lines.isEmpty())
            QApplication::clipboard()->setText(lines.join(u'\n'));
    }

    QStringList selectedTexts(const QTableWidget *table, const int column, const int role = Qt::DisplayRole)
    {
        QStringList out;
        const QModelIndexList rows = table->selectionModel()->selectedRows();
        for (const QModelIndex &i : rows)
            out << table->item(i.row(), column)->data(role).toString();
        return out;
    }

    QString announceText(const qint64 secs)
    {
        return (secs < 0) ? QStringLiteral("—")
                          : Utils::Misc::userFriendlyDuration(secs, -1, Utils::Misc::TimeResolution::Seconds);
    }

    QString numberText(const int n)
    {
        return (n < 0) ? QStringLiteral("—") : QString::number(n);
    }

    // The tabs' own right-click handling: the table and the tab body (which holds the empty-state
    // text) both open the same menu, so "Add ..." works on an empty list too.
    template <typename Tab>
    void wireContextMenu(Tab *tab, QTableWidget *table)
    {
        const auto show = [tab](const QPoint &global)
        {
            QMenu *menu = tab->buildMenu();
            menu->setAttribute(Qt::WA_DeleteOnClose);
            menu->popup(global);
        };
        table->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(table, &QWidget::customContextMenuRequested, tab, [table, show](const QPoint &pos)
        {
            show(table->viewport()->mapToGlobal(pos));
        });
        tab->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(tab, &QWidget::customContextMenuRequested, tab, [tab, show](const QPoint &pos)
        {
            show(tab->mapToGlobal(pos));
        });
    }
}

// ------------------------------------------------------------------------------------ Trackers

TrackersTab::TrackersTab(DetailsSource *source, QWidget *parent)
    : DetailsTab(source, parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 0, 12, 0);
    lay->setSpacing(0);
    m_table = DetailsUi::makeTable({tr("URL / Announce Endpoint"), tr("Tier"), tr("BT Protocol"), tr("Status"), tr("Peers")
                                    , tr("Seeds"), tr("Leeches"), tr("Times Downloaded"), tr("Message"), tr("Next Announce")
                                    , tr("Min Announce")}
                                   , {330, 52, 96, 140, 62, 62, 70, 160, 110, 118, 110});
    lay->addWidget(m_table, 1);
    wireContextMenu(this, m_table);
}

void TrackersTab::refresh()
{
    const RoPalette &pal = RoTheme::instance().pal();
    const QList<Details::Tracker> rows = m_source->trackers(m_id);

    QSet<QString> selected;
    for (const QModelIndex &i : m_table->selectionModel()->selectedRows())
        selected.insert(m_table->item(i.row(), 0)->text());
    const int scroll = m_table->verticalScrollBar()->value();

    m_table->setUpdatesEnabled(false);
    m_table->clearSelection();
    m_table->setRowCount(static_cast<int>(rows.size()));
    for (int r = 0; r < rows.size(); ++r)
    {
        const Details::Tracker &t = rows.at(r);
        QString status;
        QColor colour;
        switch (t.state)
        {
        case Details::TrackerState::Working: status = tr("Working"); colour = pal.seed; break;
        case Details::TrackerState::Updating: status = tr("Updating..."); colour = pal.info; break;
        case Details::TrackerState::NotContacted: status = tr("Not contacted yet"); colour = pal.muted; break;
        case Details::TrackerState::NotWorking: status = tr("Not working"); colour = pal.accent; break;
        case Details::TrackerState::Disabled: status = tr("Disabled"); colour = pal.muted; break;
        }
        const Qt::Alignment right = Qt::AlignRight;
        m_table->setItem(r, 0, DetailsUi::cell(t.url, t.pseudo ? pal.muted : QColor()));
        m_table->setItem(r, 1, DetailsUi::cell(t.pseudo ? QString() : QString::number(t.tier)));
        m_table->setItem(r, 2, DetailsUi::cell(t.btProtocol));
        m_table->setItem(r, 3, DetailsUi::cell(status, colour));
        m_table->setItem(r, 4, DetailsUi::cell(numberText(t.peers), {}, right));
        m_table->setItem(r, 5, DetailsUi::cell(numberText(t.seeds), {}, right));
        m_table->setItem(r, 6, DetailsUi::cell(numberText(t.leeches), {}, right));
        m_table->setItem(r, 7, DetailsUi::cell(numberText(t.downloaded), {}, right));
        m_table->setItem(r, 8, DetailsUi::cell(t.message));
        m_table->setItem(r, 9, DetailsUi::cell(announceText(t.nextAnnounceSecs)));
        m_table->setItem(r, 10, DetailsUi::cell(announceText(t.minAnnounceSecs)));
        m_table->item(r, 0)->setData(Qt::UserRole, t.pseudo);
        if (!t.message.isEmpty())
            m_table->item(r, 8)->setToolTip(t.message);
        if (selected.contains(t.url))
            m_table->selectRow(r);
    }
    m_table->verticalScrollBar()->setValue(scroll);
    m_table->setUpdatesEnabled(true);
}

QStringList TrackersTab::selectedUrls() const
{
    QStringList out;
    for (const QModelIndex &i : m_table->selectionModel()->selectedRows())
    {
        const QTableWidgetItem *it = m_table->item(i.row(), 0);
        if (!it->data(Qt::UserRole).toBool())
            out << it->text();
    }
    return out;
}

void TrackersTab::addTrackersInteractive()
{
    if (const auto urls = DetailsDialogs::addTrackers(this))
    {
        m_source->addTrackers(m_id, *urls);
        refresh();
    }
}

void TrackersTab::editSelectedInteractive()
{
    const QStringList urls = selectedUrls();
    if (urls.size() != 1)
        return;
    if (const auto text = RoInputDialog::getText(this, tr("Edit tracker URL"), tr("Tracker URL:"), urls.first()))
    {
        if (!m_source->editTracker(m_id, urls.first(), *text))
            RoMessageBox::error(this, tr("Edit tracker URL"), tr("The tracker URL could not be changed."));
        refresh();
    }
}

void TrackersTab::removeSelected()
{
    const QStringList urls = selectedUrls();
    if (urls.isEmpty())
        return;
    m_source->removeTrackers(m_id, urls);
    refresh();
}

void TrackersTab::reannounceSelected()
{
    const QStringList urls = selectedUrls();
    if (!urls.isEmpty())
        m_source->reannounce(m_id, urls);
}

void TrackersTab::reannounceAll()
{
    m_source->reannounce(m_id, {});
}

void TrackersTab::copySelectedUrl()
{
    copyLines(selectedUrls());
}

QMenu *TrackersTab::buildMenu()
{
    auto *menu = new QMenu(this);
    const bool has = !selectedUrls().isEmpty();
    const bool one = selectedUrls().size() == 1;
    const bool any = !m_id.isEmpty();
    menuItem(menu, tr("Add trackers..."), QStringLiteral("plus"), [this] { addTrackersInteractive(); }, any);
    menuItem(menu, tr("Edit tracker URL..."), QStringLiteral("m-pencil"), [this] { editSelectedInteractive(); }, one);
    menuItem(menu, tr("Remove tracker"), QStringLiteral("delete"), [this] { removeSelected(); }, has);
    menu->addSeparator();
    menuItem(menu, tr("Copy tracker URL"), QStringLiteral("m-copy"), [this] { copySelectedUrl(); }, has);
    menu->addSeparator();
    menuItem(menu, tr("Force reannounce to selected trackers"), QStringLiteral("m-signal"), [this] { reannounceSelected(); }, has);
    menuItem(menu, tr("Force reannounce to all trackers"), QStringLiteral("m-signal"), [this] { reannounceAll(); }, any);
    return menu;
}

// ------------------------------------------------------------------------------------ Peers

PeersTab::PeersTab(DetailsSource *source, QWidget *parent)
    : DetailsTab(source, parent)
{
    m_stack = new QStackedLayout(this);
    m_stack->setContentsMargins(12, 0, 12, 0);
    m_table = DetailsUi::makeTable({tr("Country / Region"), tr("IP"), tr("Port"), tr("Connection"), tr("Flags"), tr("Client")
                                    , tr("Progress"), tr("Down Speed"), tr("Up Speed"), tr("Downloaded"), tr("Uploaded")
                                    , tr("Relevance")}
                                   , {120, 140, 70, 96, 100, 168, 140, 110, 100, 110, 100, 90});
    m_table->setItemDelegateForColumn(6, new BarDelegate(m_table));
    m_empty = DetailsUi::emptyLabel(tr("No connected peers. Peers appear here while the torrent is active."));
    m_empty->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_stack->addWidget(m_table);
    m_stack->addWidget(m_empty);
    m_stack->setCurrentWidget(m_empty);
    wireContextMenu(this, m_table);
    connect(m_source, &DetailsSource::peersReady, this, &PeersTab::onPeers);
}

bool PeersTab::showingEmptyState() const
{
    return m_stack->currentWidget() == m_empty;
}

void PeersTab::setTorrent(const QString &id)
{
    if (id != m_id)
    {
        m_table->setRowCount(0);
        m_stack->setCurrentWidget(m_empty);
    }
    DetailsTab::setTorrent(id);
}

void PeersTab::refresh()
{
    if (!m_id.isEmpty())
        m_source->requestPeers(m_id);
}

void PeersTab::onPeers(const QString &id, const QList<Details::Peer> &peersIn)
{
    if (id != m_id)
        return;
    const RoPalette &pal = RoTheme::instance().pal();
    QList<Details::Peer> peers = peersIn;
    std::stable_sort(peers.begin(), peers.end(), [](const Details::Peer &a, const Details::Peer &b)
    {
        return a.progress > b.progress;
    });

    if (peers.isEmpty())
    {
        m_table->setRowCount(0);
        m_stack->setCurrentWidget(m_empty);
        return;
    }

    QSet<QString> selected;
    for (const QModelIndex &i : m_table->selectionModel()->selectedRows())
        selected.insert(m_table->item(i.row(), 1)->data(Qt::UserRole + 1).toString());
    const int scroll = m_table->verticalScrollBar()->value();

    m_table->setUpdatesEnabled(false);
    m_table->clearSelection();
    m_table->setRowCount(static_cast<int>(peers.size()));
    for (int r = 0; r < peers.size(); ++r)
    {
        const Details::Peer &p = peers.at(r);
        const Qt::Alignment right = Qt::AlignRight;
        auto *country = DetailsUi::cell(p.country);
        country->setToolTip(p.countryCode);
        auto *flags = DetailsUi::cell(p.flags, pal.muted);
        flags->setToolTip(p.flagsDescription);
        auto *progress = new QTableWidgetItem;
        progress->setData(Qt::UserRole, p.progress * 100.0);
        m_table->setItem(r, 0, country);
        m_table->setItem(r, 1, DetailsUi::cell(p.ip));
        m_table->item(r, 1)->setData(Qt::UserRole + 1, p.endpoint());
        m_table->setItem(r, 2, DetailsUi::cell(QString::number(p.port), {}, right));
        m_table->setItem(r, 3, DetailsUi::cell(p.connection));
        m_table->setItem(r, 4, flags);
        m_table->setItem(r, 5, DetailsUi::cell(p.client));
        m_table->setItem(r, 6, progress);
        m_table->setItem(r, 7, DetailsUi::cell(DetailsFormat::speed(p.downSpeed), {}, right));
        m_table->setItem(r, 8, DetailsUi::cell(DetailsFormat::speed(p.upSpeed), {}, right));
        m_table->setItem(r, 9, DetailsUi::cell(DetailsFormat::bytes(p.downloaded), {}, right));
        m_table->setItem(r, 10, DetailsUi::cell(DetailsFormat::bytes(p.uploaded), {}, right));
        m_table->setItem(r, 11, DetailsUi::cell(QString::number(p.relevance, 'f', 2), {}, right));
        if (selected.contains(p.endpoint()))
            m_table->selectRow(r);
    }
    m_table->verticalScrollBar()->setValue(scroll);
    m_table->setUpdatesEnabled(true);
    m_stack->setCurrentWidget(m_table);
}

QStringList PeersTab::selectedEndpoints() const
{
    return selectedTexts(m_table, 1, Qt::UserRole + 1);
}

void PeersTab::addPeersInteractive()
{
    const auto lines = DetailsDialogs::addPeers(this);
    if (!lines)
        return;
    const int accepted = m_source->addPeers(m_id, *lines);
    if (accepted < lines->size())
    {
        RoMessageBox::error(this, tr("Add peers"), tr("%1 of %2 peers could not be added. Check the address format "
                                                       "(IPv4:port or [IPv6]:port) and that the torrent is active.")
                            .arg(lines->size() - accepted).arg(lines->size()));
    }
    refresh();
}

void PeersTab::banSelectedInteractive(const bool confirm)
{
    const QStringList endpoints = selectedEndpoints();
    if (endpoints.isEmpty())
        return;
    if (confirm && !RoMessageBox::confirm(this, tr("Ban peer"), tr("Are you sure you want to permanently ban the selected peers?")
                                          , tr("Ban")))
        return;
    m_source->banPeers(endpoints);
    refresh();
}

void PeersTab::copySelectedEndpoint()
{
    copyLines(selectedEndpoints());
}

QMenu *PeersTab::buildMenu()
{
    auto *menu = new QMenu(this);
    const bool has = !selectedEndpoints().isEmpty();
    menuItem(menu, tr("Add peers..."), QStringLiteral("plus"), [this] { addPeersInteractive(); }, !m_id.isEmpty());
    menuItem(menu, tr("Copy IP:port"), QStringLiteral("m-copy"), [this] { copySelectedEndpoint(); }, has);
    menu->addSeparator();
    menuItem(menu, tr("Ban peer permanently"), QStringLiteral("m-ban"), [this] { banSelectedInteractive(); }, has);
    return menu;
}

// ------------------------------------------------------------------------------------ HTTP Sources

HttpTab::HttpTab(DetailsSource *source, QWidget *parent)
    : DetailsTab(source, parent)
{
    m_stack = new QStackedLayout(this);
    m_stack->setContentsMargins(12, 0, 12, 0);
    m_table = DetailsUi::makeTable({tr("URL")}, {600});
    m_empty = DetailsUi::emptyLabel(tr("No HTTP sources. Add a web seed with right-click > Add HTTP source..."));
    m_empty->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_stack->addWidget(m_table);
    m_stack->addWidget(m_empty);
    m_stack->setCurrentWidget(m_empty);
    wireContextMenu(this, m_table);
}

bool HttpTab::showingEmptyState() const
{
    return m_stack->currentWidget() == m_empty;
}

void HttpTab::refresh()
{
    const QList<Details::WebSeed> seeds = m_source->webSeeds(m_id);
    QSet<QString> selected;
    for (const QModelIndex &i : m_table->selectionModel()->selectedRows())
        selected.insert(m_table->item(i.row(), 0)->text());

    m_table->clearSelection();
    m_table->setRowCount(static_cast<int>(seeds.size()));
    for (int r = 0; r < seeds.size(); ++r)
    {
        m_table->setItem(r, 0, DetailsUi::cell(seeds.at(r).url));
        if (selected.contains(seeds.at(r).url))
            m_table->selectRow(r);
    }
    m_stack->setCurrentWidget(seeds.isEmpty() ? static_cast<QWidget *>(m_empty) : static_cast<QWidget *>(m_table));
}

QStringList HttpTab::selectedUrls() const
{
    return selectedTexts(m_table, 0);
}

void HttpTab::addInteractive()
{
    if (const auto urls = DetailsDialogs::addHttpSources(this))
    {
        m_source->addWebSeeds(m_id, *urls);
        refresh();
    }
}

void HttpTab::removeSelected()
{
    const QStringList urls = selectedUrls();
    if (urls.isEmpty())
        return;
    m_source->removeWebSeeds(m_id, urls);
    refresh();
}

void HttpTab::copySelected()
{
    copyLines(selectedUrls());
}

QMenu *HttpTab::buildMenu()
{
    auto *menu = new QMenu(this);
    const bool has = !selectedUrls().isEmpty();
    menuItem(menu, tr("Add HTTP source..."), QStringLiteral("plus"), [this] { addInteractive(); }, !m_id.isEmpty());
    menuItem(menu, tr("Copy HTTP source URL"), QStringLiteral("m-copy"), [this] { copySelected(); }, has);
    menuItem(menu, tr("Remove HTTP source"), QStringLiteral("delete"), [this] { removeSelected(); }, has);
    return menu;
}

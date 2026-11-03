// SPDX-License-Identifier: GPL-2.0-or-later
// MainWindow: torrent commands, the torrent context menu (handoff 8.8) and the sidebar menus (8.6).
// Enable / visibility rules come from ActionRules (qBittorrent's own); wording is Resume / Pause.
#include "MainWindow.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QMenu>

#include "actions/TorrentController.h"
#include "base/bittorrent/session.h"
#include "base/preferences.h"
#include "base/settingsstorage.h"
#include "GuiSettings.h"
#include "dialogs/RoInputDialog.h"
#include "dialogs/RoMessageBox.h"
#include "models/TransferListModel.h"
#include "widgets/MenuBuilder.h"
#include "widgets/Sidebar.h"
#include "widgets/TransferListView.h"

namespace
{
    const QString REMOVE_CONTENT_KEY = QStringLiteral("Gui/RemoveContentDefault");
}

void MainWindow::activateTorrent(const QString &id)
{
    if (!m_controller || id.isEmpty())
        return;

    const TorrentFacts facts = m_controller->facts(id);
    const auto *pref = Preferences::instance();
    const int action = facts.finished ? pref->getActionOnDblClOnTorrentFn() : pref->getActionOnDblClOnTorrentDl();
    const QStringList ids {id};
    switch (action)
    {
    case TOGGLE_STOP:   // upstream's stored name; it reads "Pause / resume torrent" in Options
        if (facts.stopped)
            m_controller->resume(ids);
        else
            m_controller->pause(ids);
        break;
    case OPEN_DEST:
        m_controller->openDestination(ids);
        break;
    case PREVIEW_FILE:
        if (m_hooks.previewFile)
            m_hooks.previewFile(ids);
        break;
    case SHOW_OPTIONS:
        if (m_hooks.torrentOptions)
            m_hooks.torrentOptions(ids);
        break;
    default:   // NO_ACTION
        break;
    }
}

void MainWindow::resumeSelected()
{
    if (m_controller)
        m_controller->resume(selectedIds());
}

void MainWindow::pauseSelected()
{
    if (m_controller)
        m_controller->pause(selectedIds());
}

void MainWindow::forceResumeSelected()
{
    if (m_controller)
        m_controller->forceResume(selectedIds());
}

void MainWindow::moveSelectedInQueue(const int where)
{
    if (m_controller)
        m_controller->moveInQueue(selectedIds(), static_cast<TorrentController::QueueMove>(where));
}

QStringList MainWindow::idsMatching(const RowFilter &filter) const
{
    QStringList ids;
    for (int i = 0; i < m_model->rowCount(); ++i)
        if (filter.matches(m_model->row(i)))
            ids.append(m_model->row(i).id);
    return ids;
}

void MainWindow::removeIds(const QStringList &ids, const bool askConfirmation)
{
    if (!m_controller || ids.isEmpty())
        return;
    const bool rememberedContent = SettingsStorage::instance()->loadValue<bool>(REMOVE_CONTENT_KEY, false);
    if (askConfirmation && Preferences::instance()->confirmTorrentDeletion())
    {
        const QString text = (ids.size() == 1)
            ? tr("Are you sure you want to remove \u201c%1\u201d from the transfer list?")
                  .arg(m_model->row(m_model->indexOfId(ids.first())).name)
            : tr("Are you sure you want to remove these %1 torrents from the transfer list?").arg(ids.size());
        RoMessageBox::Options o;
        o.kind = RoMessageBox::Kind::Warning;
        o.buttons = {tr("Remove"), tr("Cancel")};   // primary first, as in the kit
        o.width = 465;
        o.primary = tr("Remove");
        o.defaultButton = tr("Cancel");
        o.checkText = tr("Also permanently delete the files");
        o.checkInitial = rememberedContent;
        const auto result = RoMessageBox::show(this, tr("Remove torrent"), text, o);
        if (result.label != tr("Remove"))
            return;
        SettingsStorage::instance()->storeValue(REMOVE_CONTENT_KEY, result.checked);   // remember the choice
        m_controller->remove(ids, result.checked);
        return;
    }
    m_controller->remove(ids, rememberedContent);
}

void MainWindow::renameTorrent(const QString &id)
{
    const int row = m_model->indexOfId(id);
    if (!m_controller || row < 0)
        return;
    const auto name = RoInputDialog::getText(this, tr("Rename torrent"), tr("New name:"), m_model->row(row).name);
    if (name && !name->isEmpty() && (*name != m_model->row(row).name))
        m_controller->rename(id, *name);
}

void MainWindow::setLocation(const QStringList &ids)
{
    if (!m_controller || ids.isEmpty())
        return;
    const QString start = m_controller->destinationFolder(ids.first());
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), start);
    if (!dir.isEmpty())
        m_controller->setLocation(ids, dir);
}

void MainWindow::askNewCategory(const QStringList &ids)
{
    if (!m_controller)
        return;
    const auto input = RoInputDialog::getCategory(this, tr("New category"), {}, true);
    if (!input)
        return;
    if (!BitTorrent::Session::isValidCategoryName(input->name))
    {
        RoMessageBox::error(this, tr("Invalid category name"), tr("The category name \u201c%1\u201d is not valid.").arg(input->name));
        return;
    }
    if (!m_controller->categories().contains(input->name))
        m_controller->addCategory(input->name, input->savePath);
    m_controller->setCategory(ids, input->name);
}

void MainWindow::addCategory()
{
    if (!m_controller)
        return;
    const auto input = RoInputDialog::getCategory(this, tr("New category"), {}, true);
    if (!input)
        return;
    if (!BitTorrent::Session::isValidCategoryName(input->name))
        RoMessageBox::error(this, tr("Invalid category name"), tr("The category name \u201c%1\u201d is not valid.").arg(input->name));
    else if (!m_controller->addCategory(input->name, input->savePath))
        RoMessageBox::error(this, tr("Category exists"), tr("The category \u201c%1\u201d already exists.").arg(input->name));
}

void MainWindow::editCategory(const QString &name)
{
    if (!m_controller)
        return;
    const auto input = RoInputDialog::getCategory(this, tr("Edit category"), {name, m_controller->categorySavePath(name)}, false);
    if (input)
        m_controller->editCategory(name, input->savePath);
}

void MainWindow::exportTorrent(const QString &id)
{
    if (!m_controller)
        return;
    const QString path = QFileDialog::getSaveFileName(this, tr("Export .torrent"), m_controller->suggestedExportName(id)
                                                      , tr("Torrent Files (*.torrent)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_controller->exportTorrent(id, path, &error))
        RoMessageBox::error(this, tr("Export .torrent"), tr("Could not export the .torrent file: %1").arg(error));
}

void MainWindow::showTorrentContextMenu(const QPoint &viewportPos)
{
    const QModelIndex index = m_view->indexAt(viewportPos);
    if (!index.isValid())
        return;
    m_view->selectRow(index.row());
    if (QMenu *menu = buildTorrentMenu(selectedIds()))
        menu->popup(m_view->viewport()->mapToGlobal(viewportPos));
}

QMenu *MainWindow::buildTorrentMenu(const QStringList &ids)
{
    if (!m_controller || ids.isEmpty())
        return nullptr;
    TorrentController *c = m_controller;
    const CommandPlan plan = c->planFor(ids);
    if (!plan.any)
        return nullptr;

    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    MenuBuilder m(menu, true);
    const QString first = ids.first();

    if (plan.resume)
        m.add(tr("Resume"), QStringLiteral("resume"), [=] { c->resume(ids); });
    if (plan.pause)
        m.add(tr("Pause"), QStringLiteral("pause"), [=] { c->pause(ids); });
    if (plan.forceResume)
        m.add(tr("Force Resume"), QStringLiteral("m-bolt"), [=] { c->forceResume(ids); });
    m.separator();
    m.add(tr("Remove"), QStringLiteral("delete"), [=] { removeIds(ids); });
    m.separator();
    m.add(tr("Set location..."), QStringLiteral("m-folder-move"), [=] { setLocation(ids); });
    if (plan.single)
    {
        m.add(tr("Rename..."), QStringLiteral("m-pencil"), [=] { renameTorrent(first); });
        m.add(tr("Manage content..."), QStringLiteral("m-files"), [=] { m_hooks.manageContent(ids); });
    }
    m.add(tr("Edit trackers..."), QStringLiteral("m-pencil"), [=] { m_hooks.editTrackers(ids); });

    MenuBuilder category = m.sub(tr("Category"), QStringLiteral("folder"));
    category.add(tr("New..."), QStringLiteral("plus"), [=] { askNewCategory(ids); });
    category.add(tr("Reset"), QStringLiteral("m-ban"), [=] { c->setCategory(ids, QString()); });
    category.separator();
    for (const QString &name : c->categories())
    {
        const bool current = plan.allSameCategory && (name == plan.commonCategory);
        category.addCheck(name, QStringLiteral("folder"), current, [=](const bool on)
        {
            if (on)
                c->setCategory(ids, name);
        });
    }

    m.addCheck(tr("Automatic Torrent Management"), QStringLiteral("m-auto"), plan.autoTMM == Qt::Checked, [=](const bool on)
    {
        if (on && !RoMessageBox::confirm(this, tr("Automatic Torrent Management")
                                         , tr("Are you sure you want to enable Automatic Torrent Management for the selected torrent(s)? They may be relocated.")
                                         , tr("Enable")))
            return;
        c->setAutoTMM(ids, on);
    }, true, tr("Automatic mode means that various torrent properties (e.g. save path) will be decided by the associated category"));
    m.separator();
    m.add(tr("Torrent options..."), QStringLiteral("settings"), [=] { m_hooks.torrentOptions(ids); });
    if (plan.showSuperSeeding)
        m.addCheck(tr("Super seeding mode"), QStringLiteral("m-seed"), plan.superSeeding == Qt::Checked
                   , [=](const bool on) { c->setSuperSeeding(ids, on); });
    m.separator();
    bool addedGroup = false;
    if (plan.anyMetadata)
    {
        m.add(tr("Preview file..."), QStringLiteral("m-eye"), [=] { m_hooks.previewFile(ids); });
        addedGroup = true;
    }
    if (plan.anyUnfinished)
    {
        m.addCheck(tr("Download in sequential order"), QStringLiteral("m-queue"), plan.sequential == Qt::Checked
                   , [=](const bool on) { c->setSequential(ids, on); });
        m.addCheck(tr("Download first and last pieces first"), QStringLiteral("m-pieces"), plan.firstLast == Qt::Checked
                   , [=](const bool on) { c->setFirstLast(ids, on); });
        addedGroup = true;
    }
    if (addedGroup)
        m.separator();
    if (plan.anyMetadata)
        m.add(tr("Force recheck"), QStringLiteral("m-recheck"), [=]
        {
            if (Preferences::instance()->confirmTorrentRecheck()
                && !RoMessageBox::confirm(this, tr("Recheck confirmation")
                                          , tr("Are you sure you want to recheck the selected torrent(s)?"), tr("Recheck")))
                return;
            c->recheck(ids);
        });
    m.add(tr("Force reannounce"), QStringLiteral("m-signal"), [=] { c->reannounce(ids); }, plan.canReannounce
          , plan.canReannounce ? QString() : tr("Can not force reannounce if torrent is Paused/Queued/Errored/Checking"));
    m.separator();
    m.add(tr("Open destination folder"), QStringLiteral("folder"), [=] { c->openDestination(ids); });
    if (ActionRules::queueCommandsEnabled(plan, m_queueing))
    {
        m.separator();
        MenuBuilder queue = m.sub(tr("Queue"), QStringLiteral("m-queue"));
        queue.add(tr("Move to top"), QStringLiteral("q-top"), [=] { c->moveInQueue(ids, TorrentController::QueueMove::Top); });
        queue.add(tr("Move up"), QStringLiteral("q-up"), [=] { c->moveInQueue(ids, TorrentController::QueueMove::Up); });
        queue.add(tr("Move down"), QStringLiteral("q-down"), [=] { c->moveInQueue(ids, TorrentController::QueueMove::Down); });
        queue.add(tr("Move to bottom"), QStringLiteral("q-bottom"), [=] { c->moveInQueue(ids, TorrentController::QueueMove::Bottom); });
    }
    MenuBuilder copy = m.sub(tr("Copy"), QStringLiteral("m-copy"));
    const auto copyItem = [&](const QString &text, const QString &icon, const TorrentController::CopyKind kind, const bool enabled = true)
    {
        copy.add(text, icon, [=] { QApplication::clipboard()->setText(c->copyText(ids, kind)); }, enabled);
    };
    using K = TorrentController::CopyKind;
    copyItem(tr("Name"), QStringLiteral("m-tag"), K::Name);
    copyItem(tr("Info hash v1"), QStringLiteral("m-hash"), K::InfoHashV1, plan.copyInfoHashV1);
    copyItem(tr("Info hash v2"), QStringLiteral("m-hash"), K::InfoHashV2, plan.copyInfoHashV2);
    copyItem(tr("Magnet link"), QStringLiteral("m-magnet"), K::MagnetLink);
    copyItem(tr("Torrent ID"), QStringLiteral("m-id"), K::TorrentId);
    copyItem(tr("Comment"), QStringLiteral("m-note"), K::Comment);
    copyItem(tr("Content Path"), QStringLiteral("folder"), K::ContentPath);
    m.add(tr("Export .torrent..."), QStringLiteral("m-export"), [=] { exportTorrent(first); }, plan.single
          , plan.single ? tr("Exported torrent is not necessarily the same as the imported") : QString());
    return menu;
}

QMenu *MainWindow::buildSidebarMenu(const QString &group, const QVariant &payload)
{
    if (!m_controller)
        return nullptr;
    TorrentController *c = m_controller;
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    MenuBuilder m(menu, false);

    RowFilter filter;
    bool hasFilter = false;
    if (group == u"status")
    {
        filter.status = static_cast<RowFilter::StatusKind>(payload.toInt());
        hasFilter = true;
    }
    else if (group == u"tracker")
    {
        filter.tracker = payload.toString();
        hasFilter = true;
    }
    else if (group == u"category")
    {
        filter.category = payload.toString();
        hasFilter = true;
    }

    if (group == u"tracker")
    {
        // Copy the full announce URL(s), not just the host the row shows. Falls back to the host if the
        // engine reports no matching URL (for example a tracker removed since the row was drawn).
        const QString host = payload.toString();
        const QStringList hostIds = idsMatching(filter);
        m.add(tr("Copy tracker URL"), QStringLiteral("m-copy"), [=]
        {
            const QString urls = c->trackerUrlsForHost(hostIds, host);
            QApplication::clipboard()->setText(urls.isEmpty() ? host : urls);
        }, !host.isEmpty());
        m.separator();
    }
    if (group == u"category" || group == u"categoryHeader" || group == u"empty")
    {
        const QString name = payload.toString();
        const bool real = (group == u"category") && !name.isEmpty();   // "Uncategorized" cannot be edited
        m.add(tr("Add category..."), QStringLiteral("plus"), [=] { addCategory(); });
        if (group == u"category")
        {
            m.add(tr("Edit category..."), QStringLiteral("m-pencil"), [=] { editCategory(name); }, real);
            m.add(tr("Remove category"), QStringLiteral("delete"), [=] { c->removeCategory(name); }, real);
        }
        m.add(tr("Remove unused categories"), QStringLiteral("m-ban"), [=] { c->removeUnusedCategories(); });
        if (hasFilter)
            m.separator();
    }
    if (hasFilter)
    {
        const QStringList ids = idsMatching(filter);
        m.add(tr("Resume torrents"), QStringLiteral("resume"), [=] { c->resume(ids); }, !ids.isEmpty());
        m.add(tr("Pause torrents"), QStringLiteral("pause"), [=] { c->pause(ids); }, !ids.isEmpty());
        m.add(tr("Remove torrents"), QStringLiteral("delete"), [=] { removeIds(ids); }, !ids.isEmpty());
    }
    if (menu->isEmpty())
    {
        delete menu;
        return nullptr;
    }
    return menu;
}

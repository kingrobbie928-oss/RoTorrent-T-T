// SPDX-License-Identifier: GPL-2.0-or-later
#include "DetailsTabs.h"

#include <functional>

#include <QActionGroup>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "DetailsSource.h"
#include "DetailsWidgets.h"
#include "dialogs/RoInputDialog.h"
#include "dialogs/RoMessageBox.h"
#include "theme/RoTheme.h"
#include "widgets/MenuIcons.h"
#include "widgets/RoSearchBox.h"

namespace
{
    enum Role { FileIndexRole = Qt::UserRole + 1, PathRole, PriorityRole, SizeRole, DoneRole, WantedRole, WantedDoneRole
                , AvailSumRole, AvailWeightRole };

    QString priorityText(const int p)
    {
        switch (p)
        {
        case Details::Skip: return QObject::tr("Do not download");
        case Details::Normal: return QObject::tr("Normal");
        case Details::High: return QObject::tr("High");
        case Details::Maximum: return QObject::tr("Maximum");
        default: return QObject::tr("Mixed");
        }
    }

    bool isFolder(const QTreeWidgetItem *item) { return item->data(0, FileIndexRole).toInt() < 0; }
}

ContentTab::ContentTab(DetailsSource *source, QWidget *parent)
    : DetailsTab(source, parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 0, 12, 0);
    lay->setSpacing(0);

    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(0, 10, 0, 10);
    bar->setSpacing(8);
    m_filter = new RoSearchBox;
    m_filter->setPlaceholderText(tr("Filter files..."));
    m_filter->setFixedHeight(30);
    m_filter->setMaximumWidth(280);
    bar->addWidget(m_filter);
    bar->addStretch(1);
    for (const auto &[text, slot] : {std::pair<QString, void (ContentTab::*)()> {tr("Select All"), &ContentTab::selectAll}
                                     , std::pair<QString, void (ContentTab::*)()> {tr("Select None"), &ContentTab::selectNone}})
    {
        auto *b = new QPushButton(text);
        b->setObjectName(QStringLiteral("dBtn"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(28);
        connect(b, &QPushButton::clicked, this, slot);
        bar->addWidget(b);
    }
    lay->addLayout(bar);

    m_tree = new QTreeWidget;
    m_tree->setObjectName(QStringLiteral("dTable"));
    m_tree->setColumnCount(6);
    QStringList headers;
    for (const QString &h : {tr("Name"), tr("Total Size"), tr("Progress"), tr("Download Priority"), tr("Remaining"), tr("Availability")})
        headers << h.toUpper();
    m_tree->setHeaderLabels(headers);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(18);
    m_tree->setUniformRowHeights(true);
    m_tree->setMouseTracking(true);
    m_tree->setFocusPolicy(Qt::NoFocus);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    QHeaderView *h = m_tree->header();
    h->setStretchLastSection(false);
    h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    h->setMinimumSectionSize(60);
    const int widths[] = {330, 100, 150, 150, 110, 100};
    for (int i = 0; i < 6; ++i)
        m_tree->setColumnWidth(i, widths[i]);
    h->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->setItemDelegateForColumn(2, new BarDelegate(m_tree));
    lay->addWidget(m_tree, 1);

    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos)
    {
        if (!m_tree->itemAt(pos))
            return;
        QMenu *menu = buildMenu();
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(m_tree->viewport()->mapToGlobal(pos));
    });
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, const int column)
    {
        if (column == 0)
            onItemChanged(item);
    });
    connect(m_filter, &RoSearchBox::searchChanged, this, &ContentTab::applyFilter);
    connect(m_source, &DetailsSource::availabilityReady, this
            , [this](const QString &id, const QList<int> &, const QList<double> &f) { onAvailability(id, f); });
    connect(&RoTheme::instance(), &RoTheme::changed, this, [this]
    {
        const RoPalette &pal = RoTheme::instance().pal();
        for (auto *it : std::as_const(m_folders))
            it->setIcon(0, RoTheme::instance().icon(QStringLiteral("folder"), pal.muted, 15));
        for (auto *it : std::as_const(m_fileItems))
            it->setIcon(0, RoTheme::instance().icon(QStringLiteral("file"), pal.muted, 15));
    });
}

void ContentTab::setTorrent(const QString &id)
{
    if (id != m_id)
    {
        m_signature.clear();
        m_availability.clear();
        m_tree->clear();
        m_fileItems.clear();
        m_folders.clear();
        m_filter->clear();
    }
    m_id = id;
    refresh();
    m_source->requestAvailability(id);
}

void ContentTab::refresh()
{
    const QList<Details::File> files = m_source->files(m_id);
    QStringList paths;
    for (const Details::File &f : files)
        paths << f.path;
    const QString signature = paths.join(u'\n');
    if (signature != m_signature)
    {
        m_signature = signature;
        rebuild(files);
    }
    updateValues(files);
}

void ContentTab::rebuild(const QList<Details::File> &files)
{
    const RoPalette &pal = RoTheme::instance().pal();
    m_updating = true;
    m_tree->clear();
    m_fileItems.clear();
    m_folders.clear();

    const QIcon folderIcon = RoTheme::instance().icon(QStringLiteral("folder"), pal.muted, 15);
    const QIcon fileIcon = RoTheme::instance().icon(QStringLiteral("file"), pal.muted, 15);
    QHash<QString, QTreeWidgetItem *> folders;
    for (const Details::File &f : files)
    {
        const QStringList parts = f.path.split(u'/');
        QTreeWidgetItem *parent = nullptr;
        QString key;
        for (int k = 0; k < parts.size() - 1; ++k)
        {
            key += (k ? QStringLiteral("/") : QString()) + parts.at(k);
            QTreeWidgetItem *&folder = folders[key];
            if (!folder)
            {
                folder = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
                folder->setText(0, parts.at(k));
                folder->setIcon(0, folderIcon);
                folder->setFlags(folder->flags() | Qt::ItemIsUserCheckable);
                folder->setData(0, FileIndexRole, -1);
                folder->setData(0, PathRole, key);
                for (const int c : {1, 4, 5})
                    folder->setTextAlignment(c, Qt::AlignRight | Qt::AlignVCenter);
                m_folders << folder;
            }
            parent = folder;
        }
        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
        item->setText(0, parts.last());
        item->setIcon(0, fileIcon);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setData(0, FileIndexRole, f.index);
        item->setData(0, PathRole, f.path);
        for (const int c : {1, 4, 5})
            item->setTextAlignment(c, Qt::AlignRight | Qt::AlignVCenter);
        m_fileItems.insert(f.index, item);
    }
    m_tree->expandAll();
    m_updating = false;
    if (!m_filter->text().isEmpty())
        applyFilter(m_filter->text());
}

void ContentTab::updateValues(const QList<Details::File> &files)
{
    m_updating = true;
    for (const Details::File &f : files)
    {
        QTreeWidgetItem *it = m_fileItems.value(f.index);
        if (!it)
            continue;
        const bool wanted = (f.priority != Details::Skip);
        const double avail = (f.index < m_availability.size()) ? m_availability.at(f.index) : f.availability;
        it->setData(0, PriorityRole, static_cast<int>(f.priority));
        it->setData(0, SizeRole, f.size);
        it->setData(0, DoneRole, f.size * f.progress);
        it->setData(0, WantedRole, wanted ? f.size : 0);
        it->setData(0, WantedDoneRole, wanted ? f.size * f.progress : 0.0);
        it->setData(0, AvailSumRole, (avail >= 0 && wanted) ? avail * f.size : 0.0);
        it->setData(0, AvailWeightRole, (avail >= 0 && wanted) ? static_cast<double>(f.size) : 0.0);
        it->setText(1, DetailsFormat::bytes(f.size));
        it->setData(2, Qt::UserRole, f.progress * 100.0);
        it->setText(3, priorityText(f.priority));
        it->setText(4, DetailsFormat::bytes(wanted ? static_cast<qint64>(f.size * (1.0 - f.progress)) : 0));
        it->setText(5, (avail >= 0 && wanted && f.progress < 1.0) ? QString::number(avail, 'f', 2) : QStringLiteral("—"));
        it->setCheckState(0, wanted ? Qt::Checked : Qt::Unchecked);
    }
    recalcFolders();
    m_updating = false;
}

void ContentTab::recalcFolders()
{
    // m_folders is in creation order (parents before children): walk it backwards so children are done first.
    for (int i = static_cast<int>(m_folders.size()) - 1; i >= 0; --i)
    {
        QTreeWidgetItem *f = m_folders.at(i);
        qint64 size = 0, wanted = 0;
        double done = 0, wantedDone = 0, availSum = 0, availWeight = 0;
        int common = -2;
        bool anySkip = false, anyWanted = false;
        for (int c = 0; c < f->childCount(); ++c)
        {
            const QTreeWidgetItem *ch = f->child(c);
            size += ch->data(0, SizeRole).toLongLong();
            done += ch->data(0, DoneRole).toDouble();
            wanted += ch->data(0, WantedRole).toLongLong();
            wantedDone += ch->data(0, WantedDoneRole).toDouble();
            availSum += ch->data(0, AvailSumRole).toDouble();
            availWeight += ch->data(0, AvailWeightRole).toDouble();
            const int p = ch->data(0, PriorityRole).toInt();
            common = (common == -2) ? p : ((common == p) ? common : Details::Mixed);
            anySkip |= (ch->checkState(0) != Qt::Checked);
            anyWanted |= (ch->checkState(0) != Qt::Unchecked);
        }
        f->setData(0, SizeRole, size);
        f->setData(0, DoneRole, done);
        f->setData(0, WantedRole, wanted);
        f->setData(0, WantedDoneRole, wantedDone);
        f->setData(0, AvailSumRole, availSum);
        f->setData(0, AvailWeightRole, availWeight);
        f->setData(0, PriorityRole, common == -2 ? static_cast<int>(Details::Normal) : common);
        f->setText(1, DetailsFormat::bytes(size));
        const double pct = (wanted > 0) ? wantedDone / wanted : (size > 0 ? done / size : 0.0);
        f->setData(2, Qt::UserRole, pct * 100.0);
        f->setText(3, priorityText(common == -2 ? static_cast<int>(Details::Normal) : common));
        f->setText(4, DetailsFormat::bytes(static_cast<qint64>(wanted - wantedDone)));
        f->setText(5, (availWeight > 0 && pct < 1.0) ? QString::number(availSum / availWeight, 'f', 2) : QStringLiteral("—"));
        f->setCheckState(0, !anyWanted ? Qt::Unchecked : (anySkip ? Qt::PartiallyChecked : Qt::Checked));
    }
}

void ContentTab::onAvailability(const QString &id, const QList<double> &fractions)
{
    if (id != m_id)
        return;
    m_availability = fractions;
    refresh();
}

QList<int> ContentTab::fileIndexesUnder(const QTreeWidgetItem *item)
{
    QList<int> out;
    if (!isFolder(item))
    {
        out << item->data(0, FileIndexRole).toInt();
        return out;
    }
    for (int i = 0; i < item->childCount(); ++i)
        out << fileIndexesUnder(item->child(i));
    return out;
}

QString ContentTab::pathOf(const QTreeWidgetItem *item) const
{
    return item->data(0, PathRole).toString();
}

void ContentTab::onItemChanged(QTreeWidgetItem *item)
{
    if (m_updating)
        return;
    const Qt::CheckState s = item->checkState(0);
    if (s == Qt::PartiallyChecked)
        return;
    m_source->setFilePriority(m_id, fileIndexesUnder(item), (s == Qt::Checked) ? Details::Normal : Details::Skip);
    refresh();
}

void ContentTab::selectAll()
{
    QList<int> all = m_fileItems.keys();
    m_source->setFilePriority(m_id, all, Details::Normal);
    refresh();
}

void ContentTab::selectNone()
{
    m_source->setFilePriority(m_id, m_fileItems.keys(), Details::Skip);
    refresh();
}

void ContentTab::setPriorityOfSelection(const Details::Priority priority)
{
    QList<int> indexes;
    for (const QTreeWidgetItem *it : m_tree->selectedItems())
        indexes << fileIndexesUnder(it);
    if (indexes.isEmpty())
        return;
    m_source->setFilePriority(m_id, indexes, priority);
    refresh();
}

void ContentTab::applyFilter(const QString &text)
{
    const QString needle = text.trimmed();
    std::function<bool(QTreeWidgetItem *, bool)> visit = [&](QTreeWidgetItem *it, const bool forced) -> bool
    {
        const bool self = needle.isEmpty() || it->text(0).contains(needle, Qt::CaseInsensitive);
        bool any = false;
        for (int i = 0; i < it->childCount(); ++i)
            any |= visit(it->child(i), forced || self);
        const bool show = forced || self || any;
        it->setHidden(!show);
        return show;
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        visit(m_tree->topLevelItem(i), false);
}

void ContentTab::renameSelectedInteractive()
{
    const QList<QTreeWidgetItem *> sel = m_tree->selectedItems();
    if (sel.size() != 1)
        return;
    const QTreeWidgetItem *item = sel.first();
    const QString oldPath = pathOf(item);
    if (const auto name = RoInputDialog::getText(this, tr("Rename"), tr("New name:"), item->text(0)))
    {
        if (name->isEmpty() || name->contains(u'/') || name->contains(u'\\'))
        {
            RoMessageBox::error(this, tr("Rename"), tr("The name is empty or contains a path separator."));
            return;
        }
        const qsizetype slash = oldPath.lastIndexOf(u'/');
        const QString newPath = (slash < 0) ? *name : oldPath.left(slash + 1) + *name;
        QString error;
        if (!m_source->renameFile(m_id, oldPath, newPath, isFolder(item), &error))
            RoMessageBox::error(this, tr("Rename"), error.isEmpty() ? tr("The item could not be renamed.") : error);
        refresh();
    }
}

void ContentTab::openSelected()
{
    const QList<QTreeWidgetItem *> sel = m_tree->selectedItems();
    if (sel.isEmpty())
        return;
    const QString path = m_source->absolutePath(m_id, pathOf(sel.first()));
    if (path.isEmpty() || !QFileInfo::exists(path))
    {
        RoMessageBox::info(this, tr("Open"), tr("This file does not exist on disk yet."));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void ContentTab::openContainingFolder()
{
    const QList<QTreeWidgetItem *> sel = m_tree->selectedItems();
    if (sel.isEmpty())
        return;
    const QString path = m_source->absolutePath(m_id, pathOf(sel.first()));
    const QString folder = isFolder(sel.first()) ? path : QFileInfo(path).absolutePath();
    if (path.isEmpty() || !QFileInfo::exists(folder))
    {
        RoMessageBox::info(this, tr("Open containing folder"), tr("This folder does not exist on disk yet."));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

QMenu *ContentTab::buildMenu()
{
    auto *menu = new QMenu(this);
    const auto sel = m_tree->selectedItems();
    const bool has = !sel.isEmpty();
    const auto add = [menu](QMenu *m, const QString &text, const QString &icon, std::function<void()> fn, const bool enabled)
    {
        QAction *a = m->addAction(text);
        RoMenu::bind(a, icon, false);
        a->setEnabled(enabled);
        QObject::connect(a, &QAction::triggered, m, std::move(fn));
    };
    add(menu, tr("Open"), QStringLiteral("file"), [this] { openSelected(); }, has);
    add(menu, tr("Open containing folder"), QStringLiteral("folder"), [this] { openContainingFolder(); }, has);
    add(menu, tr("Rename..."), QStringLiteral("m-pencil"), [this] { renameSelectedInteractive(); }, sel.size() == 1);
    menu->addSeparator();

    QMenu *sub = menu->addMenu(tr("Download priority"));
    sub->setProperty("checks", true);
    sub->setEnabled(has);
    int current = -2;
    for (const QTreeWidgetItem *it : sel)
    {
        const int p = it->data(0, PriorityRole).toInt();
        current = (current == -2) ? p : ((current == p) ? current : Details::Mixed);
    }
    auto *group = new QActionGroup(sub);
    group->setExclusive(true);
    const struct { Details::Priority p; const char *text; const char *icon; } items[] =
    {
        {Details::Skip, QT_TR_NOOP("Do not download"), "m-ban"}, {Details::Normal, QT_TR_NOOP("Normal"), "m-queue"}
        , {Details::High, QT_TR_NOOP("High"), "q-up"}, {Details::Maximum, QT_TR_NOOP("Maximum"), "q-top"}
    };
    for (const auto &i : items)
    {
        QAction *a = sub->addAction(tr(i.text));
        a->setCheckable(true);
        a->setActionGroup(group);
        a->setChecked(current == static_cast<int>(i.p));
        RoMenu::bind(a, QString::fromLatin1(i.icon), true);
        const Details::Priority p = i.p;
        connect(a, &QAction::triggered, this, [this, p] { setPriorityOfSelection(p); });
    }
    return menu;
}

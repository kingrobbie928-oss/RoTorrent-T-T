/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021-2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2026  The RoTorrent project (modifications for RoTorrent, see docs/UPSTREAM.md)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "watchedfolderoptionsdialog.h"
#include "SubDialogShell.h"

#include "base/global.h"
#include "addtorrentparamswidget.h"
#include "ui_watchedfolderoptionsdialog.h"

WatchedFolderOptionsDialog::WatchedFolderOptionsDialog(
        const TorrentFilesWatcher::WatchedFolderOptions &watchedFolderOptions, QWidget *parent)
    : QDialog {parent}
    , m_ui {new Ui::WatchedFolderOptionsDialog}
    , m_addTorrentParamsWidget {new AddTorrentParamsWidget(watchedFolderOptions.addTorrentParams)}
{
    setObjectName(QStringLiteral("roUi"));   // before setupUi: its buttons are polished there (see OptionsDialog)
    m_ui->setupUi(this);
    // 110 px of room for "Recursive mode", the group title and the OK / Cancel row.
    m_ui->groupBoxParameters->layout()->addWidget(wrapInScrollArea(m_addTorrentParamsWidget, m_ui->groupBoxParameters, 110));

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

}

WatchedFolderOptionsDialog::~WatchedFolderOptionsDialog()
{
    delete m_ui;
}

TorrentFilesWatcher::WatchedFolderOptions WatchedFolderOptionsDialog::watchedFolderOptions() const
{
    TorrentFilesWatcher::WatchedFolderOptions watchedFolderOptions;
    watchedFolderOptions.recursive = m_ui->checkBoxRecursive->isChecked();
    watchedFolderOptions.addTorrentParams = m_addTorrentParamsWidget->addTorrentParams();

    return watchedFolderOptions;
}


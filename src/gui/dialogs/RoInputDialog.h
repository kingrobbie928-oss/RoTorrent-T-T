// SPDX-License-Identifier: GPL-2.0-or-later
// Small input dialogs: Rename torrent (420 px, "New name:") and the category dialog
// (torrentcategorydialog.ui fields: name + save path). Licensed GPL-2.0-or-later.
#pragma once

#include <optional>

#include <QString>

class QWidget;

namespace RoInputDialog
{
    // Returns the trimmed text, or nullopt when cancelled.
    std::optional<QString> getText(QWidget *parent, const QString &title, const QString &label
                                   , const QString &text = {});

    struct CategoryInput
    {
        QString name;
        QString savePath;
    };

    // editName=false for "edit": the name is shown read-only.
    std::optional<CategoryInput> getCategory(QWidget *parent, const QString &title
                                             , const CategoryInput &initial, bool editName);
}

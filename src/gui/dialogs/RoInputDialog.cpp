// SPDX-License-Identifier: GPL-2.0-or-later
#include "RoInputDialog.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "RoDialog.h"

namespace
{
    QDialogButtonBox *makeButtons(RoDialog &dialog, const QString &okText)
    {
        auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        QPushButton *ok = box->button(QDialogButtonBox::Ok);
        ok->setProperty("primary", true);
        if (!okText.isEmpty())
            ok->setText(okText);
        return box;
    }

    QDialog *makeHost(QVBoxLayout *&layout)
    {
        auto *host = new QDialog;
        host->setObjectName(QStringLiteral("roUi"));
        host->setWindowFlags(Qt::Widget);
        layout = new QVBoxLayout(host);
        layout->setContentsMargins(18, 14, 18, 14);
        layout->setSpacing(8);
        return host;
    }
}

std::optional<QString> RoInputDialog::getText(QWidget *parent, const QString &title, const QString &label
                                              , const QString &text)
{
    RoDialog dialog(parent, title, 420);
    QVBoxLayout *layout = nullptr;
    QDialog *host = makeHost(layout);
    layout->addWidget(new QLabel(label));
    auto *edit = new QLineEdit(text);
    layout->addWidget(edit);
    layout->addWidget(makeButtons(dialog, {}));
    dialog.setContent(host);
    edit->selectAll();
    edit->setFocus();
    if (dialog.run() != QDialog::Accepted)
        return std::nullopt;
    return edit->text().trimmed();
}

std::optional<RoInputDialog::CategoryInput> RoInputDialog::getCategory(QWidget *parent, const QString &title
                                                                      , const CategoryInput &initial, const bool editName)
{
    RoDialog dialog(parent, title, 460);
    QVBoxLayout *layout = nullptr;
    QDialog *host = makeHost(layout);
    layout->addWidget(new QLabel(QObject::tr("Category:")));
    auto *name = new QLineEdit(initial.name);
    name->setReadOnly(!editName);
    layout->addWidget(name);
    layout->addWidget(new QLabel(QObject::tr("Save path:")));
    auto *pathRow = new QHBoxLayout;
    pathRow->setSpacing(8);
    auto *path = new QLineEdit(initial.savePath);
    auto *browse = new QPushButton(QObject::tr("Browse..."));
    pathRow->addWidget(path, 1);
    pathRow->addWidget(browse);
    layout->addLayout(pathRow);
    QObject::connect(browse, &QPushButton::clicked, &dialog, [&dialog, path]
    {
        const QString dir = QFileDialog::getExistingDirectory(&dialog, QObject::tr("Choose save path"), path->text());
        if (!dir.isEmpty())
            path->setText(dir);
    });
    auto *buttons = makeButtons(dialog, {});
    QPushButton *ok = buttons->button(QDialogButtonBox::Ok);
    ok->setEnabled(!name->text().trimmed().isEmpty());
    QObject::connect(name, &QLineEdit::textChanged, ok, [ok](const QString &t) { ok->setEnabled(!t.trimmed().isEmpty()); });
    layout->addWidget(buttons);
    dialog.setContent(host);
    (editName ? name : path)->setFocus();
    if (dialog.run() != QDialog::Accepted)
        return std::nullopt;
    return CategoryInput {name->text().trimmed(), path->text().trimmed()};
}

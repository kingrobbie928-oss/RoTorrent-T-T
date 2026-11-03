// SPDX-License-Identifier: GPL-2.0-or-later
#include "DetailsDialogs.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

#include "dialogs/RoDialog.h"

namespace
{
    std::optional<QStringList> linesDialog(QWidget *parent, const QString &title, const QString &label
                                           , const QString &hint, const QString &okText)
    {
        RoDialog dialog(parent, title, 460);
        auto *host = new QDialog;
        host->setObjectName(QStringLiteral("roUi"));
        host->setWindowFlags(Qt::Widget);
        auto *layout = new QVBoxLayout(host);
        layout->setContentsMargins(18, 14, 18, 14);
        layout->setSpacing(8);

        auto *heading = new QLabel(label);
        heading->setWordWrap(true);
        layout->addWidget(heading);

        auto *edit = new QPlainTextEdit;
        edit->setMinimumHeight(150);
        edit->setPlaceholderText(hint);
        layout->addWidget(edit, 1);

        auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QPushButton *ok = box->button(QDialogButtonBox::Ok);
        ok->setText(okText);
        ok->setProperty("primary", true);
        QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(box);

        dialog.setContent(host);
        edit->setFocus();
        if (dialog.run() != QDialog::Accepted)
            return std::nullopt;
        const QStringList lines = DetailsDialogs::cleanLines(edit->toPlainText());
        if (lines.isEmpty())
            return std::nullopt;
        return lines;
    }
}

QStringList DetailsDialogs::cleanLines(const QString &text)
{
    QStringList out;
    QSet<QString> seen;
    for (const QString &raw : text.split(u'\n', Qt::SkipEmptyParts))
    {
        const QString line = raw.trimmed();
        if (line.isEmpty() || seen.contains(line))
            continue;
        seen.insert(line);
        out << line;
    }
    return out;
}

std::optional<QStringList> DetailsDialogs::addTrackers(QWidget *parent)
{
    return linesDialog(parent, QObject::tr("Add trackers"), QObject::tr("List of trackers to add (one per line):")
                       , QObject::tr("One tracker URL per line"), QObject::tr("Add"));
}

std::optional<QStringList> DetailsDialogs::addPeers(QWidget *parent)
{
    return linesDialog(parent, QObject::tr("Add peers"), QObject::tr("List of peers to add (one IP per line):")
                       , QObject::tr("Format: IPv4:port / [IPv6]:port"), QObject::tr("Add"));
}

std::optional<QStringList> DetailsDialogs::addHttpSources(QWidget *parent)
{
    return linesDialog(parent, QObject::tr("Add HTTP source"), QObject::tr("List of HTTP sources to add (one URL per line):")
                       , QObject::tr("One web seed URL per line"), QObject::tr("Add"));
}

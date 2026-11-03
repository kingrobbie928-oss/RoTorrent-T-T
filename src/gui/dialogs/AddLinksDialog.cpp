// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "AddLinksDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include "RoDialog.h"

bool AddLinksDialog::isSupportedLink(const QString &link)
{
    const QString s = link.trimmed();
    if (s.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive))
        return true;
    for (const QString &scheme : {QStringLiteral("http://"), QStringLiteral("https://"), QStringLiteral("ftp://")})
    {
        if (s.startsWith(scheme, Qt::CaseInsensitive))
            return true;
    }
    static const QRegularExpression hash(QStringLiteral("^(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64}|[2-7A-Za-z]{32})$"));
    return hash.match(s).hasMatch();
}

std::optional<QStringList> AddLinksDialog::getLinks(QWidget *parent, const QString &initialText)
{
    RoDialog dialog(parent, QObject::tr("Add torrent links"), 560);

    auto *host = new QDialog;
    host->setObjectName(QStringLiteral("roUi"));
    host->setWindowFlags(Qt::Widget);
    auto *layout = new QVBoxLayout(host);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(10);

    auto *heading = new QLabel(QObject::tr("Add torrent links"));
    heading->setObjectName(QStringLiteral("dlgHeading"));
    QFont hf = heading->font();
    hf.setWeight(QFont::Bold);
    heading->setFont(hf);
    layout->addWidget(heading);

    auto *edit = new QPlainTextEdit;
    edit->setMinimumHeight(150);
    edit->setPlainText(initialText);
    layout->addWidget(edit, 1);

    auto *hint = new QLabel(QObject::tr("One link per line (HTTP links, Magnet links and info-hashes are supported)"));
    hint->setObjectName(QStringLiteral("dlgBodyText"));
    QFont f = hint->font();
    f.setItalic(true);
    hint->setFont(f);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QPushButton *ok = buttons->button(QDialogButtonBox::Ok);
    ok->setProperty("primary", true);
    ok->setEnabled(false);
    QObject::connect(edit, &QPlainTextEdit::textChanged, ok, [edit, ok]
    {
        ok->setEnabled(!edit->toPlainText().trimmed().isEmpty());
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.setContent(host);
    edit->setFocus();
    if (initialText.isEmpty())
    {
        // Convenience: start with a link already on the clipboard, as upstream does.
        const QString clip = QApplication::clipboard()->text().trimmed();
        if (!clip.isEmpty() && !clip.contains(QLatin1Char('\n')) && isSupportedLink(clip))
        {
            edit->setPlainText(clip);
            edit->selectAll();
        }
    }
    if (dialog.run() != QDialog::Accepted)
        return std::nullopt;

    QStringList links;
    for (const QString &line : edit->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts))
    {
        const QString link = line.trimmed();
        if (!link.isEmpty() && !links.contains(link))
            links << link;
    }
    return links;
}

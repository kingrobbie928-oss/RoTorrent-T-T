// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "StacktraceDialog.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextDocument>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "base/utils/misc.h"
#include "base/version.h"
#include "theme/RoTheme.h"

namespace
{
    // Placeholder until the owner supplies the real tracker (handoff 11, item 6).
    const QString BUG_TRACKER_URL = QStringLiteral("https://bugs.rotorrent.example");
}

StacktraceDialog::StacktraceDialog(QWidget *parent)
    : RoDialog(parent, tr("Crash info"), 642)
{
    auto *host = new QDialog;
    host->setObjectName(QStringLiteral("roUi"));
    host->setWindowFlags(Qt::Widget);
    auto *v = new QVBoxLayout(host);
    v->setContentsMargins(12, 12, 12, 0);
    v->setSpacing(0);

    m_text = new QTextBrowser;
    m_text->setObjectName(QStringLiteral("crashText"));
    m_text->setOpenExternalLinks(true);
    m_text->setMinimumHeight(430);
    v->addWidget(m_text, 1);
    setContent(host);

    // The footer sits outside the "roUi" area, like the kit's: inside it the dialog stylesheet would
    // override the outlined accent look of `dlgBtn`.
    auto *footer = new QWidget;
    auto *row = new QHBoxLayout(footer);
    row->setContentsMargins(12, 10, 12, 12);
    row->addStretch(1);
    m_close = new QPushButton(tr("Close"));
    m_close->setObjectName(QStringLiteral("dlgBtn"));
    m_close->setCursor(Qt::PointingHandCursor);
    m_close->setDefault(true);
    row->addWidget(m_close);
    body()->addWidget(footer);

    connect(m_close, &QPushButton::clicked, this, &QDialog::accept);
    setText(QString(), QString());
}

void StacktraceDialog::setText(const QString &signalName, const QString &stacktrace)
{
    // Called from a signal handler: nothing here may block or need the engine.
    const RoPalette &P = RoTheme::instance().pal();
    const QString info = tr("RoTorrent version: %1 (%2-bit)\nLibtorrent version: %3\nQt version: %4\nBoost version: %5\n"
                            "OpenSSL version: %6\nzlib version: %7\nOS version: %8")
                             .arg(QStringLiteral(QBT_VERSION), QString::number(QT_POINTER_SIZE * 8)
                                  , Utils::Misc::libtorrentVersionString(), QStringLiteral(QT_VERSION_STR)
                                  , Utils::Misc::boostVersionString(), Utils::Misc::opensslVersionString()
                                  , Utils::Misc::zlibVersionString(), Utils::Misc::osName());

    QString html = QStringLiteral("<p style=\"font-size:15px;font-weight:600;color:%1;\">%2</p>")
                       .arg(P.accent.name(), tr("RoTorrent has crashed.").toHtmlEscaped());
    html += QStringLiteral("<p>%1</p>")
                .arg(tr("Please file a bug report at %1 and include the text below.")
                         .arg(QStringLiteral("<a href=\"%1\">%1</a>").arg(BUG_TRACKER_URL)));
    html += QStringLiteral("<pre>%1</pre>").arg(info.toHtmlEscaped());
    if (!signalName.isEmpty() || !stacktrace.isEmpty())
    {
        html += QStringLiteral("<p>%1 <b>%2</b></p>").arg(tr("Caught signal:").toHtmlEscaped(), signalName.toHtmlEscaped());
        html += QStringLiteral("<pre>```\n%1\n```</pre>").arg(stacktrace.toHtmlEscaped());
    }
    // Links default to dark blue, unreadable on the dark card: use the accent colour in both themes.
    m_text->document()->setDefaultStyleSheet(QStringLiteral("a { color: %1; }").arg(P.accent.name()));
    m_text->setHtml(html);
}

QString StacktraceDialog::reportText() const
{
    return m_text->toPlainText();
}

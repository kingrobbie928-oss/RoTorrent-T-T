// SPDX-License-Identifier: GPL-2.0-or-later
// About dialog (handoff 8.12). The "Based on qBittorrent" credit and the licence text live here, which is the
// one place the label audit allows the name. Licensed GPL-2.0-or-later.

#include "InfoDialogs.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "base/utils/misc.h"
#include "theme/RoTheme.h"
#include "RoDialog.h"

void AboutDialog::show(QWidget *parent)
{
    RoDialog dialog(parent, QObject::tr("About"), 640);
    QVBoxLayout *layout = nullptr;
    auto *host = new QDialog;
    host->setObjectName(QStringLiteral("roUi"));
    host->setWindowFlags(Qt::Widget);
    layout = new QVBoxLayout(host);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(8);

    auto *name = new QLabel(QObject::tr("RoTorrent %1 (64-bit)").arg(QStringLiteral("1.0")));
    QFont nf = name->font();
    nf.setPixelSize(20);
    nf.setWeight(QFont::Bold);
    name->setFont(nf);
    layout->addWidget(name);

    const QString link = RoTheme::instance().pal().accent.name();
    auto page = [&](const QString &html)
    {
        auto *b = new QTextBrowser;
        b->setOpenExternalLinks(true);
        b->setFrameShape(QFrame::NoFrame);
        b->setHtml(html);
        b->setMinimumHeight(250);
        return b;
    };
    const auto a = [&link](const QString &url, const QString &text)
    {
        return QStringLiteral("<a href=\"%1\" style=\"color:%2; text-decoration:none\">%3</a>").arg(url, link, text);
    };

    // The RO logo, 70 px tall and centred above the text of the About tab (handoff 8.12). The 764 px wide
    // source is halved in steps (one big shrink aliases), as the title-row logo is.
    auto withLogo = [&dialog](QTextBrowser *text)
    {
        const qreal dpr = dialog.devicePixelRatioF();
        const int targetH = qRound(70 * dpr);
        QImage img = QPixmap(RoTheme::instance().logoPath()).toImage();
        auto *tab = new QWidget;
        auto *box = new QVBoxLayout(tab);
        box->setContentsMargins(0, 8, 0, 0);
        box->setSpacing(6);
        if (!img.isNull())
        {
            while (img.height() > 2 * targetH)
                img = img.scaledToHeight(qMax(targetH, img.height() / 2), Qt::SmoothTransformation);
            img = img.scaledToHeight(targetH, Qt::SmoothTransformation);
            QPixmap pixmap = QPixmap::fromImage(img);
            pixmap.setDevicePixelRatio(dpr);
            auto *logo = new QLabel;
            logo->setPixmap(pixmap);
            logo->setAlignment(Qt::AlignCenter);
            box->addWidget(logo);
        }
        box->addWidget(text, 1);
        return tab;
    };

    auto *tabs = new QTabWidget;
    tabs->addTab(withLogo(page(QStringLiteral("<p>%1</p><p>%2</p><table cellspacing=6>"
                                     "<tr><td>%3</td><td>%4</td></tr><tr><td>%5</td><td>%6</td></tr>"
                                     "<tr><td>%7</td><td>%8</td></tr></table>")
                      .arg(QObject::tr("A fast, modern BitTorrent client with a clean interface, built on Qt and libtorrent-rasterbar.")
                           , QObject::tr("Copyright \u00A9 2026 The RoTorrent project")
                           , QObject::tr("Home Page:"), a(QStringLiteral("https://rotorrent.example"), QStringLiteral("https://rotorrent.example"))
                           , QObject::tr("Forum:"), a(QStringLiteral("https://forum.rotorrent.example"), QStringLiteral("https://forum.rotorrent.example"))
                           , QObject::tr("Bug Tracker:"), a(QStringLiteral("https://bugs.rotorrent.example"), QStringLiteral("https://bugs.rotorrent.example")))
                   )), QObject::tr("About"));
    tabs->addTab(page(QStringLiteral("<p><b>%1</b><br>%2 RoTorrent Team<br>%3 \u2014<br>%4 team@rotorrent.example</p>"
                                     "<p><b>%5</b><br>qBittorrent project<br>%6<br>%7</p>")
                      .arg(QObject::tr("Current maintainer"), QObject::tr("Name:"), QObject::tr("Region:"), QObject::tr("E-mail:")
                           , QObject::tr("Based on"), a(QStringLiteral("https://www.qbittorrent.org"), QStringLiteral("https://www.qbittorrent.org"))
                           , QObject::tr("License: GNU GPL v2 or later"))), QObject::tr("Authors"));
    tabs->addTab(page(QStringLiteral("<p>%1</p><ul><li>qBittorrent</li><li>libtorrent-rasterbar</li><li>Boost</li>"
                                     "<li>OpenSSL</li><li>zlib</li><li>Qt</li></ul>")
                      .arg(QObject::tr("Thanks to the testers and contributors, and to the projects below:"))), QObject::tr("Special Thanks"));
    tabs->addTab(page(QStringLiteral("<p>%1</p>").arg(QObject::tr("Translations are contributed by the community."))), QObject::tr("Translators"));
    tabs->addTab(page(QStringLiteral("<p>%1</p><p>%2</p>")
                      .arg(QObject::tr("RoTorrent is based on qBittorrent and is distributed under the GNU General Public License, "
                                       "version 2 or (at your option) any later version.")
                           , QObject::tr("This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; "
                                         "without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."))), QObject::tr("License"));
    tabs->addTab(page(QStringLiteral("<p>%1</p><ul><li>Qt: %2</li><li>libtorrent-rasterbar: %3</li><li>Boost: %4</li>"
                                     "<li>OpenSSL: %5</li><li>zlib: %6</li></ul><p>%7</p>")
                      .arg(QObject::tr("The following libraries are used in RoTorrent:")
                           , QString::fromLatin1(qVersion()), Utils::Misc::libtorrentVersionString(), Utils::Misc::boostVersionString()
                           , Utils::Misc::opensslVersionString(), Utils::Misc::zlibVersionString()
                           , QObject::tr("IP geolocation data by DB-IP (db-ip.com), licensed under CC BY 4.0."))), QObject::tr("Software Used"));
    layout->addWidget(tabs, 1);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Close);
    box->button(QDialogButtonBox::Close)->setProperty("primary", true);
    QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(box);

    dialog.setContent(host);
    dialog.run();
}


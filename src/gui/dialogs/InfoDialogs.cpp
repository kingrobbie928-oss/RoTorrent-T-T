// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "InfoDialogs.h"

#include <algorithm>

#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

#include "base/bittorrent/cachestatus.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/sessionstatus.h"
#include "base/utils/misc.h"
#include "base/version.h"
#include "theme/RoTheme.h"
#include "MainWindow.h"
#include "RoDialog.h"

namespace
{
    QDialog *makeHost(QVBoxLayout *&layout, const int margin = 18)
    {
        auto *host = new QDialog;
        host->setObjectName(QStringLiteral("roUi"));
        host->setWindowFlags(Qt::Widget);
        layout = new QVBoxLayout(host);
        layout->setContentsMargins(margin, 14, margin, 14);
        layout->setSpacing(8);
        return host;
    }

    QDialogButtonBox *closeButtons(RoDialog &dialog)
    {
        auto *box = new QDialogButtonBox(QDialogButtonBox::Close);
        box->button(QDialogButtonBox::Close)->setProperty("primary", true);
        QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        return box;
    }

    QLabel *heading(const QString &text)
    {
        auto *l = new QLabel(text.toUpper());
        l->setObjectName(QStringLiteral("dlgHeading"));
        return l;
    }

    // One "label ... value" row; returns the value label so the caller can refresh it.
    QLabel *addRow(QVBoxLayout *layout, const QString &key)
    {
        auto *row = new QHBoxLayout;
        auto *k = new QLabel(key);
        k->setObjectName(QStringLiteral("dlgKey"));
        auto *v = new QLabel(QStringLiteral("-"));
        v->setObjectName(QStringLiteral("dlgVal"));
        v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(k, 1);
        row->addWidget(v);
        layout->addLayout(row);
        return v;
    }

    QString percent(const double v)
    {
        return QString::number(v, 'f', 2) + QLatin1Char('%');
    }
}

void StatisticsDialog::show(QWidget *parent, BitTorrent::Session *session)
{
    RoDialog dialog(parent, QObject::tr("Statistics"), 420);
    QVBoxLayout *layout = nullptr;
    QDialog *host = makeHost(layout);

    layout->addWidget(heading(QObject::tr("User statistics")));
    QLabel *allUp = addRow(layout, QObject::tr("All-time upload:"));
    QLabel *allDown = addRow(layout, QObject::tr("All-time download:"));
    QLabel *ratio = addRow(layout, QObject::tr("All-time share ratio:"));
    QLabel *waste = addRow(layout, QObject::tr("Session waste:"));
    QLabel *peers = addRow(layout, QObject::tr("Connected peers:"));
    layout->addSpacing(8);
    layout->addWidget(heading(QObject::tr("Cache statistics")));
    QLabel *hits = addRow(layout, QObject::tr("Read cache hits:"));
    QLabel *buffer = addRow(layout, QObject::tr("Total buffer size:"));
    layout->addSpacing(8);
    layout->addWidget(heading(QObject::tr("Performance statistics")));
    QLabel *writeOver = addRow(layout, QObject::tr("Write cache overload:"));
    QLabel *readOver = addRow(layout, QObject::tr("Read cache overload:"));
    QLabel *jobs = addRow(layout, QObject::tr("Queued I/O jobs:"));
    QLabel *jobTime = addRow(layout, QObject::tr("Average time in queue:"));
    QLabel *queued = addRow(layout, QObject::tr("Total queued size:"));
    layout->addSpacing(8);
    layout->addWidget(heading(QObject::tr("Tracker statistics")));
    QLabel *announces = addRow(layout, QObject::tr("Queued tracker announces:"));
    layout->addSpacing(6);
    layout->addWidget(closeButtons(dialog));

    auto refresh = [=]
    {
        const BitTorrent::SessionStatus &ss = session->status();
        const BitTorrent::CacheStatus &cs = session->cacheStatus();
        allUp->setText(Utils::Misc::friendlyUnit(ss.allTimeUpload));
        allDown->setText(Utils::Misc::friendlyUnit(ss.allTimeDownload));
        ratio->setText((ss.allTimeDownload > 0)
                ? QString::number(static_cast<double>(ss.allTimeUpload) / static_cast<double>(ss.allTimeDownload), 'f', 2)
                : QStringLiteral("-"));
        waste->setText(Utils::Misc::friendlyUnit(ss.totalWasted));
        peers->setText(QString::number(ss.peersCount));
        hits->setText(percent(cs.readRatio * 100.0));
        buffer->setText(Utils::Misc::friendlyUnit(cs.totalUsedBuffers * 16 * 1024));
        const double wDen = static_cast<double>(ss.diskWriteQueue + cs.totalUsedBuffers);
        const double rDen = static_cast<double>(ss.diskReadQueue + cs.totalUsedBuffers);
        writeOver->setText(percent((wDen > 0) ? (100.0 * static_cast<double>(ss.diskWriteQueue) / wDen) : 0.0));
        readOver->setText(percent((rDen > 0) ? (100.0 * static_cast<double>(ss.diskReadQueue) / rDen) : 0.0));
        jobs->setText(QString::number(cs.jobQueueLength));
        jobTime->setText(QObject::tr("%1 ms").arg(cs.averageJobTime));
        queued->setText(Utils::Misc::friendlyUnit(cs.queuedBytes));
        announces->setText(QString::number(ss.queuedTrackerAnnounces));
    };
    refresh();
    auto *timer = new QTimer(host);
    QObject::connect(timer, &QTimer::timeout, host, refresh);
    timer->start(1500);

    dialog.setContent(host);
    dialog.run();
}

bool SpeedLimitsDialog::show(QWidget *parent, BitTorrent::Session *session)
{
    RoDialog dialog(parent, QObject::tr("Global Speed Limits"), 440);
    QVBoxLayout *layout = nullptr;
    QDialog *host = makeHost(layout);

    auto makeSpin = [](const int bytesPerSecond)
    {
        auto *spin = new QSpinBox;
        spin->setRange(0, 2097151);
        spin->setSingleStep(10);
        spin->setSuffix(QObject::tr(" KiB/s"));
        spin->setSpecialValueText(QStringLiteral("\u221E"));
        spin->setValue(std::max(0, bytesPerSecond) / 1024);
        return spin;
    };
    // A slider beside each spin box (the .ui has both): dragging sets the box, typing in the box moves the
    // slider. The slider covers 0 to 10,000 KiB/s (more when the current limit is higher); the box takes
    // anything up to 2,097,151 KiB/s, and a value beyond the slider's range parks it at the end.
    auto makeSlider = [](QSpinBox *spin)
    {
        auto *slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, std::max(10000, spin->value()));
        slider->setValue(spin->value());
        QObject::connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
        QObject::connect(spin, &QSpinBox::valueChanged, slider, [slider](const int value)
        {
            const QSignalBlocker blocker(slider);
            slider->setValue(value);
        });
        return slider;
    };
    auto group = [&](const QString &title, QSpinBox *up, QSpinBox *down)
    {
        layout->addWidget(heading(title));
        auto *grid = new QGridLayout;
        grid->setHorizontalSpacing(12);
        grid->addWidget(new QLabel(QObject::tr("Upload:")), 0, 0);
        grid->addWidget(makeSlider(up), 0, 1);
        grid->addWidget(up, 0, 2);
        grid->addWidget(new QLabel(QObject::tr("Download:")), 1, 0);
        grid->addWidget(makeSlider(down), 1, 1);
        grid->addWidget(down, 1, 2);
        grid->setColumnStretch(1, 1);
        layout->addLayout(grid);
    };
    QSpinBox *up = makeSpin(session->globalUploadSpeedLimit());
    QSpinBox *down = makeSpin(session->globalDownloadSpeedLimit());
    QSpinBox *altUp = makeSpin(session->altGlobalUploadSpeedLimit());
    QSpinBox *altDown = makeSpin(session->altGlobalDownloadSpeedLimit());
    group(QObject::tr("Speed limits"), up, down);
    layout->addSpacing(8);
    group(QObject::tr("Alternative speed limits"), altUp, altDown);
    layout->addSpacing(6);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setProperty("primary", true);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.setContent(host);
    if (dialog.run() != QDialog::Accepted)
        return false;
    session->setGlobalUploadSpeedLimit(up->value() * 1024);
    session->setGlobalDownloadSpeedLimit(down->value() * 1024);
    session->setAltGlobalUploadSpeedLimit(altUp->value() * 1024);
    session->setAltGlobalDownloadSpeedLimit(altDown->value() * 1024);
    return true;
}

void InfoDialogs::install(MainWindow &window, BitTorrent::Session *session)
{
    MainWindow *w = &window;
    window.hooks().statistics = [w, session] { StatisticsDialog::show(w, session); };
    window.hooks().speedLimits = [w, session] { SpeedLimitsDialog::show(w, session); };
    window.hooks().about = [w] { AboutDialog::show(w); };
}

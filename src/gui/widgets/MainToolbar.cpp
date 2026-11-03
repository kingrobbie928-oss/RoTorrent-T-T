// SPDX-License-Identifier: GPL-2.0-or-later
#include "MainToolbar.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include "AccentButton.h"
#include "RoSearchBox.h"
#include "chrome/RoHoverIcon.h"
#include "theme/RoTheme.h"

namespace
{
    RoHoverIcon *makeButton(const QString &icon, const QString &tip, const bool danger, QBoxLayout *layout)
    {
        auto *b = new RoHoverIcon(icon, &RoPalette::muted, &RoPalette::text, danger, 16);
        b->setObjectName(QStringLiteral("tbIcon"));
        b->setFixedSize(32, 32);
        b->setToolTip(tip);
        b->setProperty("danger", danger);
        layout->addWidget(b, 0, Qt::AlignVCenter);
        return b;
    }

    void addSeparator(QBoxLayout *layout)
    {
        auto *f = new QFrame;
        f->setObjectName(QStringLiteral("tbSep"));
        f->setFixedSize(1, 34);
        layout->addWidget(f, 0, Qt::AlignVCenter);
    }
}

MainToolbar::MainToolbar(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("mainToolbar"));
    setFixedHeight(51);   // 50 px + the 1 px bottom border
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 1);
    outer->setSpacing(0);

    auto *row = new QWidget;
    row->setObjectName(QStringLiteral("toolRow"));
    auto *rl = new QHBoxLayout(row);
    rl->setContentsMargins(14, 0, 14, 0);
    rl->setSpacing(8);
    outer->addWidget(row);

    connect(makeButton(QStringLiteral("create"), tr("Create torrent"), false, rl), &QToolButton::clicked
            , this, &MainToolbar::createRequested);
    addSeparator(rl);
    m_pause = makeButton(QStringLiteral("pause"), tr("Pause"), false, rl);
    m_resume = makeButton(QStringLiteral("resume"), tr("Resume"), false, rl);
    connect(m_pause, &QToolButton::clicked, this, &MainToolbar::pauseRequested);
    connect(m_resume, &QToolButton::clicked, this, &MainToolbar::resumeRequested);
    addSeparator(rl);
    m_delete = makeButton(QStringLiteral("delete"), tr("Delete"), true, rl);
    connect(m_delete, &QToolButton::clicked, this, &MainToolbar::deleteRequested);

    // Queue arrows: one container so separator + four buttons hide together.
    m_queueGroup = new QWidget;
    auto *ql = new QHBoxLayout(m_queueGroup);
    ql->setContentsMargins(0, 0, 0, 0);
    ql->setSpacing(8);
    addSeparator(ql);
    const struct { const char *icon; QueueButton which; } arrows[] =
    {
        {"q-top", QueueButton::Top}, {"q-up", QueueButton::Up}, {"q-down", QueueButton::Down}, {"q-bottom", QueueButton::Bottom}
    };
    const QString tips[] = {tr("Top of Queue"), tr("Move Up Queue"), tr("Move Down Queue"), tr("Bottom of Queue")};
    int i = 0;
    for (const auto &a : arrows)
    {
        RoHoverIcon *b = makeButton(QString::fromLatin1(a.icon), tips[i++], false, ql);
        const QueueButton which = a.which;
        connect(b, &QToolButton::clicked, this, [this, which] { emit queueRequested(which); });
        m_queueButtons.append(b);
    }
    rl->addWidget(m_queueGroup, 0, Qt::AlignVCenter);
    m_queueGroup->hide();   // shown only while Torrent Queueing is on

    addSeparator(rl);
    connect(makeButton(QStringLiteral("settings"), tr("Settings"), false, rl), &QToolButton::clicked
            , this, &MainToolbar::settingsRequested);

    m_search = new RoSearchBox;
    m_search->setFixedHeight(34);
    m_search->setMinimumWidth(0);
    m_search->setMaximumWidth(340);
    rl->addWidget(m_search, 1, Qt::AlignVCenter);
    rl->addStretch(1);

    auto *link = new AccentButton(QString(), QStringLiteral("link"), 14);
    link->setObjectName(QStringLiteral("linkBtn"));
    link->setFixedSize(38, 32);
    link->setToolTip(tr("Add torrent from URL"));
    connect(link, &QPushButton::clicked, this, &MainToolbar::addLinkRequested);
    rl->addWidget(link, 0, Qt::AlignVCenter);

    auto *add = new AccentButton(tr("  Add torrent"), QStringLiteral("plus"), 14);
    add->setObjectName(QStringLiteral("addBtn"));
    add->setFixedHeight(32);
    connect(add, &QPushButton::clicked, this, &MainToolbar::addTorrentRequested);
    rl->addWidget(add, 0, Qt::AlignVCenter);

    auto *stack = new QWidget;
    auto *sl = new QVBoxLayout(stack);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(5);
    m_themeToggle = new QCheckBox;
    m_themeToggle->setObjectName(QStringLiteral("themeToggle"));
    m_themeToggle->setFixedSize(40, 20);
    m_themeToggle->setToolTip(tr("Light / dark theme"));
    m_vibrantToggle = new QCheckBox;
    m_vibrantToggle->setObjectName(QStringLiteral("vibrantToggle"));
    m_vibrantToggle->setFixedSize(40, 20);
    m_vibrantToggle->setToolTip(tr("Vibrant rows"));
    stack->setFixedSize(40, 45);
    sl->addWidget(m_themeToggle);
    sl->addWidget(m_vibrantToggle);
    rl->addWidget(stack, 0, Qt::AlignVCenter);

    syncToggles();
    connect(m_themeToggle, &QCheckBox::toggled, this, [](const bool light) { RoTheme::instance().setDark(!light); });
    connect(m_vibrantToggle, &QCheckBox::toggled, this, [](const bool on) { RoTheme::instance().setVibrant(on); });
    connect(&RoTheme::instance(), &RoTheme::changed, this, &MainToolbar::syncToggles);

    // Natural widths, clipped at the right edge; only the search box shrinks (handoff 8.3).
    row->setSizePolicy(QSizePolicy::Ignored, row->sizePolicy().verticalPolicy());
    setSizePolicy(QSizePolicy::Ignored, sizePolicy().verticalPolicy());
    QTimer::singleShot(0, this, [this, row, link, add]
    {
        add->setMinimumWidth(add->sizeHint().width());
        link->setMinimumWidth(link->sizeHint().width());
        row->setMinimumWidth(row->layout()->minimumSize().width());
    });
    setCommandStates(false, false, false, false);
}

void MainToolbar::setQueueGroupVisible(const bool visible)
{
    m_queueGroup->setVisible(visible);
}

void MainToolbar::setCommandStates(const bool resume, const bool pause, const bool remove, const bool queue)
{
    m_resume->setEnabled(resume);
    m_pause->setEnabled(pause);
    m_delete->setEnabled(remove);
    for (RoHoverIcon *b : std::as_const(m_queueButtons))
        b->setEnabled(queue);
}

void MainToolbar::syncToggles()
{
    const QSignalBlocker blockTheme(m_themeToggle);
    const QSignalBlocker blockVibrant(m_vibrantToggle);
    m_themeToggle->setChecked(RoTheme::instance().isLight());
    m_vibrantToggle->setChecked(RoTheme::instance().vibrant());
}

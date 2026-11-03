// SPDX-License-Identifier: GPL-2.0-or-later
#include "DetailsTabs.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QTextDocument>
#include <QVBoxLayout>

#include "LogBuffer.h"
#include "base/settingsstorage.h"
#include "theme/RoTheme.h"
#include "widgets/RoPaint.h"

namespace
{
    const QString MASK_KEY = QStringLiteral("Gui/LogMask");

    struct KindInfo { int kind; const char *text; const char *icon; QColor RoPalette::*colour; };
    const KindInfo KINDS[] =
    {
        {LogBuffer::Normal, QT_TRANSLATE_NOOP("LogTab", "Normal"), "m-msg", &RoPalette::muted}
        , {LogBuffer::Info, QT_TRANSLATE_NOOP("LogTab", "Information"), "info", &RoPalette::info}
        , {LogBuffer::Warning, QT_TRANSLATE_NOOP("LogTab", "Warning"), "m-warn", &RoPalette::check}
        , {LogBuffer::Critical, QT_TRANSLATE_NOOP("LogTab", "Critical"), "m-crit", &RoPalette::accent}
    };

    QColor textColour(const int kind)
    {
        const RoPalette &p = RoTheme::instance().pal();
        switch (kind)
        {
        case LogBuffer::Info: return p.info;
        case LogBuffer::Warning: return p.check;
        case LogBuffer::Critical: return p.accent;
        default: return p.text;
        }
    }
}

LogTab::LogTab(DetailsSource *source, LogBuffer *buffer, QWidget *parent)
    : DetailsTab(source, parent)
    , m_buffer(buffer)
{
    m_mask = SettingsStorage::instance()->loadValue<int>(MASK_KEY, 15) & 15;

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(22, 12, 22, 12);
    lay->setSpacing(0);
    auto *top = new QHBoxLayout;
    top->setSpacing(6);
    top->setContentsMargins(0, 0, 0, 10);
    for (const KindInfo &k : KINDS)
    {
        auto *chip = new QPushButton(QStringLiteral(" ") + tr(k.text));
        chip->setObjectName(QStringLiteral("logChip"));
        chip->setCheckable(true);
        chip->setChecked(m_mask & k.kind);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setFocusPolicy(Qt::NoFocus);
        chip->setIconSize({13, 13});
        const int kind = k.kind;
        connect(chip, &QPushButton::toggled, this, [this, kind](const bool on)
        {
            setMask(on ? (m_mask | kind) : (m_mask & ~kind));
        });
        m_chips.insert(k.kind, chip);
        top->addWidget(chip);
    }
    top->addStretch(1);
    lay->addLayout(top);

    m_view = new QTextEdit;
    m_view->setObjectName(QStringLiteral("logText"));
    m_view->setReadOnly(true);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setFocusPolicy(Qt::NoFocus);
    m_view->document()->setMaximumBlockCount(LogBuffer::MaxEntries);
    lay->addWidget(m_view, 1);

    connect(m_buffer, &LogBuffer::added, this, [this]
    {
        const auto &entries = m_buffer->entries();
        if (entries.isEmpty() || !(m_mask & entries.last().kind))
            return;
        if (m_view->toPlainText() == tr("No messages to show."))   // first visible line replaces the placeholder
        {
            render();
            return;
        }
        QScrollBar *sb = m_view->verticalScrollBar();
        const bool atBottom = sb->value() >= sb->maximum() - 4;
        const auto &e = entries.last();
        m_view->append(lineHtml(e.time, e.kind, e.text));
        if (atBottom)
            sb->setValue(sb->maximum());
    });
    connect(&RoTheme::instance(), &RoTheme::changed, this, &LogTab::refresh);
    updateChipIcons();
    render();
}

QString LogTab::lineHtml(const QDateTime &time, const int kind, const QString &text)
{
    const RoPalette &p = RoTheme::instance().pal();
    QString esc = text.toHtmlEscaped();
    return QStringLiteral("<span style=\"color:%1\">[%2]</span>&nbsp;&nbsp;<span style=\"color:%3\">%4</span>")
            .arg(p.faint.name(), time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), textColour(kind).name(), esc);
}

void LogTab::updateChipIcons()
{
    const RoPalette &pal = RoTheme::instance().pal();
    for (const KindInfo &k : KINDS)
    {
        QPushButton *chip = m_chips.value(k.kind);
        const bool on = chip->isChecked();
        chip->setIcon(RoTheme::instance().icon(QString::fromLatin1(k.icon), on ? pal.*(k.colour) : pal.faint, 13));
    }
}

void LogTab::setMask(const int mask)
{
    m_mask = mask & 15;
    for (const KindInfo &k : KINDS)
    {
        QPushButton *chip = m_chips.value(k.kind);
        const QSignalBlocker block(chip);
        chip->setChecked(m_mask & k.kind);
    }
    SettingsStorage::instance()->storeValue(MASK_KEY, m_mask);
    updateChipIcons();
    render();
}

void LogTab::refresh()
{
    updateChipIcons();
    render();
}

void LogTab::render()
{
    QStringList lines;
    for (const LogBuffer::Entry &e : m_buffer->entries())
    {
        if (e.kind & m_mask)
            lines << lineHtml(e.time, e.kind, e.text);
    }
    if (lines.isEmpty())
    {
        m_view->setHtml(QStringLiteral("<span style=\"color:%1\">%2</span>")
                        .arg(RoTheme::instance().pal().faint.name(), tr("No messages to show.")));
        return;
    }
    m_view->setHtml(lines.join(QStringLiteral("<br>")));
    m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->maximum());
}

QString LogTab::plainText() const
{
    return m_view->toPlainText();
}

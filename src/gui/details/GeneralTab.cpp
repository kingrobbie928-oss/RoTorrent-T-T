// SPDX-License-Identifier: GPL-2.0-or-later
#include "DetailsTabs.h"

#include <QGridLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>

#include "DetailsSource.h"
#include "DetailsWidgets.h"
#include "models/rostatus.h"

DetailsTab::DetailsTab(DetailsSource *source, QWidget *parent)
    : QWidget(parent)
    , m_source(source)
{
}

void DetailsTab::setTorrent(const QString &id)
{
    m_id = id;
    refresh();
}

// ---------------------------------------------------------------------------------------------

namespace
{
    // Field order and labels are the spec's (8.9 General): 16 TRANSFER fields, 7 INFORMATION fields,
    // then four wide rows.
    struct Field { const char *key; const char *label; };
    const Field TRANSFER[] =
    {
        {"timeActive", QT_TRANSLATE_NOOP("GeneralTab", "Time Active")}, {"eta", QT_TRANSLATE_NOOP("GeneralTab", "ETA")}, {"connections", QT_TRANSLATE_NOOP("GeneralTab", "Connections")}
        , {"downloaded", QT_TRANSLATE_NOOP("GeneralTab", "Downloaded")}, {"uploaded", QT_TRANSLATE_NOOP("GeneralTab", "Uploaded")}, {"seeds", QT_TRANSLATE_NOOP("GeneralTab", "Seeds")}
        , {"downSpeed", QT_TRANSLATE_NOOP("GeneralTab", "Download Speed")}, {"upSpeed", QT_TRANSLATE_NOOP("GeneralTab", "Upload Speed")}, {"peers", QT_TRANSLATE_NOOP("GeneralTab", "Peers")}
        , {"downLimit", QT_TRANSLATE_NOOP("GeneralTab", "Download Limit")}, {"upLimit", QT_TRANSLATE_NOOP("GeneralTab", "Upload Limit")}, {"wasted", QT_TRANSLATE_NOOP("GeneralTab", "Wasted")}
        , {"ratio", QT_TRANSLATE_NOOP("GeneralTab", "Share Ratio")}, {"reannounce", QT_TRANSLATE_NOOP("GeneralTab", "Reannounce In")}
        , {"lastSeen", QT_TRANSLATE_NOOP("GeneralTab", "Last Seen Complete")}, {"popularity", QT_TRANSLATE_NOOP("GeneralTab", "Popularity")}
    };
    const Field INFORMATION[] =
    {
        {"totalSize", QT_TRANSLATE_NOOP("GeneralTab", "Total Size")}, {"pieces", QT_TRANSLATE_NOOP("GeneralTab", "Pieces")}, {"createdBy", QT_TRANSLATE_NOOP("GeneralTab", "Created By")}
        , {"addedOn", QT_TRANSLATE_NOOP("GeneralTab", "Added On")}, {"completedOn", QT_TRANSLATE_NOOP("GeneralTab", "Completed On")}, {"createdOn", QT_TRANSLATE_NOOP("GeneralTab", "Created On")}
        , {"private", QT_TRANSLATE_NOOP("GeneralTab", "Private")}
    };
    const Field WIDE[] =
    {
        {"hashV1", QT_TRANSLATE_NOOP("GeneralTab", "Info Hash v1")}, {"hashV2", QT_TRANSLATE_NOOP("GeneralTab", "Info Hash v2")}
        , {"savePath", QT_TRANSLATE_NOOP("GeneralTab", "Save Path")}, {"comment", QT_TRANSLATE_NOOP("GeneralTab", "Comment")}
    };
}

GeneralTab::GeneralTab(DetailsSource *source, QWidget *parent)
    : DetailsTab(source, parent)
{
    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(22, 16, 22, 16);
    l->setSpacing(0);

    auto *nameRow = new QHBoxLayout;
    nameRow->setSpacing(8);
    m_name = new QLabel;
    m_name->setObjectName(QStringLiteral("dName"));
    m_name->setTextFormat(Qt::PlainText);
    m_name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_badge = new QLabel;
    m_badge->setObjectName(QStringLiteral("dBadge"));
    nameRow->addWidget(m_name, 0, Qt::AlignVCenter);
    nameRow->addWidget(m_badge, 0, Qt::AlignVCenter);
    nameRow->addStretch(1);
    l->addLayout(nameRow);
    l->addSpacing(12);

    auto *bars = new QGridLayout;
    bars->setHorizontalSpacing(12);
    bars->setVerticalSpacing(6);
    m_progressBar = new PiecesBar(PiecesBar::Kind::Progress);
    m_availabilityBar = new PiecesBar(PiecesBar::Kind::Availability);
    int row = 0;
    for (const auto &[label, bar] : {std::pair<QString, PiecesBar *> {tr("Progress"), m_progressBar}
                                     , std::pair<QString, PiecesBar *> {tr("Availability"), m_availabilityBar}})
    {
        QLabel *a = DetailsUi::fieldLabel(label);
        a->setFixedWidth(84);
        bars->addWidget(a, row, 0);
        bars->addWidget(bar, row, 1);
        ++row;
    }
    bars->setColumnStretch(1, 1);
    l->addLayout(bars);
    l->addSpacing(18);

    const auto makeGrid = [this](const Field *fields, const int count, const int cols)
    {
        auto *g = new QGridLayout;
        g->setHorizontalSpacing(24);
        g->setVerticalSpacing(14);
        for (int c = 0; c < cols; ++c)
            g->setColumnStretch(c, 1);
        for (int i = 0; i < count; ++i)
        {
            auto *cell = new QVBoxLayout;
            cell->setSpacing(4);
            cell->addWidget(DetailsUi::fieldLabel(tr(fields[i].label)));
            auto *value = new QLabel;
            value->setObjectName(QStringLiteral("dValue"));
            value->setTextFormat(Qt::PlainText);
            value->setTextInteractionFlags(Qt::TextSelectableByMouse);
            value->setWordWrap(cols == 1);
            cell->addWidget(value);
            g->addLayout(cell, i / cols, i % cols);
            m_values.insert(QString::fromLatin1(fields[i].key), value);
            m_order << QString::fromLatin1(fields[i].key);
        }
        return g;
    };

    l->addWidget(DetailsUi::heading(tr("Transfer")));
    l->addSpacing(10);
    l->addLayout(makeGrid(TRANSFER, static_cast<int>(std::size(TRANSFER)), 4));
    l->addSpacing(22);
    l->addWidget(DetailsUi::heading(tr("Information")));
    l->addSpacing(10);
    l->addLayout(makeGrid(INFORMATION, static_cast<int>(std::size(INFORMATION)), 4));
    l->addSpacing(14);
    l->addLayout(makeGrid(WIDE, static_cast<int>(std::size(WIDE)), 1));
    l->addStretch(1);

    connect(m_source, &DetailsSource::availabilityReady, this
            , [this](const QString &id, const QList<int> &pieces, const QList<double> &) { onAvailability(id, pieces); });
}

QString GeneralTab::value(const QString &key) const
{
    const QLabel *l = m_values.value(key);
    return l ? l->text() : QString();
}

void GeneralTab::setValue(const QString &key, const QString &text)
{
    QLabel *l = m_values.value(key);
    if (l && l->text() != text)
        l->setText(text);
}

void GeneralTab::onAvailability(const QString &id, const QList<int> &pieces)
{
    if (id == m_id)
        m_availabilityBar->setAvailability(pieces);
}

void GeneralTab::setTorrent(const QString &id)
{
    if (id != m_id)
    {
        m_availabilityBar->clear();
        m_progressBar->clear();
        m_tick = 0;
    }
    DetailsTab::setTorrent(id);
}

void GeneralTab::refresh()
{
    using namespace DetailsFormat;
    const Details::General g = m_source->general(m_id);
    if (!g.valid)
        return;

    m_name->setText(g.name);
    const QString code = QString::fromLatin1(RoStatusInfo::code(g.status));
    if (m_badge->property("status").toString() != code)
    {
        m_badge->setProperty("status", code);
        m_badge->style()->unpolish(m_badge);
        m_badge->style()->polish(m_badge);
    }
    m_badge->setText(statusText(g.status));

    m_progressBar->setPieces(g.pieces);
    // Availability is an asynchronous engine query; ask again every third refresh (about 4.5 s at the
    // default interval) and right after the torrent changed.
    if ((m_tick++ % 3) == 0)
        m_source->requestAvailability(m_id);

    const QString unlimited = QStringLiteral("∞");
    setValue(QStringLiteral("timeActive"), duration(g.timeActiveSecs));
    setValue(QStringLiteral("eta"), duration(g.etaSecs));
    setValue(QStringLiteral("connections"), (g.connectionsLimit > 0)
             ? tr("%1 (%2 max)").arg(g.connections).arg(g.connectionsLimit) : QString::number(g.connections));
    setValue(QStringLiteral("downloaded"), tr("%1 (%2 this session)").arg(bytes(g.downloaded), bytes(g.downloadedSession)));
    setValue(QStringLiteral("uploaded"), tr("%1 (%2 this session)").arg(bytes(g.uploaded), bytes(g.uploadedSession)));
    setValue(QStringLiteral("seeds"), tr("%1 (%2 total)").arg(g.seeds).arg(g.seedsTotal));
    setValue(QStringLiteral("downSpeed"), speed(g.downSpeed));
    setValue(QStringLiteral("upSpeed"), speed(g.upSpeed));
    setValue(QStringLiteral("peers"), tr("%1 (%2 total)").arg(g.peers).arg(g.peersTotal));
    setValue(QStringLiteral("downLimit"), limit(g.downLimit));
    setValue(QStringLiteral("upLimit"), limit(g.upLimit));
    setValue(QStringLiteral("wasted"), bytes(g.wasted));
    setValue(QStringLiteral("ratio"), ratio(g.ratio));
    setValue(QStringLiteral("reannounce"), (g.reannounceInSecs >= 0) ? duration(g.reannounceInSecs) : QStringLiteral("—"));
    setValue(QStringLiteral("lastSeen"), g.lastSeenComplete.isValid() ? dateTime(g.lastSeenComplete) : tr("Never"));
    setValue(QStringLiteral("popularity"), QString::number(g.popularity, 'f', 2));

    setValue(QStringLiteral("totalSize"), bytes(g.totalSize));
    setValue(QStringLiteral("pieces"), g.hasMetadata
             ? tr("%1 x %2 (have %3)").arg(g.piecesCount).arg(bytes(g.pieceLength)).arg(g.piecesHave) : QStringLiteral("—"));
    setValue(QStringLiteral("createdBy"), g.createdBy.isEmpty() ? QStringLiteral("—") : g.createdBy);
    setValue(QStringLiteral("addedOn"), dateTime(g.addedOn));
    setValue(QStringLiteral("completedOn"), dateTime(g.completedOn));
    setValue(QStringLiteral("createdOn"), dateTime(g.createdOn));
    setValue(QStringLiteral("private"), g.isPrivate ? tr("Yes") : tr("No"));
    setValue(QStringLiteral("hashV1"), g.hashV1.isEmpty() ? QStringLiteral("—") : g.hashV1);
    setValue(QStringLiteral("hashV2"), g.hashV2.isEmpty() ? QStringLiteral("—") : g.hashV2);
    setValue(QStringLiteral("savePath"), g.savePath.isEmpty() ? QStringLiteral("—") : g.savePath);
    setValue(QStringLiteral("comment"), g.comment.isEmpty() ? QStringLiteral("—") : g.comment);
}

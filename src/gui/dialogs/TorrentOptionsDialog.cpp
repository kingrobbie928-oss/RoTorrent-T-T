// SPDX-License-Identifier: GPL-2.0-or-later
// Licensed GPL-2.0-or-later.

#include "TorrentOptionsDialog.h"

#include <algorithm>
#include <memory>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "base/bittorrent/infohash.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/global.h"
#include "base/unicodestrings.h"

namespace
{
    constexpr int ROW_BLANK = -1;       // limit rows: values differ between the selected torrents
    constexpr int ROW_DEFAULT = 0;
    constexpr int ROW_UNLIMITED = 1;
    constexpr int ROW_SET = 2;
    constexpr int ENUM_BLANK = -99;     // mode / action combos: values differ

    using BitTorrent::ShareLimitAction;
    using BitTorrent::ShareLimitsMode;

    QString actionName(const ShareLimitAction action)
    {
        switch (action)
        {
        case ShareLimitAction::Stop:
            return QObject::tr("Pause torrent");
        case ShareLimitAction::Remove:
            return QObject::tr("Remove torrent");
        case ShareLimitAction::RemoveWithContent:
            return QObject::tr("Remove torrent and its content");
        case ShareLimitAction::EnableSuperSeeding:
            return QObject::tr("Enable super seeding for torrent");
        case ShareLimitAction::Default:
            return QObject::tr("Default");
        }
        return {};
    }

    QString modeName(const ShareLimitsMode mode)
    {
        switch (mode)
        {
        case ShareLimitsMode::MatchAny:
            return QObject::tr("Match any limit");
        case ShareLimitsMode::MatchAll:
            return QObject::tr("Match all the limits");
        case ShareLimitsMode::Default:
            return QObject::tr("Default");
        }
        return {};
    }

    void setByData(QComboBox *box, const int data)
    {
        const int index = box->findData(data);
        if (index >= 0)
            box->setCurrentIndex(index);
    }

    void addBlank(QComboBox *box, const int data)
    {
        if (box->findData(data) < 0)
            box->insertItem(0, QString(), data);
    }

    QCheckBox *makeCheck(const QString &name, const QString &text)
    {
        auto *box = new QCheckBox(text);
        box->setObjectName(name);
        return box;
    }

    void setTriState(QCheckBox *box, const bool same, const bool value)
    {
        if (same)
            box->setChecked(value);
        else
            box->setCheckState(Qt::PartiallyChecked);
    }

    void updateSliderValue(QSlider *slider, const int value)
    {
        if (value > slider->maximum())
            slider->setMaximum(value);
        slider->setValue(value);
    }
}

TorrentOptionsDialog::TorrentOptionsDialog(QWidget *parent, BitTorrent::Session *session, const QStringList &ids)
    : RoDialog(parent, tr("Torrent options"), 600)
    , m_session(session)
    , m_ids(ids)
    , m_mixedCategoryText(u"--%1--"_s.arg(tr("Currently used categories")))
{
    buildUi();
    loadValues();
}

bool TorrentOptionsDialog::show(QWidget *parent, BitTorrent::Session *session, const QStringList &ids)
{
    if (!session || ids.isEmpty())
        return false;
    TorrentOptionsDialog dialog(parent, session, ids);
    return dialog.run() == QDialog::Accepted;
}

void TorrentOptionsDialog::buildUi()
{
    auto *host = new QDialog;
    host->setObjectName(QStringLiteral("roUi"));
    host->setWindowFlags(Qt::Widget);
    auto *hostLayout = new QVBoxLayout(host);
    hostLayout->setContentsMargins(0, 0, 0, 14);
    hostLayout->setSpacing(0);

    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("optionsScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumHeight(560);
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("optionsPage"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 14, 18, 8);
    layout->setSpacing(10);
    scroll->setWidget(page);
    hostLayout->addWidget(scroll, 1);

    m_autoTMM = makeCheck(QStringLiteral("checkAutoTMM"), tr("Automatic Torrent Management"));
    m_autoTMM->setToolTip(tr("Automatic mode means that various torrent properties (e.g. save path) will be decided by the associated category"));
    layout->addWidget(m_autoTMM);

    // --- Save at ---
    auto *pathBox = new QGroupBox(tr("Save at"));
    pathBox->setObjectName(QStringLiteral("groupBoxSavePath"));
    m_pathGroup = pathBox;
    auto *pathLayout = new QVBoxLayout(pathBox);
    pathLayout->setSpacing(8);
    auto *saveRow = new QHBoxLayout;
    m_savePath = new QLineEdit;
    m_savePath->setObjectName(QStringLiteral("savePath"));
    m_saveBrowse = new QPushButton(tr("Browse..."));
    saveRow->addWidget(m_savePath, 1);
    saveRow->addWidget(m_saveBrowse);
    pathLayout->addLayout(saveRow);
    m_useDownloadPath = makeCheck(QStringLiteral("checkUseDownloadPath"), tr("Use another path for incomplete torrent"));
    pathLayout->addWidget(m_useDownloadPath);
    auto *incompleteRow = new QHBoxLayout;
    m_downloadPath = new QLineEdit;
    m_downloadPath->setObjectName(QStringLiteral("downloadPath"));
    m_downloadBrowse = new QPushButton(tr("Browse..."));
    incompleteRow->addWidget(m_downloadPath, 1);
    incompleteRow->addWidget(m_downloadBrowse);
    pathLayout->addLayout(incompleteRow);
    layout->addWidget(pathBox);
    connect(m_saveBrowse, &QPushButton::clicked, this, [this]
    {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), m_savePath->text());
        if (!dir.isEmpty())
            m_savePath->setText(dir);
    });
    connect(m_downloadBrowse, &QPushButton::clicked, this, [this]
    {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose save path"), m_downloadPath->text());
        if (!dir.isEmpty())
            m_downloadPath->setText(dir);
    });

    // --- Category ---
    auto *categoryRow = new QHBoxLayout;
    categoryRow->addWidget(new QLabel(tr("Category:")));
    m_category = new QComboBox;
    m_category->setObjectName(QStringLiteral("comboCategory"));
    m_category->setEditable(true);
    m_category->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    categoryRow->addWidget(m_category, 1);
    layout->addLayout(categoryRow);

    // --- Speed limits ---
    auto *speedBox = new QGroupBox(tr("Torrent Speed Limits"));
    auto *speedGrid = new QGridLayout(speedBox);
    speedGrid->setHorizontalSpacing(12);
    speedGrid->setVerticalSpacing(8);
    auto speedRow = [&](const int row, const QString &label, const QString &sliderName, const QString &spinName
                        , QSlider *&slider, QSpinBox *&spin)
    {
        speedGrid->addWidget(new QLabel(label), row, 0);
        slider = new QSlider(Qt::Horizontal);
        slider->setObjectName(sliderName);
        slider->setRange(0, 10000);
        spin = new QSpinBox;
        spin->setObjectName(spinName);
        spin->setRange(0, 2000000000);
        spin->setSpecialValueText(C_INFINITY);
        spin->setSuffix(tr(" KiB/s"));
        spin->setMinimumWidth(168);
        speedGrid->addWidget(slider, row, 1);
        speedGrid->addWidget(spin, row, 2);
    };
    speedRow(0, tr("Upload:"), QStringLiteral("sliderUploadLimit"), QStringLiteral("spinUploadLimit"), m_upSlider, m_upSpin);
    speedRow(1, tr("Download:"), QStringLiteral("sliderDownloadLimit"), QStringLiteral("spinDownloadLimit"), m_downSlider, m_downSpin);
    speedGrid->setColumnStretch(1, 1);
    layout->addWidget(speedBox);

    // --- Share limits ---
    auto *shareBox = new QGroupBox(tr("Torrent Share Limits"));
    auto *shareGrid = new QGridLayout(shareBox);
    shareGrid->setHorizontalSpacing(12);
    shareGrid->setVerticalSpacing(8);
    auto limitRow = [&](const int row, const QString &label, const QString &modeName, const QString &valueName
                        , const QString &suffix, LimitRow &out)
    {
        shareGrid->addWidget(new QLabel(label), row, 0);
        auto *mode = new QComboBox;
        mode->setObjectName(modeName);
        auto *value = new QDoubleSpinBox;
        value->setObjectName(valueName);
        shareGrid->addWidget(mode, row, 1);
        shareGrid->addWidget(value, row, 2);
        out = makeLimitRow(mode, value, suffix);
    };
    limitRow(0, tr("Ratio:"), QStringLiteral("comboRatioMode"), QStringLiteral("spinRatio"), {}, m_ratio);
    limitRow(1, tr("Seeding time:"), QStringLiteral("comboSeedingMode"), QStringLiteral("spinSeedingTime"), tr(" min"), m_seeding);
    limitRow(2, tr("Inactive seeding time:"), QStringLiteral("comboInactiveMode"), QStringLiteral("spinInactiveSeedingTime"), tr(" min"), m_inactive);

    shareGrid->addWidget(new QLabel(tr("Mode:")), 3, 0);
    m_limitsMode = new QComboBox;
    m_limitsMode->setObjectName(QStringLiteral("comboLimitsMode"));
    shareGrid->addWidget(m_limitsMode, 3, 1, 1, 2);
    shareGrid->addWidget(new QLabel(tr("Action when the limit is reached:")), 4, 0, 1, 2);
    m_limitAction = new QComboBox;
    m_limitAction->setObjectName(QStringLiteral("comboLimitAction"));
    shareGrid->addWidget(m_limitAction, 5, 0, 1, 3);
    shareGrid->setColumnStretch(1, 1);
    layout->addWidget(shareBox);

    // --- Peer sources and download order ---
    m_dht = makeCheck(QStringLiteral("checkDisableDHT"), tr("Disable DHT for this torrent"));
    m_pex = makeCheck(QStringLiteral("checkDisablePEX"), tr("Disable PeX for this torrent"));
    m_lsd = makeCheck(QStringLiteral("checkDisableLSD"), tr("Disable LSD for this torrent"));
    m_sequential = makeCheck(QStringLiteral("checkSequential"), tr("Download in sequential order"));
    m_firstLast = makeCheck(QStringLiteral("checkFirstLastPieces"), tr("Download first and last pieces first"));
    for (QCheckBox *box : {m_dht, m_pex, m_lsd, m_sequential, m_firstLast})
        layout->addWidget(box);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setProperty("primary", true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(18, 8, 18, 0);
    buttonRow->addWidget(buttons);
    hostLayout->addLayout(buttonRow);

    setContent(host);
}

TorrentOptionsDialog::LimitRow TorrentOptionsDialog::makeLimitRow(QComboBox *mode, QDoubleSpinBox *value, const QString &suffix)
{
    LimitRow row;
    row.mode = mode;
    row.value = value;
    row.suffix = suffix;
    row.previous = ROW_DEFAULT;
    mode->addItem(tr("Default"), ROW_DEFAULT);
    mode->addItem(tr("Unlimited"), ROW_UNLIMITED);
    mode->addItem(tr("Set to"), ROW_SET);
    value->setDecimals(suffix.isEmpty() ? 2 : 0);
    value->setSingleStep(suffix.isEmpty() ? 0.05 : 1);
    value->setRange(0, suffix.isEmpty() ? 9999 : 525600);
    value->setEnabled(false);
    value->clear();
    return row;
}

void TorrentOptionsDialog::showRowDefault(LimitRow &row)
{
    if (row.defaultValue >= 0)
    {
        row.value->setSuffix(row.suffix);
        row.value->setValue(row.defaultValue);
    }
    else
    {
        row.value->setSuffix({});
        row.value->clear();
    }
}

void TorrentOptionsDialog::onLimitModeChanged(LimitRow &row)
{
    const int current = row.mode->currentData().toInt();
    row.value->setEnabled(current == ROW_SET);
    if (row.previous == ROW_SET)
        row.stored = row.value->value();

    if (current == ROW_SET)
    {
        row.value->setSuffix(row.suffix);
        row.value->setValue(row.stored);
    }
    else if (current == ROW_DEFAULT)
    {
        showRowDefault(row);
    }
    else
    {
        row.value->setSuffix({});
        row.value->clear();
    }
    row.previous = current;
}

void TorrentOptionsDialog::setLimitRow(LimitRow &row, const double limit, const double defaultMarker
                                       , const double noLimitMarker, const bool same)
{
    if (!same)
    {
        addBlank(row.mode, ROW_BLANK);
        setByData(row.mode, ROW_BLANK);
        return;
    }
    if (limit == defaultMarker)
    {
        setByData(row.mode, ROW_DEFAULT);
    }
    else if (limit == noLimitMarker)
    {
        setByData(row.mode, ROW_UNLIMITED);
    }
    else
    {
        row.stored = limit;
        setByData(row.mode, ROW_SET);
        row.value->setValue(limit);
    }
}

double TorrentOptionsDialog::limitRowValue(const LimitRow &row, const double defaultMarker, const double noLimitMarker) const
{
    switch (row.mode->currentData().toInt())
    {
    case ROW_DEFAULT:
        return defaultMarker;
    case ROW_UNLIMITED:
        return noLimitMarker;
    case ROW_SET:
        return row.value->value();
    default:
        return defaultMarker;   // blank: callers check the combo first and never write it
    }
}

void TorrentOptionsDialog::loadValues()
{
    using namespace BitTorrent;
    const QList<Torrent *> torrents = [this]
    {
        QList<Torrent *> out;
        for (const QString &id : asConst(m_ids))
        {
            const auto tid = TorrentID::fromString(id);
            if (Torrent *t = tid.isValid() ? m_session->getTorrent(tid) : nullptr)
                out.append(t);
        }
        return out;
    }();
    Q_ASSERT(!torrents.isEmpty());
    if (torrents.isEmpty())
        return;

    const Torrent *first = torrents.first();
    bool sameTMM = true, sameSave = true, sameDownload = true, sameUp = true, sameDown = true;
    bool sameRatio = true, sameSeed = true, sameInactive = true, sameMode = true, sameAction = true;
    bool allPrivate = true, sameDHT = true, samePEX = true, sameLSD = true, sameSeq = true, sameFL = true;
    const int firstUp = std::max(0, first->uploadLimit());
    const int firstDown = std::max(0, first->downloadLimit());
    const ShareLimits firstLimits = first->shareLimits();
    for (const Torrent *t : torrents)
    {
        m_allSameCategory &= (t->category() == first->category());
        sameTMM &= (t->isAutoTMMEnabled() == first->isAutoTMMEnabled());
        sameSave &= (t->savePath() == first->savePath());
        sameDownload &= (t->downloadPath() == first->downloadPath());
        sameUp &= (std::max(0, t->uploadLimit()) == firstUp);
        sameDown &= (std::max(0, t->downloadLimit()) == firstDown);
        const ShareLimits &l = t->shareLimits();
        sameRatio &= (l.ratioLimit == firstLimits.ratioLimit);
        sameSeed &= (l.seedingTimeLimit == firstLimits.seedingTimeLimit);
        sameInactive &= (l.inactiveSeedingTimeLimit == firstLimits.inactiveSeedingTimeLimit);
        sameMode &= (l.mode == firstLimits.mode);
        sameAction &= (l.action == firstLimits.action);
        allPrivate &= t->isPrivate();
        sameDHT &= (t->isDHTDisabled() == first->isDHTDisabled());
        samePEX &= (t->isPEXDisabled() == first->isPEXDisabled());
        sameLSD &= (t->isLSDDisabled() == first->isLSDDisabled());
        sameSeq &= (t->isSequentialDownload() == first->isSequentialDownload());
        sameFL &= (t->hasFirstLastPiecePriority() == first->hasFirstLastPiecePriority());
    }

    // automatic management and paths
    setTriState(m_autoTMM, sameTMM, first->isAutoTMMEnabled());
    if (sameSave)
        m_savePath->setText(first->savePath().toString());
    if (sameDownload)
    {
        m_downloadPath->setText(first->downloadPath().toString());
        m_useDownloadPath->setChecked(!first->downloadPath().isEmpty());
    }
    else
    {
        m_useDownloadPath->setCheckState(Qt::PartiallyChecked);
    }

    // category: index 0 is always "the current one", as upstream
    const QString firstCategory = first->category();
    if (!m_allSameCategory)
    {
        m_category->addItem(m_mixedCategoryText);
        m_category->clearEditText();
        m_category->lineEdit()->setPlaceholderText(m_mixedCategoryText);
    }
    else if (!firstCategory.isEmpty())
    {
        m_category->setCurrentText(firstCategory);
        m_category->addItem(firstCategory);
    }
    m_category->addItem(QString());
    m_categories = m_session->categories();
    std::ranges::sort(m_categories, [](const QString &a, const QString &b) { return a.compare(b, Qt::CaseInsensitive) < 0; });
    for (const QString &category : asConst(m_categories))
    {
        if (m_allSameCategory && (category == firstCategory))
            continue;
        m_category->addItem(category);
    }

    // speed limits (KiB/s), slider range follows the active global limit
    const bool alt = m_session->isAltGlobalSpeedLimitEnabled();
    const int globalUp = (alt ? m_session->altGlobalUploadSpeedLimit() : m_session->globalUploadSpeedLimit()) / 1024;
    const int globalDown = (alt ? m_session->altGlobalDownloadSpeedLimit() : m_session->globalDownloadSpeedLimit()) / 1024;
    const int upVal = firstUp / 1024;
    const int downVal = firstDown / 1024;
    const int maxUp = std::max((globalUp <= 0) ? 10000 : globalUp, upVal);
    const int maxDown = std::max((globalDown <= 0) ? 10000 : globalDown, downVal);
    auto setupSpeed = [this](QSlider *slider, QSpinBox *spin, const int maximum, const int value, const bool same)
    {
        slider->setMaximum(maximum);
        slider->setValue(same ? value : (maximum / 2));
        if (same)
        {
            spin->setValue(value);
            return;
        }
        spin->setSpecialValueText(C_INEQUALITY);
        spin->setMinimum(-1);
        spin->setValue(-1);
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(spin, qOverload<int>(&QSpinBox::valueChanged), spin, [spin, conn]
        {
            spin->setMinimum(0);
            spin->setSpecialValueText(C_INFINITY);
            QObject::disconnect(*conn);
        });
    };
    setupSpeed(m_upSlider, m_upSpin, maxUp, upVal, sameUp);
    setupSpeed(m_downSlider, m_downSpin, maxDown, downVal, sameDown);

    // share limits
    m_defaultsFromCategory = m_allSameCategory && !firstCategory.isEmpty();
    for (LimitRow *row : {&m_ratio, &m_seeding, &m_inactive})
    {
        connect(row->mode, &QComboBox::currentIndexChanged, this, [this, row] { onLimitModeChanged(*row); });
    }
    m_limitsMode->addItem(tr("Default"), static_cast<int>(ShareLimitsMode::Default));
    m_limitsMode->addItem(modeName(ShareLimitsMode::MatchAny), static_cast<int>(ShareLimitsMode::MatchAny));
    m_limitsMode->addItem(modeName(ShareLimitsMode::MatchAll), static_cast<int>(ShareLimitsMode::MatchAll));
    for (const ShareLimitAction a : {ShareLimitAction::Default, ShareLimitAction::Stop, ShareLimitAction::Remove
                                     , ShareLimitAction::RemoveWithContent, ShareLimitAction::EnableSuperSeeding})
        m_limitAction->addItem(actionName(a), static_cast<int>(a));

    refreshCategoryDefaults();
    setLimitRow(m_ratio, firstLimits.ratioLimit, DEFAULT_RATIO_LIMIT, NO_RATIO_LIMIT, sameRatio);
    setLimitRow(m_seeding, firstLimits.seedingTimeLimit, DEFAULT_SEEDING_TIME_LIMIT, NO_SEEDING_TIME_LIMIT, sameSeed);
    setLimitRow(m_inactive, firstLimits.inactiveSeedingTimeLimit, DEFAULT_SEEDING_TIME_LIMIT, NO_SEEDING_TIME_LIMIT, sameInactive);
    if (sameMode)
    {
        setByData(m_limitsMode, static_cast<int>(firstLimits.mode));
    }
    else
    {
        addBlank(m_limitsMode, ENUM_BLANK);
        setByData(m_limitsMode, ENUM_BLANK);
    }
    if (sameAction)
    {
        setByData(m_limitAction, static_cast<int>(firstLimits.action));
    }
    else
    {
        addBlank(m_limitAction, ENUM_BLANK);
        setByData(m_limitAction, ENUM_BLANK);
    }

    // peer sources: not applicable to private torrents
    if (!allPrivate)
    {
        setTriState(m_dht, sameDHT, first->isDHTDisabled());
        setTriState(m_pex, samePEX, first->isPEXDisabled());
        setTriState(m_lsd, sameLSD, first->isLSDDisabled());
    }
    else
    {
        for (QCheckBox *box : {m_dht, m_pex, m_lsd})
        {
            box->setChecked(true);
            box->setEnabled(false);
        }
    }
    const QString privateTip = tr("Not applicable to private torrents");
    for (QCheckBox *box : {m_dht, m_pex, m_lsd})
        box->setToolTip(privateTip);
    setTriState(m_sequential, sameSeq, first->isSequentialDownload());
    setTriState(m_firstLast, sameFL, first->hasFirstLastPiecePriority());

    // remember what the dialog showed, so accept() writes only what the user changed
    m_initial.savePath = Path(m_savePath->text());
    m_initial.downloadPath = Path(m_downloadPath->text());
    m_initial.category = m_category->currentText();
    m_initial.ratioMode = m_ratio.mode->currentData().toInt();
    m_initial.ratioValue = m_ratio.value->value();
    m_initial.seedingMode = m_seeding.mode->currentData().toInt();
    m_initial.seedingValue = m_seeding.value->value();
    m_initial.inactiveMode = m_inactive.mode->currentData().toInt();
    m_initial.inactiveValue = m_inactive.value->value();
    m_initial.limitsMode = m_limitsMode->currentData().toInt();
    m_initial.action = m_limitAction->currentData().toInt();
    m_initial.upLimit = m_upSpin->value();
    m_initial.downLimit = m_downSpin->value();
    m_initial.autoTMM = m_autoTMM->checkState();
    m_initial.useDownloadPath = m_useDownloadPath->checkState();
    m_initial.dht = m_dht->checkState();
    m_initial.pex = m_pex->checkState();
    m_initial.lsd = m_lsd->checkState();
    m_initial.sequential = m_sequential->checkState();
    m_initial.firstLast = m_firstLast->checkState();

    onAutoTMMChanged();
    onUseDownloadPathChanged();
    connect(m_autoTMM, &QCheckBox::clicked, this, &TorrentOptionsDialog::onAutoTMMChanged);
    connect(m_useDownloadPath, &QCheckBox::clicked, this, &TorrentOptionsDialog::onUseDownloadPathChanged);
    connect(m_category, &QComboBox::activated, this, &TorrentOptionsDialog::onCategoryActivated);

    connect(m_upSlider, &QSlider::valueChanged, m_upSpin, &QSpinBox::setValue);
    connect(m_downSlider, &QSlider::valueChanged, m_downSpin, &QSpinBox::setValue);
    connect(m_upSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](const int v) { updateSliderValue(m_upSlider, v); });
    connect(m_downSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](const int v) { updateSliderValue(m_downSlider, v); });
}

// What "Default" means for the share limit rows: the global setting, or the category's when one is shared.
void TorrentOptionsDialog::refreshCategoryDefaults()
{
    const bool mixedCurrent = !m_allSameCategory && (m_category->currentIndex() == 0);
    if (mixedCurrent)
    {
        m_defaultsFromCategory = false;
        m_defaultLimits = {};
    }
    else
    {
        const QString name = m_category->currentText();
        m_defaultsFromCategory = !name.isEmpty();
        m_defaultLimits = m_session->categoryShareLimits(name);
    }
    m_ratio.defaultValue = (m_defaultLimits.ratioLimit >= 0) ? m_defaultLimits.ratioLimit : -1;
    m_seeding.defaultValue = (m_defaultLimits.seedingTimeLimit >= 0) ? m_defaultLimits.seedingTimeLimit : -1;
    m_inactive.defaultValue = (m_defaultLimits.inactiveSeedingTimeLimit >= 0) ? m_defaultLimits.inactiveSeedingTimeLimit : -1;
    for (LimitRow *row : {&m_ratio, &m_seeding, &m_inactive})
    {
        if (row->mode->currentData().toInt() == ROW_DEFAULT)
            showRowDefault(*row);
    }
    refreshDefaultTexts();
}

void TorrentOptionsDialog::refreshDefaultTexts()
{
    const QString base = m_defaultsFromCategory ? tr("From category") : tr("Default");
    for (LimitRow *row : {&m_ratio, &m_seeding, &m_inactive})
    {
        const int index = row->mode->findData(ROW_DEFAULT);
        if (index >= 0)
            row->mode->setItemText(index, base);
    }
    auto withSuffix = [&](const QString &name, const bool isDefault)
    {
        return isDefault ? base : (m_defaultsFromCategory ? tr("From category (%1)") : tr("Default (%1)")).arg(name);
    };
    if (const int i = m_limitAction->findData(static_cast<int>(ShareLimitAction::Default)); i >= 0)
        m_limitAction->setItemText(i, withSuffix(actionName(m_defaultLimits.action), m_defaultLimits.action == ShareLimitAction::Default));
    if (const int i = m_limitsMode->findData(static_cast<int>(ShareLimitsMode::Default)); i >= 0)
        m_limitsMode->setItemText(i, withSuffix(modeName(m_defaultLimits.mode), m_defaultLimits.mode == ShareLimitsMode::Default));
}

void TorrentOptionsDialog::onCategoryActivated()
{
    if (!m_allSameCategory && (m_category->currentIndex() == 0))
    {
        if (m_autoTMM->checkState() == Qt::Checked)
            m_savePath->clear();
        m_category->clearEditText();
        m_category->lineEdit()->setPlaceholderText(m_mixedCategoryText);
    }
    else
    {
        const QString name = m_category->currentText();
        if (m_autoTMM->checkState() == Qt::Checked)
        {
            m_savePath->setText(m_session->categorySavePath(name).toString());
            const Path download = m_session->categoryDownloadPath(name);
            m_downloadPath->setText(download.toString());
            m_useDownloadPath->setChecked(!download.isEmpty());
        }
        m_category->lineEdit()->setPlaceholderText({});
    }
    refreshCategoryDefaults();
}

void TorrentOptionsDialog::onAutoTMMChanged()
{
    const Qt::CheckState state = m_autoTMM->checkState();
    if (state == Qt::Unchecked)
    {
        m_pathGroup->setEnabled(true);
        m_savePath->setText(m_initial.savePath.toString());
        m_downloadPath->setText(m_initial.downloadPath.toString());
        m_useDownloadPath->setCheckState(m_initial.useDownloadPath);
    }
    else
    {
        m_pathGroup->setEnabled(false);
        if (state == Qt::Checked && (m_allSameCategory || m_category->currentIndex() != 0))
        {
            const QString name = m_category->currentText();
            m_savePath->setText(m_session->categorySavePath(name).toString());
            const Path download = m_session->categoryDownloadPath(name);
            m_downloadPath->setText(download.toString());
            m_useDownloadPath->setChecked(!download.isEmpty());
        }
        else
        {
            m_savePath->clear();
            m_downloadPath->clear();
            m_useDownloadPath->setCheckState(Qt::PartiallyChecked);
        }
    }
    onUseDownloadPathChanged();
}

void TorrentOptionsDialog::onUseDownloadPathChanged()
{
    const bool checked = (m_useDownloadPath->checkState() == Qt::Checked);
    m_downloadPath->setEnabled(checked);
    m_downloadBrowse->setEnabled(checked && m_pathGroup->isEnabled());
    m_saveBrowse->setEnabled(m_pathGroup->isEnabled());
    if (checked && m_downloadPath->text().isEmpty())
        m_downloadPath->setText(m_session->downloadPath().toString());
}

void TorrentOptionsDialog::accept()
{
    using namespace BitTorrent;
    for (const QString &id : asConst(m_ids))
    {
        const auto tid = TorrentID::fromString(id);
        Torrent *torrent = tid.isValid() ? m_session->getTorrent(tid) : nullptr;
        if (!torrent)
            continue;

        if (m_initial.autoTMM != m_autoTMM->checkState())
            torrent->setAutoTMMEnabled(m_autoTMM->isChecked());

        if (m_autoTMM->checkState() == Qt::Unchecked)
        {
            const Path savePath(m_savePath->text());
            if (!m_savePath->text().isEmpty() && (m_initial.savePath != savePath))
                torrent->setSavePath(savePath);

            const Qt::CheckState useDownload = m_useDownloadPath->checkState();
            if (useDownload == Qt::Checked)
            {
                const Path downloadPath(m_downloadPath->text());
                if (m_initial.downloadPath != downloadPath)
                    torrent->setDownloadPath(downloadPath);
            }
            else if (useDownload == Qt::Unchecked)
            {
                torrent->setDownloadPath({});
            }
        }

        const QString category = m_category->currentText();
        // index 0 is always the current category (upstream rule)
        if ((m_initial.category != category) || (m_category->currentIndex() != 0))
        {
            if (!category.isEmpty() && !m_categories.contains(category))
                m_session->addCategory(category);
            torrent->setCategory(category);
        }

        if (m_initial.upLimit != m_upSpin->value())
            torrent->setUploadLimit(m_upSpin->value() * 1024);
        if (m_initial.downLimit != m_downSpin->value())
            torrent->setDownloadLimit(m_downSpin->value() * 1024);

        ShareLimits limits = torrent->shareLimits();
        auto rowChanged = [](const LimitRow &row, const int initialMode, const double initialValue)
        {
            const int mode = row.mode->currentData().toInt();
            return (mode != ROW_BLANK) && ((mode != initialMode) || ((mode == ROW_SET) && (row.value->value() != initialValue)));
        };
        if (rowChanged(m_ratio, m_initial.ratioMode, m_initial.ratioValue))
            limits.ratioLimit = limitRowValue(m_ratio, DEFAULT_RATIO_LIMIT, NO_RATIO_LIMIT);
        if (rowChanged(m_seeding, m_initial.seedingMode, m_initial.seedingValue))
            limits.seedingTimeLimit = qRound(limitRowValue(m_seeding, DEFAULT_SEEDING_TIME_LIMIT, NO_SEEDING_TIME_LIMIT));
        if (rowChanged(m_inactive, m_initial.inactiveMode, m_initial.inactiveValue))
            limits.inactiveSeedingTimeLimit = qRound(limitRowValue(m_inactive, DEFAULT_SEEDING_TIME_LIMIT, NO_SEEDING_TIME_LIMIT));
        if (const int mode = m_limitsMode->currentData().toInt(); (mode != ENUM_BLANK) && (mode != m_initial.limitsMode))
            limits.mode = static_cast<ShareLimitsMode>(mode);
        if (const int action = m_limitAction->currentData().toInt(); (action != ENUM_BLANK) && (action != m_initial.action))
            limits.action = static_cast<ShareLimitAction>(action);
        torrent->setShareLimits(limits);

        if (!torrent->isPrivate())
        {
            if (m_initial.dht != m_dht->checkState())
                torrent->setDHTDisabled(m_dht->isChecked());
            if (m_initial.pex != m_pex->checkState())
                torrent->setPEXDisabled(m_pex->isChecked());
            if (m_initial.lsd != m_lsd->checkState())
                torrent->setLSDDisabled(m_lsd->isChecked());
        }
        if (m_initial.sequential != m_sequential->checkState())
            torrent->setSequentialDownload(m_sequential->isChecked());
        if (m_initial.firstLast != m_firstLast->checkState())
            torrent->setFirstLastPiecePriority(m_firstLast->isChecked());
    }
    RoDialog::accept();
}

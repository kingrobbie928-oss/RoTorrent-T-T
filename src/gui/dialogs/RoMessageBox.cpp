// SPDX-License-Identifier: GPL-2.0-or-later
#include "RoMessageBox.h"

#include <QCheckBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "RoDialog.h"
#include "theme/RoTheme.h"

RoMessageBox::Result RoMessageBox::show(QWidget *parent, const QString &title, const QString &text, const Options &options)
{
    const RoPalette &P = RoTheme::instance().pal();
    RoDialog dialog(parent, title, options.width);

    auto *host = new QDialog;
    host->setObjectName(QStringLiteral("roUi"));
    host->setWindowFlags(Qt::Widget);
    auto *v = new QVBoxLayout(host);
    v->setContentsMargins(12, 14, 12, 12);
    v->setSpacing(10);

    auto *top = new QHBoxLayout;
    top->setSpacing(6);
    top->setContentsMargins(0, 0, 0, 0);
    QString iconName;
    QColor iconColour;
    switch (options.kind)
    {
    case Kind::Info: iconName = QStringLiteral("info"); iconColour = P.info; break;
    case Kind::Question: iconName = QStringLiteral("m-msg"); iconColour = P.info; break;
    case Kind::Warning: iconName = QStringLiteral("m-warn"); iconColour = P.check; break;
    case Kind::Critical: iconName = QStringLiteral("m-crit"); iconColour = P.accent; break;
    case Kind::None: break;
    }
    if (!iconName.isEmpty())
    {
        auto *icon = new QLabel;
        icon->setPixmap(RoTheme::instance().iconPx(iconName, iconColour, 28));
        icon->setFixedSize(28, 28);
        icon->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        top->addWidget(icon, 0, Qt::AlignTop);
    }
    auto *column = new QVBoxLayout;
    column->setSpacing(6);
    top->addLayout(column, 1);
    auto *message = new QLabel(text);
    message->setWordWrap(true);
    message->setTextInteractionFlags(Qt::TextSelectableByMouse);
    column->addWidget(message);
    if (!options.detail.isEmpty())
    {
        auto *detail = new QLabel(options.detail);
        detail->setWordWrap(true);
        detail->setStyleSheet(QStringLiteral("color:%1;").arg(P.muted.name()));
        column->addWidget(detail);
    }
    v->addLayout(top);

    QCheckBox *check = nullptr;
    if (!options.checkText.isEmpty())
    {
        check = new QCheckBox(options.checkText);
        check->setChecked(options.checkInitial);
        QFont italic = check->font();
        italic.setItalic(true);   // the kit's deletion dialog sets the label italic
        check->setFont(italic);
        auto *checkRow = new QHBoxLayout;
        checkRow->setContentsMargins(options.kind == Kind::None ? 0 : 46, 0, 0, 0);   // under the text column, indented 12 px
        checkRow->addWidget(check);
        v->addLayout(checkRow);
    }

    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    row->addStretch(1);
    v->addLayout(row);

    Result result;
    const QString primary = options.primary.isEmpty() ? options.buttons.last() : options.primary;
    const QString defaultLabel = options.defaultButton.isEmpty() ? primary : options.defaultButton;
    for (const QString &label : options.buttons)
    {
        auto *button = new QPushButton(label);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumWidth(84);
        if (label == primary)
            button->setProperty("primary", true);
        button->setDefault(label == defaultLabel);
        if (label == defaultLabel)
            button->setFocus();
        QObject::connect(button, &QPushButton::clicked, &dialog, [&result, &dialog, check, label]
        {
            result.label = label;
            result.checked = check && check->isChecked();
            dialog.accept();
        });
        row->addWidget(button);
    }
    dialog.setContent(host);
    dialog.run();
    return result;
}

bool RoMessageBox::confirm(QWidget *parent, const QString &title, const QString &text
                           , const QString &confirmLabel, const QString &detail)
{
    Options o;
    o.kind = Kind::Question;
    o.buttons = {confirmLabel, QObject::tr("Cancel")};   // primary first, as in the kit
    o.primary = confirmLabel;
    o.defaultButton = QObject::tr("Cancel");
    o.detail = detail;
    return show(parent, title, text, o).label == confirmLabel;
}

void RoMessageBox::error(QWidget *parent, const QString &title, const QString &text)
{
    Options o;
    o.kind = Kind::Critical;
    show(parent, title, text, o);
}

void RoMessageBox::info(QWidget *parent, const QString &title, const QString &text)
{
    show(parent, title, text, Options {});
}

void RoMessageBox::warning(QWidget *parent, const QString &title, const QString &text)
{
    Options o;
    o.kind = Kind::Warning;
    show(parent, title, text, o);
}

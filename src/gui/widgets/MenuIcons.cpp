// SPDX-License-Identifier: GPL-2.0-or-later
#include "MenuIcons.h"

#include <QAction>
#include <QActionGroup>
#include <QGuiApplication>
#include <QPainter>
#include <QPen>
#include <QScreen>

#include "theme/RoTheme.h"

namespace
{
    qreal dpr()
    {
        const QScreen *s = QGuiApplication::primaryScreen();
        return s ? s->devicePixelRatio() : 1.0;
    }

    QPixmap compose(const QString &name, const QColor &glyph, const QColor &boxColour, const QColor &accent
                    , const RoMenu::Box box, const bool on)
    {
        constexpr int S = 34;
        const qreal d = dpr();
        QPixmap pm(qRound(S * d), qRound(S * d));
        pm.fill(Qt::transparent);
        pm.setDevicePixelRatio(d);
        QPainter p(&pm);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        constexpr int B = 13;
        const int x = 1;
        const int y = (S - B) / 2;
        if (box == RoMenu::Box::Check)
        {
            if (on)
            {
                p.setPen(Qt::NoPen);
                p.setBrush(accent);
                p.drawRoundedRect(QRectF(x, y, B, B), 4, 4);
                p.setPen(QPen(Qt::white, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p.setBrush(Qt::NoBrush);
                const QPointF tick[] = {{x + B * .26, y + B * .53}, {x + B * .42, y + B * .69}, {x + B * .75, y + B * .33}};
                p.drawPolyline(tick, 3);
            }
            else
            {
                p.setPen(QPen(boxColour, 1.4));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(QRectF(x + .75, y + .75, B - 1.5, B - 1.5), 3.5, 3.5);
            }
        }
        else if (box == RoMenu::Box::Radio)
        {
            p.setPen(QPen(on ? accent : boxColour, 1.4));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QRectF(x + .75, y + .75, B - 1.5, B - 1.5));
            if (on)
            {
                p.setPen(Qt::NoPen);
                p.setBrush(accent);
                p.drawEllipse(QRectF(x + B / 2.0 - 2.6, y + B / 2.0 - 2.6, 5.2, 5.2));
            }
        }
        if (!name.isEmpty())
            p.drawPixmap(QPointF(18, (S - 16) / 2), RoTheme::instance().iconPx(name, glyph, 16));
        p.end();
        return pm;
    }
}

QIcon RoMenu::icon(const QString &name, const int px)
{
    const RoPalette &P = RoTheme::instance().pal();
    const RoTheme &t = RoTheme::instance();
    QIcon ic;
    const QPixmap active = t.iconPx(name, P.text, px);
    ic.addPixmap(t.iconPx(name, P.muted, px), QIcon::Normal);
    ic.addPixmap(active, QIcon::Active);
    ic.addPixmap(active, QIcon::Selected);
    ic.addPixmap(t.iconPx(name, P.faint, px), QIcon::Disabled);
    return ic;
}

QIcon RoMenu::wideIcon(const QString &name, const Box box, const bool on)
{
    const RoPalette &P = RoTheme::instance().pal();
    QIcon ic;
    ic.addPixmap(compose(name, P.muted, P.muted, P.accent, box, on), QIcon::Normal);
    const QPixmap active = compose(name, P.text, P.text, P.accent, box, on);
    ic.addPixmap(active, QIcon::Active);
    ic.addPixmap(active, QIcon::Selected);
    ic.addPixmap(compose(name, P.faint, P.faint, P.faint, box, on), QIcon::Disabled);
    return ic;
}

void RoMenu::bind(QAction *action, const QString &name, const bool wide)
{
    const auto refresh = [action, name, wide]
    {
        if (!wide)
        {
            action->setIcon(icon(name));
            return;
        }
        if (!action->isCheckable())
        {
            action->setIcon(wideIcon(name, Box::None, false));
            return;
        }
        const QActionGroup *group = action->actionGroup();
        const Box kind = (group && group->isExclusive()) ? Box::Radio : Box::Check;
        action->setIcon(wideIcon(name, kind, action->isChecked()));
    };
    refresh();
    if (action->isCheckable())
        QObject::connect(action, &QAction::toggled, action, refresh);
    QObject::connect(&RoTheme::instance(), &RoTheme::changed, action, refresh);
}

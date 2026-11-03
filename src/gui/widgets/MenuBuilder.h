// SPDX-License-Identifier: GPL-2.0-or-later
// Small helper for building the themed popup menus (handoff 8.4 style): every item has an icon,
// menus with checkable items reserve the 34 px [box | icon] column (property checks="true").
// Licensed GPL-2.0-or-later.
#pragma once

#include <functional>

#include <QAction>
#include <QMenu>

#include "MenuIcons.h"

class MenuBuilder
{
public:
    MenuBuilder(QMenu *menu, const bool wide)
        : m_menu(menu)
        , m_wide(wide)
    {
        menu->setProperty("checks", wide);
        menu->setToolTipsVisible(true);
    }

    QMenu *menu() const { return m_menu; }

    QAction *add(const QString &text, const QString &icon, std::function<void()> fn
                 , const bool enabled = true, const QString &tip = {})
    {
        auto *a = new QAction(text, m_menu);
        RoMenu::bind(a, icon, m_wide);
        a->setEnabled(enabled);
        if (!tip.isEmpty())
            a->setToolTip(tip);
        if (fn)
            QObject::connect(a, &QAction::triggered, a, [fn = std::move(fn)] { fn(); });
        m_menu->addAction(a);
        return a;
    }

    QAction *addCheck(const QString &text, const QString &icon, const bool checked, std::function<void(bool)> fn
                      , const bool enabled = true, const QString &tip = {})
    {
        auto *a = new QAction(text, m_menu);
        a->setCheckable(true);
        a->setChecked(checked);
        RoMenu::bind(a, icon, m_wide);
        a->setEnabled(enabled);
        if (!tip.isEmpty())
            a->setToolTip(tip);
        QObject::connect(a, &QAction::toggled, a, [fn = std::move(fn)](const bool on) { fn(on); });
        m_menu->addAction(a);
        return a;
    }

    MenuBuilder sub(const QString &title, const QString &icon)
    {
        auto *sm = new QMenu(title, m_menu);
        RoMenu::bind(sm->menuAction(), icon, m_wide);
        m_menu->addMenu(sm);
        return MenuBuilder(sm, m_wide);
    }

    void separator() { m_menu->addSeparator(); }

private:
    QMenu *m_menu;
    bool m_wide;
};

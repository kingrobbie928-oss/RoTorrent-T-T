// SPDX-License-Identifier: GPL-2.0-or-later
// Frameless main window with the RoTorrent title row (handoff 8.3):
//   45 px row = 44 px content + 1 px Hairline; logo, menu bar chips, stretch, three caption buttons.
//   System move / resize via QWindow, 6 px resize grab zone, minimum size 300 x 160,
//   Windows Snap through RoWinSnap.
#pragma once

#include <QMainWindow>

class QLabel;
class QMenuBar;
class QMouseEvent;
class RoHoverIcon;

class RoFramelessWindow : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RoFramelessWindow)

public:
    explicit RoFramelessWindow(QWidget *parent = nullptr);
    ~RoFramelessWindow() override;

    QMenuBar *titleMenuBar() const { return m_menuBar; }
    void toggleMaximized();

    static constexpr int TITLE_ROW_HEIGHT = 45;
    static constexpr int CAPTION_BUTTON_WIDTH = 46;
    static constexpr int RESIZE_EDGE = 6;
    static constexpr int MIN_WIDTH = 300;
    static constexpr int MIN_HEIGHT = 160;

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void rebuildLogo();
    Qt::Edges edgesAt(const QPoint &globalPos) const;
    void setEdgeCursor(Qt::Edges edges);
    void clearEdgeCursor();
    bool isTitleDragArea(const QObject *obj) const;
    void updateMaximizeButton();

    QWidget *m_menuWrap = nullptr;
    QMenuBar *m_menuBar = nullptr;
    QLabel *m_logo = nullptr;
    RoHoverIcon *m_minButton = nullptr;
    RoHoverIcon *m_maxButton = nullptr;
    RoHoverIcon *m_closeButton = nullptr;
    bool m_edgeCursorActive = false;
    bool m_snapInitialised = false;
};

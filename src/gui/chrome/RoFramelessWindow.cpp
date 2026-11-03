// SPDX-License-Identifier: GPL-2.0-or-later
#include "RoFramelessWindow.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QWindow>

#include <optional>

#include "Hairline.h"
#include "RoHoverIcon.h"
#include "theme/RoTheme.h"
#include "winsnap.h"

RoFramelessWindow::RoFramelessWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("RoTorrent"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);   // no OS title bar
    setMinimumSize(MIN_WIDTH, MIN_HEIGHT);
    resize(1280, 800);

    m_menuWrap = new QWidget;
    m_menuWrap->setObjectName(QStringLiteral("menuWrap"));
    auto *row = new QHBoxLayout(m_menuWrap);
    // Left 14 px to the logo; buttons are exactly 44 px tall and the 45 px row's last pixel is the
    // Hairline. 44 is a multiple of 4, so at 125/150/175/225/250% the button's bottom edge lands on
    // a whole device pixel and the hover fill cannot bleed into the line.
    row->setContentsMargins(14, 0, 0, 1);
    row->setSpacing(0);

    m_logo = new QLabel;
    m_logo->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    row->addWidget(m_logo, 0, Qt::AlignVCenter);
    row->addSpacing(12);   // + the first chip's own 2 px margin = 14 px of air after the logo

    // Own QMenuBar (not QMainWindow::menuBar(): setMenuWidget would delete that one).
    m_menuBar = new QMenuBar(m_menuWrap);
    m_menuBar->setFixedHeight(44);
    row->addWidget(m_menuBar, 1);

    struct Caption { RoHoverIcon *&button; const char *icon; const char *tip; bool danger; };
    for (const Caption &c : {Caption {m_minButton, "win-min", "Minimize", false}
                             , Caption {m_maxButton, "win-max", "Maximize", false}
                             , Caption {m_closeButton, "win-close", "Close", true}})
    {
        c.button = new RoHoverIcon(QString::fromLatin1(c.icon), &RoPalette::muted, &RoPalette::text
                                   , c.danger, 16);
        c.button->setObjectName(QStringLiteral("capBtn"));
        c.button->setFixedWidth(CAPTION_BUTTON_WIDTH);
        c.button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        c.button->setToolTip(QString::fromLatin1(c.tip));
        c.button->setProperty("danger", c.danger);
        row->addWidget(c.button);   // full row height, no gaps: hover fills the whole rectangle
    }
    connect(m_minButton, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(m_maxButton, &QToolButton::clicked, this, &RoFramelessWindow::toggleMaximized);
    connect(m_closeButton, &QToolButton::clicked, this, &QWidget::close);

    m_menuWrap->setFixedHeight(TITLE_ROW_HEIGHT);
    new Hairline(m_menuWrap);   // the row's bottom divider sits above the caption buttons
    setMenuWidget(m_menuWrap);

    rebuildLogo();
    connect(&RoTheme::instance(), &RoTheme::changed, this, &RoFramelessWindow::rebuildLogo);

    // One application-wide filter handles edge resize, title-row drag and double-click.
    qApp->installEventFilter(this);
}

RoFramelessWindow::~RoFramelessWindow()
{
    clearEdgeCursor();
}

void RoFramelessWindow::toggleMaximized()
{
    if (isMaximized())
        showNormal();
    else
        showMaximized();
}

void RoFramelessWindow::rebuildLogo()
{
    // 18 px tall in the 44 px row. The logo is ~764 px wide, so downscale in steps of at most 2x
    // (a single 48x shrink aliases) and bake the 85% opacity into the pixmap: never apply an
    // opacity effect to the logo widget.
    const qreal dpr = devicePixelRatioF();
    const int targetH = qRound(18 * dpr);
    QImage img = QPixmap(RoTheme::instance().logoPath()).toImage();   // same path as the demo (pixmap -> image)
    if (img.isNull())
        return;
    while (img.height() > 2 * targetH)
        img = img.scaledToHeight(qMax(targetH, img.height() / 2), Qt::SmoothTransformation);
    img = img.scaledToHeight(targetH, Qt::SmoothTransformation);

    QPixmap out(img.size());
    out.fill(Qt::transparent);
    {
        QPainter painter(&out);
        painter.setOpacity(0.85);
        painter.drawImage(0, 0, img);
    }
    out.setDevicePixelRatio(dpr);
    m_logo->setPixmap(out);
    m_logo->setFixedSize(qRound(out.width() / dpr), qRound(out.height() / dpr));
}

bool RoFramelessWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (RoWinSnap::nativeEvent(eventType, message, result))
        return true;
    return QMainWindow::nativeEvent(eventType, message, result);
}

void RoFramelessWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_snapInitialised)
        return;
    m_snapInitialised = true;
    RoWinSnap::enable(static_cast<quintptr>(winId()));   // Windows Snap; no-op elsewhere
    if (QWindow *handle = windowHandle())
        connect(handle, &QWindow::screenChanged, this, &RoFramelessWindow::rebuildLogo);   // new DPR
}

void RoFramelessWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
    {
        updateMaximizeButton();
        if (!isMinimized())
            QMetaObject::invokeMethod(this, [this] { repaint(); }, Qt::QueuedConnection);
    }
}

void RoFramelessWindow::updateMaximizeButton()
{
    if (!m_maxButton)
        return;
    const bool maximized = isMaximized();
    m_maxButton->setIconName(maximized ? QStringLiteral("win-restore") : QStringLiteral("win-max"));
    m_maxButton->setToolTip(maximized ? tr("Restore") : tr("Maximize"));
}

Qt::Edges RoFramelessWindow::edgesAt(const QPoint &globalPos) const
{
    if (isMaximized() || isFullScreen())
        return {};
    const QRect g = frameGeometry();
    Qt::Edges edges;
    if (globalPos.x() - g.left() < RESIZE_EDGE) edges |= Qt::LeftEdge;
    if (g.right() - globalPos.x() < RESIZE_EDGE) edges |= Qt::RightEdge;
    if (globalPos.y() - g.top() < RESIZE_EDGE) edges |= Qt::TopEdge;
    if (g.bottom() - globalPos.y() < RESIZE_EDGE) edges |= Qt::BottomEdge;
    return edges;
}

void RoFramelessWindow::setEdgeCursor(const Qt::Edges edges)
{
    std::optional<Qt::CursorShape> shape;
    if ((edges == Qt::LeftEdge) || (edges == Qt::RightEdge))
        shape = Qt::SizeHorCursor;
    else if ((edges == Qt::TopEdge) || (edges == Qt::BottomEdge))
        shape = Qt::SizeVerCursor;
    else if ((edges == (Qt::LeftEdge | Qt::TopEdge)) || (edges == (Qt::RightEdge | Qt::BottomEdge)))
        shape = Qt::SizeFDiagCursor;
    else if ((edges == (Qt::RightEdge | Qt::TopEdge)) || (edges == (Qt::LeftEdge | Qt::BottomEdge)))
        shape = Qt::SizeBDiagCursor;

    if (shape && !m_edgeCursorActive)
    {
        QGuiApplication::setOverrideCursor(*shape);
        m_edgeCursorActive = true;
    }
    else if (shape)
    {
        QGuiApplication::changeOverrideCursor(*shape);
    }
    else
    {
        clearEdgeCursor();
    }
}

void RoFramelessWindow::clearEdgeCursor()
{
    if (!m_edgeCursorActive)
        return;
    QGuiApplication::restoreOverrideCursor();
    m_edgeCursorActive = false;
}

bool RoFramelessWindow::isTitleDragArea(const QObject *obj) const
{
    return (obj == m_menuWrap) || (obj == m_menuBar) || (obj == m_logo);
}

bool RoFramelessWindow::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type type = event->type();

    if (const auto *widget = qobject_cast<QWidget *>(watched); widget && (widget->window() == this)
        && ((type == QEvent::MouseMove) || (type == QEvent::MouseButtonPress)))
    {
        const auto *me = static_cast<QMouseEvent *>(event);
        Qt::Edges edges = edgesAt(me->globalPosition().toPoint());
        // Keep the caption buttons' hover and click area intact right up to the top edge.
        if ((widget == m_minButton) || (widget == m_maxButton) || (widget == m_closeButton))
            edges &= ~Qt::Edges(Qt::TopEdge);

        if ((type == QEvent::MouseMove) && !(me->buttons() & Qt::LeftButton))
        {
            setEdgeCursor(edges);
        }
        else if ((type == QEvent::MouseButtonPress) && (me->button() == Qt::LeftButton) && edges
                 && windowHandle())
        {
            windowHandle()->startSystemResize(edges);
            return true;
        }
    }
    else if ((type == QEvent::Leave) && (watched == this))
    {
        clearEdgeCursor();
    }

    // Empty space in the title row (and the logo) drags the window; double-click toggles maximize.
    if (isTitleDragArea(watched)
        && ((type == QEvent::MouseButtonPress) || (type == QEvent::MouseButtonDblClick)))
    {
        const auto *me = static_cast<QMouseEvent *>(event);
        const bool overMenuItem = (watched == m_menuBar) && m_menuBar->actionAt(me->position().toPoint());
        if ((me->button() == Qt::LeftButton) && !overMenuItem)
        {
            if (type == QEvent::MouseButtonDblClick)
            {
                toggleMaximized();
                return true;
            }
            if (windowHandle())
            {
                windowHandle()->startSystemMove();
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QRadialGradient>
#include <QPixmap>
#include <QPainter>
#include <QPalette>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QLayout>
#include <QDebug>

#if defined(Q_OS_LINUX)
#include <QX11Info>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#elif defined(Q_OS_WIN)
#include <Windows.h>
#include <windowsx.h>

#pragma comment(lib, "User32.lib")
#endif

namespace
{
    enum CornerEdge : int
    {
        Invalid     = 0,
        Top         = 1,
        Right       = 2,
        Bottom      = 4,
        Left        = 8,
        TopLeft     = 1 | 8,
        TopRight    = 1 | 2,
        BottomLeft  = 4 | 8,
        BottomRight = 4 | 2,
    };

    static CornerEdge GetCornerEdge(const QWidget *widget, const QPoint &pos, const QMargins &margins, int borderWidth)
    {
#if defined(Q_OS_WIN)
        RECT rect;
        GetWindowRect(HWND(widget->winId()), &rect);

        auto fullRect = QRect(0, 0, rect.right-rect.left, rect.bottom-rect.top).marginsRemoved(margins);
#else
        auto fullRect = widget->rect().marginsRemoved(margins);
#endif
        auto ce = static_cast<int>(CornerEdge::Invalid);
        if ((pos.y() - fullRect.top() >= -borderWidth) && (pos.y() < fullRect.top()))
        {
            ce = ce | static_cast<int>(CornerEdge::Top);
        }
        if ((pos.x() - fullRect.left() >= -borderWidth) && (pos.x() < fullRect.left()))
        {
            ce = ce | static_cast<int>(CornerEdge::Left);
        }
        if ((pos.y() - fullRect.bottom() <= borderWidth) && (pos.y() > fullRect.bottom()))
        {
            ce = ce | static_cast<int>(CornerEdge::Bottom);
        }
        if ((pos.x() - fullRect.right() <= borderWidth) && (pos.x() > fullRect.right()))
        {
            ce = ce | static_cast<int>(CornerEdge::Right);
        }
        return static_cast<CornerEdge>(ce);
    }
}


#if defined(Q_OS_LINUX)
namespace
{
static int CornerEdge2WmGravity(const CornerEdge &ce)
    {
        switch (ce)
        {
        case CornerEdge::TopLeft:     return 0;
        case CornerEdge::Top:         return 1;
        case CornerEdge::TopRight:    return 2;
        case CornerEdge::Right:       return 3;
        case CornerEdge::BottomRight: return 4;
        case CornerEdge::Bottom:      return 5;
        case CornerEdge::BottomLeft:  return 6;
        case CornerEdge::Left:        return 7;
        default:                      return -1;
        }
    }

    static void SetMouseTransparent(const QWidget *widget, bool on)
    {
        const auto display = QX11Info::display();

        XRectangle xRect;
        memset(&xRect, 0, sizeof(xRect));

        int nRects{ 0 };
        if (!on)
        {
            xRect.width = static_cast<decltype (xRect.width)>(widget->width());
            xRect.height = static_cast<decltype (xRect.height)>(widget->height());
            nRects = 1;
        }

        XShapeCombineRectangles(display, widget->winId(), ShapeInput, 0, 0, &xRect, nRects, ShapeSet, YXBanded);
    }

    static void StartResizing(const QWidget *widget, const QPoint &pos, const QPoint &globalPos, const CornerEdge &ce)
    {
        const auto display = QX11Info::display();
        const auto winId = widget->winId();
        const auto screen = QX11Info::appScreen();

        {
            XEvent xev;
            memset(&xev, 0, sizeof(XEvent));

            xev.type = ButtonRelease;
            xev.xbutton.button = Button1;
            xev.xbutton.window = widget->effectiveWinId();
            xev.xbutton.x = pos.x();
            xev.xbutton.y = pos.y();
            xev.xbutton.x_root = globalPos.x();
            xev.xbutton.y_root = globalPos.y();
            xev.xbutton.display = display;

            XSendEvent(display, widget->effectiveWinId(), False, ButtonReleaseMask, &xev);
            XFlush(display);
        }

        XEvent xev;
        const Atom netMoveResize = XInternAtom(display, "_NET_WM_MOVERESIZE", false);
        xev.xclient.type = ClientMessage;
        xev.xclient.message_type = netMoveResize;
        xev.xclient.display = display;
        xev.xclient.window = winId;
        xev.xclient.format = 32;

        xev.xclient.data.l[0] = globalPos.x();
        xev.xclient.data.l[1] = globalPos.y();
        xev.xclient.data.l[2] = CornerEdge2WmGravity(ce);
        xev.xclient.data.l[3] = Button1;
        xev.xclient.data.l[4] = 1;
        XUngrabPointer(display, QX11Info::appTime());

        XSendEvent(display, QX11Info::appRootWindow(screen), false, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
        XFlush(display);
    }

    void StartMoving(const QWidget *widget, const QPoint &pos, const QPoint &globalPos)
    {
        const auto display = QX11Info::display();
        const auto screen = QX11Info::appScreen();

        {
            XEvent xev;
            memset(&xev, 0, sizeof(XEvent));

            xev.type = ButtonRelease;
            xev.xbutton.button = Button1;
            xev.xbutton.window = widget->effectiveWinId();
            xev.xbutton.x = pos.x();
            xev.xbutton.y = pos.y();
            xev.xbutton.x_root = globalPos.x();
            xev.xbutton.y_root = globalPos.y();
            xev.xbutton.display = display;

            XSendEvent(display, widget->effectiveWinId(), False, ButtonReleaseMask, &xev);
            XFlush(display);
        }

        const Atom netMoveResize = XInternAtom(display, "_NET_WM_MOVERESIZE", false);

        XEvent xev;
        memset(&xev, 0, sizeof(xev));

        xev.xclient.type = ClientMessage;
        xev.xclient.message_type = netMoveResize;
        xev.xclient.display = display;
        xev.xclient.window = widget->winId();
        xev.xclient.format = 32;

        xev.xclient.data.l[0] = globalPos.x();
        xev.xclient.data.l[1] = globalPos.y();
        xev.xclient.data.l[2] = 8;
        xev.xclient.data.l[3] = Button1;
        xev.xclient.data.l[4] = 0;

        XUngrabPointer(display, QX11Info::appTime());
        XSendEvent(display, QX11Info::appRootWindow(screen), false, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
        XFlush(display);
    }

    void SetWindowPosition(const QWidget *widget, const QPoint &pos)
    {
        XMoveWindow(QX11Info::display(), static_cast<uint>(widget->winId()), pos.x(), pos.y());
    }

    void SetWindowExtents(const QWidget *widget, const QMargins &margins, const int resizeHandleSize)
    {
        unsigned long value[4] =
        {
            static_cast<unsigned long>(margins.left()),
            static_cast<unsigned long>(margins.right()),
            static_cast<unsigned long>(margins.top()),
            static_cast<unsigned long>(margins.bottom())
        };

        auto winId = static_cast<uint>(widget->winId());

        auto frameExtents = XInternAtom(QX11Info::display(), "_GTK_FRAME_EXTENTS", False);
        if (frameExtents == None)
        {
            qWarning() << "Failed to create atom with name DEEPIN_WINDOW_SHADOW";
            return;
        }

        XChangeProperty(QX11Info::display(), winId, frameExtents, XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<unsigned char *>(value), 4);

        auto tmpRect = widget->rect() - margins;

        XRectangle contentXRect;
        contentXRect.x = 0;
        contentXRect.y = 0;
        contentXRect.width = static_cast<decltype (contentXRect.width)>(tmpRect.width()+resizeHandleSize*2);
        contentXRect.height = static_cast<decltype (contentXRect.height)>(tmpRect.height()+resizeHandleSize*2);
        XShapeCombineRectangles(QX11Info::display(), winId, ShapeInput, margins.left() - resizeHandleSize, margins.top() - resizeHandleSize, &contentXRect, 1, ShapeSet, YXBanded);
    }
}
#elif defined(Q_OS_WIN)
#endif

MainWindow::MainWindow(QWidget *parent)
    : QWidget{ parent }
    , ui{ new Ui::MainWindow }
{
    ui->setupUi(this);
    ui->titleBar->installEventFilter(this);

    setAttribute(Qt::WA_TranslucentBackground, true);

#if defined(Q_OS_LINUX)
    setAttribute(Qt::WA_Hover, true);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);

    SetMouseTransparent(this, true);
#elif defined(Q_OS_WIN)
    setWindowFlags(windowFlags() | Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);

    SetWindowLongPtr(HWND(winId()), GWL_STYLE, WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
#endif

    layout()->setContentsMargins(m_padding, m_padding, m_padding, m_padding);

    generateBackground();

    connect(this, &MainWindow::windowTitleChanged, [this](const QString &title){ ui->titleLabel->setText(title); });
    connect(this, &MainWindow::windowIconChanged, [this](const QIcon &icon){ ui->titleIcon->setPixmap(icon.pixmap(ui->titleIcon->size())); });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::move(int x, int y)
{
    move(QPoint{x, y});
}

void MainWindow::move(const QPoint &pos)
{
    auto newPos = pos - QPoint(m_padding, m_padding);

    if (windowState() & Qt::WindowMaximized)
    {
        setWindowState(windowState() & ~Qt::WindowMaximized);
    }
    if (windowState() & Qt::WindowFullScreen)
    {
        setWindowState(windowState() & ~Qt::WindowFullScreen);
    }

#if defined(Q_OS_LINUX)
    SetWindowPosition(this, newPos);
#elif defined(Q_OS_WIN)
    RECT rect;
    GetWindowRect(HWND(winId()), &rect);
    SetWindowPos(HWND(winId()), NULL, newPos.x(), newPos.y(), rect.right - rect.left, rect.bottom - rect.top, SWP_SHOWWINDOW);
#endif
}

bool MainWindow::event(QEvent *event)
{
    switch (event->type())
    {
    case QEvent::WindowStateChange:
        {
            if (windowState() & Qt::WindowFullScreen)
            {
                ui->titleBar->setVisible(false);
#ifdef Q_OS_WIN
                layout()->setContentsMargins(0, 0, 0, 0);
#endif
            }
            else
            {
                ui->titleBar->setVisible(true);

#ifdef Q_OS_WIN
                layout()->setContentsMargins(m_padding, m_padding, m_padding, m_padding);
#endif

                if (windowState() & Qt::WindowMaximized)
                {
                    ui->titleMaxButton->setIcon(QIcon{ ":/MainWindow/icon/window_restore.svg" });
                    ui->titleMaxButton->setToolTip(tr("还原"));

#ifdef Q_OS_WIN
                    auto w = style()->pixelMetric(QStyle::PM_DefaultFrameWidth)*2;
                    layout()->setContentsMargins(m_padding-w, m_padding-w, m_padding-w, m_padding-w);
#endif
                }
                else
                {
                    ui->titleMaxButton->setIcon(QIcon{ ":/MainWindow/icon/window_max.svg" });
                    ui->titleMaxButton->setToolTip(tr("最大化"));
                }
            }

            update();
            break;
        }
    case QEvent::HoverMove:
        {
#if defined(Q_OS_LINUX)
#ifdef CursorShape
#   undef CursorShape
            
            auto hoverEvent = dynamic_cast<QHoverEvent *>(event);
            
            static const QMap<CornerEdge, Qt::CursorShape> cursorShape
            {
                { CornerEdge::Top, Qt::SizeVerCursor },
                { CornerEdge::Bottom, Qt::SizeVerCursor },
                { CornerEdge::Left, Qt::SizeHorCursor },
                { CornerEdge::Right, Qt::SizeHorCursor },
                { CornerEdge::TopLeft, Qt::SizeFDiagCursor },
                { CornerEdge::BottomRight, Qt::SizeFDiagCursor },
                { CornerEdge::TopRight, Qt::SizeBDiagCursor },
                { CornerEdge::BottomLeft, Qt::SizeBDiagCursor },
                { CornerEdge::Invalid, Qt::ArrowCursor },
            };

            auto cornerEdge = GetCornerEdge(this, hoverEvent->pos(), layout()->contentsMargins()+m_padding/4, m_padding);
            setCursor(cursorShape.value(cornerEdge));
#endif
#endif
            break;
        }
    default:
        break;
    }

    return QWidget::event(event);
}

#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
#else
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
#endif
{
    if (eventType == "windows_generic_MSG")
    {
#ifdef Q_OS_WIN
        MSG* msg = static_cast<MSG *>(message);

        switch (msg->message)
        {
        case WM_NCCALCSIZE:
            {
                *result = 0;
                return true;
            }
        case WM_NCHITTEST:
            {
                RECT rect;
                GetWindowRect(HWND(winId()), &rect);

                QPoint mousePos{ GET_X_LPARAM(msg->lParam) - rect.left, GET_Y_LPARAM(msg->lParam) - rect.top };

                if (!isMaximized() && !isFullScreen())
                {
                    auto cornerEdge = GetCornerEdge(this, mousePos, layout()->contentsMargins() + m_padding / 4, m_padding);
                    switch (cornerEdge)
                    {
                    case CornerEdge::Top: *result = HTTOP; break;
                    case CornerEdge::Bottom: *result = HTBOTTOM; break;
                    case CornerEdge::Left: *result = HTLEFT; break;
                    case CornerEdge::Right: *result = HTRIGHT; break;
                    case CornerEdge::TopLeft: *result = HTTOPLEFT; break;
                    case CornerEdge::BottomRight: *result = HTBOTTOMRIGHT; break;
                    case CornerEdge::TopRight: *result = HTTOPRIGHT; break;
                    case CornerEdge::BottomLeft: *result = HTBOTTOMLEFT; break;
                    default: *result = 0; break;
                    }

                    if (*result != 0)
                    {
                        return true;
                    }
                }

                auto titleRect = ui->titleBar->geometry();
                if (ui->titleBar->isVisible() && titleRect.adjusted(0, 0, -titleRect.width() + ui->titleMinButton->x() + 5, 0).contains(mousePos))
                {
                    *result = HTCAPTION;
                    return true;
                }

                *result = HTCLIENT;
                return true;
            }
        default:
            break;
        }
#endif
    }

    return QWidget::nativeEvent(eventType, message, result);
}

void MainWindow::showEvent(QShowEvent *event)
{
    setAttribute(Qt::WA_Mapped);
    QWidget::showEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
#if defined(Q_OS_LINUX)
    SetWindowExtents(this, layout()->contentsMargins(), isFullScreen() ? 0 : m_padding);
#endif

    QWidget::resizeEvent(event);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
#if defined(Q_OS_LINUX)
    auto cornerEdge = GetCornerEdge(this, event->pos(), layout()->contentsMargins()+m_padding/2, m_padding);
    if (cornerEdge != CornerEdge::Invalid)
    {
        StartResizing(this, event->pos(), event->globalPos(), cornerEdge);
    }
    else
    {
        if (ui->titleBar->geometry().contains(event->pos()))
        {
            StartMoving(this, event->pos(), event->globalPos());
        }
    }
#endif

    QWidget::mousePressEvent(event);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
#ifdef Q_OS_LINUX
    if (ui->titleBar->geometry().contains(event->pos()))
    {
        changeWindowMaximizedState();
    }
#endif

    QWidget::mouseDoubleClickEvent(event);
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    auto clientRect = rect();

    QPainter painter{ this };
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);

    if (!isMaximized() && !isFullScreen())
    {
        painter.drawPixmap(0, 0, m_background.at(4));
        painter.drawPixmap(clientRect.width()-m_padding, 0, m_background.at(5));
        painter.drawPixmap(0, clientRect.height()-m_padding, m_background.at(6));
        painter.drawPixmap(clientRect.width()-m_padding, clientRect.height()-m_padding, m_background.at(7));

        painter.drawTiledPixmap(0, m_padding, m_padding, clientRect.height()-m_padding*2, m_background.at(0));
        painter.drawTiledPixmap(clientRect.width()-m_padding, m_padding, m_padding, clientRect.height()-m_padding*2, m_background.at(1));
        painter.drawTiledPixmap(m_padding, 0, clientRect.width()-m_padding*2, m_padding, m_background.at(2));
        painter.drawTiledPixmap(m_padding, clientRect.height()-m_padding, clientRect.width()-m_padding*2, m_padding, m_background.at(3));
    }

    painter.setBrush(QPalette().window());
    painter.drawRect(clientRect.marginsRemoved(layout()->contentsMargins()));
}

void MainWindow::changeWindowMaximizedState()
{
    setWindowState(isMaximized() ? (windowState() & ~Qt::WindowMaximized) : windowState() | Qt::WindowMaximized);
}

void MainWindow::on_titleMinButton_clicked()
{
    setWindowState(windowState() | Qt::WindowMinimized);
}

void MainWindow::on_titleMaxButton_clicked()
{
    changeWindowMaximizedState();
}

void MainWindow::on_titleCloseButton_clicked()
{
    close();
}

void MainWindow::generateBackground()
{
    QPixmap pixmap{ 22, 22 };
    pixmap.fill(QColor{ 0, 0, 0, 0 });

    QRadialGradient gradient{ 0, 0, 11, 0, 0 };
    gradient.setColorAt(0.0, QColor{ 0, 0, 0, 80 });
    gradient.setColorAt(0.2, QColor{ 0, 0, 0, 40 });
    gradient.setColorAt(0.5, QColor{ 0, 0, 0, 5 });
    gradient.setColorAt(1.0, QColor{ 0, 0, 0, 0 });

    QPainter painter{ &pixmap };
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(pixmap.width()/2, pixmap.height()/2);
    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(-11, -11, pixmap.width(), pixmap.height());

    m_background.append(pixmap.copy(0, 11, 10, 1));   // 0, left
    m_background.append(pixmap.copy(11, 11, 10, 1));  // 1, right
    m_background.append(pixmap.copy(11, 0, 1, 10));   // 2, top
    m_background.append(pixmap.copy(11, 11, 1, 10));  // 3, bottom
    m_background.append(pixmap.copy(0, 0, 10, 10));   // 4, top left
    m_background.append(pixmap.copy(11, 0, 10, 10));  // 5, top right
    m_background.append(pixmap.copy(0, 11, 10, 10));  // 6, bottom left
    m_background.append(pixmap.copy(11, 11, 10, 10)); // 7, bottom right
}

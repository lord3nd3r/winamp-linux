#include "QtWindowAdapter.h"
#include <QApplication>
#include <QPainter>
#include <QScreen>

QtWindowAdapter::QtWindowAdapter(QWidget *parent)
    : QWidget(parent)
    , m_wndProc(nullptr)
    , m_userData(nullptr)
{
    // Enable mouse tracking for WM_MOUSEMOVE
    setMouseTracking(true);
    
    // Accept focus for keyboard events
    setFocusPolicy(Qt::StrongFocus);
    
    // Enable custom painting
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
}

QtWindowAdapter::~QtWindowAdapter()
{
    if (m_wndProc) {
        dispatchMessage(WM_DESTROY, 0, 0);
    }
}

void QtWindowAdapter::setWndProc(WndProcCallback callback, void *userData)
{
    m_wndProc = callback;
    m_userData = userData;
    
    // Send WM_CREATE after wndproc is set
    if (m_wndProc) {
        dispatchMessage(WM_CREATE, 0, 0);
    }
}

void QtWindowAdapter::setWindowText(const wchar_t *text)
{
    m_windowText = QString::fromWCharArray(text);
    setWindowTitle(m_windowText);
}

void QtWindowAdapter::getWindowText(wchar_t *buffer, int maxCount)
{
    if (buffer && maxCount > 0) {
        int len = qMin(m_windowText.length(), maxCount - 1);
        m_windowText.toWCharArray(buffer);
        buffer[len] = L'\0';
    }
}

void QtWindowAdapter::getClientRect(void *rect)
{
    RECT *r = (RECT*)rect;
    if (r) {
        r->left = 0;
        r->top = 0;
        r->right = width();
        r->bottom = height();
    }
}

void QtWindowAdapter::getWindowRect(void *rect)
{
    RECT *r = (RECT*)rect;
    if (r) {
        QRect geom = geometry();
        r->left = geom.left();
        r->top = geom.top();
        r->right = geom.right();
        r->bottom = geom.bottom();
    }
}

void QtWindowAdapter::invalidateRect(const void *rect, bool erase)
{
    const RECT *r = (const RECT*)rect;
    if (r) {
        QRect qrect(r->left, r->top, 
                    r->right - r->left, 
                    r->bottom - r->top);
        update(qrect);
    } else {
        update();
    }
}

void QtWindowAdapter::showWindow(int cmdShow)
{
    // Map Win32 SW_* constants to Qt
    switch (cmdShow) {
        case 0: hide(); break;              // SW_HIDE
        case 1: showNormal(); break;        // SW_SHOWNORMAL
        case 3: showMaximized(); break;     // SW_SHOWMAXIMIZED
        case 6: showMinimized(); break;     // SW_MINIMIZE
        default: show(); break;
    }
}

void* QtWindowAdapter::widgetToHandle(QWidget *widget)
{
    return reinterpret_cast<void*>(widget);
}

QtWindowAdapter* QtWindowAdapter::handleToAdapter(void *handle)
{
    return reinterpret_cast<QtWindowAdapter*>(handle);
}

QtWindowAdapter* QtWindowAdapter::handleToAdapter(OSWINDOWHANDLE handle)
{
    return reinterpret_cast<QtWindowAdapter*>(handle);
}

LRESULT QtWindowAdapter::dispatchMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (m_wndProc) {
        return m_wndProc(m_userData, msg, wParam, lParam);
    }
    return 0;
}

bool QtWindowAdapter::event(QEvent *event)
{
    // Let Qt handle the event first, then dispatch to Win32 callback
    bool handled = QWidget::event(event);
    
    // Map specific Qt events to Win32 messages
    switch (event->type()) {
        case QEvent::Show:
            dispatchMessage(WM_SHOWWINDOW, 1, 0);
            break;
        case QEvent::Hide:
            dispatchMessage(WM_SHOWWINDOW, 0, 0);
            break;
        case QEvent::Timer:
            dispatchMessage(WM_TIMER, 0, 0);
            break;
        default:
            break;
    }
    
    return handled;
}

void QtWindowAdapter::paintEvent(QPaintEvent *event)
{
    // Send WM_PAINT to allow custom drawing
    if (m_wndProc) {
        dispatchMessage(WM_PAINT, 0, 0);
    }
    
    // Don't call QWidget::paintEvent - we handle all painting
}

void QtWindowAdapter::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    QSize newSize = event->size();
    LPARAM lParam = (LPARAM)((newSize.width() & 0xFFFF) | ((newSize.height() & 0xFFFF) << 16));
    dispatchMessage(WM_SIZE, 0, lParam);
}

void QtWindowAdapter::mousePressEvent(QMouseEvent *event)
{
    LPARAM lParam = (LPARAM)((event->pos().x() & 0xFFFF) | ((event->pos().y() & 0xFFFF) << 16));
    
    if (event->button() == Qt::LeftButton) {
        dispatchMessage(WM_LBUTTONDOWN, 0, lParam);
    } else if (event->button() == Qt::RightButton) {
        dispatchMessage(WM_RBUTTONDOWN, 0, lParam);
    }
}

void QtWindowAdapter::mouseReleaseEvent(QMouseEvent *event)
{
    LPARAM lParam = (LPARAM)((event->pos().x() & 0xFFFF) | ((event->pos().y() & 0xFFFF) << 16));
    
    if (event->button() == Qt::LeftButton) {
        dispatchMessage(WM_LBUTTONUP, 0, lParam);
    } else if (event->button() == Qt::RightButton) {
        dispatchMessage(WM_RBUTTONUP, 0, lParam);
    }
}

void QtWindowAdapter::mouseMoveEvent(QMouseEvent *event)
{
    LPARAM lParam = (LPARAM)((event->pos().x() & 0xFFFF) | ((event->pos().y() & 0xFFFF) << 16));
    dispatchMessage(WM_MOUSEMOVE, 0, lParam);
}

void QtWindowAdapter::keyPressEvent(QKeyEvent *event)
{
    dispatchMessage(WM_KEYDOWN, event->key(), 0);
    
    if (!event->text().isEmpty()) {
        dispatchMessage(WM_CHAR, event->text()[0].unicode(), 0);
    }
}

void QtWindowAdapter::keyReleaseEvent(QKeyEvent *event)
{
    dispatchMessage(WM_KEYUP, event->key(), 0);
}

void QtWindowAdapter::closeEvent(QCloseEvent *event)
{
    LRESULT result = dispatchMessage(WM_CLOSE, 0, 0);
    
    if (result == 0) {
        event->accept();
    } else {
        event->ignore();
    }
}

#ifndef __QT_WINDOW_ADAPTER_H
#define __QT_WINDOW_ADAPTER_H

#include <QWidget>
#include <QEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <cstdint>

/**
 * QtWindowAdapter - Bridges Win32 HWND concepts to Qt QWidget
 * 
 * This adapter allows existing Winamp code that expects HWND handles
 * to work with Qt6 widgets. Maps Win32 window messages to Qt events.
 * 
 * Note: Q_OBJECT removed to avoid MOC complications - we don't need signals/slots
 */
class QtWindowAdapter : public QWidget {

public:
    explicit QtWindowAdapter(QWidget *parent = nullptr);
    virtual ~QtWindowAdapter();

    // Win32-style window procedure callback - using standard types for MOC
    typedef int64_t (*WndProcCallback)(void *userData, uint32_t msg, uint64_t wParam, int64_t lParam);
    
    void setWndProc(WndProcCallback callback, void *userData);
    
    // Map Qt widget to handle (for Wasabi compatibility)
    void* getOSHandle() const { return reinterpret_cast<void*>(const_cast<QtWindowAdapter*>(this)); }
    
    // Win32-like window operations
    void setWindowText(const wchar_t *text);
    void getWindowText(wchar_t *buffer, int maxCount);
    void getClientRect(void *rect);  // Takes RECT* but as void* for MOC
    void getWindowRect(void *rect);
    void invalidateRect(const void *rect, bool erase);
    
    // Window state
    bool isWindowVisible() const { return QWidget::isVisible(); }
    void showWindow(int cmdShow);
    
    // Static helper - convert QWidget* to handle
    static void* widgetToHandle(QWidget *widget);
    static QtWindowAdapter* handleToAdapter(void *handle);

protected:
    // Qt event handlers - forward to Win32-style callback
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    // Map Qt events to Win32 messages
    int64_t dispatchMessage(uint32_t msg, uint64_t wParam, int64_t lParam);
    
    WndProcCallback m_wndProc;
    void *m_userData;
    QString m_windowText;
};

#endif // __QT_WINDOW_ADAPTER_H

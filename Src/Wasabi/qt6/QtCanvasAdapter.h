#ifndef __QT_CANVAS_ADAPTER_H
#define __QT_CANVAS_ADAPTER_H

#include <QPainter>
#include <QPixmap>
#include <QImage>
#include <QColor>

// Forward declarations for types
typedef unsigned long COLORREF;
struct tagRECT;
typedef struct tagRECT RECT;

/**
 * QtCanvasAdapter - Bridges Win32 HDC/GDI to Qt QPainter
 * 
 * Maps GDI drawing operations to Qt's QPainter for rendering.
 * Allows existing Winamp bitmap and drawing code to work on Qt.
 */
class QtCanvasAdapter {
public:
    explicit QtCanvasAdapter(QPaintDevice *device);
    explicit QtCanvasAdapter(QPixmap *pixmap);
    ~QtCanvasAdapter();

    // Get the underlying QPainter
    QPainter* painter() { return &m_painter; }
    
    // Win32 HDC emulation
    typedef void* HDC;
    HDC getHDC() { return reinterpret_cast<HDC>(this); }
    
    // GDI drawing operations
    void bitBlt(int x, int y, int width, int height, 
                QtCanvasAdapter *src, int srcX, int srcY, int rop = 0);
    void stretchBlt(int x, int y, int width, int height,
                    QtCanvasAdapter *src, int srcX, int srcY, 
                    int srcWidth, int srcHeight, int rop = 0);
    
    // Text operations
    void textOut(int x, int y, const wchar_t *text, int length = -1);
    void drawText(const RECT *rect, const wchar_t *text, 
                  int format = 0);
    
    // Shape operations
    void fillRect(const RECT *rect, COLORREF color);
    void frameRect(const RECT *rect, COLORREF color);
    void drawLine(int x1, int y1, int x2, int y2);
    void drawRect(int x, int y, int width, int height);
    void drawEllipse(int x, int y, int width, int height);
    
    // Pen and brush
    void setPen(COLORREF color, int width = 1);
    void setBrush(COLORREF color);
    void setFont(const QFont &font);
    
    // Color operations
    void setTextColor(COLORREF color);
    void setBkColor(COLORREF color);
    void setBkMode(int mode); // TRANSPARENT = 1, OPAQUE = 2
    
    // Bitmap operations
    void drawPixmap(int x, int y, const QPixmap &pixmap);
    void drawImage(int x, int y, const QImage &image);
    
    // Clipping
    void setClipRect(const RECT *rect);
    void clearClipRect();
    
    // State management
    void save();
    void restore();
    
    // Static conversions
    static QColor colorrefToQColor(COLORREF color);
    static COLORREF qcolorToColorref(const QColor &color);
    static QRect rectToQRect(const RECT *rect);
    static void qrectToRect(const QRect &qrect, RECT *rect);

private:
    QPainter m_painter;
    QColor m_textColor;
    QColor m_bkColor;
    int m_bkMode;
    bool m_ownsDevice;
};

// Raster operation codes (for BitBlt/StretchBlt)
#ifndef SRCCOPY
#define SRCCOPY     0x00CC0020
#define SRCPAINT    0x00EE0086
#define SRCAND      0x008800C6
#define SRCINVERT   0x00660046
#define NOTSRCCOPY  0x00330008
#endif

#endif // __QT_CANVAS_ADAPTER_H

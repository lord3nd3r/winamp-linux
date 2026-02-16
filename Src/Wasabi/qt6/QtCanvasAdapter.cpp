#include "QtCanvasAdapter.h"
#include "win32_types.h"
#include <QFontMetrics>

QtCanvasAdapter::QtCanvasAdapter(QPaintDevice *device)
    : m_painter(device)
    , m_textColor(Qt::black)
    , m_bkColor(Qt::white)
    , m_bkMode(2) // OPAQUE
    , m_ownsDevice(false)
{
    m_painter.setRenderHint(QPainter::Antialiasing, false);
    m_painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
}

QtCanvasAdapter::QtCanvasAdapter(QPixmap *pixmap)
    : m_painter(pixmap)
    , m_textColor(Qt::black)
    , m_bkColor(Qt::white)
    , m_bkMode(2)
    , m_ownsDevice(false)
{
    m_painter.setRenderHint(QPainter::Antialiasing, false);
    m_painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
}

QtCanvasAdapter::~QtCanvasAdapter()
{
    if (m_painter.isActive()) {
        m_painter.end();
    }
}

void QtCanvasAdapter::bitBlt(int x, int y, int width, int height,
                             QtCanvasAdapter *src, int srcX, int srcY, int rop)
{
    if (!src) return;
    
    // Get source painter's device as QPixmap
    QPaintDevice *srcDevice = src->m_painter.device();
    if (!srcDevice) return;
    
    QPixmap srcPixmap;
    if (QPixmap *pm = dynamic_cast<QPixmap*>(srcDevice)) {
        srcPixmap = *pm;
    } else {
        // Convert from other device types
        QImage img(srcDevice->width(), srcDevice->height(), QImage::Format_ARGB32);
        QPainter p(&img);
        // TODO: copy device content
        srcPixmap = QPixmap::fromImage(img);
    }
    
    // Handle raster operations
    QPainter::CompositionMode mode = QPainter::CompositionMode_SourceOver;
    switch (rop) {
        case SRCCOPY:
            mode = QPainter::CompositionMode_Source;
            break;
        case SRCPAINT:
            mode = QPainter::CompositionMode_SourceOver;
            break;
        case SRCAND:
            mode = QPainter::CompositionMode_DestinationIn;
            break;
        case SRCINVERT:
            mode = QPainter::CompositionMode_Xor;
            break;
        default:
            mode = QPainter::CompositionMode_Source;
            break;
    }
    
    QPainter::CompositionMode oldMode = m_painter.compositionMode();
    m_painter.setCompositionMode(mode);
    
    m_painter.drawPixmap(x, y, width, height, srcPixmap, srcX, srcY, width, height);
    
    m_painter.setCompositionMode(oldMode);
}

void QtCanvasAdapter::stretchBlt(int x, int y, int width, int height,
                                 QtCanvasAdapter *src, int srcX, int srcY,
                                 int srcWidth, int srcHeight, int rop)
{
    if (!src) return;
    
    QPaintDevice *srcDevice = src->m_painter.device();
    if (!srcDevice) return;
    
    QPixmap srcPixmap;
    if (QPixmap *pm = dynamic_cast<QPixmap*>(srcDevice)) {
        srcPixmap = *pm;
    }
    
    QPainter::CompositionMode mode = (rop == SRCCOPY) ? 
        QPainter::CompositionMode_Source : QPainter::CompositionMode_SourceOver;
    
    QPainter::CompositionMode oldMode = m_painter.compositionMode();
    m_painter.setCompositionMode(mode);
    
    m_painter.drawPixmap(x, y, width, height, srcPixmap, 
                         srcX, srcY, srcWidth, srcHeight);
    
    m_painter.setCompositionMode(oldMode);
}

void QtCanvasAdapter::textOut(int x, int y, const wchar_t *text, int length)
{
    if (!text) return;
    
    QString str = (length >= 0) ? 
        QString::fromWCharArray(text, length) : 
        QString::fromWCharArray(text);
    
    m_painter.setPen(m_textColor);
    
    if (m_bkMode == 2) { // OPAQUE
        QFontMetrics fm(m_painter.font());
        QRect bounds = fm.boundingRect(str);
        m_painter.fillRect(x, y - bounds.height(), 
                          bounds.width(), bounds.height(), m_bkColor);
    }
    
    m_painter.drawText(x, y, str);
}

void QtCanvasAdapter::drawText(const RECT *rect, const wchar_t *text, int format)
{
    if (!text || !rect) return;
    
    QString str = QString::fromWCharArray(text);
    QRect qrect = rectToQRect(rect);
    
    // Map Win32 DT_* flags to Qt alignment
    int alignment = Qt::AlignLeft | Qt::AlignTop;
    if (format & 0x01) alignment = Qt::AlignCenter; // DT_CENTER
    if (format & 0x02) alignment = Qt::AlignRight;  // DT_RIGHT
    if (format & 0x04) alignment |= Qt::AlignVCenter; // DT_VCENTER
    
    m_painter.setPen(m_textColor);
    
    if (m_bkMode == 2) { // OPAQUE
        m_painter.fillRect(qrect, m_bkColor);
    }
    
    m_painter.drawText(qrect, alignment, str);
}

void QtCanvasAdapter::fillRect(const RECT *rect, COLORREF color)
{
    if (!rect) return;
    
    QRect qrect = rectToQRect(rect);
    m_painter.fillRect(qrect, colorrefToQColor(color));
}

void QtCanvasAdapter::frameRect(const RECT *rect, COLORREF color)
{
    if (!rect) return;
    
    QRect qrect = rectToQRect(rect);
    QPen oldPen = m_painter.pen();
    m_painter.setPen(colorrefToQColor(color));
    m_painter.drawRect(qrect);
    m_painter.setPen(oldPen);
}

void QtCanvasAdapter::drawLine(int x1, int y1, int x2, int y2)
{
    m_painter.drawLine(x1, y1, x2, y2);
}

void QtCanvasAdapter::drawRect(int x, int y, int width, int height)
{
    m_painter.drawRect(x, y, width, height);
}

void QtCanvasAdapter::drawEllipse(int x, int y, int width, int height)
{
    m_painter.drawEllipse(x, y, width, height);
}

void QtCanvasAdapter::setPen(COLORREF color, int width)
{
    m_painter.setPen(QPen(colorrefToQColor(color), width));
}

void QtCanvasAdapter::setBrush(COLORREF color)
{
    m_painter.setBrush(QBrush(colorrefToQColor(color)));
}

void QtCanvasAdapter::setFont(const QFont &font)
{
    m_painter.setFont(font);
}

void QtCanvasAdapter::setTextColor(COLORREF color)
{
    m_textColor = colorrefToQColor(color);
}

void QtCanvasAdapter::setBkColor(COLORREF color)
{
    m_bkColor = colorrefToQColor(color);
}

void QtCanvasAdapter::setBkMode(int mode)
{
    m_bkMode = mode;
}

void QtCanvasAdapter::drawPixmap(int x, int y, const QPixmap &pixmap)
{
    m_painter.drawPixmap(x, y, pixmap);
}

void QtCanvasAdapter::drawImage(int x, int y, const QImage &image)
{
    m_painter.drawImage(x, y, image);
}

void QtCanvasAdapter::setClipRect(const RECT *rect)
{
    if (rect) {
        m_painter.setClipRect(rectToQRect(rect));
    }
}

void QtCanvasAdapter::clearClipRect()
{
    m_painter.setClipping(false);
}

void QtCanvasAdapter::save()
{
    m_painter.save();
}

void QtCanvasAdapter::restore()
{
    m_painter.restore();
}

// Static helper functions
QColor QtCanvasAdapter::colorrefToQColor(COLORREF color)
{
    return QColor(GetRValue(color), GetGValue(color), GetBValue(color));
}

COLORREF QtCanvasAdapter::qcolorToColorref(const QColor &color)
{
    return RGB(color.red(), color.green(), color.blue());
}

QRect QtCanvasAdapter::rectToQRect(const RECT *rect)
{
    if (!rect) return QRect();
    return QRect(rect->left, rect->top, 
                 rect->right - rect->left, 
                 rect->bottom - rect->top);
}

void QtCanvasAdapter::qrectToRect(const QRect &qrect, RECT *rect)
{
    if (rect) {
        rect->left = qrect.left();
        rect->top = qrect.top();
        rect->right = qrect.right();
        rect->bottom = qrect.bottom();
    }
}

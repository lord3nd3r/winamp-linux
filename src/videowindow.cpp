#include "videowindow.h"
#include "translator.h"

VideoWindow::VideoWindow(QWidget *parent) : QWidget(parent) {
    setWindowTitle(TR("win.video.title", "Winamp Video"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, false);  // Don't delete on close, just hide
    resize(320, 240);
    setMinimumSize(160, 120);
    setFocusPolicy(Qt::StrongFocus);  // Accept keyboard focus for F/Escape keys
    setMouseTracking(true);  // Enable cursor updates for resize edges
    
    // Create video widget for actual video rendering — inset by resizeMargin
    // so the parent window's edges are exposed for resize mouse events
    videoWidget = new QVideoWidget(this);
    const int m = resizeMargin;
    videoWidget->setGeometry(m, m, 320 - 2 * m, 240 - 2 * m);
    videoWidget->setStyleSheet("background-color: black;");
    
    // Load logo bitmap
    loadLogo();
    
    isDragging = false;
    isResizing = false;
    wasFullscreen = false;
    resizeEdge = NoEdge;
}

QVideoWidget* VideoWindow::getVideoWidget() {
    return videoWidget;
}

void VideoWindow::setHasVideo(bool has) {
    hasActiveVideo = has;
    if (has) {
        videoWidget->show();
    } else {
        videoWidget->hide();
    }
    update();
}

void VideoWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);  // Fill border area black
    
    // If no video is playing, show the logo
    if (!hasActiveVideo && !logoPixmap.isNull()) {
        QRect r = rect();
        int xp = (r.width() - logoPixmap.width()) / 2;
        int yp = (r.height() - logoPixmap.height()) / 2;
        p.drawPixmap(xp, yp, logoPixmap);
    }
}

void VideoWindow::resizeEvent(QResizeEvent*) {
    // Leave a resize margin around the video widget so edges receive mouse events
    // In fullscreen, fill the entire window
    if (isFullScreen()) {
        videoWidget->setGeometry(0, 0, width(), height());
    } else {
        const int m = resizeMargin;
        videoWidget->setGeometry(m, m, width() - 2 * m, height() - 2 * m);
    }
}

void VideoWindow::showEvent(QShowEvent*) {
    // Track fullscreen state when shown
    bool nowFullscreen = isFullScreen();
    if (nowFullscreen != wasFullscreen) {
        wasFullscreen = nowFullscreen;
        emit fullscreenChanged(nowFullscreen);
    }
}

void VideoWindow::keyPressEvent(QKeyEvent *event) {
    // F or F11 toggles fullscreen, Escape exits fullscreen or closes window
    if (event->key() == Qt::Key_F || event->key() == Qt::Key_F11) {
        toggleFullscreen();
    } else if (event->key() == Qt::Key_Escape) {
        if (isFullScreen()) exitFullscreen();
        else hide();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void VideoWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && !isFullScreen()) {
        // Check for resize edges first
        ResizeEdge edge = hitTestResize(event->pos());
        if (edge != NoEdge) {
            isResizing = true;
            resizeEdge = edge;
            resizeStartPos = event->globalPosition().toPoint();
            resizeStartGeometry = geometry();
            event->accept();
            return;
        }
        // Otherwise, start dragging
        isDragging = true;
        dragStartPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
    } else if (event->button() == Qt::RightButton) {
        // Could show context menu here
    }
}

void VideoWindow::mouseMoveEvent(QMouseEvent *event) {
    if (isFullScreen()) return;
    
    // Handle resizing
    if (isResizing) {
        QPoint delta = event->globalPosition().toPoint() - resizeStartPos;
        QRect newGeom = resizeStartGeometry;
        
        // Adjust geometry based on resize edge
        if (resizeEdge & LeftEdge) {
            int newX = resizeStartGeometry.x() + delta.x();
            int newWidth = resizeStartGeometry.width() - delta.x();
            if (newWidth >= minimumWidth()) {
                newGeom.setX(newX);
                newGeom.setWidth(newWidth);
            }
        }
        if (resizeEdge & RightEdge) {
            newGeom.setWidth(qMax(minimumWidth(), resizeStartGeometry.width() + delta.x()));
        }
        if (resizeEdge & TopEdge) {
            int newY = resizeStartGeometry.y() + delta.y();
            int newHeight = resizeStartGeometry.height() - delta.y();
            if (newHeight >= minimumHeight()) {
                newGeom.setY(newY);
                newGeom.setHeight(newHeight);
            }
        }
        if (resizeEdge & BottomEdge) {
            newGeom.setHeight(qMax(minimumHeight(), resizeStartGeometry.height() + delta.y()));
        }
        
        setGeometry(newGeom);
        return;
    }
    
    // Handle dragging
    if (isDragging) {
        move(event->globalPosition().toPoint() - dragStartPos);
        return;
    }
    
    // Update cursor for resize edges
    ResizeEdge edge = hitTestResize(event->pos());
    updateCursorForEdge(edge);
}

void VideoWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        isResizing = false;
        resizeEdge = NoEdge;
    }
}

void VideoWindow::mouseDoubleClickEvent(QMouseEvent*) {
    toggleFullscreen();
}

VideoWindow::ResizeEdge VideoWindow::hitTestResize(const QPoint &pos) {
    const int margin = resizeMargin;
    bool atLeft = pos.x() < margin;
    bool atRight = pos.x() >= width() - margin;
    bool atTop = pos.y() < margin;
    bool atBottom = pos.y() >= height() - margin;
    
    int edge = NoEdge;
    if (atLeft) edge |= LeftEdge;
    if (atRight) edge |= RightEdge;
    if (atTop) edge |= TopEdge;
    if (atBottom) edge |= BottomEdge;
    
    return static_cast<ResizeEdge>(edge);
}

void VideoWindow::updateCursorForEdge(ResizeEdge edge) {
    QCursor cursor;
    switch (edge) {
        case TopLeft:
        case BottomRight:
            cursor = Qt::SizeFDiagCursor;
            break;
        case TopRight:
        case BottomLeft:
            cursor = Qt::SizeBDiagCursor;
            break;
        case LeftEdge:
        case RightEdge:
            cursor = Qt::SizeHorCursor;
            break;
        case TopEdge:
        case BottomEdge:
            cursor = Qt::SizeVerCursor;
            break;
        default:
            cursor = Qt::ArrowCursor;
            break;
    }
    setCursor(cursor);
    videoWidget->setCursor(cursor);
}

void VideoWindow::toggleFullscreen() {
    if (isFullScreen()) {
        exitFullscreen();
    } else {
        enterFullscreen();
    }
}

void VideoWindow::enterFullscreen() {
    showFullScreen();
    wasFullscreen = true;
    emit fullscreenChanged(true);
}

void VideoWindow::exitFullscreen() {
    showNormal();
    wasFullscreen = false;
    emit fullscreenChanged(false);
}

void VideoWindow::loadLogo() {
    // Try to load video_logo.bmp from skin
    QStringList paths = {
        "skins/default/video_logo.bmp",
        "Src/Winamp/resource/video_logo.bmp"
    };
    
    for (const QString &path : paths) {
        if (QFile::exists(path)) {
            logoPixmap = QPixmap(path);
            if (!logoPixmap.isNull()) {
                break;
            }
        }
    }
}

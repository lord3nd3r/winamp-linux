#ifndef VIDEOWINDOW_H
#define VIDEOWINDOW_H

#include <QWidget>
#include <QVideoWidget>
#include <QPixmap>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFile>
#include <QPoint>
#include <QRect>

class VideoWindow : public QWidget {
    Q_OBJECT
    
public:
    VideoWindow(QWidget *parent = nullptr);
    QVideoWidget* getVideoWidget();
    void setHasVideo(bool has);
    
signals:
    void fullscreenChanged(bool isFullscreen);
    
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void showEvent(QShowEvent*) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    
private:
    enum ResizeEdge {
        NoEdge = 0,
        LeftEdge = 1,
        RightEdge = 2,
        TopEdge = 4,
        BottomEdge = 8,
        TopLeft = TopEdge | LeftEdge,
        TopRight = TopEdge | RightEdge,
        BottomLeft = BottomEdge | LeftEdge,
        BottomRight = BottomEdge | RightEdge
    };
    
    ResizeEdge hitTestResize(const QPoint &pos);
    void updateCursorForEdge(ResizeEdge edge);
    void toggleFullscreen();
    void enterFullscreen();
    void exitFullscreen();
    void loadLogo();
    
    static constexpr int resizeMargin = 6;
    QVideoWidget *videoWidget;
    QPixmap logoPixmap;
    bool hasActiveVideo = false;
    bool isDragging = false;
    bool isResizing = false;
    bool wasFullscreen = false;
    ResizeEdge resizeEdge = NoEdge;
    QPoint dragStartPos;
    QPoint resizeStartPos;
    QRect resizeStartGeometry;
};

#endif // VIDEOWINDOW_H

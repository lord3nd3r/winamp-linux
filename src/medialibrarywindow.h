#ifndef MEDIALIBRARYWINDOW_H
#define MEDIALIBRARYWINDOW_H

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QPixmap>
#include <QColor>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QImage>

class MediaLibraryWindow : public QWidget {
    Q_OBJECT
    
public:
    MediaLibraryWindow(QWidget *parent = nullptr);
    void setPlaylistWindow(QObject *pl) { playlistWindow = pl; }
    
signals:
    void addToPlaylist(const QString &filePath);
    void addToPlaylistRecursive(const QString &dirPath);
    
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    
private slots:
    void onItemDoubleClicked(const QModelIndex &index);
    
private:
    enum ResizeEdge {
        NoEdge = 0,
        LeftEdge = 1,
        RightEdge = 2,
        TopEdge = 4,
        BottomEdge = 8
    };
    
    void loadSkin();
    QPoint getTitleCharPos(QChar ch);
    int getCharWidth(QChar ch);
    ResizeEdge hitTestResize(QPoint pos);
    void updateCursorForEdge(ResizeEdge edge);
    void updateLayout();
    
    QTreeView *treeView;
    QFileSystemModel *fileModel;
    QPixmap genBitmap;
    QPixmap genexBitmap;
    QColor bgColor;
    QColor fgColor;
    
    bool isDragging;
    bool isResizing;
    ResizeEdge resizeEdge;
    QPoint dragStartPos;
    QPoint resizeStartPos;
    QRect resizeStartGeometry;
    
    QObject *playlistWindow = nullptr;
};

#endif // MEDIALIBRARYWINDOW_H

#include "medialibrarywindow.h"
#include "translator.h"

MediaLibraryWindow::MediaLibraryWindow(QWidget *parent) : QWidget(parent) {
    setWindowTitle(TR("win.library.title", "Winamp Library"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, false);  // Don't delete on close, just hide
    resize(275, 300);
    setMinimumSize(275, 200);
    
    // Load gen.bmp and genex.bmp for skinning
    loadSkin();
    
    // Create tree view for browsing music folders
    treeView = new QTreeView(this);
    fileModel = new QFileSystemModel(this);
    
    // Start at user's home music directory or home
    QString musicPath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (musicPath.isEmpty() || !QDir(musicPath).exists()) {
        musicPath = QDir::homePath();
    }
    
    fileModel->setRootPath(musicPath);
    
    // Filter for audio files
    QStringList filters;
    filters << "*.mp3" << "*.flac" << "*.ogg" << "*.wav" << "*.m4a" << "*.aac" 
            << "*.wma" << "*.opus" << "*.mp4" << "*.avi" << "*.mkv" << "*.mov" << "*.webm";
    fileModel->setNameFilters(filters);
    fileModel->setNameFilterDisables(false); // Hide non-matching files
    
    treeView->setModel(fileModel);
    treeView->setRootIndex(fileModel->index(musicPath));
    treeView->setColumnWidth(0, 200);
    treeView->setStyleSheet(QString("QTreeView { background-color: %1; color: %2; border: none; }")
                           .arg(bgColor.name()).arg(fgColor.name()));
    treeView->setSelectionMode(QTreeView::ExtendedSelection);
    
    // Double-click to add to playlist
    connect(treeView, &QTreeView::doubleClicked, this, &MediaLibraryWindow::onItemDoubleClicked);
    
    updateLayout();
    
    isDragging = false;
    isResizing = false;
    resizeEdge = NoEdge;
}

void MediaLibraryWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    
    if (genBitmap.isNull()) {
        // Fallback if skin not loaded
        p.fillRect(rect(), Qt::darkGray);
        return;
    }
    
    // Draw window frame from gen.bmp
    // The layout is similar to minibrowser: corners + edges that tile
    // gen.bmp is 194x109:
    // Top-left corner (0,0) to (24,24)
    // Top-right corner (170,0) to (194,24)
    // Bottom-left corner (0,85) to (24,109)
    // Bottom-right corner (170,85) to (194,109)
    // Left edge: (0,24) height 61
    // Right edge: (170,24) height 61
    // Top edge: (24,0) width 146
    // Bottom edge: (24,85) width 146
    
    int w = width();
    int h = height();
    
    // Corners
    p.drawPixmap(0, 0, genBitmap, 0, 0, 24, 24);                    // top-left
    p.drawPixmap(w - 24, 0, genBitmap, 170, 0, 24, 24);             // top-right
    p.drawPixmap(0, h - 24, genBitmap, 0, 85, 24, 24);              // bottom-left
    p.drawPixmap(w - 24, h - 24, genBitmap, 170, 85, 24, 24);       // bottom-right
    
    // Edges (tiled)
    for (int x = 24; x < w - 24; x += 146) {
        int tileW = qMin(146, w - 24 - x);
        p.drawPixmap(x, 0, genBitmap, 24, 0, tileW, 24);            // top edge
        p.drawPixmap(x, h - 24, genBitmap, 24, 85, tileW, 24);      // bottom edge
    }
    for (int y = 24; y < h - 24; y += 61) {
        int tileH = qMin(61, h - 24 - y);
        p.drawPixmap(0, y, genBitmap, 0, 24, 24, tileH);            // left edge
        p.drawPixmap(w - 24, y, genBitmap, 170, 24, 24, tileH);     // right edge
    }
    
    // Fill center with background color
    p.fillRect(24, 24, w - 48, h - 48, bgColor);
    
    // Draw titlebar text "Winamp Library" using gen.bmp font
    QString title = "Winamp Library";
    int textX = 30;
    for (const QChar &ch : title) {
        QPoint charPos = getTitleCharPos(ch);
        if (charPos.x() >= 0) {
            p.drawPixmap(textX, 6, genBitmap, charPos.x(), charPos.y(), getCharWidth(ch), 10);
            textX += getCharWidth(ch);
        }
    }
}

void MediaLibraryWindow::resizeEvent(QResizeEvent*) {
    updateLayout();
}

void MediaLibraryWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // Check for resize edges first
        ResizeEdge edge = hitTestResize(event->pos());
        if (edge != NoEdge) {
            isResizing = true;
            resizeEdge = edge;
            resizeStartPos = event->globalPosition().toPoint();
            resizeStartGeometry = geometry();
            return;
        }
        
        // Check if clicking in titlebar area (draggable)
        if (event->pos().y() < 24) {
            isDragging = true;
            dragStartPos = event->pos();
        }
    }
}

void MediaLibraryWindow::mouseMoveEvent(QMouseEvent *event) {
    if (isResizing) {
        QPoint delta = event->globalPosition().toPoint() - resizeStartPos;
        QRect newGeom = resizeStartGeometry;
        
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
    
    if (isDragging) {
        move(event->globalPosition().toPoint() - dragStartPos);
        return;
    }
    
    // Update cursor for resize edges
    ResizeEdge edge = hitTestResize(event->pos());
    updateCursorForEdge(edge);
}

void MediaLibraryWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        isResizing = false;
        resizeEdge = NoEdge;
    }
}

void MediaLibraryWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    // Double-click titlebar to close
    if (event->pos().y() < 24) {
        hide();
    }
}

void MediaLibraryWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void MediaLibraryWindow::onItemDoubleClicked(const QModelIndex &index) {
    QString path = fileModel->filePath(index);
    QFileInfo info(path);
    
    if (info.isFile()) {
        // Single file - add to playlist
        emit addToPlaylist(path);
    } else if (info.isDir()) {
        // Directory - add all audio files recursively
        emit addToPlaylistRecursive(path);
    }
}

void MediaLibraryWindow::loadSkin() {
    // Try to load gen.bmp and genex.bmp from skin paths
    QStringList skinPaths = {
        "skins/default",
        "Src/Winamp/resource"
    };
    
    for (const QString &basePath : skinPaths) {
        QString genPath = basePath + "/gen.bmp";
        QString genexPath = basePath + "/genex.bmp";
        
        if (QFile::exists(genPath) && genBitmap.isNull()) {
            genBitmap = QPixmap(genPath);
        }
        if (QFile::exists(genexPath) && genexBitmap.isNull()) {
            genexBitmap = QPixmap(genexPath);
        }
    }
    
    // Load colors from genex.bmp if available
    if (!genexBitmap.isNull()) {
        QImage img = genexBitmap.toImage();
        if (img.width() >= 95 && img.height() >= 1) {
            // Read color palette from genex.bmp (x=48 onwards, every other pixel)
            bgColor = img.pixelColor(52, 0);    // x=52: window background
            fgColor = img.pixelColor(56, 0);    // x=56: window text color
        }
    }
    
    // Fallback colors if not loaded
    if (!bgColor.isValid()) bgColor = QColor(36, 36, 60);
    if (!fgColor.isValid()) fgColor = QColor(255, 255, 255);
}

QPoint MediaLibraryWindow::getTitleCharPos(QChar ch) {
    // gen.bmp titlebar font starts at y=99 (highlighted) and y=100-109 (normal)
    // We'll use the highlighted version for now
    // The font is variable width, first color before 'A' is delimiter
    // For simplicity, use a standard 5-pixel width assumption
    int yPos = 99;  // Highlighted titlebar font row
    
    if (ch >= 'A' && ch <='Z') {
        return QPoint(3 + (ch.toLatin1() - 'A') * 5, yPos);
    } else if (ch >= 'a' && ch <= 'z') {
        return QPoint(3 + (ch.toLatin1() - 'a') * 5, yPos);
    } else if (ch >= '0' && ch <= '9') {
        return QPoint(3 + 26 * 5 + (ch.toLatin1() - '0') * 5, yPos);
    } else if (ch == ' ') {
        return QPoint(3 + 36 * 5, yPos);
    }
    return QPoint(-1, -1); // Unknown character
}

int MediaLibraryWindow::getCharWidth(QChar ch) {
    // For simplicity, assume 5px width for all chars
    // A real implementation would read the delimiter pixel
    if (ch == ' ') return 3;
    return 5;
}

MediaLibraryWindow::ResizeEdge MediaLibraryWindow::hitTestResize(QPoint pos) {
    const int edgeSize = 8;
    int w = width();
    int h = height();
    ResizeEdge edge = NoEdge;
    
    if (pos.x() < edgeSize) edge = (ResizeEdge)(edge | LeftEdge);
    else if (pos.x() > w - edgeSize) edge = (ResizeEdge)(edge | RightEdge);
    
    if (pos.y() < edgeSize) edge = (ResizeEdge)(edge | TopEdge);
    else if (pos.y() > h - edgeSize) edge = (ResizeEdge)(edge | BottomEdge);
    
    return edge;
}

void MediaLibraryWindow::updateCursorForEdge(ResizeEdge edge) {
    if (edge == NoEdge) {
        setCursor(Qt::ArrowCursor);
    } else if (edge == (LeftEdge | TopEdge) || edge == (RightEdge | BottomEdge)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if (edge == (RightEdge | TopEdge) || edge == (LeftEdge | BottomEdge)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (edge & (LeftEdge | RightEdge)) {
        setCursor(Qt::SizeHorCursor);
    } else if (edge & (TopEdge | BottomEdge)) {
        setCursor(Qt::SizeVerCursor);
    }
}

void MediaLibraryWindow::updateLayout() {
    // Position tree view inside the window frame (24px border on all sides)
    if (treeView) {
        treeView->setGeometry(24, 24, width() - 48, height() - 48);
    }
}

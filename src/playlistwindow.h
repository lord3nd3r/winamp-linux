#pragma once
#include <QWidget>
#include <QListWidget>
#include <QMimeData>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDirIterator>
#include <QFileInfo>
#include <QTimer>
#include <QSettings>
#include <QMediaPlayer>
#include <QPainter>
#include <QScrollBar>
#include <QMenu>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QTextStream>
#include <QStandardPaths>
#include <QRandomGenerator>
#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>

class WinampWindow;

// Custom QListWidget that handles drag-drop of files
class PlaylistListWidget : public QListWidget {
    Q_OBJECT
public:
    PlaylistListWidget(QWidget *parent = nullptr) : QListWidget(parent) {}
    
signals:
    void filesDropped(const QStringList &paths);
    // Internal drag signals used to implement "live-swap" reordering like classic Winamp
    void internalDragStarted(int initialRow, const QList<int> &selectedRows);
    void internalDragMoved(int row);
    void internalDragFinished();
    
protected:
    QMimeData* mimeData(const QList<QListWidgetItem*> &items) const override {
        QMimeData *data = new QMimeData();
        QList<QUrl> urls;
        
        for (const QListWidgetItem *item : items) {
            QString filePath = item->data(Qt::UserRole).toString();
            if (!filePath.isEmpty()) {
                urls.append(QUrl::fromLocalFile(filePath));
            }
        }
        
        if (!urls.isEmpty()) {
            data->setUrls(urls);
        }
        
        return data;
    }

    // Create our own QDrag instead of calling QListWidget::startDrag().
    // The base implementation auto-removes items when a MoveAction drop completes,
    // which conflicts with our live-swap reordering.  We handle all reordering
    // ourselves via internalDragMoved → PlaylistWindow::liveReorderMove.
    void startDrag(Qt::DropActions supportedActions) override {
        QList<int> rows;
        for (const QModelIndex &idx : selectedIndexes()) rows.append(idx.row());
        std::sort(rows.begin(), rows.end());
        int first = rows.isEmpty() ? -1 : rows.first();
        emit internalDragStarted(first, rows);

        // Build our own drag so we control what happens after exec() returns.
        QDrag *drag = new QDrag(this);
        QMimeData *data = mimeData(selectedItems());
        if (!data) { delete drag; emit internalDragFinished(); return; }
        drag->setMimeData(data);
        drag->exec(supportedActions, Qt::CopyAction);   // CopyAction default → cursor hint
        // Do NOT remove source items — live swap already repositioned everything.
        emit internalDragFinished();
    }

    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->source() == this) {
            // internal drag — accept and let dragMoveEvent handle live swaps
            event->acceptProposedAction();
            return;
        }
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        } else {
            QListWidget::dragEnterEvent(event);
        }
    }
    
    void dragMoveEvent(QDragMoveEvent *event) override {
        if (event->source() == this) {
            event->acceptProposedAction();
            // report the logical row under the mouse to the parent for live-swap
            int row = indexAt(event->position().toPoint()).row();
            if (row < 0) row = count() - 1;
            emit internalDragMoved(row);
            // simple autoscroll when near edges
            const int margin = 18;
            if (event->position().toPoint().y() < margin) {
                verticalScrollBar()->setValue(verticalScrollBar()->value() - 1);
            } else if (event->position().toPoint().y() > height() - margin) {
                verticalScrollBar()->setValue(verticalScrollBar()->value() + 1);
            }
            return;
        }

        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        } else {
            QListWidget::dragMoveEvent(event);
        }
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override {
        // dragLeaveEvent does not provide source(); always notify finish (safe no-op if not internal)
        emit internalDragFinished();
        QListWidget::dragLeaveEvent(event);
    }
    
    void dropEvent(QDropEvent *event) override {
        // Internal drag — live-swap already handled all reordering during dragMoveEvent.
        // Accept with IgnoreAction so Qt doesn't auto-remove source items.
        if (event->source() == this) {
            event->setDropAction(Qt::IgnoreAction);
            event->accept();
            return;
        }

        if (event->mimeData()->hasUrls()) {
            QStringList paths;
            QStringList audioExts = {"mp3", "wav", "flac", "ogg", "m4a", "aac", "wma", "opus",
                                     "mp4", "avi", "mkv", "mov", "webm"};
            for (const QUrl &url : event->mimeData()->urls()) {
                QString path = url.toLocalFile();
                if (path.isEmpty()) continue;
                QFileInfo fi(path);
                if (fi.isDir()) {
                    QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
                    while (it.hasNext()) {
                        it.next();
                        if (audioExts.contains(it.fileInfo().suffix().toLower()))
                            paths << it.filePath();
                    }
                } else if (fi.isFile()) {
                    paths << path;
                }
            }
            event->acceptProposedAction();
            if (!paths.isEmpty()) {
                QStringList captured = paths;
                QTimer::singleShot(0, this, [this, captured]() {
                    emit filesDropped(captured);
                });
            }
            return;
        }
        QListWidget::dropEvent(event);
    }
};

// Playlist Window
class PlaylistWindow : public QWidget {
    Q_OBJECT
public:
    PlaylistWindow(WinampWindow *parent = nullptr);
    void addTrack(const QString &filePath);
    void clearPlaylist();
    bool isSnapped() const { return isSnappedToMain; }
    void followMain();
    void checkSnap();
    void saveSettings(QSettings &s);
    void loadSettings(QSettings &s);

    // Track navigation accessors
    int trackCount() const { return tracks.size(); }
    QString trackAt(int index) const { return (index >= 0 && index < tracks.size()) ? tracks[index] : QString(); }
    int currentTrackIndex() const { return listWidget->currentRow(); }
    void setCurrentTrackIndex(int index) { if (index >= 0 && index < tracks.size()) listWidget->setCurrentRow(index); }
    QStringList allTracks() const { return tracks; }
    
    // Track navigation convenience methods (called by MPRIS2, tray menu, etc.)
    void nextTrack() {
        int idx = currentTrackIndex();
        if (idx + 1 < trackCount()) {
            setCurrentTrackIndex(idx + 1);
            emit trackDoubleClicked(trackAt(idx + 1));
        }
    }
    void prevTrack() {
        int idx = currentTrackIndex();
        if (idx > 0) {
            setCurrentTrackIndex(idx - 1);
            emit trackDoubleClicked(trackAt(idx - 1));
        }
    }
    void playCurrentTrack() {
        int idx = currentTrackIndex();
        if (idx >= 0 && idx < trackCount())
            emit trackDoubleClicked(trackAt(idx));
    }
    
    // Appearance
    void applyPlaylistColors();
    void setPlaylistFont(const QString &family, int size) {
        playlistFontFamily = family;
        playlistFontSize = size;
        applyPlaylistColors();
    }
    
    // Window shade mode (compact single-line view)
    void toggleShadeMode() {
        shadeMode = !shadeMode;
        if (shadeMode) {
            savedHeight = height();
            setMinimumSize(275, 14);
            resize(width(), 14);
        } else {
            setMinimumSize(275, 116);
            resize(width(), savedHeight > 116 ? savedHeight : 232);
            updateListGeometry();
        }
        listWidget->setVisible(!shadeMode);
        update();
    }
    bool isShadeMode() const { return shadeMode; }

signals:
    void trackDoubleClicked(const QString &filePath);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void updateTotalTimeDisplay();
    void updateListGeometry();
    void paintModernPlaylist(QPainter &p);
    void drawText(QPainter &painter, const QString &text, int x, int y);
    QPoint getTextCharPos(QChar ch);
    void showAddMenu(QPoint globalPos);
    void showRemMenu(QPoint globalPos);
    void showSelMenu(QPoint globalPos);
    void showMiscMenu(QPoint globalPos);
    void showListMenu(QPoint globalPos);
    void showContextMenu(QPoint globalPos);
    
    // Resize edge detection
    enum ResizeEdge { NoEdge = 0, RightEdge = 1, BottomEdge = 2, BottomRight = 3 };
    ResizeEdge hitTestResize(const QPoint &pos) {
        const int margin = 6;
        bool atRight = pos.x() >= width() - margin;
        bool atBottom = pos.y() >= height() - margin;
        if (atRight && atBottom) return BottomRight;
        if (atRight) return RightEdge;
        if (atBottom) return BottomEdge;
        return NoEdge;
    }
    
    // Playlist operations
    void removeSelected();
    void cropSelected();
    void removeDeadFiles();
    void moveSelectedUp();
    void moveSelectedDown();
    void sortByTitle();
    void sortByFilename();
    void sortByPath();
    void reverseList();
    void randomizeList();
    void exploreFolderOfSelected();
    void generateHtmlPlaylist();
    QString trackDisplayName(int index, const QString &filePath) {
        return QString("%1. %2").arg(index + 1).arg(QFileInfo(filePath).fileName());
    }
    void rebuildListDisplay() {
        listWidget->clear();
        for (int i = 0; i < tracks.size(); i++) {
            QListWidgetItem *item = new QListWidgetItem(trackDisplayName(i, tracks[i]));
            item->setData(Qt::UserRole, tracks[i]);
            listWidget->addItem(item);
        }
    }

    PlaylistListWidget *listWidget;
    QList<QString> tracks;
    QList<qint64> trackDurations;
    QString totalTimeStr;
    QPoint dragPosition;
    bool isDragging = false;
    bool isSnappedToMain = false;
    WinampWindow *mainWindow = nullptr;
    int snapMode = 0;

    // Live-reorder state (Windows-like "live swap" while dragging)
    bool liveReorderActive = false;
    int liveMovePos = -1; // logical row under mouse during live reorder
    QList<QPair<QString,int>> liveSelectedKeys; // (filePath, occurrenceIndex) to reselect correctly
    QPair<QString,int> liveCurrentKey; // previous current item key to restore current row

    // Live-reorder helpers
    void startLiveReorder(int initialRow, const QList<int> &selectedRows);
    void liveReorderMove(int row);
    void finishLiveReorder();
    QList<QPair<QString,int>> captureSelectionKeys(const QList<int> &rows);
    void restoreSelectionFromKeys(const QList<QPair<QString,int>> &keys);
    int findIndexForKey(const QPair<QString,int> &key);
    
    // Shade mode
    bool shadeMode = false;
    int savedHeight = 232;
    
    // Resize
    ResizeEdge resizeEdge = NoEdge;
    bool isResizing = false;
    QPoint resizeStartPos;
    QSize resizeStartSize;
    
    // Scrollbar dragging
    bool isDraggingScrollbar = false;
    
    // Font settings
    QString playlistFontFamily = "Courier New";
    int playlistFontSize = 8;
};

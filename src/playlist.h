// playlist.h — Winamp Playlist Editor
#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QFileDialog>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QMimeData>
#include <QUrl>
#include <QScrollBar>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QVector>
#include <QPair>
#include "constants.h"
#include "translator.h"
#include "bookmark_manager.h"
#include "dialogs.h"
#include <QSettings>
#include <atomic>
#include <memory>

class QThread;

class WinampWindow;
class PlaylistWindow;

class PlaylistListWidget : public QListWidget {
    Q_OBJECT
public:
    PlaylistListWidget(QWidget *parent = nullptr) : QListWidget(parent) {}
    
protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QMimeData* mimeData(const QList<QListWidgetItem*> &items) const override {
#else
    QMimeData* mimeData(const QList<QListWidgetItem*> items) const {
#endif
        // Keep QListWidget's own format so internal reordering works; add file URLs for drag-out.
        QMimeData *data = QListWidget::mimeData(items);
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
};

class PlaylistWindow : public QWidget {
    Q_OBJECT
    friend class TestPlaylist;
    friend class PlaylistListWidget;
public:
    PlaylistWindow(WinampWindow *parent = nullptr);
    ~PlaylistWindow() override;
    void addTrack(const QString &filePath);
    // Append every entry in an M3U/PLS body. `base` resolves relative paths
    // (playlist directory, or the remote playlist URL). Returns how many were added.
    int addPlaylistText(const QByteArray &data, const QUrl &base);
    void addFolderAsync(const QString &dirPath, bool autoPlay = false);
    void clearPlaylist();
    bool isSnapped() const { return isSnappedToMain && snapMode != 0; }
    void followMain();
    void checkSnap();
    void setSnapMode(int mode) {
        snapMode = mode;
        isSnappedToMain = (mode != 0);
    }
    int currentSnapMode() const { return snapMode; }
    // Place flush to the right of main and mark docked (Winamp 2.x default).
    void dockRightOfMain();
    void saveSettings(QSettings &s);
    void loadSettings(QSettings &s);

    // Track navigation accessors
    int trackCount() const { return tracks.size(); }
    QString trackAt(int index) const { return (index >= 0 && index < tracks.size()) ? tracks[index] : QString(); }
    int currentTrackIndex() const { return listWidget->currentRow(); }
    void setCurrentTrackIndex(int index) { if (index >= 0 && index < tracks.size()) listWidget->setCurrentRow(index); }
    // Selection (current row) and the track that is actually playing are
    // separate. A single click must not retarget Next or gapless.
    int playingTrackIndex() const {
        if (playingIndex >= 0 && playingIndex < tracks.size() && tracks[playingIndex] == playingPath)
            return playingIndex;
        if (!playingPath.isEmpty())
            return tracks.indexOf(playingPath);
        return -1;
    }
    void notePlayingTrack(const QString &file) {
        playingPath = file;
        const int selected = currentTrackIndex();
        if (selected >= 0 && trackAt(selected) == file)
            playingIndex = selected;
        else
            playingIndex = tracks.indexOf(file);
    }
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
    void setUseCustomFont(bool enabled) { useCustomPlaylistFont = enabled; applyPlaylistColors(); }
    void setPlaylistFontFamily(const QString &family) { playlistFontFamily = family; applyPlaylistColors(); }
    void setPlaylistFontSizeOnly(int size) { playlistFontSize = size; applyPlaylistColors(); }
    void setRecycleBinEnabled(bool enabled) { playlistRecycleBinEnabled = enabled; }
    void setShowTrackNumbers(bool show) { showTrackNumbersInPlaylist = show; rebuildListDisplay(); }
    void restoreLastRemoved();
    // Read-only accessors so Preferences can seed its dialog from real state.
    bool customFontEnabled() const { return useCustomPlaylistFont; }
    QString fontFamily() const { return playlistFontFamily; }
    int fontSize() const { return playlistFontSize; }
    bool recycleBinEnabled() const { return playlistRecycleBinEnabled; }
    bool trackNumbersShown() const { return showTrackNumbersInPlaylist; }
    
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

    // Rebuild list rows (e.g. after ID3 tags are edited).
    void rebuildListDisplay() {
        listWidget->clear();
        for (int i = 0; i < tracks.size(); i++) {
            QListWidgetItem *item = new QListWidgetItem(trackDisplayName(i, tracks[i]));
            item->setData(Qt::UserRole, tracks[i]);
            listWidget->addItem(item);
        }
    }

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
    QString trackDisplayName(int index, const QString &filePath);
    // Internal drag reorder. destinationRow is the insertion index in the
    // current list (0..count, count means append). Does not insert copies.
    void moveTrackRows(const QList<int> &rows, int destinationRow);
    // QListModel::moveRows() reports a destination that may equal size()
    // (append). Mirror that adjustment so QList::move() is never called
    // with an index equal to size().
    void applyRowsMoved(int sourceStart, int sourceEnd, int destinationRow);

    PlaylistListWidget *listWidget;
    QList<QString> tracks;
    QList<qint64> trackDurations; // Store durations in milliseconds
    QString totalTimeStr; // Formatted string for display
    QPoint dragPosition;
    bool isDragging = false;
    bool isSnappedToMain = false;
    WinampWindow *mainWindow = nullptr;
    int snapMode = 0;  // 0=none, 1=right of main, 2=below EQ, 3=below main
    int snapOffsetY = 0; // vertical offset from the main window when snapped to its side
    
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
    bool useCustomPlaylistFont = false;

    // Preferences-driven playlist behavior
    bool playlistRecycleBinEnabled = false;
    bool showTrackNumbersInPlaylist = true;
    QVector<QPair<int, QString>> removedTracksStack; // capped in removeSelected()

    // Async duration probe queue (replaces blocking QEventLoop per-track).
    // One probe is in flight. A status is applied only when the player source
    // is still the path that probe asked for, and the timeout is cancelled
    // when that probe finishes.
    QMediaPlayer *durationProbePlayer = nullptr;
    QTimer *durationProbeTimer = nullptr;
    QStringList durationProbeQueue;
    QString durationProbePath;
    QString playingPath;
    int playingIndex = -1;
    quint64 durationProbeSerial = 0;
    bool durationProbeActive = false;
    bool closing = false;
    void startNextDurationProbe();
    void cancelDurationProbes();
    void onDurationProbeStatus(QMediaPlayer::MediaStatus status);
    void onDurationProbeTimeout();

    // Folder scans run off the GUI thread. The flag is cleared and the threads
    // are joined in the destructor so a late callback cannot touch a dead window.
    std::shared_ptr<std::atomic<bool>> folderScanAlive =
        std::make_shared<std::atomic<bool>>(true);
    QList<QThread *> folderScanThreads;
};


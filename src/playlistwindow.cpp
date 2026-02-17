#include "playlistwindow.h"
#include "winampwindow.h"
#include "skinutils.h"
#include "winampbitmaps.h"
#include "modernskinengine.h"
#include "translator.h"
#include <algorithm>
#include <random>
#include <QAbstractItemView>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDialog>
#include <QToolTip>

PlaylistWindow::PlaylistWindow(WinampWindow *parent) : QWidget(nullptr), mainWindow(parent) {
    setMinimumSize(275, 116);
    resize(275, 232);
    setWindowTitle(TR("win.playlist.title", "Winamp Playlist Editor"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    setAcceptDrops(true);  // Accept file drops on playlist window borders/titlebar
    
    // Position list widget within the skin frame
    // Titlebar=20px, left border=12px, right border=20px (incl scrollbar), bottom=38px
    listWidget = new PlaylistListWidget(this);
    updateListGeometry();
    applyPlaylistColors();
    listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // Enable drag and drop for files
    listWidget->setAcceptDrops(true);
    listWidget->setDragEnabled(true);
    listWidget->setDropIndicatorShown(true);
    listWidget->setDragDropMode(QAbstractItemView::DragDrop); // Allow both internal move and drag-out
    
    // Enable resizing from edges and corners
    setMouseTracking(true);
    
    // Connect signals for drag and drop
    connect(listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (item) {
            int index = listWidget->row(item);
            emit trackDoubleClicked(tracks[index]);
        }
    });

    // Connect file drop signal from the list widget
    connect(listWidget, &PlaylistListWidget::filesDropped, this, [this](const QStringList &paths) {
        for (const QString &p : paths)
            addTrack(p);
    });

    connect(listWidget->model(), &QAbstractItemModel::rowsMoved, this, [this](const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row) {
        // Rearrange tracks and durations lists to match the new order in the list widget
        for (int i = 0; i <= end - start; ++i) {
            tracks.move(start, row + i);
            trackDurations.move(start, row + i);
        }
    });

    // Install event filter for keyboard shortcuts and right-click context menu
    listWidget->installEventFilter(this);
}

void PlaylistWindow::applyPlaylistColors() {
    if (g_isModernSkin) {
        listWidget->setStyleSheet(
            QString("QListWidget {"
            "  background-color: #1b1c28;"
            "  color: #c0c0d0;"
            "  border: none;"
            "  font-family: 'Arial', 'Helvetica';"
            "  font-size: 8pt;"
            "  selection-background-color: #3a3b52;"
            "  selection-color: #ffffff;"
            "}"
            "QListWidget::item {"
            "  padding: 1px 2px;"
            "}"
            "QListWidget::item:hover {"
            "  background-color: #2a2b3e;"
            "}")
        );
    } else {
        listWidget->setStyleSheet(
            QString("QListWidget {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: none;"
            "  font-family: 'Courier New', 'Courier';"
            "  font-size: %3pt;"
            "  selection-background-color: %4;"
            "  selection-color: %5;"
            "}"
            "QListWidget::item {"
            "  padding: 0px;"
            "}")
            .arg(g_plColors.normBg.name())
            .arg(g_plColors.normal.name())
            .arg(playlistFontSize)
            .arg(g_plColors.selectBg.name())
            .arg(g_plColors.current.name())
        );
    }
}

void PlaylistWindow::updateListGeometry() {
    if (g_isModernSkin) {
        // Modern skin: 6px side borders, ~22px top (titlebar 18 + border 5 - 1), 35px bottom (buttons)
        listWidget->setGeometry(6, 22, width() - 12, height() - 22 - 35);
    } else {
        listWidget->setGeometry(12, 20, width() - 12 - 20, height() - 20 - 38);
    }
}

void PlaylistWindow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateListGeometry();
}

bool PlaylistWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == listWidget) {
        // Right-click on list widget → show context menu
        if (event->type() == QEvent::ContextMenu) {
            QContextMenuEvent *ce = static_cast<QContextMenuEvent*>(event);
            showContextMenu(ce->globalPos());
            return true;
        }
        if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        Qt::KeyboardModifiers mods = ke->modifiers();
        int key = ke->key();

        // Delete — remove selected
        if (key == Qt::Key_Delete && mods == Qt::NoModifier) {
            removeSelected(); return true;
        }
        // Ctrl+Delete — crop to selected
        if (key == Qt::Key_Delete && mods == Qt::ControlModifier) {
            cropSelected(); return true;
        }
        // Ctrl+Shift+Delete — clear playlist
        if (key == Qt::Key_Delete && mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
            clearPlaylist(); return true;
        }
        // Alt+Delete — remove missing/dead files
        if (key == Qt::Key_Delete && mods == Qt::AltModifier) {
            removeDeadFiles(); return true;
        }
        // Enter — play selected
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            int row = listWidget->currentRow();
            if (row >= 0 && row < tracks.size())
                emit trackDoubleClicked(tracks[row]);
            return true;
        }
        // Ctrl+A — select all
        if (key == Qt::Key_A && mods == Qt::ControlModifier) {
            listWidget->selectAll(); return true;
        }
        // Ctrl+I — invert selection
        if (key == Qt::Key_I && mods == Qt::ControlModifier) {
            for (int i = 0; i < listWidget->count(); i++)
                listWidget->item(i)->setSelected(!listWidget->item(i)->isSelected());
            return true;
        }
        // Ctrl+R — reverse list
        if (key == Qt::Key_R && mods == Qt::ControlModifier) {
            reverseList(); return true;
        }
        // Ctrl+Shift+R — randomize list
        if (key == Qt::Key_R && mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
            randomizeList(); return true;
        }
        // Ctrl+Shift+1 — sort by title
        if (key == Qt::Key_1 && mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
            sortByTitle(); return true;
        }
        // Ctrl+Shift+2 — sort by filename
        if (key == Qt::Key_2 && mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
            sortByFilename(); return true;
        }
        // Ctrl+Shift+3 — sort by path
        if (key == Qt::Key_3 && mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
            sortByPath(); return true;
        }
        // Alt+Up — move selection up
        if (key == Qt::Key_Up && mods == Qt::AltModifier) {
            moveSelectedUp(); return true;
        }
        // Alt+Down — move selection down
        if (key == Qt::Key_Down && mods == Qt::AltModifier) {
            moveSelectedDown(); return true;
        }
        // Ctrl+F — explore folder
        if (key == Qt::Key_F && mods == Qt::ControlModifier) {
            exploreFolderOfSelected(); return true;
        }
        // Ctrl+N — new playlist
        if (key == Qt::Key_N && mods == Qt::ControlModifier) {
            clearPlaylist(); return true;
        }
        // Ctrl+O — open playlist
        if (key == Qt::Key_O && mods == Qt::ControlModifier) {
            showListMenu(QCursor::pos()); return true;
        }
        // Ctrl+S — save playlist
        if (key == Qt::Key_S && mods == Qt::ControlModifier) {
            // Trigger save directly
            QString fileName = QFileDialog::getSaveFileName(this, "Save Playlist", "",
                "M3U Playlist (*.m3u);;M3U8 Playlist (*.m3u8);;All Files (*)");
            if (!fileName.isEmpty()) {
                QFile file(fileName);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&file);
                    out << "#EXTM3U\n";
                    for (int i = 0; i < tracks.size(); i++) {
                        qint64 durSec = (i < trackDurations.size()) ? trackDurations[i] / 1000 : -1;
                        QString title = QFileInfo(tracks[i]).baseName();
                        out << "#EXTINF:" << durSec << "," << title << "\n";
                        out << tracks[i] << "\n";
                    }
                    file.close();
                }
            }
            return true;
        }
        // Ctrl+Alt+G — generate HTML playlist
        if (key == Qt::Key_G && mods == (Qt::ControlModifier | Qt::AltModifier)) {
            generateHtmlPlaylist(); return true;
        }
        // L — add file(s)
        if (key == Qt::Key_L && mods == Qt::NoModifier) {
            showAddMenu(QCursor::pos()); return true;
        }
        // Home — scroll to top
        if (key == Qt::Key_Home && mods == Qt::NoModifier) {
            listWidget->scrollToTop();
            if (listWidget->count() > 0) listWidget->setCurrentRow(0);
            return true;
        }
        // End — scroll to bottom
        if (key == Qt::Key_End && mods == Qt::NoModifier) {
            listWidget->scrollToBottom();
            if (listWidget->count() > 0) listWidget->setCurrentRow(listWidget->count() - 1);
            return true;
        }
        } // end KeyPress
    } // end obj == listWidget
    return QWidget::eventFilter(obj, event);
}

// ---- Playlist operation helpers (used by menus and keyboard shortcuts) ----

void PlaylistWindow::removeSelected() {
    QList<int> rows;
    for (QListWidgetItem *item : listWidget->selectedItems())
        rows.append(listWidget->row(item));
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows) {
        if (row >= 0 && row < tracks.size()) {
            tracks.removeAt(row);
            if (row < trackDurations.size())
                trackDurations.removeAt(row);
        }
    }
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::cropSelected() {
    QList<QListWidgetItem*> selectedItems = listWidget->selectedItems();
    QStringList newTracks;
    QList<qint64> newDurations;
    for (QListWidgetItem *item : selectedItems) {
        int row = listWidget->row(item);
        if (row >= 0 && row < tracks.size()) {
            newTracks.append(tracks[row]);
            newDurations.append(row < trackDurations.size() ? trackDurations[row] : 0);
        }
    }
    tracks = newTracks;
    trackDurations = newDurations;
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::removeDeadFiles() {
    int removed = 0;
    for (int i = tracks.size() - 1; i >= 0; i--) {
        if (!QFile::exists(tracks[i])) {
            tracks.removeAt(i);
            if (i < trackDurations.size())
                trackDurations.removeAt(i);
            removed++;
        }
    }
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::moveSelectedUp() {
    QList<int> rows;
    for (QListWidgetItem *item : listWidget->selectedItems())
        rows.append(listWidget->row(item));
    std::sort(rows.begin(), rows.end());
    if (rows.isEmpty() || rows.first() == 0) return;
    for (int row : rows) {
        if (row > 0 && row < tracks.size()) {
            tracks.swapItemsAt(row, row - 1);
            if (row < trackDurations.size() && row - 1 < trackDurations.size())
                trackDurations.swapItemsAt(row, row - 1);
        }
    }
    rebuildListDisplay();
    // Re-select moved items
    for (int row : rows) {
        if (row - 1 >= 0 && row - 1 < listWidget->count())
            listWidget->item(row - 1)->setSelected(true);
    }
    listWidget->setCurrentRow(rows.first() - 1);
}

void PlaylistWindow::moveSelectedDown() {
    QList<int> rows;
    for (QListWidgetItem *item : listWidget->selectedItems())
        rows.append(listWidget->row(item));
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    if (rows.isEmpty() || rows.first() >= tracks.size() - 1) return;
    for (int row : rows) {
        if (row >= 0 && row + 1 < tracks.size()) {
            tracks.swapItemsAt(row, row + 1);
            if (row < trackDurations.size() && row + 1 < trackDurations.size())
                trackDurations.swapItemsAt(row, row + 1);
        }
    }
    rebuildListDisplay();
    for (int row : rows) {
        if (row + 1 < listWidget->count())
            listWidget->item(row + 1)->setSelected(true);
    }
    std::sort(rows.begin(), rows.end());
    listWidget->setCurrentRow(rows.first() + 1);
}

void PlaylistWindow::sortByTitle() {
    // Sort by display filename (baseName)
    QList<QPair<QString, qint64>> combined;
    for (int i = 0; i < tracks.size(); i++)
        combined.append({tracks[i], i < trackDurations.size() ? trackDurations[i] : 0});
    std::sort(combined.begin(), combined.end(), [](const auto &a, const auto &b) {
        return QFileInfo(a.first).baseName().toLower() < QFileInfo(b.first).baseName().toLower();
    });
    tracks.clear(); trackDurations.clear();
    for (const auto &p : combined) { tracks.append(p.first); trackDurations.append(p.second); }
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::sortByFilename() {
    QList<QPair<QString, qint64>> combined;
    for (int i = 0; i < tracks.size(); i++)
        combined.append({tracks[i], i < trackDurations.size() ? trackDurations[i] : 0});
    std::sort(combined.begin(), combined.end(), [](const auto &a, const auto &b) {
        return QFileInfo(a.first).fileName().toLower() < QFileInfo(b.first).fileName().toLower();
    });
    tracks.clear(); trackDurations.clear();
    for (const auto &p : combined) { tracks.append(p.first); trackDurations.append(p.second); }
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::sortByPath() {
    QList<QPair<QString, qint64>> combined;
    for (int i = 0; i < tracks.size(); i++)
        combined.append({tracks[i], i < trackDurations.size() ? trackDurations[i] : 0});
    std::sort(combined.begin(), combined.end(), [](const auto &a, const auto &b) {
        return a.first.toLower() < b.first.toLower();
    });
    tracks.clear(); trackDurations.clear();
    for (const auto &p : combined) { tracks.append(p.first); trackDurations.append(p.second); }
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::reverseList() {
    std::reverse(tracks.begin(), tracks.end());
    std::reverse(trackDurations.begin(), trackDurations.end());
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::randomizeList() {
    QList<QPair<QString, qint64>> combined;
    for (int i = 0; i < tracks.size(); i++)
        combined.append({tracks[i], i < trackDurations.size() ? trackDurations[i] : 0});
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(combined.begin(), combined.end(), g);
    tracks.clear(); trackDurations.clear();
    for (const auto &p : combined) { tracks.append(p.first); trackDurations.append(p.second); }
    rebuildListDisplay();
    updateTotalTimeDisplay();
}

void PlaylistWindow::exploreFolderOfSelected() {
    int row = listWidget->currentRow();
    if (row >= 0 && row < tracks.size()) {
        QString folder = QFileInfo(tracks[row]).absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    }
}

void PlaylistWindow::generateHtmlPlaylist() {
    QString fileName = QFileDialog::getSaveFileName(this, "Generate HTML Playlist", "",
        "HTML Files (*.html);;All Files (*)");
    if (fileName.isEmpty()) return;
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    out << "<html><head><title>Winamp Generated Playlist</title>\n";
    out << "<style>body{background:#000;color:#0f0;font-family:monospace;}";
    out << "table{border-collapse:collapse;width:100%;}";
    out << "td{padding:2px 8px;border-bottom:1px solid #333;}";
    out << "tr:hover{background:#003;}h1{color:#0f0;}</style></head>\n";
    out << "<body><h1>Winamp Playlist</h1>\n";
    out << "<p>" << tracks.size() << " tracks</p>\n";
    out << "<table><tr><th>#</th><th>Title</th><th>File</th></tr>\n";
    for (int i = 0; i < tracks.size(); i++) {
        out << "<tr><td>" << (i+1) << "</td><td>" 
            << QFileInfo(tracks[i]).baseName().toHtmlEscaped() 
            << "</td><td>" << tracks[i].toHtmlEscaped() << "</td></tr>\n";
    }
    out << "</table></body></html>\n";
    file.close();
    // Open it in default browser
    QDesktopServices::openUrl(QUrl::fromLocalFile(fileName));
}

void PlaylistWindow::updateTotalTimeDisplay() {
    qint64 totalMilliseconds = 0;
    for (qint64 duration : trackDurations) {
        totalMilliseconds += duration;
    }

    qint64 totalSeconds = totalMilliseconds / 1000;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    if (hours > 0) {
        totalTimeStr = QString("%1 tracks, %2h %3m %4s").arg(tracks.size()).arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
    } else {
        totalTimeStr = QString("%1 tracks, %2m %3s").arg(tracks.size()).arg(minutes).arg(seconds, 2, 10, QChar('0'));
    }
    
    // Request a repaint to show the new time
    update();
}

void PlaylistWindow::addTrack(const QString &filePath) {
    QFileInfo fileInfo(filePath);
    if (fileInfo.exists() && fileInfo.isFile()) {
        QListWidgetItem *item = new QListWidgetItem(trackDisplayName(tracks.size(), filePath));
        item->setData(Qt::UserRole, filePath); // Store full file path for drag-out
        listWidget->addItem(item);
        tracks.append(filePath);
        trackDurations.append(0); // placeholder until async probe completes

        // Probe duration asynchronously to avoid re-entrant event loop crashes
        // (the old blocking QEventLoop could crash when called during drag-drop)
        int trackIndex = tracks.size() - 1;
        QMediaPlayer *probe = new QMediaPlayer(this);
        probe->setSource(QUrl::fromLocalFile(filePath));
        connect(probe, &QMediaPlayer::mediaStatusChanged, this, [this, probe, trackIndex](QMediaPlayer::MediaStatus status){
            if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::InvalidMedia) {
                if (trackIndex < trackDurations.size()) {
                    trackDurations[trackIndex] = (status == QMediaPlayer::LoadedMedia) ? probe->duration() : 0;
                    updateTotalTimeDisplay();
                }
                probe->deleteLater();
            }
        });
    }
}

void PlaylistWindow::clearPlaylist() {
    listWidget->clear();
    tracks.clear();
    trackDurations.clear();
    updateTotalTimeDisplay();
}

void PlaylistWindow::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    
    // ---- Modern skin mode ----
    if (g_isModernSkin && g_modernSkin) {
        paintModernPlaylist(painter);
        return;
    }
    
    auto &bmp = WinampBitmaps::instance();
    int w = width();
    int h = height();
    
    if (bmp.pledit.isNull()) {
        painter.fillRect(rect(), QColor(0, 0, 0));
        painter.setPen(QColor(0, 255, 0));
        painter.setFont(QFont("Tahoma", 7, QFont::Bold));
        painter.drawText(6, 10, "Winamp Playlist Editor");
        return;
    }
    
    // === Compose playlist skin from Pledit.bmp sprite pieces ===
    
    // --- Titlebar (20px tall) ---
    bool active = isActiveWindow();
    int tbRow = active ? 0 : 21;  // active at y=0, inactive at y=21
    // Left corner 25x20
    painter.drawPixmap(0, 0, bmp.pledit, 0, tbRow, 25, 20);
    // Title text area 100x20 (centered)
    int titleMid = (w - 100) / 2;
    painter.drawPixmap(titleMid, 0, bmp.pledit, 26, tbRow, 100, 20);
    // Fill between left corner and title with filler tile
    for (int fx = 25; fx < titleMid; fx += 25)
        painter.drawPixmap(fx, 0, bmp.pledit, 127, tbRow, qMin(25, titleMid - fx), 20);
    // Fill between title and right corner with filler tile  
    for (int fx = titleMid + 100; fx < w - 25; fx += 25)
        painter.drawPixmap(fx, 0, bmp.pledit, 127, tbRow, qMin(25, w - 25 - fx), 20);
    // Right corner 25x20
    painter.drawPixmap(w - 25, 0, bmp.pledit, 153, tbRow, 25, 20);
    
    // --- Side borders (between titlebar and bottom bar) ---
    int bodyTop = 20;
    int bodyBottom = h - 38;  // bottom bar is 38px
    // Left border: 12px wide, tile 29px tall from (0,42)
    for (int fy = bodyTop; fy < bodyBottom; fy += 29)
        painter.drawPixmap(0, fy, bmp.pledit, 0, 42, 12, qMin(29, bodyBottom - fy));
    // Right border: 5+7=12px wide, from (31,42) and (44,42)
    for (int fy = bodyTop; fy < bodyBottom; fy += 29) {
        int tileH = qMin(29, bodyBottom - fy);
        painter.drawPixmap(w - 20, fy, bmp.pledit, 31, 42, 5, tileH);   // left part of right border
        painter.drawPixmap(w - 15, fy, bmp.pledit, 36, 42, 8, tileH);   // scrollbar track area
        painter.drawPixmap(w - 7, fy, bmp.pledit, 44, 42, 7, tileH);    // right part
    }
    
    // --- Body fill (black background for track list area) ---
    painter.fillRect(12, bodyTop, w - 12 - 20, bodyBottom - bodyTop, QColor(0, 0, 0));
    
    // --- Scrollbar track and thumb (right side) ---
    // Matching Windows draw_pe_vslide() at draw_pe.cpp line 213
    int scrollX = w - 15;
    int scrollTop = bodyTop;
    int scrollBottom = bodyBottom;
    int trackH = scrollBottom - scrollTop;
    int thumbH = 18;
    
    // Draw scrollbar track background (already tiled in border loop above)
    // Calculate thumb position based on QListWidget scroll position
    int thumbY = scrollTop;
    if (listWidget && listWidget->count() > 0) {
        QScrollBar *vsb = listWidget->verticalScrollBar();
        if (vsb && vsb->maximum() > 0) {
            // Calculate thumb position: map scrollbar value to track position
            int scrollRange = vsb->maximum() - vsb->minimum();
            int thumbRange = trackH - thumbH;
            if (scrollRange > 0 && thumbRange > 0) {
                thumbY = scrollTop + (vsb->value() * thumbRange) / scrollRange;
            }
        }
    }
    
    // Draw thumb sprite (8x18 at Pledit.bmp (52,53) normal, (61,53) pressed)
    painter.drawPixmap(scrollX, thumbY, bmp.pledit, 52, 53, 8, thumbH);
    
    // --- Bottom bar (38px tall) ---
    // Bottom left 125x38 from (0,72)
    painter.drawPixmap(0, bodyBottom, bmp.pledit, 0, 72, 125, 38);
    // Bottom right 150x38 from (126,72)
    painter.drawPixmap(w - 150, bodyBottom, bmp.pledit, 126, 72, 150, 38);
    // Fill middle with filler tile 25x38 from (179,0)
    for (int fx = 125; fx < w - 150; fx += 25)
        painter.drawPixmap(fx, bodyBottom, bmp.pledit, 179, 0, qMin(25, w - 150 - fx), 38);
    
    // --- Total time text at bottom ---
    drawText(painter, totalTimeStr.toUpper(), w - 143, h - 28);
    
    // --- Bottom buttons (ADD/REM/SEL/MISC/LIST) from Pledit.bmp ---
    // Matches Windows draw_pe.cpp button drawing - show normal state only
    // (Popup menu states are rendered dynamically when clicked, not in paintEvent)
    int btnY = h - 30;
    // ADD button at x=14 - show "file" normal state
    painter.drawPixmap(14, btnY, bmp.pledit, 0, 149, 22, 18);
    
    // REM button at x=43 - show "remove sel" normal state
    painter.drawPixmap(43, btnY, bmp.pledit, 54, 149, 22, 18);
    
    // SEL button at x=72 - show "all" normal state
    painter.drawPixmap(72, btnY, bmp.pledit, 104, 149, 22, 18);
    
    // MISC button at x=101 - show "info" normal state
    painter.drawPixmap(101, btnY, bmp.pledit, 154, 149, 22, 18);
    
    // LIST/FILE button at x=width-44 - show "load" normal state
    painter.drawPixmap(w - 44, btnY, bmp.pledit, 204, 149, 22, 18);
}

void PlaylistWindow::paintModernPlaylist(QPainter &p) {
    auto &ms = *g_modernSkin;
    int w = width();
    int h = height();
    
    // ---- Base texture fill ----
    QPixmap baseTex = ms.getBitmap("wasabi.frame.basetexture");
    if (!baseTex.isNull()) {
        for (int ty = 0; ty < h; ty += baseTex.height())
            for (int tx = 0; tx < w; tx += baseTex.width())
                p.drawPixmap(tx, ty, baseTex);
    } else {
        p.fillRect(0, 0, w, h, QColor(43, 45, 61));
    }
    
    // ---- Titlebar (18px) ----
    QPixmap tbLeft = ms.getBitmap("wasabi.frame.top.left");
    QPixmap tbCenter = ms.getBitmap("wasabi.frame.top");
    QPixmap tbRight = ms.getBitmap("wasabi.frame.top.right");
    
    if (!tbLeft.isNull()) p.drawPixmap(0, 0, tbLeft);
    if (!tbCenter.isNull()) {
        for (int tx = 10; tx < w - 10; tx += tbCenter.width()) {
            int tw = qMin(tbCenter.width(), w - 10 - tx);
            p.drawPixmap(tx, 0, tbCenter, 0, 0, tw, 18);
        }
    }
    if (!tbRight.isNull()) p.drawPixmap(w - 10, 0, tbRight);
    
    // Titlebar text background
    QPixmap tbTextLeft = ms.getBitmap(isActiveWindow() ?
        "wasabi.titlebar.left.active" : "wasabi.titlebar.left.inactive");
    QPixmap tbTextCenter = ms.getBitmap(isActiveWindow() ?
        "wasabi.titlebar.center.active" : "wasabi.titlebar.center.inactive");
    QPixmap tbTextRight = ms.getBitmap(isActiveWindow() ?
        "wasabi.titlebar.right.active" : "wasabi.titlebar.right.inactive");
    
    if (!tbTextLeft.isNull()) p.drawPixmap(10, 5, tbTextLeft);
    if (!tbTextCenter.isNull()) {
        for (int tx = 20; tx < w - 55; tx += tbTextCenter.width()) {
            int tw = qMin(tbTextCenter.width(), w - 55 - tx);
            p.drawPixmap(tx, 5, tbTextCenter, 0, 0, tw, tbTextCenter.height());
        }
    }
    if (!tbTextRight.isNull()) p.drawPixmap(w - 55, 5, tbTextRight);
    
    // Title text
    p.setPen(QColor(200, 200, 220));
    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.drawText(15, 14, "PLAYLIST EDITOR");
    
    // Close button
    QPixmap closeBg = ms.getBitmap("wasabi.button.bg.title");
    QPixmap closeBtn = ms.getBitmap("wasabi.button.exit");
    if (!closeBg.isNull()) p.drawPixmap(w - 18, 4, closeBg);
    if (!closeBtn.isNull()) p.drawPixmap(w - 17, 4, closeBtn);
    
    // ---- Playlist content area frame ----
    int fy = 18; // below titlebar
    int fh = h - 18; // rest of the window
    
    // Frame borders from player.pl.* bitmaps
    QPixmap plTL = ms.getBitmap("player.pl.topleft");       // 6x5
    QPixmap plTC = ms.getBitmap("player.pl.topcenter");     // 10x5
    QPixmap plTR = ms.getBitmap("player.pl.topright");      // 6x5
    QPixmap plL = ms.getBitmap("player.pl.left");           // 6x5
    QPixmap plR = ms.getBitmap("player.pl.right");          // 6x5
    QPixmap plBL = ms.getBitmap("player.pl.bottomleft");    // 20x67
    QPixmap plBR = ms.getBitmap("player.pl.bottomright");   // 20x67
    QPixmap plBC = ms.getBitmap("player.pl.bottomcenter");  // 10x25
    
    // Top border
    if (!plTL.isNull()) p.drawPixmap(0, fy, plTL);
    if (!plTC.isNull()) {
        for (int tx = 6; tx < w - 6; tx += plTC.width()) {
            int tw = qMin(plTC.width(), w - 6 - tx);
            p.drawPixmap(tx, fy, plTC, 0, 0, tw, plTC.height());
        }
    }
    if (!plTR.isNull()) p.drawPixmap(w - 6, fy, plTR);
    
    // Side borders
    int borderTop = fy + 5;
    int borderBottom = h - 67; // bottom corners are 67px tall
    for (int by = borderTop; by < borderBottom; by += 5) {
        int bh = qMin(5, borderBottom - by);
        if (!plL.isNull()) p.drawPixmap(0, by, plL, 0, 0, 6, bh);
        if (!plR.isNull()) p.drawPixmap(w - 6, by, plR, 0, 0, 6, bh);
    }
    
    // Body fill
    p.fillRect(6, borderTop, w - 12, borderBottom - borderTop, QColor(27, 28, 40));
    
    // Bottom corners and center
    if (!plBL.isNull()) p.drawPixmap(0, h - 67, plBL);
    if (!plBR.isNull()) p.drawPixmap(w - 20, h - 67, plBR);
    if (!plBC.isNull()) {
        for (int tx = 20; tx < w - 20; tx += plBC.width()) {
            int tw = qMin(plBC.width(), w - 20 - tx);
            p.drawPixmap(tx, h - 25, plBC, 0, 0, tw, plBC.height());
        }
    }
    // Fill area between bottom corners above bottom center strip
    p.fillRect(20, h - 67, w - 40, 42, QColor(27, 28, 40));
    
    // ---- Bottom buttons ----
    int btnY = h - 23;
    
    // Button backgrounds and images
    auto drawPlBtn = [&](const QString &bgId, const QString &btnId, int bx, int by) {
        QPixmap bg = ms.getBitmap(bgId);
        QPixmap btn = ms.getBitmap(btnId);
        if (!bg.isNull()) p.drawPixmap(bx, by, bg);
        if (!btn.isNull()) p.drawPixmap(bx + 3, by + 4, btn);
    };
    
    drawPlBtn("player.pl.button.add.bg", "player.pl.button.add", 5, btnY);
    drawPlBtn("player.pl.button.rem.bg", "player.pl.button.rem", 48, btnY);
    drawPlBtn("player.pl.button.sel.bg", "player.pl.button.sel", 93, btnY);
    drawPlBtn("player.pl.button.misc.bg", "player.pl.button.misc", 132, btnY);
    
    // List button (right-aligned)
    QPixmap listBg = ms.getBitmap("player.pl.button.list.bg");
    QPixmap listBtn = ms.getBitmap("player.pl.button.list");
    if (!listBg.isNull()) p.drawPixmap(w - 115, btnY, listBg);
    if (!listBtn.isNull()) p.drawPixmap(w - 112, btnY + 4, listBtn);
    
    // Time display
    QPixmap timeBg = ms.getBitmap("player.pl.time");
    if (!timeBg.isNull()) p.drawPixmap(w - 62, btnY - 2, timeBg);
    
    // Total time text
    if (!totalTimeStr.isEmpty()) {
        ms.drawBitmapText(p, "player.pe.time.font", totalTimeStr.toUpper(),
                         w - 55, btnY + 5, 55);
    }
    
    // Resizer
    QPixmap resizer = ms.getBitmap("player.pl.resizer");
    if (!resizer.isNull()) p.drawPixmap(w - 24, btnY + 4, resizer);
}

void PlaylistWindow::drawText(QPainter &painter, const QString &text, int x, int y) {
    int currentX = x;
    for (QChar ch : text) {
        QPoint pos = getTextCharPos(ch);
        if (pos.x() != -1) {
            painter.drawPixmap(currentX, y, WinampBitmaps::instance().text, pos.x(), pos.y(), 5, 6);
        }
        currentX += 5;
    }
}

QPoint PlaylistWindow::getTextCharPos(QChar ch) {
    return ::getTextCharPos(ch);
}

void PlaylistWindow::mousePressEvent(QMouseEvent *event) {
    int x = event->pos().x();
    int y = event->pos().y();
    int h = height();

    // Right-click on playlist items shows context menu
    if (event->button() == Qt::RightButton) {
        // Bottom buttons still respond to right click (matching drawn button positions)
        if (y >= h - 30 && y < h - 12) {
            if (x >= 14 && x < 36) { showAddMenu(event->globalPosition().toPoint()); return; }
            if (x >= 43 && x < 65) { showRemMenu(event->globalPosition().toPoint()); return; }
            if (x >= 72 && x < 94) { showSelMenu(event->globalPosition().toPoint()); return; }
            if (x >= 101 && x < 123) { showMiscMenu(event->globalPosition().toPoint()); return; }
            if (x >= width() - 44 && x < width() - 22) { showListMenu(event->globalPosition().toPoint()); return; }
        }
        // Right-click on list area shows the full context menu
        if (y >= 20 && y < h - 38) {
            showContextMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        }
        return;
    }

    // Left-click bottom buttons (matching drawn positions from draw_pe.cpp)
    if (y >= h - 30 && y < h - 12) {
        if (x >= 14 && x < 36) {
            showAddMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        } else if (x >= 43 && x < 65) {
            showRemMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        } else if (x >= 72 && x < 94) {
            showSelMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        } else if (x >= 101 && x < 123) {
            showMiscMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        } else if (x >= width() - 44 && x < width() - 22) {
            showListMenu(event->globalPosition().toPoint());
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        // Close button: (w-11, 3) 9x9
        if (x >= width() - 11 && x < width() - 2 && y >= 3 && y < 12) {
            hide();
            event->accept();
            return;
        }
        
        // Scrollbar drag: x=[width-15, width-7], y=[20, height-38]
        int scrollX = width() - 15;
        int bodyTop = 20;
        int bodyBottom = height() - 38;
        if (x >= scrollX && x < scrollX + 8 && y >= bodyTop && y < bodyBottom) {
            isDraggingScrollbar = true;
            // Calculate scroll position from mouse Y
            if (listWidget && listWidget->count() > 0) {
                QScrollBar *vsb = listWidget->verticalScrollBar();
                if (vsb && vsb->maximum() > 0) {
                    int trackH = bodyBottom - bodyTop;
                    int thumbH = 18;
                    int thumbRange = trackH - thumbH;
                    int clickY = y - bodyTop;
                    int scrollValue = (clickY * vsb->maximum()) / thumbRange;
                    vsb->setValue(qBound(0, scrollValue, vsb->maximum()));
                    update();
                }
            }
            event->accept();
            return;
        }
        
        // Check for resize edges
        if (!shadeMode) {
            ResizeEdge edge = hitTestResize(event->pos());
            if (edge != NoEdge) {
                isResizing = true;
                resizeEdge = edge;
                resizeStartPos = event->globalPosition().toPoint();
                resizeStartSize = size();
                event->accept();
                return;
            }
        }
        isDragging = true;
        dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void PlaylistWindow::dropEvent(QDropEvent *event) {
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QStringList audioExts = {"mp3", "wav", "flac", "ogg", "m4a", "aac", "wma", "opus",
                                 "mp4", "avi", "mkv", "mov", "webm"};
        for (const QUrl &url : mimeData->urls()) {
            QString path = url.toLocalFile();
            if (path.isEmpty()) continue;
            QFileInfo fi(path);
            if (fi.isDir()) {
                // Recursively add audio files from dropped directory
                QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    it.next();
                    if (audioExts.contains(it.fileInfo().suffix().toLower()))
                        addTrack(it.filePath());
                }
            } else if (fi.isFile()) {
                addTrack(path);
            }
        }
        event->acceptProposedAction();
    }
}

void PlaylistWindow::mouseMoveEvent(QMouseEvent *event) {
    // Handle resizing
    if (isResizing) {
        QPoint delta = event->globalPosition().toPoint() - resizeStartPos;
        QSize newSize = resizeStartSize;
        if (resizeEdge & RightEdge)
            newSize.setWidth(qMax(275, resizeStartSize.width() + delta.x()));
        if (resizeEdge & BottomEdge)
            newSize.setHeight(qMax(116, resizeStartSize.height() + delta.y()));
        resize(newSize);
        return;
    }
    
    // Handle scrollbar dragging
    if (isDraggingScrollbar) {
        int y = event->position().y();
        int bodyTop = 20;
        int bodyBottom = height() - 38;
        if (listWidget && listWidget->count() > 0) {
            QScrollBar *vsb = listWidget->verticalScrollBar();
            if (vsb && vsb->maximum() > 0) {
                int trackH = bodyBottom - bodyTop;
                int thumbH = 18;
                int thumbRange = trackH - thumbH;
                int dragY = y - bodyTop;
                int scrollValue = (dragY * vsb->maximum()) / thumbRange;
                vsb->setValue(qBound(0, scrollValue, vsb->maximum()));
                update();
            }
        }
        return;
    }
    
    // Update cursor for resize edges
    if (!isDragging && !shadeMode) {
        ResizeEdge edge = hitTestResize(event->pos());
        if (edge == BottomRight)
            setCursor(Qt::SizeFDiagCursor);
        else if (edge == RightEdge)
            setCursor(Qt::SizeHorCursor);
        else if (edge == BottomEdge)
            setCursor(Qt::SizeVerCursor);
        else
            setCursor(Qt::ArrowCursor);
    }
    
    if (isDragging) {
        move(event->globalPosition().toPoint() - dragPosition);
        checkSnap();
    }
}

void PlaylistWindow::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    isDragging = false;
    isResizing = false;
    isDraggingScrollbar = false;
    resizeEdge = NoEdge;
}

void PlaylistWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    // Double-click on titlebar toggles shade mode
    if (event->pos().y() < 20) {
        toggleShadeMode();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void PlaylistWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void PlaylistWindow::saveSettings(QSettings &s) {
    s.beginGroup("Playlist");
    s.setValue("x", x());
    s.setValue("y", y());
    s.setValue("visible", isVisible());
    s.setValue("snapMode", snapMode);
    s.setValue("width", width());
    s.setValue("height", height());
    // Save track list as a proper string list
    s.setValue("trackList", QVariant(tracks));
    s.endGroup();
}

void PlaylistWindow::loadSettings(QSettings &s) {
    s.beginGroup("Playlist");
    if (s.contains("x")) {
        move(s.value("x").toInt(), s.value("y").toInt());
    }
    snapMode = s.value("snapMode", 0).toInt();
    isSnappedToMain = (snapMode != 0);

    // Restore tracks — try new format first, then legacy comma-separated
    QStringList savedTracks;
    if (s.contains("trackList")) {
        savedTracks = s.value("trackList").toStringList();
    } else if (s.contains("tracks")) {
        // Legacy format: comma-separated
        QString raw = s.value("tracks").toString();
        savedTracks = raw.split(", ", Qt::SkipEmptyParts);
    }
    s.endGroup();

    // Add each saved track back into the playlist
    for (const QString &track : savedTracks) {
        QString trimmed = track.trimmed();
        if (!trimmed.isEmpty() && QFile::exists(trimmed)) {
            listWidget->addItem(trackDisplayName(tracks.size(), trimmed));
            tracks.append(trimmed);
            trackDurations.append(0); // Duration will be 0 until played
        }
    }
    updateTotalTimeDisplay();
}

// Right-click context menu on playlist items (matches Windows Winamp)
void PlaylistWindow::showContextMenu(QPoint globalPos) {
    static const char *menuStyle =
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
        "QMenu::item:disabled { color: #666; }"
        "QMenu::separator { height: 1px; background: #555; margin: 2px 4px; }";

    QMenu menu;
    menu.setStyleSheet(menuStyle);

    bool hasSelection = !listWidget->selectedItems().isEmpty();

    QAction *playAct = menu.addAction("Play item(s)");
    playAct->setShortcut(QKeySequence(Qt::Key_Return));
    playAct->setEnabled(hasSelection);
    menu.addSeparator();

    QAction *removeAct = menu.addAction("Remove item(s)");
    removeAct->setShortcut(QKeySequence(Qt::Key_Delete));
    removeAct->setEnabled(hasSelection);
    QAction *cropAct = menu.addAction("Crop files");
    cropAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Delete));
    cropAct->setEnabled(hasSelection);
    menu.addSeparator();

    QAction *fileInfoAct = menu.addAction("View file info...");
    fileInfoAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_3));
    fileInfoAct->setEnabled(hasSelection);
    QAction *editEntryAct = menu.addAction("Playlist entry...");
    editEntryAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    editEntryAct->setEnabled(hasSelection);
    menu.addSeparator();

    // Sort submenu
    QMenu *sortMenu = menu.addMenu("Sort");
    sortMenu->setStyleSheet(menuStyle);
    QAction *sortTitleAct = sortMenu->addAction("Sort list by title");
    sortTitleAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_1));
    QAction *sortFilenameAct = sortMenu->addAction("Sort list by filename");
    sortFilenameAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_2));
    QAction *sortPathAct = sortMenu->addAction("Sort list by path + filename");
    sortPathAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_3));
    sortMenu->addSeparator();
    QAction *reverseAct = sortMenu->addAction("Reverse list");
    reverseAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    QAction *randomizeAct = sortMenu->addAction("Randomize list");
    randomizeAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));

    menu.addSeparator();

    QAction *exploreAct = menu.addAction("Explore item folder");
    exploreAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    exploreAct->setEnabled(hasSelection);

    QAction *moveUpAct = menu.addAction("Move up");
    moveUpAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
    moveUpAct->setEnabled(hasSelection);
    QAction *moveDownAct = menu.addAction("Move down");
    moveDownAct->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Down));
    moveDownAct->setEnabled(hasSelection);

    menu.addSeparator();

    QAction *selAllAct = menu.addAction("Select all");
    selAllAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));
    QAction *selNoneAct = menu.addAction("Select none");
    QAction *selInvAct = menu.addAction("Invert selection");
    selInvAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));

    QAction *sel = menu.exec(globalPos);
    if (!sel) return;

    if (sel == playAct) {
        int row = listWidget->currentRow();
        if (row >= 0 && row < tracks.size())
            emit trackDoubleClicked(tracks[row]);
    }
    else if (sel == removeAct) removeSelected();
    else if (sel == cropAct) cropSelected();
    else if (sel == fileInfoAct) {
        // Show basic file info dialog
        int row = listWidget->currentRow();
        if (row >= 0 && row < tracks.size()) {
            QFileInfo fi(tracks[row]);
            QString info = QString("File: %1\nPath: %2\nSize: %3 KB\nModified: %4")
                .arg(fi.fileName())
                .arg(fi.absolutePath())
                .arg(fi.size() / 1024)
                .arg(fi.lastModified().toString("yyyy-MM-dd hh:mm:ss"));
            QMessageBox::information(this, "File Info", info);
        }
    }
    else if (sel == editEntryAct) {
        int row = listWidget->currentRow();
        if (row >= 0 && row < tracks.size()) {
            bool ok;
            QString newPath = QInputDialog::getText(this, "Edit Playlist Entry",
                "File path:", QLineEdit::Normal, tracks[row], &ok);
            if (ok && !newPath.isEmpty()) {
                tracks[row] = newPath;
                rebuildListDisplay();
            }
        }
    }
    else if (sel == sortTitleAct) sortByTitle();
    else if (sel == sortFilenameAct) sortByFilename();
    else if (sel == sortPathAct) sortByPath();
    else if (sel == reverseAct) reverseList();
    else if (sel == randomizeAct) randomizeList();
    else if (sel == exploreAct) exploreFolderOfSelected();
    else if (sel == moveUpAct) moveSelectedUp();
    else if (sel == moveDownAct) moveSelectedDown();
    else if (sel == selAllAct) listWidget->selectAll();
    else if (sel == selNoneAct) listWidget->clearSelection();
    else if (sel == selInvAct) {
        for (int i = 0; i < listWidget->count(); i++)
            listWidget->item(i)->setSelected(!listWidget->item(i)->isSelected());
    }
}

void PlaylistWindow::showAddMenu(QPoint globalPos) {
    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
    );
    
    QAction *addFiles = menu.addAction("Add file(s)\tL");
    QAction *addDir = menu.addAction("Add folder\tShift+L");
    QAction *addUrl = menu.addAction("Add URL\tCtrl+L");
    
    QAction *selected = menu.exec(globalPos);
    if (selected == addFiles) {
        QStringList files = QFileDialog::getOpenFileNames(this, "Add Files", QString(), 
            "Audio Files (*.mp3 *.wav *.flac *.ogg *.m4a *.aac);;All Files (*)");
        for (const QString &file : files) {
            if (!file.isEmpty()) {
                addTrack(file);
            }
        }
    } else if (selected == addDir) {
        QString dir = QFileDialog::getExistingDirectory(this, "Add Directory");
        if (!dir.isEmpty()) {
            QDir directory(dir);
            QStringList filters = {"*.mp3", "*.wav", "*.flac", "*.ogg", "*.m4a", "*.aac"};
            QStringList files = directory.entryList(filters, QDir::Files, QDir::Name);
            for (const QString &file : files) {
                addTrack(directory.absoluteFilePath(file));
            }
        }
    } else if (selected == addUrl) {
        bool ok;
        QString url = QInputDialog::getText(this, "Add URL",
            "Enter URL:", QLineEdit::Normal, "http://", &ok);
        if (ok && !url.isEmpty()) {
            // Add URL as a track (QMediaPlayer can handle remote URLs)
            listWidget->addItem(trackDisplayName(tracks.size(), url));
            tracks.append(url);
            trackDurations.append(0);
            updateTotalTimeDisplay();
        }
    }
}

void PlaylistWindow::showRemMenu(QPoint globalPos) {
    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
        "QMenu::separator { height: 1px; background: #555; margin: 2px 4px; }"
    );
    
    QAction *removeSel = menu.addAction("Remove selected\tDel");
    QAction *crop = menu.addAction("Crop selected\tCtrl+Del");
    QAction *clear = menu.addAction("Clear playlist\tCtrl+Shift+Del");
    menu.addSeparator();
    QAction *removeDead = menu.addAction("Remove missing files\tAlt+Del");
    QAction *removeDupes = menu.addAction("Remove duplicates");
    
    QAction *selected = menu.exec(globalPos);
    if (selected == removeSel) removeSelected();
    else if (selected == crop) cropSelected();
    else if (selected == clear) clearPlaylist();
    else if (selected == removeDead) removeDeadFiles();
    else if (selected == removeDupes) {
        // Remove duplicate entries (keep first occurrence)
        QSet<QString> seen;
        for (int i = tracks.size() - 1; i >= 0; i--) {
            if (seen.contains(tracks[i])) {
                tracks.removeAt(i);
                if (i < trackDurations.size()) trackDurations.removeAt(i);
            } else {
                seen.insert(tracks[i]);
            }
        }
        rebuildListDisplay();
        updateTotalTimeDisplay();
    }
}

void PlaylistWindow::showSelMenu(QPoint globalPos) {
    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
    );
    
    QAction *selectAll = menu.addAction("Select all\tCtrl+A");
    QAction *selectNone = menu.addAction("Select none");
    QAction *invertSel = menu.addAction("Invert selection\tCtrl+I");
    
    QAction *selected = menu.exec(globalPos);
    if (selected == selectAll) {
        listWidget->selectAll();
    } else if (selected == selectNone) {
        listWidget->clearSelection();
    } else if (selected == invertSel) {
        for (int i = 0; i < listWidget->count(); i++)
            listWidget->item(i)->setSelected(!listWidget->item(i)->isSelected());
    }
}

void PlaylistWindow::showMiscMenu(QPoint globalPos) {
    static const char *menuStyle =
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
        "QMenu::separator { height: 1px; background: #555; margin: 2px 4px; }";

    QMenu menu;
    menu.setStyleSheet(menuStyle);

    // File Info submenu (matches Windows MISC → File info)
    QMenu *fileInfoMenu = menu.addMenu("File info");
    fileInfoMenu->setStyleSheet(menuStyle);
    QAction *fileInfoAct = fileInfoMenu->addAction("View file info...\tAlt+3");
    QAction *editEntryAct = fileInfoMenu->addAction("Playlist entry...\tCtrl+E");

    // Sort submenu (matches Windows MISC → Sort)
    QMenu *sortMenu = menu.addMenu("Sort");
    sortMenu->setStyleSheet(menuStyle);
    QAction *sortTitleAct = sortMenu->addAction("Sort list by title\tCtrl+Shift+1");
    QAction *sortFilenameAct = sortMenu->addAction("Sort list by filename\tCtrl+Shift+2");
    QAction *sortPathAct = sortMenu->addAction("Sort list by path + filename\tCtrl+Shift+3");
    sortMenu->addSeparator();
    QAction *reverseAct = sortMenu->addAction("Reverse list\tCtrl+R");
    QAction *randomizeAct = sortMenu->addAction("Randomize list\tCtrl+Shift+R");

    // Misc submenu (matches Windows MISC → Misc)
    QMenu *miscMenu = menu.addMenu("Misc");
    miscMenu->setStyleSheet(menuStyle);
    QAction *htmlAct = miscMenu->addAction("Generate HTML playlist...\tCtrl+Alt+G");
    miscMenu->addSeparator();
    QAction *moveUpAct = miscMenu->addAction("Move selected up\tAlt+Up");
    QAction *moveDownAct = miscMenu->addAction("Move selected down\tAlt+Down");
    miscMenu->addSeparator();
    QAction *exploreAct = miscMenu->addAction("Explore item folder\tCtrl+F");

    QAction *selected = menu.exec(globalPos);
    if (!selected) return;

    if (selected == fileInfoAct) {
        int row = listWidget->currentRow();
        if (row >= 0 && row < tracks.size()) {
            QFileInfo fi(tracks[row]);
            QString info = QString("File: %1\nPath: %2\nSize: %3 KB\nModified: %4")
                .arg(fi.fileName()).arg(fi.absolutePath())
                .arg(fi.size() / 1024).arg(fi.lastModified().toString("yyyy-MM-dd hh:mm:ss"));
            QMessageBox::information(this, "File Info", info);
        }
    }
    else if (selected == editEntryAct) {
        int row = listWidget->currentRow();
        if (row >= 0 && row < tracks.size()) {
            bool ok;
            QString newPath = QInputDialog::getText(this, "Edit Playlist Entry",
                "File path:", QLineEdit::Normal, tracks[row], &ok);
            if (ok && !newPath.isEmpty()) {
                tracks[row] = newPath;
                rebuildListDisplay();
            }
        }
    }
    else if (selected == sortTitleAct) sortByTitle();
    else if (selected == sortFilenameAct) sortByFilename();
    else if (selected == sortPathAct) sortByPath();
    else if (selected == reverseAct) this->reverseList();
    else if (selected == randomizeAct) this->randomizeList();
    else if (selected == htmlAct) generateHtmlPlaylist();
    else if (selected == moveUpAct) moveSelectedUp();
    else if (selected == moveDownAct) moveSelectedDown();
    else if (selected == exploreAct) exploreFolderOfSelected();
}

void PlaylistWindow::showListMenu(QPoint globalPos) {
    QMenu menu;
    menu.setStyleSheet(
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
    );

    QAction *newPl = menu.addAction("New playlist\tCtrl+N");
    QAction *openPl = menu.addAction("Open playlist...\tCtrl+O");
    QAction *savePl = menu.addAction("Save playlist...\tCtrl+S");
    menu.addSeparator();
    QAction *genPl = menu.addAction("Generate playlist...");

    QAction *selected = menu.exec(globalPos);
    if (selected == newPl) {
        clearPlaylist();
    } else if (selected == openPl) {
        QString fileName = QFileDialog::getOpenFileName(this, "Open Playlist", "",
            "Playlist Files (*.m3u *.m3u8 *.pls);;All Files (*)");
        if (!fileName.isEmpty()) {
            clearPlaylist();
            QFile file(fileName);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                QString basePath = QFileInfo(fileName).absolutePath();
                while (!in.atEnd()) {
                    QString line = in.readLine().trimmed();
                    if (line.isEmpty() || line.startsWith('#'))
                        continue;
                    // Handle relative paths
                    if (!QFileInfo(line).isAbsolute())
                        line = basePath + "/" + line;
                    if (QFile::exists(line))
                        addTrack(line);
                }
                file.close();
            }
        }
    } else if (selected == savePl) {
        QString fileName = QFileDialog::getSaveFileName(this, "Save Playlist", "",
            "M3U Playlist (*.m3u);;M3U8 Playlist (*.m3u8);;All Files (*)");
        if (!fileName.isEmpty()) {
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << "#EXTM3U\n";
                for (int i = 0; i < tracks.size(); i++) {
                    qint64 durSec = (i < trackDurations.size()) ? trackDurations[i] / 1000 : -1;
                    QString title = QFileInfo(tracks[i]).baseName();
                    out << "#EXTINF:" << durSec << "," << title << "\n";
                    out << tracks[i] << "\n";
                }
                file.close();
            }
        }
    } else if (selected == genPl) {
        // Show playlist generator dialog
        QDialog dialog(this);
        dialog.setWindowTitle(TR("win.plgen.title", "Playlist Generator"));
        dialog.setModal(true);
        dialog.resize(350, 200);
        dialog.setStyleSheet(
            "QDialog { background-color: #2b2d3d; color: #00ff00; }"
            "QLabel { color: #00ff00; }"
            "QPushButton { background-color: #1a1c2a; color: #00ff00; border: 1px solid #555; padding: 5px 15px; }"
            "QPushButton:hover { background-color: #0000c6; }"
            "QSpinBox { background-color: #1a1c2a; color: #00ff00; border: 1px solid #555; }"
            "QCheckBox { color: #00ff00; }"
        );
        
        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        
        // Number of tracks
        QHBoxLayout *countLayout = new QHBoxLayout();
        QLabel *countLabel = new QLabel(TR("plgen.numtracks", "Number of tracks:"));
        QSpinBox *countSpin = new QSpinBox();
        countSpin->setMinimum(1);
        countSpin->setMaximum(1000);
        countSpin->setValue(50);
        countLayout->addWidget(countLabel);
        countLayout->addWidget(countSpin);
        countLayout->addStretch();
        layout->addLayout(countLayout);
        
        // Replace or add option
        QCheckBox *replaceCheck = new QCheckBox(TR("plgen.replace", "Replace current playlist (otherwise add to current)"));
        replaceCheck->setChecked(false);
        layout->addWidget(replaceCheck);
        
        layout->addStretch();
        
        // Buttons
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        QPushButton *okBtn = new QPushButton(TR("button.generate", "Generate"));
        QPushButton *cancelBtn = new QPushButton(TR("button.cancel", "Cancel"));
        buttonLayout->addWidget(okBtn);
        buttonLayout->addWidget(cancelBtn);
        layout->addLayout(buttonLayout);
        
        connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
        
        if (dialog.exec() == QDialog::Accepted) {
            int numTracks = countSpin->value();
            bool replace = replaceCheck->isChecked();
            
            if (replace) {
                clearPlaylist();
            }
            
            // Scan music directory for all audio files
            QStringList allFiles;
            QString musicPath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
            if (musicPath.isEmpty() || !QDir(musicPath).exists()) {
                musicPath = QDir::homePath();
            }
            
            QStringList queue;
            queue << musicPath;
            QStringList filters;
            filters << "*.mp3" << "*.flac" << "*.ogg" << "*.wav" << "*.m4a" 
                   << "*.aac" << "*.wma" << "*.opus";
            
            while (!queue.isEmpty() && allFiles.size() < numTracks * 10) {
                QString currentDir = queue.takeFirst();
                QDir dir(currentDir);
                
                // Add audio files
                QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
                for (const QFileInfo &file : files) {
                    allFiles << file.absoluteFilePath();
                }
                
                // Add subdirectories to queue
                QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QFileInfo &subdir : subdirs) {
                    queue << subdir.absoluteFilePath();
                }
            }
            
            if (allFiles.isEmpty()) {
                QMessageBox::information(this, "Playlist Generator", 
                    "No audio files found in " + musicPath);
                return;
            }
            
            // Randomly select tracks
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(allFiles.begin(), allFiles.end(), g);
            
            int tracksToAdd = qMin(numTracks, allFiles.size());
            for (int i = 0; i < tracksToAdd; i++) {
                addTrack(allFiles[i]);
            }
        }
    }
}

void PlaylistWindow::checkSnap() {
    if (!mainWindow) return;
    
    const int snapDist = 15;
    QPoint mainPos = mainWindow->pos();
    QSize mainSize = mainWindow->size();
    QPoint myPos = pos();
    
    // Snap to right of main window
    if (qAbs(myPos.x() - (mainPos.x() + mainSize.width())) < snapDist &&
        qAbs(myPos.y() - mainPos.y()) < snapDist) {
        move(mainPos.x() + mainSize.width(), mainPos.y());
        snapMode = 1;  // right of main
        return;
    }
    
    // Snap below EQ (if EQ is visible and snapped below main)
    // EQ is at main.y + main.height, so playlist goes at main.y + main.height + eq.height
    int eqH = g_isModernSkin ? 113 : 116;
    int eqBottom = mainPos.y() + mainSize.height() + eqH;
    if (qAbs(myPos.x() - mainPos.x()) < snapDist &&
        qAbs(myPos.y() - eqBottom) < snapDist) {
        move(mainPos.x(), eqBottom);
        snapMode = 2;  // below EQ
        return;
    }
    
    // Snap below main window directly
    if (qAbs(myPos.x() - mainPos.x()) < snapDist &&
        qAbs(myPos.y() - (mainPos.y() + mainSize.height())) < snapDist) {
        move(mainPos.x(), mainPos.y() + mainSize.height());
        snapMode = 3;  // below main
        return;
    }
    
    snapMode = 0;
}

void PlaylistWindow::followMain() {
    if (!mainWindow || !isVisible()) return;
    QPoint mainPos = mainWindow->pos();
    
    switch (snapMode) {
        case 1:  // right of main
            move(mainPos.x() + mainWindow->width(), mainPos.y());
            break;
        case 2:  // below EQ
        {
            int eqH = g_isModernSkin ? 113 : 116;
            move(mainPos.x(), mainPos.y() + mainWindow->height() + eqH);
            break;
        }
        case 3:  // below main
            move(mainPos.x(), mainPos.y() + mainWindow->height());
            break;
    }
}

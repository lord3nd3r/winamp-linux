#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QFileDialog>
#include <QMouseEvent>
#include <QTimer>
#include <QRandomGenerator>
#include <QListWidget>
#include <QVBoxLayout>
#include <QPixmap>
#include <QImage>

// Forward declaration
class WinampWindow;

// Bitmap Resource Manager
class WinampBitmaps {
public:
    static WinampBitmaps& instance() {
        static WinampBitmaps inst;
        return inst;
    }
    
    bool loadAll(const QString &resourcePath) {
        basePath = resourcePath;
        
        // Load all classic Winamp bitmaps
        main = QPixmap(basePath + "/MAIN.BMP");
        cbuttons = QPixmap(basePath + "/CBUTTONS.BMP");
        titlebar = QPixmap(basePath + "/titlebar.bmp");
        numbers = QPixmap(basePath + "/numbers.bmp");
        text = QPixmap(basePath + "/text.bmp");
        playpaus = QPixmap(basePath + "/PLAYPAUS.BMP");
        monoster = QPixmap(basePath + "/MONOSTER.BMP");
        posbar = QPixmap(basePath + "/POSBAR.BMP");
        volume = QPixmap(basePath + "/volume.bmp");
        shufrep = QPixmap(basePath + "/SHUFREP.BMP");
        eqmain = QPixmap(basePath + "/Eqmain.bmp");
        pledit = QPixmap(basePath + "/Pledit.bmp");
        
        return !main.isNull() && !cbuttons.isNull();
    }
    
    QPixmap main, cbuttons, titlebar, numbers, text;
    QPixmap playpaus, monoster, posbar, volume, shufrep;
    QPixmap eqmain, pledit;
    QString basePath;
    
private:
    WinampBitmaps() {}
};

// Playlist Window
class PlaylistWindow : public QWidget {
public:
    PlaylistWindow(WinampWindow *parent = nullptr);
    
    void setMainWindow(WinampWindow *main) { mainWindow = main; }
    
    void addTrack(const QString &filename) {
        QFileInfo fi(filename);
        listWidget->addItem(fi.fileName());
        tracks.append(filename);
    }
    
    QString getTrack(int index) {
        if (index >= 0 && index < tracks.size()) {
            return tracks[index];
        }
        return QString();
    }
    
    int count() const { return tracks.size(); }
    
    void followMain();
    void checkSnap();
    
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        
        auto &bmp = WinampBitmaps::instance();
        int w = width();
        int h = height();
        
        if (bmp.pledit.isNull()) {
            p.fillRect(rect(), QColor(0, 0, 0));
            p.setPen(QColor(0, 255, 0));
            p.setFont(QFont("Tahoma", 7, QFont::Bold));
            p.drawText(6, 10, "Winamp Playlist Editor");
            return;
        }
        
        // === Titlebar (20px tall) ===
        // Active: y=0, Inactive: y=21 in Pledit.bmp
        int tbY = isActiveWindow() ? 0 : 21;
        
        // Left corner: (0,tbY) 25x20
        p.drawPixmap(0, 0, bmp.pledit, 0, tbY, 25, 20);
        
        // Right corner: (153,tbY) 25x20
        p.drawPixmap(w - 25, 0, bmp.pledit, 153, tbY, 25, 20);
        
        // Center title "PLAYLIST EDITOR": (26,tbY) 100x20
        int centerX = (w - 100) / 2;
        p.drawPixmap(centerX, 0, bmp.pledit, 26, tbY, 100, 20);
        
        // Fill between left corner and center with filler tile: (127,tbY) 25x20
        for (int x = 25; x < centerX; x += 25) {
            int tw = qMin(25, centerX - x);
            p.drawPixmap(x, 0, bmp.pledit, 127, tbY, tw, 20);
        }
        
        // Fill between center and right corner
        for (int x = centerX + 100; x < w - 25; x += 25) {
            int tw = qMin(25, w - 25 - x);
            p.drawPixmap(x, 0, bmp.pledit, 127, tbY, tw, 20);
        }
        
        // Close button: dest(w-11, 3), src(167,3) 9x9
        p.drawPixmap(w - 11, 3, bmp.pledit, 167, 3, 9, 9);
        
        // Shade button: dest(w-20, 3), src(158,3) 9x9
        p.drawPixmap(w - 20, 3, bmp.pledit, 158, 3, 9, 9);
        
        // === Side Borders (tiled 29px chunks) ===
        for (int y = 20; y < h - 38; y += 29) {
            int th = qMin(29, h - 38 - y);
            // Left border: (0,42) 12x29
            p.drawPixmap(0, y, bmp.pledit, 0, 42, 12, th);
            // Right border: (31,42) 5x29 + (44,42) 7x29 = 12px total at right edge
            p.drawPixmap(w - 12, y, bmp.pledit, 31, 42, 5, th);
            p.drawPixmap(w - 7, y, bmp.pledit, 44, 42, 7, th);
        }
        
        // Scrollbar track: (36,42) 8x29, at x=(w-15)
        for (int y = 20; y < h - 38; y += 29) {
            int th = qMin(29, h - 38 - y);
            p.drawPixmap(w - 20, y, bmp.pledit, 36, 42, 8, th);
        }
        
        // === Bottom Bar (38px tall) ===
        // Bottom-left: (0,72) 125x38
        p.drawPixmap(0, h - 38, bmp.pledit, 0, 72, 125, 38);
        
        // Bottom-right: (126,72) 150x38
        p.drawPixmap(w - 150, h - 38, bmp.pledit, 126, 72, 150, 38);
        
        // Fill gap between bottom-left and bottom-right: (179,0) 25x38
        for (int x = 125; x < w - 150; x += 25) {
            int tw = qMin(25, w - 150 - x);
            p.drawPixmap(x, h - 38, bmp.pledit, 179, 0, tw, 38);
        }
    }
    
    void mousePressEvent(QMouseEvent *event) override {
        int x = event->pos().x();
        int y = event->pos().y();
        
        if (y < 20) {
            // Close button: (w-11, 3) 9x9
            if (x >= width() - 11 && x < width() - 2 && y >= 3 && y < 12) {
                hide();
                return;
            }
            isDragging = true;
            dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        }
    }
    
    void mouseMoveEvent(QMouseEvent *event) override {
        if (isDragging) {
            QPoint newPos = event->globalPosition().toPoint() - dragPosition;
            move(newPos);
            checkSnap();
        }
    }
    
    void mouseReleaseEvent(QMouseEvent *event) override {
        isDragging = false;
    }
    
    bool isSnapped() const { return isSnappedToMain; }
    
private:
    QListWidget *listWidget;
    QStringList tracks;
    QPoint dragPosition;
    bool isDragging = false;
    WinampWindow *mainWindow = nullptr;
    bool isSnappedToMain = false;
};

// Equalizer Window
class EqualizerWindow : public QWidget {
public:
    EqualizerWindow(WinampWindow *parent = nullptr);
    
    void setMainWindow(WinampWindow *main) { mainWindow = main; }
    
    void followMain();
    void checkSnap();
    
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        
        auto &bmp = WinampBitmaps::instance();
        if (bmp.eqmain.isNull()) {
            p.fillRect(rect(), QColor(66, 66, 99));
            p.setPen(QColor(0, 255, 0));
            p.setFont(QFont("Tahoma", 7, QFont::Bold));
            p.drawText(6, 10, "Winamp Equalizer");
            return;
        }
        
        // Fill with dark background first
        p.fillRect(rect(), QColor(35, 36, 34));
        
        // Titlebar: active at (0,134), inactive at (0,149), 275x14
        int tbY = isActiveWindow() ? 134 : 149;
        p.drawPixmap(0, 0, bmp.eqmain, 0, tbY, 275, 14);
        
        // ON button: dest(14,18), 25x12
        // States: OFF=(10,119), ON=(69,119), OFF pressed=(128,119), ON pressed=(187,119)
        int onSrcX = eqEnabled ? 69 : 10;
        p.drawPixmap(14, 18, bmp.eqmain, onSrcX, 119, 25, 12);
        
        // AUTO button: dest(39,18), 33x12
        int autoSrcX = autoEnabled ? 94 : 35;
        p.drawPixmap(39, 18, bmp.eqmain, autoSrcX, 119, 33, 12);
        
        // Presets button: dest(217,18), 44x12
        p.drawPixmap(217, 18, bmp.eqmain, 224, 164, 44, 12);
        
        // EQ graph background: dest(86,17), src(0,294), 113x19
        p.drawPixmap(86, 17, bmp.eqmain, 0, 294, 113, 19);
        
        // Draw slider grooves and thumbs
        // Preamp at x=21, bands at x=78+n*18
        drawEqSlider(p, 0, 21);  // Preamp
        for (int i = 0; i < 10; i++) {
            drawEqSlider(p, i + 1, 78 + i * 18);
        }
    }
    
    void drawEqSlider(QPainter &p, int which, int destX) {
        auto &bmp = WinampBitmaps::instance();
        int pos = (which == 0) ? preampValue : eqValues[which - 1];
        
        // Groove background: 28 images (14 per row)
        // n = (pos * 28) / 64, clamped to 0-27
        int n = (pos * 27) / 63;
        if (n > 27) n = 27;
        if (n < 0) n = 0;
        
        int grooveSrcX, grooveSrcY;
        if (n < 14) {
            grooveSrcX = 13 + n * 15;
            grooveSrcY = 164;
        } else {
            grooveSrcX = 13 + (n - 14) * 15;
            grooveSrcY = 229;
        }
        p.drawPixmap(destX, 38, bmp.eqmain, grooveSrcX, grooveSrcY, 14, 63);
        
        // Slider thumb (knob): 11x11 at src(0,164) unpressed
        int thumbY = 38 + 63 - 12 - ((63 - pos) * 52) / 64;
        p.drawPixmap(destX + 1, thumbY, bmp.eqmain, 0, 164, 11, 11);
    }
    
    void mousePressEvent(QMouseEvent *event) override {
        int x = event->pos().x();
        int y = event->pos().y();
        
        // Title bar
        if (y < 14) {
            if (x >= 264) { hide(); return; }
            isDragging = true;
            dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            return;
        }
        
        // ON button: (14,18)-(39,30)
        if (x >= 14 && x < 39 && y >= 18 && y < 30) {
            eqEnabled = !eqEnabled;
            update();
            return;
        }
        
        // AUTO button: (39,18)-(72,30)
        if (x >= 39 && x < 72 && y >= 18 && y < 30) {
            autoEnabled = !autoEnabled;
            update();
            return;
        }
        
        // Slider dragging
        // Preamp: x=21..34, bands: x=78+n*18..78+n*18+14
        if (y >= 38 && y <= 101) {
            if (x >= 21 && x <= 34) {
                draggingSlider = 0;
                updateSliderFromY(y);
                return;
            }
            for (int i = 0; i < 10; i++) {
                int sx = 78 + i * 18;
                if (x >= sx && x <= sx + 14) {
                    draggingSlider = i + 1;
                    updateSliderFromY(y);
                    return;
                }
            }
        }
    }
    
    void updateSliderFromY(int y) {
        int pos = 63 - ((y - 38) * 63) / 52;
        if (pos < 0) pos = 0;
        if (pos > 63) pos = 63;
        if (draggingSlider == 0) preampValue = pos;
        else eqValues[draggingSlider - 1] = pos;
        update();
    }
    
    void mouseMoveEvent(QMouseEvent *event) override {
        if (draggingSlider >= 0) {
            updateSliderFromY(event->pos().y());
            return;
        }
        if (isDragging) {
            QPoint newPos = event->globalPosition().toPoint() - dragPosition;
            move(newPos);
            checkSnap();
        }
    }
    
    void mouseReleaseEvent(QMouseEvent *event) override {
        isDragging = false;
        draggingSlider = -1;
    }
    
    bool isSnapped() const { return isSnappedToMain; }
    
private:
    int eqValues[10];
    int preampValue;
    bool eqEnabled = true;
    bool autoEnabled = false;
    int draggingSlider = -1;
    QPoint dragPosition;
    bool isDragging = false;
    WinampWindow *mainWindow = nullptr;
    bool isSnappedToMain = false;
};

// Playlist Window Constructor
PlaylistWindow::PlaylistWindow(WinampWindow *parent) : QWidget(nullptr), mainWindow(parent) {
    setFixedSize(275, 232);
    setWindowTitle("Winamp Playlist Editor");
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    
    // Position list widget manually within the skin frame
    // Titlebar=20px, side borders=12px left + 20px right, bottom=38px
    listWidget = new QListWidget(this);
    listWidget->setGeometry(12, 20, 275 - 12 - 20, 232 - 20 - 38);
    listWidget->setStyleSheet(
        "QListWidget {"
        "  background-color: #000000;"
        "  color: #00FF00;"
        "  border: none;"
        "  font-family: 'Courier New', 'Courier';"
        "  font-size: 8pt;"
        "  selection-background-color: #0000C6;"
        "  selection-color: #00FF00;"
        "}"
        "QListWidget::item {"
        "  padding: 0px;"
        "}"
    );
    listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

// Equalizer Window Constructor
EqualizerWindow::EqualizerWindow(WinampWindow *parent) : QWidget(nullptr), mainWindow(parent) {
    setFixedSize(275, 116);
    setWindowTitle("Winamp Equalizer");
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    
    // Initialize EQ bands to center position (32 out of 63)
    for (int i = 0; i < 10; i++) {
        eqValues[i] = 32;
    }
    preampValue = 32;
}

// Main Winamp Window
class WinampWindow : public QWidget {
public:
    WinampWindow(QWidget *parent = nullptr) : QWidget(parent), dragPosition(0,0), isDragging(false), 
                 volume(200), hoveredButton(-1), pressedButton(-1),
                 shuffleOn(false), repeatOn(false), eqBtnOn(true), plBtnOn(true),
                 isDraggingVolume(false), isDraggingPos(false), scrollOffset(0) {
        setFixedSize(275, 116);
        setWindowTitle("Winamp 5.666 for Linux");
        setWindowFlags(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, false);
        setMouseTracking(true);
        
        // Setup audio
        player = new QMediaPlayer(this);
        audioOutput = new QAudioOutput(this);
        player->setAudioOutput(audioOutput);
        audioOutput->setVolume(volume / 255.0f);
        
        // Update timer (50ms = 20fps like original)
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &WinampWindow::updateDisplay);
        timer->start(50);
        
        // Scroll timer for song title
        scrollTimer = new QTimer(this);
        connect(scrollTimer, &QTimer::timeout, this, [this]() {
            scrollOffset++;
            update();
        });
        scrollTimer->start(150);
        
        connect(player, &QMediaPlayer::positionChanged, this, [this](qint64) { update(); });
        
        // Create playlist and EQ windows
        playlistWindow = new PlaylistWindow(this);
        eqWindow = new EqualizerWindow(this);
        
        // Position windows
        playlistWindow->move(x() + width(), y());
        eqWindow->move(x(), y() + height());
    }
    
    ~WinampWindow() {
        delete playlistWindow;
        delete eqWindow;
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        
        auto &bmp = WinampBitmaps::instance();
        
        if (bmp.main.isNull()) {
            drawFallbackUI(p);
            return;
        }
        
        // === 1. Main background ===
        p.drawPixmap(0, 0, bmp.main, 0, 0, 275, 116);
        
        // === 2. Title bar from titlebar.bmp ===
        if (!bmp.titlebar.isNull()) {
            // Active/inactive title bar: src starts at x=27
            int srcY = isActiveWindow() ? 0 : 15;
            p.drawPixmap(0, 0, 275, 14, bmp.titlebar, 27, srcY, 275, 14);
            
            // Title bar buttons from titlebar.bmp
            // Menu icon: dest(6,3), src(0, state*9), 9x9
            p.drawPixmap(6, 3, bmp.titlebar, 0, 0, 9, 9);
            // Minimize: dest(244,3), src(9, state*9), 9x9
            p.drawPixmap(244, 3, bmp.titlebar, 9, 0, 9, 9);
            // Shade: dest(254,3), src(0, 18), 9x9
            p.drawPixmap(254, 3, bmp.titlebar, 0, 18, 9, 9);
            // Close: dest(264,3), src(18, state*9), 9x9
            p.drawPixmap(264, 3, bmp.titlebar, 18, 0, 9, 9);
        }
        
        // === 3. Time display from numbers.bmp ===
        if (!bmp.numbers.isNull()) {
            qint64 pos = player->position();
            int totalSecs = pos / 1000;
            int mins = totalSecs / 60;
            int secs = totalSecs % 60;
            
            // Digit positions from original: 36, 48, 60, 78, 90 at y=26
            int dw = 9, dh = 13;
            int dy = 26;
            
            // Minutes tens digit (or blank if < 10)
            int minTens = (mins / 10) % 10;
            int srcX = (mins >= 10) ? (minTens * dw) : 90; // 90 = blank
            p.drawPixmap(36, dy, bmp.numbers, srcX, 0, dw, dh);
            
            // Minutes ones
            p.drawPixmap(48, dy, bmp.numbers, (mins % 10) * dw, 0, dw, dh);
            
            // Seconds tens
            p.drawPixmap(60, dy, bmp.numbers, ((secs / 10) % 10) * dw, 0, dw, dh);
            
            // Seconds ones
            p.drawPixmap(78, dy, bmp.numbers, (secs % 10) * dw, 0, dw, dh);
        }
        
        // === 4. Play/pause/stop indicator from PLAYPAUS.BMP ===
        // Layout: play=0, pause=9, stop=18, blank=27 (each 9x9 at srcY=0)
        if (!bmp.playpaus.isNull()) {
            int srcX;
            switch (player->playbackState()) {
                case QMediaPlayer::PlayingState: srcX = 0; break;   // Play
                case QMediaPlayer::PausedState:  srcX = 9; break;   // Pause
                default:                         srcX = 18; break;  // Stop
            }
            // Work indicator (3px) at dest(24,28), main icon at dest(26,28)
            p.drawPixmap(26, 28, bmp.playpaus, srcX, 0, 9, 9);
        }
        
        // === 5. Stereo/Mono from MONOSTER.BMP ===
        // Layout: stereo active (0,0 29x12), stereo dim (0,12), mono active (29,0 28x12), mono dim (29,12)
        if (!bmp.monoster.isNull()) {
            bool playing = (player->playbackState() == QMediaPlayer::PlayingState);
            // Stereo indicator at dest(239,41) - always show, lit when playing
            p.drawPixmap(239, 41, bmp.monoster, 0, playing ? 0 : 12, 29, 12);
            // Mono indicator at dest(212,41) - always dimmed (we assume stereo)
            p.drawPixmap(212, 41, bmp.monoster, 29, 12, 28, 12);
        }
        
        // === 6. Transport buttons from CBUTTONS.BMP ===
        // Layout: 136x36, buttons at srcX: prev=0, play=23, pause=46, stop=69, next=92 (each 23x18)
        // Eject at srcX=114, size 22x16
        // Row 0 (y=0)=normal, Row 1 (y=18)=pressed
        if (!bmp.cbuttons.isNull()) {
            auto drawBtn = [&](int id, int srcX, int destX, int destY, int w, int h, int pressedY) {
                int sy = (pressedButton == id) ? pressedY : 0;
                p.drawPixmap(destX, destY, bmp.cbuttons, srcX, sy, w, h);
            };
            
            drawBtn(0,  0,  16, 88, 23, 18, 18);  // Previous
            drawBtn(1, 23,  39, 88, 23, 18, 18);  // Play
            drawBtn(2, 46,  62, 88, 23, 18, 18);  // Pause
            drawBtn(3, 69,  85, 88, 23, 18, 18);  // Stop
            drawBtn(4, 92, 108, 88, 22, 18, 18);  // Next (22px wide!)
            drawBtn(5, 114, 136, 89, 22, 16, 16); // Eject (22x16, pressed row at y=16)
        }
        
        // === 7. Position/seek bar from POSBAR.BMP ===
        // Layout: background (0,0 248x10), slider normal (248,0 29x10), slider pressed (278,0 29x10)
        if (!bmp.posbar.isNull()) {
            // Draw bar background at dest(16,72)
            p.drawPixmap(16, 72, bmp.posbar, 0, 0, 248, 10);
            
            if (player->duration() > 0) {
                // Slider position: range is 0 to (248-29)=219 pixels
                qint64 pos = player->position();
                qint64 dur = player->duration();
                int sliderX = (int)((double)pos / dur * 219);
                int sliderSrcX = isDraggingPos ? 278 : 248;
                p.drawPixmap(16 + sliderX, 72, bmp.posbar, sliderSrcX, 0, 29, 10);
            }
        }
        
        // === 8. Volume bar from volume.bmp ===
        // Layout: 68x433. 28 frames of 68x15 backgrounds (0-27), slider at y=422 (14x11)
        if (!bmp.volume.isNull()) {
            // Select background frame based on volume level (0-255 -> 0-27)
            int frame = (volume * 27) / 255;
            int srcY = frame * 15;
            p.drawPixmap(107, 57, bmp.volume, 0, srcY, 68, 13);
            
            // Draw slider knob
            int sliderX = (volume * 51) / 255;  // Range: 0-51 pixels
            int knobSrcX = isDraggingVolume ? 0 : 15;
            p.drawPixmap(107 + sliderX, 58, bmp.volume, knobSrcX, 422, 14, 11);
        }
        
        // === 9. Shuffle/Repeat from SHUFREP.BMP ===
        if (!bmp.shufrep.isNull()) {
            // Shuffle: dest(164,89), 47x15, src x=28, y = (on?30:0) + (pressed?15:0)
            int shufY = (shuffleOn ? 30 : 0);
            p.drawPixmap(164, 89, bmp.shufrep, 28, shufY, 47, 15);
            
            // Repeat: dest(210,89), 28x15, src x=0, y = (on?30:0) + (pressed?15:0)
            int repY = (repeatOn ? 30 : 0);
            p.drawPixmap(210, 89, bmp.shufrep, 0, repY, 28, 15);
            
            // EQ button: dest(219,58), 23x12, src x=(pressed?46:0), y=(on?73:61)
            p.drawPixmap(219, 58, bmp.shufrep, eqBtnOn ? 0 : 46, eqBtnOn ? 73 : 61, 23, 12);
            
            // PL button: dest(242,58), 23x12, src x=(pressed?69:23), y=(on?73:61)
            p.drawPixmap(242, 58, bmp.shufrep, plBtnOn ? 23 : 69, plBtnOn ? 73 : 61, 23, 12);
        }
        
        // === 10. Song title text from text.bmp ===
        if (!bmp.text.isNull() && !currentFile.isEmpty()) {
            drawSongTitle(p);
        }
        
        // === 11. Bitrate/sample rate display ===
        if (!bmp.text.isNull() && player->playbackState() != QMediaPlayer::StoppedState) {
            drawBitrateInfo(p);
        }
        
        // === 12. Simple visualization ===
        drawVisualization(p);
    }
    
    void drawSongTitle(QPainter &p) {
        auto &bmp = WinampBitmaps::instance();
        QFileInfo fi(currentFile);
        QString title = fi.completeBaseName().toUpper();
        
        // Clip to song title area (111,27) to (265,33), 154px wide
        p.setClipRect(111, 27, 154, 6);
        
        int charW = 5, charH = 6;
        int totalWidth = title.length() * charW;
        int offset = -(scrollOffset % (totalWidth + 100));
        
        for (int i = 0; i < title.length(); i++) {
            int dx = 111 + offset + i * charW;
            if (dx > 265) break;
            if (dx + charW < 111) continue;
            
            QChar ch = title[i];
            int srcX, srcY;
            getTextCharPos(ch, srcX, srcY);
            p.drawPixmap(dx, 27, bmp.text, srcX, srcY, charW, charH);
        }
        
        p.setClipping(false);
    }
    
    void getTextCharPos(QChar ch, int &srcX, int &srcY) {
        char c = ch.toLatin1();
        if (c >= 'A' && c <= 'Z') {
            srcX = (c - 'A') * 5;
            srcY = 0;
        } else if (c >= '0' && c <= '9') {
            srcX = (c - '0') * 5;
            srcY = 6;
        } else if (c == '-') { srcX = 60; srcY = 6; }
        else if (c == '.') { srcX = 50; srcY = 6; }
        else if (c == ':') { srcX = 55; srcY = 6; }
        else if (c == '(') { srcX = 65; srcY = 6; }
        else if (c == ')') { srcX = 70; srcY = 6; }
        else if (c == '\'') { srcX = 80; srcY = 6; }
        else if (c == '!') { srcX = 85; srcY = 6; }
        else if (c == '_') { srcX = 90; srcY = 6; }
        else if (c == '+') { srcX = 95; srcY = 6; }
        else { srcX = 100; srcY = 12; } // blank/space
    }
    
    void drawBitrateInfo(QPainter &p) {
        auto &bmp = WinampBitmaps::instance();
        // Bitrate at (111,43), sample rate at (156,43) using text.bmp
        QString br = "128"; // Default
        QString sr = "44";
        
        int charW = 5, charH = 6;
        
        // Draw bitrate
        for (int i = 0; i < br.length() && i < 3; i++) {
            int sx, sy;
            getTextCharPos(br[i], sx, sy);
            p.drawPixmap(111 + i * charW, 43, bmp.text, sx, sy, charW, charH);
        }
        
        // Draw "KHZ" label area - sample rate  
        for (int i = 0; i < sr.length() && i < 2; i++) {
            int sx, sy;
            getTextCharPos(sr[i], sx, sy);
            p.drawPixmap(156 + i * charW, 43, bmp.text, sx, sy, charW, charH);
        }
    }
    
    void drawVisualization(QPainter &p) {
        // Simple spectrum analyzer in viz area (24,43 76x16)
        if (player->playbackState() == QMediaPlayer::PlayingState) {
            for (int i = 0; i < 19; i++) {
                int h = QRandomGenerator::global()->bounded(2, 14);
                int x = 24 + i * 4;
                
                for (int j = 0; j < h; j++) {
                    int y = 58 - j;
                    int g = 80 + (j * 175 / 14);
                    p.setPen(QColor(0, g, 0));
                    p.drawLine(x, y, x + 2, y);
                }
            }
        }
    }
    
    void drawFallbackUI(QPainter &p) {
        p.fillRect(rect(), QColor(66, 66, 99));
        QLinearGradient titleGrad(0, 0, 0, 14);
        titleGrad.setColorAt(0, QColor(82, 90, 132));
        titleGrad.setColorAt(1, QColor(58, 66, 107));
        p.fillRect(0, 0, width(), 14, titleGrad);
        p.setPen(QColor(0, 255, 0));
        p.setFont(QFont("Tahoma", 7, QFont::Bold));
        p.drawText(6, 10, "*** Winamp 5.666 ***");
    }
    
    void mousePressEvent(QMouseEvent *event) override {
        int x = event->pos().x();
        int y = event->pos().y();
        
        // Title bar
        if (y < 14) {
            if (x >= 264 && x < 273) { close(); return; }           // Close
            if (x >= 244 && x < 253) { showMinimized(); return; }   // Minimize
            isDragging = true;
            dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            return;
        }
        
        // Transport buttons
        int btnId = getButtonAt(x, y);
        if (btnId >= 0) {
            pressedButton = btnId;
            update();
            return;
        }
        
        // Shuffle button: (164,89) to (211,104)
        if (x >= 164 && x < 211 && y >= 89 && y < 104) {
            shuffleOn = !shuffleOn;
            update();
            return;
        }
        
        // Repeat button: (210,89) to (238,104)
        if (x >= 210 && x < 238 && y >= 89 && y < 104) {
            repeatOn = !repeatOn;
            update();
            return;
        }
        
        // EQ button: (219,58) to (242,70)
        if (x >= 219 && x < 242 && y >= 58 && y < 70) {
            eqBtnOn = !eqBtnOn;
            if (eqBtnOn) eqWindow->show(); else eqWindow->hide();
            update();
            return;
        }
        
        // PL button: (242,58) to (265,70)
        if (x >= 242 && x < 265 && y >= 58 && y < 70) {
            plBtnOn = !plBtnOn;
            if (plBtnOn) playlistWindow->show(); else playlistWindow->hide();
            update();
            return;
        }
        
        // Volume slider: (107,57) to (175,70)
        if (x >= 107 && x <= 175 && y >= 57 && y <= 70) {
            isDraggingVolume = true;
            volume = ((x - 107) * 255) / 68;
            if (volume > 255) volume = 255;
            if (volume < 0) volume = 0;
            audioOutput->setVolume(volume / 255.0f);
            update();
            return;
        }
        
        // Position bar: (16,72) to (264,82)
        if (x >= 16 && x <= 264 && y >= 72 && y <= 82 && player->duration() > 0) {
            isDraggingPos = true;
            qint64 newPos = ((qint64)(x - 16) * player->duration()) / 248;
            player->setPosition(newPos);
            update();
            return;
        }
        
        update();
    }
    
    void mouseMoveEvent(QMouseEvent *event) override {
        int x = event->position().x();
        int y = event->position().y();
        
        // Update hovered button
        int oldHover = hoveredButton;
        hoveredButton = getButtonAt(x, y);
        if (oldHover != hoveredButton) update();
        
        // Volume drag
        if (isDraggingVolume) {
            volume = ((x - 107) * 255) / 68;
            if (volume > 255) volume = 255;
            if (volume < 0) volume = 0;
            audioOutput->setVolume(volume / 255.0f);
            update();
        }
        
        // Position drag
        if (isDraggingPos && player->duration() > 0) {
            int clampX = qBound(16, (int)x, 264);
            qint64 newPos = ((qint64)(clampX - 16) * player->duration()) / 248;
            player->setPosition(newPos);
            update();
        }
        
        if (isDragging) {
            move(event->globalPosition().toPoint() - dragPosition);
            playlistWindow->followMain();
            eqWindow->followMain();
        }
    }
    
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (pressedButton >= 0) {
            int x = event->pos().x();
            int y = event->pos().y();
            int btnId = getButtonAt(x, y);
            
            if (btnId == pressedButton) {
                switch (btnId) {
                    case 0: player->setPosition(0); break;          // Previous
                    case 1:                                          // Play
                        if (!currentFile.isEmpty()) player->play();
                        else openFile();
                        break;
                    case 2: player->pause(); break;                  // Pause
                    case 3: player->stop(); break;                   // Stop
                    case 4: break;                                   // Next
                    case 5: openFile(); break;                       // Eject
                }
            }
            pressedButton = -1;
            update();
        }
        
        isDraggingVolume = false;
        isDraggingPos = false;
        isDragging = false;
    }
    
    void openFile() {
        QString fileName = QFileDialog::getOpenFileName(this, "Open Audio File", "",
            "Audio Files (*.mp3 *.wav *.flac *.ogg *.m4a);;All Files (*)");
        if (!fileName.isEmpty()) {
            currentFile = fileName;
            player->setSource(QUrl::fromLocalFile(fileName));
            player->play();
            playlistWindow->addTrack(fileName);
        }
    }
    
    void updateDisplay() {
        update();
    }

    int getButtonAt(int x, int y) {
        if (y >= 88 && y <= 106) {
            if (x >= 16 && x < 39)  return 0;  // Previous
            if (x >= 39 && x < 62)  return 1;  // Play
            if (x >= 62 && x < 85)  return 2;  // Pause
            if (x >= 85 && x < 108) return 3;  // Stop
            if (x >= 108 && x < 130) return 4; // Next
        }
        if (y >= 89 && y <= 105 && x >= 136 && x < 158) return 5; // Eject
        return -1;
    }

private:
    QMediaPlayer *player;
    QAudioOutput *audioOutput;
    QTimer *timer;
    QTimer *scrollTimer;
    QString currentFile;
    QPoint dragPosition;
    bool isDragging;
    int volume;  // 0-255 like original
    int hoveredButton;
    int pressedButton;
    bool shuffleOn, repeatOn, eqBtnOn, plBtnOn;
    bool isDraggingVolume, isDraggingPos;
    int scrollOffset;
    
    PlaylistWindow *playlistWindow;
    EqualizerWindow *eqWindow;
};

// PlaylistWindow snap methods
void PlaylistWindow::checkSnap() {
    if (!mainWindow) return;
    
    const int snapDist = 15;
    QPoint mainPos = mainWindow->pos();
    QSize mainSize = mainWindow->size();
    QPoint myPos = pos();
    
    if (qAbs(myPos.x() - (mainPos.x() + mainSize.width())) < snapDist &&
        qAbs(myPos.y() - mainPos.y()) < snapDist) {
        move(mainPos.x() + mainSize.width(), mainPos.y());
        isSnappedToMain = true;
    } else {
        isSnappedToMain = false;
    }
}

void PlaylistWindow::followMain() {
    if (isSnappedToMain && mainWindow && isVisible()) {
        move(mainWindow->pos().x() + mainWindow->width(), mainWindow->pos().y());
    }
}

// EqualizerWindow snap methods
void EqualizerWindow::checkSnap() {
    if (!mainWindow) return;
    
    const int snapDist = 15;
    QPoint mainPos = mainWindow->pos();
    QSize mainSize = mainWindow->size();
    QPoint myPos = pos();
    
    if (qAbs(myPos.x() - mainPos.x()) < snapDist &&
        qAbs(myPos.y() - (mainPos.y() + mainSize.height())) < snapDist) {
        move(mainPos.x(), mainPos.y() + mainSize.height());
        isSnappedToMain = true;
    } else {
        isSnappedToMain = false;
    }
}

void EqualizerWindow::followMain() {
    if (isSnappedToMain && mainWindow && isVisible()) {
        move(mainWindow->pos().x(), mainWindow->pos().y() + mainWindow->height());
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Load authentic Winamp bitmaps - try multiple paths
    QStringList searchPaths = {
        "/home/ender/winamp/Src/Winamp/resource",  // Absolute path
        "../../Src/Winamp/resource",                // Relative from build dir
        "../Src/Winamp/resource"                    // Relative from demo dir
    };
    
    bool loaded = false;
    QString usedPath;
    for (const QString &path : searchPaths) {
        if (WinampBitmaps::instance().loadAll(path)) {
            loaded = true;
            usedPath = path;
            break;
        }
    }
    
    if (!loaded) {
        qWarning("Warning: Could not load Winamp bitmaps from any search path");
        qWarning("Continuing with fallback rendered graphics...");
    } else {
        qInfo("Successfully loaded authentic Winamp bitmaps from: %s", qPrintable(usedPath));
    }
    
    WinampWindow window;
    window.show();
    
    return app.exec();
}

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
        
        // Use authentic Pledit.bmp if available
        if (!WinampBitmaps::instance().pledit.isNull()) {
            // Draw tiled/stretched playlist background
            p.drawPixmap(0, 0, width(), 14, WinampBitmaps::instance().pledit, 0, 0, 275, 20);
        } else {
            // Fallback to gradient
            QLinearGradient titleGrad(0, 0, 0, 14);
            titleGrad.setColorAt(0, QColor(82, 90, 132));
            titleGrad.setColorAt(1, QColor(58, 66, 107));
            p.fillRect(0, 0, width(), 14, titleGrad);
        }
        
        p.setPen(QColor(0, 255, 0));
        p.setFont(QFont("Tahoma", 7, QFont::Bold));
        p.drawText(6, 10, "Winamp Playlist Editor");
        
        // Close button
        p.fillRect(width() - 12, 3, 9, 9, QColor(66, 74, 107));
        p.setPen(QColor(198, 0, 0));
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.drawText(width() - 10, 10, "X");
    }
    
    void mousePressEvent(QMouseEvent *event) override {
        if (event->pos().y() < 14) {
            if (event->pos().x() > width() - 12) {
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
        
        // Use authentic Eqmain.bmp if available
        if (!WinampBitmaps::instance().eqmain.isNull()) {
            p.drawPixmap(0, 0, WinampBitmaps::instance().eqmain);
        } else {
            // Fallback
            p.fillRect(rect(), QColor(66, 66, 99));
            
            // Title bar
            QLinearGradient titleGrad(0, 0, 0, 14);
            titleGrad.setColorAt(0, QColor(82, 90, 132));
            titleGrad.setColorAt(1, QColor(58, 66, 107));
            p.fillRect(0, 0, width(), 14, titleGrad);
            
            p.setPen(QColor(0, 255, 0));
            p.setFont(QFont("Tahoma", 7, QFont::Bold));
            p.drawText(6, 10, "Winamp Equalizer");
        }
    }
    
    void mousePressEvent(QMouseEvent *event) override {
        int x = event->pos().x();
        int y = event->pos().y();
        
        // Title bar drag
        if (y < 14) {
            if (x > width() - 12) {
                hide();
                return;
            }
            isDragging = true;
            dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            return;
        }
        
        // ON/OFF toggle (approximate position)
        if (x >= 10 && x <= 40 && y >= 95 && y <= 110) {
            eqEnabled = !eqEnabled;
            update();
            return;
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
    int eqValues[10];
    int preampValue;
    bool eqEnabled = true;
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
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 14, 0, 0);
    layout->setSpacing(0);
    
    listWidget = new QListWidget(this);
    listWidget->setStyleSheet(
        "QListWidget {"
        "  background-color: #000000;"
        "  color: #00FF00;"
        "  border: 1px solid #333333;"
        "  font-family: 'Courier';"
        "  font-size: 10pt;"
        "  selection-background-color: #0000AA;"
        "  selection-color: #00FF00;"
        "}"
    );
    layout->addWidget(listWidget);
}

// Equalizer Window Constructor
EqualizerWindow::EqualizerWindow(WinampWindow *parent) : QWidget(nullptr), mainWindow(parent) {
    setFixedSize(275, 116);
    setWindowTitle("Winamp Equalizer");
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    
    // Initialize EQ bands to center (0)
    for (int i = 0; i < 10; i++) {
        eqValues[i] = 50;
    }
    preampValue = 50;
}

// Main Winamp Window
class WinampWindow : public QWidget {
public:
    WinampWindow(QWidget *parent = nullptr) : QWidget(parent), dragPosition(0,0), isDragging(false), volume(50) {
        setFixedSize(275, 116);
        setWindowTitle("Winamp 5.666 for Linux");
        setWindowFlags(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, false);
        
        // Setup audio
        player = new QMediaPlayer(this);
        audioOutput = new QAudioOutput(this);
        player->setAudioOutput(audioOutput);
        audioOutput->setVolume(0.5);
        
        // Update timer
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &WinampWindow::updateDisplay);
        timer->start(50);
        
        connect(player, &QMediaPlayer::positionChanged, this, [this](qint64 pos) {
            update();
        });
        
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
        
        // Use authentic MAIN.BMP if loaded
        if (!WinampBitmaps::instance().main.isNull()) {
            p.drawPixmap(0, 0, WinampBitmaps::instance().main, 0, 0, 275, 116);
            
            // Draw authentic LED time display using numbers.bmp
            if (!WinampBitmaps::instance().numbers.isNull()) {
                qint64 pos = player->position();
                int secs = (pos / 1000) % 60;
                int mins = (pos / 1000) / 60;
                
                // Each number in numbers.bmp is 9x13 pixels
                // Numbers are arranged horizontally: 0-9, then minus sign
                int digitWidth = 9;
                int digitHeight = 13;
                int displayX = 48;  // Position in MAIN.BMP where time displays
                int displayY = 26;
                
                // Draw minutes (1 digit)
                int minsDigit = mins % 10;
                p.drawPixmap(displayX, displayY, WinampBitmaps::instance().numbers, 
                            minsDigit * digitWidth, 0, digitWidth, digitHeight);
                
                // Draw seconds (2 digits)
                int secsTens = (secs / 10) % 10;
                int secsOnes = secs % 10;
                p.drawPixmap(displayX + 15, displayY, WinampBitmaps::instance().numbers,
                            secsTens * digitWidth, 0, digitWidth, digitHeight);
                p.drawPixmap(displayX + 23, displayY, WinampBitmaps::instance().numbers,
                            secsOnes * digitWidth, 0, digitWidth, digitHeight);
            }
            
            // Draw play/pause indicator from PLAYPAUS.BMP
            if (!WinampBitmaps::instance().playpaus.isNull()) {
                int state = (player->playbackState() == QMediaPlayer::PlayingState) ? 0 : 1;
                p.drawPixmap(24, 28, WinampBitmaps::instance().playpaus, state * 9, 0, 9, 9);
            }
            
            // Draw stereo/mono indicator from MONOSTER.BMP
            if (!WinampBitmaps::instance().monoster.isNull() && 
                player->playbackState() == QMediaPlayer::PlayingState) {
                // 0 = stereo, 1 = mono
                p.drawPixmap(212, 41, WinampBitmaps::instance().monoster, 0, 0, 29, 12);
            }
            
        } else {
            // Fallback to painted version if bitmaps not loaded
            drawFallbackUI(p);
        }
    }
    
    void drawFallbackUI(QPainter &p) {
        // Same as previous painted version
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
            if (x > width() - 12) {
                close();
                return;
            }
            if (x > width() - 23) {
                showMinimized();
                return;
            }
            isDragging = true;
            dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            return;
        }
        
        // Control buttons (using MAIN.BMP coordinates)
        // Play button: x=16-39, y=88-106
        if (x >= 16 && x < 40 && y >= 88 && y <= 106) {
            if (!currentFile.isEmpty()) {
                player->play();
            } else {
                openFile();
            }
        }
        // Pause: 39-62
        else if (x >= 39 && x < 62 && y >= 88 && y <= 106) {
            player->pause();
        }
        // Stop: 62-85
        else if (x >= 62 && x < 85 && y >= 88 && y <= 106) {
            player->stop();
        }
        // Open: 136-159
        else if (x >= 136 && x < 159 && y >= 88 && y <= 106) {
            openFile();
        }
        
        update();
    }
    
    void mouseMoveEvent(QMouseEvent *event) override {
        if (isDragging) {
            move(event->globalPosition().toPoint() - dragPosition);
            playlistWindow->followMain();
            eqWindow->followMain();
        }
    }
    
    void mouseReleaseEvent(QMouseEvent *event) override {
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

private:
    QMediaPlayer *player;
    QAudioOutput *audioOutput;
    QTimer *timer;
    QString currentFile;
    QPoint dragPosition;
    bool isDragging;
    int volume;
    
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

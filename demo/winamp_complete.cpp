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
#include <QSlider>

// Forward declaration
class WinampWindow;

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
        
        // Title bar
        QLinearGradient titleGrad(0, 0, 0, 14);
        titleGrad.setColorAt(0, QColor(82, 90, 132));
        titleGrad.setColorAt(1, QColor(58, 66, 107));
        p.fillRect(0, 0, width(), 14, titleGrad);
        
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
        p.fillRect(rect(), QColor(66, 66, 99));
        
        // Title bar
        QLinearGradient titleGrad(0, 0, 0, 14);
        titleGrad.setColorAt(0, QColor(82, 90, 132));
        titleGrad.setColorAt(1, QColor(58, 66, 107));
        p.fillRect(0, 0, width(), 14, titleGrad);
        
        p.setPen(QColor(0, 255, 0));
        p.setFont(QFont("Tahoma", 7, QFont::Bold));
        p.drawText(6, 10, "Winamp Equalizer");
        
        // Close button
        p.fillRect(width() - 12, 3, 9, 9, QColor(66, 74, 107));
        p.setPen(QColor(198, 0, 0));
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.drawText(width() - 10, 10, "X");
        
        // Draw preamp slider
        p.setPen(QColor(200, 200, 200));
        p.setFont(QFont("Arial", 6));
        p.drawText(10, 28, "Preamp");
        drawSlider(p, 15, 32, preampValue);
        
        // Draw EQ bands
        const char* labels[] = {"60", "170", "310", "600", "1k", "3k", "6k", "12k", "14k", "16k"};
        int startX = 50;
        for (int i = 0; i < 10; i++) {
            p.drawText(startX + i * 22 - 5, 28, labels[i]);
            drawSlider(p, startX + i * 22, 32, eqValues[i]);
        }
        
        // ON/OFF button
        p.fillRect(10, 95, 30, 15, eqEnabled ? QColor(0, 180, 0) : QColor(90, 98, 132));
        p.setPen(QColor(255, 255, 255));
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.drawText(13, 106, eqEnabled ? "ON" : "OFF");
        
        // Auto button
        p.fillRect(45, 95, 30, 15, QColor(90, 98, 132));
        p.drawText(50, 106, "Auto");
    }
    
    void drawSlider(QPainter &p, int x, int y, int value) {
        // Slider track
        p.fillRect(x, y, 14, 50, QColor(24, 24, 41));
        p.setPen(QColor(8, 8, 16));
        p.drawRect(x, y, 14, 50);
        
        // Center line
        p.setPen(QColor(100, 100, 100));
        p.drawLine(x, y + 25, x + 14, y + 25);
        
        // Slider thumb
        int thumbY = y + ((100 - value) * 45 / 100);
        p.fillRect(x + 2, thumbY, 10, 5, QColor(132, 140, 173));
        p.setPen(QColor(165, 173, 206));
        p.drawRect(x + 2, thumbY, 10, 5);
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
        
        // ON/OFF toggle
        if (x >= 10 && x <= 40 && y >= 95 && y <= 110) {
            eqEnabled = !eqEnabled;
            update();
            return;
        }
        
        // Adjust sliders
        if (y >= 32 && y <= 82) {
            // Preamp
            if (x >= 15 && x <= 29) {
                preampValue = 100 - ((y - 32) * 100 / 50);
                update();
            }
            // EQ bands
            int startX = 50;
            for (int i = 0; i < 10; i++) {
                if (x >= startX + i * 22 && x <= startX + i * 22 + 14) {
                    eqValues[i] = 100 - ((y - 32) * 100 / 50);
                    update();
                    break;
                }
            }
        }
    }
    
    void mouseMoveEvent(QMouseEvent *event) override {
        if (isDragging) {
            QPoint newPos = event->globalPosition().toPoint() - dragPosition;
            move(newPos);
            checkSnap();
        } else {
            // Allow dragging sliders
            mousePressEvent(event);
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
        eqValues[i] = 50; // 50 = center position
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
        
        // Base color
        p.fillRect(rect(), QColor(66, 66, 99));
        
        // Title bar
        QLinearGradient titleGrad(0, 0, 0, 14);
        titleGrad.setColorAt(0, QColor(82, 90, 132));
        titleGrad.setColorAt(1, QColor(58, 66, 107));
        p.fillRect(0, 0, width(), 14, titleGrad);
        
        p.setPen(QColor(132, 148, 198));
        p.drawLine(0, 1, width(), 1);
        p.setPen(QColor(33, 41, 66));
        p.drawLine(0, 13, width(), 13);
        
        // Title text
        p.setPen(QColor(0, 255, 0));
        p.setFont(QFont("Tahoma", 7, QFont::Bold));
        p.drawText(6, 10, "*** Winamp 5.666 ***");
        
        // Menu buttons (Winamp icon style)
        drawMenuButton(p, 6, 88, "O", playlistWindow->isVisible());  // Playlist toggle
        drawMenuButton(p, 6, 100, "D", false);  // Unused
        
        // Title buttons
        drawTitleButton(p, width() - 12, 3, "X", QColor(198, 0, 0));
        drawTitleButton(p, width() - 23, 3, "_", QColor(0, 0, 0));
        
        // Display area
        p.fillRect(16, 18, 155, 12, QColor(0, 0, 0));
        p.setPen(QColor(33, 41, 66));
        p.drawRect(15, 17, 156, 13);
        
        // Time display
        qint64 pos = player->position();
        int secs = (pos / 1000) % 60;
        int mins = (pos / 1000) / 60;
        QString timeStr = QString("%1:%2").arg(mins, 1).arg(secs, 2, 10, QChar('0'));
        
        QFont ledFont("Courier", 10, QFont::Bold);
        p.setFont(ledFont);
        p.setPen(QColor(24, 243, 24, 100));
        p.drawText(18, 28, timeStr);
        p.setPen(QColor(24, 243, 24));
        p.drawText(19, 27, timeStr);
        
        // Status indicators
        p.setFont(QFont("Arial", 6));
        p.setPen(QColor(24, 243, 24));
        p.drawText(85, 27, player->playbackState() == QMediaPlayer::PlayingState ? "128 kbps" : "");
        p.drawText(130, 27, player->playbackState() == QMediaPlayer::PlayingState ? "stereo" : "");
        
        // Visualizers
        p.fillRect(16, 43, 76, 16, QColor(0, 0, 8));
        p.setPen(QColor(24, 41, 24));
        p.drawRect(15, 42, 77, 17);
        
        if (player->playbackState() == QMediaPlayer::PlayingState) {
            p.setPen(QColor(0, 200, 0));
            int centerY = 51;
            for (int i = 0; i < 75; i++) {
                int val = QRandomGenerator::global()->bounded(15) - 7;
                p.drawPoint(17 + i, centerY + val);
            }
        }
        
        // Spectrum analyzer
        p.fillRect(107, 43, 58, 16, QColor(0, 0, 8));
        p.setPen(QColor(24, 41, 24));
        p.drawRect(106, 42, 59, 17);
        
        if (player->playbackState() == QMediaPlayer::PlayingState) {
            for (int i = 0; i < 16; i++) {
                int height = QRandomGenerator::global()->bounded(14) + 1;
                for (int y = 0; y < height; y++) {
                    int intensity = 255 - (y * 20);
                    p.setPen(QColor(0, intensity, 0));
                    p.drawLine(109 + i * 3, 57 - y, 110 + i * 3, 57 - y);
                }
            }
        }
        
        // Position slider
        p.fillRect(16, 72, 248, 10, QColor(24, 24, 41));
        p.setPen(QColor(8, 8, 16));
        p.drawRect(15, 71, 249, 11);
        
        if (player->duration() > 0) {
            int thumbPos = 16 + (player->position() * 238) / player->duration();
            QLinearGradient thumbGrad(thumbPos, 72, thumbPos, 81);
            thumbGrad.setColorAt(0, QColor(140, 148, 181));
            thumbGrad.setColorAt(1, QColor(82, 90, 123));
            p.fillRect(thumbPos, 72, 10, 10, thumbGrad);
            p.setPen(QColor(165, 173, 206));
            p.drawRect(thumbPos, 72, 10, 10);
        }
        
        // Control buttons
        int btnY = 88;
        drawWinampButton(p, 16, btnY, "<<", false);
        drawWinampButton(p, 39, btnY, ">", player->playbackState() == QMediaPlayer::PlayingState);
        drawWinampButton(p, 62, btnY, "||", false);
        drawWinampButton(p, 85, btnY, "[]", false);
        drawWinampButton(p, 108, btnY, ">>", false);
        drawWinampButton(p, 136, btnY, "^^", false);
        
        // Volume slider
        drawVolumeSlider(p, 107, 57, volume);
        
        // Equalizer toggle button
        drawSmallButton(p, 219, 58, "EQ", eqWindow->isVisible());
        drawSmallButton(p, 242, 58, "PL", playlistWindow->isVisible());
        
        // Filename display
        p.setFont(QFont("Arial", 6));
        p.setPen(QColor(132, 148, 198));
        if (!currentFile.isEmpty()) {
            QFileInfo fi(currentFile);
            QString fname = fi.fileName();
            if (fname.length() > 30) fname = fname.left(27) + "...";
            p.drawText(180, 27, fname);
        }
    }
    
    void drawMenuButton(QPainter &p, int x, int y, const QString &text, bool active) {
        p.fillRect(x, y, 8, 9, active ? QColor(0, 180, 0) : QColor(90, 98, 132));
        p.setPen(active ? QColor(255, 255, 255) : QColor(200, 200, 200));
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.drawText(x + 2, y + 7, text);
    }
    
    void drawSmallButton(QPainter &p, int x, int y, const QString &text, bool active) {
        p.fillRect(x, y, 18, 12, active ? QColor(0, 180, 0) : QColor(90, 98, 132));
        p.setPen(active ? QColor(255, 255, 255) : QColor(200, 200, 200));
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.drawText(x + 2, y + 9, text);
    }
    
    void drawTitleButton(QPainter &p, int x, int y, const QString &text, const QColor &color) {
        p.fillRect(x, y, 9, 9, QColor(66, 74, 107));
        p.setPen(color);
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.drawText(x + 2, y + 7, text);
    }
    
    void drawWinampButton(QPainter &p, int x, int y, const QString &text, bool active) {
        QColor baseColor = active ? QColor(0, 180, 0) : QColor(90, 98, 132);
        QColor lightColor = active ? QColor(0, 240, 0) : QColor(132, 140, 173);
        QColor darkColor = active ? QColor(0, 120, 0) : QColor(49, 57, 90);
        
        p.fillRect(x, y, 23, 18, baseColor);
        
        p.setPen(lightColor);
        p.drawLine(x, y, x + 22, y);
        p.drawLine(x, y, x, y + 17);
        
        p.setPen(darkColor);
        p.drawLine(x, y + 17, x + 22, y + 17);
        p.drawLine(x + 22, y, x + 22, y + 17);
        
        p.setPen(baseColor.darker(120));
        p.drawLine(x + 1, y + 16, x + 21, y + 16);
        p.drawLine(x + 21, y + 1, x + 21, y + 16);
        
        p.setPen(active ? QColor(255, 255, 255) : QColor(200, 200, 200));
        p.setFont(QFont("Arial", 9, QFont::Bold));
        QRect textRect(x, y, 23, 18);
        p.drawText(textRect, Qt::AlignCenter, text);
    }
    
    void drawVolumeSlider(QPainter &p, int x, int y, int vol) {
        p.fillRect(x, y + 12, 58, 4, QColor(8, 8, 16));
        
        int volWidth = (vol * 58) / 100;
        QLinearGradient volGrad(x, y + 12, x, y + 16);
        volGrad.setColorAt(0, QColor(0, 200, 0));
        volGrad.setColorAt(1, QColor(0, 140, 0));
        p.fillRect(x, y + 12, volWidth, 4, volGrad);
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
        
        // EQ toggle button
        if (x >= 219 && x <= 237 && y >= 58 && y <= 70) {
            if (eqWindow->isVisible()) {
                eqWindow->hide();
            } else {
                eqWindow->move(this->x(), this->y() + height());
                eqWindow->show();
            }
            update();
            return;
        }
        
        // PL toggle button
        if (x >= 242 && x <= 260 && y >= 58 && y <= 70) {
            if (playlistWindow->isVisible()) {
                playlistWindow->hide();
            } else {
                playlistWindow->move(this->x() + width(), this->y());
                playlistWindow->show();
            }
            update();
            return;
        }
        
        // Control buttons
        if (y >= 88 && y <= 106) {
            if (x >= 39 && x < 62) {  // Play
                if (!currentFile.isEmpty()) {
                    player->play();
                } else {
                    openFile();
                }
            }
            else if (x >= 62 && x < 85) {  // Pause
                player->pause();
            }
            else if (x >= 85 && x < 108) {  // Stop
                player->stop();
            }
            else if (x >= 136 && x < 159) {  // Open
                openFile();
            }
        }
        
        // Volume control
        if (x >= 107 && x <= 165 && y >= 69 && y <= 73) {
            volume = ((x - 107) * 100) / 58;
            audioOutput->setVolume(volume / 100.0);
        }
        
        update();
    }
    
    void mouseMoveEvent(QMouseEvent *event) override {
        if (isDragging) {
            move(event->globalPosition().toPoint() - dragPosition);
            
            // Move snapped windows along with main window
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
    
    bool snapped = false;
    QPoint snapPos = myPos;
    
    // Snap to right edge of main window
    if (qAbs(myPos.x() - (mainPos.x() + mainSize.width())) < snapDist &&
        qAbs(myPos.y() - mainPos.y()) < snapDist) {
        snapPos = QPoint(mainPos.x() + mainSize.width(), mainPos.y());
        snapped = true;
        isSnappedToMain = true;
    } else {
        isSnappedToMain = false;
    }
    
    if (snapped) {
        move(snapPos);
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
    
    bool snapped = false;
    QPoint snapPos = myPos;
    
    // Snap to bottom edge of main window
    if (qAbs(myPos.x() - mainPos.x()) < snapDist &&
        qAbs(myPos.y() - (mainPos.y() + mainSize.height())) < snapDist) {
        snapPos = QPoint(mainPos.x(), mainPos.y() + mainSize.height());
        snapped = true;
        isSnappedToMain = true;
    } else {
        isSnappedToMain = false;
    }
    
    if (snapped) {
        move(snapPos);
    }
}

void EqualizerWindow::followMain() {
    if (isSnappedToMain && mainWindow && isVisible()) {
        move(mainWindow->pos().x(), mainWindow->pos().y() + mainWindow->height());
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    WinampWindow window;
    window.show();
    
    return app.exec();
}

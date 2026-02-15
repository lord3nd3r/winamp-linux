#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QFileDialog>
#include <QMouseEvent>
#include <QTimer>
#include <QRandomGenerator>

class WinampWindow : public QWidget {
public:
    WinampWindow(QWidget *parent = nullptr) : QWidget(parent), dragPosition(0,0), isDragging(false), volume(50) {
        // Classic Winamp size: 275x116 pixels
        setFixedSize(275, 116);
        setWindowTitle("Winamp 5.666 for Linux");
        setWindowFlags(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, false);
        
        // Setup audio
        player = new QMediaPlayer(this);
        audioOutput = new QAudioOutput(this);
        player->setAudioOutput(audioOutput);
        audioOutput->setVolume(0.5);
        
        // Update timer for display
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &WinampWindow::updateDisplay);
        timer->start(50); // Faster refresh for smoother viz
        
        connect(player, &QMediaPlayer::positionChanged, this, [this](qint64 pos) {
            update();
        });
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false); // Pixel-perfect rendering
        
        // Classic Winamp base color - dark metallic blue/grey
        p.fillRect(rect(), QColor(66, 66, 99));
        
        // Title bar with gradient
        QLinearGradient titleGrad(0, 0, 0, 14);
        titleGrad.setColorAt(0, QColor(82, 90, 132));
        titleGrad.setColorAt(1, QColor(58, 66, 107));
        p.fillRect(0, 0, width(), 14, titleGrad);
        
        // Title bar highlight
        p.setPen(QColor(132, 148, 198));
        p.drawLine(0, 1, width(), 1);
        p.setPen(QColor(33, 41, 66));
        p.drawLine(0, 13, width(), 13);
        
        // Title text - classic Winamp green
        p.setPen(QColor(0, 255, 0));
        p.setFont(QFont("Tahoma", 7, QFont::Bold));
        p.drawText(6, 10, "*** Winamp 5.666 ***");
        
        // Close/minimize buttons (simplified)
        drawTitleButton(p, width() - 12, 3, "X", QColor(198, 0, 0));
        drawTitleButton(p, width() - 23, 3, "_", QColor(0, 0, 0));
        
        // Main display area background
        p.fillRect(16, 18, 155, 12, QColor(0, 0, 0));
        p.setPen(QColor(33, 41, 66));
        p.drawRect(15, 17, 156, 13);
        p.setPen(QColor(99, 107, 140));
        p.drawLine(16, 18, 170, 18);
        p.drawLine(16, 18, 16, 29);
        
        // Time display - classic Winamp LED style
        qint64 pos = player->position();
        int secs = (pos / 1000) % 60;
        int mins = (pos / 1000) / 60;
        QString timeStr = QString("%1:%2").arg(mins, 1).arg(secs, 2, 10, QChar('0'));
        
        QFont ledFont("Courier", 10, QFont::Bold);
        p.setFont(ledFont);
        // LED glow effect
        p.setPen(QColor(24, 243, 24, 100));
        p.drawText(18, 28, timeStr);
        // Bright LED text
        p.setPen(QColor(24, 243, 24));
        p.drawText(19, 27, timeStr);
        
        // Bit rate / Sample rate display
        p.setFont(QFont("Arial", 6));
        p.setPen(QColor(24, 243, 24));
        p.drawText(85, 27, player->playbackState() == QMediaPlayer::PlayingState ? "128 kbps" : "");
        
        // Mono/Stereo indicator
        p.drawText(130, 27, player->playbackState() == QMediaPlayer::PlayingState ? "stereo" : "");
        
        // Main visualizer area
        p.fillRect(16, 43, 76, 16, QColor(0, 0, 8));
        p.setPen(QColor(24, 41, 24));
        p.drawRect(15, 42, 77, 17);
        
        // Oscilloscope-style visualizer
        if (player->playbackState() == QMediaPlayer::PlayingState) {
            p.setPen(QColor(0, 200, 0));
            int centerY = 51;
            for (int i = 0; i < 75; i++) {
                int val = QRandomGenerator::global()->bounded(15) - 7;
                p.drawPoint(17 + i, centerY + val);
            }
        }
        
        // Spectrum analyzer bars
        p.fillRect(107, 43, 58, 16, QColor(0, 0, 8));
        p.setPen(QColor(24, 41, 24));
        p.drawRect(106, 42, 59, 17);
        
        if (player->playbackState() == QMediaPlayer::PlayingState) {
            for (int i = 0; i < 16; i++) {
                int height = QRandomGenerator::global()->bounded(14) + 1;
                // Draw spectrum bars with gradient
                for (int y = 0; y < height; y++) {
                    int intensity = 255 - (y * 20);
                    p.setPen(QColor(0, intensity, 0));
                    p.drawLine(109 + i * 3, 57 - y, 110 + i * 3, 57 - y);
                }
            }
        }
        
        // Position slider track
        p.fillRect(16, 72, 248, 10, QColor(24, 24, 41));
        p.setPen(QColor(8, 8, 16));
        p.drawRect(15, 71, 249, 11);
        
        // Position slider thumb
        if (player->duration() > 0) {
            int thumbPos = 16 + (player->position() * 238) / player->duration();
            QLinearGradient thumbGrad(thumbPos, 72, thumbPos, 81);
            thumbGrad.setColorAt(0, QColor(140, 148, 181));
            thumbGrad.setColorAt(1, QColor(82, 90, 123));
            p.fillRect(thumbPos, 72, 10, 10, thumbGrad);
            p.setPen(QColor(165, 173, 206));
            p.drawRect(thumbPos, 72, 10, 10);
        }
        
        // Control buttons - authentic Winamp style
        int btnY = 88;
        drawWinampButton(p, 16, btnY, "<<", false);  // Prev
        drawWinampButton(p, 39, btnY, ">", player->playbackState() == QMediaPlayer::PlayingState);  // Play
        drawWinampButton(p, 62, btnY, "||", false);  // Pause
        drawWinampButton(p, 85, btnY, "[]", false);  // Stop
        drawWinampButton(p, 108, btnY, ">>", false);  // Next
        drawWinampButton(p, 136, btnY, "^^", false);  // Eject/Open
        
        // Volume slider
        drawVolumeSlider(p, 107, 57, volume);
        
        // Status indicators at bottom
        p.setFont(QFont("Arial", 6));
        p.setPen(QColor(132, 148, 198));
        if (!currentFile.isEmpty()) {
            QFileInfo fi(currentFile);
            QString fname = fi.fileName();
            if (fname.length() > 30) fname = fname.left(27) + "...";
            p.drawText(180, 27, fname);
        }
    }
    
    void drawTitleButton(QPainter &p, int x, int y, const QString &text, const QColor &color) {
        p.fillRect(x, y, 9, 9, QColor(66, 74, 107));
        p.setPen(color);
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.drawText(x + 2, y + 7, text);
    }
    
    void drawWinampButton(QPainter &p, int x, int y, const QString &text, bool active) {
        // Button base with classic Winamp 3D look
        QColor baseColor = active ? QColor(0, 180, 0) : QColor(90, 98, 132);
        QColor lightColor = active ? QColor(0, 240, 0) : QColor(132, 140, 173);
        QColor darkColor = active ? QColor(0, 120, 0) : QColor(49, 57, 90);
        
        p.fillRect(x, y, 23, 18, baseColor);
        
        // 3D borders
        p.setPen(lightColor);
        p.drawLine(x, y, x + 22, y);  // Top
        p.drawLine(x, y, x, y + 17);  // Left
        
        p.setPen(darkColor);
        p.drawLine(x, y + 17, x + 22, y + 17);  // Bottom
        p.drawLine(x + 22, y, x + 22, y + 17);  // Right
        
        // Inner shadow
        p.setPen(baseColor.darker(120));
        p.drawLine(x + 1, y + 16, x + 21, y + 16);
        p.drawLine(x + 21, y + 1, x + 21, y + 16);
        
        // Button text/icon
        p.setPen(active ? QColor(255, 255, 255) : QColor(200, 200, 200));
        p.setFont(QFont("Arial", 9, QFont::Bold));
        QRect textRect(x, y, 23, 18);
        p.drawText(textRect, Qt::AlignCenter, text);
    }
    
    void drawVolumeSlider(QPainter &p, int x, int y, int vol) {
        // Volume bar background  
        p.fillRect(x, y + 12, 58, 4, QColor(8, 8, 16));
        
        // Volume level
        int volWidth = (vol * 58) / 100;
        QLinearGradient volGrad(x, y + 12, x, y + 16);
        volGrad.setColorAt(0, QColor(0, 200, 0));
        volGrad.setColorAt(1, QColor(0, 140, 0));
        p.fillRect(x, y + 12, volWidth, 4, volGrad);
    }
    
    void mousePressEvent(QMouseEvent *event) override {
        int x = event->pos().x();
        int y = event->pos().y();
        
        // Title bar drag
        if (y < 14) {
            if (x > width() - 12) {  // Close button
                close();
                return;
            }
            if (x > width() - 23) {  // Minimize button
                showMinimized();
                return;
            }
            isDragging = true;
            dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
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
        }
    }
    
    void updateDisplay() {
        update();  // Refresh visualization
    }

private:
    QMediaPlayer *player;
    QAudioOutput *audioOutput;
    QTimer *timer;
    QString currentFile;
    QPoint dragPosition;
    bool isDragging;
    int volume;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    WinampWindow window;
    window.show();
    
    return app.exec();
}

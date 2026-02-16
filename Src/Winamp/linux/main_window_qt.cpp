/**
 * main_window_qt.cpp - Classic Winamp main window implementation
 */

#include "main_window_qt.h"
#include "../../Wasabi/qt6/win32_types.h"
#include <QPainter>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QDebug>

// Classic Winamp dimensions
static const int WINAMP_WIDTH = 275;
static const int WINAMP_HEIGHT = 116;
static const int WINAMP_SHADE_HEIGHT = 14;

MainWindowQt::MainWindowQt(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_mediaPlayer(nullptr)
    , m_audioOutput(nullptr)
    , m_currentTrack(0)
    , m_displayTimer(nullptr)
    , m_windowShade(false)
    , m_doubleSizeMode(false)
    , m_equalizerVisible(false)
    , m_playlistVisible(false)
    , m_shuffleEnabled(false)
    , m_repeatEnabled(false)
    , m_skinLoaded(false)
{
    // Set fixed size (classic Winamp is not resizable by default)
    setFixedSize(WINAMP_WIDTH, WINAMP_HEIGHT);
    
    // Enable drag and drop
    setAcceptDrops(true);
    
    // Remove window decorations for custom titlebar
    setWindowFlags(Qt::Widget | Qt::FramelessWindowHint);
    
    // Setup components
    setupUI();
    setupMediaPlayer();
    setupConnections();
    loadDefaultSkin();
    
    // Start display update timer
    m_displayTimer = new QTimer(this);
    connect(m_displayTimer, &QTimer::timeout, this, &MainWindowQt::updateDisplay);
    m_displayTimer->start(50); // 20 FPS for visualization
}

MainWindowQt::~MainWindowQt()
{
    if (m_mediaPlayer) {
        m_mediaPlayer->stop();
    }
}

void MainWindowQt::setupUI()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    
    // We'll do custom drawing, so make it accept paint events
    m_centralWidget->setAttribute(Qt::WA_OpaquePaintEvent);
    m_centralWidget->setAutoFillBackground(false);
    
    // Initialize UI elements (positions will be set in drawControls)
    // For now, we'll do custom painting instead of actual Qt widgets
    // to match the classic Winamp pixel-perfect look
}

void MainWindowQt::setupMediaPlayer()
{
    m_mediaPlayer = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_mediaPlayer->setAudioOutput(m_audioOutput);
    
    // Set default volume (0.0 to 1.0)
    m_audioOutput->setVolume(0.5);
}

void MainWindowQt::setupConnections()
{
    if (m_mediaPlayer) {
        connect(m_mediaPlayer, &QMediaPlayer::positionChanged,
                this, &MainWindowQt::onPositionChanged);
        connect(m_mediaPlayer, &QMediaPlayer::durationChanged,
                this, &MainWindowQt::onDurationChanged);
        connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged,
                this, &MainWindowQt::onMediaStatusChanged);
    }
}

void MainWindowQt::loadDefaultSkin()
{
    // For now, create a basic colored interface
    // Later, this will load actual Winamp skin bitmaps
    m_skinMain = QPixmap(WINAMP_WIDTH, WINAMP_HEIGHT);
    m_skinMain.fill(QColor(0, 0, 0)); // Black background
    
    m_skinLoaded = true;
    update();
}

void MainWindowQt::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    
    drawMainWindow(&painter);
}

void MainWindowQt::drawMainWindow(QPainter *painter)
{
    // Draw background
    painter->fillRect(rect(), QColor(0, 0, 0));
    
    // Draw title bar
    drawTitleBar(painter);
    
    // Draw main display area
    QRect displayRect(10, 22, 255, 62);
    painter->fillRect(displayRect, QColor(0, 32, 0)); // Classic greenish display
    
    // Draw time display
    QString timeText = "00:00";
    if (m_mediaPlayer && m_mediaPlayer->duration() > 0) {
        timeText = formatTime(m_mediaPlayer->position());
    }
    painter->setPen(QColor(0, 255, 0)); // Bright green text
    QFont displayFont("Courier", 12, QFont::Bold);
    painter->setFont(displayFont);
    painter->drawText(QRect(12, 26, 60, 20), Qt::AlignRight | Qt::AlignVCenter, timeText);
    
    // Draw track title area
    if (!m_playlist.isEmpty() && m_currentTrack < m_playlist.size()) {
        QFileInfo fileInfo(m_playlist[m_currentTrack]);
        QString title = fileInfo.completeBaseName();
        painter->drawText(QRect(80, 28, 180, 18), Qt::AlignLeft | Qt::AlignVCenter, title);
    }
    
    // Draw visualization placeholder
    drawVisualization(painter);
    
    // Draw controls
    drawControls(painter);
    
    // Draw status info
    painter->setPen(QColor(0, 192, 0));
    QFont smallFont("Arial", 7);
    painter->setFont(smallFont);
    
    if (m_mediaPlayer && m_mediaPlayer->isPlaying()) {
        painter->drawText(QRect(200, 60, 60, 12), Qt::AlignLeft, "PLAYING");
    }
}

void MainWindowQt::drawTitleBar(QPainter *painter)
{
    // Title bar background
    QLinearGradient gradient(0, 0, 0, 14);
    gradient.setColorAt(0, QColor(82, 90, 115));
    gradient.setColorAt(1, QColor(41, 49, 66));
    painter->fillRect(0, 0, WINAMP_WIDTH, 14, gradient);
    
    // Winamp title
    painter->setPen(QColor(255, 255, 255));
    QFont titleFont("Arial", 8, QFont::Bold);
    painter->setFont(titleFont);
    painter->drawText(QRect(6, 2, 200, 12), Qt::AlignLeft | Qt::AlignVCenter, "Winamp Linux");
    
    // Window controls (minimize, shade, close)
    // X button
    QRect closeBtn(WINAMP_WIDTH - 11, 3, 9, 9);
    painter->fillRect(closeBtn, QColor(200, 50, 50));
    painter->setPen(QColor(255, 255, 255));
    painter->drawLine(closeBtn.left() + 2, closeBtn.top() + 2,
                     closeBtn.right() - 2, closeBtn.bottom() - 2);
    painter->drawLine(closeBtn.right() - 2, closeBtn.top() + 2,
                     closeBtn.left() + 2, closeBtn.bottom() - 2);
}

void MainWindowQt::drawVisualization(QPainter *painter)
{
    // Placeholder visualization (will implement spectrum analyzer later)
    QRect visRect(24, 43, 76, 16);
    painter->fillRect(visRect, QColor(0, 16, 0));
    
    // Draw some fake bars for now
    if (m_mediaPlayer && m_mediaPlayer->isPlaying()) {
        painter->setPen(QColor(0, 255, 0));
        for (int i = 0; i < 19; i++) {
            int height = (qrand() % 14) + 2;
            painter->drawLine(26 + i * 4, visRect.bottom() - height,
                            26 + i * 4, visRect.bottom());
        }
    }
}

void MainWindowQt::drawControls(QPainter *painter)
{
    // Control buttons area (bottom of window)
    QRect controlsRect(16, 88, 240, 24);
    
    // Previous button
    painter->fillRect(16, 88, 23, 18, QColor(60, 60, 80));
    painter->setPen(QColor(200, 200, 200));
    painter->drawRect(16, 88, 23, 18);
    painter->drawText(QRect(16, 88, 23, 18), Qt::AlignCenter, "|<");
    
    // Play button  
    painter->fillRect(39, 88, 23, 18, m_mediaPlayer->isPlaying() ? 
                     QColor(50, 120, 50) : QColor(60, 60, 80));
    painter->drawRect(39, 88, 23, 18);
    painter->drawText(QRect(39, 88, 23, 18), Qt::AlignCenter, "▶");
    
    // Pause button
    painter->fillRect(62, 88, 23, 18, QColor(60, 60, 80));
    painter->drawRect(62, 88, 23, 18);
    painter->drawText(QRect(62, 88, 23, 18), Qt::AlignCenter, "||");
    
    // Stop button
    painter->fillRect(85, 88, 23, 18, QColor(60, 60, 80));
    painter->drawRect(85, 88, 23, 18);
    painter->drawText(QRect(85, 88, 23, 18), Qt::AlignCenter, "■");
    
    // Next button
    painter->fillRect(108, 88, 23, 18, QColor(60, 60, 80));
    painter->drawRect(108, 88, 23, 18);
    painter->drawText(QRect(108, 88, 23, 18), Qt::AlignCenter, ">|");
    
    // Open file button
    painter->fillRect(136, 89, 22, 16, QColor(60, 60, 80));
    painter->drawRect(136, 89, 22, 16);
    painter->setFont(QFont("Arial", 7));
    painter->drawText(QRect(136, 89, 22, 16), Qt::AlignCenter, "DIR");
}

void MainWindowQt::onPlayClicked()
{
    play();
}

void MainWindowQt::onPauseClicked()
{
    pause();
}

void MainWindowQt::onStopClicked()
{
    stop();
}

void MainWindowQt::onPreviousClicked()
{
    previous();
}

void MainWindowQt::onNextClicked()
{
    next();
}

void MainWindowQt::play()
{
    if (m_mediaPlayer) {
        if (m_playlist.isEmpty()) {
            // Open file dialog
            QStringList files = QFileDialog::getOpenFileNames(
                this, "Open Media Files", QDir::homePath(),
                "Media Files (*.mp3 *.wav *.ogg *.flac *.m4a *.wma *.aac);;All Files (*)");
            
            if (!files.isEmpty()) {
                loadFiles(files);
            }
        }
        
        if (!m_playlist.isEmpty()) {
            if (m_mediaPlayer->playbackState() == QMediaPlayer::PausedState) {
                m_mediaPlayer->play();
            } else if (m_currentTrack < m_playlist.size()) {
                m_mediaPlayer->setSource(QUrl::fromLocalFile(m_playlist[m_currentTrack]));
                m_mediaPlayer->play();
            }
            emit playbackStarted();
        }
    }
}

void MainWindowQt::pause()
{
    if (m_mediaPlayer) {
        m_mediaPlayer->pause();
        emit playbackPaused();
    }
}

void MainWindowQt::stop()
{
    if (m_mediaPlayer) {
        m_mediaPlayer->stop();
        emit playbackStopped();
    }
}

void MainWindowQt::previous()
{
    if (m_currentTrack > 0) {
        m_currentTrack--;
        if (m_mediaPlayer && !m_playlist.isEmpty()) {
            m_mediaPlayer->setSource(QUrl::fromLocalFile(m_playlist[m_currentTrack]));
            m_mediaPlayer->play();
        }
    }
}

void MainWindowQt::next()
{
    if (m_currentTrack < m_playlist.size() - 1) {
        m_currentTrack++;
        if (m_mediaPlayer && !m_playlist.isEmpty()) {
            m_mediaPlayer->setSource(QUrl::fromLocalFile(m_playlist[m_currentTrack]));
            m_mediaPlayer->play();
        }
    } else if (m_repeatEnabled) {
        m_currentTrack = 0;
        if (m_mediaPlayer && !m_playlist.isEmpty()) {
            m_mediaPlayer->setSource(QUrl::fromLocalFile(m_playlist[m_currentTrack]));
            m_mediaPlayer->play();
        }
    }
}

void MainWindowQt::loadFiles(const QStringList &files)
{
    m_playlist = files;
    m_currentTrack = 0;
    update();
}

void MainWindowQt::enqueueFile(const QString &file)
{
    m_playlist.append(file);
    update();
}

void MainWindowQt::clearPlaylist()
{
    m_playlist.clear();
    m_currentTrack = 0;
    update();
}

void MainWindowQt::onPositionChanged(qint64 position)
{
    emit positionChanged(position);
    update(); // Redraw time display
}

void MainWindowQt::onDurationChanged(qint64 duration)
{
    emit durationChanged(duration);
}

void MainWindowQt::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        next(); // Auto-advance to next track
    }
}

void MainWindowQt::updateDisplay()
{
    // Update visualization and displays
    update();
}

QString MainWindowQt::formatTime(qint64 milliseconds)
{
    int seconds = milliseconds / 1000;
    int minutes = seconds / 60;
    seconds = seconds % 60;
    
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

void MainWindowQt::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
}

void MainWindowQt::closeEvent(QCloseEvent *event)
{
    if (m_mediaPlayer) {
        m_mediaPlayer->stop();
    }
    event->accept();
}

void MainWindowQt::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_Space:
            if (m_mediaPlayer->isPlaying()) {
                pause();
            } else {
                play();
            }
            break;
        case Qt::Key_S:
            stop();
            break;
        case Qt::Key_Z:
            previous();
            break;
        case Qt::Key_B:
            next();
            break;
        default:
            QMainWindow::keyPressEvent(event);
    }
}

void MainWindowQt::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindowQt::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QStringList files;
        for (const QUrl &url : mimeData->urls()) {
            if (url.isLocalFile()) {
                files.append(url.toLocalFile());
            }
        }
        if (!files.isEmpty()) {
            loadFiles(files);
            play();
        }
    }
}

void MainWindowQt::toggleWindowShade()
{
    m_windowShade = !m_windowShade;
    if (m_windowShade) {
        setFixedSize(WINAMP_WIDTH, WINAMP_SHADE_HEIGHT);
    } else {
        setFixedSize(WINAMP_WIDTH, WINAMP_HEIGHT);
    }
}

void MainWindowQt::toggleDoubleSizeMode()
{
    m_doubleSizeMode = !m_doubleSizeMode;
    if (m_doubleSizeMode) {
        setFixedSize(WINAMP_WIDTH * 2, WINAMP_HEIGHT * 2);
    } else {
        setFixedSize(WINAMP_WIDTH, WINAMP_HEIGHT);
    }
}

void MainWindowQt::toggleEqualizer()
{
    m_equalizerVisible = !m_equalizerVisible;
    // TODO: Show/hide equalizer window
}

void MainWindowQt::togglePlaylist()
{
    m_playlistVisible = !m_playlistVisible;
    // TODO: Show/hide playlist window
}

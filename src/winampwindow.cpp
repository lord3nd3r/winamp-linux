#include "winampwindow.h"
#include "playlistwindow.h"
#include "equalizerwindow.h"
#include "videowindow.h"
#include "milkdropwindow.h"
#include "medialibrarywindow.h"
#include "skinutils.h"
#include "winampbitmaps.h"
#include "translator.h"
#include "bookmarkmanager.h"
#include "recentfilesmanager.h"
#include "dialogs.h"
#include "eqpresets.h"

#ifdef QT_DBUS_LIB
#include "mpris2.h"
#endif

#include <QAudioBuffer>
#include <QAudioFormat>
#include <cmath>
#include <cstring>

// ============================================================================
// Constructor
// ============================================================================
WinampWindow::WinampWindow(QWidget *parent) : QWidget(parent), dragPosition(0,0), isDragging(false),
             volume(200), balance(0), hoveredButton(-1), pressedButton(-1),
             shuffleOn(false), repeatOn(false), eqBtnOn(false), plBtnOn(false),
             repeatTrack(false), stopAfterCurrent(false),
             isDraggingVolume(false), isDraggingBalance(false), isDraggingPos(false),
             scrollOffset(0), visMode(1), doubleSize(false), shadeMode(false),
             alwaysOnTop(false), clutterbarOpen(false) {
    setFixedSize(275, 116);
    setWindowTitle("Winamp 0.5 BETA for Linux");
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);  // Accept drag-drop on main window too

    // Initialize visualization state
    memset(saBarHeight, 0, sizeof(saBarHeight));
    memset(saPeakHeight, 0, sizeof(saPeakHeight));
    memset(saPeakVel, 0, sizeof(saPeakVel));
    memset(spectrumData, 0, sizeof(spectrumData));
    memset(oscData, 0, sizeof(oscData));
    memset(vuData, 0, sizeof(vuData));

    // Initialize easter egg state
    memset(eggStr, 0, sizeof(eggStr));
    eggStat = 0;

    // Setup audio — dual path:
    // 1) QAudioOutput for direct playback (used as fallback / when EQ is off)
    // 2) QAudioBufferOutput → EQ10 DSP → QAudioSink (when EQ is on)
    player = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    player->setAudioOutput(audioOutput);
    audioOutput->setVolume(volume / 255.0f);

    // Setup second player for gapless playback
    nextPlayer = new QMediaPlayer(this);
    nextAudioOutput = new QAudioOutput(this);
    nextPlayer->setAudioOutput(nextAudioOutput);
    nextAudioOutput->setVolume(volume / 255.0f);
    usingNextPlayer = false;

    // Initialize EQ DSP state
    memset(eqState, 0, sizeof(eqState));
    eqSampleRate = 0;
    eqChannels = 0;
    eqDspActive = false;

    // Setup audio buffer output for visualization + EQ DSP
    audioBufferOutput = new QAudioBufferOutput(this);
    player->setAudioBufferOutput(audioBufferOutput);
    connect(audioBufferOutput, &QAudioBufferOutput::audioBufferReceived,
            this, &WinampWindow::processAudioBuffer);

    // System tray icon (matches Windows SYSTRAY.cpp)
    setupSystemTray();

    // MPRIS2 D-Bus integration — Linux desktop media keys and remote control
    // (equivalent to Windows global hotkeys + WM_COMMAND remote control)
#ifdef QT_DBUS_LIB
    new Mpris2RootAdaptor(this);
    new Mpris2PlayerAdaptor(player, audioOutput, this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject("/org/mpris/MediaPlayer2", this);
    dbus.registerService("org.mpris.MediaPlayer2.winamp");
#endif

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

    // Auto-show video window when video content is detected
    connect(player, &QMediaPlayer::hasVideoChanged, this, [this](bool hasVideo) {
        if (hasVideo && videoWindow) {
            videoWindow->setHasVideo(true);
            videoWindow->show();
            videoWindow->raise();
        } else if (videoWindow) {
            videoWindow->setHasVideo(false);
            videoWindow->hide();
        }
    });

    // Auto-advance to next track when current one ends
    connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            // Stop after current track (like Windows g_stopaftercur)
            if (stopAfterCurrent) {
                stopAfterCurrent = false;
                player->stop();
                update();
                return;
            }

            // Repeat track (repeat one): replay same track
            if (repeatOn && repeatTrack) {
                player->setPosition(0);
                player->play();
                return;
            }

            // Gapless playback: if next track is preloaded, swap players
            if (nextPlayer->source().isValid() && !shuffleOn) {
                // Swap players for seamless transition
                std::swap(player, nextPlayer);
                std::swap(audioOutput, nextAudioOutput);

                // Update visualization to use the now-active player
                player->setAudioBufferOutput(audioBufferOutput);
                nextPlayer->setAudioBufferOutput(nullptr);

                // Start the preloaded track
                player->play();

                // Update currentFile
                currentFile = player->source().toLocalFile();

                // Update playlist index
                int curIdx = playlistWindow->currentTrackIndex();
                int count = playlistWindow->trackCount();
                int nextIdx = curIdx + 1;
                if (nextIdx < count) {
                    playlistWindow->setCurrentTrackIndex(nextIdx);
                } else if (repeatOn && count > 0) {
                    playlistWindow->setCurrentTrackIndex(0);
                }

                // Update tray and show notification
                QString fileName = currentFile;
                RecentFilesManager::instance().addFile(fileName);
                updateTrayTooltip();
                if (showSongNotifications && trayIcon) {
                    QString title = metaTitle.isEmpty() ? QFileInfo(fileName).completeBaseName() : metaTitle;
                    trayIcon->showMessage("Winamp", title, QSystemTrayIcon::Information, 3000);
                }

                // Preload the next track
                preloadNextTrack();
                return;
            }

            // Fallback to normal track advancing (for shuffle or when preload failed)
            int curIdx = playlistWindow->currentTrackIndex();
            int count = playlistWindow->trackCount();
            if (count > 0) {
                int nextIdx;
                if (shuffleOn) {
                    nextIdx = QRandomGenerator::global()->bounded(count);
                } else {
                    nextIdx = curIdx + 1;
                }
                if (nextIdx < count) {
                    playlistWindow->setCurrentTrackIndex(nextIdx);
                    playTrack(playlistWindow->trackAt(nextIdx));
                } else if (repeatOn) {
                    // Repeat all: wrap to beginning
                    playlistWindow->setCurrentTrackIndex(0);
                    playTrack(playlistWindow->trackAt(0));
                }
            }
        }
    });

    // Extract bitrate and song metadata when available
    connect(player, &QMediaPlayer::metaDataChanged, this, [this]() {
        QMediaMetaData md = player->metaData();
        QVariant br = md.value(QMediaMetaData::AudioBitRate);
        if (br.isValid()) {
            mediaBitrate = br.toInt() / 1000;  // bps -> kbps
        }
        // Extract title and artist for scrolling display
        QString title = md.value(QMediaMetaData::Title).toString();
        QString artist;
        QVariant artistVar = md.value(QMediaMetaData::ContributingArtist);
        if (artistVar.canConvert<QStringList>())
            artist = artistVar.toStringList().join(", ");
        else
            artist = artistVar.toString();

        QString newMetaTitle;
        if (!title.isEmpty()) {
            if (!artist.isEmpty())
                newMetaTitle = artist + " - " + title;
            else
                newMetaTitle = title;
        }

        // If metadata changed (for streams), show notification
        if (!newMetaTitle.isEmpty() && newMetaTitle != metaTitle) {
            metaTitle = newMetaTitle;
            if (showSongNotifications && trayIcon) {
                trayIcon->showMessage("Winamp", metaTitle, QSystemTrayIcon::Information, 3000);
            }
        } else if (!newMetaTitle.isEmpty()) {
            metaTitle = newMetaTitle;
        }

        // Update tray tooltip
        updateTrayTooltip();
    });

    // Create playlist and EQ windows
    playlistWindow = new PlaylistWindow(this);
    connect(playlistWindow, &PlaylistWindow::trackDoubleClicked, this, &WinampWindow::playTrack);
    eqWindow = new EqualizerWindow(this);

    // Create video window (hidden by default)
    videoWindow = new VideoWindow(this);
    videoWindow->hide();
    player->setVideoOutput(videoWindow->getVideoWidget());

    // Create media library window (hidden by default)
    mediaLibraryWindow = new MediaLibraryWindow(this);
    mediaLibraryWindow->hide();
    connect(mediaLibraryWindow, &MediaLibraryWindow::addToPlaylist, this, [this](const QString &path) {
        playlistWindow->addTrack(path);
    });
    connect(mediaLibraryWindow, &MediaLibraryWindow::addToPlaylistRecursive, this, [this](const QString &dirPath) {
        // Add all audio files in directory recursively
        QStringList queue;
        queue << dirPath;
        while (!queue.isEmpty()) {
            QString currentDir = queue.takeFirst();
            QDir dir(currentDir);

            // Add audio files
            QStringList filters;
            filters << "*.mp3" << "*.flac" << "*.ogg" << "*.wav" << "*.m4a"
                   << "*.aac" << "*.wma" << "*.opus" << "*.mp4" << "*.avi"
                   << "*.mkv" << "*.mov" << "*.webm";
            QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
            for (const QFileInfo &file : files) {
                playlistWindow->addTrack(file.absoluteFilePath());
            }

            // Add subdirectories to queue
            QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo &subdir : subdirs) {
                queue << subdir.absoluteFilePath();
            }
        }
    });

    // Hide EQ and playlist when video goes fullscreen (like Milkdrop)
    connect(videoWindow, &VideoWindow::fullscreenChanged, this, [this](bool fs) {
        if (fs) {
            // Entering fullscreen - hide main, EQ, and playlist
            hide();
            eqWindow->hide();
            playlistWindow->hide();
        } else {
            // Exiting fullscreen - restore visibility based on previous state
            show();
            if (eqBtnOn) eqWindow->show();
            if (plBtnOn) playlistWindow->show();
        }
    });

    // Position windows: EQ below main, playlist to the right of main
    playlistWindow->move(x() + width(), y());  // right of main
    eqWindow->move(x(), y() + height());

    // Load bookmarks and recent files
    BookmarkManager::instance().load();
    RecentFilesManager::instance().load();

    // Load saved settings (overrides defaults above)
    loadAllSettings();
}

// ============================================================================
// Destructor
// ============================================================================
WinampWindow::~WinampWindow() {
    delete playlistWindow;
    delete eqWindow;
    delete videoWindow;
}

// ============================================================================
// playFile / playUrl
// ============================================================================
void WinampWindow::playFile(const QString &file) {
    if (!file.isEmpty() && QFile::exists(file)) {
        currentFile = file;
        player->setSource(QUrl::fromLocalFile(file));
        player->play();
    }
}

void WinampWindow::playUrl(const QString &url) {
    if (!url.isEmpty()) {
        currentFile = url;
        player->setSource(QUrl(url));
        player->play();
        updateTrayTooltip();

        // Show notification for stream
        if (showSongNotifications && trayIcon) {
            QString title = metaTitle.isEmpty() ? url : metaTitle;
            trayIcon->showMessage("Winamp", title, QSystemTrayIcon::Information, 3000);
        }

        // Don't preload next track for streams
    }
}

// ============================================================================
// Public slots
// ============================================================================
void WinampWindow::onPlayFile() {
    QString file = QFileDialog::getOpenFileName(this, "Open File", QString(),
        "Audio Files (*.mp3 *.wav *.flac *.ogg *.m4a *.aac *.wma);;All Files (*)");
    if (!file.isEmpty()) {
        playFile(file);
        RecentFilesManager::instance().addFile(file);
    }
}

void WinampWindow::onPlayLocation() {
    PlayLocationDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString url = dialog.getUrl();
        if (!url.isEmpty()) {
            playUrl(url);
        }
    }
}

void WinampWindow::onToggleAlwaysOnTop(bool checked) {
    alwaysOnTop = checked;
    setWindowFlag(Qt::WindowStaysOnTopHint, checked);
    show(); // Re-show to apply the flag change
}

void WinampWindow::onToggleDoubleSize() {
    doubleSize = !doubleSize;
    if (doubleSize) {
        setFixedSize(550, 232);
    } else {
        setFixedSize(275, 116);
    }
    update();
}

void WinampWindow::onToggleShadeMode() {
    shadeMode = !shadeMode;
    if (shadeMode) {
        setFixedSize(275, 14);
    } else {
        if (doubleSize)
            setFixedSize(550, 232);
        else
            setFixedSize(275, 116);
    }
    update();
}

void WinampWindow::onShowAbout() {
    AboutDialog aboutDialog(WinampBitmaps::instance().basePath, this);
    aboutDialog.exec();
}

void WinampWindow::onJumpToFile() {
    JumpToFileDialog dialog(playlistWindow->allTracks(), this);
    if (dialog.exec() == QDialog::Accepted) {
        int idx = dialog.getSelectedIndex();
        if (idx >= 0 && idx < playlistWindow->trackCount()) {
            playlistWindow->setCurrentTrackIndex(idx);
            playTrack(playlistWindow->trackAt(idx));
        }
    }
}

void WinampWindow::onAddBookmark() {
    if (!currentFile.isEmpty()) {
        QString title = metaTitle.isEmpty() ? QFileInfo(currentFile).baseName() : metaTitle;
        BookmarkManager::instance().addBookmark(title, currentFile);
    }
}

void WinampWindow::onSkinChanged(const QString &skinPath) {
    // Check if this is a modern (XML-based) skin
    if (isModernSkinDir(skinPath)) {
        if (modernSkin.loadSkin(skinPath)) {
            isModernSkin = true;
            g_isModernSkin = true;
            g_modernSkin = &modernSkin;
            // Remove fixed size constraint, allow resizing
            setMinimumSize(0, 0);
            setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            setMinimumSize(354, 144);
            resize(354, 144);
            setMouseTracking(true);
            update();
            qDebug() << "Loaded modern skin:" << modernSkin.getSkinName();
        }
    } else {
        // Classic skin
        isModernSkin = false;
        g_isModernSkin = false;
        g_modernSkin = nullptr;
        WinampBitmaps::instance().loadAll(skinPath);
        g_plColors = parsePleditTxt(skinPath);

        QString appDir = QCoreApplication::applicationDirPath();
        QStringList fallbacks = {
            appDir + "/../skins/default",
            appDir + "/../../skins/default",
            QDir::homePath() + "/.winamp/skins/default",
            appDir + "/../Src/Winamp/resource",
            appDir + "/../../Src/Winamp/resource"
        };
        for (const QString &fb : fallbacks) {
            QDir d(fb);
            if (d.exists())
                WinampBitmaps::instance().loadMissing(d.absolutePath());
        }

        // Restore classic fixed size
        if (doubleSize)
            setFixedSize(550, 232);
        else if (shadeMode)
            setFixedSize(275, 14);
        else
            setFixedSize(275, 116);
    }

    // Force all windows to repaint
    update();
    playlistWindow->applyPlaylistColors();
    playlistWindow->update();
    if (g_isModernSkin) {
        // Modern EQ: match main window width (354), height=18+89+6=113
        eqWindow->setMinimumSize(0, 0);
        eqWindow->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        eqWindow->setFixedSize(354, 113);
        // Match playlist width to main window
        playlistWindow->setMinimumSize(275, 116);
        playlistWindow->resize(354, playlistWindow->height());
    } else {
        eqWindow->setFixedSize(275, 116);
    }
    eqWindow->update();

    QSettings s(configPath(), QSettings::IniFormat);
    s.setValue("skin", skinPath);
}

// ============================================================================
// Modern skin helpers
// ============================================================================
QRect WinampWindow::modernButtonRect(int idx) const {
    int W = width();
    int py = MODERN_TH; // player.main y offset

    switch (idx) {
        // Playback buttons (in player.main, group offset x=4, y=93)
        case MB_PREV:  return QRect(4, py + 93, 30, 26);
        case MB_PLAY:  return QRect(34, py + 93, 30, 29);
        case MB_PAUSE: return QRect(64, py + 93, 30, 29);
        case MB_STOP:  return QRect(94, py + 93, 30, 29);
        case MB_NEXT:  return QRect(124, py + 93, 30, 26);
        // Right-side buttons (relatx=1)
        case MB_EJECT: return QRect(W - 86, py + 75, 19, 13);
        case MB_PL:    return QRect(W - 60, py + 75, 22, 13);
        case MB_ML:    return QRect(W - 34, py + 75, 22, 13);
        case MB_MUTE:  return QRect(164, py + 104, 15, 15);
        case MB_REPEAT:  return QRect(W - 40, py + 22, 20, 15);
        case MB_SHUFFLE: return QRect(W - 40, py + 45, 20, 15);
        // Titlebar buttons (no py offset)
        case MB_MINIMIZE: return QRect(W - 41, 4, 11, 10);
        case MB_CLOSE:    return QRect(W - 17, 4, 11, 10);
        default: return QRect();
    }
}

int WinampWindow::modernGetButtonAt(int x, int y) const {
    for (int i = 0; i < MB_COUNT; i++) {
        if (modernButtonRect(i).contains(x, y))
            return i;
    }
    return -1;
}

QRect WinampWindow::modernSeekRect() const {
    int W = width();
    int py = MODERN_TH;
    return QRect(6, py + 75, W - 106, 13);
}

QRect WinampWindow::modernVolumeRect() const {
    int py = MODERN_TH;
    return QRect(183, py + 110, 86, 13);
}

// ============================================================================
// paintModern — Modern (XML-based) skin renderer
// ============================================================================
void WinampWindow::paintModern(QPainter &p) {
    int W = width();
    int H = height();

    // ---- Fill background with base texture ----
    QPixmap baseTex = modernSkin.getBitmap("wasabi.frame.basetexture");
    if (!baseTex.isNull()) {
        for (int ty = 0; ty < H; ty += baseTex.height())
            for (int tx = 0; tx < W; tx += baseTex.width())
                p.drawPixmap(tx, ty, baseTex);
    } else {
        p.fillRect(0, 0, W, H, QColor(43, 45, 61));
    }

    // ---- Titlebar (18px) ----
    QPixmap tbLeft = modernSkin.getBitmap("wasabi.frame.top.left");
    QPixmap tbCenter = modernSkin.getBitmap("wasabi.frame.top");
    QPixmap tbRight = modernSkin.getBitmap("wasabi.frame.top.right");

    if (!tbLeft.isNull()) p.drawPixmap(0, 0, tbLeft);
    if (!tbCenter.isNull()) {
        for (int tx = 10; tx < W - 10; tx += tbCenter.width()) {
            int tw = qMin(tbCenter.width(), W - 10 - tx);
            p.drawPixmap(tx, 0, tbCenter, 0, 0, tw, MODERN_TH);
        }
    }
    if (!tbRight.isNull()) p.drawPixmap(W - 10, 0, tbRight);

    // Titlebar active/inactive text background
    QPixmap tbTextLeft = modernSkin.getBitmap(isActiveWindow() ?
        "wasabi.titlebar.left.active" : "wasabi.titlebar.left.inactive");
    QPixmap tbTextCenter = modernSkin.getBitmap(isActiveWindow() ?
        "wasabi.titlebar.center.active" : "wasabi.titlebar.center.inactive");
    QPixmap tbTextRight = modernSkin.getBitmap(isActiveWindow() ?
        "wasabi.titlebar.right.active" : "wasabi.titlebar.right.inactive");

    if (!tbTextLeft.isNull()) p.drawPixmap(10, 5, tbTextLeft);
    if (!tbTextCenter.isNull()) {
        for (int tx = 20; tx < W - 55; tx += tbTextCenter.width()) {
            int tw = qMin(tbTextCenter.width(), W - 55 - tx);
            p.drawPixmap(tx, 5, tbTextCenter, 0, 0, tw, tbTextCenter.height());
        }
    }
    if (!tbTextRight.isNull()) p.drawPixmap(W - 55, 5, tbTextRight);

    // Title text "WINAMP"
    p.setPen(QColor(200, 200, 220));
    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.drawText(15, 14, "WINAMP");

    // Titlebar buttons
    auto drawModernBtn = [&](int idx, const QString &normalId, const QString &hId, const QString &dId) {
        QRect r = modernButtonRect(idx);
        QString id = normalId;
        if (modernPressed == idx) id = dId;
        else if (modernHovered == idx) id = hId;
        QPixmap px = modernSkin.getBitmap(id);
        if (!px.isNull()) p.drawPixmap(r.x(), r.y(), px);
    };

    // Button backgrounds behind titlebar buttons
    QPixmap btnBgTitle = modernSkin.getBitmap("wasabi.button.bg.title");
    if (!btnBgTitle.isNull()) {
        p.drawPixmap(W - 42, 4, btnBgTitle);
        p.drawPixmap(W - 30, 4, btnBgTitle);
        p.drawPixmap(W - 18, 4, btnBgTitle);
    }

    drawModernBtn(MB_MINIMIZE, "wasabi.button.minimize", "wasabi.button.minimize.hover", "wasabi.button.minimize.pressed");
    drawModernBtn(MB_CLOSE, "wasabi.button.exit", "wasabi.button.exit.hover", "wasabi.button.exit.pressed");

    // ---- Player Main Area (y = MODERN_TH, h = 126) ----
    int py = MODERN_TH;

    // Background layers: left (180px) + center (tiled) + right (90px, right-aligned)
    QPixmap bgLeft = modernSkin.getBitmap("player.main.left");
    QPixmap bgCenter = modernSkin.getBitmap("player.main.center");
    QPixmap bgRight = modernSkin.getBitmap("player.main.right");

    if (!bgLeft.isNull()) p.drawPixmap(0, py, bgLeft);
    if (!bgCenter.isNull()) {
        int fillW = W - 180 - 90;
        for (int tx = 180; tx < 180 + fillW; tx += bgCenter.width()) {
            int tw = qMin(bgCenter.width(), 180 + fillW - tx);
            p.drawPixmap(tx, py, bgCenter, 0, 0, tw, bgCenter.height());
        }
    }
    if (!bgRight.isNull()) p.drawPixmap(W - 90, py, bgRight);

    // BG2 secondary layer (y=95 within player.main)
    QPixmap bg2Left = modernSkin.getBitmap("player.main.bg2.left");
    QPixmap bg2Center = modernSkin.getBitmap("player.main.bg2.center");
    QPixmap bg2Right = modernSkin.getBitmap("player.main.bg2.right");

    if (!bg2Left.isNull()) p.drawPixmap(138, py + 95, bg2Left);
    if (!bg2Center.isNull()) {
        int bg2FillW = W - 288;
        for (int tx = 198; tx < 198 + bg2FillW; tx += bg2Center.width()) {
            int tw = qMin(bg2Center.width(), 198 + bg2FillW - tx);
            p.drawPixmap(tx, py + 95, bg2Center, 0, 0, tw, bg2Center.height());
        }
    }
    if (!bg2Right.isNull()) p.drawPixmap(W - 90, py + 95, bg2Right);

    // ---- Display area (x=5, y=3 within player.main) ----
    int dx = 5, dy = py + 3;
    int dw = W - 49; // display width

    // Display background
    QPixmap dBgLeft = modernSkin.getBitmap("player.display.bg.left");
    QPixmap dBgCenter = modernSkin.getBitmap("player.display.bg.center");
    QPixmap dBgRight = modernSkin.getBitmap("player.display.bg.right");

    if (!dBgLeft.isNull()) p.drawPixmap(dx, dy, dBgLeft);
    if (!dBgCenter.isNull()) {
        for (int tx = dx + 60; tx < dx + dw - 60; tx += dBgCenter.width()) {
            int tw = qMin(dBgCenter.width(), dx + dw - 60 - tx);
            p.drawPixmap(tx, dy, dBgCenter, 0, 0, tw, dBgCenter.height());
        }
    }
    if (!dBgRight.isNull()) p.drawPixmap(dx + dw - 60, dy, dBgRight);

    // Display overlay (translucent effect)
    QPixmap dLeft = modernSkin.getBitmap("player.display.left");
    QPixmap dCenter = modernSkin.getBitmap("player.display.center");
    QPixmap dRight = modernSkin.getBitmap("player.display.right");

    if (!dLeft.isNull()) { p.setOpacity(0.05); p.drawPixmap(dx, dy, dLeft); p.setOpacity(1.0); }
    if (!dCenter.isNull()) {
        p.setOpacity(0.05);
        for (int tx = dx + 60; tx < dx + dw - 60; tx += dCenter.width()) {
            int tw = qMin(dCenter.width(), dx + dw - 60 - tx);
            p.drawPixmap(tx, dy, dCenter, 0, 0, tw, dCenter.height());
        }
        p.setOpacity(1.0);
    }
    if (!dRight.isNull()) { p.setOpacity(0.05); p.drawPixmap(dx + dw - 60, dy, dRight); p.setOpacity(1.0); }

    // Songticker background (y=44 within display, 19px tall)
    QPixmap stBgLeft = modernSkin.getBitmap("player.display.songticker.bg.left");
    QPixmap stBgCenter = modernSkin.getBitmap("player.display.songticker.bg.center");
    QPixmap stBgRight = modernSkin.getBitmap("player.display.songticker.bg.right");

    if (!stBgLeft.isNull()) p.drawPixmap(dx, dy + 44, stBgLeft);
    if (!stBgCenter.isNull()) {
        for (int tx = dx + 60; tx < dx + dw - 60; tx += stBgCenter.width()) {
            int tw = qMin(stBgCenter.width(), dx + dw - 60 - tx);
            p.drawPixmap(tx, dy + 44, stBgCenter, 0, 0, tw, stBgCenter.height());
        }
    }
    if (!stBgRight.isNull()) p.drawPixmap(dx + dw - 60, dy + 44, stBgRight);

    // Display left/right overlays (gradient edges)
    QPixmap dLeftOverlay = modernSkin.getBitmap("player.display.left.overlay");
    QPixmap dRightOverlay = modernSkin.getBitmap("player.display.right.overlay");
    if (!dLeftOverlay.isNull()) p.drawPixmap(dx, dy, dLeftOverlay);
    if (!dRightOverlay.isNull()) p.drawPixmap(dx + dw - 21, dy, dRightOverlay);

    // ---- Timer (x=20, y=16 within display) ----
    qint64 displayMs;
    if (showRemainingTime && player->duration() > 0)
        displayMs = player->duration() - player->position();
    else
        displayMs = player->position();

    int totalSec = displayMs / 1000;
    int mins = totalSec / 60;
    int secs = totalSec % 60;

    // Format: "MM:SS" or "-MM:SS"
    QString timeStr;
    if (showRemainingTime && player->duration() > 0)
        timeStr = QString("-%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
    else
        timeStr = QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));

    modernSkin.drawBitmapText(p, "player.BIGNUM", timeStr, dx + 20, dy + 16, 78);

    // ---- Playback status (x=11, y=15 within display) ----
    QString statusBmp;
    if (player->playbackState() == QMediaPlayer::PlayingState)
        statusBmp = "player.status.play";
    else if (player->playbackState() == QMediaPlayer::PausedState)
        statusBmp = "player.status.pause";
    else
        statusBmp = "player.status.stop";
    QPixmap statusPx = modernSkin.getBitmap(statusBmp);
    if (!statusPx.isNull()) p.drawPixmap(dx + 11, dy + 15, statusPx);

    // ---- Visualization (x = dw-94, y=12 within display) ----
    QPixmap visBg = modernSkin.getBitmap("player.visualization.background");
    if (!visBg.isNull()) p.drawPixmap(dx + dw - 94, dy + 12, visBg);

    // Draw simple spectrum analyzer visualization
    if (visMode == 1) {
        int vx = dx + dw - 88, vy = dy + 13;
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 19 && i < 75; i++) {
            int barH = (int)(spectrumData[i] * 25);
            barH = qBound(0, barH, 25);
            for (int j = 0; j < barH; j++) {
                int intensity = 255 - j * 8;
                p.fillRect(vx + i * 4, vy + 25 - j, 3, 1, QColor(intensity, intensity, 255));
            }
        }
    } else if (visMode == 2) {
        // Oscilloscope
        int vx = dx + dw - 88, vy = dy + 25;
        p.setPen(QColor(200, 200, 255));
        for (int i = 0; i < 71; i++) {
            int y1 = vy + (int)(oscData[i] * 12);
            int y2 = vy + (int)(oscData[i + 1] * 12);
            p.drawLine(vx + i, y1, vx + i + 1, y2);
        }
    }

    QPixmap visOverlay = modernSkin.getBitmap("player.visualization.overlay");
    if (!visOverlay.isNull()) p.drawPixmap(dx + dw - 99, dy + 12, visOverlay);

    // ---- Songticker text (x=8, y=43 within display) ----
    QString songTitle;
    if (!metaTitle.isEmpty())
        songTitle = metaTitle;
    else if (!currentFile.isEmpty())
        songTitle = QFileInfo(currentFile).completeBaseName();

    if (!songTitle.isEmpty()) {
        // Scrolling text using songticker font
        int stX = dx + 8, stY = dy + 43;
        int stW = dw - 16;
        p.setClipRect(stX, stY, stW, 22);
        int textW = modernSkin.measureText("player.songticker.font", songTitle);
        if (textW > stW) {
            // Scroll
            QString scrollStr = songTitle + "     " + songTitle;
            int offset = scrollOffset % (textW + modernSkin.measureText("player.songticker.font", "     "));
            modernSkin.drawBitmapText(p, "player.songticker.font", scrollStr, stX - offset, stY + 1);
        } else {
            // Center
            modernSkin.drawBitmapText(p, "player.songticker.font", songTitle, stX + (stW - textW) / 2, stY + 1);
        }
        p.setClipping(false);
    }

    // ---- Song info (x=96, y=17 within display) ----
    QPixmap kbps = modernSkin.getBitmap("player.songinfo.kbps");
    QPixmap khz = modernSkin.getBitmap("player.songinfo.khz");
    if (!kbps.isNull()) p.drawPixmap(dx + 96 + 7, dy + 17, kbps);
    if (!khz.isNull()) p.drawPixmap(dx + 96 + 56, dy + 17, khz);

    // Bitrate/frequency text
    if (mediaBitrate > 0)
        modernSkin.drawBitmapText(p, "player.songinfo.font", QString::number(mediaBitrate), dx + 96 + 27, dy + 17, 30);
    if (mediaSampleRate > 0)
        modernSkin.drawBitmapText(p, "player.songinfo.font", QString::number(mediaSampleRate), dx + 96 + 78, dy + 17, 30);

    // Mono/stereo indicator
    QString chBmp = "player.songinfo.none";
    if (mediaChannels >= 2) chBmp = "player.songinfo.stereo";
    else if (mediaChannels == 1) chBmp = "player.songinfo.mono";
    QPixmap chPx = modernSkin.getBitmap(chBmp);
    if (!chPx.isNull()) p.drawPixmap(dx + 96 + 7, dy + 27, chPx);

    // EQ on indicator
    QPixmap eqInd = modernSkin.getBitmap(eqBtnOn ? "player.songinfo.eq.on" : "player.songinfo.eq.off");
    if (!eqInd.isNull()) p.drawPixmap(dx + 96 + 101, dy + 17, eqInd);

    // ---- Seek bar (x=6, y=75 within player.main) ----
    QPixmap seekLeft = modernSkin.getBitmap("player.seekbar.left");
    QPixmap seekCenter = modernSkin.getBitmap("player.seekbar.center");
    QPixmap seekRight = modernSkin.getBitmap("player.seekbar.right");

    QRect seekRect = modernSeekRect();
    if (!seekLeft.isNull()) p.drawPixmap(seekRect.x(), seekRect.y(), seekLeft);
    if (!seekCenter.isNull()) {
        for (int tx = seekRect.x() + 10; tx < seekRect.right() - 10; tx += seekCenter.width()) {
            int tw = qMin(seekCenter.width(), seekRect.right() - 10 - tx);
            p.drawPixmap(tx, seekRect.y(), seekCenter, 0, 0, tw, seekCenter.height());
        }
    }
    if (!seekRight.isNull()) p.drawPixmap(seekRect.right() - 10, seekRect.y(), seekRight);

    // Seek thumb position
    if (player->duration() > 0) {
        double seekFrac = (double)player->position() / player->duration();
        QPixmap seekThumb = modernSkin.getBitmap(modernDraggingSeek ? "player.button.seek.pressed" :
            (modernSeekRect().contains(mapFromGlobal(QCursor::pos())) ? "player.button.seek.hover" : "player.button.seek"));
        if (!seekThumb.isNull()) {
            int thumbX = seekRect.x() + (int)((seekRect.width() - seekThumb.width()) * seekFrac);
            p.drawPixmap(thumbX, seekRect.y(), seekThumb);
        }
    }

    // ---- Playback buttons ----
    // Button backgrounds
    auto drawBg = [&](const QString &id, int bx, int by) {
        QPixmap px = modernSkin.getBitmap(id);
        if (!px.isNull()) p.drawPixmap(bx, by + py, px);
    };
    drawBg("player.button.previous.bg", 4, 93);
    drawBg("player.button.play.bg", 34, 93);
    drawBg("player.button.pause.bg", 64, 93);
    drawBg("player.button.stop.bg", 94, 93);
    drawBg("player.button.next.bg", 124, 93);

    // Playback status overlay on buttons
    QString btnStatusBmp;
    if (player->playbackState() == QMediaPlayer::PlayingState)
        btnStatusBmp = "player.button.status.play";
    else if (player->playbackState() == QMediaPlayer::PausedState)
        btnStatusBmp = "player.button.status.pause";
    else
        btnStatusBmp = "player.button.status.stop";
    QPixmap btnStatusPx = modernSkin.getBitmap(btnStatusBmp);
    if (!btnStatusPx.isNull()) p.drawPixmap(34, py + 93, btnStatusPx);

    // Button images with hover/pressed states
    drawModernBtn(MB_PREV, "player.button.previous", "player.button.previous.hover", "player.button.previous.pressed");
    drawModernBtn(MB_PLAY, "player.button.play", "player.button.play.hover", "player.button.play.pressed");
    drawModernBtn(MB_PAUSE, "player.button.pause", "player.button.pause.hover", "player.button.pause.pressed");
    drawModernBtn(MB_STOP, "player.button.stop", "player.button.stop.hover", "player.button.stop.pressed");
    drawModernBtn(MB_NEXT, "player.button.next", "player.button.next.hover", "player.button.next.pressed");

    // ---- Volume area ----
    QPixmap volBg = modernSkin.getBitmap("player.volume.bg");
    if (!volBg.isNull()) p.drawPixmap(183, py + 100, volBg);

    // Volume bar fill
    QPixmap volBar = modernSkin.getBitmap("player.volumebar");
    if (!volBar.isNull()) {
        int fillW = (int)(10.0 * volume / 255.0);
        if (fillW > 0) p.drawPixmap(185, py + 115, volBar, 0, 0, fillW, volBar.height());
    }

    // Volume slider thumb
    QRect volRect = modernVolumeRect();
    double volFrac = volume / 255.0;
    QPixmap volThumb = modernSkin.getBitmap(modernDraggingVolume ? "player.button.volume.pressed" :
        "player.button.volume");
    if (!volThumb.isNull()) {
        int thumbX = volRect.x() + (int)((volRect.width() - volThumb.width()) * volFrac);
        p.drawPixmap(thumbX, volRect.y(), volThumb);
    }

    // ---- Mute button ----
    QPixmap muteBg = modernSkin.getBitmap("player.button.mute.bg");
    if (!muteBg.isNull()) p.drawPixmap(160, py + 99, muteBg);
    // (Mute toggle drawn with drawModernBtn)
    bool isMuted = (audioOutput->volume() < 0.01f && volume > 0);
    QString muteId = isMuted ? "player.button.mute.on" : "player.button.mute.off";
    if (modernPressed == MB_MUTE) muteId = isMuted ? "player.button.mute.on.pressed" : "player.button.mute.off.pressed";
    QPixmap mutePx = modernSkin.getBitmap(muteId);
    if (!mutePx.isNull()) p.drawPixmap(164, py + 104, mutePx);

    // ---- MLPL buttons area ----
    QPixmap mlplBg = modernSkin.getBitmap("player.button.mlpl.bg");
    if (!mlplBg.isNull()) p.drawPixmap(W - 94, py + 69, mlplBg);

    drawModernBtn(MB_EJECT, "player.button.eject", "player.button.eject.hover", "player.button.eject.pressed");

    // PL button (active state when playlist visible)
    {
        QString plId = plBtnOn ? "player.button.pl.active" : "player.button.pl";
        if (modernPressed == MB_PL) plId = "player.button.pl.pressed";
        else if (modernHovered == MB_PL) plId = "player.button.pl.hover";
        QPixmap px = modernSkin.getBitmap(plId);
        QRect r = modernButtonRect(MB_PL);
        if (!px.isNull()) p.drawPixmap(r.x(), r.y(), px);
    }
    // ML button (active state when ML visible)
    {
        QString mlId = (mediaLibraryWindow && mediaLibraryWindow->isVisible()) ? "player.button.ml.active" : "player.button.ml";
        if (modernPressed == MB_ML) mlId = "player.button.ml.pressed";
        else if (modernHovered == MB_ML) mlId = "player.button.ml.hover";
        QPixmap px = modernSkin.getBitmap(mlId);
        QRect r = modernButtonRect(MB_ML);
        if (!px.isNull()) p.drawPixmap(r.x(), r.y(), px);
    }

    // ---- Shuffle/Repeat with LED indicators ----
    QPixmap repBg = modernSkin.getBitmap("player.button.repeat.bg");
    QPixmap shufBg = modernSkin.getBitmap("player.button.shuffle.bg");
    if (!repBg.isNull()) p.drawPixmap(W - 44, py + 18, repBg);
    if (!shufBg.isNull()) p.drawPixmap(W - 44, py + 41, shufBg);

    drawModernBtn(MB_REPEAT, "player.button.repeat", "player.button.repeat.hover", "player.button.repeat.pressed");
    drawModernBtn(MB_SHUFFLE, "player.button.shuffle", "player.button.shuffle.hover", "player.button.shuffle.pressed");

    // LED indicators
    QPixmap ledOn = modernSkin.getBitmap("player.led.on");
    QPixmap ledOff = modernSkin.getBitmap("player.led.off");
    if (!ledOn.isNull() && !ledOff.isNull()) {
        p.drawPixmap(W - 19, py + 22, repeatOn ? ledOn : ledOff);
        p.drawPixmap(W - 19, py + 45, shuffleOn ? ledOn : ledOff);
    }

    // ---- Bolt icon (about button) ----
    QPixmap boltBg = modernSkin.getBitmap("player.button.bolt.bg");
    QPixmap bolt = modernSkin.getBitmap("player.button.bolt");
    if (!boltBg.isNull()) p.drawPixmap(W - 31, py + 101, boltBg);
    if (!bolt.isNull()) p.drawPixmap(W - 31, py + 101, bolt);

    // ---- Resizer ----
    QPixmap resizer = modernSkin.getBitmap("player.resizer");
    if (!resizer.isNull()) p.drawPixmap(W - 17, py + 108, resizer);
}

// ============================================================================
// processAudioBuffer — visualization + EQ DSP processing
// ============================================================================
void WinampWindow::processAudioBuffer(const QAudioBuffer &buffer) {
    const QAudioFormat fmt = buffer.format();
    int sampleCount = buffer.frameCount();
    int channels = fmt.channelCount();

    // Update media info from audio format
    mediaChannels = channels;
    int sr = fmt.sampleRate();
    if (sr > 0) mediaSampleRate = sr / 1000; // e.g. 44100 -> 44

    // ---- VISUALIZATION (always runs, regardless of EQ) ----
    auto extractData = [&](auto *data) {
        float scale = 1.0f;
        if constexpr (std::is_same_v<std::remove_const_t<decltype(*data)>, qint16>)
            scale = 1.0f / 32768.0f;

        for (int i = 0; i < 75 && i < sampleCount; i++)
            oscData[i] = data[i * channels] * scale;

        float fftInput[512];
        memset(fftInput, 0, sizeof(fftInput));
        int n = qMin(sampleCount, 512);
        for (int i = 0; i < n; i++)
            fftInput[i] = data[i * channels] * scale;

        float magnitudes[256];
        fft512(fftInput, magnitudes);

        for (int i = 0; i < 19; i++) {
            int startBin = i * 8 + 1;
            int endBin = qMin(startBin + 8, 256);
            float maxVal = 0;
            for (int j = startBin; j < endBin; j++)
                if (magnitudes[j] > maxVal) maxVal = magnitudes[j];
            float db = 0;
            if (maxVal > 0.001f) {
                db = log10f(1.0f + maxVal * 5.0f) / log10f(1.0f + 5.0f * 50.0f);
            }
            spectrumData[i] = qBound(0.0f, db, 1.0f);
        }
    };

    // Extract visualization data + VU meter
    if (fmt.sampleFormat() == QAudioFormat::Int16) {
        extractData(buffer.constData<qint16>());
        const qint16 *data = buffer.constData<qint16>();
        float lSum = 0, rSum = 0;
        int n = qMin(sampleCount, 512);
        for (int i = 0; i < n; i++) {
            float l = data[i * channels] / 32768.0f;
            float r = (channels > 1) ? data[i * channels + 1] / 32768.0f : l;
            lSum += l * l;
            rSum += r * r;
        }
        vuData[0] = sqrtf(lSum / n) * 3.0f;
        vuData[1] = sqrtf(rSum / n) * 3.0f;
        if (milkdropWindow && milkdropWindow->isVisible())
            milkdropWindow->feedPCMInt16(buffer.constData<qint16>(), sampleCount, channels);
    } else if (fmt.sampleFormat() == QAudioFormat::Float) {
        extractData(buffer.constData<float>());
        const float *data = buffer.constData<float>();
        float lSum = 0, rSum = 0;
        int n = qMin(sampleCount, 512);
        for (int i = 0; i < n; i++) {
            float l = data[i * channels];
            float r = (channels > 1) ? data[i * channels + 1] : l;
            lSum += l * l;
            rSum += r * r;
        }
        vuData[0] = sqrtf(lSum / n) * 3.0f;
        vuData[1] = sqrtf(rSum / n) * 3.0f;
        if (milkdropWindow && milkdropWindow->isVisible())
            milkdropWindow->feedPCMFloat(buffer.constData<float>(), sampleCount, channels);
    }

    // ---- EQ DSP PROCESSING (matches Windows eq10dsp.cpp + In.cpp) ----
    // When EQ is enabled: mute QAudioOutput, process through EQ10, output via QAudioSink
    bool eqEnabled = eqWindow && eqWindow->isEnabled();

    if (eqEnabled) {
        int sampleRate = fmt.sampleRate();

        // Mute the direct QAudioOutput path — we'll output processed audio via QAudioSink
        if (!eqDspActive) {
            audioOutput->setVolume(0.0f);
            eqDspActive = true;
        }

        // Setup/reconfigure QAudioSink if format changed
        if (sampleRate != eqSampleRate || channels != eqChannels) {
            eqSampleRate = sampleRate;
            eqChannels = qMin(channels, 2); // stereo max for EQ

            // Re-initialize EQ filter state for new sample rate
            eq10_setup(eqState, eqChannels, (double)sampleRate);

            // Update EQ gains from current slider positions
            for (int b = 0; b < 10; b++) {
                int sliderVal = eqWindow->getBandValue(b);
                double dB = eq10_valtodb(sliderVal);
                eq10_setgain(eqState, eqChannels, b, dB);
            }

            // Create QAudioSink with matching format
            if (audioSink) {
                audioSink->stop();
                delete audioSink;
            }
            QAudioFormat outFmt;
            outFmt.setSampleRate(sampleRate);
            outFmt.setChannelCount(eqChannels);
            outFmt.setSampleFormat(QAudioFormat::Float);

            audioSink = new QAudioSink(QMediaDevices::defaultAudioOutput(), outFmt, this);
            audioSink->setBufferSize(sampleRate * eqChannels * sizeof(float) / 5); // ~200ms buffer
            audioSinkDevice = audioSink->start();
        }

        if (!audioSinkDevice) return;

        // Update EQ gains every buffer (cheap, ensures sliders are responsive)
        for (int b = 0; b < 10; b++) {
            int sliderVal = eqWindow->getBandValue(b);
            double dB = eq10_valtodb(sliderVal);
            eq10_setgain(eqState, eqChannels, b, dB);
        }

        // Get preamp value — uses the original Winamp lookup table
        int preampSlider = eqWindow->getPreampValue();
        float preampGain = eq_preamp_table[qBound(0, preampSlider, 63)];

        // Volume and balance (applied post-EQ, matching Windows output chain)
        float vol = volume / 255.0f;
        float balL = 1.0f, balR = 1.0f;
        if (balance < 0) balR = (127.0f + balance) / 127.0f; // left-biased
        if (balance > 0) balL = (127.0f - balance) / 127.0f; // right-biased

        // Allocate float working buffer
        int totalSamples = sampleCount * eqChannels;
        QVector<float> floatBuf(totalSamples);
        QVector<float> outBuf(totalSamples);

        // Convert input to float with preamp applied (matches In.cpp FillFloat)
        if (fmt.sampleFormat() == QAudioFormat::Int16) {
            const qint16 *src = buffer.constData<qint16>();
            for (int i = 0; i < sampleCount; i++) {
                for (int ch = 0; ch < eqChannels; ch++) {
                    floatBuf[i * eqChannels + ch] = (src[i * channels + ch] / 32768.0f) * preampGain;
                }
            }
        } else if (fmt.sampleFormat() == QAudioFormat::Float) {
            const float *src = buffer.constData<float>();
            for (int i = 0; i < sampleCount; i++) {
                for (int ch = 0; ch < eqChannels; ch++) {
                    floatBuf[i * eqChannels + ch] = src[i * channels + ch] * preampGain;
                }
            }
        } else {
            return; // unsupported format
        }

        // Process through EQ10 for each channel (matches Windows inner loop)
        for (int ch = 0; ch < eqChannels; ch++) {
            eq10_processf(&eqState[ch], floatBuf.data(), outBuf.data(),
                          sampleCount, ch, eqChannels);
        }

        // Apply volume and balance post-EQ
        for (int i = 0; i < sampleCount; i++) {
            if (eqChannels >= 2) {
                outBuf[i * eqChannels + 0] *= vol * balL;
                outBuf[i * eqChannels + 1] *= vol * balR;
            } else {
                outBuf[i * eqChannels + 0] *= vol;
            }
        }

        // Write to QAudioSink
        if (audioSinkDevice) {
            qint64 bytes = totalSamples * sizeof(float);
            audioSinkDevice->write(reinterpret_cast<const char*>(outBuf.data()), bytes);
        }
    } else {
        // EQ is off — restore direct QAudioOutput path
        if (eqDspActive) {
            audioOutput->setVolume(volume / 255.0f);
            eqDspActive = false;
            // Stop the DSP sink
            if (audioSink) {
                audioSink->stop();
                delete audioSink;
                audioSink = nullptr;
                audioSinkDevice = nullptr;
            }
            eqSampleRate = 0;
            eqChannels = 0;
        }
    }
}

// ============================================================================
// paintEvent — Classic skin renderer
// ============================================================================
void WinampWindow::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // ---- Modern skin mode: use XML-based renderer ----
    if (isModernSkin) {
        paintModern(p);
        return;
    }

    auto &bmp = WinampBitmaps::instance();

    // ---- Shade mode: compact 275x14 titlebar-only view ----
    if (shadeMode) {
        // Draw shade background from titlebar.bmp (shade bar)
        if (!bmp.titlebar.isNull()) {
            // Shade titlebar: active at line 29, inactive at line 42, each 275x14
            int shadeY = isActiveWindow() ? 29 : 42;
            p.drawPixmap(0, 0, bmp.titlebar, 27, shadeY, 275, 14);
        } else {
            p.fillRect(0, 0, 275, 14, QColor(66, 66, 99));
        }
        // Draw scrolling title text in shade mode
        QString title = metaTitle.isEmpty() ? QFileInfo(currentFile).completeBaseName() : metaTitle;
        if (!title.isEmpty()) {
            p.setPen(QColor(0, 255, 0));
            p.setFont(QFont("Small Fonts", 7));
            p.setClipRect(30, 2, 200, 11);
            p.drawText(30 - (scrollOffset % (title.length() * 6 + 200)), 10, title + "  ***  " + title);
            p.setClipping(false);
        }
        return;
    }

    // ---- Double-size: scale everything 2x ----
    if (doubleSize) {
        p.scale(2.0, 2.0);
    }

    // Main background
    if (!bmp.main.isNull())
        p.drawPixmap(0, 0, bmp.main);
    else {
        p.fillRect(rect(), QColor(66, 66, 99));
        p.setPen(QColor(0, 255, 0));
        p.setFont(QFont("Tahoma", 7, QFont::Bold));
        p.drawText(10, 14, "Winamp 0.5 BETA for Linux");
        return;
    }

    // Titlebar: starts at x=27 in titlebar.bmp, active at y=0, inactive at y=15, 275x14
    if (!bmp.titlebar.isNull()) {
        int tbY = isActiveWindow() ? 0 : 15;
        p.drawPixmap(0, 0, bmp.titlebar, 27, tbY, 275, 14);
    }

    // Clutterbar — left side options bar (O/A/I/D/V buttons) at (10,22)
    // Matching Windows draw.cpp draw_clutterbar() function
    drawClutterbar(p);

    // Play/pause status indicator at (26,28), each 9x9
    if (!bmp.playpaus.isNull()) {
        int srcX = 27; // stopped/not playing
        if (player->playbackState() == QMediaPlayer::PlayingState) srcX = 0;
        else if (player->playbackState() == QMediaPlayer::PausedState) srcX = 9;
        p.drawPixmap(26, 28, bmp.playpaus, srcX, 0, 9, 9);
    }

    // Time display — digits are 9x13 in numbers.bmp
    // Positions: mins_tens(36,26), mins_ones(48,26), secs_tens(78,26), secs_ones(90,26)
    // The colon is baked into MAIN.BMP background — no colon glyph in numbers.bmp
    // BUT if nums_ex.bmp is present, use it and draw animated colon (matches Windows draw.cpp)
    // Click the time area to toggle elapsed / remaining

    // Prefer nums_ex.bmp if available (extended numbers with animated colon)
    const QPixmap &numberBitmap = !bmp.numbers_ex.isNull() ? bmp.numbers_ex : bmp.numbers;
    bool hasExtended = !bmp.numbers_ex.isNull();

    if (!numberBitmap.isNull()) {
        qint64 displayMs;
        if (showRemainingTime && player->duration() > 0) {
            displayMs = player->duration() - player->position();
            // Draw minus indicator from numbers.bmp 12th glyph, or fallback dash
            if (numberBitmap.width() >= 108)
                p.drawPixmap(27, 26, numberBitmap, 99, 0, 9, 13);
            else
                p.fillRect(29, 32, 5, 1, QColor(0, 198, 0));
        } else {
            displayMs = player->position();
        }
        int sec = displayMs / 1000;
        int mins = sec / 60;
        sec %= 60;
        auto drawDigit = [&](int dx, int d) {
            int srcX = (d >= 0 && d <= 9) ? d * 9 : 90; // 90 = blank
            p.drawPixmap(dx, 26, numberBitmap, srcX, 0, 9, 13);
        };

        // Draw animated colon if nums_ex.bmp is present (matches Windows ex==1 path)
        if (hasExtended) {
            // nums_ex.bmp has colon at x=90 (Windows draw_main.cpp line 240)
            p.drawPixmap(38, 26, numberBitmap, 90, 0, 9, 13);
        }
        // else: colon is baked into MAIN.BMP at position ~68, so don't draw it

        drawDigit(36, (mins / 10) % 10);
        drawDigit(48, mins % 10);
        drawDigit(78, sec / 10);
        drawDigit(90, sec % 10);
    }

    // Scrolling song title in text area (111,27) ~154x6
    // Uses metadata (Artist - Title) when available, falls back to filename
    if (!bmp.text.isNull() && !currentFile.isEmpty()) {
        QString title;
        if (!metaTitle.isEmpty())
            title = metaTitle.toUpper();
        else
            title = QFileInfo(currentFile).baseName().toUpper();
        QString scrollText = title + "  ***  " + title;
        int charW = 5;
        int totalW = (title.length() + 7) * charW;
        if (totalW < 1) totalW = 1;

        p.save();
        p.setClipRect(111, 27, 154, 6);
        int textX = 111 - (scrollOffset % totalW);
        for (QChar ch : scrollText) {
            QPoint cp = ::getTextCharPos(ch);
            if (cp.x() >= 0)
                p.drawPixmap(textX, 27, bmp.text, cp.x(), cp.y(), 5, 6);
            textX += charW;
        }
        p.restore();
    }

    // kbps display at (111, 43) — 3 chars using text.bmp 5x6 font
    // khz display at (156, 43) — 2 chars using text.bmp 5x6 font
    if (!bmp.text.isNull() && player->playbackState() != QMediaPlayer::StoppedState) {
        auto drawSmallChar = [&](int dx, int dy, QChar ch) {
            QPoint cp = ::getTextCharPos(ch);
            if (cp.x() >= 0)
                p.drawPixmap(dx, dy, bmp.text, cp.x(), cp.y(), 5, 6);
        };
        // kbps (3 digits, right-aligned)
        if (mediaBitrate > 0) {
            QString kbStr = QString::number(mediaBitrate).rightJustified(3, ' ');
            for (int i = 0; i < 3; i++)
                drawSmallChar(111 + i * 5, 43, kbStr[i]);
        }
        // khz (2 digits)
        if (mediaSampleRate > 0) {
            QString khStr = QString::number(mediaSampleRate).rightJustified(2, ' ');
            for (int i = 0; i < 2; i++)
                drawSmallChar(156 + i * 5, 43, khStr[i]);
        }
    }

    // Mono/Stereo indicator at (212,41) — each state 29x12
    if (!bmp.monoster.isNull()) {
        // stereo on: (0,0), stereo off: (0,12), mono on: (29,0), mono off: (29,12)
        bool isStereo = (mediaChannels >= 2);
        bool isMono = (mediaChannels == 1);
        bool playing = (player->playbackState() != QMediaPlayer::StoppedState);
        p.drawPixmap(212, 41, bmp.monoster, 0, (playing && isStereo) ? 0 : 12, 29, 12);
        p.drawPixmap(239, 41, bmp.monoster, 29, (playing && isMono) ? 0 : 12, 27, 12);
    }

    // Transport buttons from CBUTTONS.BMP — each 23x18, pressed row at y+18
    if (!bmp.cbuttons.isNull()) {
        auto drawBtn = [&](int id, int dx, int sx, int w, int h) {
            int sy = (pressedButton == id) ? h : 0;
            p.drawPixmap(dx, 88, bmp.cbuttons, sx, sy, w, h);
        };
        drawBtn(0, 16,  0,  23, 18);  // Previous
        drawBtn(1, 39,  23, 23, 18);  // Play
        drawBtn(2, 62,  46, 23, 18);  // Pause
        drawBtn(3, 85,  69, 23, 18);  // Stop
        drawBtn(4, 108, 92, 22, 18);  // Next
        // Eject: 22x16
        int ejY = (pressedButton == 5) ? 16 : 0;
        p.drawPixmap(136, 89, bmp.cbuttons, 114, ejY, 22, 16);
    }

    // Volume slider — 28 frames, each 68x13, spaced 15px apart in volume.bmp
    if (!bmp.volume.isNull()) {
        int frame = qBound(0, (volume * 27) / 255, 27);
        p.drawPixmap(107, 57, bmp.volume, 0, frame * 15, 68, 13);
        // Volume thumb: 14x11 at (0,422) normal, (15,422) pressed
        int thumbX = 107 + (volume * 51) / 255; // 68-14=54 range, but visually ~51
        int thumbSrcX = isDraggingVolume ? 15 : 0;
        p.drawPixmap(thumbX, 58, bmp.volume, thumbSrcX, 422, 14, 11);
    }

    // Balance/Pan slider — uses BALANCE.BMP (same format as volume)
    // Falls back to volume.bmp if balance isn't available
    if (!bmp.balance.isNull()) {
        int balNorm = qBound(0, (balance + 127) * 27 / 254, 27); // -127..+127 -> 0..27
        // Source sprite starts at x=9 in balance.bmp (matching Windows draw_panbar)
        p.drawPixmap(177, 57, bmp.balance, 9, balNorm * 15, 38, 13);
        int balThumbX = 177 + ((balance + 127) * 24) / 254;
        int balThumbSrcX = isDraggingBalance ? 0 : 15; // pressed=0, normal=15 (reversed from volume!)
        p.drawPixmap(balThumbX, 58, bmp.balance, balThumbSrcX, 422, 14, 11);
    } else if (!bmp.volume.isNull()) {
        // Fallback: draw balance using volume.bmp (cropped narrower)
        int balNorm = qBound(0, (balance + 127) * 27 / 254, 27);
        p.drawPixmap(177, 57, bmp.volume, 15, balNorm * 15, 38, 13);
        int balThumbX = 177 + ((balance + 127) * 24) / 254;
        p.drawPixmap(balThumbX, 58, bmp.volume, 0, 422, 14, 11);
    }

    // Position bar
    if (!bmp.posbar.isNull()) {
        p.drawPixmap(16, 72, bmp.posbar, 0, 0, 248, 10);
        if (player->duration() > 0) {
            int thumbX = 16 + (int)((player->position() * 219LL) / player->duration());
            int thumbSrcX = isDraggingPos ? 278 : 248;
            p.drawPixmap(thumbX, 72, bmp.posbar, thumbSrcX, 0, 29, 10);
        }
    }

    // Shuffle/Repeat/EQ/PL from SHUFREP.BMP
    if (!bmp.shufrep.isNull()) {
        p.drawPixmap(164, 89, bmp.shufrep, 28, shuffleOn ? 15 : 0, 47, 15);
        p.drawPixmap(210, 89, bmp.shufrep, 0, repeatOn ? 15 : 0, 28, 15);
        p.drawPixmap(219, 58, bmp.shufrep, 0, eqBtnOn ? 73 : 61, 23, 12);
        p.drawPixmap(242, 58, bmp.shufrep, 23, plBtnOn ? 73 : 61, 23, 12);
    }

    // Visualization area: (24,43) to (99,59) — 75x16
    if (visMode == 1) drawSpectrumAnalyzer(p);
    else if (visMode == 2) drawOscilloscope(p);
    else if (visMode == 3) drawVUMeter(p);

    // Double-size mode: scale 2x
    if (doubleSize && !shadeMode) {
        // Already handled by transform in actual paint
    }
}

// ============================================================================
// Clutterbar — Options bar on left side (O/A/I/D/V buttons)
// Matches Windows draw.cpp draw_clutterbar() at line 550
// ============================================================================
void WinampWindow::drawClutterbar(QPainter &p) {
    auto &bmp = WinampBitmaps::instance();
    if (bmp.titlebar.isNull()) return;

    // Clutterbar region: x=10, y=22, width=8, height=43
    // Source sprite at titlebar.bmp x=304
    int enable = clutterbarOpen ? 1 : 0;
    int x, y;

    if (!enable) {
        x = 8;  // Closed state
        y = 0;
    } else {
        x = 0;  // Open state
        y = 0;
    }

    // Draw main clutterbar strip (8x43 pixels)
    p.drawPixmap(10, 22, bmp.titlebar, 304 + x, y, 8, 43);

    // Draw Always On Top button state (at button position y=22+11=33)
    if (enable) {
        if (alwaysOnTop) {
            // AOT enabled: draw pressed sprite
            p.drawPixmap(11, 22 + 11, bmp.titlebar, 312 + 1, 44 + 11, 7, 8);
        } else {
            // AOT disabled: draw normal sprite
            p.drawPixmap(11, 22 + 11, bmp.titlebar, 304 + 1, 11, 7, 8);
        }

        // Draw Double Size button state (at button position y=22+27=49)
        if (doubleSize) {
            // Double size enabled: draw pressed sprite
            p.drawPixmap(11, 22 + 27, bmp.titlebar, 328 + 1, 44 + 27, 7, 6);
        } else {
            // Double size disabled: draw normal sprite
            p.drawPixmap(11, 22 + 27, bmp.titlebar, 304 + 1, 27, 7, 6);
        }
    }
}

// ============================================================================
// Visualization renderers
// ============================================================================
void WinampWindow::drawSpectrumAnalyzer(QPainter &p) {
    const int visX = 24, visY = 43, visH = 16;
    // Fill background
    p.fillRect(visX, visY, 75, visH, visColors[0]);
    // 19 bars, each 3px wide, 1px gap
    for (int i = 0; i < 19; i++) {
        float val = spectrumData[i]; // 0.0 - 1.0 (log-scaled)
        int target = (int)(val * 16.0f);
        if (target > 15) target = 15;
        // Smooth falloff
        if (target > saBarHeight[i]) saBarHeight[i] = target;
        else if (saBarHeight[i] > 0) saBarHeight[i]--;
        int h = saBarHeight[i];
        // Draw bar with color gradient
        for (int j = 0; j < h; j++) {
            int colorIdx = 17 - (j * 15 / 15);
            if (colorIdx < 2) colorIdx = 2;
            if (colorIdx > 17) colorIdx = 17;
            int py = visY + visH - 1 - j;
            p.fillRect(visX + i * 4, py, 3, 1, visColors[colorIdx]);
        }
        // Peak dot
        if (h > saPeakHeight[i]) {
            saPeakHeight[i] = h;
            saPeakVel[i] = 0;
        } else {
            saPeakVel[i] += 0.1f;
            saPeakHeight[i] -= (int)saPeakVel[i];
            if (saPeakHeight[i] < 0) saPeakHeight[i] = 0;
        }
        if (saPeakHeight[i] > 0) {
            int peakY = visY + visH - 1 - saPeakHeight[i];
            p.fillRect(visX + i * 4, peakY, 3, 1, visColors[23]);
        }
    }
}

void WinampWindow::drawOscilloscope(QPainter &p) {
    const int visX = 24, visY = 43, visH = 16;
    p.fillRect(visX, visY, 75, visH, visColors[0]);
    p.setPen(visColors[18]);
    int prevY = visY + visH / 2;
    for (int i = 0; i < 75; i++) {
        int cy = visY + visH / 2 - (int)(oscData[i] * (visH / 2));
        if (cy < visY) cy = visY;
        if (cy >= visY + visH) cy = visY + visH - 1;
        p.drawLine(visX + i, prevY, visX + i, cy);
        prevY = cy;
    }
}

// VU Meter — dual-channel level meter (matches Windows vu.cpp)
void WinampWindow::drawVUMeter(QPainter &p) {
    const int visX = 24, visY = 43, visH = 16;
    p.fillRect(visX, visY, 75, visH, visColors[0]);

    // Left channel
    int leftLevel = qBound(0, (int)(vuData[0] * 35), 35);
    for (int i = 0; i < leftLevel; i++) {
        int colorIdx = 17 - (i * 15 / 35);
        if (colorIdx < 2) colorIdx = 2;
        if (colorIdx > 17) colorIdx = 17;
        p.fillRect(visX + i * 2, visY, 1, 7, visColors[colorIdx]);
    }

    // Right channel
    int rightLevel = qBound(0, (int)(vuData[1] * 35), 35);
    for (int i = 0; i < rightLevel; i++) {
        int colorIdx = 17 - (i * 15 / 35);
        if (colorIdx < 2) colorIdx = 2;
        if (colorIdx > 17) colorIdx = 17;
        p.fillRect(visX + i * 2, visY + 9, 1, 7, visColors[colorIdx]);
    }

    // Labels
    p.setPen(visColors[1]);
    p.setFont(QFont("Courier", 5));
    p.drawText(visX + 72, visY + 6, "L");
    p.drawText(visX + 72, visY + 15, "R");
}

// ============================================================================
// keyPressEvent
// ============================================================================
void WinampWindow::keyPressEvent(QKeyEvent *event) {
    // Easter egg detection (matches Windows eggstat/eggstr from main.cpp)
    if (event->text().length() == 1) {
        QChar ch = event->text().at(0).toUpper();
        const QString egg1 = "NULLSOFT";
        const QString egg2 = "WINAMP";
        // Shift characters left, append new char
        for (int i = 0; i < 7; i++) eggStr[i] = eggStr[i + 1];
        eggStr[7] = ch.toLatin1();
        eggStr[8] = 0;

        if (QString(eggStr).endsWith(egg1)) {
            eggStat = 1;
            setWindowTitle("Winamp - \"It really whips the llama's ass!\"");
            QTimer::singleShot(3000, this, [this]() {
                setWindowTitle("Winamp 0.5 BETA for Linux");
                eggStat = 0;
            });
        } else if (QString(eggStr).endsWith(egg2)) {
            eggStat = 2;
            setWindowTitle("Winamp - by Justin Frankel & the Nullsoft crew");
            QTimer::singleShot(3000, this, [this]() {
                setWindowTitle("Winamp 0.5 BETA for Linux");
                eggStat = 0;
            });
        }
    }

    switch (event->key()) {
        case Qt::Key_Space:
            if (player->playbackState() == QMediaPlayer::PlayingState)
                player->pause();
            else if (!currentFile.isEmpty())
                player->play();
            else
                openFile();
            break;
        case Qt::Key_V:
            if (event->modifiers() == Qt::NoModifier) {
                player->stop();
            }
            break;
        case Qt::Key_C:
            player->pause();
            break;
        case Qt::Key_Z: {
            int curIdx = playlistWindow->currentTrackIndex();
            if (curIdx > 0) {
                playlistWindow->setCurrentTrackIndex(curIdx - 1);
                playTrack(playlistWindow->trackAt(curIdx - 1));
            } else {
                player->setPosition(0);
            }
            break;
        }
        case Qt::Key_B: {
            int curIdx = playlistWindow->currentTrackIndex();
            int count = playlistWindow->trackCount();
            if (curIdx + 1 < count) {
                playlistWindow->setCurrentTrackIndex(curIdx + 1);
                playTrack(playlistWindow->trackAt(curIdx + 1));
            }
            break;
        }
        case Qt::Key_X:
            // X = Play (like Windows Winamp)
            if (!currentFile.isEmpty()) player->play();
            else openFile();
            break;
        case Qt::Key_L:
            if (event->modifiers() & Qt::ControlModifier)
                onPlayLocation();
            else
                openFile();
            break;
        case Qt::Key_J: {
            if (event->modifiers() & Qt::ControlModifier) {
                // Ctrl+J = Jump to file (search in playlist)
                onJumpToFile();
            } else {
                // J = Jump to time dialog
                bool ok;
                QString timeStr = QInputDialog::getText(this, "Jump to Time",
                    "Enter time (MM:SS or seconds):", QLineEdit::Normal, "", &ok);
                if (ok && !timeStr.isEmpty()) {
                    qint64 jumpMs = 0;
                    if (timeStr.contains(':')) {
                        QStringList parts = timeStr.split(':');
                        if (parts.size() >= 2)
                            jumpMs = (parts[0].toInt() * 60 + parts[1].toInt()) * 1000;
                    } else {
                        jumpMs = timeStr.toInt() * 1000;
                    }
                    player->setPosition(qBound(0LL, jumpMs, player->duration()));
                }
            }
            break;
        }
        case Qt::Key_D:
            if (event->modifiers() & Qt::ControlModifier) {
                // Ctrl+D = Toggle double size
                onToggleDoubleSize();
            }
            break;
        case Qt::Key_W:
            if (event->modifiers() & Qt::ControlModifier) {
                // Ctrl+W = Toggle windowshade mode
                onToggleShadeMode();
            }
            break;
        case Qt::Key_T:
            if (event->modifiers() & Qt::ControlModifier) {
                // Ctrl+T = Toggle always on top
                onToggleAlwaysOnTop(!alwaysOnTop);
            }
            break;
        case Qt::Key_P:
            if (event->modifiers() & Qt::ControlModifier) {
                // Ctrl+P = Preferences
                PreferencesDialog *prefs = new PreferencesDialog(this);
                connect(prefs, &PreferencesDialog::skinChanged, this, &WinampWindow::onSkinChanged);
                connect(prefs, &PreferencesDialog::settingChanged, this, [this](const QString &key, const QVariant &value) {
                    if (key == "showNotifications") {
                        showSongNotifications = value.toBool();
                    } else if (key == "aot") {
                        onToggleAlwaysOnTop(value.toBool());
                    } else if (key == "doubleSize") {
                        if (value.toBool() != doubleSize) {
                            doubleSize = value.toBool();
                            setFixedSize(doubleSize ? 550 : 275, doubleSize ? 232 : 116);
                            update();
                        }
                    } else if (key == "stopAfterCurrent") {
                        stopAfterCurrent = value.toBool();
                    }
                });
                prefs->setAttribute(Qt::WA_DeleteOnClose);
                prefs->exec();
            }
            break;
        case Qt::Key_3:
            if (event->modifiers() & Qt::AltModifier) {
                // Alt+3 = File info dialog (matches Windows WINAMP_EDIT_ID3 / in_infobox)
                if (!currentFile.isEmpty()) {
                    FileInfoDialog *dlg = new FileInfoDialog(currentFile, player, this);
                    dlg->setAttribute(Qt::WA_DeleteOnClose);
                    dlg->exec();
                }
            }
            break;
        case Qt::Key_Left:
            player->setPosition(qMax(0LL, player->position() - 5000));
            break;
        case Qt::Key_Right:
            player->setPosition(qMin(player->duration(), player->position() + 5000));
            break;
        case Qt::Key_Up:
            volume = qMin(255, volume + 10);
            applyVolume();
            update();
            break;
        case Qt::Key_Down:
            volume = qMax(0, volume - 10);
            applyVolume();
            update();
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            volume = qMin(255, volume + 10);
            applyVolume();
            update();
            break;
        case Qt::Key_Minus:
            volume = qMax(0, volume - 10);
            applyVolume();
            update();
            break;
        case Qt::Key_R:
            if (event->modifiers() == Qt::NoModifier) {
                // R = Toggle repeat
                repeatOn = !repeatOn;
                update();
            }
            break;
        case Qt::Key_S:
            if (event->modifiers() == Qt::NoModifier) {
                // S = Toggle shuffle
                shuffleOn = !shuffleOn;
                update();
            }
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
    }
    event->accept();
}

// ============================================================================
// showContextMenu
// ============================================================================
void WinampWindow::showContextMenu(QPoint globalPos) {
    static const char *menuStyle =
        "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
        "QMenu::item:selected { background-color: #0000c6; }"
        "QMenu::item:checked { font-weight: bold; }"
        "QMenu::item:disabled { color: #666; }"
        "QMenu::separator { height: 1px; background: #555; margin: 2px 4px; }";

    QMenu menu;
    menu.setStyleSheet(menuStyle);

    // === Winamp main menu (matching Windows main.cpp top_menu) ===

    // -- Play submenu --
    QMenu *playMenu = menu.addMenu("Play");
    playMenu->setStyleSheet(menuStyle);
    QAction *playFileAct = playMenu->addAction("Play file...\tL");
    QAction *playLocAct = playMenu->addAction("Play location...\tCtrl+L");
    playMenu->addSeparator();

    // -- Recent files submenu --
    QMenu *recentMenu = playMenu->addMenu("Recent files");
    recentMenu->setStyleSheet(menuStyle);
    auto &recent = RecentFilesManager::instance();
    if (recent.recentFiles.isEmpty()) {
        QAction *empty = recentMenu->addAction("(no recent files)");
        empty->setEnabled(false);
    } else {
        for (int i = 0; i < recent.recentFiles.size(); i++) {
            QAction *a = recentMenu->addAction(
                QString("%1. %2").arg(i + 1).arg(QFileInfo(recent.recentFiles[i]).fileName()));
            a->setData(recent.recentFiles[i]);
        }
    }

    // -- Bookmarks submenu --
    QMenu *bmMenu = menu.addMenu("Bookmarks");
    bmMenu->setStyleSheet(menuStyle);
    QAction *addBmAct = bmMenu->addAction("Add current as bookmark");
    addBmAct->setEnabled(!currentFile.isEmpty());
    bmMenu->addSeparator();
    auto &bmMgr = BookmarkManager::instance();
    for (int i = 0; i < bmMgr.bookmarks.size(); i++) {
        QAction *a = bmMenu->addAction(bmMgr.bookmarks[i].title);
        a->setData("bm:" + QString::number(i));
    }
    if (bmMgr.bookmarks.isEmpty()) {
        QAction *empty = bmMenu->addAction("(no bookmarks)");
        empty->setEnabled(false);
    }

    menu.addSeparator();

    // -- Options submenu --
    QMenu *optMenu = menu.addMenu("Options");
    optMenu->setStyleSheet(menuStyle);
    QAction *aotAct = optMenu->addAction("Always on top\tCtrl+T");
    aotAct->setCheckable(true);
    aotAct->setChecked(alwaysOnTop);

    QAction *dsizeAct = optMenu->addAction("Double size\tCtrl+D");
    dsizeAct->setCheckable(true);
    dsizeAct->setChecked(doubleSize);

    QAction *shadeAct = optMenu->addAction("Windowshade mode\tCtrl+W");
    shadeAct->setCheckable(true);
    shadeAct->setChecked(shadeMode);

    optMenu->addSeparator();
    QAction *prefsAct = optMenu->addAction("Preferences...\tCtrl+P");

    optMenu->addSeparator();
    QAction *stopAfterAct = optMenu->addAction("Stop after current");
    stopAfterAct->setCheckable(true);
    stopAfterAct->setChecked(stopAfterCurrent);

    // -- Playback submenu --
    QMenu *pbMenu = menu.addMenu("Playback");
    pbMenu->setStyleSheet(menuStyle);
    QAction *jumpTimeAct = pbMenu->addAction("Jump to time...\tJ");
    QAction *jumpFileAct = pbMenu->addAction("Jump to file...\tCtrl+J");
    pbMenu->addSeparator();

    QAction *shuffAct = pbMenu->addAction("Shuffle");
    shuffAct->setCheckable(true);
    shuffAct->setChecked(shuffleOn);

    // Repeat submenu
    QMenu *repMenu = pbMenu->addMenu("Repeat");
    repMenu->setStyleSheet(menuStyle);
    QAction *repOffAct = repMenu->addAction("Off");
    repOffAct->setCheckable(true);
    repOffAct->setChecked(!repeatOn);
    QAction *repAllAct = repMenu->addAction("Repeat all");
    repAllAct->setCheckable(true);
    repAllAct->setChecked(repeatOn && !repeatTrack);
    QAction *repOneAct = repMenu->addAction("Repeat track");
    repOneAct->setCheckable(true);
    repOneAct->setChecked(repeatOn && repeatTrack);

    // -- Windows submenu --
    QMenu *winMenu = menu.addMenu("Windows");
    winMenu->setStyleSheet(menuStyle);
    QAction *eqTogAct = winMenu->addAction("Equalizer\tAlt+G");
    eqTogAct->setCheckable(true);
    eqTogAct->setChecked(eqBtnOn);
    QAction *plTogAct = winMenu->addAction("Playlist editor\tAlt+E");
    plTogAct->setCheckable(true);
    plTogAct->setChecked(plBtnOn);
    QAction *vidTogAct = winMenu->addAction("Video window");
    vidTogAct->setCheckable(true);
    vidTogAct->setChecked(videoWindow && videoWindow->isVisible());
    QAction *mlTogAct = winMenu->addAction("Media library\tAlt+L");
    mlTogAct->setCheckable(true);
    mlTogAct->setChecked(mediaLibraryWindow && mediaLibraryWindow->isVisible());
    winMenu->addSeparator();
    QAction *milkdropAct = winMenu->addAction("Milkdrop visualization");

    // -- Visualization submenu --
    QMenu *visMenu = menu.addMenu("Visualization");
    visMenu->setStyleSheet(menuStyle);
    QAction *visOffAct = visMenu->addAction("Off");
    visOffAct->setCheckable(true);
    visOffAct->setChecked(visMode == 0);
    QAction *visSpecAct = visMenu->addAction("Spectrum analyzer");
    visSpecAct->setCheckable(true);
    visSpecAct->setChecked(visMode == 1);
    QAction *visOscAct = visMenu->addAction("Oscilloscope");
    visOscAct->setCheckable(true);
    visOscAct->setChecked(visMode == 2);
    QAction *visVuAct = visMenu->addAction("VU meter");
    visVuAct->setCheckable(true);
    visVuAct->setChecked(visMode == 3);
    visMenu->addSeparator();
    QAction *visMilkdropAct = visMenu->addAction("Milkdrop visualization...");

    menu.addSeparator();

    QAction *aboutAct = menu.addAction("About Winamp...");
    menu.addSeparator();
    QAction *quitAct = menu.addAction("Exit");

    // === Handle selection ===
    QAction *sel = menu.exec(globalPos);
    if (!sel) return;

    if (sel == playFileAct) onPlayFile();
    else if (sel == playLocAct) onPlayLocation();
    else if (sel == addBmAct) onAddBookmark();
    else if (sel == aotAct) onToggleAlwaysOnTop(sel->isChecked());
    else if (sel == dsizeAct) onToggleDoubleSize();
    else if (sel == shadeAct) onToggleShadeMode();
    else if (sel == stopAfterAct) stopAfterCurrent = sel->isChecked();
    else if (sel == prefsAct) {
        PreferencesDialog *prefs = new PreferencesDialog(this);
        connect(prefs, &PreferencesDialog::skinChanged, this, &WinampWindow::onSkinChanged);
        connect(prefs, &PreferencesDialog::settingChanged, this, [this](const QString &key, const QVariant &value) {
            if (key == "showNotifications") {
                showSongNotifications = value.toBool();
            } else if (key == "aot") {
                onToggleAlwaysOnTop(value.toBool());
            } else if (key == "doubleSize") {
                if (value.toBool() != doubleSize) {
                    doubleSize = value.toBool();
                    setFixedSize(doubleSize ? 550 : 275, doubleSize ? 232 : 116);
                    update();
                }
            } else if (key == "stopAfterCurrent") {
                stopAfterCurrent = value.toBool();
            }
        });
        prefs->setAttribute(Qt::WA_DeleteOnClose);
        prefs->exec();
    }
    else if (sel == jumpTimeAct) {
        bool ok;
        QString timeStr = QInputDialog::getText(this, "Jump to Time",
            "Enter time (MM:SS or seconds):", QLineEdit::Normal, "", &ok);
        if (ok && !timeStr.isEmpty()) {
            qint64 jumpMs = 0;
            if (timeStr.contains(':')) {
                QStringList parts = timeStr.split(':');
                if (parts.size() >= 2)
                    jumpMs = (parts[0].toInt() * 60 + parts[1].toInt()) * 1000;
            } else {
                jumpMs = timeStr.toInt() * 1000;
            }
            player->setPosition(qBound(0LL, jumpMs, player->duration()));
        }
    }
    else if (sel == jumpFileAct) onJumpToFile();
    else if (sel == shuffAct) { shuffleOn = sel->isChecked(); update(); }
    else if (sel == repOffAct) { repeatOn = false; repeatTrack = false; update(); }
    else if (sel == repAllAct) { repeatOn = true; repeatTrack = false; update(); }
    else if (sel == repOneAct) { repeatOn = true; repeatTrack = true; update(); }
    else if (sel == eqTogAct) {
        eqBtnOn = sel->isChecked();
        if (eqBtnOn) eqWindow->show(); else eqWindow->hide();
        update();
    }
    else if (sel == plTogAct) {
        plBtnOn = sel->isChecked();
        if (plBtnOn) playlistWindow->show(); else playlistWindow->hide();
        update();
    }
    else if (sel == vidTogAct) {
        if (sel->isChecked()) videoWindow->show(); else videoWindow->hide();
    }
    else if (sel == mlTogAct) {
        if (sel->isChecked()) mediaLibraryWindow->show(); else mediaLibraryWindow->hide();
    }
    else if (sel == milkdropAct) openMilkdrop();
    else if (sel == visOffAct) { visMode = 0; update(); }
    else if (sel == visSpecAct) { visMode = 1; update(); }
    else if (sel == visOscAct) { visMode = 2; update(); }
    else if (sel == visVuAct) { visMode = 3; update(); }
    else if (sel == visMilkdropAct) openMilkdrop();
    else if (sel == aboutAct) onShowAbout();
    else if (sel == quitAct) close();
    // Recent files
    else if (sel->data().toString().startsWith("/") || sel->data().toString().startsWith("file:")) {
        QString path = sel->data().toString();
        if (QFile::exists(path)) {
            playFile(path);
        }
    }
    // Bookmarks
    else if (sel->data().toString().startsWith("bm:")) {
        int idx = sel->data().toString().mid(3).toInt();
        if (idx >= 0 && idx < bmMgr.bookmarks.size()) {
            QString path = bmMgr.bookmarks[idx].path;
            if (path.startsWith("http://") || path.startsWith("https://"))
                playUrl(path);
            else if (QFile::exists(path))
                playFile(path);
        }
    }
}

// ============================================================================
// mousePressEvent
// ============================================================================
void WinampWindow::mousePressEvent(QMouseEvent *event) {
    // ---- Modern skin mouse press handling ----
    if (isModernSkin) {
        int x = event->pos().x();
        int y = event->pos().y();

        if (event->button() == Qt::RightButton) {
            showContextMenu(event->globalPosition().toPoint());
            return;
        }

        // Check seek bar
        QRect seekR = modernSeekRect();
        if (seekR.contains(x, y) && player->duration() > 0) {
            modernDraggingSeek = true;
            double frac = qBound(0.0, (double)(x - seekR.x()) / seekR.width(), 1.0);
            player->setPosition((qint64)(frac * player->duration()));
            update();
            return;
        }

        // Check volume slider
        QRect volR = modernVolumeRect();
        if (volR.contains(x, y)) {
            modernDraggingVolume = true;
            double frac = qBound(0.0, (double)(x - volR.x()) / volR.width(), 1.0);
            volume = (int)(frac * 255);
            applyVolume();
            update();
            return;
        }

        // Check buttons
        int btn = modernGetButtonAt(x, y);
        if (btn >= 0) {
            modernPressed = btn;
            update();
            return;
        }

        // Drag window (titlebar or empty area)
        isDragging = true;
        dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        return;
    }

    // ---- Classic skin mouse press ----
    if (event->button() == Qt::RightButton) {
        int x = event->pos().x();
        int y = event->pos().y();
        // Right-click on repeat button: show repeat mode menu
        if (x >= 210 && x < 238 && y >= 89 && y < 104) {
            QMenu menu;
            menu.setStyleSheet(
                "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
                "QMenu::item:selected { background-color: #0000c6; }"
                "QMenu::item:checked { font-weight: bold; }"
            );
            QAction *repOffAct = menu.addAction("Repeat off");
            repOffAct->setCheckable(true);
            repOffAct->setChecked(!repeatOn);
            QAction *repAllAct = menu.addAction("Repeat all");
            repAllAct->setCheckable(true);
            repAllAct->setChecked(repeatOn && !repeatTrack);
            QAction *repOneAct = menu.addAction("Repeat track");
            repOneAct->setCheckable(true);
            repOneAct->setChecked(repeatOn && repeatTrack);
            QAction *sel = menu.exec(event->globalPosition().toPoint());
            if (sel == repOffAct) { repeatOn = false; repeatTrack = false; }
            else if (sel == repAllAct) { repeatOn = true; repeatTrack = false; }
            else if (sel == repOneAct) { repeatOn = true; repeatTrack = true; }
            update();
            return;
        }
        showContextMenu(event->globalPosition().toPoint());
        return;
    }
    int x = event->pos().x();
    int y = event->pos().y();

    // Time display area click: toggle elapsed/remaining
    if (x >= 36 && x < 99 && y >= 26 && y < 40) {
        showRemainingTime = !showRemainingTime;
        update();
        return;
    }

    // Visualization area click: (27,40)-(99,61)
    // Single click: cycle modes 0->1->2->3->0 (off/spectrum/osc/vu)
    // (double-click opens Milkdrop — handled in mouseDoubleClickEvent)
    if (x >= 27 && x < 99 && y >= 40 && y < 61) {
        visMode++;
        if (visMode > 3) visMode = 0;
        // Reset viz state when switching modes
        memset(saBarHeight, 0, sizeof(saBarHeight));
        memset(saPeakHeight, 0, sizeof(saPeakHeight));
        memset(saPeakVel, 0, sizeof(saPeakVel));
        update();
        return;
    }

    // Clutterbar toggle/buttons (matches Windows Ui.cpp do_clutterbar)
    // Toggle bar: x=10-18, y=22-30
    if (x >= 10 && x < 18 && y >= 22 && y < 30) {
        clutterbarOpen = !clutterbarOpen;
        update();
        return;
    }
    // Clutterbar buttons (only when open)
    if (clutterbarOpen && x >= 11 && x < 18) {
        // AOT (Always On Top) button: y=33-41
        if (y >= 33 && y < 41) {
            alwaysOnTop = !alwaysOnTop;
            setWindowFlag(Qt::WindowStaysOnTopHint, alwaysOnTop);
            show(); // Re-show to apply flag change
            update();
            return;
        }
        // File Info button: y=42-49
        if (y >= 42 && y < 49) {
            // Show file info dialog (same as Alt+3)
            if (!currentFile.isEmpty()) {
                FileInfoDialog *dlg = new FileInfoDialog(currentFile, player, this);
                dlg->show();
            }
            return;
        }
        // Double Size button: y=49-55
        if (y >= 49 && y < 55) {
            doubleSize = !doubleSize;
            if (doubleSize) {
                setFixedSize(275 * 2, 116 * 2);
            } else {
                setFixedSize(275, 116);
            }
            update();
            return;
        }
        // Visualization menu button: y=58-65 (TODO: implement vis menu)
        if (y >= 58 && y < 65) {
            // Could show visualization options menu
            update();
            return;
        }
    }

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
        applyVolume();
        update();
        return;
    }

    // Balance slider: (177,57) to (215,70)
    if (x >= 177 && x <= 215 && y >= 57 && y <= 70) {
        isDraggingBalance = true;
        balance = ((x - 177) * 254) / 38 - 127;
        balance = qBound(-127, balance, 127);
        // Apply stereo balance via QAudioOutput
        // Qt6 doesn't have direct balance, but we can approximate with stereo channel volumes
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

// ============================================================================
// mouseMoveEvent
// ============================================================================
void WinampWindow::mouseMoveEvent(QMouseEvent *event) {
    int x = event->position().x();
    int y = event->position().y();

    // ---- Modern skin mouse move ----
    if (isModernSkin) {
        // Seek drag
        if (modernDraggingSeek && player->duration() > 0) {
            QRect seekR = modernSeekRect();
            double frac = qBound(0.0, (double)(x - seekR.x()) / seekR.width(), 1.0);
            player->setPosition((qint64)(frac * player->duration()));
            update();
            return;
        }
        // Volume drag
        if (modernDraggingVolume) {
            QRect volR = modernVolumeRect();
            double frac = qBound(0.0, (double)(x - volR.x()) / volR.width(), 1.0);
            volume = (int)(frac * 255);
            applyVolume();
            update();
            return;
        }
        // Window drag
        if (isDragging) {
            move(event->globalPosition().toPoint() - dragPosition);
            playlistWindow->followMain();
            eqWindow->followMain();
            return;
        }
        // Hover tracking
        int oldHover = modernHovered;
        modernHovered = modernGetButtonAt(x, y);
        if (oldHover != modernHovered) update();
        return;
    }

    // ---- Classic skin mouse move ----
    // Update hovered button
    int oldHover = hoveredButton;
    hoveredButton = getButtonAt(x, y);
    if (oldHover != hoveredButton) update();

    // Show tooltips for controls (matching Windows tooltips)
    QString tooltip;
    if (y >= 88 && y <= 106) {
        if (x >= 16 && x < 39) tooltip = "Previous Track";
        else if (x >= 39 && x < 62) tooltip = "Play";
        else if (x >= 62 && x < 85) tooltip = "Pause";
        else if (x >= 85 && x < 108) tooltip = "Stop";
        else if (x >= 108 && x < 130) tooltip = "Next Track";
    } else if (y >= 89 && y <= 105 && x >= 136 && x < 158) {
        tooltip = "Eject / Open File";
    } else if (y >= 89 && y <= 104) {
        if (x >= 164 && x < 211) tooltip = "Toggle Shuffle";
        else if (x >= 210 && x < 238) tooltip = "Toggle Repeat";
    } else if (y >= 58 && y <= 70) {
        if (x >= 219 && x < 242) tooltip = "Toggle Equalizer";
        else if (x >= 242 && x < 265) tooltip = "Toggle Playlist";
    } else if (y >= 57 && y <= 70) {
        if (x >= 107 && x <= 175) tooltip = "Volume Control";
        else if (x >= 177 && x <= 215) tooltip = "Balance/Pan Control";
    } else if (y >= 72 && y <= 82 && x >= 16 && x <= 264) {
        tooltip = "Seek Position";
    } else if (y >= 26 && y < 40 && x >= 36 && x < 99) {
        tooltip = "Time Display (click to toggle)";
    } else if (y >= 40 && y < 61 && x >= 27 && x < 99) {
        tooltip = "Visualization (click to cycle modes, double-click for Milkdrop)";
    } else if (y >= 22 && y < 30 && x >= 10 && x < 18) {
        tooltip = "Toggle Clutterbar";
    }

    if (!tooltip.isEmpty()) {
        QToolTip::showText(event->globalPosition().toPoint(), tooltip, this);
    } else {
        QToolTip::hideText();
    }

    // Volume drag
    if (isDraggingVolume) {
        volume = ((x - 107) * 255) / 68;
        if (volume > 255) volume = 255;
        if (volume < 0) volume = 0;
        applyVolume();
        update();
    }

    // Balance drag
    if (isDraggingBalance) {
        balance = ((x - 177) * 254) / 38 - 127;
        balance = qBound(-127, balance, 127);
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

// ============================================================================
// mouseReleaseEvent
// ============================================================================
void WinampWindow::mouseReleaseEvent(QMouseEvent *event) {
    // ---- Modern skin mouse release ----
    if (isModernSkin) {
        if (modernDraggingSeek || modernDraggingVolume) {
            modernDraggingSeek = false;
            modernDraggingVolume = false;
            update();
            return;
        }
        if (modernPressed >= 0) {
            int x = event->pos().x();
            int y = event->pos().y();
            int btn = modernGetButtonAt(x, y);
            if (btn == modernPressed) {
                // Execute action based on button index
                switch (btn) {
                    case MB_PREV: {
                        int curIdx = playlistWindow->currentTrackIndex();
                        if (curIdx > 0) {
                            playlistWindow->setCurrentTrackIndex(curIdx - 1);
                            playTrack(playlistWindow->trackAt(curIdx - 1));
                        } else {
                            player->setPosition(0);
                        }
                        break;
                    }
                    case MB_PLAY:
                        if (!currentFile.isEmpty()) player->play();
                        else openFile();
                        break;
                    case MB_PAUSE: player->pause(); break;
                    case MB_STOP: player->stop(); break;
                    case MB_NEXT: {
                        int curIdx = playlistWindow->currentTrackIndex();
                        int count = playlistWindow->trackCount();
                        if (curIdx + 1 < count) {
                            playlistWindow->setCurrentTrackIndex(curIdx + 1);
                            playTrack(playlistWindow->trackAt(curIdx + 1));
                        }
                        break;
                    }
                    case MB_EJECT: openFile(); break;
                    case MB_PL:
                        plBtnOn = !plBtnOn;
                        if (plBtnOn) playlistWindow->show();
                        else playlistWindow->hide();
                        break;
                    case MB_ML:
                        if (mediaLibraryWindow) {
                            if (mediaLibraryWindow->isVisible()) mediaLibraryWindow->hide();
                            else mediaLibraryWindow->show();
                        }
                        break;
                    case MB_MUTE: {
                        // Toggle mute
                        static int savedVolume = 200;
                        if (audioOutput->volume() > 0.01f) {
                            savedVolume = volume;
                            audioOutput->setVolume(0.0);
                        } else {
                            volume = savedVolume;
                            applyVolume();
                        }
                        break;
                    }
                    case MB_REPEAT:
                        if (!repeatOn) { repeatOn = true; repeatTrack = false; }
                        else if (!repeatTrack) { repeatTrack = true; }
                        else { repeatOn = false; repeatTrack = false; }
                        break;
                    case MB_SHUFFLE:
                        shuffleOn = !shuffleOn;
                        break;
                    case MB_MINIMIZE: showMinimized(); break;
                    case MB_CLOSE: close(); break;
                }
            }
            modernPressed = -1;
            update();
            return;
        }
        isDragging = false;
        return;
    }

    // ---- Classic skin mouse release ----
    if (pressedButton >= 0) {
        int x = event->pos().x();
        int y = event->pos().y();
        int btnId = getButtonAt(x, y);

        if (btnId == pressedButton) {
            switch (btnId) {
                case 0: {                                       // Previous
                    int curIdx = playlistWindow->currentTrackIndex();
                    if (curIdx > 0) {
                        playlistWindow->setCurrentTrackIndex(curIdx - 1);
                        playTrack(playlistWindow->trackAt(curIdx - 1));
                    } else {
                        player->setPosition(0);
                    }
                    break;
                }
                case 1:                                          // Play
                    if (!currentFile.isEmpty()) player->play();
                    else openFile();
                    break;
                case 2: player->pause(); break;                  // Pause
                case 3: player->stop(); break;                   // Stop
                case 4: {                                        // Next
                    int curIdx = playlistWindow->currentTrackIndex();
                    int count = playlistWindow->trackCount();
                    if (curIdx + 1 < count) {
                        playlistWindow->setCurrentTrackIndex(curIdx + 1);
                        playTrack(playlistWindow->trackAt(curIdx + 1));
                    }
                    break;
                }
                case 5: openFile(); break;                       // Eject
            }
        }
        pressedButton = -1;
        update();
    }

    isDraggingVolume = false;
    isDraggingBalance = false;
    isDraggingPos = false;
    isDragging = false;
}

// ============================================================================
// mouseDoubleClickEvent
// ============================================================================
void WinampWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    int x = event->pos().x();
    int y = event->pos().y();
    // Double-click on visualization area opens Milkdrop
    if (x >= 27 && x < 99 && y >= 40 && y < 61) {
        openMilkdrop();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

// ============================================================================
// openFile / drag-and-drop
// ============================================================================
void WinampWindow::openFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open Audio File", "",
        "Audio Files (*.mp3 *.wav *.flac *.ogg *.m4a *.aac *.wma);;All Files (*)");
    if (!fileName.isEmpty()) {
        currentFile = fileName;
        player->setSource(QUrl::fromLocalFile(fileName));
        player->play();
        playlistWindow->addTrack(fileName);
        RecentFilesManager::instance().addFile(fileName);
    }
}

// Drag-and-drop on main window (matches Windows ole.cpp)
void WinampWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void WinampWindow::dropEvent(QDropEvent *event) {
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        for (const QUrl &url : urls) {
            QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                QFileInfo fi(path);
                if (fi.isDir()) {
                    // Add all audio files from directory
                    QDir dir(path);
                    QStringList filters = {"*.mp3", "*.wav", "*.flac", "*.ogg", "*.m4a", "*.aac"};
                    QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);
                    for (const QString &f : files)
                        playlistWindow->addTrack(dir.absoluteFilePath(f));
                } else {
                    playlistWindow->addTrack(path);
                }
            }
        }
        // Play the first dropped file
        if (!urls.isEmpty()) {
            QString firstPath = urls.first().toLocalFile();
            if (!firstPath.isEmpty() && QFileInfo(firstPath).isFile()) {
                playFile(firstPath);
            }
        }
        event->acceptProposedAction();
    }
}

// ============================================================================
// preloadNextTrack — gapless playback
// ============================================================================
void WinampWindow::preloadNextTrack() {
    int curIdx = playlistWindow->currentTrackIndex();
    int count = playlistWindow->trackCount();
    if (count == 0) return;

    int nextIdx;
    if (shuffleOn) {
        // For shuffle, we can't really preload since it's random
        return;
    } else {
        nextIdx = curIdx + 1;
    }

    if (nextIdx < count) {
        QString nextFile = playlistWindow->trackAt(nextIdx);
        if (!nextFile.isEmpty() && QFile::exists(nextFile)) {
            nextPlayer->setSource(QUrl::fromLocalFile(nextFile));
            // Don't play yet, just preload
        }
    } else if (repeatOn && count > 0) {
        // If repeat all is on, preload first track
        QString nextFile = playlistWindow->trackAt(0);
        if (!nextFile.isEmpty() && QFile::exists(nextFile)) {
            nextPlayer->setSource(QUrl::fromLocalFile(nextFile));
        }
    }
}

// ============================================================================
// playTrack / applyVolume / updateDisplay / getButtonAt
// ============================================================================
void WinampWindow::playTrack(const QString &fileName) {
    if (!fileName.isEmpty() && QFile::exists(fileName)) {
        currentFile = fileName;
        // Reset media info — will be refreshed by metaDataChanged and processAudioBuffer
        mediaBitrate = 0;
        mediaSampleRate = 0;
        mediaChannels = 0;
        metaTitle.clear();

        // Auto-load EQ preset if AUTO is enabled (matches Windows eq_autoload from Play.cpp line 58)
        if (eqWindow && eqWindow->isAutoEnabled()) {
            eqWindow->autoLoadPreset(fileName);
        }

        player->setSource(QUrl::fromLocalFile(fileName));
        player->play();
        RecentFilesManager::instance().addFile(fileName);
        updateTrayTooltip();

        // Show song change notification (matches Windows balloon tooltips)
        if (showSongNotifications && trayIcon) {
            QString title = metaTitle.isEmpty() ? QFileInfo(fileName).completeBaseName() : metaTitle;
            trayIcon->showMessage("Winamp", title, QSystemTrayIcon::Information, 3000);
        }

        // Preload next track for gapless playback
        preloadNextTrack();
    }
}

// Apply volume to audio outputs — respects EQ DSP path
// When EQ DSP is active, volume is applied in the DSP chain, not via QAudioOutput
void WinampWindow::applyVolume() {
    if (!eqDspActive) {
        audioOutput->setVolume(volume / 255.0f);
    }
    // nextAudioOutput always gets volume (it plays before DSP takes over)
    nextAudioOutput->setVolume(volume / 255.0f);
}

void WinampWindow::updateDisplay() {
    update();
}

int WinampWindow::getButtonAt(int x, int y) {
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

// ============================================================================
// closeEvent / saveAllSettings / loadAllSettings
// ============================================================================
void WinampWindow::closeEvent(QCloseEvent *event) {
    saveAllSettings();
    event->accept();
    QApplication::quit();
}

void WinampWindow::saveAllSettings() {
    QSettings s(configPath(), QSettings::IniFormat);

    s.beginGroup("MainWindow");
    s.setValue("x", x());
    s.setValue("y", y());
    s.endGroup();

    s.beginGroup("Playback");
    s.setValue("volume", volume);
    s.setValue("balance", balance);
    s.setValue("shuffle", shuffleOn);
    s.setValue("repeat", repeatOn);
    s.setValue("repeatTrack", repeatTrack);
    s.setValue("eqVisible", eqBtnOn);
    s.setValue("plVisible", plBtnOn);
    s.setValue("visMode", visMode);
    s.setValue("showRemainingTime", showRemainingTime);
    if (!currentFile.isEmpty()) {
        s.setValue("lastFile", currentFile);
    }
    s.endGroup();

    s.beginGroup("WindowState");
    s.setValue("alwaysOnTop", alwaysOnTop);
    s.setValue("doubleSize", doubleSize);
    s.setValue("shadeMode", shadeMode);
    s.setValue("stopAfterCurrent", stopAfterCurrent);
    s.setValue("showSongNotifications", showSongNotifications);
    s.endGroup();

    eqWindow->saveSettings(s);
    playlistWindow->saveSettings(s);

    s.sync();
}

void WinampWindow::loadAllSettings() {
    QString path = configPath();
    if (!QFile::exists(path)) return;

    QSettings s(path, QSettings::IniFormat);

    s.beginGroup("MainWindow");
    if (s.contains("x")) {
        move(s.value("x").toInt(), s.value("y").toInt());
    }
    s.endGroup();

    s.beginGroup("Playback");
    volume = s.value("volume", 200).toInt();
    balance = s.value("balance", 0).toInt();
    applyVolume();
    shuffleOn = s.value("shuffle", false).toBool();
    repeatOn = s.value("repeat", false).toBool();
    repeatTrack = s.value("repeatTrack", false).toBool();
    eqBtnOn = s.value("eqVisible", false).toBool();
    plBtnOn = s.value("plVisible", true).toBool();  // Show playlist by default for testing
    visMode = s.value("visMode", 1).toInt();
    showRemainingTime = s.value("showRemainingTime", false).toBool();
    QString lastFile = s.value("lastFile").toString();
    if (!lastFile.isEmpty() && QFile::exists(lastFile)) {
        currentFile = lastFile;
    }
    s.endGroup();

    s.beginGroup("WindowState");
    alwaysOnTop = s.value("alwaysOnTop", false).toBool();
    doubleSize = s.value("doubleSize", false).toBool();
    shadeMode = s.value("shadeMode", false).toBool();
    stopAfterCurrent = s.value("stopAfterCurrent", false).toBool();
    showSongNotifications = s.value("showSongNotifications", true).toBool();
    if (alwaysOnTop) {
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
        show();
    }
    s.endGroup();

    eqWindow->loadSettings(s);
    playlistWindow->loadSettings(s);

    // Position child windows relative to loaded main position
    eqWindow->move(x(), y() + height());
    playlistWindow->move(x() + width(), y());  // right of main

    // Show/hide child windows based on saved state
    if (eqBtnOn) eqWindow->show();
    if (plBtnOn) playlistWindow->show();

    update();
}

// ============================================================================
// openMilkdrop / setupSystemTray / updateTrayTooltip
// ============================================================================
void WinampWindow::openMilkdrop() {
    if (milkdropWindow) {
        milkdropWindow->raise();
        milkdropWindow->activateWindow();
        return;
    }
    milkdropWindow = new MilkdropWindow();
    connect(milkdropWindow, &QObject::destroyed, this, [this]() {
        milkdropWindow = nullptr;
    });
    connect(milkdropWindow, &MilkdropWindow::fullscreenChanged, this, [this](bool fs) {
            if (fs) {
                // Hide all Winamp windows when Milkdrop goes fullscreen
                eqWasVisible = eqWindow && eqWindow->isVisible();
                plWasVisible = playlistWindow && playlistWindow->isVisible();
                mainWasVisible = isVisible();
                if (eqWindow) eqWindow->hide();
                if (playlistWindow) playlistWindow->hide();
                hide();
            } else {
                // Restore windows when exiting fullscreen
                if (mainWasVisible) show();
                if (eqWasVisible && eqWindow) eqWindow->show();
                if (plWasVisible && playlistWindow) playlistWindow->show();
            }
        });
    milkdropWindow->show();
    milkdropWindow->raise();
    milkdropWindow->activateWindow();
}

void WinampWindow::setupSystemTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(windowIcon().isNull() ? QIcon::fromTheme("audio-headphones") : windowIcon());
    trayIcon->setToolTip("Winamp 0.5 BETA for Linux");

    trayMenu = new QMenu(this);
    trayMenu->addAction("Winamp", this, [this]() {
        show();
        raise();
        activateWindow();
    });
    trayMenu->addSeparator();
    trayMenu->addAction("Previous Track", this, [this]() {
        if (playlistWindow) playlistWindow->prevTrack();
    });
    QAction *playPauseAction = trayMenu->addAction("Play", this, [this]() {
        if (player->playbackState() == QMediaPlayer::PlayingState) {
            player->pause();
        } else if (player->playbackState() == QMediaPlayer::PausedState) {
            player->play();
        } else if (playlistWindow && playlistWindow->trackCount() > 0) {
            playlistWindow->playCurrentTrack();
        }
    });
    Q_UNUSED(playPauseAction);
    trayMenu->addAction("Stop", this, [this]() { player->stop(); update(); });
    trayMenu->addAction("Next Track", this, [this]() {
        if (playlistWindow) playlistWindow->nextTrack();
    });
    trayMenu->addSeparator();
    trayMenu->addAction("Open File...", this, [this]() { openFile(); });
    trayMenu->addSeparator();
    trayMenu->addAction("Exit", qApp, &QApplication::quit);

    trayIcon->setContextMenu(trayMenu);

    connect(trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            if (isVisible()) {
                hide();
                if (eqWindow) eqWindow->hide();
                if (playlistWindow) playlistWindow->hide();
            } else {
                show();
                raise();
                activateWindow();
                if (eqBtnOn && eqWindow) eqWindow->show();
                if (plBtnOn && playlistWindow) playlistWindow->show();
            }
        }
    });

    trayIcon->show();
}

void WinampWindow::updateTrayTooltip() {
    if (!trayIcon) return;
    QString tip = "Winamp";
    if (!metaTitle.isEmpty()) {
        tip = metaTitle;
    } else if (!currentFile.isEmpty()) {
        tip = QFileInfo(currentFile).completeBaseName();
    }
    // Tray tooltip is limited to 127 chars on most platforms
    if (tip.length() > 120) tip = tip.left(117) + "...";
    trayIcon->setToolTip(tip);
}

// ============================================================================
// MPRIS2 out-of-line method implementations (need full WinampWindow definition)
// ============================================================================
#ifdef QT_DBUS_LIB
void Mpris2PlayerAdaptor::Next() {
    WinampWindow *w = qobject_cast<WinampWindow*>(parent());
    if (w) {
        PlaylistWindow *pl = w->getPlaylistWindow();
        if (pl) pl->nextTrack();
    }
}
void Mpris2PlayerAdaptor::Previous() {
    WinampWindow *w = qobject_cast<WinampWindow*>(parent());
    if (w) {
        PlaylistWindow *pl = w->getPlaylistWindow();
        if (pl) pl->prevTrack();
    }
}
void Mpris2PlayerAdaptor::OpenUri(const QString &uri) {
    QUrl url(uri);
    if (url.isLocalFile()) {
        WinampWindow *w = qobject_cast<WinampWindow*>(parent());
        if (w) w->playTrack(url.toLocalFile());
    }
}
#endif

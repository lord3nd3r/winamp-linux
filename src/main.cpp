#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QMediaPlayer>
#include <QMediaMetaData>
#include <QAudioOutput>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QAudioBufferOutput>
#endif
#include <QAudioBuffer>
#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#endif
#include <QVideoWidget>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QVideoSink>
#endif
#include <QFileDialog>
#include <QMouseEvent>
#include <QTimer>
#include <QRandomGenerator>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QIODevice>
#include <QListWidget>
#include <QVBoxLayout>
#include <QPixmap>
#include <QImage>
#include <QIcon>
#include <QFile>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSettings>
#include <QDir>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <algorithm>
#include <random>
#include <cmath>
#include <cstring>
#include <memory>
#include <QInputDialog>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QDebug>
#include <QFileInfo>
#include <QDateTime>
#include <QProcess>
#include <QDesktopServices>
#include <QMessageBox>
#include <QSet>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QKeyEvent>
#include <QTreeView>
#include <QFileSystemModel>
#include <QStandardPaths>
#include <type_traits>
#include <QSystemTrayIcon>
#include <QToolTip>
#include <QSplashScreen>
#include <QStandardPaths>
#include <QScreen>
#include <QScrollBar>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QHeaderView>
#include <QSlider>
#include <QTextEdit>
#include <QSizeGrip>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QStyle>

// Qt5/Qt6 compatibility layer
#include "compat.h"

// D-Bus for MPRIS2 media player interface (Linux desktop integration)
#if defined(QT_DBUS_LIB) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusInterface>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QDBusMetaType>
#endif

// projectM — Milkdrop-compatible visualization engine
#include <libprojectM/projectM.hpp>

// EQ10 DSP engine (10-band graphic equalizer)
#include "eq_dsp.h"

// Skin helpers, playlist colors, archive extraction
#include "constants.h"

// Global skin playlist colors (loaded when skin changes)
SkinPlaylistColors g_plColors;

// ============================================================
// Bookmark Manager — store/retrieve bookmarked files and URLs
// ============================================================
#include "bookmark_manager.h"

// ============================================================
// Recent Files Manager — track recently played files
// ============================================================
#include "recent_files.h"

// ============================================================
// Jump to File Dialog — search within playlist (Ctrl+J / J)
// ============================================================

// ====================================================================
// File Info Dialog — Display/edit ID3 tags (Alt+3, matches FileInfo.cpp)
// ====================================================================

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
// ============================================================
// Simple FFT for spectrum analyzer (radix-2 DIT, 512-point)
// ============================================================
void fft512(const float *input, float *magnitudes) {
    const int N = 512;
    // Bit-reversal permutation
    float re[N], im[N];
    for (int i = 0; i < N; i++) {
        int j = 0;
        for (int b = 0; b < 9; b++)
            j |= ((i >> b) & 1) << (8 - b);
        re[j] = input[i];
        im[j] = 0.0f;
    }
    // Cooley-Tukey FFT
    for (int s = 1; s <= 9; s++) {
        int m = 1 << s;
        int m2 = m >> 1;
        float wRe = cosf(-2.0f * M_PI / m);
        float wIm = sinf(-2.0f * M_PI / m);
        for (int k = 0; k < N; k += m) {
            float tRe = 1.0f, tIm = 0.0f;
            for (int j = 0; j < m2; j++) {
                float uRe = re[k + j], uIm = im[k + j];
                float vRe = tRe * re[k + j + m2] - tIm * im[k + j + m2];
                float vIm = tRe * im[k + j + m2] + tIm * re[k + j + m2];
                re[k + j] = uRe + vRe;
                im[k + j] = uIm + vIm;
                re[k + j + m2] = uRe - vRe;
                im[k + j + m2] = uIm - vIm;
                float newTRe = tRe * wRe - tIm * wIm;
                tIm = tRe * wIm + tIm * wRe;
                tRe = newTRe;
            }
        }
    }
    // Output magnitudes for first 256 bins
    for (int i = 0; i < N / 2; i++) {
        magnitudes[i] = sqrtf(re[i] * re[i] + im[i] * im[i]);
    }
}
#endif

// Winamp visualization colors (24 entries from draw.cpp ppal2[])
// Can be overridden by viscolor.txt in skin directory
QColor visColors[24] = {
    QColor(0, 0, 0),         // 0: background
    QColor(24, 33, 41),      // 1: grey dots
    QColor(239, 49, 16),     // 2: spectrum top (brightest)
    QColor(206, 41, 16),     // 3
    QColor(214, 90, 0),      // 4
    QColor(214, 102, 0),     // 5
    QColor(214, 115, 0),     // 6
    QColor(198, 123, 8),     // 7
    QColor(222, 165, 24),    // 8
    QColor(214, 181, 33),    // 9
    QColor(189, 222, 41),    // 10
    QColor(148, 222, 33),    // 11
    QColor(41, 206, 16),     // 12
    QColor(50, 190, 16),     // 13
    QColor(57, 181, 16),     // 14
    QColor(49, 156, 8),      // 15
    QColor(41, 148, 0),      // 16
    QColor(24, 132, 8),      // 17: spectrum bottom (dimmest)
    QColor(255, 255, 255),   // 18: oscilloscope brightest
    QColor(214, 214, 222),   // 19
    QColor(181, 189, 189),   // 20
    QColor(160, 170, 175),   // 21
    QColor(148, 156, 165),   // 22
    QColor(150, 150, 150),   // 23: analyzer peak dot
};

// ============================================================
// Simple Translation System for Language Pack Support
// ============================================================
#include "translator.h"

// Convenience macro
#define TR(key, def) Translator::instance().tr(key, def)

// Forward declarations
class WinampWindow;
class VideoWindow;
class MilkdropWindow;
class MediaLibraryWindow;



// About Dialog — Demoscene-style animated credits (faithful to Windows original)

// Play Location Dialog
#include "dialogs.h"

// Preferences Dialog — Tree-based layout matching Windows Winamp (Options.cpp)
#include "preferences.h"


#include "winamp_bitmaps.h"

#include "modern_skin.h"

// Detect modern skin (now in constants.h: isModernSkinDir)

bool g_isModernSkin = false;
ModernSkinEngine *g_modernSkin = nullptr;

// Custom list widget for dragging tracks to file manager

// Playlist Window
#include "playlist.h"

// Equalizer Window
#include "equalizer.h"

// Playlist Window Constructor

// Equalizer Window Constructor


// ============================================================
// VideoWindow — Video playback window with skin support
// ============================================================
#include "video.h"

// ============================================================
// MilkdropWindow — projectM-powered Milkdrop visualization
// ============================================================
#include "milkdrop.h"

// ============================================================================
// MPRIS2 D-Bus Adaptor — Linux desktop media player integration
// Implements org.mpris.MediaPlayer2 and org.mpris.MediaPlayer2.Player
// This enables media keys (play/pause/next/prev), KDE Connect, panel widgets, etc.
// Matches what Windows Winamp does with its global hotkeys + remote control features
// ============================================================================
#if defined(QT_DBUS_LIB) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)


#include "mpris2_adaptors.h"

#endif // QT_DBUS_LIB && Qt6

// ============================================================
// MediaLibraryWindow — Media Library browser with gen.bmp skin
// ============================================================
#include "media_library.h"

// Main Winamp Window
#include "winamp_window.h"

// PlaylistWindow snap methods

// EqualizerWindow snap methods




// MPRIS2 out-of-line method implementations (need full WinampWindow definition)
#if defined(QT_DBUS_LIB) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
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

#include "python_plugin.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Winamp");
    app.setApplicationVersion("5.666");
    app.setOrganizationName("Nullsoft");
    app.setDesktopFileName("winamp.desktop");

    // Load the Winamp icon from the source resource directory
    QString appDir = QCoreApplication::applicationDirPath();
    QIcon appIcon;
    QStringList iconCandidates = {
        appDir + "/../../assets/WinampIcon.ico",
        appDir + "/../assets/WinampIcon.ico",
        appDir + "/WinampIcon.ico"
    };
    for (const QString &root : winampDataRoots(appDir)) {
        iconCandidates << (root + "/resource/WinampIcon.ico");
    }
    for (const QString &iconPath : iconCandidates) {
        if (QFile::exists(iconPath)) {
            appIcon = QIcon(iconPath);
            break;
        }
    }
    if (appIcon.isNull()) {
        appIcon = createFallbackAppIcon();
    }
    app.setWindowIcon(appIcon);

    // Load bitmaps — try saved skin, then project skins/default, then resource dir
    QSettings settings(configPath(), QSettings::IniFormat);
    QString skinPath = settings.value("skin").toString();
    
    // Load language pack
    QString langCode = settings.value("language", "en").toString();
    Translator::instance().loadLanguage(langCode);

    // Build a list of candidate paths
    QStringList candidates;
    if (!skinPath.isEmpty()) candidates << skinPath;
    candidates << winampSkinAndResourcePaths(appDir);

    // Check if saved skin is a modern (XML) skin — still load classic as fallback
    bool savedIsModern = !skinPath.isEmpty() && isModernSkinDir(skinPath);

    bool loaded = false;
    for (const QString &path : candidates) {
        QDir d(path);
        // Skip modern skin paths when loading classic bitmaps
        if (isModernSkinDir(d.absolutePath())) continue;
        if (d.exists() && WinampBitmaps::instance().loadAll(d.absolutePath())) {
            qDebug() << "Successfully loaded authentic Winamp bitmaps from:" << d.absolutePath();
            loaded = true;
            break;
        }
    }

    // If no classic skin loaded yet and saved skin was modern, load default classic as fallback
    if (!loaded) {
        QStringList fallbackClassic = winampSkinAndResourcePaths(appDir);
        for (const QString &path : fallbackClassic) {
            QDir d(path);
            if (d.exists() && WinampBitmaps::instance().loadAll(d.absolutePath())) {
                loaded = true;
                break;
            }
        }
    }

    // Also try loading any missing bitmaps from all other candidate paths
    if (loaded) {
        for (const QString &path : candidates) {
            QDir d(path);
            if (d.exists()) {
                WinampBitmaps::instance().loadMissing(d.absolutePath());
            }
        }
    }

    if (!loaded) {
        qWarning() << "Could not load Winamp skin bitmaps from any candidate path.";
        // Continue anyway — fallback rendering will be used
    }

    // Splash screen (matches Windows SPLASH.cpp)
    QPixmap splashPix;
    QSplashScreen *splash = nullptr;
    // Look for SPLASH.BMP in the loaded skin or fallback paths
    QStringList splashCandidates = candidates;
    splashCandidates << appDir + "/../Src/resources" << appDir + "/../../Src/resources";
    for (const QString &root : winampDataRoots(appDir)) {
        splashCandidates << (root + "/resource");
    }
    for (const QString &path : splashCandidates) {
        QString splashFile = QDir(path).filePath("SPLASH.BMP");
        if (QFile::exists(splashFile)) {
            splashPix.load(splashFile);
            break;
        }
        // Try lowercase
        splashFile = QDir(path).filePath("splash.bmp");
        if (QFile::exists(splashFile)) {
            splashPix.load(splashFile);
            break;
        }
    }
    if (!splashPix.isNull()) {
        splash = new QSplashScreen(splashPix);
        splash->show();
        app.processEvents();
    }

    WinampWindow w;
    // Out-of-process Python host: construction itself should not throw, but
    // keep a narrow guard so a future change cannot take down startup.
    std::unique_ptr<PythonPluginManager> pyManager;
    try {
        pyManager = std::make_unique<PythonPluginManager>(&w);
    } catch (const std::exception &e) {
        qWarning() << "[Python Plugins] Host manager init failed:" << e.what();
        qWarning() << "[Python Plugins] Continuing without plugin support.";
    }

    // If saved skin is modern, trigger modern skin loading
    if (savedIsModern) {
        w.onSkinChanged(skinPath);
    }
    
    // Process command-line arguments (matches Windows cmdline.cpp)
    // Supported: file paths to play, directories to scan, -play, -pause, -stop, -enqueue
    QStringList filesToPlay;
    QStringList dirsToPlay;
    bool enqueueMode = false;
    for (int i = 1; i < argc; i++) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "-enqueue" || arg == "--enqueue") {
            enqueueMode = true;
        } else if (arg == "-play" || arg == "--play") {
            // Will auto-play after adding files
        } else if (arg == "-pause" || arg == "--pause") {
            QTimer::singleShot(100, &w, [&w]() {
                if (w.getPlayer()->playbackState() == QMediaPlayer::PlayingState)
                    w.getPlayer()->pause();
            });
        } else if (arg == "-stop" || arg == "--stop") {
            QTimer::singleShot(100, &w, [&w]() { w.getPlayer()->stop(); });
        } else if (arg.startsWith("-")) {
            // Unknown flag — ignore
        } else {
            // Treat as file or directory path
            QFileInfo fi(arg);
            if (fi.isDir()) {
                dirsToPlay << fi.absoluteFilePath();
            } else if (fi.exists()) {
                filesToPlay << fi.absoluteFilePath();
            }
        }
    }
    
    // Add CLI files to playlist and optionally play
    PlaylistWindow *pl = w.getPlaylistWindow();
    if (pl) {
        // Enqueue directories asynchronously
        for (const QString &dir : dirsToPlay) {
            pl->addFolderAsync(dir, !enqueueMode && filesToPlay.isEmpty() && (dir == dirsToPlay.first()));
        }
        
        // Enqueue regular files
        for (const QString &f : filesToPlay) {
            pl->addTrack(f);
        }
        
        // Auto-play first file unless in enqueue mode
        if (!enqueueMode && !filesToPlay.isEmpty() && pl->trackCount() > 0) {
            pl->setCurrentTrackIndex(0);
            w.playTrack(pl->trackAt(0));
        }
    }
    
    if (splash) {
        QTimer::singleShot(1500, splash, [splash]() {
            splash->close();
            splash->deleteLater();
        });
    }
    w.show();
    return app.exec();
}


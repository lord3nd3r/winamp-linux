#pragma once
#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QAudioBufferOutput>
#include <QAudioSink>
#include <QMediaDevices>
#include <QIODevice>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSettings>
#include <QFileDialog>
#include <QInputDialog>
#include <QFileInfo>
#include <QDir>
#include <QRandomGenerator>
#include <QToolTip>
#include <QApplication>

#ifdef QT_DBUS_LIB
#include <QDBusConnection>
#endif

#include "eq10dsp.h"
#include "modernskinengine.h"

class PlaylistWindow;
class EqualizerWindow;
class VideoWindow;
class MilkdropWindow;
class MediaLibraryWindow;

class WinampWindow : public QWidget {
    Q_OBJECT
public:
    WinampWindow(QWidget *parent = nullptr);
    ~WinampWindow();

    void playFile(const QString &file);
    void playUrl(const QString &url);

public slots:
    void onPlayFile();
    QMediaPlayer *getPlayer() { return player; }
    PlaylistWindow *getPlaylistWindow() { return playlistWindow; }
    void onPlayLocation();
    void onToggleAlwaysOnTop(bool checked);
    void onToggleDoubleSize();
    void onToggleShadeMode();
    void onShowAbout();
    void onJumpToFile();
    void onAddBookmark();
    void onSkinChanged(const QString &skinPath);

public:
    // Modern skin button layout
    static constexpr int MB_PREV = 0, MB_PLAY = 1, MB_PAUSE = 2, MB_STOP = 3, MB_NEXT = 4;
    static constexpr int MB_EJECT = 5, MB_PL = 6, MB_ML = 7, MB_MUTE = 8;
    static constexpr int MB_REPEAT = 9, MB_SHUFFLE = 10;
    static constexpr int MB_MINIMIZE = 11, MB_CLOSE = 12;
    static constexpr int MB_COUNT = 13;
    static constexpr int MODERN_TH = 18;
    
    QRect modernButtonRect(int idx) const;
    int modernGetButtonAt(int x, int y) const;
    QRect modernSeekRect() const;
    QRect modernVolumeRect() const;
    void paintModern(QPainter &p);
    void processAudioBuffer(const QAudioBuffer &buffer);

    void playTrack(const QString &fileName);
    void applyVolume();
    void updateDisplay();
    int getButtonAt(int x, int y);

protected:
    void paintEvent(QPaintEvent *) override;
    void drawClutterbar(QPainter &p);
    void drawSpectrumAnalyzer(QPainter &p);
    void drawOscilloscope(QPainter &p);
    void drawVUMeter(QPainter &p);
    void keyPressEvent(QKeyEvent *event) override;
    void showContextMenu(QPoint globalPos);
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void openFile();
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void preloadNextTrack();
    void closeEvent(QCloseEvent *event) override;
    void saveAllSettings();
    void loadAllSettings();

private:
    QMediaPlayer *player;
    QAudioOutput *audioOutput;
    QAudioBufferOutput *audioBufferOutput;
    QMediaPlayer *nextPlayer;
    QAudioOutput *nextAudioOutput;
    bool usingNextPlayer;
    
    // EQ DSP processing
    QAudioSink *audioSink = nullptr;
    QIODevice *audioSinkDevice = nullptr;
    eq10_t eqState[2];
    int eqSampleRate = 0;
    int eqChannels = 0;
    bool eqDspActive = false;
    QTimer *timer;
    QTimer *scrollTimer;
    QString currentFile;
    QPoint dragPosition;
    bool isDragging;
    int volume;
    int hoveredButton;
    int pressedButton;
    bool shuffleOn, repeatOn, eqBtnOn, plBtnOn;
    bool repeatTrack;
    bool stopAfterCurrent;
    bool isDraggingVolume, isDraggingPos, isDraggingBalance;
    int scrollOffset;
    int balance;
    bool doubleSize;
    bool shadeMode;
    bool alwaysOnTop;
    bool clutterbarOpen;
    bool showSongNotifications = true;
    
    // Visualization state
    int visMode;
    int saBarHeight[19];
    int saPeakHeight[19];
    float saPeakVel[19];
    float spectrumData[75];
    float oscData[75];
    
    // Media info
    int mediaBitrate = 0;
    int mediaSampleRate = 0;
    int mediaChannels = 0;
    bool showRemainingTime = false;
    QString metaTitle;
    
    // VU meter data
    float vuData[2];
    
    // Easter egg state
    char eggStr[9];
    int eggStat;
    
    // System tray
    QSystemTrayIcon *trayIcon = nullptr;
    QMenu *trayMenu = nullptr;
    
    PlaylistWindow *playlistWindow;
    EqualizerWindow *eqWindow;
    VideoWindow *videoWindow = nullptr;
    MilkdropWindow *milkdropWindow = nullptr;
    MediaLibraryWindow *mediaLibraryWindow = nullptr;
    
    // Modern skin engine
    ModernSkinEngine modernSkin;
    bool isModernSkin = false;
    int modernHovered = -1;
    int modernPressed = -1;
    bool modernDraggingSeek = false;
    bool modernDraggingVolume = false;
    
    void openMilkdrop();
    bool eqWasVisible = false;
    bool plWasVisible = false;
    bool mainWasVisible = true;
    
    void setupSystemTray();
    void updateTrayTooltip();
};

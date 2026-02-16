#ifndef __MAIN_WINDOW_QT_H
#define __MAIN_WINDOW_QT_H

#include <QMainWindow>
#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QMediaPlayer>
#include <QAudioOutput>
#include "../qt6/QtWindowAdapter.h"

/**
 * MainWindowQt - Main Winamp window using Qt6
 * 
 * Replicates the classic Winamp interface on Linux using Qt6.
 * Maintains the iconic 275x116 pixel window with skin support.
 */
class MainWindowQt : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindowQt(QWidget *parent = nullptr);
    ~MainWindowQt();

    // Playback control
    void play();
    void pause();
    void stop();
    void previous();
    void next();
    
    // Playlist management
    void loadFiles(const QStringList &files);
    void enqueueFile(const QString &file);
    void clearPlaylist();
    
    // Window state
    void toggleWindowShade();
    void toggleDoubleSizeMode();
    void toggleEqualizer();
    void togglePlaylist();
    
signals:
    void playbackStarted();
    void playbackPaused();
    void playbackStopped();
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onPlayClicked();
    void onPauseClicked();
    void onStopClicked();
    void onPreviousClicked();
    void onNextClicked();
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void updateDisplay();

private:
    void setupUI();
    void setupMediaPlayer();
    void setupConnections();
    void drawMainWindow(QPainter *painter);
    void drawTitleBar(QPainter *painter);
    void drawVisualization(QPainter *painter);
    void drawControls(QPainter *painter);
    void loadDefaultSkin();
    QString formatTime(qint64 milliseconds);

    // UI Elements (classic Winamp layout)
    QWidget *m_centralWidget;
    
    // Control buttons
    QPushButton *m_btnPrevious;
    QPushButton *m_btnPlay;
    QPushButton *m_btnPause;
    QPushButton *m_btnStop;
    QPushButton *m_btnNext;
    QPushButton *m_btnOpen;
    
    // Display controls
    QPushButton *m_btnEqualizer;
    QPushButton *m_btnPlaylist;
    QPushButton *m_btnShuffle;
    QPushButton *m_btnRepeat;
    
    // Window controls
    QPushButton *m_btnMinimize;
    QPushButton *m_btnWindowShade;
    QPushButton *m_btnClose;
    
    // Sliders
    QSlider *m_volumeSlider;
    QSlider *m_positionSlider;
    QSlider *m_balanceSlider;
    
    // Display labels
    QLabel *m_timeDisplay;
    QLabel *m_titleDisplay;
    QLabel *m_bitrateDisplay;
    QLabel *m_freqDisplay;
    
    // Visualization widget
    QWidget *m_visualizationWidget;
    
    // Media player (Qt6 Multimedia)
    QMediaPlayer *m_mediaPlayer;
    QAudioOutput *m_audioOutput;
    
    // Playlist
    QStringList m_playlist;
    int m_currentTrack;
    
    // Display update timer
    QTimer *m_displayTimer;
    
    // Window state
    bool m_windowShade;
    bool m_doubleSizeMode;
    bool m_equalizerVisible;
    bool m_playlistVisible;
    bool m_shuffleEnabled;
    bool m_repeatEnabled;
    
    // Skin support
    QPixmap m_skinMain;
    QPixmap m_skinCbuttons;
    QPixmap m_skinTitlebar;
    QPixmap m_skinVolume;
    QPixmap m_skinBalance;
    QPixmap m_skinMonoster;
    QPixmap m_skinShufrep;
    QPixmap m_skinText;
    QPixmap m_skinNumbers;
    bool m_skinLoaded;
};

#endif // __MAIN_WINDOW_QT_H

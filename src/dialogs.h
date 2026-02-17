#ifndef DIALOGS_H
#define DIALOGS_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QListWidget>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QMediaMetaData>
#include <QTabWidget>
#include <QFormLayout>
#include <QTimer>
#include <QDateTime>
#include <QImage>
#include <QPainter>
#include <QKeyEvent>
#include <QFont>
#include <QRadialGradient>
#include <QMessageBox>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QSlider>
#include <QHeaderView>
#include <QFileDialog>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QSettings>
#include <QDir>
#include <QUrl>

// Forward declarations
class BookmarkManager;
class Translator;

// ============================================================
// Jump to File Dialog — search within playlist (Ctrl+J / J)
// ============================================================
class JumpToFileDialog : public QDialog {
    Q_OBJECT
public:
    JumpToFileDialog(const QStringList &tracks, QWidget *parent = nullptr);
    int getSelectedIndex() const { return selectedIndex; }

signals:
    void queueTrack(int index);

private slots:
    void filterList(const QString &text);
    void onItemSelected(QListWidgetItem *);

private:
    QStringList allTracks;
    QList<int> filteredIndices;
    QLineEdit *searchEdit;
    QListWidget *resultList;
    int selectedIndex = -1;
};

// ====================================================================
// File Info Dialog — Display/edit ID3 tags (Alt+3, matches FileInfo.cpp)
// ====================================================================
class FileInfoDialog : public QDialog {
    Q_OBJECT
public:
    FileInfoDialog(const QString &filePath, QMediaPlayer *player, QWidget *parent = nullptr);

private slots:
    void onSave();

private:
    QString m_filePath;
    QMediaPlayer *m_player;
    
    QLineEdit *titleEdit;
    QLineEdit *artistEdit;
    QLineEdit *albumEdit;
    QLineEdit *yearEdit;
    QLineEdit *trackEdit;
    QLineEdit *genreEdit;
    QTextEdit *commentEdit;
};

// ============================================================
// About Dialog — Demoscene-style animated credits
// ============================================================
class AboutDialog : public QDialog {
    Q_OBJECT
public:
    AboutDialog(const QString &skinPath, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *) override;

private slots:
    void tick();

private:
    static constexpr int NUM_STARS = 200;
    struct Star { double x, y, z, speed; };
    Star stars[NUM_STARS];

    QImage splashImg, teamImg;
    QList<QImage> teamFrames;
    QStringList credits;

    QTimer *animTimer;
    int frameCount = 0;
    int warpPhase = 0;
    int sqTable[65536];

    int creditIndex = 0;
    int creditFrame = 0;
    int creditX = 100, creditY = 150;

    double currentFps = 0;
    qint64 lastFpsTime = 0;
};

// ============================================================
// Play Location Dialog
// ============================================================
class PlayLocationDialog : public QDialog {
public:
    PlayLocationDialog(QWidget *parent = nullptr);
    QString getUrl() const;

private:
    QLineEdit *urlLineEdit;
};

// ============================================================
// Preferences Dialog — Tree-based layout matching Windows Winamp
// ============================================================
class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    PreferencesDialog(QWidget *parent = nullptr);

signals:
    void skinChanged(const QString &skinPath);
    void settingChanged(const QString &key, const QVariant &value);

private:
    QTreeWidget *treeWidget;
    QStackedWidget *stackedWidget;
    QString defaultSkinPath;

    QWidget *createGeneralPage();
    QWidget *createFileTypesPage();
    QWidget *createTitlesPage();
    QWidget *createLanguagePage();
    QWidget *createSkinsPage();
    QWidget *createClassicSkinsPage();
    QWidget *createModernSkinsPage();
    QWidget *createPlaybackPage();
    QWidget *createPlaylistPrefsPage();
    QWidget *createBookmarksPage();
    QWidget *createVisualizationPage();
    QWidget *createPluginsPage();

    QListWidget *skinListWidget = nullptr;
    void populateSkins();

    QListWidget *modernSkinListWidget = nullptr;
    void populateModernSkins();
    void onModernSkinSelected(QListWidgetItem *item);
    void onSkinSelected(QListWidgetItem *item);
};

#endif // DIALOGS_H

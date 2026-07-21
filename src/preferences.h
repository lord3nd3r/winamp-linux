// preferences.h — Winamp Preferences Dialog
#pragma once

#include <QDialog>
#include <QVariant>

class QTreeWidget;
class QStackedWidget;
class QListWidget;
class QSettings;
class QTreeWidgetItem;
class QListWidgetItem;

// Snapshot of the app's actual current settings, used to initialize the dialog's controls so
// they reflect reality instead of hardcoded defaults every time the dialog is opened.
struct PreferencesInitialState {
    bool alwaysOnTop = false;
    bool showInTray = true;
    bool minimizeToTray = false;
    bool showNotifications = true;
    bool showTooltips = true;
    bool snapWindows = true;
    int snapDistance = 25;
    bool doubleSize = false;
    int playbackPriority = 3; // combo index: 0=Idle .. 5=Highest, 3=Normal
    bool stopAfterCurrent = false;
    bool continuePlaybackOnStartup = false;
    bool fadeOnStopPause = false;
    bool useCustomPlaylistFont = false;
    QString playlistFontFamily = "Courier New";
    int playlistFontSize = 8;
    bool playlistRecycleBin = false;
    bool showTrackNumbers = true;
    int saFalloffSpeed = 1;     // 0=Slow, 1=Medium, 2=Fast, 3=Fastest
    int saPeakFalloffSpeed = 1;
    bool showPeakDots = true;
};

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    PreferencesDialog(QWidget *parent = nullptr, const PreferencesInitialState &state = PreferencesInitialState());

signals:
    void skinChanged(const QString &skinPath);
    void settingChanged(const QString &key, const QVariant &value);

private:
    QTreeWidget *treeWidget;
    QStackedWidget *stackedWidget;
    QString defaultSkinPath;
    QListWidget *pluginListWidget = nullptr;
    QListWidget *skinListWidget = nullptr;
    QListWidget *modernSkinListWidget = nullptr;
    PreferencesInitialState initState;

    QWidget *createGeneralPage();
    QWidget *createFileTypesPage();
    QWidget *createTitlesPage();
    QWidget *createLanguagePage();
    QWidget *createSkinsPage();
    QWidget *createClassicSkinsPage();
    QWidget *createPlaybackPage();
    QWidget *createPlaylistPrefsPage();
    QWidget *createBookmarksPage();
    QWidget *createVisualizationPage();
    QWidget *createPluginsPage();
    void populatePlugins();
    void populateSkins();
    QWidget *createModernSkinsPage();
    void populateModernSkins();
    void onModernSkinSelected(QListWidgetItem *item);
    void onSkinSelected(QListWidgetItem *item);
};

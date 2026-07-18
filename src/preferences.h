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
    QListWidget *pluginListWidget = nullptr;
    QListWidget *skinListWidget = nullptr;
    QListWidget *modernSkinListWidget = nullptr;

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

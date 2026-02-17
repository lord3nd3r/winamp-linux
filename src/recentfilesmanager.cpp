#include "recentfilesmanager.h"

void RecentFilesManager::load() {
    recentFiles.clear();
    QSettings s(QDir::homePath() + "/.config/winamp/winamp.conf", QSettings::IniFormat);
    int count = s.beginReadArray("RecentFiles");
    for (int i = 0; i < count; i++) {
        s.setArrayIndex(i);
        recentFiles.append(s.value("path").toString());
    }
    s.endArray();
}

void RecentFilesManager::save() {
    QSettings s(QDir::homePath() + "/.config/winamp/winamp.conf", QSettings::IniFormat);
    s.beginWriteArray("RecentFiles");
    for (int i = 0; i < recentFiles.size(); i++) {
        s.setArrayIndex(i);
        s.setValue("path", recentFiles[i]);
    }
    s.endArray();
}

void RecentFilesManager::addFile(const QString &path) {
    recentFiles.removeAll(path);
    recentFiles.prepend(path);
    while (recentFiles.size() > maxRecent)
        recentFiles.removeLast();
    save();
}

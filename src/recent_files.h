// recent_files.h — Recent Files Manager (track recently played files)
#pragma once

#include <QString>
#include <QStringList>
#include <QSettings>
#include <QDir>

class RecentFilesManager {
public:
    static RecentFilesManager& instance() {
        static RecentFilesManager mgr;
        return mgr;
    }

    void load() {
        recentFiles.clear();
        QSettings s(QDir::homePath() + "/.config/winamp/winamp.conf", QSettings::IniFormat);
        int count = s.beginReadArray("RecentFiles");
        for (int i = 0; i < count; i++) {
            s.setArrayIndex(i);
            recentFiles.append(s.value("path").toString());
        }
        s.endArray();
    }

    void save() {
        QSettings s(QDir::homePath() + "/.config/winamp/winamp.conf", QSettings::IniFormat);
        s.beginWriteArray("RecentFiles");
        for (int i = 0; i < recentFiles.size(); i++) {
            s.setArrayIndex(i);
            s.setValue("path", recentFiles[i]);
        }
        s.endArray();
    }

    void addFile(const QString &path) {
        recentFiles.removeAll(path);
        recentFiles.prepend(path);
        while (recentFiles.size() > maxRecent)
            recentFiles.removeLast();
        save();
    }

    QStringList recentFiles;
    int maxRecent = 15;

private:
    RecentFilesManager() { load(); }
};

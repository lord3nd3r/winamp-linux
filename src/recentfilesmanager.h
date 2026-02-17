#ifndef RECENTFILESMANAGER_H
#define RECENTFILESMANAGER_H

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

    void load();
    void save();
    void addFile(const QString &path);

    QStringList recentFiles;
    int maxRecent = 15;

private:
    RecentFilesManager() { load(); }
};

#endif // RECENTFILESMANAGER_H

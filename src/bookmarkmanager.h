#ifndef BOOKMARKMANAGER_H
#define BOOKMARKMANAGER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>

class BookmarkManager {
public:
    struct Bookmark {
        QString title;
        QString path;  // file path or URL
    };

    static BookmarkManager& instance() {
        static BookmarkManager mgr;
        return mgr;
    }

    void load();
    void save();
    void addBookmark(const QString &title, const QString &path);
    void removeBookmark(int index);

    QList<Bookmark> bookmarks;

private:
    BookmarkManager() { load(); }
};

#endif // BOOKMARKMANAGER_H

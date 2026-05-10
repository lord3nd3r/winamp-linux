// bookmark_manager.h — Bookmark Manager (store/retrieve bookmarked files and URLs)
#pragma once

#include <QString>
#include <QList>
#include <QDir>
#include <QFile>
#include <QTextStream>
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

    void load() {
        bookmarks.clear();
        QString dir = QDir::homePath() + "/.config/winamp";
        QDir().mkpath(dir);
        QFile file(dir + "/bookmarks.txt");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.isEmpty() || line.startsWith('#')) continue;
                int sep = line.indexOf('\t');
                Bookmark bm;
                if (sep > 0) {
                    bm.title = line.left(sep);
                    bm.path = line.mid(sep + 1);
                } else {
                    bm.path = line;
                    bm.title = QFileInfo(line).baseName();
                }
                bookmarks.append(bm);
            }
            file.close();
        }
    }

    void save() {
        QString dir = QDir::homePath() + "/.config/winamp";
        QDir().mkpath(dir);
        QFile file(dir + "/bookmarks.txt");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "# Winamp Bookmarks\n";
            for (const Bookmark &bm : bookmarks) {
                out << bm.title << "\t" << bm.path << "\n";
            }
            file.close();
        }
    }

    void addBookmark(const QString &title, const QString &path) {
        // Don't add duplicates
        for (const Bookmark &bm : bookmarks) {
            if (bm.path == path) return;
        }
        bookmarks.append({title, path});
        save();
    }

    void removeBookmark(int index) {
        if (index >= 0 && index < bookmarks.size()) {
            bookmarks.removeAt(index);
            save();
        }
    }

    QList<Bookmark> bookmarks;

private:
    BookmarkManager() { load(); }
};

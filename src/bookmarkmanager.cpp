#include "bookmarkmanager.h"

void BookmarkManager::load() {
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

void BookmarkManager::save() {
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

void BookmarkManager::addBookmark(const QString &title, const QString &path) {
    // Don't add duplicates
    for (const Bookmark &bm : bookmarks) {
        if (bm.path == path) return;
    }
    bookmarks.append({title, path});
    save();
}

void BookmarkManager::removeBookmark(int index) {
    if (index >= 0 && index < bookmarks.size()) {
        bookmarks.removeAt(index);
        save();
    }
}

// constants.h — Shared constants, helpers, and types for Winamp Linux
#pragma once

#include <QString>
#include <QStringList>
#include <QDir>
#include <QColor>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QFileInfo>
#include <QUrl>

// Shared Winamp-style QMenu stylesheet (used by all context menus)
static const char *kWinampMenuStyle =
    "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
    "QMenu::item:selected { background-color: #0000c6; }"
    "QMenu::item:checked { font-weight: bold; }"
    "QMenu::item:disabled { color: #666; }"
    "QMenu::separator { height: 1px; background: #555; margin: 2px 4px; }";

// Shared audio file filter string for all file dialogs
static const char *kAudioFileFilter =
    "Audio Files (*.mp3 *.wav *.flac *.ogg *.m4a *.aac *.wma *.opus);;All Files (*)";

// Config file path helper
static inline QString configPath() {
    QString dir = QDir::homePath() + "/.config/winamp";
    QDir().mkpath(dir);
    return dir + "/winamp.conf";
}

// Standard data root search paths for installed assets
static inline QStringList winampDataRoots(const QString &appDir) {
    return {
        appDir + "/../share/winamp",
        appDir + "/../../share/winamp",
        "/usr/local/share/winamp",
        "/usr/share/winamp",
        QDir::homePath() + "/.local/share/winamp"
    };
}

// Combined skin and resource search paths
static inline QStringList winampSkinAndResourcePaths(const QString &appDir) {
    QStringList out = {
        appDir + "/../skins/default",
        appDir + "/../../skins/default",
        QDir::homePath() + "/.winamp/skins/default",
        appDir + "/../assets",
        appDir + "/../../assets"
    };

    const QStringList roots = winampDataRoots(appDir);
    for (const QString &root : roots) {
        out << (root + "/skins/default")
            << (root + "/resource");
    }
    return out;
}

// Check if a value is a remote URL (uses QUrl parsing to match original)
static inline bool isRemoteMediaLocation(const QString &value) {
    QUrl url = QUrl::fromUserInput(value.trimmed());
    if (!url.isValid() || url.isLocalFile() || url.scheme().isEmpty()) return false;
    const QString scheme = url.scheme().toLower();
    return scheme == "http" || scheme == "https" || scheme == "icy" || scheme == "ftp";
}

// Display label for a playlist entry (filename for local, host/path for remote)
static inline QString playlistEntryLabel(const QString &value) {
    QUrl url = QUrl::fromUserInput(value.trimmed());
    if (url.isValid() && !url.isLocalFile() && !url.scheme().isEmpty()) {
        QString label = url.fileName();
        if (label.isEmpty()) label = url.host();
        if (label.isEmpty()) label = value;
        return label;
    }
    return QFileInfo(value).fileName();
}

// Sort key for playlist entries
static inline QString playlistSortLabel(const QString &value) {
    QUrl url = QUrl::fromUserInput(value.trimmed());
    if (url.isValid() && !url.isLocalFile() && !url.scheme().isEmpty()) {
        QString label = url.host() + url.path();
        if (label.isEmpty()) label = value;
        return label.toLower();
    }
    return QFileInfo(value).baseName().toLower();
}

// ============================================================
// Skin Playlist Colors — parse PLEDIT.TXT (from Windows skins)
// ============================================================
struct SkinPlaylistColors {
    QColor normal      = QColor(0, 255, 0);       // Normal text
    QColor current     = QColor(255, 255, 255);    // Currently playing
    QColor normBg      = QColor(0, 0, 0);          // Background
    QColor selectBg    = QColor(0, 0, 198);         // Selection background
    QColor mbFg        = QColor(0, 255, 0);         // Minibar foreground
    QColor mbBg        = QColor(0, 0, 0);           // Minibar background
};

static inline SkinPlaylistColors parsePleditTxt(const QString &skinPath) {
    SkinPlaylistColors colors;
    QStringList candidates = {
        skinPath + "/PLEDIT.TXT",
        skinPath + "/pledit.txt",
        skinPath + "/Pledit.txt"
    };
    for (const QString &path : candidates) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                auto parseColor = [](const QString &val) -> QColor {
                    if (val.startsWith('#')) return QColor(val);
                    QStringList p = val.split(',');
                    if (p.size() == 3) return QColor(p[0].toInt(), p[1].toInt(), p[2].toInt());
                    return QColor();
                };
                auto tryParse = [&](const QString &prefix, QColor &target) {
                    if (line.startsWith(prefix, Qt::CaseInsensitive)) {
                        QString val = line.mid(line.indexOf('=') + 1).trimmed();
                        QColor c = parseColor(val);
                        if (c.isValid()) target = c;
                    }
                };
                tryParse("Normal=", colors.normal);
                tryParse("Current=", colors.current);
                tryParse("NormalBG=", colors.normBg);
                tryParse("SelectedBG=", colors.selectBg);
                tryParse("MbFG=", colors.mbFg);
                tryParse("MbBG=", colors.mbBg);
            }
            file.close();
            break;
        }
    }
    return colors;
}

// Extract a .wsz or .zip skin archive to a cache directory.
// Returns the path to the extracted folder, or empty string on failure.
static inline QString extractSkinArchive(const QString &archivePath) {
    QFileInfo fi(archivePath);
    if (!fi.exists()) return {};
    
    // Cache dir: ~/.cache/winamp/skins/<basename_without_ext>
    QString cacheDirBase = QDir::homePath() + "/.cache/winamp/skins";
    QString skinName = fi.completeBaseName();
    QString extractDir = cacheDirBase + "/" + skinName;
    QDir().mkpath(extractDir);
    
    // Check if already extracted (any .bmp file exists)
    QDir ed(extractDir);
    QStringList bmps = ed.entryList(QStringList() << "*.bmp" << "*.BMP", QDir::Files);
    if (!bmps.isEmpty()) {
        return extractDir;
    }
    
    // Extract using unzip
    QProcess proc;
    proc.setWorkingDirectory(extractDir);
    proc.start("unzip", QStringList() << "-o" << "-j" << archivePath << "-d" << extractDir);
    proc.waitForFinished(10000);
    
    if (proc.exitCode() != 0) {
        qWarning() << "Failed to extract skin archive:" << archivePath << proc.readAllStandardError();
        return {};
    }
    
    return extractDir;
}

// Detect whether a skin directory is a modern (XML) skin vs classic (BMP) skin
static inline bool isModernSkinDir(const QString &path) {
    return QFile::exists(path + "/skin.xml");
}

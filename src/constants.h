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
#include <QDebug>

// Product version — keep aligned with GitHub release tags (currently v0.5.0-beta2).
// CMake project(VERSION) uses the numeric 0.5.0; this string is the full label.
inline constexpr const char *kWinampVersion = "0.5.0-beta2";
inline constexpr const char *kWinampWindowTitle = "Winamp 0.5.0-beta2 for Linux";
inline constexpr const char *kWinampAboutLine = "Winamp v0.5.0-beta2 for Linux";

// Shared Winamp-style QMenu stylesheet (used by all context menus)
inline constexpr const char *kWinampMenuStyle =
    "QMenu { background-color: #2b2d3d; color: #00ff00; border: 1px solid #555; font-size: 9pt; }"
    "QMenu::item:selected { background-color: #0000c6; }"
    "QMenu::item:checked { font-weight: bold; }"
    "QMenu::item:disabled { color: #666; }"
    "QMenu::separator { height: 1px; background: #555; margin: 2px 4px; }";

// Shared audio file filter string for all file dialogs
inline constexpr const char *kAudioFileFilter =
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
    
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        qWarning() << "Failed to extract skin archive:" << archivePath << proc.readAllStandardError();
        return {};
    }
    
    return extractDir;
}

// Detect whether a skin directory is a modern (XML) skin vs classic (BMP) skin
static inline bool isModernSkinDir(const QString &path) {
    return QFile::exists(path + "/skin.xml");
}

#include <QPoint>

// Shared text.bmp character lookup (5x6 per char)
static inline QPoint getTextCharPos(QChar ch) {
    if (ch >= 'A' && ch <= 'Z') return QPoint((ch.toLatin1() - 'A') * 5, 0);
    if (ch >= 'a' && ch <= 'z') return QPoint((ch.toLatin1() - 'a') * 5, 0);
    if (ch >= '0' && ch <= '9') return QPoint((ch.toLatin1() - '0') * 5, 6);
    switch (ch.toLatin1()) {
        case ' ': return QPoint(142, 0);
        case ':': return QPoint(60, 6);
        case '.': return QPoint(55, 6);
        case '\'': case '`': return QPoint(80, 6);
        case '(': return QPoint(65, 6);
        case ')': return QPoint(70, 6);
        case '-': return QPoint(75, 6);
        case '!': return QPoint(85, 6);
        case '_': return QPoint(90, 6);
        case '+': return QPoint(95, 6);
        case '\\': return QPoint(100, 6);
        case '/': return QPoint(105, 6);
        case '[': case '{': case '<': return QPoint(110, 6);
        case ']': case '}': case '>': return QPoint(115, 6);
        case '~': case '^': return QPoint(120, 6);
        case '&': return QPoint(125, 6);
        case '%': return QPoint(130, 6);
        case ',': return QPoint(135, 6);
        case '=': return QPoint(140, 6);
        case '$': return QPoint(145, 6);
        case '#': return QPoint(150, 6);
        case '"': return QPoint(130, 0);
        case '@': return QPoint(135, 0);
        case '?': return QPoint(15, 12);
        case '*': return QPoint(20, 12);
    }
    return QPoint(-1, -1);
}

// Shared visualization colors array
extern QColor visColors[24];

// Load viscolor.txt if present (Windows-compatible format: 24 lines of "r,g,b")
static inline void loadVisColors(const QString &skinPath) {
    QFile file(skinPath + "/viscolor.txt");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Try case variations
        file.setFileName(skinPath + "/VISCOLOR.TXT");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    }
    
    QTextStream in(&file);
    int idx = 0;
    while (!in.atEnd() && idx < 24) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#") || line.startsWith(";")) continue;
        
        // Parse "r,g,b" format (matching draw.cpp line 402)
        QStringList parts = line.split(",");
        if (parts.size() >= 3) {
            int r = parts[0].trimmed().toInt();
            int g = parts[1].trimmed().toInt();
            int b = parts[2].trimmed().toInt();
            visColors[idx] = QColor(r, g, b);
            idx++;
        }
    }
}

// Global modern skin and playlist color variables
extern bool g_isModernSkin;
class ModernSkinEngine;
extern ModernSkinEngine *g_modernSkin;
extern SkinPlaylistColors g_plColors;

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
void fft512(const float *input, float *magnitudes);
#endif

// Built-in EQ Presets (slider values 0-63, center=32)
struct EqPreset {
    const char *name;
    int values[10];
};

inline constexpr EqPreset builtinPresets[] = {
    {"Flat",                {32, 32, 32, 32, 32, 32, 32, 32, 32, 32}},
    {"Classical",           {32, 32, 32, 32, 32, 32, 44, 44, 44, 48}},
    {"Club",                {32, 32, 25, 21, 21, 21, 25, 32, 32, 32}},
    {"Dance",               {14, 18, 27, 32, 32, 40, 43, 43, 32, 32}},
    {"Full Bass",           {14, 14, 14, 21, 29, 37, 44, 48, 50, 50}},
    {"Full Bass & Treble",  {18, 21, 32, 43, 37, 28, 16, 12,  8,  6}},
    {"Full Treble",         {48, 48, 48, 37, 27, 12,  4,  4,  4,  2}},
    {"Laptop Speakers",     {25, 12, 23, 36, 33, 29, 25, 15,  8,  4}},
    {"Large Hall",          {14, 14, 21, 21, 32, 37, 37, 37, 32, 32}},
    {"Live",                {37, 32, 25, 21, 21, 21, 25, 28, 28, 29}},
    {"Party",               {18, 18, 32, 32, 32, 32, 32, 32, 18, 18}},
    {"Pop",                 {33, 25, 18, 16, 21, 32, 33, 33, 33, 33}},
    {"Reggae",              {32, 32, 32, 40, 32, 22, 22, 32, 32, 32}},
    {"Rock",                {18, 25, 39, 43, 35, 26, 16, 12, 12, 12}},
    {"Ska",                 {34, 37, 37, 32, 26, 23, 16, 15, 12, 15}},
    {"Soft",                {25, 29, 32, 34, 32, 25, 16, 14, 12, 10}},
    {"Soft Rock",           {25, 25, 28, 32, 37, 35, 29, 25, 22, 16}},
    {"Techno",              {18, 21, 32, 39, 37, 32, 18, 15, 15, 16}},
};
inline constexpr int numPresets = sizeof(builtinPresets) / sizeof(builtinPresets[0]);

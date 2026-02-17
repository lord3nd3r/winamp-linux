#ifndef SKINUTILS_H
#define SKINUTILS_H

#include <QString>
#include <QColor>
#include <QPoint>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>

// Extract a .wsz or .zip skin archive to a cache directory.
QString extractSkinArchive(const QString &archivePath);

// Skin Playlist Colors — parse PLEDIT.TXT (from Windows skins)
struct SkinPlaylistColors {
    QColor normal      = QColor(0, 255, 0);
    QColor current     = QColor(255, 255, 255);
    QColor normBg      = QColor(0, 0, 0);
    QColor selectBg    = QColor(0, 0, 198);
    QColor mbFg        = QColor(0, 255, 0);
    QColor mbBg        = QColor(0, 0, 0);
};

SkinPlaylistColors parsePleditTxt(const QString &skinPath);

// Global skin playlist colors (loaded when skin changes)
extern SkinPlaylistColors g_plColors;

// Simple FFT for spectrum analyzer (radix-2 DIT, 512-point)
void fft512(const float *input, float *magnitudes);

// Winamp visualization colors (24 entries)
extern QColor visColors[24];

// Load viscolor.txt if present
void loadVisColors(const QString &skinPath);

// Shared text.bmp character lookup (5x6 per char)
QPoint getTextCharPos(QChar ch);

// Detect whether a skin directory is a modern (XML) skin
bool isModernSkinDir(const QString &path);

// Config file path helper
QString configPath();

#endif // SKINUTILS_H

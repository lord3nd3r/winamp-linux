// modern_skin.h — Winamp 5 XML-based skin support
#pragma once

#include <QString>
#include <QMap>
#include <QImage>
#include <QPixmap>
#include <QXmlStreamReader>
#include <QFile>
#include <QDir>
#include <QSet>
#include <QDebug>
#include "constants.h"

// ============================================================
// Modern Skin Engine — Winamp 5 XML-based skin support
// Parses skin.xml with recursive <include>, loads bitmap sprite
// sheets, provides getBitmap(id) for rendering. Supports the
// full Wasabi skin XML format's <elements> section.
// ============================================================

struct ModernBitmapDef {
    QString file;
    int x = 0, y = 0, w = 0, h = 0;
    QString gammagroup;
};

struct ModernBitmapFontDef {
    QString bitmapId;  // references a <bitmap> id
    int charWidth = 0, charHeight = 0;
    int hSpacing = 0, vSpacing = 0;
};

class ModernSkinEngine {
public:
    bool loadSkin(const QString &dir) {
        skinDir = dir;
        bitmapDefs.clear();
        loadedBitmaps.clear();
        imageCache.clear();
        bitmapFonts.clear();
        skinName.clear();
        valid = false;

        QString skinXml = skinDir + "/skin.xml";
        if (!QFile::exists(skinXml)) {
            qDebug() << "ModernSkin: skin.xml not found at" << skinXml;
            return false;
        }

        qDebug() << "ModernSkin: parsing skin from" << skinDir;
        QSet<QString> visited;
        parseFile(skinXml, 0, visited);
        qDebug() << "ModernSkin: parsed" << bitmapDefs.size() << "bitmap defs," << bitmapFonts.size() << "fonts";
        loadAllBitmaps();
        qDebug() << "ModernSkin: loaded" << loadedBitmaps.size() << "bitmaps from" << imageCache.size() << "image files";
        
        // Debug: report any bitmap defs that failed to load
        int missing = 0;
        for (auto it = bitmapDefs.constBegin(); it != bitmapDefs.constEnd(); ++it) {
            if (!loadedBitmaps.contains(it.key()) && !it.value().file.isEmpty()) {
                if (missing < 10) qDebug() << "  MISSING:" << it.key() << "file:" << it.value().file;
                missing++;
            }
        }
        if (missing > 10) qDebug() << "  ... and" << (missing - 10) << "more missing";
        
        valid = !loadedBitmaps.isEmpty();
        return valid;
    }

    QPixmap getBitmap(const QString &id) const {
        return loadedBitmaps.value(id);
    }

    bool hasBitmap(const QString &id) const {
        return loadedBitmaps.contains(id);
    }

    bool isValid() const { return valid; }
    QString getSkinName() const { return skinName; }

    // Map a character to (x, y) source coordinates in a bitmap font image.
    // Follows the Wasabi BitmapFont::getXYfromChar layout:
    //   Row 0: A-Z (pos 0-25), " (26), @ (27), space (30)
    //   Row 1: 0-9 (pos 0-9), \1 (10), . (11), : (12), ( (13), ) (14),
    //          - (15), ' (16), ! (17), _ (18), + (19), \ (20), / (21),
    //          [ (22), ] (23), ~ (24), & (25), % (26), , (27), = (28),
    //          $ (29), # (30)
    //   Row 2: ? (3), * (4)
    static void getXYfromChar(QChar qch, int charWidth, int charHeight, int *outX, int *outY) {
        int c = 30; // default = space position
        int row = 0;
        wchar_t ic = qch.unicode();

        // Accent folding (subset matching Wasabi)
        switch (ic) {
            case 0x00B0: ic = L'0'; break;
            case 0x00C6: case 0x00C1: case 0x00C2: ic = L'A'; break;
            case 0x00C7: ic = L'C'; break;
            case 0x00C9: ic = L'E'; break;
            case 0x00D1: ic = L'N'; break;
            case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E6: ic = L'a'; break;
            case 0x00E7: ic = L'c'; break;
            case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB: ic = L'e'; break;
            case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF: ic = L'i'; break;
            case 0x00F1: ic = L'n'; break;
            case 0x00F2: case 0x00F3: case 0x00F4: ic = L'o'; break;
            case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC: ic = L'u'; break;
            case 0x00FD: ic = L'y'; break;
            case 0x00DC: ic = L'U'; break;
            case 0x0192: ic = L'f'; break;
            default: break;
        }

        if (ic >= L'A' && ic <= L'Z') {
            c = ic - L'A'; row = 0;
        } else if (ic >= L'a' && ic <= L'z') {
            c = ic - L'a'; row = 0;
        } else if (ic == L' ') {
            c = 30; row = 0;
        } else if (ic == L'"') {
            c = 26; row = 0;
        } else if (ic == L'@') {
            c = 27; row = 0;
        } else if (ic >= L'0' && ic <= L'9') {
            c = ic - L'0'; row = 1;
        } else if (ic == L'\1') { c = 10; row = 1; }
        else if (ic == L'.') { c = 11; row = 1; }
        else if (ic == L':') { c = 12; row = 1; }
        else if (ic == L'(') { c = 13; row = 1; }
        else if (ic == L')') { c = 14; row = 1; }
        else if (ic == L'-') { c = 15; row = 1; }
        else if (ic == L'\'' || ic == L'`') { c = 16; row = 1; }
        else if (ic == L'!') { c = 17; row = 1; }
        else if (ic == L'_') { c = 18; row = 1; }
        else if (ic == L'+') { c = 19; row = 1; }
        else if (ic == L'\\') { c = 20; row = 1; }
        else if (ic == L'/') { c = 21; row = 1; }
        else if (ic == L'[' || ic == L'{' || ic == L'<') { c = 22; row = 1; }
        else if (ic == L']' || ic == L'}' || ic == L'>') { c = 23; row = 1; }
        else if (ic == L'~' || ic == L'^') { c = 24; row = 1; }
        else if (ic == L'&') { c = 25; row = 1; }
        else if (ic == L'%') { c = 26; row = 1; }
        else if (ic == L',') { c = 27; row = 1; }
        else if (ic == L'=') { c = 28; row = 1; }
        else if (ic == L'$') { c = 29; row = 1; }
        else if (ic == L'#') { c = 30; row = 1; }
        else if (ic == L'?') { c = 3; row = 2; }
        else if (ic == L'*') { c = 4; row = 2; }
        else { c = 30; row = 0; } // fallback to space

        *outX = c * charWidth;
        *outY = row * charHeight;
    }

    // Draw text using a skin bitmap font (e.g. player.BIGNUM, player.songticker.font)
    void drawBitmapText(QPainter &p, const QString &fontId, const QString &text,
                        int x, int y, int maxWidth = -1) const {
        auto it = bitmapFonts.constFind(fontId);
        if (it == bitmapFonts.constEnd()) return;

        const ModernBitmapFontDef &font = it.value();
        QPixmap fontBitmap = getBitmap(font.bitmapId);
        if (fontBitmap.isNull()) return;

        int cx = x;

        for (const QChar &ch : text) {
            int srcX, srcY;
            getXYfromChar(ch, font.charWidth, font.charHeight, &srcX, &srcY);

            if (maxWidth > 0 && (cx - x + font.charWidth) > maxWidth) break;

            // Bounds check against bitmap dimensions
            if (srcX + font.charWidth <= fontBitmap.width() &&
                srcY + font.charHeight <= fontBitmap.height()) {
                p.drawPixmap(cx, y, fontBitmap, srcX, srcY, font.charWidth, font.charHeight);
            }
            cx += font.charWidth + font.hSpacing;
        }
    }

    int measureText(const QString &fontId, const QString &text) const {
        auto it = bitmapFonts.constFind(fontId);
        if (it == bitmapFonts.constEnd()) return text.length() * 8;
        const ModernBitmapFontDef &font = it.value();
        return text.length() * (font.charWidth + font.hSpacing);
    }

    int fontHeight(const QString &fontId) const {
        auto it = bitmapFonts.constFind(fontId);
        if (it == bitmapFonts.constEnd()) return 10;
        return it.value().charHeight;
    }

private:
    static constexpr int kMaxIncludeDepth = 10;

    void parseFile(const QString &filePath, int depth, QSet<QString> &visited) {
        // Guard against excessive include depth (malicious/malformed skins)
        if (depth > kMaxIncludeDepth) {
            qWarning() << "ModernSkin: include depth limit exceeded at" << filePath;
            return;
        }

        // Case-insensitive file open for Linux
        QString resolved = resolveCasePath(filePath);
        QString canonical = QFileInfo(resolved).canonicalFilePath();
        if (canonical.isEmpty()) canonical = resolved;

        // Guard against circular includes
        if (visited.contains(canonical)) {
            qWarning() << "ModernSkin: circular include detected, skipping" << filePath;
            return;
        }
        visited.insert(canonical);

        QFile file(resolved);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

        QXmlStreamReader xml(&file);
        QString baseDir = QFileInfo(resolved).absolutePath();

        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                if (xml.name() == u"include") {
                    QString includeFile = xml.attributes().value("file").toString();
                    if (!includeFile.isEmpty()) {
                        // Resolve include path case-insensitively
                        QString includePath = resolveCasePath(baseDir + "/" + includeFile);
                        parseFile(includePath, depth + 1, visited);
                    }
                } else if (xml.name() == u"skininfo") {
                    while (!(xml.isEndElement() && xml.name() == u"skininfo") && !xml.atEnd()) {
                        xml.readNext();
                        if (xml.isStartElement() && xml.name() == u"name") {
                            skinName = xml.readElementText();
                        }
                    }
                } else if (xml.name() == u"bitmap") {
                    parseBitmapElement(xml);
                } else if (xml.name() == u"bitmapfont") {
                    parseBitmapFontElement(xml);
                }
            }
        }

        if (xml.hasError()) {
            qWarning() << "ModernSkin: XML parse error in" << filePath << ":" << xml.errorString();
        }
    }

    void parseBitmapElement(QXmlStreamReader &xml) {
        QXmlStreamAttributes attrs = xml.attributes();
        QString id = attrs.value("id").toString();
        if (id.isEmpty()) return;

        ModernBitmapDef def;
        def.file = attrs.value("file").toString();
        def.x = attrs.value("x").toInt();
        def.y = attrs.value("y").toInt();
        def.w = attrs.value("w").toInt();
        def.h = attrs.value("h").toInt();
        def.gammagroup = attrs.value("gammagroup").toString();

        bitmapDefs[id] = def;
    }

    void parseBitmapFontElement(QXmlStreamReader &xml) {
        QXmlStreamAttributes attrs = xml.attributes();
        QString id = attrs.value("id").toString();
        if (id.isEmpty()) return;

        ModernBitmapFontDef font;
        font.bitmapId = attrs.value("file").toString(); // references a bitmap id
        font.charWidth = attrs.value("charwidth").toInt();
        font.charHeight = attrs.value("charheight").toInt();
        font.hSpacing = attrs.value("hspacing").toInt();
        font.vSpacing = attrs.value("vspacing").toInt();

        bitmapFonts[id] = font;
    }

    // Case-insensitive file lookup for Linux (Winamp skins come from Windows with mixed case)
    static QString resolveCasePath(const QString &fullPath) {
        if (QFile::exists(fullPath)) return fullPath;

        // Split into directory and filename parts, do case-insensitive match
        QFileInfo fi(fullPath);
        QString dirPath = fi.absolutePath();
        QString fileName = fi.fileName();

        // First resolve the directory case-insensitively
        QDir dir(dirPath);
        if (!dir.exists()) {
            QFileInfo dirFi(dirPath);
            QDir parentDir(dirFi.absolutePath());
            QStringList dirs = parentDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &d : dirs) {
                if (d.compare(dirFi.fileName(), Qt::CaseInsensitive) == 0) {
                    dir = QDir(parentDir.absolutePath() + "/" + d);
                    break;
                }
            }
        }

        if (dir.exists()) {
            QStringList entries = dir.entryList(QDir::Files);
            for (const QString &entry : entries) {
                if (entry.compare(fileName, Qt::CaseInsensitive) == 0)
                    return dir.absolutePath() + "/" + entry;
            }
        }
        return fullPath; // Return original path as fallback
    }

    QString resolveFilePath(const QString &relPath) const {
        return resolveCasePath(skinDir + "/" + relPath);
    }

    void loadAllBitmaps() {
        for (auto it = bitmapDefs.constBegin(); it != bitmapDefs.constEnd(); ++it) {
            const ModernBitmapDef &def = it.value();
            if (def.file.isEmpty()) continue;

            // Load the full image file (cached per path, case-insensitive)
            QString fullPath = resolveFilePath(def.file);
            if (!imageCache.contains(fullPath)) {
                QImage img(fullPath);
                if (!img.isNull()) {
                    imageCache[fullPath] = img;
                }
            }

            const QImage &srcImage = imageCache.value(fullPath);
            if (srcImage.isNull()) continue;

            // Extract sub-rectangle from sprite sheet
            int sx = def.x, sy = def.y;
            int sw = def.w > 0 ? def.w : srcImage.width();
            int sh = def.h > 0 ? def.h : srcImage.height();

            // Clamp to image bounds
            sw = qMin(sw, srcImage.width() - sx);
            sh = qMin(sh, srcImage.height() - sy);

            if (sw > 0 && sh > 0 && sx >= 0 && sy >= 0) {
                loadedBitmaps[it.key()] = QPixmap::fromImage(srcImage.copy(sx, sy, sw, sh));
            }
        }
    }

    QString skinDir;
    QString skinName;
    bool valid = false;

    QMap<QString, ModernBitmapDef> bitmapDefs;
    QMap<QString, QPixmap> loadedBitmaps;
    QMap<QString, QImage> imageCache;
    QMap<QString, ModernBitmapFontDef> bitmapFonts;
};

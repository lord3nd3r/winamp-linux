#ifndef MODERNSKINENGINE_H
#define MODERNSKINENGINE_H

#include <QString>
#include <QMap>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QXmlStreamReader>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QColor>

struct ModernBitmapDef {
    QString file;
    int x = 0, y = 0, w = 0, h = 0;
    QString gammagroup;
    QString baseDir;    // Directory of the XML file that defined this bitmap
    QColor solidColor;  // For $solid pseudo-bitmaps
};

struct ModernBitmapFontDef {
    QString bitmapId;  // references a <bitmap> id
    int charWidth = 0, charHeight = 0;
    int hSpacing = 0, vSpacing = 0;
};

class ModernSkinEngine {
public:
    bool loadSkin(const QString &dir);
    QPixmap getBitmap(const QString &id) const;
    bool hasBitmap(const QString &id) const;
    bool isValid() const { return valid; }
    QString getSkinName() const { return skinName; }

    QString resolveFontId(const QString &fontId) const;

    // Map a character to (x, y) source coordinates in a bitmap font image.
    static void getXYfromChar(QChar qch, int charWidth, int charHeight, int *outX, int *outY);

    // Draw text using a skin bitmap font
    void drawBitmapText(QPainter &p, const QString &fontId, const QString &text,
                        int x, int y, int maxWidth = -1) const;

    int measureText(const QString &fontId, const QString &text) const;
    int fontHeight(const QString &fontId) const;

private:
    static QString bentoAlias(const QString &id);
    static QString bentoFontAlias(const QString &fontId);
    void parseFile(const QString &filePath);
    void parseBitmapElement(QXmlStreamReader &xml, const QString &skinRootDir);
    void parseBitmapFontElement(QXmlStreamReader &xml);
    QString findSkinRoot(const QString &xmlFilePath) const;
    static QString resolveCasePath(const QString &fullPath);
    static QString resolveCasePathDeep(const QString &fullPath);
    QString resolveFilePath(const QString &relPath, const QString &bitmapSkinRoot) const;
    void loadAllBitmaps();

    QString skinDir;
    QString skinName;
    bool valid = false;

    QMap<QString, ModernBitmapDef> bitmapDefs;
    QMap<QString, QPixmap> loadedBitmaps;
    QMap<QString, QImage> imageCache;
    QMap<QString, ModernBitmapFontDef> bitmapFonts;
};

// Global modern skin state
extern bool g_isModernSkin;
extern ModernSkinEngine *g_modernSkin;

#endif // MODERNSKINENGINE_H

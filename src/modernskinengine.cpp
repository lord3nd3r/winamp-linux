#include "modernskinengine.h"

// Global modern skin state
bool g_isModernSkin = false;
ModernSkinEngine *g_modernSkin = nullptr;

bool ModernSkinEngine::loadSkin(const QString &dir) {
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
    parseFile(skinXml);
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

QPixmap ModernSkinEngine::getBitmap(const QString &id) const {
    if (loadedBitmaps.contains(id)) return loadedBitmaps.value(id);
    // Try Bento-style alias (handles Big Bento / Bento skins that use
    // different bitmap ID naming conventions from Winamp Modern)
    QString alias = bentoAlias(id);
    if (!alias.isEmpty() && loadedBitmaps.contains(alias))
        return loadedBitmaps.value(alias);
    return QPixmap();
}

bool ModernSkinEngine::hasBitmap(const QString &id) const {
    if (loadedBitmaps.contains(id)) return true;
    QString alias = bentoAlias(id);
    return !alias.isEmpty() && loadedBitmaps.contains(alias);
}

QString ModernSkinEngine::resolveFontId(const QString &fontId) const {
    if (bitmapFonts.contains(fontId)) return fontId;
    QString alias = bentoFontAlias(fontId);
    if (!alias.isEmpty() && bitmapFonts.contains(alias)) return alias;
    return fontId;
}

void ModernSkinEngine::getXYfromChar(QChar qch, int charWidth, int charHeight, int *outX, int *outY) {
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

void ModernSkinEngine::drawBitmapText(QPainter &p, const QString &fontId, const QString &text,
                                       int x, int y, int maxWidth) const {
    QString resolved = resolveFontId(fontId);
    auto it = bitmapFonts.constFind(resolved);
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

int ModernSkinEngine::measureText(const QString &fontId, const QString &text) const {
    QString resolved = resolveFontId(fontId);
    auto it = bitmapFonts.constFind(resolved);
    if (it == bitmapFonts.constEnd()) return text.length() * 8;
    const ModernBitmapFontDef &font = it.value();
    return text.length() * (font.charWidth + font.hSpacing);
}

int ModernSkinEngine::fontHeight(const QString &fontId) const {
    QString resolved = resolveFontId(fontId);
    auto it = bitmapFonts.constFind(resolved);
    if (it == bitmapFonts.constEnd()) return 10;
    return it.value().charHeight;
}

QString ModernSkinEngine::bentoAlias(const QString &id) {
    // Build a static mapping table on first use
    static const QMap<QString, QString> map = {
        // === Frame / Titlebar ===
        {"wasabi.frame.basetexture",          "window.background.center"},
        {"wasabi.frame.top",                  "window.background.top"},
        {"wasabi.frame.top.left",             "window.background.topleft"},
        {"wasabi.frame.top.right",            "window.background.topright"},
        {"wasabi.titlebar.left.active",       "window.titlebar.grid.left"},
        {"wasabi.titlebar.center.active",     "window.titlebar.grid.middle"},
        {"wasabi.titlebar.right.active",      "window.titlebar.grid.right"},
        {"wasabi.titlebar.left.inactive",     "window.titlebar.grid.left"},
        {"wasabi.titlebar.center.inactive",   "window.titlebar.grid.middle"},
        {"wasabi.titlebar.right.inactive",    "window.titlebar.grid.right"},
        // Window buttons
        {"wasabi.button.minimize",            "window.titlebar.button.minimize.normal"},
        {"wasabi.button.minimize.hover",      "window.titlebar.button.minimize.hover"},
        {"wasabi.button.minimize.pressed",    "window.titlebar.button.minimize.down"},
        {"wasabi.button.exit",                "window.titlebar.button.close.normal"},
        {"wasabi.button.exit.hover",          "window.titlebar.button.close.hover"},
        {"wasabi.button.exit.pressed",        "window.titlebar.button.close.down"},

        // === Display ===
        {"player.display.bg.left",            "player.display.background.left"},
        {"player.display.bg.center",          "player.display.background.center"},
        {"player.display.bg.right",           "player.display.background.right"},
        {"player.display.left",               "player.display.foreground.left"},
        {"player.display.center",             "player.display.foreground.center"},
        {"player.display.right",              "player.display.foreground.right"},

        // === Status icons ===
        {"player.status.play",                "player.display.status.playing"},
        {"player.status.pause",               "player.display.status.paused"},
        {"player.status.stop",                "player.display.status.stopped"},

        // === Song info ===
        {"player.songinfo.kbps",              "player.songinfo.bitrate"},
        {"player.songinfo.khz",               "player.songinfo.frequency"},
        {"player.songinfo.none",              "player.songinfo.na"},

        // === Seek bar ===
        {"player.seekbar.left",               "player.slider.background.left"},
        {"player.seekbar.center",             "player.slider.background.center"},
        {"player.seekbar.right",              "player.slider.background.right"},
        {"player.button.seek",                "player.posbar.thumb.normal"},
        {"player.button.seek.hover",          "player.posbar.thumb.hover"},
        {"player.button.seek.pressed",        "player.posbar.thumb.down"},

        // === Playback buttons (bare name -> .normal, .pressed -> .down) ===
        {"player.button.previous",            "player.button.previous.normal"},
        {"player.button.previous.pressed",    "player.button.previous.down"},
        {"player.button.previous.bg",         "player.button.previous.placeholder"},
        {"player.button.play",                "player.button.play.normal"},
        {"player.button.play.pressed",        "player.button.play.down"},
        {"player.button.play.bg",             "player.button.pps.glow"},
        {"player.button.pause",               "player.button.pause.normal"},
        {"player.button.pause.pressed",       "player.button.pause.down"},
        {"player.button.pause.bg",            "player.button.pps.glow"},
        {"player.button.stop",                "player.button.stop.normal"},
        {"player.button.stop.pressed",        "player.button.stop.down"},
        {"player.button.stop.bg",             "player.button.pps.glow"},
        {"player.button.next",                "player.button.next.normal"},
        {"player.button.next.pressed",        "player.button.next.down"},
        {"player.button.next.bg",             "player.button.next.placeholder"},
        {"player.button.eject",               "player.button.eject.normal"},
        {"player.button.eject.pressed",       "player.button.eject.down"},

        // === Volume ===
        {"player.button.volume",              "player.volume.thumb.normal"},
        {"player.button.volume.pressed",      "player.volume.thumb.down"},
        {"player.button.mute.bg",             "player.button.mute.placeholder"},
        {"player.button.mute.on",             "player.button.mute.active"},
        {"player.button.mute.on.pressed",     "player.button.mute.down"},
        {"player.button.mute.off",            "player.button.demute.normal"},
        {"player.button.mute.off.pressed",    "player.button.demute.down"},

        // === Repeat / Shuffle ===
        {"player.button.repeat",              "player.button.repeat.normal0"},
        {"player.button.repeat.hover",        "player.button.repeat.hover0"},
        {"player.button.repeat.pressed",      "player.button.repeat.down0"},
        {"player.button.repeat.bg",           "player.button.repeat.glow"},
        {"player.button.shuffle",             "player.button.shuffle.normal0"},
        {"player.button.shuffle.hover",       "player.button.shuffle.hover0"},
        {"player.button.shuffle.pressed",     "player.button.shuffle.down0"},
        {"player.button.shuffle.bg",          "player.button.shuffle.glow"},

        // === Bolt ===
        {"player.button.bolt",                "player.button.bolt.normal"},
        {"player.button.bolt.bg",             "player.button.bolt.glow"},

        // === Resizer ===
        {"player.resizer",                    "window.background.resizer"},

        // === Main area (use window background as fallback) ===
        {"player.main.left",                  "window.background.left"},
        {"player.main.center",                "window.background.center"},
        {"player.main.right",                 "window.background.right"},
    };
    return map.value(id);
}

QString ModernSkinEngine::bentoFontAlias(const QString &fontId) {
    static const QMap<QString, QString> map = {
        {"player.BIGNUM",           "player.bitmapfont.nums"},
        {"player.songticker.font",  "player.bitmapfont.songinfo"},
        {"player.songinfo.font",    "player.bitmapfont.songinfo"},
        {"player.pe.time.font",     "player.bitmapfont.songinfo"},
    };
    return map.value(fontId);
}

void ModernSkinEngine::parseFile(const QString &filePath) {
    // Case-insensitive file open for Linux
    QString resolved = resolveCasePathDeep(filePath);
    QFile file(resolved);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QXmlStreamReader xml(&file);
    QString baseDir = QFileInfo(resolved).absolutePath();

    // Determine the skin root for this XML file.
    // Wasabi resolves bitmap file= paths relative to the skin root (the
    // directory containing skin.xml), NOT relative to the XML file itself.
    // When Bento includes ../Big Bento/xml/system-elements.xml, those
    // bitmaps with file="window/scrollbars.png" must resolve against
    // Big Bento's root, not Bento's.
    QString rootDir = findSkinRoot(resolved);

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == u"include") {
                QString includeFile = xml.attributes().value("file").toString();
                if (!includeFile.isEmpty()) {
                    // Normalize Windows backslashes
                    includeFile.replace('\\', '/');
                    // Resolve include path case-insensitively
                    QString includePath = resolveCasePathDeep(baseDir + "/" + includeFile);
                    parseFile(includePath);
                }
            } else if (xml.name() == u"skininfo") {
                while (!(xml.isEndElement() && xml.name() == u"skininfo") && !xml.atEnd()) {
                    xml.readNext();
                    if (xml.isStartElement() && xml.name() == u"name") {
                        skinName = xml.readElementText();
                    }
                }
            } else if (xml.name() == u"bitmap") {
                parseBitmapElement(xml, rootDir);
            } else if (xml.name() == u"bitmapfont") {
                parseBitmapFontElement(xml);
            }
        }
    }
}

void ModernSkinEngine::parseBitmapElement(QXmlStreamReader &xml, const QString &skinRootDir) {
    QXmlStreamAttributes attrs = xml.attributes();
    QString id = attrs.value("id").toString();
    if (id.isEmpty()) return;

    ModernBitmapDef def;
    def.file = attrs.value("file").toString();
    // Normalize Windows backslashes to forward slashes
    def.file.replace('\\', '/');
    def.x = attrs.value("x").toInt();
    def.y = attrs.value("y").toInt();
    def.w = attrs.value("w").toInt();
    def.h = attrs.value("h").toInt();
    def.gammagroup = attrs.value("gammagroup").toString();
    // Track the skin root directory for this bitmap definition.
    // Wasabi resolves bitmap file= paths relative to the skin root (the
    // directory containing skin.xml). When Bento includes XMLs from
    // ../Big Bento/, the rootDir for those bitmaps is Big Bento's root,
    // so file="window/scrollbars.png" resolves to Big Bento/window/scrollbars.png.
    def.baseDir = skinRootDir;

    // Handle $solid pseudo-bitmaps (synthetic solid-color rectangles)
    if (def.file == "$solid") {
        QString colorStr = attrs.value("color").toString();
        QStringList parts = colorStr.split(',');
        if (parts.size() >= 3) {
            def.solidColor = QColor(parts[0].trimmed().toInt(),
                                    parts[1].trimmed().toInt(),
                                    parts[2].trimmed().toInt());
        } else {
            def.solidColor = QColor(0, 0, 0);
        }
    }

    bitmapDefs[id] = def;
}

void ModernSkinEngine::parseBitmapFontElement(QXmlStreamReader &xml) {
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

QString ModernSkinEngine::findSkinRoot(const QString &xmlFilePath) const {
    QString dir = QFileInfo(xmlFilePath).absolutePath();
    // Walk up at most 5 levels
    for (int i = 0; i < 5; i++) {
        if (QFile::exists(dir + "/skin.xml")) return dir;
        // Also check case-insensitively
        QDir d(dir);
        QStringList entries = d.entryList(QDir::Files);
        for (const QString &e : entries) {
            if (e.compare("skin.xml", Qt::CaseInsensitive) == 0)
                return dir;
        }
        QString parent = QFileInfo(dir).absolutePath();
        if (parent == dir) break; // reached filesystem root
        dir = parent;
    }
    return skinDir; // fallback
}

QString ModernSkinEngine::resolveCasePath(const QString &fullPath) {
    if (QFile::exists(fullPath)) return fullPath;

    QFileInfo fi(fullPath);
    QString dirPath = fi.absolutePath();
    QString fileName = fi.fileName();

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
    return fullPath;
}

QString ModernSkinEngine::resolveCasePathDeep(const QString &fullPath) {
    if (QFile::exists(fullPath)) return fullPath;

    // Canonicalize: resolve ".." segments first
    QString cleaned = QDir::cleanPath(fullPath);
    if (QFile::exists(cleaned)) return cleaned;

    // Split into components. We walk from root down, resolving each.
    QStringList components = cleaned.split('/');
    if (components.isEmpty()) return fullPath;

    // Start from root
    QString resolved;
    if (cleaned.startsWith('/')) {
        resolved = "/";
        components.removeFirst(); // remove empty first element
    }

    for (int i = 0; i < components.size(); i++) {
        const QString &comp = components[i];
        if (comp.isEmpty()) continue;

        QString candidate = resolved.isEmpty() ? comp : (resolved.endsWith('/') ? resolved + comp : resolved + "/" + comp);

        if (QFileInfo(candidate).exists()) {
            resolved = candidate;
        } else {
            // Case-insensitive search in parent directory
            QDir parent(resolved);
            if (!parent.exists()) return fullPath; // parent doesn't exist, give up

            bool found = false;
            // Check both dirs and files
            QStringList entries = parent.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
            for (const QString &entry : entries) {
                if (entry.compare(comp, Qt::CaseInsensitive) == 0) {
                    resolved = resolved.endsWith('/') ? resolved + entry : resolved + "/" + entry;
                    found = true;
                    break;
                }
            }
            if (!found) return fullPath; // component not found
        }
    }

    return resolved;
}

QString ModernSkinEngine::resolveFilePath(const QString &relPath, const QString &bitmapSkinRoot) const {
    // First try resolving against the bitmap's skin root
    if (!bitmapSkinRoot.isEmpty()) {
        QString path = resolveCasePathDeep(bitmapSkinRoot + "/" + relPath);
        if (QFile::exists(path)) return path;
    }
    // Fallback: resolve against the loading skin's skinDir
    QString path = resolveCasePathDeep(skinDir + "/" + relPath);
    if (QFile::exists(path)) return path;
    // Last resort: old single-level resolution
    return resolveCasePath(skinDir + "/" + relPath);
}

void ModernSkinEngine::loadAllBitmaps() {
    for (auto it = bitmapDefs.constBegin(); it != bitmapDefs.constEnd(); ++it) {
        const ModernBitmapDef &def = it.value();

        // Handle $solid pseudo-bitmaps — generate a solid color rectangle
        if (def.file == "$solid") {
            int w = def.w > 0 ? def.w : 1;
            int h = def.h > 0 ? def.h : 1;
            QPixmap solidPx(w, h);
            solidPx.fill(def.solidColor.isValid() ? def.solidColor : QColor(0, 0, 0));
            loadedBitmaps[it.key()] = solidPx;
            continue;
        }

        if (def.file.isEmpty() || def.file.startsWith('$')) continue;

        // Load the full image file (cached per path, case-insensitive)
        // Use the bitmap's own baseDir for resolution (critical for Bento
        // skins where included XMLs from ../Big Bento/ define bitmaps
        // with paths relative to Big Bento, not the loading skin)
        QString fullPath = resolveFilePath(def.file, def.baseDir);
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

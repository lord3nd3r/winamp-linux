#include "winampbitmaps.h"
#include "skinutils.h"

bool WinampBitmaps::loadAll(const QString &resourcePath) {
    basePath = resourcePath;
    
    // Helper: try loading a pixmap with case-insensitive fallback
    auto loadBmp = [&](const QString &name) -> QPixmap {
        QPixmap pm(basePath + "/" + name);
        if (!pm.isNull()) return pm;
        // Try uppercase/lowercase variants
        pm = QPixmap(basePath + "/" + name.toUpper());
        if (!pm.isNull()) return pm;
        pm = QPixmap(basePath + "/" + name.toLower());
        return pm;
    };
    
    // Load all classic Winamp bitmaps
    main = loadBmp("MAIN.BMP");
    cbuttons = loadBmp("CBUTTONS.BMP");
    titlebar = loadBmp("titlebar.bmp");
    numbers = loadBmp("numbers.bmp");
    numbers_ex = loadBmp("nums_ex.bmp");  // Extended numbers with animated colon (optional)
    text = loadBmp("text.bmp");
    playpaus = loadBmp("PLAYPAUS.BMP");
    monoster = loadBmp("MONOSTER.BMP");
    posbar = loadBmp("POSBAR.BMP");
    volume = loadBmp("volume.bmp");
    balance = loadBmp("BALANCE.BMP");
    shufrep = loadBmp("SHUFREP.BMP");
    eqmain = loadBmp("Eqmain.bmp");
    pledit = loadBmp("Pledit.bmp");
    
    // Load custom visualization colors if present (Windows viscolor.txt format)
    loadVisColors(basePath);
    
    return !main.isNull();
}

void WinampBitmaps::loadMissing(const QString &fallbackPath) {
    auto tryLoad = [&](QPixmap &pm, const QString &name) {
        if (pm.isNull()) {
            pm = QPixmap(fallbackPath + "/" + name);
            if (pm.isNull()) pm = QPixmap(fallbackPath + "/" + name.toUpper());
            if (pm.isNull()) pm = QPixmap(fallbackPath + "/" + name.toLower());
        }
    };
    tryLoad(main, "MAIN.BMP");
    tryLoad(cbuttons, "CBUTTONS.BMP");
    tryLoad(titlebar, "titlebar.bmp");
    tryLoad(numbers, "numbers.bmp");
    tryLoad(numbers_ex, "nums_ex.bmp");
    tryLoad(text, "text.bmp");
    tryLoad(playpaus, "PLAYPAUS.BMP");
    tryLoad(monoster, "MONOSTER.BMP");
    tryLoad(posbar, "POSBAR.BMP");
    tryLoad(volume, "volume.bmp");
    tryLoad(balance, "BALANCE.BMP");
    tryLoad(shufrep, "SHUFREP.BMP");
    tryLoad(eqmain, "Eqmain.bmp");
    tryLoad(pledit, "Pledit.bmp");
}

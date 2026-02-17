#ifndef WINAMPBITMAPS_H
#define WINAMPBITMAPS_H

#include <QPixmap>
#include <QString>
#include <QFile>
#include <QDir>

class WinampBitmaps {
public:
    static WinampBitmaps& instance() {
        static WinampBitmaps inst;
        return inst;
    }
    
    bool loadAll(const QString &resourcePath);
    void loadMissing(const QString &fallbackPath);
    
    QPixmap main, cbuttons, titlebar, numbers, numbers_ex, text;
    QPixmap playpaus, monoster, posbar, volume, balance, shufrep;
    QPixmap eqmain, pledit;
    QString basePath;
    
private:
    WinampBitmaps() {}
};

#endif // WINAMPBITMAPS_H

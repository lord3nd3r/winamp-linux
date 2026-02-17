#include "skinutils.h"
#include <cmath>

// Global skin playlist colors
SkinPlaylistColors g_plColors;

QString extractSkinArchive(const QString &archivePath) {
    QFileInfo fi(archivePath);
    if (!fi.exists()) return {};
    
    QString cacheDirBase = QDir::homePath() + "/.cache/winamp/skins";
    QString skinName = fi.completeBaseName();
    QString extractDir = cacheDirBase + "/" + skinName;
    QDir().mkpath(extractDir);
    
    QDir ed(extractDir);
    QStringList bmps = ed.entryList(QStringList() << "*.bmp" << "*.BMP", QDir::Files);
    if (!bmps.isEmpty()) {
        return extractDir;
    }
    
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

SkinPlaylistColors parsePleditTxt(const QString &skinPath) {
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
                if (line.startsWith("Normal=", Qt::CaseInsensitive)) {
                    line = line.mid(line.indexOf('=') + 1).trimmed();
                    if (line.startsWith('#')) colors.normal = QColor(line);
                    else { QStringList p = line.split(','); if (p.size()==3) colors.normal = QColor(p[0].toInt(), p[1].toInt(), p[2].toInt()); }
                } else if (line.startsWith("Current=", Qt::CaseInsensitive)) {
                    line = line.mid(line.indexOf('=') + 1).trimmed();
                    if (line.startsWith('#')) colors.current = QColor(line);
                    else { QStringList p = line.split(','); if (p.size()==3) colors.current = QColor(p[0].toInt(), p[1].toInt(), p[2].toInt()); }
                } else if (line.startsWith("NormalBG=", Qt::CaseInsensitive)) {
                    line = line.mid(line.indexOf('=') + 1).trimmed();
                    if (line.startsWith('#')) colors.normBg = QColor(line);
                    else { QStringList p = line.split(','); if (p.size()==3) colors.normBg = QColor(p[0].toInt(), p[1].toInt(), p[2].toInt()); }
                } else if (line.startsWith("SelectedBG=", Qt::CaseInsensitive)) {
                    line = line.mid(line.indexOf('=') + 1).trimmed();
                    if (line.startsWith('#')) colors.selectBg = QColor(line);
                    else { QStringList p = line.split(','); if (p.size()==3) colors.selectBg = QColor(p[0].toInt(), p[1].toInt(), p[2].toInt()); }
                } else if (line.startsWith("MbFG=", Qt::CaseInsensitive)) {
                    line = line.mid(line.indexOf('=') + 1).trimmed();
                    if (line.startsWith('#')) colors.mbFg = QColor(line);
                    else { QStringList p = line.split(','); if (p.size()==3) colors.mbFg = QColor(p[0].toInt(), p[1].toInt(), p[2].toInt()); }
                } else if (line.startsWith("MbBG=", Qt::CaseInsensitive)) {
                    line = line.mid(line.indexOf('=') + 1).trimmed();
                    if (line.startsWith('#')) colors.mbBg = QColor(line);
                    else { QStringList p = line.split(','); if (p.size()==3) colors.mbBg = QColor(p[0].toInt(), p[1].toInt(), p[2].toInt()); }
                }
            }
            file.close();
            break;
        }
    }
    return colors;
}

QColor visColors[24] = {
    QColor(0, 0, 0),
    QColor(24, 33, 41),
    QColor(239, 49, 16),
    QColor(206, 41, 16),
    QColor(214, 90, 0),
    QColor(214, 102, 0),
    QColor(214, 115, 0),
    QColor(198, 123, 8),
    QColor(222, 165, 24),
    QColor(214, 181, 33),
    QColor(189, 222, 41),
    QColor(148, 222, 33),
    QColor(41, 206, 16),
    QColor(50, 190, 16),
    QColor(57, 181, 16),
    QColor(49, 156, 8),
    QColor(41, 148, 0),
    QColor(24, 132, 8),
    QColor(255, 255, 255),
    QColor(214, 214, 222),
    QColor(181, 189, 189),
    QColor(160, 170, 175),
    QColor(148, 156, 165),
    QColor(150, 150, 150),
};

void loadVisColors(const QString &skinPath) {
    QFile file(skinPath + "/viscolor.txt");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        file.setFileName(skinPath + "/VISCOLOR.TXT");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    }
    
    QTextStream in(&file);
    int idx = 0;
    while (!in.atEnd() && idx < 24) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("#") || line.startsWith(";")) continue;
        
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

void fft512(const float *input, float *magnitudes) {
    const int N = 512;
    float re[N], im[N];
    for (int i = 0; i < N; i++) {
        int j = 0;
        for (int b = 0; b < 9; b++)
            j |= ((i >> b) & 1) << (8 - b);
        re[j] = input[i];
        im[j] = 0.0f;
    }
    for (int s = 1; s <= 9; s++) {
        int m = 1 << s;
        int m2 = m >> 1;
        float wRe = cosf(-2.0f * M_PI / m);
        float wIm = sinf(-2.0f * M_PI / m);
        for (int k = 0; k < N; k += m) {
            float tRe = 1.0f, tIm = 0.0f;
            for (int j = 0; j < m2; j++) {
                float uRe = re[k + j], uIm = im[k + j];
                float vRe = tRe * re[k + j + m2] - tIm * im[k + j + m2];
                float vIm = tRe * im[k + j + m2] + tIm * re[k + j + m2];
                re[k + j] = uRe + vRe;
                im[k + j] = uIm + vIm;
                re[k + j + m2] = uRe - vRe;
                im[k + j + m2] = uIm - vIm;
                float newTRe = tRe * wRe - tIm * wIm;
                tIm = tRe * wIm + tIm * wRe;
                tRe = newTRe;
            }
        }
    }
    for (int i = 0; i < N / 2; i++) {
        magnitudes[i] = sqrtf(re[i] * re[i] + im[i] * im[i]);
    }
}

QPoint getTextCharPos(QChar ch) {
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

bool isModernSkinDir(const QString &path) {
    return QFile::exists(path + "/skin.xml");
}

QString configPath() {
    QString dir = QDir::homePath() + "/.config/winamp";
    QDir().mkpath(dir);
    return dir + "/winamp.conf";
}

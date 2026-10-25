// constants.h — Shared constants, helpers, and types for Winamp Linux
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QDir>
#include <QColor>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QUrl>
#include <QDebug>
#include <QCryptographicHash>
#include <QDirIterator>
#include <algorithm>
#include <cstring>
#include <utility>
#include <zlib.h>

// Product version — keep aligned with GitHub release tags (currently v1.2.2).
// CMake project(VERSION) and WINAMP_VERSION_FULL must match this label.
inline constexpr const char *kWinampVersion = "1.2.2";
inline constexpr const char *kWinampWindowTitle = "Winamp 1.2.2 for Linux";
inline constexpr const char *kWinampAboutLine = "Winamp v1.2.2 for Linux";

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

// Check if a path/URL points to a playlist reference (.pls/.m3u/.m3u8) rather than a
// directly playable stream. Feeding a playlist's raw text straight to the decoder
// (instead of parsing it for the real media URL) produces no audio: FFmpeg will
// happily misdetect the text as some other container (e.g. subtitle/lyrics) and
// "play" it with no audio stream at all.
static inline bool isPlaylistPath(const QString &pathOrUrl) {
    const QString lower = pathOrUrl.toLower();
    return lower.endsWith(".pls") || lower.endsWith(".m3u") || lower.endsWith(".m3u8");
}

// "File" or "FileN" (PLS). TitleN / LengthN / NumberOfEntries are not entries.
static inline bool playlistFileKey(const QString &key, int *index) {
    if (!key.startsWith(QLatin1String("file"), Qt::CaseInsensitive))
        return false;
    const QString rest = key.mid(4);
    if (rest.isEmpty()) {
        *index = 0;
        return true;
    }
    bool ok = false;
    const int n = rest.toInt(&ok);
    if (!ok || n < 0)
        return false;
    *index = n;
    return true;
}

// Resolve a playlist entry against the playlist's own location. `base` is the
// playlist file URL (remote) or the playlist's directory as a local-file URL
// with a trailing slash. Absolute URLs and absolute paths are left alone.
static inline QString resolvePlaylistLocation(const QString &raw, const QUrl &base) {
    QString value = raw.trimmed();
    value.remove(QLatin1Char('\r'));
    if (value.isEmpty())
        return QString();
    if (!value.contains(QLatin1String("://")) && value.contains(QLatin1Char('\\')))
        value.replace(QLatin1Char('\\'), QLatin1Char('/'));
    // QUrl::fromUserInput("b.mp3") becomes http://b.mp3, so only an explicit scheme
    // counts as a remote entry. Relative names are resolved against `base`.
    const QUrl explicitUrl(value);
    const QString scheme = explicitUrl.scheme().toLower();
    if (explicitUrl.isValid() && !scheme.isEmpty()
        && (scheme == QLatin1String("http") || scheme == QLatin1String("https")
            || scheme == QLatin1String("icy") || scheme == QLatin1String("ftp")))
        return value;
    if (explicitUrl.isValid() && explicitUrl.isLocalFile())
        return QFileInfo(explicitUrl.toLocalFile()).absoluteFilePath();
    if (QFileInfo(value).isAbsolute())
        return QFileInfo(value).absoluteFilePath();
    if (!base.isValid())
        return value;
    if (base.isLocalFile())
        return QDir(base.toLocalFile()).absoluteFilePath(value);
    return base.resolved(QUrl(value)).toString();
}

// Every playable entry in an M3U/M3U8 or PLS body, in playlist order.
// PLS: every FileN= value (not only the first). M3U: every non-comment line.
// Relative paths are resolved against `base`.
static inline QStringList playlistEntries(const QByteArray &data, const QUrl &base) {
    const QStringList lines = QString::fromUtf8(data).split(QLatin1Char('\n'));
    bool pls = false;
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        int index = 0;
        if (playlistFileKey(line.left(eq).trimmed(), &index) && index > 0) {
            pls = true;
            break;
        }
    }

    struct Row {
        int order;
        QString location;
    };
    QVector<Row> rows;
    int lineNo = 0;
    for (const QString &rawLine : lines) {
        ++lineNo;
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char('[')))
            continue;
        QString candidate = line;
        int order = lineNo;
        const int eq = line.indexOf(QLatin1Char('='));
        if (pls) {
            if (eq <= 0)
                continue;
            int index = 0;
            if (!playlistFileKey(line.left(eq).trimmed(), &index))
                continue;
            candidate = line.mid(eq + 1).trimmed();
            order = index;
        }
        const QString resolved = resolvePlaylistLocation(candidate, base);
        if (!resolved.isEmpty())
            rows.append(Row{order, resolved});
    }
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
        return a.order < b.order;
    });
    QStringList out;
    out.reserve(rows.size());
    for (const Row &row : rows)
        out.append(row.location);
    return out;
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

// Skin archives are ordinary zip files (.wsz / .zip / .wal). They are read with zlib
// so packaging does not depend on the unzip program. Nested directories are kept.
// A cache hit requires the archive's size, mtime, and SHA-256 to match the last extract.
static constexpr qint64 kSkinMaxUncompressedBytes = 128LL * 1024 * 1024;
static constexpr qint64 kSkinMaxArchiveBytes = 128LL * 1024 * 1024;
static constexpr int kSkinMaxEntries = 512;

static inline bool zipHave(const QByteArray &b, qint64 off, qint64 n) {
    return off >= 0 && n >= 0 && off <= b.size() && n <= b.size() - off;
}
static inline quint16 zipU16(const uchar *p) {
    return quint16(p[0]) | (quint16(p[1]) << 8);
}
static inline quint32 zipU32(const uchar *p) {
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}
static inline quint64 zipU64(const uchar *p) {
    return quint64(zipU32(p)) | (quint64(zipU32(p + 4)) << 32);
}

struct ZipCentralEntry {
    QString name;
    quint16 method = 0;
    quint16 flags = 0;
    quint32 crc = 0;
    quint64 compSize = 0;
    quint64 uncompSize = 0;
    quint64 localOffset = 0;
    quint32 extAttr = 0;
    bool directory = false;
};

static inline void applyZip64Extra(ZipCentralEntry *entry, const uchar *extra, int extraLen) {
    const bool needUncomp = entry->uncompSize == 0xFFFFFFFFu;
    const bool needComp = entry->compSize == 0xFFFFFFFFu;
    const bool needOff = entry->localOffset == 0xFFFFFFFFu;
    int off = 0;
    while (off + 4 <= extraLen) {
        const quint16 id = zipU16(extra + off);
        const quint16 sz = zipU16(extra + off + 2);
        off += 4;
        if (off + sz > extraLen)
            return;
        if (id == 0x0001) {
            int z = 0;
            auto take = [&](quint64 *dst, bool need) {
                if (!need || z + 8 > sz)
                    return;
                *dst = zipU64(extra + off + z);
                z += 8;
            };
            take(&entry->uncompSize, needUncomp);
            take(&entry->compSize, needComp);
            take(&entry->localOffset, needOff);
        }
        off += sz;
    }
}

// Empty string means "skip" (directory, junk, symlink). A null QString is not used;
// unsafe paths return a null QString via an out-flag so the caller can reject the archive.
static inline QString safeZipRelativePath(const QByteArray &rawName, bool *unsafe, bool *directory) {
    QString name = QString::fromUtf8(rawName);
    name.replace(QLatin1Char('\\'), QLatin1Char('/'));
    *directory = name.endsWith(QLatin1Char('/'));
    if (name.startsWith(QLatin1Char('/')) || name.contains(QLatin1Char(':'))) {
        *unsafe = true;
        return QString();
    }
    QStringList parts;
    const QStringList split = name.split(QLatin1Char('/'));
    for (const QString &part : split) {
        if (part.isEmpty() || part == QLatin1String("."))
            continue;
        if (part == QLatin1String("..")) {
            *unsafe = true;
            return QString();
        }
        parts.append(part);
    }
    if (parts.isEmpty())
        return QString();
    if (parts.first() == QLatin1String("__MACOSX"))
        return QString();
    return parts.join(QLatin1Char('/'));
}

static inline bool readZipCentralDirectory(const QByteArray &blob, QVector<ZipCentralEntry> *entries, QString *error) {
    const uchar *p = reinterpret_cast<const uchar *>(blob.constData());
    const int size = blob.size();
    int eocd = -1;
    const int scanFrom = qMax(0, size - (22 + 65535));
    for (int i = size - 22; i >= scanFrom; --i) {
        if (zipU32(p + i) == 0x06054b50u && i + 22 + zipU16(p + i + 20) == size) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        *error = QStringLiteral("not a zip archive");
        return false;
    }
    quint64 entryCount = zipU16(p + eocd + 10);
    quint64 cdSize = zipU32(p + eocd + 12);
    quint64 cdOffset = zipU32(p + eocd + 16);
    if (entryCount == 0xFFFFu || cdSize == 0xFFFFFFFFu || cdOffset == 0xFFFFFFFFu) {
        if (eocd < 20 || zipU32(p + eocd - 20) != 0x07064b50u) {
            *error = QStringLiteral("zip64 locator missing");
            return false;
        }
        const quint64 z64 = zipU64(p + eocd - 12);
        if (!zipHave(blob, static_cast<qint64>(z64), 56) || zipU32(p + z64) != 0x06064b50u) {
            *error = QStringLiteral("zip64 directory missing");
            return false;
        }
        entryCount = zipU64(p + z64 + 32);
        cdSize = zipU64(p + z64 + 40);
        cdOffset = zipU64(p + z64 + 48);
    }
    if (entryCount > static_cast<quint64>(kSkinMaxEntries)) {
        *error = QStringLiteral("too many entries");
        return false;
    }
    if (!zipHave(blob, static_cast<qint64>(cdOffset), static_cast<qint64>(cdSize))) {
        *error = QStringLiteral("central directory out of range");
        return false;
    }
    qint64 pos = static_cast<qint64>(cdOffset);
    quint64 declared = 0;
    for (quint64 n = 0; n < entryCount; ++n) {
        if (!zipHave(blob, pos, 46) || zipU32(p + pos) != 0x02014b50u) {
            *error = QStringLiteral("bad central directory");
            return false;
        }
        ZipCentralEntry entry;
        entry.flags = zipU16(p + pos + 8);
        entry.method = zipU16(p + pos + 10);
        entry.crc = zipU32(p + pos + 16);
        entry.compSize = zipU32(p + pos + 20);
        entry.uncompSize = zipU32(p + pos + 24);
        const int nameLen = zipU16(p + pos + 28);
        const int extraLen = zipU16(p + pos + 30);
        const int commentLen = zipU16(p + pos + 32);
        entry.extAttr = zipU32(p + pos + 38);
        entry.localOffset = zipU32(p + pos + 42);
        if (!zipHave(blob, pos + 46, nameLen + extraLen + commentLen)) {
            *error = QStringLiteral("central directory truncated");
            return false;
        }
        const QByteArray rawName(blob.constData() + pos + 46, nameLen);
        if (extraLen)
            applyZip64Extra(&entry, p + pos + 46 + nameLen, extraLen);
        pos += 46 + nameLen + extraLen + commentLen;
        if (entry.flags & 0x1) {
            *error = QStringLiteral("encrypted zip entries are not supported");
            return false;
        }
        if (entry.method != 0 && entry.method != 8) {
            *error = QStringLiteral("unsupported compression method");
            return false;
        }
        declared += entry.uncompSize;
        if (declared > static_cast<quint64>(kSkinMaxUncompressedBytes)) {
            *error = QStringLiteral("uncompressed size exceeds 128MB");
            return false;
        }
        const quint32 unixMode = entry.extAttr >> 16;
        if ((unixMode & 0170000) == 0120000)
            continue; // symlink: do not extract
        bool unsafe = false;
        entry.name = safeZipRelativePath(rawName, &unsafe, &entry.directory);
        if (unsafe) {
            *error = QStringLiteral("zip entry escapes the extract directory");
            return false;
        }
        if (entry.name.isEmpty())
            continue;
        entries->append(entry);
    }
    return true;
}

static inline bool inflateZipEntry(const QByteArray &comp, quint64 uncompSize, QByteArray *out) {
    if (uncompSize > static_cast<quint64>(kSkinMaxUncompressedBytes))
        return false;
    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
        return false;
    out->resize(static_cast<int>(uncompSize));
    strm.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(comp.constData()));
    strm.avail_in = static_cast<uInt>(comp.size());
    strm.next_out = reinterpret_cast<Bytef *>(out->data());
    strm.avail_out = static_cast<uInt>(out->size());
    const int rc = inflate(&strm, Z_FINISH);
    const uLong produced = strm.total_out;
    inflateEnd(&strm);
    return rc == Z_STREAM_END && produced == uncompSize;
}

static inline bool extractZipBlob(const QByteArray &blob, const QString &destDir, QString *error) {
    QVector<ZipCentralEntry> entries;
    if (!readZipCentralDirectory(blob, &entries, error))
        return false;
    const uchar *p = reinterpret_cast<const uchar *>(blob.constData());
    quint64 written = 0;
    for (const ZipCentralEntry &entry : entries) {
        if (!zipHave(blob, static_cast<qint64>(entry.localOffset), 30)
            || zipU32(p + entry.localOffset) != 0x04034b50u) {
            *error = QStringLiteral("bad local header");
            return false;
        }
        const int nameLen = zipU16(p + entry.localOffset + 26);
        const int extraLen = zipU16(p + entry.localOffset + 28);
        const qint64 dataOff = static_cast<qint64>(entry.localOffset) + 30 + nameLen + extraLen;
        if (!zipHave(blob, dataOff, static_cast<qint64>(entry.compSize))) {
            *error = QStringLiteral("compressed data out of range");
            return false;
        }
        QByteArray payload;
        if (entry.method == 0) {
            if (entry.compSize != entry.uncompSize) {
                *error = QStringLiteral("stored entry size mismatch");
                return false;
            }
            payload = QByteArray(blob.constData() + dataOff, static_cast<int>(entry.compSize));
        } else {
            const QByteArray comp(blob.constData() + dataOff, static_cast<int>(entry.compSize));
            if (!inflateZipEntry(comp, entry.uncompSize, &payload)) {
                *error = QStringLiteral("deflate failed");
                return false;
            }
        }
        if (entry.crc != 0 && entry.uncompSize != 0) {
            const quint32 got = crc32(0L, reinterpret_cast<const Bytef *>(payload.constData()),
                                       static_cast<uInt>(payload.size()));
            if (got != entry.crc) {
                *error = QStringLiteral("crc mismatch");
                return false;
            }
        }
        written += static_cast<quint64>(payload.size());
        if (written > static_cast<quint64>(kSkinMaxUncompressedBytes)) {
            *error = QStringLiteral("extract exceeded size ceiling");
            return false;
        }
        const QString dest = destDir + QLatin1Char('/') + entry.name;
        if (entry.directory) {
            QDir().mkpath(dest);
            continue;
        }
        QDir().mkpath(QFileInfo(dest).absolutePath());
        QFile out(dest);
        if (!out.open(QIODevice::WriteOnly) || out.write(payload) != payload.size()) {
            *error = QStringLiteral("could not write ") + dest;
            return false;
        }
    }
    return true;
}

static inline bool directoryHasSkinMarker(const QString &dir) {
    static const char *names[] = {
        "MAIN.BMP", "main.bmp", "Main.bmp",
        "EQMAIN.BMP", "eqmain.bmp", "Eqmain.bmp",
        "skin.xml", "SKIN.XML", nullptr
    };
    for (const char *name : names) {
        if (!name)
            break;
        if (QFile::exists(dir + QLatin1Char('/') + QLatin1String(name)))
            return true;
    }
    return false;
}

// Shallowest directory that actually holds the skin. A pack that wraps its
// bitmaps in one folder is used from that folder; files beside it stay put.
static inline QString findSkinRoot(const QString &extractDir) {
    if (directoryHasSkinMarker(extractDir))
        return extractDir;
    QStringList pending;
    pending.append(extractDir);
    while (!pending.isEmpty()) {
        const QString dir = pending.takeFirst();
        const QFileInfoList subs = QDir(dir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &sub : subs) {
            if (sub.fileName() == QLatin1String("__MACOSX") || sub.fileName().startsWith(QLatin1Char('.')))
                continue;
            if (directoryHasSkinMarker(sub.absoluteFilePath()))
                return sub.absoluteFilePath();
            pending.append(sub.absoluteFilePath());
        }
    }
    return QString();
}

static inline QString skinSourceStamp(const QString &archivePath) {
    QFile f(archivePath);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    const QByteArray hash = QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex();
    const QFileInfo fi(archivePath);
    return QString::number(fi.size()) + QLatin1Char(' ')
        + QString::number(fi.lastModified().toMSecsSinceEpoch()) + QLatin1Char(' ')
        + QString::fromLatin1(hash);
}

// Extract a .wsz, .zip, or .wal skin archive to a cache directory.
// Returns the skin root (the directory that contains the bitmaps or skin.xml).
static inline QString extractSkinArchive(const QString &archivePath) {
    QFileInfo fi(archivePath);
    if (!fi.exists() || !fi.isFile())
        return {};
    if (fi.size() > kSkinMaxArchiveBytes) {
        qWarning() << "Skin archive exceeds size ceiling:" << archivePath;
        return {};
    }

    const QString cacheDir = QDir::homePath() + QStringLiteral("/.cache/winamp/skins/") + fi.completeBaseName();
    const QString stampPath = cacheDir + QStringLiteral("/.source-stamp");
    const QString stamp = skinSourceStamp(archivePath);
    if (!stamp.isEmpty()) {
        QFile existing(stampPath);
        if (existing.open(QIODevice::ReadOnly)
            && QString::fromUtf8(existing.readAll()).trimmed() == stamp) {
            const QString cached = findSkinRoot(cacheDir);
            if (!cached.isEmpty())
                return cached;
        }
    }

    QFile archive(archivePath);
    if (!archive.open(QIODevice::ReadOnly))
        return {};
    const QByteArray blob = archive.readAll();
    archive.close();

    QDir(cacheDir).removeRecursively();
    if (!QDir().mkpath(cacheDir))
        return {};
    QString error;
    if (!extractZipBlob(blob, cacheDir, &error)) {
        qWarning() << "Failed to extract skin archive:" << archivePath << error;
        QDir(cacheDir).removeRecursively();
        return {};
    }
    QFile stampFile(stampPath);
    if (stampFile.open(QIODevice::WriteOnly))
        stampFile.write(stamp.toUtf8());
    const QString root = findSkinRoot(cacheDir);
    return root.isEmpty() ? cacheDir : root;
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

// Window-snapping preferences (Preferences > Setup > Snap windows together / distance).
// Shared across equalizer.cpp and playlist.cpp, which each implement their own checkSnap().
extern bool g_snapWindowsEnabled;
extern int g_snapDistance;

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

#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <mpegfile.h>
#include <id3v1tag.h>
#include <id3v2tag.h>
#include "../src/tag_metadata.h"

class TestTagMetadata : public QObject {
    Q_OBJECT
private slots:
    void testId3v1GenreList();
    void testDisplayTitleFormatting();
    void testWriteReadRoundTrip();
    void testId3v1KeepsLatin1PrefixAndWritesV23();
    void testStripTags();
};

void TestTagMetadata::testId3v1GenreList() {
    const QStringList genres = TagMetadata::id3v1Genres();
    QVERIFY(genres.size() >= 80); // classic ID3v1 set is 80+ (Winamp extended ~148)
    QVERIFY(genres.contains(QStringLiteral("Rock")));
    QVERIFY(genres.contains(QStringLiteral("Blues")));
}

void TestTagMetadata::testDisplayTitleFormatting() {
    MediaTags t;
    t.valid = true;
    t.artist = QStringLiteral("Artist");
    t.title = QStringLiteral("Title");
    QCOMPARE(TagMetadata::displayTitle(t, "/tmp/x.mp3"),
             QStringLiteral("Artist - Title"));

    t.artist.clear();
    QCOMPARE(TagMetadata::displayTitle(t, "/tmp/song.mp3"),
             QStringLiteral("Title"));

    t.title.clear();
    t.valid = false;
    QCOMPARE(TagMetadata::displayTitle(t, "/tmp/path/MyTrack.mp3"),
             QStringLiteral("MyTrack"));
}

static QString makeSampleMp3(QTemporaryDir &dir) {
    const QString path = dir.filePath(QStringLiteral("sample.mp3"));
    // Prefer ffmpeg when available (CI images vary).
    const int rc = QProcess::execute(QStringLiteral("ffmpeg"), {
        QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-y"),
        QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"), QStringLiteral("anullsrc=r=44100:cl=mono"),
        QStringLiteral("-t"), QStringLiteral("0.1"),
        QStringLiteral("-codec:a"), QStringLiteral("libmp3lame"),
        QStringLiteral("-q:a"), QStringLiteral("9"),
        path
    });
    if (rc == 0 && QFile::exists(path) && QFileInfo(path).size() > 64)
        return path;

    // Fallback: tiny MPEG-1 Layer III frame (mono, 44.1kHz, 32kbps) + padding.
    // Enough for TagLib to treat as a valid MPEG file.
    static const unsigned char kMiniMp3[] = {
        0xFF, 0xFB, 0x10, 0xC4, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QString();
    for (int i = 0; i < 8; ++i)
        f.write(reinterpret_cast<const char *>(kMiniMp3), sizeof(kMiniMp3));
    f.close();
    return path;
}

void TestTagMetadata::testWriteReadRoundTrip() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = makeSampleMp3(dir);
    QVERIFY2(!path.isEmpty(), "Could not create sample MP3");

    MediaTags w;
    w.valid = true;
    w.title = QStringLiteral("Llama Whip");
    w.artist = QStringLiteral("Nullsoft");
    w.album = QStringLiteral("Winamp Classics");
    w.year = QStringLiteral("1997");
    w.track = 3;
    w.genre = QStringLiteral("Rock");
    w.comment = QStringLiteral("It really whips the llama's ass");

    QString err;
    QVERIFY2(TagMetadata::writeTags(path, w, &err), qPrintable(err));

    MediaTags r = TagMetadata::readTags(path);
    QVERIFY(r.valid);
    QCOMPARE(r.title, w.title);
    QCOMPARE(r.artist, w.artist);
    QCOMPARE(r.album, w.album);
    QCOMPARE(r.year, w.year);
    QCOMPARE(r.track, w.track);
    QCOMPARE(r.genre, w.genre);
    QCOMPARE(r.comment, w.comment);

    QCOMPARE(TagMetadata::displayTitle(path),
             QStringLiteral("Nullsoft - Llama Whip"));

    MediaAudioInfo info = TagMetadata::readAudioInfo(path);
    QVERIFY(info.valid);
    // After save, MPEG files should report ID3 presence
    QVERIFY(info.hasId3v1 || info.hasId3v2);
}

static QString fromTagString(const TagLib::String &s) {
    const char *utf8 = s.toCString(true);
    return QString::fromUtf8(utf8 ? utf8 : "");
}

void TestTagMetadata::testId3v1KeepsLatin1PrefixAndWritesV23() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = makeSampleMp3(dir);
    QVERIFY2(!path.isEmpty(), "Could not create sample MP3");

    // '😀' is one non-BMP character (a surrogate pair). '音' is BMP, outside Latin-1.
    const QString title = QStringLiteral("A😀B ") + QString(40, QLatin1Char('X'));
    const QString artist = QStringLiteral("音");
    const QString album = QString(35, QLatin1Char('L'));
    const QString comment = QString(40, QLatin1Char('C'));

    MediaTags w;
    w.valid = true;
    w.title = title;
    w.artist = artist;
    w.album = album;
    w.comment = comment;
    w.year = QStringLiteral("10000");
    w.track = 300;
    w.genre = QStringLiteral("Rock");

    QString err;
    QVERIFY2(TagMetadata::writeTags(path, w, &err), qPrintable(err));

    MediaTags r = TagMetadata::readTags(path);
    QVERIFY(r.valid);
    QCOMPARE(r.title, title);
    QCOMPARE(r.artist, artist);
    QCOMPARE(r.album, album);
    QCOMPARE(r.comment, comment);
    QCOMPARE(r.year, QString());
    QCOMPARE(r.track, 300);
    QCOMPARE(r.genre, QStringLiteral("Rock"));

    QFile raw(path);
    QVERIFY(raw.open(QIODevice::ReadOnly));
    const QByteArray header = raw.read(4);
    raw.close();
    QCOMPARE(header.left(3), QByteArray("ID3"));
    QCOMPARE(int(static_cast<unsigned char>(header.at(3))), 3);

    TagLib::MPEG::File mpeg(QFile::encodeName(path).constData());
    QVERIFY(mpeg.isValid());
    QVERIFY(mpeg.hasID3v1Tag());
    QVERIFY(mpeg.hasID3v2Tag());
    TagLib::ID3v1::Tag *v1 = mpeg.ID3v1Tag();
    QVERIFY(v1);

    const QString v1Title = fromTagString(v1->title());
    QCOMPARE(v1Title, QStringLiteral("A?B ") + QString(26, QLatin1Char('X')));
    QCOMPARE(v1Title.size(), 30);
    QCOMPARE(fromTagString(v1->artist()), QStringLiteral("?"));
    QCOMPARE(fromTagString(v1->album()), QString(30, QLatin1Char('L')));
    QCOMPARE(fromTagString(v1->comment()), QString(28, QLatin1Char('C')));
    QCOMPARE(v1->year(), 0u);
    QCOMPARE(v1->track(), 0u);
    QCOMPARE(fromTagString(v1->genre()), QStringLiteral("Rock"));

    // A track in range uses the 28-byte comment; the full text stays on v2.
    w.year = QStringLiteral("1997");
    w.track = 3;
    w.comment = comment;
    QVERIFY2(TagMetadata::writeTags(path, w, &err), qPrintable(err));
    TagLib::MPEG::File mpeg2(QFile::encodeName(path).constData());
    QVERIFY(mpeg2.ID3v1Tag());
    QCOMPARE(fromTagString(mpeg2.ID3v1Tag()->comment()), QString(28, QLatin1Char('C')));
    QCOMPARE(mpeg2.ID3v1Tag()->year(), 1997u);
    QCOMPARE(mpeg2.ID3v1Tag()->track(), 3u);
    QCOMPARE(TagMetadata::readTags(path).comment, comment);
    QCOMPARE(TagMetadata::readTags(path).year, QStringLiteral("1997"));
}

void TestTagMetadata::testStripTags() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = makeSampleMp3(dir);
    QVERIFY(!path.isEmpty());

    MediaTags w;
    w.valid = true;
    w.title = QStringLiteral("Temp");
    w.artist = QStringLiteral("Temp");
    QVERIFY(TagMetadata::writeTags(path, w));

    QString err;
    QVERIFY2(TagMetadata::stripTags(path, &err), qPrintable(err));

    MediaTags r = TagMetadata::readTags(path);
    QVERIFY(r.valid);
    QVERIFY(r.title.isEmpty());
    QVERIFY(r.artist.isEmpty());
}

QTEST_MAIN(TestTagMetadata)
#include "test_tag_metadata.moc"

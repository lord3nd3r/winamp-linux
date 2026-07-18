#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include "../src/tag_metadata.h"

class TestTagMetadata : public QObject {
    Q_OBJECT
private slots:
    void testId3v1GenreList();
    void testDisplayTitleFormatting();
    void testWriteReadRoundTrip();
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

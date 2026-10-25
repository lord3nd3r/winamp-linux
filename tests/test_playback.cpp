#include <QtTest>
#include <QObject>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QUrl>
#include <memory>

#include "../src/constants.h"
#include "../src/winamp_window.h"

bool g_isModernSkin = false;
ModernSkinEngine *g_modernSkin = nullptr;
SkinPlaylistColors g_plColors;
bool g_snapWindowsEnabled = true;
int g_snapDistance = 25;
QColor visColors[24];

#if defined(QT_DBUS_LIB) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void Mpris2PlayerAdaptor::Next() {
    WinampWindow *w = qobject_cast<WinampWindow *>(parent());
    if (w && w->getPlaylistWindow())
        w->playNext();
}
void Mpris2PlayerAdaptor::Previous() {
    WinampWindow *w = qobject_cast<WinampWindow *>(parent());
    if (w && w->getPlaylistWindow())
        w->getPlaylistWindow()->prevTrack();
}
void Mpris2PlayerAdaptor::OpenUri(const QString &uri) {
    QUrl url(uri);
    WinampWindow *w = qobject_cast<WinampWindow *>(parent());
    if (w && url.isLocalFile())
        w->playTrack(url.toLocalFile());
}
#endif

class TestPlayback : public QObject {
    Q_OBJECT

private slots:
    void testGaplessSwapFollowsActivePlayer();
    void testGaplessClearsStaleSuccessor();
    void testSettingsWrite();
    void testStalePlaybackCallback();
    void testLocalPlaylistEnqueuesEveryEntry();
};

void TestPlayback::testGaplessSwapFollowsActivePlayer() {
    WinampWindow window;
    window.shuffleOn = false;
    window.repeatOn = false;
    window.repeatTrack = false;
    window.stopAfterCurrent = false;

    PlaylistWindow *playlist = window.playlistWindow;
    playlist->addTrack("http://example.com/a.mp3");
    playlist->addTrack("http://example.com/b.mp3");
    playlist->setCurrentTrackIndex(0);
    window.currentFile = QStringLiteral("http://example.com/a.mp3");

    QMediaPlayer *original = window.player;
    QMediaPlayer *preloaded = window.nextPlayer;
    waSetSource(original, QUrl(QStringLiteral("http://example.com/a.mp3")));
    waSetSource(preloaded, QUrl(QStringLiteral("http://example.com/b.mp3")));
    window.player->setVideoOutput(window.videoWindow->getVideoWidget());

    window.handleActiveMediaStatus(original, QMediaPlayer::EndOfMedia);

    QCOMPARE(window.player, preloaded);
    QCOMPARE(window.nextPlayer, original);
    QCOMPARE(window.currentFile, QStringLiteral("http://example.com/b.mp3"));
    QCOMPARE(playlist->currentTrackIndex(), 1);
    QCOMPARE(window.player->videoOutput(), static_cast<QObject *>(window.videoWindow->getVideoWidget()));
    QCOMPARE(window.nextPlayer->videoOutput(), static_cast<QObject *>(nullptr));
    // The track that just finished must not stay queued on the idle player.
    QVERIFY(waSource(window.nextPlayer).isEmpty());
#if defined(QT_DBUS_LIB) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QVERIFY(window.mprisPlayer);
    QCOMPARE(window.mprisPlayer->activePlayer(), window.player);
    QCOMPARE(window.mprisPlayer->activeAudioOutput(), window.audioOutput);
#endif

    // A late status event from the idle player must not swap back or skip.
    window.handleActiveMediaStatus(original, QMediaPlayer::EndOfMedia);
    QCOMPARE(window.player, preloaded);
    QCOMPARE(window.currentFile, QStringLiteral("http://example.com/b.mp3"));
    QCOMPARE(playlist->currentTrackIndex(), 1);
}

void TestPlayback::testGaplessClearsStaleSuccessor() {
    WinampWindow window;
    window.shuffleOn = false;
    window.repeatOn = false;
    window.repeatTrack = false;

    PlaylistWindow *playlist = window.playlistWindow;
    playlist->addTrack("http://example.com/only.mp3");
    playlist->setCurrentTrackIndex(0);
    waSetSource(window.nextPlayer, QUrl(QStringLiteral("http://example.com/stale.mp3")));

    window.preloadNextTrack();
    QVERIFY(waSource(window.nextPlayer).isEmpty());

    playlist->addTrack("http://example.com/next.mp3");
    window.preloadNextTrack();
    QCOMPARE(waSource(window.nextPlayer).toString(), QStringLiteral("http://example.com/next.mp3"));

    window.shuffleOn = true;
    window.preloadNextTrack();
    QVERIFY(waSource(window.nextPlayer).isEmpty());
}

void TestPlayback::testSettingsWrite() {
    WinampWindow window;
    window.volume = 123;
    window.balance = -40;
    window.shuffleOn = true;
    window.repeatOn = true;

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString ini = dir.filePath("winamp.ini");
    {
        QSettings s(ini, QSettings::IniFormat);
        window.writeSettings(s);
    }
    QSettings loaded(ini, QSettings::IniFormat);
    loaded.beginGroup("Playback");
    QCOMPARE(loaded.value("volume").toInt(), 123);
    QCOMPARE(loaded.value("balance").toInt(), -40);
    QCOMPARE(loaded.value("shuffle").toBool(), true);
    QCOMPARE(loaded.value("repeat").toBool(), true);
    loaded.endGroup();
}

void TestPlayback::testLocalPlaylistEnqueuesEveryEntry() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir(dir.path()).mkpath(QStringLiteral("sub"));
    QFile song(dir.filePath(QStringLiteral("b.mp3")));
    QVERIFY(song.open(QIODevice::WriteOnly));
    song.write("x");
    song.close();
    QFile nested(dir.filePath(QStringLiteral("sub/c.mp3")));
    QVERIFY(nested.open(QIODevice::WriteOnly));
    nested.write("y");
    nested.close();
    QFile pls(dir.filePath(QStringLiteral("list.pls")));
    QVERIFY(pls.open(QIODevice::WriteOnly));
    pls.write("[playlist]\n"
              "File1=http://example.com/a.mp3\n"
              "Title1=Nope\n"
              "File2=b.mp3\n"
              "File3=sub/c.mp3\n"
              "NumberOfEntries=3\n");
    pls.close();

    WinampWindow window;
    const auto started = std::make_shared<bool>(false);
    window.resolveLocalPlaylistFile(pls.fileName(), 0, window.playbackGeneration, started);
    QCOMPARE(window.playlistWindow->trackCount(), 3);
    QCOMPARE(window.playlistWindow->trackAt(0), QStringLiteral("http://example.com/a.mp3"));
    QCOMPARE(window.playlistWindow->trackAt(1), song.fileName());
    QCOMPARE(window.playlistWindow->trackAt(2), nested.fileName());
    QCOMPARE(window.playlistWindow->currentTrackIndex(), 0);
    QVERIFY(*started);
}

void TestPlayback::testStalePlaybackCallback() {
    WinampWindow window;
    const quint64 before = window.playbackGeneration;
    window.playUrl(QStringLiteral("http://example.test/a.mp3"));
    const quint64 firstRequest = window.playbackGeneration;
    QVERIFY(firstRequest > before);
    QVERIFY(window.playbackCallbackCurrent(firstRequest));

    window.playUrl(QStringLiteral("http://example.test/b.mp3"));
    QVERIFY(window.playbackGeneration > firstRequest);
    QVERIFY(!window.playbackCallbackCurrent(firstRequest));
    QVERIFY(window.playbackCallbackCurrent(window.playbackGeneration));
}

QTEST_MAIN(TestPlayback)
#include "test_playback.moc"

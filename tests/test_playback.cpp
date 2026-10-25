#include <QtTest>
#include <QObject>
#include <QColor>
#include <QUrl>

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

QTEST_MAIN(TestPlayback)
#include "test_playback.moc"

#include <QtTest>
#include <QObject>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include "../src/playlist.h"

// Mock/stub any global required variables if not linked
// In our build, playlist.cpp links with constants.h which declares the externs.
// We should define them here so the test executable links properly if main.cpp is not linked.
bool g_isModernSkin = false;
class ModernSkinEngine;
ModernSkinEngine *g_modernSkin = nullptr;
SkinPlaylistColors g_plColors;
bool g_snapWindowsEnabled = true;
int g_snapDistance = 25;

class TestPlaylist : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    void testAddAndClear();
    void testNavigation();
    void testSortingAndReversal();
    void testSelectionAndRemoval();
    void testRemoveDeadFiles();
    void testAddFolderAsync();
    void testPreferenceSettingsPersist();
    void testRecycleBinRestore();
};

void TestPlaylist::initTestCase() {
    // Set up app name for translation/settings
    QCoreApplication::setApplicationName("WinampTest");
}

void TestPlaylist::cleanupTestCase() {
}

void TestPlaylist::testAddAndClear() {
    PlaylistWindow playlist;
    QCOMPARE(playlist.trackCount(), 0);
    
    // Add remote track (bypasses file check)
    playlist.addTrack("http://example.com/track1.mp3");
    QCOMPARE(playlist.trackCount(), 1);
    QCOMPARE(playlist.trackAt(0), QString("http://example.com/track1.mp3"));
    
    // Add invalid local file (should be ignored)
    playlist.addTrack("/nonexistent/file/path/here.mp3");
    QCOMPARE(playlist.trackCount(), 1);
    
    // Clear playlist
    playlist.clearPlaylist();
    QCOMPARE(playlist.trackCount(), 0);
}

void TestPlaylist::testNavigation() {
    PlaylistWindow playlist;
    
    // Add some mock remote tracks
    playlist.addTrack("http://example.com/a.mp3");
    playlist.addTrack("http://example.com/b.mp3");
    playlist.addTrack("http://example.com/c.mp3");
    
    QCOMPARE(playlist.trackCount(), 3);
    QCOMPARE(playlist.currentTrackIndex(), -1); // No selection initially
    
    playlist.setCurrentTrackIndex(1);
    QCOMPARE(playlist.currentTrackIndex(), 1);
    QCOMPARE(playlist.trackAt(playlist.currentTrackIndex()), QString("http://example.com/b.mp3"));
    
    // Test next/prev nav
    playlist.nextTrack();
    QCOMPARE(playlist.currentTrackIndex(), 2);
    
    playlist.prevTrack();
    QCOMPARE(playlist.currentTrackIndex(), 1);
    
    playlist.prevTrack();
    QCOMPARE(playlist.currentTrackIndex(), 0);
}

void TestPlaylist::testSortingAndReversal() {
    PlaylistWindow playlist;
    
    // Add remote tracks in unsorted order
    playlist.addTrack("http://example.com/z.mp3");
    playlist.addTrack("http://example.com/m.mp3");
    playlist.addTrack("http://example.com/a.mp3");
    
    QCOMPARE(playlist.trackCount(), 3);
    QCOMPARE(playlist.trackAt(0), QString("http://example.com/z.mp3"));
    QCOMPARE(playlist.trackAt(1), QString("http://example.com/m.mp3"));
    QCOMPARE(playlist.trackAt(2), QString("http://example.com/a.mp3"));
    
    // Test reverse list
    playlist.reverseList();
    QCOMPARE(playlist.trackAt(0), QString("http://example.com/a.mp3"));
    QCOMPARE(playlist.trackAt(1), QString("http://example.com/m.mp3"));
    QCOMPARE(playlist.trackAt(2), QString("http://example.com/z.mp3"));
    
    // Test sort by filename/title
    playlist.sortByFilename();
    QCOMPARE(playlist.trackAt(0), QString("http://example.com/a.mp3"));
    QCOMPARE(playlist.trackAt(1), QString("http://example.com/m.mp3"));
    QCOMPARE(playlist.trackAt(2), QString("http://example.com/z.mp3"));
    
    // Sort reverse and sort again
    playlist.reverseList();
    playlist.sortByPath();
    QCOMPARE(playlist.trackAt(0), QString("http://example.com/a.mp3"));
    
    // Test randomize list doesn't lose items
    playlist.randomizeList();
    QCOMPARE(playlist.trackCount(), 3);
    QStringList all = playlist.allTracks();
    QVERIFY(all.contains("http://example.com/a.mp3"));
    QVERIFY(all.contains("http://example.com/m.mp3"));
    QVERIFY(all.contains("http://example.com/z.mp3"));
}

void TestPlaylist::testSelectionAndRemoval() {
    PlaylistWindow playlist;
    playlist.addTrack("http://example.com/1.mp3");
    playlist.addTrack("http://example.com/2.mp3");
    playlist.addTrack("http://example.com/3.mp3");
    
    // Select index 1 (2.mp3)
    playlist.listWidget->item(1)->setSelected(true);
    
    // Remove selected
    playlist.removeSelected();
    QCOMPARE(playlist.trackCount(), 2);
    QCOMPARE(playlist.trackAt(0), QString("http://example.com/1.mp3"));
    QCOMPARE(playlist.trackAt(1), QString("http://example.com/3.mp3"));
    
    // Reset and test cropSelected
    playlist.clearPlaylist();
    playlist.addTrack("http://example.com/1.mp3");
    playlist.addTrack("http://example.com/2.mp3");
    playlist.addTrack("http://example.com/3.mp3");
    
    playlist.listWidget->item(1)->setSelected(true);
    playlist.cropSelected();
    QCOMPARE(playlist.trackCount(), 1);
    QCOMPARE(playlist.trackAt(0), QString("http://example.com/2.mp3"));
}

void TestPlaylist::testRemoveDeadFiles() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    
    QString existingFile = dir.filePath("test_exist.mp3");
    QFile file(existingFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("mock media content");
    file.close();
    
    QString missingFile = dir.filePath("test_missing.mp3");
    
    PlaylistWindow playlist;
    // Inject both paths without addTrack: addTrack queues a QMediaPlayer duration
    // probe that can SIGSEGV in headless CI when GStreamer lacks a full graph.
    // removeDeadFiles() only checks QFile::exists / remote URLs.
    playlist.tracks.append(existingFile);
    playlist.tracks.append(missingFile);
    playlist.trackDurations.append(0);
    playlist.trackDurations.append(0);
    playlist.rebuildListDisplay();
    QCOMPARE(playlist.trackCount(), 2);
    
    playlist.removeDeadFiles();
    QCOMPARE(playlist.trackCount(), 1);
    QCOMPARE(playlist.trackAt(0), existingFile);
}

void TestPlaylist::testAddFolderAsync() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    
    // Create nested directory structure
    QString subDirPath = dir.filePath("subdir");
    QDir().mkpath(subDirPath);
    
    // Create a few files in root and subdir
    QString file1 = dir.filePath("track1.mp3");
    QFile f1(file1);
    QVERIFY(f1.open(QIODevice::WriteOnly));
    f1.write("dummy");
    f1.close();
    
    QString file2 = subDirPath + "/track2.flac";
    QFile f2(file2);
    QVERIFY(f2.open(QIODevice::WriteOnly));
    f2.write("dummy");
    f2.close();
    
    // Create a non-audio file (should be ignored)
    QString file3 = dir.filePath("readme.txt");
    QFile f3(file3);
    QVERIFY(f3.open(QIODevice::WriteOnly));
    f3.write("dummy");
    f3.close();
    
    PlaylistWindow playlist;
    QCOMPARE(playlist.trackCount(), 0);
    
    // Run async directory load
    playlist.addFolderAsync(dir.path(), false);
    
    // Wait for the background thread to finish and trigger the posted callback
    QTest::qWait(1000);
    
    // Check that files are added and readme.txt is ignored
    QCOMPARE(playlist.trackCount(), 2);
    
    QStringList expected;
    expected << QFileInfo(file1).absoluteFilePath() << QFileInfo(file2).absoluteFilePath();
    expected.sort();
    
    QCOMPARE(playlist.trackAt(0), expected[0]);
    QCOMPARE(playlist.trackAt(1), expected[1]);
}

// Regression test for the Preferences dialog controls that used to be cosmetic-only
// (see PreferencesDialog / WinampWindow::applyPreferenceChange): confirms values actually
// round-trip through PlaylistWindow::saveSettings()/loadSettings().
void TestPlaylist::testPreferenceSettingsPersist() {
    QTemporaryDir dir;
    QString cfgPath = dir.path() + "/prefs.conf";

    {
        PlaylistWindow p1;
        p1.setUseCustomFont(true);
        p1.setPlaylistFontFamily("Tahoma");
        p1.setPlaylistFontSizeOnly(14);
        p1.setRecycleBinEnabled(true);
        p1.setShowTrackNumbers(false);

        QSettings s(cfgPath, QSettings::IniFormat);
        p1.saveSettings(s);
        s.sync();
    }

    PlaylistWindow p2;
    QCOMPARE(p2.customFontEnabled(), false); // sanity: defaults before load
    QSettings s2(cfgPath, QSettings::IniFormat);
    p2.loadSettings(s2);

    QCOMPARE(p2.customFontEnabled(), true);
    QCOMPARE(p2.fontFamily(), QString("Tahoma"));
    QCOMPARE(p2.fontSize(), 14);
    QCOMPARE(p2.recycleBinEnabled(), true);
    QCOMPARE(p2.trackNumbersShown(), false);

    // trackDisplayName should drop the "N. " prefix when showTrackNumbers is off
    QCOMPARE(p2.trackDisplayName(0, "song.mp3"), QString("song.mp3"));
    p2.setShowTrackNumbers(true);
    QCOMPARE(p2.trackDisplayName(0, "song.mp3"), QString("1. song.mp3"));
}

void TestPlaylist::testRecycleBinRestore() {
    PlaylistWindow playlist;
    playlist.setRecycleBinEnabled(true);
    playlist.addTrack("http://example.com/a.mp3");
    playlist.addTrack("http://example.com/b.mp3");
    playlist.addTrack("http://example.com/c.mp3");
    QCOMPARE(playlist.trackCount(), 3);

    playlist.listWidget->item(1)->setSelected(true);
    playlist.removeSelected();
    QCOMPARE(playlist.trackCount(), 2);

    playlist.restoreLastRemoved();
    QCOMPARE(playlist.trackCount(), 3);
    QCOMPARE(playlist.trackAt(1), QString("http://example.com/b.mp3"));
}

QTEST_MAIN(TestPlaylist)
#include "test_playlist.moc"

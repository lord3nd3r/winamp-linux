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
    playlist.addTrack(existingFile);
    QCOMPARE(playlist.trackCount(), 1);
    
    // We cannot use addTrack for a missing local file directly since addTrack
    // validates existence. We manually inject it into the tracks list to test removeDeadFiles.
    playlist.tracks.append(missingFile);
    playlist.trackDurations.append(0);
    playlist.rebuildListDisplay();
    QCOMPARE(playlist.trackCount(), 2);
    
    // Remove dead files (should filter out the missing track)
    playlist.removeDeadFiles();
    QCOMPARE(playlist.trackCount(), 1);
    QCOMPARE(playlist.trackAt(0), existingFile);
}

QTEST_MAIN(TestPlaylist)
#include "test_playlist.moc"

// tag_metadata.h — TagLib-backed audio metadata (Winamp 2.x ID3 File Info)
#pragma once

#include <QString>
#include <QStringList>

// Fields matching classic Winamp 2.x / Nullsoft ID3 editor (ID3v1 + ID3v2).
struct MediaTags {
    QString title;
    QString artist;
    QString album;
    QString comment;
    QString genre;
    QString year;   // free-form string; stored as TagLib year when numeric
    int track = 0;  // 0 = unset
    bool valid = false;
};

// Technical / MPEG stream info shown on File Info (read-only).
struct MediaAudioInfo {
    int lengthSeconds = 0;
    int bitrateKbps = 0;
    int sampleRate = 0;
    int channels = 0;
    QString format;     // e.g. "MPEG", "FLAC", "Ogg Vorbis", "AAC"
    QString details;    // e.g. "Layer III", "VBR"
    bool hasId3v1 = false;
    bool hasId3v2 = false;
    bool valid = false;
};

namespace TagMetadata {

// Read tags from a local media file. Remote URLs / missing files → invalid.
MediaTags readTags(const QString &filePath);

// Write tags. For MPEG/MP3, writes both ID3v1 and ID3v2 (Winamp 2.x behavior).
// Other formats use TagLib's native tag (Vorbis comment, FLAC, MP4, etc.).
// Returns true on successful save to disk.
bool writeTags(const QString &filePath, const MediaTags &tags, QString *errorOut = nullptr);

// Strip embedded tags (ID3v1+v2 on MPEG; generic tag clear otherwise).
bool stripTags(const QString &filePath, QString *errorOut = nullptr);

// Audio properties for the Technical pane.
MediaAudioInfo readAudioInfo(const QString &filePath);

// Canonical ID3v1 genre list (Winamp genre dropdown order).
QStringList id3v1Genres();

// Winamp-style display: "Artist - Title", then title, then filename base.
QString displayTitle(const QString &filePath);
QString displayTitle(const MediaTags &tags, const QString &filePathFallback);

// Sort key for "Sort by title" (artist+title or filename).
QString sortTitleKey(const QString &filePath);

// True if path looks like a local media file we can open with TagLib.
bool canTagFile(const QString &filePath);

} // namespace TagMetadata

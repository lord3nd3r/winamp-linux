// tag_metadata.cpp — TagLib read/write for classic Winamp 2.x ID3 workflow
#include "tag_metadata.h"

#include <QFileInfo>
#include <QFile>
#include <QUrl>

#include <fileref.h>
#include <tag.h>
#include <tpropertymap.h>
#include <mpegfile.h>
#include <id3v2tag.h>
#include <id3v1tag.h>
#include <id3v1genres.h>
#include <flacfile.h>
#include <vorbisfile.h>
#include <oggflacfile.h>
#include <mp4file.h>
#include <wavfile.h>
#include <aifffile.h>
#include <audioproperties.h>
#include <tstring.h>
#include <tfile.h>
#include <id3v2.h>

// Optional formats — available in TagLib 1.x/2.x depending on build
#if __has_include(<opusfile.h>)
#  include <opusfile.h>
#  define WINAMP_HAS_TAGLIB_OPUS 1
#endif
#if __has_include(<oggflacfile.h>)
#  include <oggflacfile.h>
#  define WINAMP_HAS_TAGLIB_OGGFLAC 1
#endif

namespace {

bool isRemoteLocation(const QString &value) {
    QUrl url = QUrl::fromUserInput(value.trimmed());
    if (!url.isValid() || url.isLocalFile() || url.scheme().isEmpty())
        return false;
    const QString scheme = url.scheme().toLower();
    return scheme == QLatin1String("http") || scheme == QLatin1String("https")
        || scheme == QLatin1String("icy") || scheme == QLatin1String("ftp");
}

QString remoteLabel(const QString &value) {
    QUrl url = QUrl::fromUserInput(value.trimmed());
    if (url.isValid() && !url.isLocalFile() && !url.scheme().isEmpty()) {
        QString label = url.fileName();
        if (label.isEmpty()) label = url.host();
        if (label.isEmpty()) label = value;
        return label;
    }
    return QFileInfo(value).fileName();
}

TagLib::String toTString(const QString &s) {
    if (s.isEmpty())
        return TagLib::String();
    const QByteArray utf8 = s.toUtf8();
    return TagLib::String(utf8.constData(), TagLib::String::UTF8);
}

QString fromTString(const TagLib::String &s) {
    if (s.isEmpty())
        return QString();
    return QString::fromUtf8(s.toCString(true));
}

void applyTagFields(TagLib::Tag *tag, const MediaTags &tags) {
    if (!tag)
        return;
    tag->setTitle(toTString(tags.title));
    tag->setArtist(toTString(tags.artist));
    tag->setAlbum(toTString(tags.album));
    tag->setComment(toTString(tags.comment));
    tag->setGenre(toTString(tags.genre));
    bool ok = false;
    const unsigned int year = tags.year.trimmed().toUInt(&ok);
    tag->setYear(ok ? year : 0);
    tag->setTrack(tags.track > 0 ? static_cast<unsigned int>(tags.track) : 0);
}

MediaTags extractTagFields(const TagLib::Tag *tag) {
    MediaTags out;
    if (!tag)
        return out;
    out.title = fromTString(tag->title()).trimmed();
    out.artist = fromTString(tag->artist()).trimmed();
    out.album = fromTString(tag->album()).trimmed();
    out.comment = fromTString(tag->comment()).trimmed();
    out.genre = fromTString(tag->genre()).trimmed();
    if (tag->year() > 0)
        out.year = QString::number(tag->year());
    out.track = static_cast<int>(tag->track());
    out.valid = true;
    return out;
}

// Prefer ID3v2 over ID3v1 when both exist (Winamp shows the richer set).
MediaTags readMpegTags(TagLib::MPEG::File &mpeg) {
    if (mpeg.hasID3v2Tag() && mpeg.ID3v2Tag())
        return extractTagFields(mpeg.ID3v2Tag());
    if (mpeg.hasID3v1Tag() && mpeg.ID3v1Tag())
        return extractTagFields(mpeg.ID3v1Tag());
    if (mpeg.tag())
        return extractTagFields(mpeg.tag());
    MediaTags empty;
    empty.valid = true; // file opened; tags simply empty
    return empty;
}

bool writeMpegTags(TagLib::MPEG::File &mpeg, const MediaTags &tags) {
    // Winamp 2.x / classic nullsoft: keep both ID3v1 and ID3v2 in sync on save.
    TagLib::ID3v2::Tag *v2 = mpeg.ID3v2Tag(true);
    TagLib::ID3v1::Tag *v1 = mpeg.ID3v1Tag(true);
    applyTagFields(v2, tags);
    applyTagFields(v1, tags);
    // ID3v1 title/artist/album are 30-char; TagLib truncates on write.
    return mpeg.save(TagLib::MPEG::File::AllTags,
                     TagLib::File::StripOthers,
                     TagLib::ID3v2::v4,
                     TagLib::File::Duplicate);
}

QString formatFromMimeOrExt(const QString &path, TagLib::File *file) {
    if (dynamic_cast<TagLib::MPEG::File *>(file))
        return QStringLiteral("MPEG");
    if (dynamic_cast<TagLib::FLAC::File *>(file))
        return QStringLiteral("FLAC");
    if (dynamic_cast<TagLib::Ogg::Vorbis::File *>(file))
        return QStringLiteral("Ogg Vorbis");
#if WINAMP_HAS_TAGLIB_OGGFLAC
    if (dynamic_cast<TagLib::Ogg::FLAC::File *>(file))
        return QStringLiteral("Ogg FLAC");
#endif
#if WINAMP_HAS_TAGLIB_OPUS
    if (dynamic_cast<TagLib::Ogg::Opus::File *>(file))
        return QStringLiteral("Opus");
#endif
    if (dynamic_cast<TagLib::MP4::File *>(file))
        return QStringLiteral("MP4/AAC");
    if (dynamic_cast<TagLib::RIFF::WAV::File *>(file))
        return QStringLiteral("WAV");
    if (dynamic_cast<TagLib::RIFF::AIFF::File *>(file))
        return QStringLiteral("AIFF");
    const QString ext = QFileInfo(path).suffix().toUpper();
    return ext.isEmpty() ? QStringLiteral("Unknown") : ext;
}

} // namespace

namespace TagMetadata {

bool canTagFile(const QString &filePath) {
    if (filePath.isEmpty() || isRemoteLocation(filePath))
        return false;
    QFileInfo fi(filePath);
    return fi.exists() && fi.isFile() && fi.isReadable();
}

MediaTags readTags(const QString &filePath) {
    MediaTags out;
    if (!canTagFile(filePath))
        return out;

    const QByteArray pathUtf8 = QFile::encodeName(filePath);
    TagLib::FileName fn(pathUtf8.constData());

    // MPEG path: explicit ID3v1/v2 handling
    {
        TagLib::MPEG::File mpeg(fn, true, TagLib::AudioProperties::Accurate);
        if (mpeg.isValid())
            return readMpegTags(mpeg);
    }

    TagLib::FileRef ref(fn, true, TagLib::AudioProperties::Accurate);
    if (ref.isNull() || !ref.tag())
        return out;
    out = extractTagFields(ref.tag());
    return out;
}

bool writeTags(const QString &filePath, const MediaTags &tags, QString *errorOut) {
    if (!canTagFile(filePath)) {
        if (errorOut)
            *errorOut = QStringLiteral("File is missing, remote, or unreadable.");
        return false;
    }
    if (!QFileInfo(filePath).isWritable()) {
        if (errorOut)
            *errorOut = QStringLiteral("File is not writable.");
        return false;
    }

    const QByteArray pathUtf8 = QFile::encodeName(filePath);
    TagLib::FileName fn(pathUtf8.constData());

    {
        TagLib::MPEG::File mpeg(fn, false);
        if (mpeg.isValid()) {
            if (!writeMpegTags(mpeg, tags)) {
                if (errorOut)
                    *errorOut = QStringLiteral("Failed to save MPEG ID3 tags.");
                return false;
            }
            return true;
        }
    }

    TagLib::FileRef ref(fn, false);
    if (ref.isNull() || !ref.tag()) {
        if (errorOut)
            *errorOut = QStringLiteral("Unsupported or unreadable audio format.");
        return false;
    }
    applyTagFields(ref.tag(), tags);
    if (!ref.save()) {
        if (errorOut)
            *errorOut = QStringLiteral("TagLib failed to write tags.");
        return false;
    }
    return true;
}

bool stripTags(const QString &filePath, QString *errorOut) {
    if (!canTagFile(filePath)) {
        if (errorOut)
            *errorOut = QStringLiteral("File is missing, remote, or unreadable.");
        return false;
    }
    if (!QFileInfo(filePath).isWritable()) {
        if (errorOut)
            *errorOut = QStringLiteral("File is not writable.");
        return false;
    }

    const QByteArray pathUtf8 = QFile::encodeName(filePath);
    TagLib::FileName fn(pathUtf8.constData());

    TagLib::MPEG::File mpeg(fn, false);
    if (mpeg.isValid()) {
        // Strip all tag blocks (classic "remove ID3" behavior).
        mpeg.strip(TagLib::MPEG::File::AllTags);
        if (!mpeg.save()) {
            if (errorOut)
                *errorOut = QStringLiteral("Failed to strip ID3 tags.");
            return false;
        }
        return true;
    }

    // Generic: clear fields and save.
    TagLib::FileRef ref(fn, false);
    if (ref.isNull() || !ref.tag()) {
        if (errorOut)
            *errorOut = QStringLiteral("Unsupported or unreadable audio format.");
        return false;
    }
    MediaTags empty;
    empty.valid = true;
    applyTagFields(ref.tag(), empty);
    if (!ref.save()) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to clear tags.");
        return false;
    }
    return true;
}

MediaAudioInfo readAudioInfo(const QString &filePath) {
    MediaAudioInfo info;
    if (!canTagFile(filePath))
        return info;

    const QByteArray pathUtf8 = QFile::encodeName(filePath);
    TagLib::FileName fn(pathUtf8.constData());

    TagLib::FileRef ref(fn, true, TagLib::AudioProperties::Accurate);
    if (ref.isNull() || !ref.file())
        return info;

    info.format = formatFromMimeOrExt(filePath, ref.file());
    info.valid = true;

    if (auto *mpeg = dynamic_cast<TagLib::MPEG::File *>(ref.file())) {
        info.hasId3v1 = mpeg->hasID3v1Tag();
        info.hasId3v2 = mpeg->hasID3v2Tag();
        info.details = QStringLiteral("MPEG audio");
    }

    if (TagLib::AudioProperties *props = ref.audioProperties()) {
        info.lengthSeconds = props->lengthInSeconds();
        info.bitrateKbps = props->bitrate();
        info.sampleRate = props->sampleRate();
        info.channels = props->channels();
    }
    return info;
}

QStringList id3v1Genres() {
    QStringList list;
    const TagLib::StringList genres = TagLib::ID3v1::genreList();
    for (const auto &g : genres)
        list.append(fromTString(g));
    return list;
}

QString displayTitle(const MediaTags &tags, const QString &filePathFallback) {
    if (tags.valid) {
        if (!tags.artist.isEmpty() && !tags.title.isEmpty())
            return tags.artist + QStringLiteral(" - ") + tags.title;
        if (!tags.title.isEmpty())
            return tags.title;
        if (!tags.artist.isEmpty())
            return tags.artist;
    }
    if (isRemoteLocation(filePathFallback))
        return remoteLabel(filePathFallback);
    return QFileInfo(filePathFallback).completeBaseName();
}

QString displayTitle(const QString &filePath) {
    if (isRemoteLocation(filePath))
        return remoteLabel(filePath);
    return displayTitle(readTags(filePath), filePath);
}

QString sortTitleKey(const QString &filePath) {
    if (isRemoteLocation(filePath))
        return remoteLabel(filePath).toLower();
    const MediaTags t = readTags(filePath);
    if (t.valid && (!t.artist.isEmpty() || !t.title.isEmpty())) {
        return (t.artist + QLatin1Char('\x1f') + t.title).toLower();
    }
    return QFileInfo(filePath).completeBaseName().toLower();
}

} // namespace TagMetadata

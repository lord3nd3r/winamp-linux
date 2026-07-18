// mpris2_adaptors.h — DBus MPRIS2 Integration
#pragma once

#include <QObject>
#include <QStringList>
#include <QMap>
#include <QVariant>
#if defined(QT_DBUS_LIB)
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QDBusMessage>
#include <QDBusConnection>
#endif
#include <QApplication>
#include <QMediaPlayer>
#include <QMediaMetaData>

#if defined(QT_DBUS_LIB) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class Mpris2RootAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)

public:
    explicit Mpris2RootAdaptor(QObject *parent) : QDBusAbstractAdaptor(parent) {}

    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return "Winamp"; }
    QStringList supportedUriSchemes() const { return {"file", "http", "https"}; }
    QStringList supportedMimeTypes() const {
        return {"audio/mpeg", "audio/x-wav", "audio/ogg", "audio/flac", "audio/x-m4a", 
                "audio/aac", "audio/opus", "audio/x-ms-wma"};
    }

public slots:
    void Raise() {
        QWidget *w = qobject_cast<QWidget*>(parent());
        if (w) { w->show(); w->raise(); w->activateWindow(); }
    }
    void Quit() { QApplication::quit(); }
};

class Mpris2PlayerAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ minimumRate)
    Q_PROPERTY(double MaximumRate READ maximumRate)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPause)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl)

public:
    Mpris2PlayerAdaptor(QMediaPlayer *player, QAudioOutput *audioOut, QObject *parent)
        : QDBusAbstractAdaptor(parent), m_player(player), m_audioOutput(audioOut) {
        
        connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this]() {
            emitPropertyChanged("PlaybackStatus", playbackStatus());
        });
        connect(m_player, &QMediaPlayer::metaDataChanged, this, [this]() {
            emitPropertyChanged("Metadata", metadata());
        });
        connect(m_player, &QMediaPlayer::positionChanged, this, [this]() {
            // Don't spam position updates — only on significant changes
        });
    }

    QString playbackStatus() const {
        switch (m_player->playbackState()) {
            case QMediaPlayer::PlayingState: return "Playing";
            case QMediaPlayer::PausedState: return "Paused";
            default: return "Stopped";
        }
    }
    
    double rate() const { return 1.0; }
    void setRate(double) {} // Not supported
    double minimumRate() const { return 1.0; }
    double maximumRate() const { return 1.0; }

    QVariantMap metadata() const {
        QVariantMap map;
        map["mpris:trackid"] = QVariant::fromValue(QDBusObjectPath("/org/mpris/MediaPlayer2/CurrentTrack"));
        if (m_player->duration() > 0)
            map["mpris:length"] = m_player->duration() * 1000; // microseconds
        
        QMediaMetaData meta = m_player->metaData();
        QString title = meta.stringValue(QMediaMetaData::Title);
        if (!title.isEmpty()) map["xesam:title"] = title;
        
        QString artist = meta.stringValue(QMediaMetaData::AlbumArtist);
        if (artist.isEmpty()) artist = meta.stringValue(QMediaMetaData::ContributingArtist);
        if (!artist.isEmpty()) map["xesam:artist"] = QStringList{artist};
        
        QString album = meta.stringValue(QMediaMetaData::AlbumTitle);
        if (!album.isEmpty()) map["xesam:album"] = album;
        
        QUrl source = waSource(m_player);
        if (source.isValid()) map["xesam:url"] = source.toString();
        
        return map;
    }

    double volume() const { return m_audioOutput->volume(); }
    void setVolume(double v) { m_audioOutput->setVolume(qBound(0.0, v, 1.0)); }

    qlonglong position() const { return m_player->position() * 1000; } // microseconds

    bool canGoNext() const { return true; }
    bool canGoPrevious() const { return true; }
    bool canPlay() const { return true; }
    bool canPause() const { return true; }
    bool canSeek() const { return m_player->duration() > 0; }
    bool canControl() const { return true; }

public slots:
    void Next();
    void Previous();
    void Pause() { m_player->pause(); }
    void PlayPause() {
        if (m_player->playbackState() == QMediaPlayer::PlayingState)
            m_player->pause();
        else
            m_player->play();
    }
    void Stop() { m_player->stop(); }
    void Play() { m_player->play(); }
    void Seek(qlonglong offset) {
        qint64 newPos = m_player->position() + offset / 1000; // from microseconds
        m_player->setPosition(qBound(0LL, newPos, m_player->duration()));
    }
    void SetPosition(const QDBusObjectPath &, qlonglong pos) {
        m_player->setPosition(pos / 1000); // from microseconds
    }
    void OpenUri(const QString &uri);

private:
    void emitPropertyChanged(const QString &property, const QVariant &value) {
        QDBusMessage msg = QDBusMessage::createSignal(
            "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties",
            "PropertiesChanged");
        msg << "org.mpris.MediaPlayer2.Player";
        QVariantMap changedProps;
        changedProps[property] = value;
        msg << changedProps;
        msg << QStringList();
        QDBusConnection::sessionBus().send(msg);
    }

    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;
};
#endif


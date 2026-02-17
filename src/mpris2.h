#ifndef MPRIS2_H
#define MPRIS2_H

#ifdef QT_DBUS_LIB

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QMediaMetaData>
#include <QApplication>
#include <QVariant>
#include <QWidget>

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
    explicit Mpris2RootAdaptor(QObject *parent);

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
    void Raise();
    void Quit();
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
    Mpris2PlayerAdaptor(QMediaPlayer *player, QAudioOutput *audioOut, QObject *parent);

    QString playbackStatus() const;
    double rate() const { return 1.0; }
    void setRate(double) {}
    double minimumRate() const { return 1.0; }
    double maximumRate() const { return 1.0; }
    QVariantMap metadata() const;
    double volume() const;
    void setVolume(double v);
    qlonglong position() const;

    bool canGoNext() const { return true; }
    bool canGoPrevious() const { return true; }
    bool canPlay() const { return true; }
    bool canPause() const { return true; }
    bool canSeek() const;
    bool canControl() const { return true; }

public slots:
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qlonglong offset);
    void SetPosition(const QDBusObjectPath &, qlonglong pos);
    void OpenUri(const QString &uri);

private:
    void emitPropertyChanged(const QString &property, const QVariant &value);

    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;
};

#endif // QT_DBUS_LIB

#endif // MPRIS2_H

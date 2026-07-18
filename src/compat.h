// compat.h — Qt5/Qt6 compatibility shims for Winamp Linux
// Provides a unified API surface across Qt5 and Qt6 for media player,
// audio output, mouse event handling, and network stream operations.
#pragma once

#include <QMediaPlayer>
#include <QMouseEvent>
#include <QPixmap>
#include <QPainter>
#include <QIcon>
#include <QIODevice>
#include <QUrl>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#define playbackState state
#define playbackStateChanged stateChanged
#define hasVideoChanged videoAvailableChanged
using WAudioOutput = QObject;
static inline QPoint waMouseGlobalPos(const QMouseEvent *event) {
    return event->globalPos();
}
static inline QPointF waMousePos(const QMouseEvent *event) {
    return event->localPos();
}
static inline void waSetSource(QMediaPlayer *player, const QUrl &url) {
    player->setMedia(url);
}
static inline QUrl waSource(const QMediaPlayer *player) {
    return player->media().request().url();
}
static inline WAudioOutput *waCreateAudioOutput(QObject *parent) {
    Q_UNUSED(parent);
    return nullptr;
}
static inline void waAttachAudioOutput(QMediaPlayer *player, WAudioOutput *audioOutput) {
    Q_UNUSED(player);
    Q_UNUSED(audioOutput);
}
static inline void waSetOutputVolume(QMediaPlayer *player, WAudioOutput *audioOutput, float volume) {
    Q_UNUSED(audioOutput);
    int vol100 = qBound(0, static_cast<int>(volume * 100.0f), 100);
    player->setVolume(vol100);
}
static inline float waOutputVolume(const QMediaPlayer *player, const WAudioOutput *audioOutput) {
    Q_UNUSED(audioOutput);
    return player->volume() / 100.0f;
}

static inline void waSetNetworkStream(QMediaPlayer *player, QIODevice *device, const QUrl &sourceUrl) {
    player->setMedia(QMediaContent(), device);
    Q_UNUSED(sourceUrl);
}

static inline QIcon createFallbackAppIcon() {
    QPixmap pix(64, 64);
    pix.fill(QColor(18, 18, 18));

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 255, 0));
    p.drawRoundedRect(QRectF(6, 6, 52, 52), 8, 8);
    p.setPen(QColor(12, 12, 12));
    QFont font = p.font();
    font.setBold(true);
    font.setPointSize(24);
    p.setFont(font);
    p.drawText(pix.rect(), Qt::AlignCenter, "W");
    return QIcon(pix);
}
#else
#include <QAudioOutput>
using WAudioOutput = QAudioOutput;
static inline QPoint waMouseGlobalPos(const QMouseEvent *event) {
    return event->globalPosition().toPoint();
}
static inline QPointF waMousePos(const QMouseEvent *event) {
    return event->position();
}
static inline void waSetSource(QMediaPlayer *player, const QUrl &url) {
    player->setSource(url);
}
static inline QUrl waSource(const QMediaPlayer *player) {
    return player->source();
}
static inline WAudioOutput *waCreateAudioOutput(QObject *parent) {
    return new QAudioOutput(parent);
}
static inline void waAttachAudioOutput(QMediaPlayer *player, WAudioOutput *audioOutput) {
    player->setAudioOutput(audioOutput);
}
static inline void waSetOutputVolume(QMediaPlayer *player, WAudioOutput *audioOutput, float volume) {
    Q_UNUSED(player);
    audioOutput->setVolume(volume);
}
static inline float waOutputVolume(const QMediaPlayer *player, const WAudioOutput *audioOutput) {
    Q_UNUSED(player);
    return audioOutput->volume();
}

static inline void waSetNetworkStream(QMediaPlayer *player, QIODevice *device, const QUrl &sourceUrl) {
    player->setSourceDevice(device, sourceUrl);
}

static inline QIcon createFallbackAppIcon() {
    QPixmap pix(64, 64);
    pix.fill(QColor(18, 18, 18));

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 255, 0));
    p.drawRoundedRect(QRectF(6, 6, 52, 52), 8, 8);
    p.setPen(QColor(12, 12, 12));
    QFont font = p.font();
    font.setBold(true);
    font.setPointSize(24);
    p.setFont(font);
    p.drawText(pix.rect(), Qt::AlignCenter, "W");
    return QIcon(pix);
}
#endif

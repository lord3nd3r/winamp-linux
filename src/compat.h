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
#include <QEventLoop>
#include <QTimer>
#include <QPointer>
#include <QHash>
#include <QSet>

// Qt 6.8's FFmpeg backend cancels an in-flight avformat_open_input by poking
// an interrupt callback and then freeing the format context. That races: the
// demuxer thread segfaults (playback_test on Fedora 41, openSUSE Tumbleweed,
// Ubuntu Resolute). Never call setSource, and never destroy the player, while
// mediaStatus() is still LoadingMedia. A later setSource is remembered and
// applied once the open finishes or fails on its own.
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
struct WaSourceRequest {
    int epoch = 0;
    bool draining = false;
    bool pending = false;
    bool sealed = false; // teardown: drop coalesced opens, do not start another
    bool useDevice = false;
    QUrl url;
    QIODevice *device = nullptr;
};

struct WaSourceGate {
    QHash<QMediaPlayer *, WaSourceRequest> requests;
    QSet<QMediaPlayer *> hooked;
    // Leaked on purpose so it outlives every player and is not torn down
    // after QApplication.
    QObject *context = new QObject();
};

inline WaSourceGate &waSourceGate() {
    static WaSourceGate gate;
    return gate;
}

inline void waApplySourceRequest(QMediaPlayer *player, const WaSourceRequest &req) {
    if (req.useDevice)
        player->setSourceDevice(req.device, req.url);
    else
        player->setSource(req.url);
}

inline bool waDrainMediaLoad(QMediaPlayer *player) {
    if (!player || player->mediaStatus() != QMediaPlayer::LoadingMedia)
        return true;
    if (waSourceGate().requests.value(player).draining)
        return false;
    waSourceGate().requests[player].draining = true;
    QEventLoop loop;
    QPointer<QMediaPlayer> alive(player);
    const QMetaObject::Connection statusConn = QObject::connect(
        player, &QMediaPlayer::mediaStatusChanged, &loop,
        [&loop](QMediaPlayer::MediaStatus status) {
            if (status != QMediaPlayer::LoadingMedia)
                loop.quit();
        });
    QObject::connect(player, &QObject::destroyed, &loop, &QEventLoop::quit);
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    // FFmpeg's own network timeout is 5s. Stop waiting shortly after that.
    timeout.start(8000);
    loop.exec();
    QObject::disconnect(statusConn);
    if (!alive)
        return false;
    waSourceGate().requests[alive.data()].draining = false;
    return alive->mediaStatus() != QMediaPlayer::LoadingMedia;
}

inline void waPumpSource(QMediaPlayer *player) {
    if (!player)
        return;
    WaSourceGate &gate = waSourceGate();
    const WaSourceRequest state = gate.requests.value(player);
    if (state.sealed || state.draining)
        return;
    if (player->mediaStatus() == QMediaPlayer::LoadingMedia) {
        if (!waDrainMediaLoad(player))
            return;
        if (!gate.requests.contains(player) || gate.requests.value(player).sealed)
            return;
    }
    if (player->mediaStatus() == QMediaPlayer::LoadingMedia)
        return;
    WaSourceRequest req = gate.requests.value(player);
    if (!req.pending)
        return;
    gate.requests[player].pending = false;
    gate.requests[player].useDevice = false;
    const int epoch = req.epoch;
    waApplySourceRequest(player, req);
    const WaSourceRequest now = gate.requests.value(player);
    // setSource emits synchronously. A slot may have queued a newer url.
    // Apply it only when that call did not itself start an open.
    if (now.pending && now.epoch != epoch &&
        player->mediaStatus() != QMediaPlayer::LoadingMedia)
        waPumpSource(player);
}

inline void waEnsureSourcePump(QMediaPlayer *player) {
    WaSourceGate &gate = waSourceGate();
    if (!player || gate.hooked.contains(player))
        return;
    gate.hooked.insert(player);
    // Queued so this does not re-enter setMedia. The player may already be
    // destroyed by the time the event runs (the previous test's open finishing
    // during this one's drain), so the pointer is guarded.
    const QPointer<QMediaPlayer> alive(player);
    QObject::connect(player, &QMediaPlayer::mediaStatusChanged, gate.context,
                     [alive](QMediaPlayer::MediaStatus status) {
                         if (!alive || status == QMediaPlayer::LoadingMedia)
                             return;
                         waPumpSource(alive.data());
                     },
                     Qt::QueuedConnection);
    QObject::connect(player, &QObject::destroyed, gate.context, [player]() {
        WaSourceGate &gate = waSourceGate();
        gate.requests.remove(player);
        gate.hooked.remove(player);
    });
}

inline void waQueueSource(QMediaPlayer *player, const QUrl &url, QIODevice *device, bool useDevice) {
    if (!player)
        return;
    waEnsureSourcePump(player);
    WaSourceRequest &slot = waSourceGate().requests[player];
    if (slot.sealed)
        return;
    ++slot.epoch;
    slot.pending = true;
    slot.useDevice = useDevice;
    slot.url = url;
    slot.device = device;
    if (slot.draining)
        return;
    waPumpSource(player);
}
#endif

// Drop any coalesced setSource and wait until an in-flight open finishes.
// Used from destructors. Below Qt 6.8 the cancel path does not crash.
static inline void waWaitForMediaLoad(QMediaPlayer *player) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (!player)
        return;
    WaSourceRequest &slot = waSourceGate().requests[player];
    slot.sealed = true;
    slot.pending = false;
    waDrainMediaLoad(player);
    if (waSourceGate().requests.contains(player))
        waSourceGate().requests[player].pending = false;
#else
    Q_UNUSED(player);
#endif
}

// If the open is still running after the wait, detach the player so ~QObject
// does not cancel it. It deletes itself once the open settles.
static inline void waReleaseMediaPlayer(QMediaPlayer *player) {
    if (!player)
        return;
    QPointer<QMediaPlayer> alive(player);
    waWaitForMediaLoad(player);
    if (!alive)
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (alive->mediaStatus() != QMediaPlayer::LoadingMedia)
        return;
    alive->setParent(nullptr);
    QObject::connect(alive.data(), &QMediaPlayer::mediaStatusChanged, waSourceGate().context,
                     [alive](QMediaPlayer::MediaStatus status) {
                         if (alive && status != QMediaPlayer::LoadingMedia)
                             alive->deleteLater();
                     });
#endif
}

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
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    waQueueSource(player, url, nullptr, false);
#else
    player->setSource(url);
#endif
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    waQueueSource(player, sourceUrl, device, true);
#else
    player->setSourceDevice(device, sourceUrl);
#endif
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

// Wayland forbids clients positioning their own windows, so move() is a no-op
// there. Hand the drag to the compositor instead; returns true if it took it.
#include <QGuiApplication>
#include <QWidget>
#include <QWindow>
static inline bool waSystemMove(QWidget *w) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QWindow *h = w->window()->windowHandle();
    return QGuiApplication::platformName() == "wayland" && h && h->startSystemMove();
#else
    Q_UNUSED(w);
    return false;
#endif
}

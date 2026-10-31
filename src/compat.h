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
#include <QCoreApplication>

// Qt 6.8's FFmpeg backend cancels an in-flight avformat_open_input, then frees
// the format context while the demuxer thread is still inside it. That
// segfaults. Remember a replacement source and apply it only after the open
// finishes or fails on its own. Teardown seals the player so the wait cannot
// start another open.
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
struct WaSourceRequest {
    int epoch = 0;
    bool draining = false;
    bool sealed = false;
    bool pending = false;
    bool useDevice = false;
    QUrl url;
    QIODevice *device = nullptr;
};

struct WaSourceGate {
    QHash<QMediaPlayer *, WaSourceRequest> requests;
    QSet<QMediaPlayer *> hooked;
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
    timeout.start(8000);
    loop.exec();
    QObject::disconnect(statusConn);
    if (!alive)
        return false;
    waSourceGate().requests[alive.data()].draining = false;
    return alive->mediaStatus() != QMediaPlayer::LoadingMedia;
}

inline void waPumpSource(QMediaPlayer *player);

inline void waEnsureSourcePump(QMediaPlayer *player) {
    WaSourceGate &gate = waSourceGate();
    if (!player || gate.hooked.contains(player))
        return;
    gate.hooked.insert(player);
    const QPointer<QMediaPlayer> alive(player);
    QObject::connect(player, &QMediaPlayer::mediaStatusChanged, gate.context,
                     [alive](QMediaPlayer::MediaStatus status) {
                         if (!alive || status == QMediaPlayer::LoadingMedia)
                             return;
                         waPumpSource(alive.data());
                     },
                     Qt::QueuedConnection);
    QObject::connect(player, &QObject::destroyed, gate.context, [player]() {
        WaSourceGate &g = waSourceGate();
        g.requests.remove(player);
        g.hooked.remove(player);
    });
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
    if (now.pending && now.epoch != epoch &&
        player->mediaStatus() != QMediaPlayer::LoadingMedia)
        waPumpSource(player);
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

inline void waReleaseMediaPlayerImpl(QMediaPlayer *player) {
    if (!player)
        return;
    QPointer<QMediaPlayer> alive(player);
    {
        WaSourceRequest &slot = waSourceGate().requests[player];
        slot.sealed = true;
        slot.pending = false;
    }
    // A failed open posts its completion back to this thread after FFmpeg has
    // already logged the error. Destroying the player before that callback runs
    // cancels avformat while it is still closing the context (SIGSEGV after a
    // 404 on Qt 6.10 / FFmpeg 7.1). Deliver the callback first, then wait out
    // any load that is still in progress.
    for (int pass = 0; alive && pass < 3; ++pass) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        if (!alive)
            return;
        if (alive->mediaStatus() != QMediaPlayer::LoadingMedia)
            break;
        if (!waDrainMediaLoad(alive.data()))
            break;
    }
    if (!alive)
        return;
    if (waSourceGate().requests.contains(alive.data()))
        waSourceGate().requests[alive.data()].pending = false;
    if (alive->mediaStatus() != QMediaPlayer::LoadingMedia)
        return;
    alive->setParent(nullptr);
    QObject::connect(alive.data(), &QMediaPlayer::mediaStatusChanged, waSourceGate().context,
                     [alive](QMediaPlayer::MediaStatus status) {
                         if (alive && status != QMediaPlayer::LoadingMedia)
                             alive->deleteLater();
                     });
}
#endif

static inline void waReleaseMediaPlayer(QMediaPlayer *player) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    waReleaseMediaPlayerImpl(player);
#else
    Q_UNUSED(player);
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
static inline bool waCanPositionWindows() {
    return QGuiApplication::platformName() != QLatin1String("wayland");
}

static inline bool waSystemMove(QWidget *w) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QWindow *h = w->window()->windowHandle();
    return QGuiApplication::platformName() == "wayland" && h && h->startSystemMove();
#else
    Q_UNUSED(w);
    return false;
#endif
}

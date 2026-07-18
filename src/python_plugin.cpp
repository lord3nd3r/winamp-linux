// python_plugin.cpp — Sandbox-Isolated Python Plugin Host Manager implementation
#include "python_plugin.h"
#include "winamp_window.h"
#include "playlist.h"

#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTextStream>
#include <QFile>
#include <QDebug>
#include <QMediaPlayer>

PythonPluginManager::PythonPluginManager(WinampWindow *w)
    : QObject(nullptr)
    , winamp(w)
    , process(nullptr)
{
    QString pluginDir =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/winamp/plugins";
    QDir().mkpath(pluginDir);

    // Host script lives outside the user plugins dir so it does not appear in Preferences
    QString hostDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/winamp";
    QDir().mkpath(hostDir);
    hostScriptPath = hostDir + "/winamp_plugin_host.py";
    writeHostScript();

    qDebug() << "[Python Plugins] Spawning sandbox plugin host process...";

    process = new QProcess(this);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(process, &QProcess::readyReadStandardOutput,
            this, &PythonPluginManager::handleReadyRead);
    connect(process, &QProcess::readyReadStandardError, this, [this]() {
        const QByteArray err = process->readAllStandardError();
        if (!err.isEmpty())
            qWarning().noquote() << "[Python Plugins]" << QString::fromLocal8Bit(err).trimmed();
    });
    connect(process, &QProcess::errorOccurred,
            this, &PythonPluginManager::handleProcessError);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PythonPluginManager::handleProcessFinished);

    process->start(QStringLiteral("python3"), QStringList() << hostScriptPath);

    if (process->waitForStarted(2000)) {
        qDebug() << "[Python Plugins] Host process started successfully (PID:"
                 << process->processId() << ")";
        sendEvent(QStringLiteral("start"));
    } else {
        qWarning() << "[Python Plugins] Failed to start plugin host process:"
                   << process->errorString();
    }
}

PythonPluginManager::~PythonPluginManager()
{
    if (process && process->state() == QProcess::Running) {
        qDebug() << "[Python Plugins] Shutting down plugin host process...";
        sendEvent(QStringLiteral("exit"));
        if (!process->waitForFinished(1000)) {
            process->terminate();
            if (!process->waitForFinished(1000))
                process->kill();
        }
    }
}

void PythonPluginManager::handleReadyRead()
{
    if (!process)
        return;

    while (process->canReadLine()) {
        QByteArray line = process->readLine().trimmed();
        if (line.isEmpty())
            continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        // Host RPC is JSON-only; ignore non-JSON lines (stray plugin print/noise)
        if (line.isEmpty() || line.at(0) != '{') {
            qDebug().noquote() << "[Python Plugins]" << QString::fromUtf8(line);
            continue;
        }
        if (err.error != QJsonParseError::NoError) {
            qWarning() << "[Python Plugins] JSON parse error in host stdout:"
                       << err.errorString() << "Line:" << line;
            continue;
        }

        QJsonObject obj = doc.object();
        const QString type = obj.value(QStringLiteral("type")).toString();

        if (type == QLatin1String("request") || type == QLatin1String("notification")) {
            handleRequest(obj);
        } else {
            qWarning() << "[Python Plugins] Unknown message type from host:" << type;
        }
    }
}

void PythonPluginManager::handleProcessError(QProcess::ProcessError error)
{
    qWarning() << "[Python Plugins] Subprocess error occurred:" << error;
}

void PythonPluginManager::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "[Python Plugins] Subprocess finished with exit code:" << exitCode
             << "status:" << exitStatus;
}

void PythonPluginManager::handleRequest(const QJsonObject &req)
{
    const QString method = req.value(QStringLiteral("method")).toString();
    const QJsonArray params = req.value(QStringLiteral("params")).toArray();
    QJsonValue result;

    if (method == QLatin1String("play_track")) {
        if (!params.isEmpty())
            winamp->playTrack(params.at(0).toString());
    } else if (method == QLatin1String("play")) {
        if (QMediaPlayer *p = winamp->getPlayer())
            p->play();
    } else if (method == QLatin1String("pause")) {
        if (QMediaPlayer *p = winamp->getPlayer()) {
            if (p->playbackState() == QMediaPlayer::PlayingState)
                p->pause();
            else if (p->playbackState() == QMediaPlayer::PausedState)
                p->play();
        }
    } else if (method == QLatin1String("stop")) {
        if (QMediaPlayer *p = winamp->getPlayer())
            p->stop();
    } else if (method == QLatin1String("next_track")) {
        if (PlaylistWindow *pl = winamp->getPlaylistWindow())
            pl->nextTrack();
    } else if (method == QLatin1String("prev_track")) {
        if (PlaylistWindow *pl = winamp->getPlaylistWindow())
            pl->prevTrack();
    } else if (method == QLatin1String("set_volume")) {
        if (!params.isEmpty())
            winamp->setPluginVolume(qBound(0, params.at(0).toInt(), 255));
    } else if (method == QLatin1String("get_volume")) {
        result = winamp->getPluginVolume();
    } else if (method == QLatin1String("get_current_file")) {
        result = winamp->getPluginCurrentFile();
    } else if (method == QLatin1String("is_playing")) {
        QMediaPlayer *p = winamp->getPlayer();
        result = (p && p->playbackState() == QMediaPlayer::PlayingState);
    } else if (method == QLatin1String("is_paused")) {
        QMediaPlayer *p = winamp->getPlayer();
        result = (p && p->playbackState() == QMediaPlayer::PausedState);
    } else if (method == QLatin1String("get_position")) {
        QMediaPlayer *p = winamp->getPlayer();
        result = p ? p->position() / 1000.0 : 0.0;
    } else if (method == QLatin1String("get_duration")) {
        QMediaPlayer *p = winamp->getPlayer();
        result = p ? p->duration() / 1000.0 : 0.0;
    } else if (method == QLatin1String("seek")) {
        if (!params.isEmpty()) {
            if (QMediaPlayer *p = winamp->getPlayer())
                p->setPosition(static_cast<qint64>(params.at(0).toDouble() * 1000.0));
        }
    } else if (method == QLatin1String("playlist_count")) {
        PlaylistWindow *pl = winamp->getPlaylistWindow();
        result = pl ? pl->trackCount() : 0;
    } else if (method == QLatin1String("playlist_add")) {
        if (!params.isEmpty()) {
            if (PlaylistWindow *pl = winamp->getPlaylistWindow())
                pl->addTrack(params.at(0).toString());
        }
    } else if (method == QLatin1String("playlist_clear")) {
        if (PlaylistWindow *pl = winamp->getPlaylistWindow())
            pl->clearPlaylist();
    } else {
        qWarning() << "[Python Plugins] Unimplemented RPC method:" << method;
    }

    if (req.contains(QStringLiteral("id")))
        sendResponse(req.value(QStringLiteral("id")).toInt(), result);
}

void PythonPluginManager::sendResponse(int id, const QJsonValue &result)
{
    if (!process || process->state() != QProcess::Running)
        return;

    QJsonObject resp;
    resp.insert(QStringLiteral("type"), QStringLiteral("response"));
    resp.insert(QStringLiteral("id"), id);
    resp.insert(QStringLiteral("result"), result);

    process->write(QJsonDocument(resp).toJson(QJsonDocument::Compact) + '\n');
}

void PythonPluginManager::sendEvent(const QString &eventName)
{
    if (!process || process->state() != QProcess::Running)
        return;

    QJsonObject ev;
    ev.insert(QStringLiteral("type"), QStringLiteral("event"));
    ev.insert(QStringLiteral("name"), eventName);

    process->write(QJsonDocument(ev).toJson(QJsonDocument::Compact) + '\n');
}

void PythonPluginManager::writeHostScript()
{
    QFile file(hostScriptPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[Python Plugins] Failed to write host script:" << hostScriptPath;
        return;
    }

    QTextStream out(&file);
    // Host uses a dedicated stdin reader thread so synchronous API calls from
    // on_winamp_start (get_volume, etc.) do not deadlock waiting for a response
    // that nobody is reading.
    out << R"PY(#!/usr/bin/env python3
# winamp_plugin_host.py — sandboxed plugin host (generated; do not edit)
import importlib
import json
import os
import queue
import sys
import threading
import time

class WinampApiProxy:
    def __init__(self, host):
        self.host = host

    def play_track(self, path):
        self.host.send_notification('play_track', [path])

    def play(self):
        self.host.send_notification('play', [])

    def pause(self):
        self.host.send_notification('pause', [])

    def stop(self):
        self.host.send_notification('stop', [])

    def next_track(self):
        self.host.send_notification('next_track', [])

    def prev_track(self):
        self.host.send_notification('prev_track', [])

    def set_volume(self, v):
        self.host.send_notification('set_volume', [v])

    def get_volume(self):
        return self.host.send_request('get_volume', [])

    def get_current_file(self):
        return self.host.send_request('get_current_file', [])

    def is_playing(self):
        return self.host.send_request('is_playing', [])

    def is_paused(self):
        return self.host.send_request('is_paused', [])

    def get_position(self):
        return self.host.send_request('get_position', [])

    def get_duration(self):
        return self.host.send_request('get_duration', [])

    def seek(self, seconds):
        self.host.send_notification('seek', [seconds])

    def playlist_count(self):
        return self.host.send_request('playlist_count', [])

    def playlist_add(self, path):
        self.host.send_notification('playlist_add', [path])

    def playlist_clear(self):
        self.host.send_notification('playlist_clear', [])


class PluginHost:
    def __init__(self):
        self.req_id = 0
        self.pending_requests = {}
        self.lock = threading.Lock()
        self.event_queue = queue.Queue()
        self.loaded_plugins = []
        self.api = WinampApiProxy(self)

    def _write(self, msg):
        with self.lock:
            sys.stdout.write(json.dumps(msg) + '\n')
            sys.stdout.flush()

    def send_notification(self, method, params):
        self._write({'type': 'notification', 'method': method, 'params': params})

    def send_request(self, method, params):
        with self.lock:
            self.req_id += 1
            cur_id = self.req_id
            event = threading.Event()
            self.pending_requests[cur_id] = {'event': event, 'result': None}
        self._write({
            'type': 'request',
            'id': cur_id,
            'method': method,
            'params': params,
        })
        event.wait(timeout=5.0)
        with self.lock:
            resp = self.pending_requests.pop(cur_id, None)
            if resp:
                return resp['result']
        return None

    def handle_line(self, line):
        try:
            msg = json.loads(line)
        except Exception:
            return

        mtype = msg.get('type')
        if mtype == 'response':
            rid = msg.get('id')
            result = msg.get('result')
            with self.lock:
                req = self.pending_requests.get(rid)
                if req:
                    req['result'] = result
                    req['event'].set()
        elif mtype == 'event':
            self.event_queue.put(msg)

    def load_and_start_plugins(self):
        plugin_dir = os.path.expanduser('~/.config/winamp/plugins')
        if plugin_dir not in sys.path:
            sys.path.insert(0, plugin_dir)
        if not os.path.isdir(plugin_dir):
            return

        # Prefer instance API over import winamp.Api for sandbox proxy
        class WinampDummyModule:
            Api = type('Api', (), {})
        sys.modules['winamp'] = WinampDummyModule()

        for f in sorted(os.listdir(plugin_dir)):
            # Preferences disables as name.py.disabled (not loaded: not *.py)
            if not f.endswith('.py') or f.startswith('.'):
                continue
            if f == 'winamp_plugin_host.py':
                continue
            mod_name = f[:-3]
            try:
                mod = importlib.import_module(mod_name)
                self.loaded_plugins.append(mod)
                if hasattr(mod, 'on_winamp_start'):
                    mod.on_winamp_start(self.api)
                sys.stderr.write(f'[Plugin Host] Loaded {f}\n')
                sys.stderr.flush()
            except Exception as e:
                sys.stderr.write(f'[Plugin Host] Error loading {f}: {e}\n')
                sys.stderr.flush()

    def stop_plugins(self):
        for mod in self.loaded_plugins:
            try:
                if hasattr(mod, 'on_winamp_exit'):
                    mod.on_winamp_exit()
            except Exception:
                pass
        self.loaded_plugins.clear()

    def process_events(self):
        while True:
            msg = self.event_queue.get()
            if msg is None:
                break
            name = msg.get('name')
            if name == 'start':
                self.load_and_start_plugins()
            elif name == 'exit':
                self.stop_plugins()
                break


def main():
    # Keep real stdout for JSON-RPC only. Plugin print()/logging goes to stderr
    # so it cannot corrupt the control channel to C++.
    rpc_out = sys.stdout
    sys.stdout = sys.stderr

    host = PluginHost()
    # Rebind host writes to the real RPC pipe
    def _write(msg):
        with host.lock:
            rpc_out.write(json.dumps(msg) + '\n')
            rpc_out.flush()
    host._write = _write

    def reader():
        while True:
            line = sys.stdin.readline()
            if not line:
                host.event_queue.put(None)
                break
            host.handle_line(line)

    t = threading.Thread(target=reader, daemon=True)
    t.start()
    host.process_events()


if __name__ == '__main__':
    main()
)PY";
    file.close();
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                        | QFile::ReadGroup | QFile::ReadOther);
}

// python_plugin.h — Embedded Python Plugin System for Winamp Linux
#pragma once

// Python's object.h defines a 'slots' field that conflicts with Qt's 'slots' keyword.
// We must temporarily undefine it before including pybind11.
#undef slots
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/functional.h>
#define slots Q_SLOTS

#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QFileInfo>

namespace py = pybind11;

// Forward declaration — WinampWindow is defined in main.cpp before this header is included
class WinampWindow;

class WinampPythonAPI {
    WinampWindow* winamp;
public:
    WinampPythonAPI(WinampWindow* w) : winamp(w) {}

    // Playback controls using public WinampWindow API
    void play_track(const std::string& path) {
        winamp->playTrack(QString::fromStdString(path));
    }

    void play() {
        QMediaPlayer* p = winamp->getPlayer();
        if (p) p->play();
    }

    void pause() {
        QMediaPlayer* p = winamp->getPlayer();
        if (p) {
            if (p->playbackState() == QMediaPlayer::PlayingState)
                p->pause();
            else if (p->playbackState() == QMediaPlayer::PausedState)
                p->play();
        }
    }

    void stop() {
        QMediaPlayer* p = winamp->getPlayer();
        if (p) p->stop();
    }

    void next_track() {
        PlaylistWindow* pl = winamp->getPlaylistWindow();
        if (pl) pl->nextTrack();
    }

    void prev_track() {
        PlaylistWindow* pl = winamp->getPlaylistWindow();
        if (pl) pl->prevTrack();
    }

    void set_volume(int v) {
        // Use the public applyVolume after setting via the player's audio output
        v = qBound(0, v, 255);
        // Access volume via friend class
        winamp->setPluginVolume(v);
    }

    int get_volume() {
        return winamp->getPluginVolume();
    }

    std::string get_current_file() {
        return winamp->getPluginCurrentFile().toStdString();
    }

    bool is_playing() {
        QMediaPlayer* p = winamp->getPlayer();
        return p && p->playbackState() == QMediaPlayer::PlayingState;
    }

    bool is_paused() {
        QMediaPlayer* p = winamp->getPlayer();
        return p && p->playbackState() == QMediaPlayer::PausedState;
    }

    double get_position() {
        QMediaPlayer* p = winamp->getPlayer();
        return p ? p->position() / 1000.0 : 0.0;
    }

    double get_duration() {
        QMediaPlayer* p = winamp->getPlayer();
        return p ? p->duration() / 1000.0 : 0.0;
    }

    void seek(double seconds) {
        QMediaPlayer* p = winamp->getPlayer();
        if (p) p->setPosition((qint64)(seconds * 1000));
    }

    // Playlist helpers
    int playlist_count() {
        PlaylistWindow* pl = winamp->getPlaylistWindow();
        return pl ? pl->trackCount() : 0;
    }

    void playlist_add(const std::string& path) {
        PlaylistWindow* pl = winamp->getPlaylistWindow();
        if (pl) pl->addTrack(QString::fromStdString(path));
    }

    void playlist_clear() {
        PlaylistWindow* pl = winamp->getPlaylistWindow();
        if (pl) pl->clearPlaylist();
    }
};

// Expose the WinampPythonAPI to Python as the 'winamp' module
PYBIND11_EMBEDDED_MODULE(winamp, m) {
    m.doc() = "Winamp Linux Python Plugin API";

    py::class_<WinampPythonAPI>(m, "Api")
        // Playback
        .def("play_track", &WinampPythonAPI::play_track, "Play a file by path")
        .def("play", &WinampPythonAPI::play, "Resume playback")
        .def("pause", &WinampPythonAPI::pause, "Toggle pause")
        .def("stop", &WinampPythonAPI::stop, "Stop playback")
        .def("next_track", &WinampPythonAPI::next_track, "Skip to next track")
        .def("prev_track", &WinampPythonAPI::prev_track, "Go to previous track")

        // Volume
        .def("set_volume", &WinampPythonAPI::set_volume, "Set volume (0-255)")
        .def("get_volume", &WinampPythonAPI::get_volume, "Get current volume (0-255)")

        // Status
        .def("get_current_file", &WinampPythonAPI::get_current_file, "Get current file path")
        .def("is_playing", &WinampPythonAPI::is_playing, "Check if playing")
        .def("is_paused", &WinampPythonAPI::is_paused, "Check if paused")
        .def("get_position", &WinampPythonAPI::get_position, "Get position in seconds")
        .def("get_duration", &WinampPythonAPI::get_duration, "Get duration in seconds")
        .def("seek", &WinampPythonAPI::seek, "Seek to position in seconds")

        // Playlist
        .def("playlist_count", &WinampPythonAPI::playlist_count, "Get playlist track count")
        .def("playlist_add", &WinampPythonAPI::playlist_add, "Add a file to the playlist")
        .def("playlist_clear", &WinampPythonAPI::playlist_clear, "Clear the playlist");
}

class __attribute__((visibility("hidden"))) PythonPluginManager {
    py::scoped_interpreter guard{};
    WinampPythonAPI api;
    std::vector<py::module_> loadedPlugins;

public:
    PythonPluginManager(WinampWindow* w) : api(w) {
        QString pluginDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/winamp/plugins";
        QDir().mkpath(pluginDir);

        qDebug() << "[Python Plugins] Scanning:" << pluginDir;

        try {
            // Add plugin directory to Python sys.path
            py::module_ sys = py::module_::import("sys");
            sys.attr("path").attr("append")(pluginDir.toStdString());

            QDir d(pluginDir);
            QStringList pyFiles = d.entryList(QStringList() << "*.py", QDir::Files);

            for (const QString& file : pyFiles) {
                QString modName = file.chopped(3); // strip .py
                try {
                    qDebug() << "[Python Plugins] Loading:" << file;
                    py::module_ mod = py::module_::import(modName.toStdString().c_str());
                    loadedPlugins.push_back(mod);

                    if (py::hasattr(mod, "on_winamp_start")) {
                        mod.attr("on_winamp_start")(py::cast(&api, py::return_value_policy::reference));
                        qDebug() << "[Python Plugins] Initialized:" << modName;
                    }
                } catch (std::exception& e) {
                    qWarning() << "[Python Plugins] Error loading" << file << ":" << e.what();
                }
            }

            qDebug() << "[Python Plugins]" << loadedPlugins.size() << "plugin(s) loaded.";

        } catch (std::exception& e) {
            qWarning() << "[Python Plugins] Fatal error:" << e.what();
        }
    }

    ~PythonPluginManager() {
        // Call on_winamp_exit for each plugin that defines it
        for (auto& mod : loadedPlugins) {
            try {
                if (py::hasattr(mod, "on_winamp_exit")) {
                    mod.attr("on_winamp_exit")();
                }
            } catch (...) {}
        }
    }
};

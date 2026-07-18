# Winamp for Linux

Native Winamp-inspired player for Linux with classic skins, playlist and equalizer windows, projectM/Milkdrop visualization, and desktop integration. The current codebase builds with Qt5 or Qt6 and uses C++17.

![Winamp Linux](https://img.shields.io/badge/Winamp-Linux-00FF00?style=for-the-badge&logo=linux&logoColor=white)
![Qt](https://img.shields.io/badge/Qt5%2FQt6-Multimedia-41CD52?style=for-the-badge&logo=qt&logoColor=white)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)

## Highlights

- Classic Winamp-style main player
- Playlist editor and 10-band equalizer (faithful port of the original EQ10 DSP algorithm)
- Non-blocking async playlist loading with background duration probing
- Logarithmic spectrum analyzer matching the original Winamp frequency mapping
- Skin loading from installed assets, custom folders, and skin archives
- Modern Winamp 5-style skins are currently disabled because they can break the UI
- projectM/Milkdrop visualization support
- System tray integration
- MPRIS2 support on Qt6
- Bookmarks, recent files, language packs, and persisted settings
- HTTP/HTTPS stream URL playback with redirect following and automatic fallback
- Python plugin system — extend Winamp with scripts in `~/.config/winamp/plugins/`
- In-app plugin manager (Preferences → Plug-ins) to enable, disable, configure, add, and remove plugins

## Build Requirements

On Debian/Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake ninja-build \
  qtbase5-dev qtmultimedia5-dev libqt5opengl5-dev \
  qt6-base-dev qt6-multimedia-dev qt6-base-dev-tools \
  libgl-dev \
  libprojectm-dev projectm-data \
  pybind11-dev python3-dev
```

Qt5 or Qt6 will work. CMake prefers Qt6 when available and falls back to Qt5.

## Runtime Dependencies

Qt6 uses GStreamer as its multimedia backend on Linux. You'll need the GStreamer plugin
packages installed for audio format support. Without these you may see warnings like
`No decoder available for type 'application/x-id3'`.

```bash
sudo apt-get install -y \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-plugins-ugly \
  gstreamer1.0-libav
```

- **plugins-good** — ID3 tag demuxer, MP3/FLAC/OGG parsers
- **plugins-ugly** — MP3 decoder (mpg123), patent-encumbered codecs
- **plugins-bad** — Additional format support (AAC, Opus, etc.)
- **gstreamer1.0-libav** — FFmpeg-based fallback decoders

## Build

Qt5 fallback build:

```bash
cmake -S . -B build-qt5 -G Ninja -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON
ninja -C build-qt5
```

Qt6 build:

```bash
cmake -S . -B build-qt6 -G Ninja
ninja -C build-qt6
```

## Run

```bash
./build-qt5/winamp
```

or, if you built Qt6:

```bash
./build-qt6/winamp
```

## Install System-Wide

Configure with `/usr` as the install prefix, then install:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j
sudo cmake --install build
```

The install places:

- Binary: `/usr/bin/winamp`
- Desktop entry: `/usr/share/applications/winamp.desktop`
- Assets: `/usr/share/winamp`

## Packaging & Releases

The project supports CPack for generating Debian packages (`.deb`) and compressed tarballs (`.tar.gz`) locally or automatically via CI.

### Generate Packages Locally

To generate installation packages on your local machine, run the CPack package target after building:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build package
```

This generates `winamp-*.deb` and `winamp-*.tar.gz` package files in the build directory.

### Automated GitHub Releases

When you push a tag matching `v*` (e.g., `v0.5.0-beta2`), a GitHub Actions workflow automatically:
1. Spins up a build container running `ubuntu:resolute` (Ubuntu 26.04) to match the target runtime environment (e.g., Python 3.15).
2. Compiles the binary with compiler hardening and optimizations enabled.
3. Generates the `.deb` and `.tar.gz` packages.
4. Creates a new GitHub Release and uploads the packages as release assets.


## Asset Search Paths

At runtime, the app looks for skins and resources in:

- `/usr/share/winamp`
- `/usr/local/share/winamp`
- `~/.winamp/skins`
- repo-relative paths when running from the source tree

## Notes

- Both Qt5 and Qt6 build cleanly with zero errors and zero warnings.
- Qt6 enables the full MPRIS2 path (desktop media keys, KDE Connect, panel widgets).
- Qt5 builds include full streaming support; MPRIS2 and EQ DSP processing are Qt6-only.
- The app uses the real Winamp bitmap assets from the repo and installed share directories.
- User configuration is stored under `~/.config/winamp/`.
- Modern Winamp 5 skins are intentionally disabled for now; use classic skins only.

## Technical Details

- **EQ DSP**: Faithful port of George Yohng's `eq10dsp.cpp` with asymmetric Q, dynamic limiter, and pre-allocated audio buffers for glitch-free real-time processing.
- **Spectrum Analyzer**: Uses logarithmic (octave-based) FFT bin mapping matching Winamp's original `sa_tab[]` frequency distribution.
- **Playlist Loading**: Async duration probing via a single shared `QMediaPlayer` queue with 5-second timeout, replacing the old blocking-per-track pattern.
- **Gapless Playback**: Dual `QMediaPlayer` preload/swap architecture for seamless track transitions.

## Repository Layout

- `CMakeLists.txt` - build configuration and Qt selection
- `src/` - modularized C++ source code and headers
  - `main.cpp` - main application implementation (formerly `winamp_authentic.cpp`)
  - `playlist.h`, `equalizer.h`, etc. - extracted UI windows
  - `eq_dsp.h` - EQ DSP algorithm
  - `mpris2_adaptors.h` - DBus integration
  - `python_plugin.h` - embedded Python plugin system
- `plugins/examples/` - bundled example plugins (hello, Icecast DJ)
- `skins/` - default skin assets
- `assets/` - classic Winamp resource assets
- `lang/` - translation files

## Python Plugins

Winamp for Linux supports Python plugins via an embedded interpreter (pybind11). Place `.py` files in `~/.config/winamp/plugins/` and they will be loaded automatically at startup.

Plugins can also be managed from within Winamp via **Preferences → Plug-ins**, which provides enable/disable toggling, configuration file editing, adding, and removing plugins — just like the classic Windows Winamp plugin manager.

### Plugin API

Each plugin receives a `winamp.Api` object with these methods:

| Method | Description |
|---|---|
| `play_track(path)` | Play a file by path |
| `play()` / `pause()` / `stop()` | Playback controls |
| `next_track()` / `prev_track()` | Track navigation |
| `set_volume(v)` / `get_volume()` | Volume (0-255) |
| `get_current_file()` | Current file path |
| `is_playing()` / `is_paused()` | Playback state |
| `get_position()` / `get_duration()` | Time in seconds |
| `seek(seconds)` | Seek to position |
| `playlist_count()` / `playlist_add(path)` / `playlist_clear()` | Playlist management |

### Example Plugin

```python
# ~/.config/winamp/plugins/hello_winamp.py

def on_winamp_start(api):
    print(f"Volume: {api.get_volume()}/255")
    print(f"Playlist: {api.playlist_count()} tracks")

def on_winamp_exit():
    print("Goodbye!")
```

### Icecast DJ Plugin

An included plugin for DJs that streams directly to Icecast servers. Install it by copying
`plugins/examples/icecast_dj.py` to `~/.config/winamp/plugins/`. On first run it creates
`icecast_dj.json` with all available options:

- **Server**: host, port, username, password, TLS toggle
- **Mount points**: multiple mounts with independent format/bitrate/samplerate/channels
- **Formats**: MP3, OGG Vorbis, Opus, AAC, FLAC
- **DJ metadata**: name, description, URL
- **Auto-reconnect**: configurable delay and max attempts
- **Metadata push**: auto-updates Icecast song title on track change
- **Logging**: optional log file output

Requires `ffmpeg` (`sudo apt install ffmpeg`).

See `plugins/examples/` for all bundled plugins.

## License

This project is licensed under the [GNU General Public License v2.0](LICENSE.md).

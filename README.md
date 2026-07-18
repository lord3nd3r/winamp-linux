# Winamp for Linux

A fast, lightweight, native classic Winamp (2.x) clone for modern Linux desktops, written in C++17 and Qt6 (with Qt5 fallback).

![Winamp Linux](https://img.shields.io/badge/Winamp-Linux-00FF00?style=for-the-badge&logo=linux&logoColor=white)
![Qt](https://img.shields.io/badge/Qt6%2FQt5-Multimedia-41CD52?style=for-the-badge&logo=qt&logoColor=white)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)

---

## Key Features

- **Classic Skin Engine**: Supports classic Winamp skin archives (`.wsz` or `.zip`) and uncompressed skin folders.
- **Graphic Equalizer**: Faithful port of **George Yohng's EQ10 DSP** algorithm with asymmetric Q factor, preamp control, and 10 graphic bands.
- **Fast Audio Pipeline**: Pre-allocated buffers, dual-player preloading for gapless playback, and a logarithmic spectrum analyzer matching the original Winamp frequency mapping.
- **Robust URL Streaming**: Support for HTTP/HTTPS stream URL playback with automatic redirects and format decoding.
- **Sandboxed Python plugins**: Extend functionality via out-of-process Python scripts (JSON-RPC host) with lifecycle callbacks and a stable player API.
- **MPRIS2 Integration**: System media key controls, KDE Connect compatibility, and lock screen media player widget support (Qt6 only).
- **Visualization Support**: Demoscene visualizer matching Milkdrop presets (utilizing `libprojectM`).

---

## Detailed Documentation Guides

To keep the repository clean and developer-friendly, the documentation has been split into dedicated guides:

- 🛠️ **[CONTRIBUTING.md](CONTRIBUTING.md)**: Setup compiler packages, build with Qt5/Qt6, run the test suites (CTest), package release binaries (`.deb`/`.tar.gz`), and review architectural designs.
- 🐍 **[PLUGIN_DEVELOPMENT.md](PLUGIN_DEVELOPMENT.md)**: Write user-defined Python plugins under `~/.config/winamp/plugins/`, use lifecycle callbacks, thread safety rules, and review the full `winamp.Api` method reference.
- ⚙️ **[CONFIGURATION.md](CONFIGURATION.md)**: Browse configuration file paths, bookmarks lists, and check INI property options for `winamp.conf`.

---

## Quick Start

### Build Prerequisites (Debian/Ubuntu)
```bash
sudo apt-get update
sudo apt-get install -y \
  cmake ninja-build \
  qt6-base-dev qt6-multimedia-dev libqt6multimediawidgets6 \
  libgl-dev libprojectm-dev projectm-data \
  python3
```

*Note: GStreamer plugins (`gstreamer1.0-plugins-good/bad/ugly`) are required at runtime for audio decoding.*

### Compilation (Qt6)
```bash
cmake -S . -B build-qt6 -G Ninja
ninja -C build-qt6
```

### Execution
```bash
./build-qt6/winamp
```

---

## Repository Structure

- `src/` — Modular C++ C++17 source modules:
  - `main.cpp` — Application initialization and CLI flag parser.
  - `winamp_window.h` — Main Winamp widget (player logic, volumes, balance, skins).
  - `winamp_bitmaps.h` — Asset resource manager for classic bitmaps.
  - `playlist.h` & `playlist.cpp` — Playlist widget, queue sorting, async duration scanner.
  - `equalizer.h` — Equalizer slider panel.
  - `eq_dsp.h` — Core George Yohng EQ10 DSP process.
  - `python_plugin.h` — Embedded Python interpreter bindings.
  - `constants.h` — Global layouts, static variables, text glyph mappings, and skin loader.
- `tests/` — Automated Qt Test suite.
- `plugins/examples/` — Pre-loaded Python plugin templates (e.g. Icecast DJ).
- `skins/` — Default classic skin assets.
- `assets/` — Embedded Winamp retro graphical components.
- `lang/` — Localization properties for language pack transitions.

---

## License

This project is licensed under the [GNU General Public License v2.0](LICENSE.md).

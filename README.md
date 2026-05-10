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

## Build Requirements

On Debian/Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake ninja-build \
  qtbase5-dev qtmultimedia5-dev libqt5opengl5-dev \
  qt6-base-dev qt6-multimedia-dev qt6-base-dev-tools \
  libgl-dev \
  libprojectm-dev projectm-data
```

Qt5 or Qt6 will work. CMake prefers Qt6 when available and falls back to Qt5.

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
- `winamp_authentic.cpp` - main application implementation
- `skins/` - default skin assets
- `Src/Winamp/resource/` - classic Winamp resource assets
- `lang/` - translation files

## License

See `LICENSE.md`.

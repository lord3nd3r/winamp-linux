# Winamp for Linux

Native Winamp-inspired player for Linux with classic skins, playlist and equalizer windows, modern skin support, projectM/Milkdrop visualization, and desktop integration. The current codebase builds with Qt5 or Qt6 and uses C++17.

![Winamp Linux](https://img.shields.io/badge/Winamp-Linux-00FF00?style=for-the-badge&logo=linux&logoColor=white)
![Qt](https://img.shields.io/badge/Qt5%2FQt6-Multimedia-41CD52?style=for-the-badge&logo=qt&logoColor=white)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)

## Highlights

- Classic Winamp-style main player
- Playlist editor and 10-band equalizer
- Skin loading from installed assets, custom folders, and skin archives
- Modern Winamp 5-style skin support
- projectM/Milkdrop visualization support
- System tray integration
- MPRIS2 support on Qt6
- Bookmarks, recent files, language packs, and persisted settings

## Build Requirements

On Debian/Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake ninja-build \
  qtbase5-dev qtmultimedia5-dev libqt5opengl5-dev \
  qt6-base-dev qtmultimedia5-dev qt6-base-dev-tools \
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

Install the Qt5 build into `/usr`:

```bash
sudo cmake --install build-qt5 --prefix /usr
```

Install the Qt6 build into `/usr`:

```bash
sudo cmake --install build-qt6 --prefix /usr
```

The install places the binary at `/usr/bin/winamp` and assets under `/usr/share/winamp`.

## Asset Search Paths

At runtime, the app looks for skins and resources in:

- `/usr/share/winamp`
- `/usr/local/share/winamp`
- `~/.winamp/skins`
- repo-relative paths when running from the source tree

## Notes

- Qt6 enables the full MPRIS2 path.
- Qt5 builds are supported, but MPRIS2 is disabled there.
- The app uses the real Winamp bitmap assets from the repo and installed share directories.
- User configuration is stored under `~/.config/winamp/`.

## Repository Layout

- `CMakeLists.txt` - build configuration and Qt selection
- `winamp_authentic.cpp` - main application implementation
- `skins/` - default skin assets
- `Src/Winamp/resource/` - classic Winamp resource assets
- `lang/` - translation files

## License

See `LICENSE.md`.

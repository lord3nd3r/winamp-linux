# Building Winamp for Linux

Dependencies, Qt 5 and Qt 6 configures, tests, CPack, and the AppImage. The CI and release matrix is in [CI.md](CI.md). How the player is put together is in [ARCHITECTURE.md](ARCHITECTURE.md).

## Tools

| Tool | Notes |
|------|--------|
| C++17 compiler | GCC or Clang |
| CMake ≥ 3.16 | Project minimum |
| Ninja | Recommended generator |
| Qt 6 **or** Qt 5 | Qt 6 preferred |
| `libprojectM` | Visualization. CMake uses pkg-config (`libprojectM`) |
| `python3` | Plugin host at runtime (interpreter on `PATH`) |

Use `Debug` while developing and `Release` for packages.

## Debian / Ubuntu

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  file \
  dpkg-dev \
  qt6-base-dev \
  qt6-multimedia-dev \
  libqt6opengl6-dev \
  libqt6multimediawidgets6 \
  libgl-dev \
  libprojectm-dev \
  projectm-data \
  libtag1-dev \
  libavutil-dev \
  pkg-config \
  python3 \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-plugins-ugly \
  gstreamer1.0-libav
```

Decoding uses the Qt Multimedia backend. On Debian and Ubuntu that is usually GStreamer (the plugin packages above). A distro package also needs the libraries it links. The AppImage already carries Qt, TagLib, projectM, and GStreamer.

```bash
sudo apt-get install -y libprojectm3 libqt6multimediawidgets6 libqt6openglwidgets6 libtag2
```

Ubuntu 26.10 (stonking) ships `libprojectm3` 3.1.12 (`libprojectm-dev`). That `.deb` depends on `libprojectm3`, not the older `libprojectm2v5`.

| Package | Role |
|---------|------|
| `qt6-base-dev`, `libqt6opengl6-dev`, multimedia | UI, OpenGL, playback. Qt 6.8+ enables the PCM tap (spectrum, EQ, Milkdrop, balance) |
| `libprojectm-dev`, `projectm-data` | Milkdrop-compatible visualizations |
| `libtag1-dev` | ID3v1/v2 read and write (File Info / Alt+3). MP3 writes ID3v2.3 |
| `libavutil-dev` | Optional. When pkg-config finds it, CMake defines `HAVE_AVUTIL` and the player sets FFmpeg’s log level to error |
| `python3` | Out-of-process plugin host (not linked into the binary) |
| `file`, `dpkg-dev` | CPack `.deb` helpers |

Qt 5 packages: `qtbase5-dev`, `qtmultimedia5-dev`, `libqt5opengl5-dev`, `libqt5multimedia5-plugins`.

## Fedora and openSUSE

`libavutil` package names differ: `libavutil-free-devel` on Fedora, `libavutil-devel` on openSUSE. Fedora 45’s release RPM is built on `fedora:45` and links FFmpeg 9 `libavutil.so.61`. The Fedora 44 RPM still needs `libavutil.so.60`. The package lists CI installs are in [CI.md](CI.md).

## Arch Linux

Playback, the spectrum and EQ (the PCM tap), and Milkdrop were tested on real Arch hardware with Qt 6 and projectM 3.1.12.

Extra has two different libraries. Install **`projectm` 3.1.12** (`libprojectM` 3.x). Do not use **`libprojectm` 4.1.8**, and do not use the AUR package `projectm-git` (that tree is projectM 4). There is no separate AUR package that still builds 3.1.12.

```bash
sudo pacman -S --needed base-devel cmake ninja \
  qt6-base qt6-multimedia qt6-svg qt6-tools \
  taglib ffmpeg python pkgconf projectm
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

The release package is `sudo pacman -U winamp-<version>-arch.pkg.tar.zst`. The PKGBUILD is `packaging/arch/PKGBUILD` and lists `projectm` in `depends`.

## Qt 6

```bash
git clone https://github.com/lord3nd3r/winamp-linux.git
cd winamp-linux
cmake -S . -B build-qt6 -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja -C build-qt6
./build-qt6/winamp
```

MPRIS2 is enabled when Qt DBus is present. The PCM tap (`QAudioBufferOutput`) is compiled only when Qt is 6.8 or newer. That tap feeds the spectrum FFT, the EQ10 process, Milkdrop’s samples, and stereo balance. On Qt 6.2 and 6.4 the same source builds, the controls stay on screen, and dragging balance appends ` (NO TAP)`. Audio still plays through `QAudioOutput`.

`libavutil` is optional. When pkg-config finds it, configure prints `libavutil found - HAVE_AVUTIL` and `main` calls `av_log_set_level(AV_LOG_ERROR)`.

## Qt 5

```bash
cmake -S . -B build-qt5 -G Ninja \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON \
  -DCMAKE_BUILD_TYPE=Debug
ninja -C build-qt5
```

MPRIS2 is disabled. There is no PCM tap, so spectrum, EQ processing, Milkdrop samples, and balance stay on the older-Qt path. Some Qt 6-only multimedia calls are compiled out in `src/compat.h`.

CI’s `ubuntu-22.04-qt5` job is this configuration (`qtbase5-dev`, `qtmultimedia5-dev`, `libqt5opengl5-dev`, `libqt5multimedia5-plugins`, plus `libavutil-dev`) into `build-qt5`. It requires the configure log to contain `Using Qt5`.

## Compiler flags

Applied to the `winamp` target:

| Flag / define | Purpose |
|---------------|---------|
| `-Wall -Wextra -Wpedantic` | Warning surface |
| `-Wno-unused-parameter` | Qt slot signatures |
| `-fstack-protector-strong` | Stack hardening |
| `_FORTIFY_SOURCE=2` | Fortified libc checks, unless `CMAKE_CXX_FLAGS` already defines `_FORTIFY_SOURCE` (Arch `makepkg` passes 3) |

Policy: zero warnings on both the Qt 5 and Qt 6 configures.

## CMake options

| Variable | Effect |
|----------|--------|
| `CMAKE_BUILD_TYPE` | `Debug` / `Release` / `RelWithDebInfo` |
| `CMAKE_DISABLE_FIND_PACKAGE_Qt6=ON` | Force the Qt 5 path |
| `CMAKE_INSTALL_PREFIX` | Install root |
| `WINAMP_DISTRO_ID` | Suffix in package filenames (default `linux`) |
| `CPACK_GENERATOR` | `DEB;TGZ` (default) or `RPM;TGZ` |

`project(Winamp VERSION 1.4.0 ...)` in `CMakeLists.txt`, `WINAMP_VERSION_FULL`, and `kWinampVersion` in `src/constants.h` must stay aligned with the GitHub release tag. Bump all three when cutting a release and record the user-visible changes in [CHANGELOG.md](../CHANGELOG.md).

## Tests

```bash
ninja -C build-qt6
ctest --test-dir build-qt6 --output-on-failure

./build-qt6/winamp_tests
./build-qt6/playlist_tests
./build-qt6/playback_tests
./build-qt6/tag_metadata_tests

QT_QPA_PLATFORM=offscreen ctest --test-dir build-qt6 --output-on-failure
```

| Target | Sources | What it asserts |
|--------|---------|-----------------|
| `winamp_tests` / `eq_dsp_test` | `tests/test_eq_dsp.cpp` | dB conversion, setup, gains, process path, balance gain, plugin-host restart limit, Milkdrop channel counts, short sink writes |
| `playlist_tests` / `playlist_test` | `tests/test_playlist.cpp` plus playlist, EQ, preferences, and video objects | Add/clear, navigation, sort, selection, dead file removal, async folder add, M3U/PLS text, duration probes, nested skin archives |
| `playback_tests` / `playback_test` | `tests/test_playback.cpp` | Gapless player swap on every Qt, including the Qt 6.8+ FFmpeg backend (not skipped), settings write, stale playback callbacks, local playlist enqueue, previous-track wrap, repeat cycle (off → all → one), MPRIS volume and OpenUri |
| `tag_metadata_tests` / `tag_metadata_test` | `tests/test_tag_metadata.cpp` | Tag round trip, ID3v2.3 on write, ID3v1 Latin-1 truncation |

When adding a test, prefer a behavior check over a snapshot of private details. Register it with `add_executable` and `add_test` in `CMakeLists.txt`. Keep it off the network unless the case is mocked. For UI-adjacent code, extract pure logic so Qt Test can host it without a full `WinampWindow`.

## Packaging (CPack)

```bash
cmake -S . -B build-qt6 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DWINAMP_DISTRO_ID=ubuntu-24.04
ninja -C build-qt6 package

cmake -S . -B build-rpm -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DWINAMP_DISTRO_ID=fedora-43 \
  -DCPACK_GENERATOR="RPM;TGZ"
ninja -C build-rpm package
```

CPack metadata is in `CMakeLists.txt` (DEB shlibdeps, RPM autoreq, license, contact). RPM builds need `rpm-build`.

```bash
cmake --install build-qt6 --prefix /usr/local
```

| Path | Content |
|------|---------|
| `bin/winamp` | Executable |
| `share/applications/winamp.desktop` | Desktop entry |
| `share/icons/hicolor/*/apps/winamp.png` | Themed app icon (16–256 px) |
| `share/winamp/skins/` | Skins |
| `share/winamp/resource/` | Classic resource bitmaps |
| `share/winamp/lang/` | Language packs |
| `share/metainfo/io.github.lord3nd3r.winamp.metainfo.xml` | AppStream metadata |

Container example:

```bash
docker run --rm -it -v "$(pwd)":/workspace -w /workspace ubuntu:24.04 bash
# inside:
apt-get update && apt-get install -y \
  cmake ninja-build build-essential \
  qt6-base-dev qt6-multimedia-dev \
  libprojectm-dev projectm-data libtag1-dev libavutil-dev python3 \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  libgl1-mesa-dev file dpkg-dev git
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DWINAMP_DISTRO_ID=ubuntu-24.04
ninja -C build package
```

## AppImage

`packaging/appimage/build-appimage.sh` produces `Winamp-<version>-x86_64.AppImage`. The release workflow runs it on Ubuntu 22.04 (glibc 2.35, Qt 6.2.4) with `APPIMAGE_SMOKE=1`. A tag named `v1.4.0` sets `VERSION=1.4.0`, so the artifact is `Winamp-1.4.0-x86_64.AppImage` plus a `.zsync` file (`gh-releases-zsync`). linuxdeploy plus the Qt and GStreamer plugins bundle platforms (xcb, Wayland, offscreen), image formats, OpenSSL, GStreamer, TagLib, projectM presets, HarfBuzz, and FriBidi. OpenGL and `python3` stay on the host. Because that Qt is 6.2, the AppImage is a no-tap build: spectrum, EQ processing, Milkdrop samples, and balance are compiled out.

Smoke coverage: `--help`, `--version`, an offscreen launch, and `--cli` against a generated WAV. A `v*` tag attaches the AppImage to the GitHub release. `workflow_dispatch` uploads it as an artifact only.

```bash
chmod +x Winamp-1.4.0-x86_64.AppImage
./Winamp-1.4.0-x86_64.AppImage --version
```

If FUSE is missing, `APPIMAGE_EXTRACT_AND_RUN=1 ./Winamp-1.4.0-x86_64.AppImage`.

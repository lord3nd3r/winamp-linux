# Winamp for Linux — working notes (1.3.0)

Orientation for people changing this tree. Product version is **1.3.0**: `kWinampVersion` in `src/constants.h`, `project(Winamp VERSION 1.3.0)` and `WINAMP_VERSION_FULL` in `CMakeLists.txt`, and the GitHub tag `v1.3.0`. Those three strings stay in lockstep.

C++17, CMake ≥ 3.16, Ninja. Qt 6 is preferred. Qt 5 is a real fallback (`-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON`); CI job `ubuntu-22.04-qt5` builds it. MPRIS is Qt 6 + Qt DBus only.

User-facing detail lives in [README.md](README.md), [CONFIGURATION.md](CONFIGURATION.md), [CONTRIBUTING.md](CONTRIBUTING.md), and [PLUGIN_DEVELOPMENT.md](PLUGIN_DEVELOPMENT.md). This file is the short map of behavior that is easy to document wrong.

## Qt 6.8 PCM tap

`QAudioBufferOutput` exists from Qt 6.8. `WinampWindow` connects it only inside `#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)`. That callback is `processAudioBuffer`:

- spectrum FFT (`fft512`, inline in `src/eq_dsp.h`)
- EQ10 processing
- Milkdrop `feedPCM*`
- stereo balance (`playbackOutputGain`), including when the equalizer is off

Older Qt, including Qt 5 and Ubuntu’s Qt 6.2 / 6.4, still builds. Playback uses `QAudioOutput`. The equalizer window, spectrum chrome, and Milkdrop window remain; they do not see or shape PCM. Dragging the balance slider appends ` (NO TAP)`.

The AppImage is built on Ubuntu 22.04’s **Qt 6.2.4**, so it is a no-tap build. Fedora 41 and openSUSE Tumbleweed in CI are new enough to compile the tap.

## Skins

`.wsz`, `.zip`, and `.wal` are zip files read with zlib in `extractSkinArchive()` (`src/constants.h`). There is no `unzip` dependency. Nested directories are preserved. Cache: `~/.cache/winamp/skins/<name>/` with a `.source-stamp` of size, mtime, and SHA-256. A mismatch deletes the cache and extracts again. Archives over 128 MB or 512 entries are rejected.

## Plugins

`PythonPluginManager` runs `python3` on a generated `~/.cache/winamp/winamp_plugin_host.py`. That is process isolation, not an OS sandbox: no seccomp, filesystem jail, or network policy. The plugin has the user’s normal privileges.

`PluginHostRestart` (`src/python_plugin.h`): backoff 500 ms, 1 s, 2 s, 4 s, 8 s; stop after `kMaxRestarts` (5); a host up for `kStableResetMs` (30 s) resets the count.

The AppImage does not bundle `python3`.

## Playlists, MPRIS, repeat, tags

- `playlistEntries()` reads every M3U non-comment line and every PLS `FileN=`. Relative paths resolve against the playlist. Only an explicit `http`, `https`, `icy`, or `ftp` scheme is remote (`QUrl::fromUserInput("b.mp3")` must not become an HTTP URL). Remote playlists are fetched and every entry is enqueued. Nested playlists stop at 3 hops.
- MPRIS (`src/main.cpp`, Qt 6 + DBus): `setVolume` maps 0–1 onto 0–255 via `setPluginVolume`. `OpenUri` accepts a local file or `http`/`https`. `Previous` calls `playPrevious()`, which matches `playNext()` (shuffle, and repeat wraps).
- Repeat: `cycleRepeat()` is off → all → one → off. The classic button, the `R` key, and the modern repeat control share it. A stale (`repeat` false, `repeatTrack` true) pair is treated as off.
- MP3 save writes **ID3v2.3** and ID3v1 (`src/tag_metadata.cpp`). v1 is Latin-1, truncated (30 / 30 / 30 / 28 for title, artist, album, comment). Year above 9999 is omitted. Track outside 1–255 becomes 0 on v1. TagLib ≥ 1.12 uses `save(..., ID3v2::v3, DoNotDuplicate)`. TagLib 1.11 uses `save(AllTags, true, 3, false)`.

## AppImage and CI

`packaging/appimage/build-appimage.sh` on Ubuntu 22.04 (glibc 2.35). Output `Winamp-<version>-x86_64.AppImage` plus `.zsync`. linuxdeploy bundles Qt plugins (xcb, Wayland, offscreen), image formats, OpenSSL, GStreamer, TagLib, projectM, HarfBuzz, and FriBidi (libgstlibav needs FriBidi). OpenGL stays on the host. Release job `appimage` runs with `APPIMAGE_SMOKE=1` (`--help`, `--version`, offscreen, `--cli` on a generated WAV). A `v*` tag publishes the file; the filename for this release is `Winamp-1.3.0-x86_64.AppImage`.

```bash
chmod +x Winamp-1.3.0-x86_64.AppImage
./Winamp-1.3.0-x86_64.AppImage
```

CI (`.github/workflows/ci.yml`): Ubuntu 22.04, Ubuntu 24.04, Debian Bookworm, Fedora 41, openSUSE Tumbleweed, and `ubuntu-22.04-qt5`. Each job installs libavutil (`libavutil-dev`, Fedora `libavutil-free-devel`, openSUSE `libavutil-devel`) and greps for `libavutil found - HAVE_AVUTIL`.

## Tests

```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

| CTest name | Binary | Source |
|------------|--------|--------|
| `eq_dsp_test` | `winamp_tests` | `tests/test_eq_dsp.cpp` |
| `playlist_test` | `playlist_tests` | `tests/test_playlist.cpp` |
| `playback_test` | `playback_tests` | `tests/test_playback.cpp` |
| `tag_metadata_test` | `tag_metadata_tests` | `tests/test_tag_metadata.cpp` |

`playback_test` covers gapless swap, settings, stale callbacks, playlist enqueue, previous-track wrap, the repeat cycle, and MPRIS volume / OpenUri.

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Warnings are expected to stay at zero (`-Wall -Wextra -Wpedantic`). Do not allocate on the EQ hot path. Classic skins are the primary UI.

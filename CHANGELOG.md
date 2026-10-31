# Changelog

All notable changes to Winamp for Linux are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Entries come from the published GitHub release notes and the commits between tags.
Release package text that only repeats the distro matrix is summarized, not copied in full.

## [Unreleased]

## [1.3.1] - 2026-10-30

### Fixed

- On Qt 6.8 and newer, changing tracks or quitting while a file is still opening no longer cancels FFmpeg mid-open (that cancel crashed the player). The open's completion is delivered before the player is destroyed. The gapless playback tests run on that FFmpeg backend; they are not skipped.
- Dropping a file on the window, or playing a bookmark, no longer resumes the previous playlist's preloaded next track when the new file ends.
- Clicking a playlist row only moves the highlight. Next, Previous, and gapless follow the track that is actually playing.
- Previous is the same action from the main window, the Z key, the tray, the terminal UI, plugins, and MPRIS (shuffle and repeat wrap).
- A playlist snapped to the side of the main window keeps its vertical position instead of jumping to the title bar.
- Closing the shaded playlist turns the main window's playlist button off.
- A brief HTTP stall is not treated as a failed stream.
- Desktop shells that watch MPRIS see a new track id when the song changes.
- Starting the Python plugin host no longer freezes the window.

### Changed

- On native Wayland (`QT_QPA_PLATFORM=wayland`), the equalizer and playlist are not docked to the main window. Docking still works on X11 and on XWayland, which is what the player uses when that variable is unset.
- CI and the release workflow build Fedora 43 and Fedora 44 instead of Fedora 41 and Fedora 42. Ubuntu 22.04, Ubuntu 24.04, Ubuntu Resolute, Debian Bookworm, Debian Trixie, and openSUSE Tumbleweed are unchanged.
- projectM is located with pkg-config (`libprojectM`) so a moved library is not kept as a stale CMake path.

## [1.3.0] - 2026-10-25

Spectrum, the 10-band EQ, Milkdrop samples, and stereo balance require Qt 6.8 or newer (`QAudioBufferOutput`). On older Qt, including Qt 5 and Qt 6.2/6.4 (Ubuntu 22.04, Ubuntu 24.04, Debian Bookworm, and the 1.3.0 AppImage), audio still plays through the normal output and those four features are compiled out. Dragging the balance slider appends ` (NO TAP)`.

### Fixed

- Playlist drag-and-drop keeps the row order and the track list together.
- Gapless swap leaves playback, end-of-track, metadata, and video on the player that is actually active.
- Qt builds without the audio tap no longer present the spectrum, EQ, Milkdrop, and balance as if they were processing audio.
- Balance is applied on the PCM tap when the equalizer itself is off (Qt 6.8+).
- Settings are written on every quit path (window close, tray, MPRIS, and the terminal UI).
- A stream lookup that finishes after playback has moved on is ignored. Redirects to a disallowed host are still refused.
- The terminal UI is stopped before quit destroys the window.
- A folder scan is finished before the playlist window is destroyed.
- Restored playlist durations are probed one track at a time. A timed-out or superseded probe is ignored.
- The equalizer center detent (slider 32) is 0 dB, so Flat is actually flat. Saved slider positions are left where they were.
- MPRIS volume 0–1 maps onto the player’s 0–255 scale. `OpenUri` accepts a local file or `http`/`https`. `Previous` follows the same shuffle and repeat wrap as `Next`.
- Milkdrop is fed the right number of samples per channel. A short write to the audio sink is retried until the buffer is accepted.

### Added

- PLS playlists enqueue every `FileN=` entry. Remote M3U and PLS playlists enqueue every entry, and relative paths resolve against the playlist. Only an explicit `http`, `https`, `icy`, or `ftp` scheme is remote.
- A crashed Python plugin host restarts after 500 ms, 1 s, 2 s, 4 s, then 8 s, and stops after five crashes. A host that stays up for 30 seconds resets that count. The host is a separate process with the user’s privileges. It is not an OS sandbox.
- `.wsz`, `.zip`, and `.wal` skins are read in-process with zlib. Nested folders are kept. The cache under `~/.cache/winamp/skins/` is rebuilt when the archive’s size, mtime, or SHA-256 changes. The `unzip` program is not required.
- MP3 File Info writes ID3v2.3 and an ID3v1 tag. TagLib 1.12 and newer use the current save API; 1.11 still passes version 3. ID3v1 text outside Latin-1 is mapped and truncated (title, artist, and album 30 bytes, comment 28) instead of dropping the field. A year above 9999 is omitted. A track outside 1–255 is stored as 0 on v1.
- `Winamp-1.3.0-x86_64.AppImage` and `.zsync`, built on Ubuntu 22.04 (glibc 2.35). It bundles Qt 6.2, GStreamer, TagLib, and projectM. OpenGL and `python3` stay on the host. This Qt is older than 6.8, so the AppImage is a no-tap build.

### Changed

- The classic repeat button, the `R` key, and the modern repeat control cycle off, then repeat all, then repeat one, then off.
- CI installs libavutil (`libavutil-dev` on Debian/Ubuntu, `libavutil-free-devel` on Fedora, `libavutil-devel` on openSUSE) and builds `HAVE_AVUTIL`. A Qt 5 job configures with `-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON`, builds, and runs the tests.
- Wayland window positioning and classic-skin fixes landed on master between v1.2.2 and this release, along with the README donate badges.
- Published packages: Ubuntu 22.04 and 24.04, Debian Bookworm and Trixie, Fedora 42, and openSUSE Tumbleweed (`.deb` / `.rpm` / `.tar.gz`), plus the AppImage. Fedora 41 and Ubuntu Resolute did not upload packages because `playback_test` crashed in `testGaplessSwapFollowsActivePlayer` on those images.

## [1.2.2] - 2026-07-22

### Added

- Terminal mode (`--tui` / `--cli`).

### Changed

- The README install layout documents the themed app icon.
- Published packages for Ubuntu 22.04, 24.04, and Resolute, Debian Bookworm and Trixie, Fedora 41 and 42, and openSUSE Tumbleweed (`.deb`, `.rpm`, and `.tar.gz`).

## [1.2.1] - 2026-07-21

### Fixed

- The app icon shows up in XFCE and other strict icon-theme menus.
- Internet radio streams that played with no audio.

### Changed

- Published the same distro package set as v1.2.2.

## [1.2.0] - 2026-07-21

### Fixed

- Preferences settings now persist.

### Changed

- Published the same distro package set as v1.2.2.

## [1.1.1] - 2026-07-18

### Fixed

- Window snapping.

### Changed

- Published the same distro package set as v1.2.2.

## [1.1.0] - 2026-07-18

### Added

- Full ID3 read/write for the Winamp 2.x File Info dialog.

### Fixed

- TagLib 1.11 support, and openSUSE CI dependencies.

### Changed

- Published the same distro package set as v1.2.2.

## [1.0.0] - 2026-07-18

### Added

- Multi-distro GitHub release packages (the same Ubuntu, Debian, Fedora, and openSUSE set as v1.2.2).

### Fixed

- CI dependency installs for Ubuntu 22.04, Debian Bookworm, and openSUSE.

## [0.5.0-beta2] - 2026-07-18

The GitHub release body for this tag is empty. The items below are the commits between v0.5.0-beta1 and v0.5.0-beta2.

### Added

- Out-of-process Python plugin host, and a plugin manager in Preferences (enable, disable, configure, add, remove).
- Icecast DJ streaming plugin with multi-mount and metadata support.
- Playlist integration tests, Qt Test, and the CONTRIBUTING, plugin-development, and configuration guides.
- Packaging and release workflow, including an Ubuntu Resolute container build.

### Changed

- The player was split out of the monolith into `src/` (compat, EQ DSP, constants, dialogs, skins, playlist, equalizer, video, Milkdrop, media library, preferences, MPRIS).
- License is GPLv2.
- README and related docs were rebuilt. The product version string matches the tag.
- Equalizer, Preferences, and Video windows were decoupled from headers. Folder scanning runs on a background thread.
- GStreamer runtime dependencies for playback are documented.

### Fixed

- `QAudioBufferOutput` / `setAudioBufferOutput` version checks on older Qt 6, and missing Qt 5 includes.
- Stream URL persistence, the default skin when installed under `/usr`, inverted EQ slider direction, Linux stream playback fallback, and Qt 5 performance.
- Security, stability, and warning cleanups in the packaging pass (no further detail is in that commit subject).

### Removed

- Broken modern-skin support.
- Orphaned Windows-only files, `build_linux.sh`, and temporary Python extraction scripts.
- Private local tool instruction stores are no longer tracked.

## [0.5.0-beta1] - 2026-02-18

First beta. Pre-release. Packages: `.deb` (Debian / Ubuntu / Mint), `.rpm` (Fedora / RHEL / openSUSE), `.AppImage`, and `.tar.gz`.

### Added

- Classic Winamp UI with `.wsz` skin support.
- Audio playback via Qt 6 Multimedia.
- 10-band equalizer with presets.
- Playlist editor and media library.
- Milkdrop visualizations via projectM.
- MPRIS2 (media keys), system tray, and drag-and-drop file loading.
- English, German, and Spanish.

### Known limitations

- Modern/XML skins (Wasabi/Bento) are not supported.
- Some plugins are not ported.
- A Windows build was planned via CI and is not part of this release.

[Unreleased]: https://github.com/lord3nd3r/winamp-linux/compare/v1.3.1...HEAD
[1.3.1]: https://github.com/lord3nd3r/winamp-linux/compare/v1.3.0...v1.3.1
[1.3.0]: https://github.com/lord3nd3r/winamp-linux/compare/v1.2.2...v1.3.0
[1.2.2]: https://github.com/lord3nd3r/winamp-linux/compare/v1.2.1...v1.2.2
[1.2.1]: https://github.com/lord3nd3r/winamp-linux/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/lord3nd3r/winamp-linux/compare/v1.1.1...v1.2.0
[1.1.1]: https://github.com/lord3nd3r/winamp-linux/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/lord3nd3r/winamp-linux/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/lord3nd3r/winamp-linux/compare/v0.5.0-beta2...v1.0.0
[0.5.0-beta2]: https://github.com/lord3nd3r/winamp-linux/compare/v0.5.0-beta1...v0.5.0-beta2
[0.5.0-beta1]: https://github.com/lord3nd3r/winamp-linux/releases/tag/v0.5.0-beta1

# Configuration reference

This document describes **where Winamp for Linux stores state**, how skins and languages are resolved, and the **INI keys** written by the application. Keys below are cross-checked against the current sources (`winamp_window.h`, `playlist.cpp`, `equalizer.cpp`, `main.cpp`, `recent_files.h`, `preferences.cpp`).

---

## Table of contents

1. [Directory layout](#1-directory-layout)
2. [Main config file](#2-main-config-file-winampconf)
3. [INI sections and keys](#3-ini-sections-and-keys)
4. [Skins](#4-skins)
5. [Languages](#5-languages)
6. [Plugins](#6-plugins)
7. [Bookmarks](#7-bookmarks)
8. [Recent files](#8-recent-files)
9. [Environment & runtime](#9-environment--runtime)
10. [Reset / backup](#10-reset--backup)
11. [Playback behavior stored outside the INI](#playback-behavior-stored-outside-the-ini)

---

## 1. Directory layout

### User (per-account)

| Path | Role |
|------|------|
| `~/.config/winamp/` | User configuration root |
| `~/.config/winamp/winamp.conf` | Primary settings (Qt `QSettings` INI) |
| `~/.config/winamp/plugins/` | User Python plugins (`*.py`) |
| `~/.config/winamp/plugins/*.json` | Optional per-plugin config (e.g. `icecast_dj.json`) |
| `~/.config/winamp/bookmarks.txt` | Bookmark list (one entry per line) |
| `~/.cache/winamp/winamp_plugin_host.py` | **Generated** plugin host (do not treat as a user plugin) |
| `~/.cache/winamp/skins/<name>/` | Extracted `.wsz` / `.zip` / `.wal` skin, plus a `.source-stamp` |
| `~/.cache/winamp/winamp-tui.log` | stderr while `--tui` / `--cli` is running |

Qt resolves `~/.config` via `QStandardPaths::ConfigLocation` and cache via `CacheLocation`.

### System (when installed via CMake/CPack)

| Path | Role |
|------|------|
| `/usr/share/winamp/skins/` | Installed skins |
| `/usr/share/winamp/resource/` | Classic resource bitmaps |
| `/usr/share/winamp/lang/` | Language packs |
| `/usr/bin/winamp` or `/usr/local/bin/winamp` | Executable (prefix-dependent) |
| `/usr/share/applications/winamp.desktop` | Desktop entry |

Development runs typically load bitmaps from the source tree `assets/` / `skins/default/` relative to the binary.

---

## 2. Main config file (`winamp.conf`)

- **Format:** INI (`QSettings::IniFormat`)
- **Path:** `~/.config/winamp/winamp.conf`
- **When written:** Application exit / settings save paths (`saveAllSettings()` and related UI actions)
- **When read:** Startup (`loadAllSettings()`, skin/language bootstrap in `main.cpp`)

You may edit the file while Winamp is **closed**. Editing while running may be overwritten on exit.

### Root-level keys (no section)

Written/read outside the grouped window sections:

| Key | Type | Description |
|-----|------|-------------|
| `skin` | string | Path to active skin directory or package |
| `language` | string | Language code (default `en`); UI language packs under `lang/` |

---

## 3. INI sections and keys

### `[MainWindow]`

| Key | Type | Description |
|-----|------|-------------|
| `x` | int | Desktop X position of the main window |
| `y` | int | Desktop Y position of the main window |

### `[Playback]`

| Key | Type | Default (load) | Description |
|-----|------|----------------|-------------|
| `volume` | int | `200` | Volume 0–255 (classic scale) |
| `balance` | int | `0` | Stereo balance (−127 … +127; 0 = center) |
| `shuffle` | bool | `false` | Shuffle playlist order |
| `repeat` | bool | `false` | Repeat playlist (`repeatOn`) |
| `repeatTrack` | bool | `false` | Repeat the current track. The classic repeat button, the `R` key, and the modern repeat control cycle together: off (`repeat` false, `repeatTrack` false) → all (`repeat` true, `repeatTrack` false) → one (both true) → off. The menu can set any of the three directly. A saved pair with `repeat` false and `repeatTrack` true is treated as off on the next cycle |
| `eqVisible` | bool | `false` | Equalizer window shown |
| `plVisible` | bool | `true` | Playlist window shown |
| `visMode` | int | `1` | Spectrum / visualization mode index |
| `showRemainingTime` | bool | `false` | Time display shows remaining vs elapsed |
| `lastFile` | string | _(empty)_ | Last loaded local file path (restored if it still exists) |

### `[WindowState]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `alwaysOnTop` | bool | `false` | Main window stays on top |
| `doubleSize` | bool | `false` | Double-size classic chrome |
| `shadeMode` | bool | `false` | Shade (compact title) mode |
| `stopAfterCurrent` | bool | `false` | Stop when current track ends |
| `showSongNotifications` | bool | `true` | Desktop notification on track change |

### `[Equalizer]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `x`, `y` | int | — | Equalizer window position |
| `visible` | bool | — | Saved visibility (paired with main UI toggles) |
| `enabled` | bool | `true` | EQ DSP processing active |
| `auto` | bool | `false` | Auto-EQ mode flag |
| `preamp` | int | `32` | Preamp slider 0–63 (center 32 = 0 dB). The DSP path that applies it needs Qt 6.8 |
| `band0` … `band9` | int | `32` | Per-band gain 0–63 (center 32 = 0 dB) |
| `snapped` | bool | `false` | Snapped to main window |

**Band center frequencies** (`eq10_freq` in `eq_dsp.h`):  
`band0` 70 Hz · `band1` 180 Hz · `band2` 320 Hz · `band3` 600 Hz · `band4` 1 kHz · `band5` 3 kHz · `band6` 6 kHz · `band7` 12 kHz · `band8` 14 kHz · `band9` 16 kHz

### `[Playlist]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `x`, `y` | int | — | Playlist window position |
| `visible` | bool | — | Visibility flag |
| `snapMode` | int | `0` | Snap mode; non-zero means snapped to main |
| `width` | int | — | Window width (classic minimum sizing applies in UI) |
| `height` | int | — | Window height |
| `trackList` | string list | — | Saved playlist paths/URLs |
| `tracks` | string | — | **Legacy** comma-separated list (still read if `trackList` missing) |

### `[RecentFiles]`

Managed by `recent_files.h`:

| Key | Type | Description |
|-----|------|-------------|
| `size` | int | Number of entries |
| `1/path`, `2/path`, … | string | Absolute paths of recent media |

---

## 4. Skins

### Classic skins

Classic skins are directories or archives of bitmap resources: `MAIN.BMP`, `CBUTTONS.BMP`, `titlebar.bmp`, `Eqmain.bmp`, `Pledit.bmp`, `volume.bmp`, `text.bmp`, `numbers.bmp`, and related files. Loading is case-insensitive on Linux where the loader probes variants.

`.wsz`, `.zip`, and `.wal` packs are ordinary zip files. `extractSkinArchive()` in `constants.h` reads them with zlib. The `unzip` program is not used. Nested directories inside the archive are kept, and the loader then looks for the bitmap root (or `skin.xml`) inside that tree. Archives larger than 128 MB, or with more than 512 entries, are rejected.

Extracts land in `~/.cache/winamp/skins/<archive base name>/`. A `.source-stamp` file records size, mtime, and SHA-256. A later launch reuses the cache only when all three still match; otherwise the directory is deleted and extracted again.

**Search order (typical):**

1. User-selected path from Preferences / `skin` key  
2. Bundled `skins/default` and `assets` relative to the executable  
3. System `share/winamp/skins` when installed  

### Optional `viscolor.txt`

If present in a skin directory, up to **24** `r,g,b` lines override spectrum analyzer colors (Windows-compatible layout). See `loadVisColors()` in `constants.h`.

### Modern skins

XML-based modern skins are parsed in `modern_skin.h` with:

- Maximum include depth **10**
- Circular include detection via canonical paths  

Modern skins remain a secondary path; classic is the supported primary experience.

---

## 5. Languages

| Item | Detail |
|------|--------|
| Packs | `lang/*.lang` (e.g. `de.lang`, `es.lang`) |
| Install location | `share/winamp/lang/` |
| Config key | `language` (e.g. `en`, `de`, `es`) |
| UI | Preferences → Language |

Missing packs fall back to built-in English strings.

---

## 6. Plugins

| Item | Detail |
|------|--------|
| Directory | `~/.config/winamp/plugins/` |
| Enabled | `name.py` |
| Disabled | `name.py.disabled` (Preferences toggle renames) |
| Host script | `~/.cache/winamp/winamp_plugin_host.py` (generated; not a plugin) |
| Runtime | System `python3` on `PATH` |

Plugin-specific JSON (example: `icecast_dj.json`) is owned by the plugin, not by core INI settings.

See **[PLUGIN_DEVELOPMENT.md](PLUGIN_DEVELOPMENT.md)** for the API and lifecycle.

---

## 7. Bookmarks

| Item | Detail |
|------|--------|
| File | `~/.config/winamp/bookmarks.txt` |
| Format | One path or URL per line |
| UI | Preferences → Bookmarks; main menus where wired |

---

## 8. Recent files

Stored in `winamp.conf` under `[RecentFiles]` (see above). Updated as local files are opened/played. Remote-only URLs may not always populate the same way as local paths—check behavior if you rely on stream history.

---

## 9. Environment & runtime

| Variable / factor | Effect |
|-------------------|--------|
| `PATH` | Must include `python3` for plugins. The AppImage does not bundle the interpreter |
| `QT_QPA_PLATFORM` | e.g. `offscreen` for headless tests. On Wayland the GUI defaults to `xcb` so window positions work; set `wayland` to opt into native Wayland |
| `QT_LOGGING_RULES` | Qt category logging |
| Multimedia backend | GStreamer/FFmpeg packages determine format support |
| Qt version | Spectrum, EQ processing, Milkdrop PCM, and balance need Qt 6.8 (`QAudioBufferOutput`). Older Qt plays through `QAudioOutput`. Dragging balance appends ` (NO TAP)` |
| DBus session bus | Required for MPRIS2. The interface is registered only on Qt 6 with Qt DBus, as `org.mpris.MediaPlayer2.winamp` |
| `APPIMAGE_EXTRACT_AND_RUN` | Set to `1` to run the AppImage without FUSE |

---

## 10. Reset / backup

```bash
# Backup everything user-specific
cp -a ~/.config/winamp ~/winamp-config-backup-$(date +%F)

# Soft reset (keeps a backup)
mv ~/.config/winamp ~/.config/winamp.bak.$(date +%s)

# Plugin host only
rm -f ~/.cache/winamp/winamp_plugin_host.py

# Force skin archives to extract again
rm -rf ~/.cache/winamp/skins
```

After reset, defaults apply on next launch (volume 200, playlist visible, etc.—see load defaults above).

---

## Playback behavior stored outside the INI

These are code paths, not extra keys. They are listed here because they change what a saved playlist or a desktop controller does.

### M3U and PLS

Opening a `.m3u`, `.m3u8`, or `.pls` file (local or remote) parses every entry and enqueues them. M3U uses every non-comment line. PLS uses every `FileN=` value (`TitleN` and `LengthN` are not tracks). Relative names are resolved against the playlist’s directory or, for a remote playlist, against the playlist URL. A name with no scheme is a relative path; it is not turned into `http://`. Nested playlist files are followed for up to 3 hops. The first real media entry starts playback.

### MPRIS

On Qt 6 with DBus, `Volume` is a 0–1 double written through the player’s 0–255 control (`setPluginVolume`). `OpenUri` plays a local file path or an `http`/`https` URL. `ftp` is ignored. `Previous` calls the same shuffle and repeat logic as `Next`: shuffle picks another track, and repeat wraps from the first track to the last.

### ID3

File Info (Alt+3) writes MP3 tags as ID3v2.3 plus ID3v1. ID3v2 keeps the full text. ID3v1 is Latin-1, cut to 30 bytes for title, artist, and album and 28 bytes for the comment. A numeric year above 9999 is omitted on both tags, because v2.3’s year frame is four digits and would otherwise store `10000` as `1000`. A track outside 1–255 becomes 0 on the v1 tag and is kept on v2. TagLib 1.12 and newer use the current `save` API with `ID3v2::v3` and `DoNotDuplicate`. TagLib 1.11 uses `save(AllTags, true, 3, false)`.

### Repeat cycle

See `[Playback]` `repeat` / `repeatTrack` above. One click, the `R` key, and the modern repeat button all call `cycleRepeat()`.

---

## Example `winamp.conf` fragment

```ini
skin=/usr/share/winamp/skins/default
language=en

[MainWindow]
x=120
y=80

[Playback]
volume=200
balance=0
shuffle=false
repeat=false
repeatTrack=false
eqVisible=true
plVisible=true
visMode=1
showRemainingTime=false

[WindowState]
alwaysOnTop=false
doubleSize=false
shadeMode=false
stopAfterCurrent=false
showSongNotifications=true

[Equalizer]
enabled=true
auto=false
preamp=32
band0=32
band1=32
band2=32
band3=32
band4=32
band5=32
band6=32
band7=32
band8=32
band9=32

[Playlist]
width=275
height=232
snapMode=1
```

Values you never set will use in-code defaults on load.

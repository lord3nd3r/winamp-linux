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
| `repeat` | bool | `false` | Repeat playlist |
| `repeatTrack` | bool | `false` | Repeat current track only |
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
| `preamp` | int | `32` | Preamp slider 0–63 (center 32 ≈ 0 dB) |
| `band0` … `band9` | int | `32` | Per-band gain 0–63 (center 32 ≈ 0 dB) |
| `snapped` | bool | `false` | Snapped to main window |

**Band frequency mapping (classic):**  
`band0` 60 Hz · `band1` 170 Hz · `band2` 310 Hz · `band3` 600 Hz · `band4` 1 kHz · `band5` 3 kHz · `band6` 6 kHz · `band7` 12 kHz · `band8` 14 kHz · `band9` 16 kHz

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

Classic skins are directories (or archives) of bitmap resources: `MAIN.BMP`, `CBUTTONS.BMP`, `titlebar.bmp`, `Eqmain.bmp`, `Pledit.bmp`, `volume.bmp`, `text.bmp`, `numbers.bmp`, and related files. Loading is case-insensitive on Linux where the loader probes variants.

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
| `PATH` | Must include `python3` for plugins |
| `QT_QPA_PLATFORM` | e.g. `offscreen` for headless tests |
| `QT_LOGGING_RULES` | Qt category logging |
| Multimedia backend | GStreamer/FFmpeg packages determine format support |
| DBus session bus | Required for MPRIS2 clients to see the player |

---

## 10. Reset / backup

```bash
# Backup everything user-specific
cp -a ~/.config/winamp ~/winamp-config-backup-$(date +%F)

# Soft reset (keeps a backup)
mv ~/.config/winamp ~/.config/winamp.bak.$(date +%s)

# Plugin host only
rm -f ~/.cache/winamp/winamp_plugin_host.py
```

After reset, defaults apply on next launch (volume 200, playlist visible, etc.—see load defaults above).

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

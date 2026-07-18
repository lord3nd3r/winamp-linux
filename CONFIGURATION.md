# Winamp for Linux Configuration Guide

This document describes the directory layouts, configuration files, and key-value properties used by Winamp-Linux.

---

## 1. Directories and Files

All user-specific configurations, playlists, database indexes, and script additions are located under your home directory:

| Path | Description |
|---|---|
| `~/.config/winamp/` | The root config directory. |
| `~/.config/winamp/winamp.conf` | Main application configuration file (ini format). |
| `~/.config/winamp/bookmarks.txt` | Text file containing user bookmarked URLs and local files (one per line). |
| `~/.config/winamp/plugins/` | User directory for custom Python plugins (scanned at startup). |
| `/usr/share/winamp/` | Default location for installed application assets, languages, and skins. |

---

## 2. Main Configuration (`winamp.conf`) Reference

The configuration file is formatted in the standard INI layout with sections:

### `[MainWindow]`
- **`x`** (int): X desktop coordinate of the main window.
- **`y`** (int): Y desktop coordinate of the main window.

### `[Playback]`
- **`volume`** (int, `0` to `255`): Output volume multiplier (default is `200`).
- **`balance`** (int, `-127` to `127`): Stereo balance slider (negative is Left, positive is Right, `0` is center).
- **`shuffle`** (bool, `true`/`false`): Activates random playlist order selection.
- **`repeat`** (bool, `true`/`false`): Loops the current playlist.
- **`eq`** (bool, `true`/`false`): Toggles visual display of the Equalizer window.
- **`playlist`** (bool, `true`/`false`): Toggles visual display of the Playlist Editor window.
- **`repeatTrack`** (bool, `true`/`false`): Toggles repeat mode for the single current track.
- **`stopAfterCurrent`** (bool, `true`/`false`): Stops the player automatically when the currently playing track ends.
- **`showSongNotifications`** (bool, `true`/`false`): Shows a desktop notification bubble when a new song starts playing.

### `[PlaylistWindow]`
- **`x`** (int): X desktop coordinate of the playlist window.
- **`y`** (int): Y desktop coordinate of the playlist window.
- **`width`** (int, min `275`): Width of the playlist window.
- **`height`** (int, min `116`): Height of the playlist window.
- **`shadeMode`** (bool, `true`/`false`): Toggles compact single-line visual layout for the playlist.

### `[Equalizer]`
- **`x`** (int): X desktop coordinate of the Equalizer window.
- **`y`** (int): Y desktop coordinate of the Equalizer window.
- **`on`** (bool, `true`/`false`): Toggles whether the EQ DSP audio process is active.
- **`auto`** (bool, `true`/`false`): Activates automatic EQ preset selection based on track metadata tags (if available).
- **`preamp`** (int, `0` to `63`): Pre-amplifier gain level (slider center is `32`).
- **`band0`** to **`band9`** (int, `0` to `63`): Gain levels for the 10 graphic EQ bands (60Hz, 170Hz, 310Hz, 600Hz, 1kHz, 3kHz, 6kHz, 12kHz, 14kHz, 16kHz). Center is `32` (0dB).

### `[RecentFiles]`
Contains an array of recently played track paths.
- **`size`** (int): Number of entries.
- **`1/path`, `2/path`, ...** (string): Absolute file paths to recently played media.

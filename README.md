# Winamp for Linux — Qt6 Native Port

A pixel-accurate recreation of the classic Winamp 2.x/5.x interface using Qt6 and C++17, targeting a **1:1 match** with the original Windows version. Uses the **real Winamp bitmap assets** from the source tree for fully authentic rendering.

![Winamp Linux](https://img.shields.io/badge/Winamp-Linux-00FF00?style=for-the-badge&logo=linux&logoColor=white)
![Qt6](https://img.shields.io/badge/Qt6-Multimedia-41CD52?style=for-the-badge&logo=qt&logoColor=white)
![C++17](https://img.shields.io/badge/C++-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)

---

## Building

```bash
# Install dependencies (Debian/Ubuntu)
sudo apt-get install -y qt6-base-dev qt6-multimedia-dev \
  libgl-dev qt6-base-dev-tools \
  libprojectm-dev projectm-data \
  cmake ninja-build

# Build
cmake -B build -G Ninja
ninja -C build

# Run
./build/winamp
```

Milkdrop visualization (via projectM) is compiled in automatically — no extra flags needed.

---

## Implemented Features

### Main Player Window

- **Authentic skin rendering** — pixel-exact background, titlebar (active/inactive states), and all UI elements from the original Winamp skin bitmaps
- **Transport controls** — Previous, Play, Pause, Stop, Next, Eject with skinned button states
- **LED time display** — MM:SS with bitmap digit font; animated colon separator when `nums_ex.bmp` is present
- **Scrolling song title** — bitmap font rendering with smooth scrolling and wrap
- **Play/Pause/Stop indicator** — status icon reflecting current playback state
- **Mono/Stereo indicator** — dynamically reflects actual channel count
- **Bitrate and sample rate display** — kbps and kHz from media metadata
- **Volume slider** — 28-frame skinned animation with click and drag
- **Balance/pan slider** — range -127 to +127 with skin support
- **Position/seek bar** — draggable seek with skinned thumb
- **Shuffle and Repeat toggles** — with skin-accurate on/off states
- **EQ and PL toggle buttons** — show/hide equalizer and playlist windows
- **Clutterbar** — clickable O/A/I/D/V options bar (Options, Always On Top, File Info, Double Size, Visualization)
- **Double-size mode** — 2x pixel scaling (Ctrl+D)
- **Shade mode** — compact titlebar-only view (Ctrl+W)
- **Always on top** — Ctrl+T toggle, persisted across sessions
- **Right-click context menu** — full submenu hierarchy matching Windows Winamp
- **Tooltips** on all interactive controls
- **VU meter** — dual-channel RMS level visualization
- **Easter eggs** — type "NULLSOFT" or "WINAMP" for hidden messages
- **Drag-and-drop** — drop audio files or directories to play
- **Stop after current** — plays current track then stops
- **Auto-advance** — next track on end; respects shuffle and repeat
- **System tray icon** — minimize to tray with context menu and tooltip
- **Splash screen** on startup
- **File info dialog** — Alt+3 for metadata viewing/editing and technical info
- **Keyboard shortcuts** — X/V/C/Z/B/Space (transport), J (jump to time), Ctrl+J (jump to file), Ctrl+D/W/T/P/L, Alt+3, R/S/L, arrows, +/-

### Spectrum Analyzer and Oscilloscope

- **4 visualization modes** — Off / Spectrum Analyzer / Oscilloscope / VU Meter (click to cycle)
- **Real-time audio analysis** — live PCM capture with 512-point FFT
- **19-bar spectrum analyzer** — color gradient bars with smooth falloff and gravity-based peak dots
- **75-sample oscilloscope** — connected waveform display
- **Skin-aware color palette** — loads `viscolor.txt` per skin; falls back to authentic Winamp defaults

### 10-Band Graphic Equalizer

- **Full skin rendering** — background, titlebar, and all controls from skin bitmaps
- **ON/OFF and AUTO toggles** — AUTO auto-loads per-file presets from `~/.config/winamp/eqpresets/`
- **Preamp + 10 frequency band sliders** — 60Hz through 16kHz with skinned groove and thumb
- **18 built-in presets** — Flat, Classical, Club, Dance, Full Bass, Full Treble, and more
- **Presets menu** — load/save/delete `.eqf` preset files
- **Frequency response curve** — real-time spline-interpolated visualization
- **Real DSP processing** — 4Front EQ10 engine (10-band IIR biquad with preamp and dynamic limiter)
- **Shade mode** — compact single-line view
- **Draggable** with snap-to-main support

### Playlist Editor

- **Skinned frame** — composited from skin bitmaps with tiled borders and scrollbar
- **Track list** — skin-colored text with selection highlighting
- **Track duration display** — total time shown at bottom; per-track durations probed on add
- **Internal drag reorder** — rearrange tracks within the list
- **External file drop** — drag audio files from file manager
- **Double-click to play** selected track
- **ADD/REM/SEL/MISC/LIST menus** — add files/folders/URLs, remove/crop/clear, select all/none/invert, sort/reverse/randomize, open/save .m3u playlists, generate random playlists
- **Track persistence** — full playlist saved/restored across sessions
- **Resizable** — drag edges and corners with dynamic skin re-tiling
- **Shade mode** — compact single-line view
- **Skin-aware colors** — reads `PLEDIT.TXT` for playlist colors
- **Draggable** with multi-mode snapping

### Window Management

- **Magnetic window snapping** (15px snap distance) — EQ below main, playlist to right/below
- **Synchronized movement** — dragging main window moves all snapped children
- **4 snap modes** — free, right of main, below EQ, below main

### Settings Persistence

All state saved to `~/.config/winamp/winamp.conf` (INI format) on exit, restored on startup:

- Window positions, volume, balance, shuffle, repeat, EQ/PL visibility
- Visualization mode, time display mode (elapsed/remaining)
- Always on top, double-size, shade mode, stop-after-current
- Full EQ state (enabled, auto, preamp, all 10 bands, snap)
- Playlist (position, snap mode, dimensions, complete track list)
- Skin path and language preference

### Language Packs

- **Multi-language support** — `.lang` file format (KEY=Value pairs)
- **Built-in languages** — English (default), German, Spanish
- **Custom language files** — place `.lang` files in `~/.winamp/lang/` or `lang/`
- **Language selector** in Preferences > Setup > Language
- **UTF-8 encoding** with full Unicode support

### Skin System

- **WSZ/ZIP skin support** — place archives in `~/.winamp/skins/`; auto-extracted and cached
- **Folder skins** — unzipped directories also supported
- **Skin browser** in Preferences > Skins; double-click to apply
- **Fallback loading** — missing bitmaps filled from default skin
- **Case-insensitive BMP loading** for cross-platform compatibility

### Modern Skin Engine (Winamp 5)

- **XML-based skin format** — full parser for Winamp 5 modern skins with recursive `<include>` support
- **BitmapFont rendering** — Wasabi-compatible character mapping for timer, songticker, songinfo, and playlist fonts
- **Dual-mode window** — switches between classic (275×116) and modern (resizable) layouts
- **Full player rendering** — titlebar, tiled backgrounds, display area, seek bar, volume, playback buttons with hover/pressed states
- **13-button interaction system** — all player controls with bitmap states
- **Modern Skins preferences** — scan built-in and user skins; `.wal` archive support
- **Modern EQ and Playlist windows** — full skinned rendering with all controls
- **Visualization** — spectrum analyzer and oscilloscope in the modern display area
- **Scrolling songticker and song info display**
- **Case-insensitive path resolution** for Linux compatibility

### Dialogs

- **About Winamp** — demoscene-style animated dialog with starfield, orbiting cube, fire spheres, and fading credits
- **Play Location** — URL input for stream/remote playback
- **Preferences** — tree-based dialog: Setup (General, File Types, Titles, Language), Skins (Classic, Modern), Playback, Playlist, Bookmarks, Visualization, Plug-ins
- **Jump to File** — search-within-playlist with filter and play/queue (Ctrl+J)

### Audio

- **Qt Multimedia backend** (QMediaPlayer + QAudioOutput + QAudioBufferOutput)
- **Supported formats** — MP3, WAV, FLAC, OGG, M4A, AAC, WMA, Opus
- **URL playback** — play remote streams via URL dialog
- **SHOUTcast/Icecast streaming** with metadata display and notifications
- **Crossfade / gapless playback** — dual-player preloading for seamless transitions
- **Real-time audio buffer capture** for visualization

### Video

- **Video playback** — QVideoWidget rendering within frameless window
- **Supported formats** — MP4, AVI, MKV, MOV, WebM (Qt6 Multimedia + system codecs)
- **Auto-show** when video content is detected; logo display when idle
- **Fullscreen mode** — F key or double-click to toggle
- **Resizable and draggable** frameless window

### Milkdrop Visualization (projectM)

- **Full Milkdrop preset support** — 274 `.milk` presets
- **OpenGL rendering** — QOpenGLWidget with projectM engine
- **Real-time audio reactive** — PCM data fed directly to projectM
- **Preset navigation** — Space/Backspace (next/prev), R (shuffle), L (lock)
- **Fullscreen mode** — F/F11 or double-click; Escape to exit
- **Auto-shuffle** — presets change every 30 seconds with smooth transitions

### Media Library

- **File system browser** — QTreeView-based file/folder navigation from user's music directory
- **Skinned window frame** — authentic generic window rendering
- **Audio file filtering** — shows only supported formats
- **Add to playlist** — double-click file or folder to add
- **Resizable and draggable** with keyboard shortcuts (Alt+L, Escape)

### Bookmarks & Recent Files

- **Bookmark manager** — add/open bookmarked files from context menu; persisted at `~/.config/winamp/bookmarks.txt`
- **Recent files** — last 15 played files in context menu

### System Integration

- **MPRIS2 D-Bus** — Linux desktop media player integration (media keys, KDE Connect, GNOME/KDE panel widgets)
- **System tray icon** — minimize to tray with context menu and tooltip
- **Song change notifications** — desktop popups on track change (toggleable)
- **Command-line interface** — `-play`, `-pause`, `-stop`, `-enqueue`, files and directories
- **Drag tracks out of playlist** to file manager via file:// MIME data

---

## Architecture

The codebase is organized into focused modules under `src/`:

| Module | Purpose |
|--------|---------|
| `main.cpp` | Application entry point, command-line parsing, splash screen |
| `winampwindow` | Main player window — transport, display, menus, drag/drop, skin rendering |
| `winampbitmaps` | Bitmap/skin asset loading with multi-path fallback and case-insensitive search |
| `skinutils` | Shared skin utilities — bitmap rendering helpers, color parsing, region calculations |
| `equalizerwindow` | 10-band graphic equalizer window with sliders and curve display |
| `eq10dsp` | 4Front EQ10 DSP engine — 10-band IIR biquad processing with preamp and limiter |
| `eqpresets` | EQ preset management — load/save/delete `.eqf` files, 18 built-in presets |
| `playlistwindow` | Playlist editor window — track list, menus, scrollbar, resize, drag reorder |
| `videowindow` | Video playback window — QVideoWidget, fullscreen, resize |
| `milkdropwindow` | Milkdrop visualization — projectM OpenGL rendering with preset navigation |
| `medialibrarywindow` | Media library file browser — QTreeView with skinned frame |
| `modernskinengine` | Winamp 5 modern skin XML parser and bitmap font renderer |
| `dialogs` | All dialogs — About, Preferences, Jump to Time/File, File Info, Play Location |
| `bookmarkmanager` | Bookmark persistence and menu integration |
| `recentfilesmanager` | Recent files tracking (last 15 played) |
| `mpris2` | MPRIS2 D-Bus interface for Linux desktop integration |
| `translator` | Multi-language `.lang` file loader and string lookup |

Each module has a `.h` and `.cpp` pair. The legacy monolithic `winamp_authentic.cpp` is retained for reference.

---

## TODO — Road to 1:1 Windows Parity

### Remaining

- [ ] Mini-Browser window
- [ ] Plugin architecture — input, output, DSP, general purpose, visualization plugin APIs
- [ ] CD playback and ripping
- [ ] Skin renderer abstraction — decouple skin loading from widget logic
- [ ] Audio engine abstraction — replaceable backend (PipeWire, PulseAudio, ALSA)
- [ ] Plugin ABI compatibility — load original Winamp `.dll` plugins via Wine bridge (stretch goal)

### Completed

<details>
<summary>Click to expand completed items</summary>

- [x] Real EQ DSP processing (4Front EQ10 engine, 10-band IIR biquad)
- [x] `viscolor.txt` parsing for per-skin visualization colors
- [x] Clutterbar (O/A/I/D/V options bar)
- [x] Windowshade mode (main, EQ, playlist)
- [x] Balance slider
- [x] Playlist scrollbar, resize, bottom button graphics
- [x] Keyboard shortcuts (full set)
- [x] Jump to time / Jump to file dialogs
- [x] File info dialog (Alt+3)
- [x] Song title from metadata (ID3/Vorbis tags)
- [x] Time display toggle (elapsed/remaining)
- [x] Animated colon separator
- [x] Double-size mode (Ctrl+D)
- [x] Always on top (Ctrl+T)
- [x] EQ frequency response curve
- [x] EQ AUTO behavior (per-file/genre presets)
- [x] Playlist track numbering, keyboard controls, skin colors
- [x] Media Library window
- [x] Video playback
- [x] Milkdrop visualization (projectM)
- [x] Global hotkeys (MPRIS2 D-Bus)
- [x] SHOUTcast/Icecast streaming
- [x] Crossfade / gapless playback
- [x] Bookmarks and EQ preset files
- [x] Playlist generator
- [x] System tray, command-line interface, notifications
- [x] Localization (.lang files)
- [x] Modern skin support (Winamp 5 XML skins)
- [x] Drag tracks out of playlist to file manager
- [x] Multi-file architecture refactor

</details>

---

## File Structure

```
├── CMakeLists.txt              # Qt6 + OpenGL + projectM build configuration
├── README.md                   # This file
├── winamp_authentic.cpp        # Legacy monolithic source (retained for reference)
├── src/
│   ├── main.cpp                # Entry point
│   ├── winampwindow.cpp/.h     # Main player window
│   ├── winampbitmaps.cpp/.h    # Skin bitmap loading
│   ├── skinutils.cpp/.h        # Shared skin utilities
│   ├── equalizerwindow.cpp/.h  # Equalizer window
│   ├── eq10dsp.cpp/.h          # EQ DSP engine
│   ├── eqpresets.cpp/.h        # EQ preset management
│   ├── playlistwindow.cpp/.h   # Playlist editor
│   ├── videowindow.cpp/.h      # Video window
│   ├── milkdropwindow.cpp/.h   # Milkdrop visualization
│   ├── medialibrarywindow.cpp/.h # Media library browser
│   ├── modernskinengine.cpp/.h # Modern skin engine
│   ├── dialogs.cpp/.h          # All dialogs
│   ├── bookmarkmanager.cpp/.h  # Bookmark manager
│   ├── recentfilesmanager.cpp/.h # Recent files
│   ├── mpris2.cpp/.h           # MPRIS2 D-Bus
│   └── translator.cpp/.h      # Language translation
├── skins/default/              # Default skin bitmaps
├── lang/                       # Language packs (de.lang, es.lang)
├── Src/Winamp/resource/        # Additional bitmap assets
└── build/
    └── winamp                  # Executable
```

Skin bitmaps loaded from `skins/default/` and `Src/Winamp/resource/` with automatic fallback merging.
Custom skins: place `.wsz`/`.zip` files or folders in `~/.winamp/skins/`.
Milkdrop presets loaded from `/usr/share/projectM/presets/` (installed via `projectm-data`).

---

## License

Based on the Winamp source code released by Winamp SA. See repository [LICENSE.md](LICENSE.md) for details.

---

*It really whips the llama's ass!*

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

### Main Player Window (275x116 px)

- **Authentic MAIN.BMP background** — pixel-exact base image from the original skin
- **Skinned titlebar** — active/inactive states from `titlebar.bmp` (sprite offset 27,0 / 27,15), 275x14
- **Close and minimize buttons** — clickable hit zones in the titlebar
- **Play/Pause/Stop status indicator** — 9x9 icon at (26,28) from `PLAYPAUS.BMP` with 3 states
- **LED time display** — MM:SS rendered with `numbers.bmp` (9x13 glyphs) at correct digit positions (36,26 / 48,26 / 78,26 / 90,26); colon baked into `MAIN.BMP`; if `nums_ex.bmp` present, uses animated colon separator (matching Windows enhanced number display)
- **Scrolling song title** — bitmap font rendering from `text.bmp` (5x6 per character) in a clipped 154x6 region at (111,27); scrolls at 150ms with `***` separator wrap
- **Mono/Stereo indicator** — `MONOSTER.BMP` at (212,41); dynamically reflects actual channel count (mono/stereo on/off states)
- **Bitrate display** — kbps shown at (111,43) using `text.bmp` font, from `QMediaMetaData::AudioBitRate`
- **Sample rate display** — kHz shown at (156,43) using `text.bmp` font, from audio buffer format
- **Transport buttons** — Previous, Play, Pause, Stop, Next, Eject from `CBUTTONS.BMP` with normal/pressed sprite rows; actions fire on mouse release
- **Previous/Next navigate playlist** — Previous goes to prior track (or seeks to 0 if first), Next advances to next track
- **Auto-advance** — automatically plays next track when current one ends; respects shuffle (random) and repeat (wrap) modes
- **Volume slider** — 28-frame sprite animation from `volume.bmp` (68x13 each, 15px stride); click and drag, range 0-255
- **Position/seek bar** — background + thumb from `POSBAR.BMP` (248x10 bar, 29x10 thumb); draggable seek with distinct pressed thumb sprite
- **Shuffle and Repeat toggles** — `SHUFREP.BMP` on/off states (47x15 and 28x15)
- **EQ and PL toggle buttons** — `SHUFREP.BMP` on/off states (23x12 each); show/hide child windows
- **Frameless window** with custom titlebar dragging
- **Right-click context menu** — full submenu hierarchy matching Windows: Play (file/location), Bookmarks (add/open), Options (always on top, double size, shade, prefs), Playback (stop after current, jump to time/file, shuffle, repeat off/all/one), Windows (EQ/PL/Milkdrop), Visualization (off/spectrum/oscilloscope/VU/Milkdrop), Recent Files, About, Exit
- **Tooltips** — hover over buttons and controls to see function descriptions (Previous/Play/Pause/Stop/Next, Volume, Balance, Seek, EQ/PL buttons, time display, visualization, clutterbar)
- **Balance/pan slider** — `BALANCE.BMP` skin support (falls back to `volume.bmp`); range -127 to +127 matching Windows `pan127`
- **Double-size mode** — 2x pixel scaling (Ctrl+D toggle), authentic scaled rendering
- **Shade mode** — compact 275x14 titlebar-only view with scrolling title (Ctrl+W toggle)
- **Always on top** — Ctrl+T toggle, persisted across sessions
- **Clutterbar** — left-side options bar (10,22-65) from `titlebar.bmp` sprite (x=304); clickable O/A/I/D/V buttons: Options, Always On Top, File Info, Double Size, Visualization; toggle by clicking top button; AAT and Double Size show pressed state when active
- **VU meter** — dual-channel RMS level visualization as vis mode 3
- **Easter eggs** — type "NULLSOFT" or "WINAMP" for hidden messages (matching Windows `eggstat`)
- **Drag-and-drop** — drop audio files or directories onto main window to play
- **Stop after current** — plays current track then stops (matching Windows `g_stopaftercur`)
- **System tray icon** — minimize to tray, tray context menu (prev/play/stop/next/open/exit), tooltip shows current song
- **Splash screen** — shows SPLASH.BMP on startup (matching Windows `SPLASH.cpp`)
- **Extended keyboard shortcuts** — X (play), V (stop), C (pause), Z/B (prev/next), Space (play/pause), J (jump to time), Ctrl+J (jump to file), Ctrl+D (double size), Ctrl+W (shade), Ctrl+T (always on top), Ctrl+P (preferences), Ctrl+L (play location), Alt+3 (file info), R (repeat), S (shuffle), L (open file), arrows (seek), +/- (volume)
- **File info dialog** — Alt+3 shows tabbed dialog with metadata (Title, Artist, Album, Year, Track, Genre, Comment) and technical info (bitrate, sample rate, duration); matches Windows IDD_FILEINFO

### Spectrum Analyzer and Oscilloscope

- **4 visualization modes** — Off / Spectrum Analyzer / Oscilloscope / VU Meter, cycled by clicking the viz area
- **Real-time audio analysis** — `QAudioBufferOutput` captures live PCM data (Int16 and Float sample formats)
- **512-point radix-2 FFT** — custom Cooley-Tukey implementation for spectrum computation
- **19-bar spectrum analyzer** — 3px-wide bars with 1px gaps in a 75x16 area; color gradient from green (bottom) to red (top) matching the original `ppal2[]` palette from `draw.cpp`; smooth falloff (1 unit/frame at 20fps); gravity-based peak dots
- **75-sample oscilloscope** — connected waveform drawn as vertical line segments
- **Authentic 24-entry color palette** — `visColors[]` matching Windows Winamp defaults
- **VU meter** — dual-channel (L/R) RMS level bars with color gradient, computed from live audio buffer

### 10-Band Graphic Equalizer (275x116 px)

- **Full Eqmain.bmp skin rendering** — background, titlebar (active/inactive), button sprites
- **ON/OFF toggle** — enables/disables EQ with sprite state feedback
- **AUTO toggle** — auto EQ mode flag; when enabled, automatically loads per-file EQ presets from `~/.config/winamp/eqpresets/{filename}.eqf` or falls back to `Default.eqf` (matches Windows eq_autoload behavior)
- **Preamp slider** — 0-63 range with 28-frame groove animation and 11x11 thumb from Eqmain sprite sheet
- **10 frequency band sliders** — 60Hz through 16kHz, same groove/thumb rendering as preamp
- **18 built-in presets** — Flat, Classical, Club, Dance, Full Bass, Full Bass & Treble, Full Treble, Laptop Speakers, Large Hall, Live, Party, Pop, Reggae, Rock, Ska, Soft, Soft Rock, Techno
- **Presets popup menu** — clickable button opens menu; applies preset values to all sliders; load/save/delete `.eqf` preset files
- **EQ graph background** — 113x19 from sprite (0,294)
- **Frequency response curve** — real-time spline-interpolated curve visualization showing combined effect of preamp and 10 bands; uses Catmull-Rom spline (matching Windows draw_eq_graphthingy)
- **Shade mode** — compact single-line view (double-click titlebar)
- **Draggable** with snap-to-main support
- **Close button** in titlebar

### Playlist Editor (275x232 px)

- **Composited skin frame from Pledit.bmp** — titlebar (corner + title + filler + corner pieces, active/inactive), tiled left/right borders with scrollbar track, black body fill, bottom bar (left + filler + right)
- **Track list** — green-on-black Courier 8pt with blue selection highlight (#0000C6)
- **Total time display** — bitmap font rendering at bottom showing track count + total duration
- **Track duration detection** — probes each file with temporary QMediaPlayer on add
- **Internal drag reorder** — drag tracks within the list to rearrange; tracks and durations stay in sync
- **External file drop** — drag audio files from file manager into the playlist
- **Double-click to play** — plays the selected track
- **ADD menu** — Add file(s), Add directory (recursive scan), Add location
- **REM menu** — Remove selected, Crop, Clear playlist, Remove misc
- **SEL menu** — Select all, Select none, Invert selection
- **MISC menu** — Sort by title, Sort by filename, Sort by path, Reverse list, Randomize list
- **LIST menu** — New playlist (clear), Open playlist (.m3u/.m3u8/.pls), Save playlist (.m3u with #EXTM3U/#EXTINF metadata), Generate playlist (random selection from music library with customizable track count)
- **Track persistence** — full track list saved/restored across sessions
- **Resizable** — drag edges and corners to resize (minimum 275x116); dynamic re-tiling of skin pieces
- **Shade mode** — compact single-line view (double-click titlebar)
- **Skin-aware colors** — reads `PLEDIT.TXT` for playlist background, text, and selection colors
- **Draggable** with multi-mode snapping
- **Close button** in titlebar

### Window Management

- **Magnetic window snapping** (15px snap distance):
  - EQ snaps below main window
  - Playlist snaps to right of main, below EQ, or below main
- **Synchronized movement** — dragging main window moves all snapped children
- **Snap mode tracking** — 4 modes: free, right of main, below EQ, below main

### Settings Persistence

All state saved to `~/.config/winamp/winamp.conf` (INI format) on exit, restored on startup:

- Main window position
- Volume, balance, shuffle, repeat, EQ/PL visibility
- Visualization mode, time display mode (elapsed/remaining)
- Always on top, double-size, shade mode, stop-after-current
- Last played file (restored on startup)
- Full EQ state: enabled, auto, preamp, all 10 band values, snap state
- Playlist: position, snap mode, dimensions, complete track list
- Skin path
- Language preference

### Language Packs

- **Multi-language support** — simple `.lang` file format (KEY=Value pairs) for UI string translation
- **Built-in languages** — English (default), German (de.lang), Spanish (es.lang) included
- **Custom language files** — place `.lang` files in `~/.winamp/lang/` or `lang/` directory
- **Language selector** — Preferences > Setup > Language for choosing interface language
- **UTF-8 encoding** — full Unicode support for international characters
- **Extensible system** — translation files can be easily created or modified with any text editor
- **Restart required** — language changes take effect on next launch

### Skin System

- **WSZ/ZIP skin support** — place `.wsz` or `.zip` skin archives in `~/.winamp/skins/`; auto-extracted to `~/.cache/winamp/skins/` via `unzip`; cached for subsequent loads
- **Folder skins** — unzipped skin directories in `~/.winamp/skins/` also supported
- **"Winamp Default" entry** — always available in skin list to restore the stock skin
- **Skin browser** — Preferences > Skins tab lists all available skins; double-click to apply
- **Fallback loading** — missing bitmaps in custom skins automatically filled from default skin assets
- **Case-insensitive BMP loading** — handles mixed-case filenames across skin archives

### Modern Skin Engine (Winamp 5)

- **XML-based skin format** — full parser for Winamp 5 modern skins (`skin.xml` with recursive `<include>` support)
- **ModernSkinEngine class** — loads `<bitmap>` sprite sheet definitions and sub-rectangle extraction from PNG images
- **BitmapFont rendering** — grid-based text rendering for timer (BIGNUM), songticker, and songinfo fonts
- **Dual-mode window** — WinampWindow switches between classic (fixed 275×116) and modern (resizable, min 354×144) modes
- **Full player rendering** — titlebar with active/inactive states, 3-part tiled backgrounds (main + bg2 layers), display area with overlay effects, seek bar with thumb, volume slider, playback buttons with hover/pressed states
- **13-button interaction system** — Previous, Play, Pause, Stop, Next, Eject, PL, ML, Mute, Repeat, Shuffle, Minimize, Close — all with bitmap hover/pressed states
- **Modern Skins preferences page** — scan built-in resource skins (Winamp Modern, Bento, Big Bento) and user skins; `.wal` archive support
- **LED indicators** — shuffle/repeat on/off LEDs from sprite sheet
- **Visualization** — spectrum analyzer and oscilloscope rendered in the modern display area
- **Scrolling songticker** — animated song title text using bitmap font with seamless looping
- **Song info display** — bitrate (kbps), sample rate (khz), channel mode (mono/stereo/surround), EQ indicator
- **Case-insensitive path resolution** — handles Windows-originated mixed-case filenames on Linux

### Dialogs

- **About Winamp** — demoscene-style animated dialog: starfield background, warped splash2.bmp, orbiting team.bmp cube, fire spheres, fading credits at 33fps
- **Play Location** — URL input for stream/remote playback
- **Preferences** — full tree-based dialog matching Windows Options.cpp: Setup (General, File Types, Titles, Language), Skins (Classic Skins browser, Modern Skins browser), Playback, Playlist, Bookmarks, Visualization, Plug-ins
- **Jump to File** — search-within-playlist dialog with filter and play/queue (Ctrl+J, matching Windows `JSEARCH.cpp`)

### Milkdrop Visualization (projectM)

- **Full Milkdrop preset support** — 274 `.milk` presets from `/usr/share/projectM/presets/`
- **OpenGL rendering** — QOpenGLWidget with projectM engine at 33fps
- **Real-time audio reactive** — PCM data from live audio buffer fed directly to projectM
- **Preset navigation** — Space (next), Backspace (previous), R (toggle shuffle), L (lock/unlock)
- **Fullscreen mode** — F/F11 or double-click to toggle; hides main/EQ/playlist windows; Escape to exit
- **Launch from UI** — double-click the visualization area or right-click → "Milkdrop visualization"
- **Auto-shuffle** — presets change every 30 seconds with smooth 5-second transitions

### Bookmarks & Recent Files

- **Bookmark manager** — add current track to bookmarks, open bookmarked files from context menu; persisted at `~/.config/winamp/bookmarks.txt`
- **Recent files** — last 15 played files tracked and shown in context menu (matching Windows recent files)

### System Integration

- **MPRIS2 D-Bus** — full Linux desktop media player integration: play/pause/next/prev/seek via media keys, KDE Connect, GNOME/KDE panel widgets. Implements `org.mpris.MediaPlayer2` and `org.mpris.MediaPlayer2.Player` interfaces
- **System tray icon** — minimize to tray with context menu (prev/play-pause/stop/next/open/exit); click to show/hide; tooltip shows current song
- **Song change notifications** — desktop notification popup when track changes (using Qt tray messages); toggleable in Preferences > Setup > General
- **Command-line arguments** — pass files/directories on startup, `-play`, `-pause`, `-stop`, `-enqueue` flags (matching Windows `cmdline.cpp`)
- **Splash screen** — shows `SPLASH.BMP` from skin on startup

### Audio

- **Qt Multimedia backend** (QMediaPlayer + QAudioOutput + QAudioBufferOutput)
- **Supported formats**: MP3, WAV, FLAC, OGG, M4A, AAC, WMA, Opus
- **URL playback** — play remote streams via URL dialog
- **Real-time audio buffer capture** for visualization (Int16 and Float sample formats)

### Video

- **Video playback** — QVideoWidget rendering within frameless window
- **Supported formats**: MP4, AVI, MKV, MOV, WebM (all formats supported by Qt6 Multimedia + system codecs)
- **Auto-show window** — video window appears automatically when video content is detected
- **Logo display** — video_logo.bmp shown when no video is playing
- **Fullscreen mode** — F key or double-click to toggle fullscreen; hides main/EQ/playlist windows
- **Resizable** — drag all 8 edges and corners to resize (minimum 160x120)
- **Draggable window** — frameless window with custom drag
- **Menu toggle** — show/hide via Windows → Video window

### Media Library

- **File system browser** — QTreeView-based file/folder navigation starting at user's music directory
- **gen.bmp/genex.bmp skinning** — authentic generic window frame rendering matching Windows plugin windows
- **Audio file filtering** — shows only supported audio/video formats (MP3, FLAC, OGG, WAV, M4A, AAC, WMA, Opus, MP4, AVI, MKV, MOV, WebM)
- **Add to playlist** — double-click file to add single track, double-click folder to add all audio files recursively
- **Resizable** — drag edges and corners (8-direction resize support, minimum 275x200)
- **Draggable window** — frameless window with titlebar drag
- **Keyboard shortcuts** — Alt+L to toggle, Escape to close
- **Menu toggle** — show/hide via Windows → Media library

### Bitmap Assets Used

| File | Size | Purpose |
|------|------|---------|
| `MAIN.BMP` | 275x116 | Main window background |
| `titlebar.bmp` | 344x87 | Titlebar active/inactive + buttons + windowshade sprites |
| `numbers.bmp` | 99x13 | LED digit font 0-9 + blank |
| `text.bmp` | 155x74 | Bitmap character font (A-Z, 0-9, symbols, extended) |
| `CBUTTONS.BMP` | 136x36 | Transport button sprites (normal + pressed) |
| `PLAYPAUS.BMP` | 42x9 | Play/pause/stop status icons |
| `MONOSTER.BMP` | 58x24 | Mono/stereo indicators |
| `POSBAR.BMP` | 307x10 | Seek bar background + thumb (normal + pressed) |
| `volume.bmp` | 68x433 | Volume slider 28 frames |
| `BALANCE.BMP` | 38x433 | Balance/pan slider 28 frames |
| `SHUFREP.BMP` | 92x85 | Shuffle, repeat, EQ, PL button states |
| `Eqmain.bmp` | 275x315 | Full EQ: background, buttons, sliders, graph |
| `Pledit.bmp` | 280x186 | Playlist: titlebar, borders, bottom bar, buttons |
| `gen.bmp` | 194x109 | Generic window frame + titlebar font for plugin windows |
| `genex.bmp` | 130x75 | Generic window buttons, scrollbars, and color palette |
| `WinampIcon.ico` | — | Application window icon |

Bitmap loading searches multiple fallback paths and merges missing assets from all candidates.

---

## TODO — Road to 1:1 Windows Parity

### High Priority

- [x] Real EQ DSP processing — 4Front EQ10 engine ported from original Winamp source (10-band IIR biquad with asymmetric Q, preamp lookup table, dynamic limiter); processes PCM via QAudioSink when EQ enabled
- [x] `viscolor.txt` parsing — load per-skin visualization colors; use `SPEC.BMP` (66x16)
- [x] Clutterbar — clickable options bar from `titlebar.bmp` sprite (x=304) with O/A/I/D/V buttons (Options, Always On Top, File Info, Double Size, Visualization)
- [x] Windowshade mode — compact single-bar mode for main, EQ, and playlist windows
- [x] Balance slider — `BALANCE.BMP` rendered with drag interaction
- [x] Playlist scrollbar — functional scroll thumb from Pledit.bmp sprites with drag support
- [x] Playlist resize — drag edges/corners; re-tile skin pieces dynamically
- [x] Playlist bottom button graphics — render ADD/REM/SEL/MISC/LIST button sprites from Pledit.bmp (all 5 buttons with 3-state popup menus)

### Medium Priority

- [x] Keyboard shortcuts — Space (play/pause), V (stop), Z/B (prev/next), C (pause), L (open file), J (jump to time), arrows (seek +/-5s), +/- (volume)
- [x] Jump to time dialog (Ctrl+J)
- [x] Jump to file dialog (Ctrl+J) — type-ahead search in playlist with filter and play/queue
- [x] File info dialog (Alt+3) — display/edit ID3 tags
- [x] Song title from metadata — read ID3/Vorbis tags instead of using filename
- [x] Time display toggle — click time area to switch between elapsed / remaining
- [x] Animated colon separator — use `nums_ex.bmp` if present
- [x] Double-size mode — 2x pixel scaling (Ctrl+D toggle)
- [x] Always on top — Ctrl+T toggle, fully wired and persisted
- [x] EQ frequency response curve — draw the actual curve in the EQ graph area
- [x] EQ AUTO behavior — auto-load preset based on file/genre
- [x] Playlist track numbering — show "1. Artist - Title (3:45)" format per entry
- [x] Playlist keyboard — Delete removes selected, Enter plays selected
- [x] Playlist info text from `pledit.txt` — skin-specific font colors and background via PLEDIT.TXT parser

### Low Priority

- [x] Media Library window — gen.bmp/genex.bmp-skinned file browser with recursive folder scanning, add-to-playlist via double-click
- [ ] Mini-Browser window
- [x] Video playback — QVideoWidget-based video window with frameless draggable window, video_logo.bmp display when idle, auto-show on video content detection, fullscreen support (F/double-click)
- [ ] Plugin architecture — input, output, DSP, general purpose, visualization plugin APIs
- [x] Milkdrop visualization — projectM-powered with 274 presets, fullscreen, audio-reactive; launchable from Visualization menu
- [x] Global hotkeys — MPRIS2 D-Bus integration for system-wide media key support (play/pause/next/prev/seek)
- [x] SHOUTcast/Icecast streaming with metadata display — auto-update title/tooltip on metadata change with notifications
- [x] Crossfade / gapless playback between tracks — dual-player preloading for seamless transitions
- [x] Bookmarks — save/recall favorite files and streams (persistent at `~/.config/winamp/bookmarks.txt`)
- [x] EQ preset files — import/export `.eqf` format (load/save/delete from presets menu)
- [x] Playlist generator — random playlist generation from music library with customizable track count, add or replace mode
- [ ] CD playback and ripping
- [x] System tray — minimize to tray with tooltip and controls
- [x] Command-line interface — `-play`, `-pause`, `-stop`, `-enqueue`, files and directories
- [x] Song change notifications — desktop notification popup when track changes (toggleable in preferences)
- [x] Localization — .lang file-based language pack system with UTF-8 support; English, German, Spanish included; extensible via ~/.winamp/lang/
- [ ] Modern skin support — Winamp 3/5 XML-based skins
- [x] Drag tracks out of playlist to file manager — export via file:// MIME data for external apps

### Architecture

- [ ] Multi-file refactor — split into separate source files per window/component
- [ ] Skin renderer abstraction — decouple skin loading from widget logic
- [ ] Audio engine abstraction — replaceable backend (PipeWire, PulseAudio, ALSA)
- [ ] Plugin ABI compatibility — load original Winamp `.dll` plugins via Wine bridge (stretch goal)

---

## File Structure

```
├── CMakeLists.txt              # Qt6 + OpenGL + projectM build configuration
├── README.md                   # This file
├── winamp_authentic.cpp        # Main implementation (~5200 lines)
├── skins/default/              # Default skin bitmaps
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

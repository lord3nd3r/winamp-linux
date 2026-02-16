# Winamp for Linux — Qt6 Native Port

A pixel-accurate recreation of the classic Winamp 2.x/5.x interface using Qt6 and C++17, targeting a **1:1 match** with the original Windows version. Uses the **real Winamp bitmap assets** from the source tree for fully authentic rendering.

![Winamp Linux](https://img.shields.io/badge/Winamp-Linux-00FF00?style=for-the-badge&logo=linux&logoColor=white)
![Qt6](https://img.shields.io/badge/Qt6-Multimedia-41CD52?style=for-the-badge&logo=qt&logoColor=white)
![C++17](https://img.shields.io/badge/C++-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)

---

## Building

```bash
# Install dependencies (Debian/Ubuntu)
sudo apt-get install -y qt6-base-dev qt6-multimedia-dev cmake ninja-build

# Build
cmake -B build -G Ninja
ninja -C build

# Run
./build/winamp
```

---

## Implemented Features

### Main Player Window (275x116 px)

- **Authentic MAIN.BMP background** — pixel-exact base image from the original skin
- **Skinned titlebar** — active/inactive states from `titlebar.bmp` (sprite offset 27,0 / 27,15), 275x14
- **Close and minimize buttons** — clickable hit zones in the titlebar
- **Play/Pause/Stop status indicator** — 9x9 icon at (26,28) from `PLAYPAUS.BMP` with 3 states
- **LED time display** — MM:SS rendered with `numbers.bmp` (9x13 glyphs) at correct digit positions (36,26 / 48,26 / 78,26 / 90,26); colon baked into `MAIN.BMP`
- **Scrolling song title** — bitmap font rendering from `text.bmp` (5x6 per character) in a clipped 154x6 region at (111,27); scrolls at 150ms with `***` separator wrap
- **Mono/Stereo indicator** — `MONOSTER.BMP` at (212,41); 4 sprite states (stereo on/off, mono on/off)
- **Transport buttons** — Previous, Play, Pause, Stop, Next, Eject from `CBUTTONS.BMP` with normal/pressed sprite rows; actions fire on mouse release
- **Volume slider** — 28-frame sprite animation from `volume.bmp` (68x13 each, 15px stride); click and drag, range 0-255
- **Position/seek bar** — background + thumb from `POSBAR.BMP` (248x10 bar, 29x10 thumb); draggable seek with distinct pressed thumb sprite
- **Shuffle and Repeat toggles** — `SHUFREP.BMP` on/off states (47x15 and 28x15)
- **EQ and PL toggle buttons** — `SHUFREP.BMP` on/off states (23x12 each); show/hide child windows
- **Frameless window** with custom titlebar dragging
- **Right-click context menu** — Play file, Play location, About Winamp, Exit

### Spectrum Analyzer and Oscilloscope

- **3 visualization modes** — Off / Spectrum Analyzer / Oscilloscope, cycled by clicking the viz area
- **Real-time audio analysis** — `QAudioBufferOutput` captures live PCM data (Int16 and Float sample formats)
- **512-point radix-2 FFT** — custom Cooley-Tukey implementation for spectrum computation
- **19-bar spectrum analyzer** — 3px-wide bars with 1px gaps in a 75x16 area; color gradient from green (bottom) to red (top) matching the original `ppal2[]` palette from `draw.cpp`; smooth falloff (1 unit/frame at 20fps); gravity-based peak dots
- **75-sample oscilloscope** — connected waveform drawn as vertical line segments
- **Authentic 24-entry color palette** — `visColors[]` matching Windows Winamp defaults

### 10-Band Graphic Equalizer (275x116 px)

- **Full Eqmain.bmp skin rendering** — background, titlebar (active/inactive), button sprites
- **ON/OFF toggle** — enables/disables EQ with sprite state feedback
- **AUTO toggle** — auto EQ mode flag
- **Preamp slider** — 0-63 range with 28-frame groove animation and 11x11 thumb from Eqmain sprite sheet
- **10 frequency band sliders** — 60Hz through 16kHz, same groove/thumb rendering as preamp
- **18 built-in presets** — Flat, Classical, Club, Dance, Full Bass, Full Bass & Treble, Full Treble, Laptop Speakers, Large Hall, Live, Party, Pop, Reggae, Rock, Ska, Soft, Soft Rock, Techno
- **Presets popup menu** — clickable button opens menu; applies preset values to all sliders
- **EQ graph background** — 113x19 from sprite (0,294)
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
- **ADD menu** (right-click) — Add file(s), Add directory (recursive scan), Add location
- **REM menu** (right-click) — Remove selected, Crop, Clear playlist, Remove misc
- **SEL menu** (right-click) — Select all, Select none, Invert selection
- **MISC menu** (right-click) — Sort by title, Sort by filename, Sort by path, Reverse list, Randomize list
- **Track persistence** — full track list saved/restored across sessions
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
- Volume, shuffle, repeat, EQ/PL visibility
- Last played file (restored on startup)
- Full EQ state: enabled, auto, preamp, all 10 band values, snap state
- Playlist: position, snap mode, dimensions, complete track list
- Skin path

### Dialogs

- **About Winamp** — version, copyright, tagline ("it really whips the llama's ass!")
- **Play Location** — URL input for stream/remote playback
- **Preferences** — tabbed dialog with General and Skins tabs; skin browser lists `~/.winamp/skins/` folders

### Audio

- **Qt Multimedia backend** (QMediaPlayer + QAudioOutput + QAudioBufferOutput)
- **Supported formats**: MP3, WAV, FLAC, OGG, M4A, AAC
- **URL playback** — play remote streams via URL dialog
- **Real-time audio buffer capture** for visualization (Int16 and Float sample formats)

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
| `SHUFREP.BMP` | 92x85 | Shuffle, repeat, EQ, PL button states |
| `Eqmain.bmp` | 275x315 | Full EQ: background, buttons, sliders, graph |
| `Pledit.bmp` | 280x186 | Playlist: titlebar, borders, bottom bar, buttons |
| `WinampIcon.ico` | — | Application window icon |

Bitmap loading searches multiple fallback paths and merges missing assets from all candidates.

---

## TODO — Road to 1:1 Windows Parity

### High Priority

- [ ] Skin system — load `.wsz` (ZIP) skin files; parse `viscolor.txt`; swap all bitmaps at runtime
- [ ] Real EQ DSP processing — apply 10-band EQ + preamp to actual audio output (currently visual only)
- [ ] Next/Previous track — wire to playlist (currently Previous seeks to 0, Next is a no-op)
- [ ] Shuffle playback — randomize playlist order when shuffle is on (flag exists, not wired)
- [ ] Repeat modes — repeat track / repeat playlist (flag exists, not wired)
- [ ] Auto-advance — play next track when current one finishes
- [ ] Bitrate / sample rate display — show kbps and kHz in the main window info area
- [ ] Clutterbar — clickable options bar from `titlebar.bmp` sprite (x=304)
- [ ] Windowshade mode — compact single-bar mode for main, EQ, and playlist windows
- [ ] Balance slider — `BALANCE.BMP` (68x433) exists, not loaded or rendered
- [ ] Playlist scrollbar — functional scroll thumb from Pledit.bmp sprites
- [ ] Playlist resize — drag bottom-right corner; re-tile skin pieces dynamically
- [ ] Playlist bottom buttons — render ADD/REM/SEL/MISC/LIST button graphics from Pledit.bmp sprites (currently right-click menus only)
- [ ] Visualization colors from skin — load `viscolor.txt` from skin for per-skin viz colors; use `SPEC.BMP` (66x16)

### Medium Priority

- [ ] Keyboard shortcuts — Space (play/pause), V (stop), Z/B (prev/next), C (pause), L (open file), J (jump to time), arrows (seek +/-5s), +/- (volume)
- [ ] Jump to time dialog (Ctrl+J)
- [ ] Jump to file dialog (J) — type-ahead search in playlist
- [ ] File info dialog (Alt+3) — display/edit ID3 tags
- [ ] Song title from metadata — read ID3/Vorbis tags instead of using filename
- [ ] Time display toggle — click time area to switch between elapsed / remaining
- [ ] Animated colon separator — use `nums_ex.bmp` if present
- [ ] Double-size mode — 2x pixel scaling (Ctrl+D or clutterbar toggle)
- [ ] Always on top — context menu toggle (slot exists, wiring incomplete)
- [ ] EQ frequency response curve — draw the actual curve in the EQ graph area
- [ ] EQ AUTO behavior — auto-load preset based on file/genre
- [ ] Playlist track numbering — show "1. Artist - Title (3:45)" format per entry
- [ ] Playlist keyboard — Delete removes selected, Enter plays selected
- [ ] Save/Load playlist — .m3u / .pls export and import
- [ ] Playlist info text from `pledit.txt` — skin-specific font colors and background

### Low Priority

- [ ] Media Library window — general purpose window using `gen.bmp` / `genex.bmp`
- [ ] Mini-Browser window
- [ ] Video playback — video window with `video.bmp` / `video_logo.bmp` skin
- [ ] Plugin architecture — input, output, DSP, general purpose, visualization plugin APIs
- [ ] Milkdrop / AVS visualization plugins
- [ ] Global hotkeys — system-wide media key support
- [ ] SHOUTcast/Icecast streaming with metadata display
- [ ] Crossfade / gapless playback between tracks
- [ ] Bookmarks — save/recall favorite files and streams
- [ ] EQ preset files — import/export `.eqf` format
- [ ] Playlist generator — auto-generate from library
- [ ] CD playback and ripping
- [ ] System tray — minimize to tray with tooltip and controls
- [ ] Command-line interface — `/PLAY`, `/PAUSE`, `/STOP`, `/NEXT`, `/PREV`, `/ADD`
- [ ] Localization — language pack support
- [ ] Modern skin support — Winamp 3/5 XML-based skins
- [ ] Song change notifications
- [ ] Drag tracks out of playlist to file manager

### Architecture

- [ ] Multi-file refactor — split into separate source files per window/component
- [ ] Skin renderer abstraction — decouple skin loading from widget logic
- [ ] Audio engine abstraction — replaceable backend (PipeWire, PulseAudio, ALSA)
- [ ] Plugin ABI compatibility — load original Winamp `.dll` plugins via Wine bridge (stretch goal)

---

## File Structure

```
├── CMakeLists.txt              # Qt6 build configuration
├── README.md                   # This file
├── winamp_authentic.cpp        # Main implementation (~1940 lines)
├── skins/default/              # Default skin bitmaps
├── Src/Winamp/resource/        # Additional bitmap assets
└── build/
    └── winamp                  # Executable
```

Skin bitmaps loaded from `skins/default/` and `Src/Winamp/resource/` in the source tree.

---

## License

Based on the Winamp source code released by Winamp SA. See repository [LICENSE.md](LICENSE.md) for details.

---

*It really whips the llama's ass!*

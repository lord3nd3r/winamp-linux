# Project Context

## Architecture
- **Core Orchestrator**: `src/main.cpp` is a large monolithic application file (approx. 212KB) that contains the central application state, UI initialization, and core event handling.
- **Modular Components**: Auxiliary functionality is broken out into header files in `src/`:
  - `equalizer.h`, `eq_dsp.h`: UI and DSP logic for the 10-band equalizer (George Yohng's EQ10 port).
  - `playlist.h`, `media_library.h`: Playlist window and media management.
  - `preferences.h`: Preferences window, including the in-app plugin manager.
  - `python_plugin.h`: Pybind11 integration for embedded Python scripts.
  - `mpris2_adaptors.h`: DBus and desktop environment integration.
  - `video.h`, `milkdrop.h`, `modern_skin.h`, `dialogs.h`, etc.: Various window and feature integrations.

## Core Logic & Data Flows
1. **Audio Playback**: Uses `QMediaPlayer` via Qt's Multimedia backend (GStreamer on Linux). To achieve gapless playback, it features a dual preload/swap architecture where the next track is preloaded while the current one finishes.
2. **DSP Processing**: Audio stream data is intercepted and passed through `eq10dsp` pre-allocated buffers. This allows glitch-free real-time EQ processing.
3. **Plugin System**: An embedded Python interpreter loads scripts from `~/.config/winamp/plugins/`. Plugins receive a `winamp.Api` object, which maps Python calls to core C++ functions safely.
4. **Visualizations**: FFT data from the audio stream is passed to a logarithmic spectrum analyzer (matching the original `sa_tab[]` distribution) or to projectM/Milkdrop via `libprojectm`.

## Key Decisions
- **Qt5/Qt6 Dual Support**: Supporting both provides a fallback for older distributions while enabling newer features (full MPRIS2, DSP processing) on Qt6.
- **Header-centric Organization**: Despite the monolithic `main.cpp`, new UI components and systems are separated into headers to mitigate file size growth.
- **Async Metadata Probing**: Duration probing happens asynchronously using a single shared `QMediaPlayer` queue with a 5-second timeout. This replaces older blocking patterns and keeps the UI responsive during large playlist loads.

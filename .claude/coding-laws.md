# Coding Laws & Standards

## 1. C++ and Framework Standards
- **Standard**: C++17.
- **Framework**: Qt5 or Qt6. The codebase must compile cleanly against both.
- **Zero Warnings Policy**: Ensure code compiles with zero errors and zero warnings.
- **Code Style**: Match the existing coding style closely.

## 2. Design & Architecture
- **UI & Skinning**: Classic Winamp UI is the primary focus. Modern Winamp 5 skins are currently disabled intentionally because they can break the UI. Do not attempt to forcefully enable them unless instructed.
- **Header Files Structure**: The project modularizes UI components in headers (e.g., `playlist.h`, `equalizer.h`) while keeping `main.cpp` as the core orchestrator. 
- **Asynchronous Execution**: Respect the async nature of operations like playlist loading. Use the shared `QMediaPlayer` queues for metadata probing with timeouts to avoid blocking the main thread.
- **Plugins Integration**: Python plugins use pybind11. Ensure the Python plugin interface (`winamp.Api`) remains stable and well-documented when extending features.

## 3. Safety & Integrity
- **Do NOT modify existing core logic** unless explicitly instructed by the user. 
- **Audio Integrity**: Carefully manage resources for audio playback. The gapless playback architecture uses a dual `QMediaPlayer` preload/swap mechanism; be extremely careful when touching playback logic to avoid glitches or regressions.
- **DSP**: The EQ uses pre-allocated audio buffers for glitch-free real-time processing. Do not introduce dynamic allocations in the audio path.

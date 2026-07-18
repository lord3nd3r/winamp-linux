# Winamp for Linux - AI Assistant Guidelines

## Project Overview
Winamp for Linux is a native, Qt-based (Qt5/Qt6) media player inspired by the classic Winamp. It features classic skins support, a playlist editor, a 10-band equalizer (ported from George Yohng's EQ10), a logarithmic spectrum analyzer, Python plugin support (via pybind11), and MPRIS2 DBus integration.

## Bootstrap / Build Instructions
- **Dependencies**: Qt5/Qt6 (Multimedia, OpenGL, Base), CMake, Ninja, pybind11, projectm, gstreamer.
- **Build**:
  ```bash
  # Qt6 (Preferred)
  cmake -S . -B build-qt6 -G Ninja
  ninja -C build-qt6
  
  # Qt5 fallback
  cmake -S . -B build-qt5 -G Ninja -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON
  ninja -C build-qt5
  ```
- **Containerized Build (Ubuntu 26.04/Resolute .deb packaging)**:
  ```bash
  docker run -it -v $(pwd):/workspace -w /workspace ubuntu:resolute bash
  # Inside container:
  apt-get update && apt-get install -y cmake ninja-build build-essential qt6-base-dev qt6-multimedia-dev libprojectm-dev pybind11-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgl1-mesa-dev git
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
  ninja -C build package
  ```
- **Run**: `./build-qt6/winamp` or `./build-qt5/winamp`

## AI Operating Rules
1. **Never modify existing logic unless explicitly asked.** Always begin by analyzing the codebase thoroughly.
2. Check the `.claude/` folder at the start of every session for coding standards (`coding-laws.md`) and architectural context (`project-context.md`).
3. For every future task, maintain and update the following tracking files inside `.claude/`:
   - `progress-history-{task}.md`
   - `learned-{task}.md`
   - `bug-fix-{task}.md`
4. Prioritize using specific tools (e.g., `view_file`, `grep_search`, `write_to_file`) over generic bash commands.
5. Provide concise updates and confirm explicitly when maintaining tracking files.

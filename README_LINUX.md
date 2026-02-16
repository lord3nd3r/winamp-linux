# Winamp for Linux - Qt6 Port

This is a native Linux port of Winamp using Qt6 as the UI foundation. The goal is to maintain the classic Winamp look and feel while running natively on Linux.

## Project Status

🚧 **Early Development** - Basic framework in place

### Completed
- ✅ CMake build system for Linux
- ✅ Qt6 integration and project structure
- ✅ Win32-to-Qt abstraction layer (QtWindowAdapter, QtCanvasAdapter)
- ✅ Basic main window with classic Winamp appearance
- ✅ Qt6 Multimedia integration for audio playback
- ✅ Drag-and-drop file support
- ✅ Basic playback controls

### In Progress
- 🔨 Wasabi framework porting to Qt
- 🔨 Plugin system adaptation
- 🔨 Skin loading system

### TODO
- ⏳ Full GDI-to-QPainter rendering pipeline
- ⏳ Equalizer window
- ⏳ Playlist editor window
- ⏳ Visualization plugins
- ⏳ Media Library
- ⏳ Complete skin support
- ⏳ Input/Output plugin framework
- ⏳ All DSP effects

## Building

### Prerequisites

#### Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    qt6-base-dev \
    qt6-multimedia-dev \
    libqt6multimedia6 \
    qt6-tools-dev \
    libgl1-mesa-dev \
    libx11-dev \
    libxext-dev \
    zlib1g-dev \
    libpng-dev \
    libjpeg-dev
```

#### Fedora/RHEL:
```bash
sudo dnf install -y \
    gcc gcc-c++ cmake ninja-build \
    qt6-qtbase-devel \
    qt6-qtmultimedia-devel \
    qt6-qttools-devel \
    mesa-libGL-devel \
    libX11-devel \
    libXext-devel \
    zlib-devel \
    libpng-devel \
    libjpeg-turbo-devel
```

#### Arch Linux:
```bash
sudo pacman -S \
    base-devel cmake ninja \
    qt6-base qt6-multimedia qt6-tools \
    mesa libx11 libxext \
    zlib libpng libjpeg-turbo
```

### Build Instructions

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    ..

# Build
ninja

# Install (optional)
sudo ninja install
```

### Running

```bash
# From build directory
./Src/Winamp/winamp

# With files
./Src/Winamp/winamp ~/Music/*.mp3

# Command line options
./Src/Winamp/winamp --help
```

## Architecture

### Layer Structure

```
┌─────────────────────────────────────┐
│   Winamp Application (main.cpp)    │
│   - Classic UI                      │
│   - Playlist management             │
│   - Plugin management               │
└────────────┬────────────────────────┘
             │
┌────────────▼────────────────────────┐
│   Qt6 Abstraction Layer             │
│   - QtWindowAdapter (HWND→QWidget)  │
│   - QtCanvasAdapter (HDC→QPainter)  │
│   - Event mapping (MSG→QEvent)      │
└────────────┬────────────────────────┘
             │
┌────────────▼────────────────────────┐
│   Wasabi Framework                  │
│   - Window management               │
│   - Skin engine                     │
│   - Component services              │
└────────────┬────────────────────────┘
             │
┌────────────▼────────────────────────┐
│   Qt6 Framework                     │
│   - QWidget, QPainter               │
│   - QMediaPlayer, QAudioOutput      │
│   - QOpenGL for visualization       │
└─────────────────────────────────────┘
```

### Win32 to Qt6 Mapping

| Win32 Concept | Qt6 Equivalent | Adapter |
|---------------|----------------|---------|
| HWND | QWidget* | QtWindowAdapter |
| HDC | QPainter* | QtCanvasAdapter |
| WndProc | QWidget::event() | Event mapping |
| WM_* messages | QEvent types | QtWindowAdapter |
| BitBlt/StretchBlt | QPainter::drawPixmap() | QtCanvasAdapter |
| CreateWindowEx | new QWidget | QtWindowAdapter |
| DirectSound | QAudioOutput | QMediaPlayer |
| DirectX/GDI | QPainter/QOpenGL | QtCanvasAdapter |

## Features

### Currently Working
- ✅ Basic audio playback (MP3, WAV, OGG, FLAC, M4A)
- ✅ Classic Winamp interface (275x116px)
- ✅ Transport controls (Play, Pause, Stop, Previous, Next)
- ✅ Drag and drop files
- ✅ Time display
- ✅ Track title display
- ✅ Keyboard shortcuts

### Keyboard Shortcuts
- **Space**: Play/Pause
- **S**: Stop
- **Z**: Previous track
- **B**: Next track

## Development

### Project Structure

```
Src/
├── Wasabi/              # UI framework
│   ├── api/            # Wasabi APIs
│   ├── bfc/            # Base foundation classes
│   ├── qt6/            # Qt6 adapter layer ← NEW
│   │   ├── QtWindowAdapter.cpp
│   │   └── QtCanvasAdapter.cpp
│   └── CMakeLists.txt
├── Winamp/             # Main application
│   ├── linux/          # Linux-specific code ← NEW
│   │   ├── main_linux.cpp
│   │   ├── main_window_qt.cpp
│   │   └── main_window_qt.h
│   └── CMakeLists.txt
├── nu/                 # Nullsoft utilities
├── tataki/             # Codec abstraction
└── [plugins]/          # Various plugins (to be ported)
```

### Next Steps for Contributors

1. **Skin Loading**: Implement full .wsz/.zip skin loading
2. **Visualization**: Port visualization plugins to OpenGL
3. **Plugins**: Adapt input/output plugin system for Linux
4. **Playlist Editor**: Create Qt-based playlist window
5. **Equalizer**: Port 10-band equalizer to Qt
6. **Media Library**: Port modern ML interface

## Differences from Windows Version

### By Design
- Uses Qt6 Multimedia instead of DirectSound
- Uses Qt's event loop instead of Win32 message loop
- Native Linux window management
- XDG-compliant file paths (~/.config/winamp)

### Temporary Limitations
- Some plugins not yet ported
- Skin support incomplete
- Media library not implemented
- No Milkdrop visualization yet

## Contributing

This is a massive undertaking. Contributions welcome!

Priority areas:
1. Skin rendering system
2. Plugin framework adaptation
3. Visualization plugins (especially Milkdrop)
4. Input format plugins
5. Output backend optimization
6. Testing and bug fixes

## License

Same as original Winamp source code. See LICENSE.md.

## Credits

- Original Winamp by Nullsoft/AOL/Radionomy
- Linux port using Qt6 framework
- Community contributors

## Resources

- [Qt6 Documentation](https://doc.qt.io/qt-6/)
- [Original Winamp Plugin SDK](https://github.com/WinampDesktop/winamp)
- [Wasabi SDK Documentation](https://wiki.winamp.com/wiki/Wasabi_SDK)

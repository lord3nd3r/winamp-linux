#!/bin/bash
# AppImage build recipe for Winamp
# Requires: linuxdeploy, linuxdeploy-plugin-qt

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build_appimage"
APPDIR="${BUILD_DIR}/AppDir"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}Building Winamp AppImage...${NC}"

# Download linuxdeploy if not present
LINUXDEPLOY="${BUILD_DIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="${BUILD_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"

mkdir -p "$BUILD_DIR"

if [ ! -f "$LINUXDEPLOY" ]; then
    echo -e "${YELLOW}Downloading linuxdeploy...${NC}"
    wget -q -O "$LINUXDEPLOY" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY"
fi

if [ ! -f "$LINUXDEPLOY_QT" ]; then
    echo -e "${YELLOW}Downloading linuxdeploy Qt plugin...${NC}"
    wget -q -O "$LINUXDEPLOY_QT" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY_QT"
fi

# Build the project
echo -e "${YELLOW}Building Winamp...${NC}"
cd "$PROJECT_ROOT"
cmake -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD_DIR"
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR"

# Copy icon for AppImage
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
if [ -f "$PROJECT_ROOT/Src/Winamp/resource/WinampIcon.ico" ]; then
    cp "$PROJECT_ROOT/Src/Winamp/resource/WinampIcon.ico" \
       "$APPDIR/usr/share/icons/hicolor/256x256/apps/winamp.ico"
fi

# Build AppImage
echo -e "${YELLOW}Creating AppImage...${NC}"
export QMAKE=$(which qmake6 2>/dev/null || which qmake 2>/dev/null)
export VERSION="0.5.0-beta"

cd "$BUILD_DIR"
"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/winamp.desktop" \
    --plugin qt \
    --output appimage

echo -e "${GREEN}AppImage created successfully!${NC}"
ls -lh "$BUILD_DIR"/Winamp*.AppImage 2>/dev/null || ls -lh "$BUILD_DIR"/winamp*.AppImage 2>/dev/null

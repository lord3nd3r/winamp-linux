#!/bin/bash
# Quick build script for Winamp Linux

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}╔═══════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   Winamp for Linux - Build Script   ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════╝${NC}"
echo ""

# Check if running on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo -e "${RED}Error: This build script is for Linux only${NC}"
    exit 1
fi

# Check for required tools
echo -e "${YELLOW}Checking dependencies...${NC}"

MISSING_DEPS=()

command -v cmake >/dev/null 2>&1 || MISSING_DEPS+=("cmake")
command -v ninja >/dev/null 2>&1 || MISSING_DEPS+=("ninja-build")
command -v g++ >/dev/null 2>&1 || MISSING_DEPS+=("g++")

# Check for Qt6
if ! pkg-config --exists Qt6Core 2>/dev/null; then
    MISSING_DEPS+=("qt6-base-dev qt6-multimedia-dev")
fi

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo -e "${RED}Missing dependencies: ${MISSING_DEPS[*]}${NC}"
    echo ""
    echo "Install with:"
    
    if command -v apt-get >/dev/null 2>&1; then
        echo "  sudo apt-get install ${MISSING_DEPS[*]}"
    elif command -v dnf >/dev/null 2>&1; then
        echo "  sudo dnf install ${MISSING_DEPS[*]}"
    elif command -v pacman >/dev/null 2>&1; then
        echo "  sudo pacman -S ${MISSING_DEPS[*]}"
    fi
    exit 1
fi

echo -e "${GREEN}✓ All dependencies found${NC}"
echo ""

# Build type
BUILD_TYPE="${1:-Release}"
BUILD_DIR="build_linux"

echo -e "${YELLOW}Build configuration:${NC}"
echo "  Build type: $BUILD_TYPE"
echo "  Build directory: $BUILD_DIR"
echo ""

# Clean old build if requested
if [ "$2" == "clean" ]; then
    echo -e "${YELLOW}Cleaning old build...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    ..

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Configuration successful${NC}"
echo ""

# Build
echo -e "${YELLOW}Building Winamp...${NC}"
ninja -v

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}╔═══════════════════════════════════════╗${NC}"
echo -e "${GREEN}║        Build Successful! 🎉          ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════╝${NC}"
echo ""
echo -e "Run Winamp with:"
echo -e "  ${GREEN}cd $BUILD_DIR && ./Src/Winamp/winamp${NC}"
echo ""
echo -e "Or install system-wide:"
echo -e "  ${GREEN}cd $BUILD_DIR && sudo ninja install${NC}"
echo ""

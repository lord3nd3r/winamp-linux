#!/bin/bash
# ═══════════════════════════════════════════════════════
#  Winamp for Linux — Release Packaging Script
#  Generates .deb, .rpm, .tar.gz, and AppImage packages
# ═══════════════════════════════════════════════════════
set -e

VERSION="0.5.0"
RELEASE="beta1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build_release"
OUTPUT_DIR="${SCRIPT_DIR}/release"
ARCH="$(uname -m)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

print_header() {
    echo ""
    echo -e "${GREEN}╔═══════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║  Winamp ${VERSION} BETA — Release Packager       ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════╝${NC}"
    echo ""
}

usage() {
    echo "Usage: $0 [OPTIONS] [TARGETS...]"
    echo ""
    echo "Targets (build one or more, default: all):"
    echo "  deb        Debian/Ubuntu .deb package (requires dpkg-deb)"
    echo "  rpm        Fedora/RHEL .rpm package (requires rpmbuild)"
    echo "  tarball    Generic .tar.gz archive"
    echo "  appimage   Universal AppImage (requires wget for tools)"
    echo "  all        Build all available package types"
    echo ""
    echo "Options:"
    echo "  --clean    Remove build directory before starting"
    echo "  --help     Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                  # build all packages"
    echo "  $0 deb tarball      # build only .deb and .tar.gz"
    echo "  $0 --clean rpm      # clean build, then make .rpm"
}

# ── Parse arguments ────────────────────────────────────
CLEAN=false
TARGETS=()

for arg in "$@"; do
    case "$arg" in
        --clean) CLEAN=true ;;
        --help|-h) usage; exit 0 ;;
        deb|rpm|tarball|appimage|all) TARGETS+=("$arg") ;;
        *) echo -e "${RED}Unknown argument: $arg${NC}"; usage; exit 1 ;;
    esac
done

# Default to all
if [ ${#TARGETS[@]} -eq 0 ]; then
    TARGETS=("all")
fi

# Expand "all" into individual targets
if [[ " ${TARGETS[*]} " =~ " all " ]]; then
    TARGETS=("tarball")
    command -v dpkg-deb >/dev/null 2>&1 && TARGETS+=("deb")
    command -v rpmbuild >/dev/null 2>&1 && TARGETS+=("rpm")
    command -v wget >/dev/null 2>&1 && TARGETS+=("appimage")
fi

print_header

echo -e "${CYAN}Targets:${NC} ${TARGETS[*]}"
echo -e "${CYAN}Architecture:${NC} ${ARCH}"
echo ""

# ── Dependency check ──────────────────────────────────
echo -e "${YELLOW}Checking build dependencies...${NC}"
MISSING=()
command -v cmake >/dev/null 2>&1 || MISSING+=("cmake")
command -v ninja >/dev/null 2>&1 || MISSING+=("ninja-build")
command -v g++ >/dev/null 2>&1   || MISSING+=("g++")

if [ ${#MISSING[@]} -ne 0 ]; then
    echo -e "${RED}Missing: ${MISSING[*]}${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Core build tools found${NC}"

# ── Clean ─────────────────────────────────────────────
if $CLEAN; then
    echo -e "${YELLOW}Cleaning previous build...${NC}"
    rm -rf "$BUILD_DIR" "$OUTPUT_DIR"
fi

mkdir -p "$BUILD_DIR" "$OUTPUT_DIR"

# ── Build ─────────────────────────────────────────────
echo ""
echo -e "${YELLOW}Configuring & building Winamp...${NC}"
cmake -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    "$SCRIPT_DIR"

cmake --build "$BUILD_DIR" --parallel

echo -e "${GREEN}✓ Build successful${NC}"

# ── Install to staging area ───────────────────────────
STAGING="${BUILD_DIR}/staging"
rm -rf "$STAGING"
DESTDIR="$STAGING" cmake --install "$BUILD_DIR"
echo -e "${GREEN}✓ Staged install to ${STAGING}${NC}"

# ═══════════════════════════════════════════════════════
#  Package builders
# ═══════════════════════════════════════════════════════

build_tarball() {
    echo ""
    echo -e "${CYAN}── Building .tar.gz ──${NC}"
    local NAME="winamp-${VERSION}-${RELEASE}-linux-${ARCH}"
    local TARDIR="${BUILD_DIR}/${NAME}"
    rm -rf "$TARDIR"
    cp -a "$STAGING/usr" "$TARDIR"
    # Add a simple run script
    cat > "$TARDIR/run-winamp.sh" <<'RUNEOF'
#!/bin/bash
DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="${DIR}/lib:${LD_LIBRARY_PATH}"
exec "${DIR}/bin/winamp" "$@"
RUNEOF
    chmod +x "$TARDIR/run-winamp.sh"
    cp "$SCRIPT_DIR/LICENSE.md" "$TARDIR/"
    cp "$SCRIPT_DIR/README.md" "$TARDIR/"

    tar -czf "${OUTPUT_DIR}/${NAME}.tar.gz" -C "$BUILD_DIR" "$NAME"
    echo -e "${GREEN}✓ ${OUTPUT_DIR}/${NAME}.tar.gz${NC}"
}

build_deb() {
    echo ""
    echo -e "${CYAN}── Building .deb ──${NC}"
    local NAME="winamp_${VERSION}-${RELEASE}_amd64"
    local DEBROOT="${BUILD_DIR}/${NAME}"
    rm -rf "$DEBROOT"

    # Copy staged files
    cp -a "$STAGING" "$DEBROOT"

    # Create DEBIAN control
    mkdir -p "${DEBROOT}/DEBIAN"
    cat > "${DEBROOT}/DEBIAN/control" <<EOF
Package: winamp
Version: ${VERSION}-${RELEASE}
Section: sound
Priority: optional
Architecture: amd64
Maintainer: lord3nd3r <lord3nd3r@github.com>
Homepage: https://github.com/lord3nd3r/winamp-linux
Description: Winamp media player for Linux (Qt6 port)
 A native Linux port of the legendary Winamp media player using Qt6.
 Features the classic Winamp look and feel with support for classic skins,
 equalizer, playlist editor, media library, and MilkDrop visualizations.
Depends: libqt6core6 (>= 6.2) | libqt6core6t64 (>= 6.2), libqt6gui6 (>= 6.2) | libqt6gui6t64 (>= 6.2), libqt6widgets6 (>= 6.2) | libqt6widgets6t64 (>= 6.2), libqt6multimedia6 (>= 6.2) | libqt6multimedia6t64 (>= 6.2), libqt6opengl6 (>= 6.2) | libqt6opengl6t64 (>= 6.2), libgl1-mesa-glx | libgl1
EOF

    # Post-install: update desktop database
    cat > "${DEBROOT}/DEBIAN/postinst" <<'EOF'
#!/bin/bash
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q /usr/share/icons/hicolor 2>/dev/null || true
fi
EOF
    chmod 755 "${DEBROOT}/DEBIAN/postinst"

    dpkg-deb --build --root-owner-group "$DEBROOT" "${OUTPUT_DIR}/${NAME}.deb"
    echo -e "${GREEN}✓ ${OUTPUT_DIR}/${NAME}.deb${NC}"
}

build_rpm() {
    echo ""
    echo -e "${CYAN}── Building .rpm ──${NC}"

    local RPMBUILD_DIR="${BUILD_DIR}/rpmbuild"
    mkdir -p "${RPMBUILD_DIR}"/{BUILD,RPMS,SOURCES,SPECS,SRPMS,BUILDROOT}

    # Create tarball source for rpmbuild
    local SRCNAME="winamp-${VERSION}"
    local SRCDIR="${RPMBUILD_DIR}/SOURCES"
    rm -rf "${BUILD_DIR}/${SRCNAME}"
    cp -a "$SCRIPT_DIR" "${BUILD_DIR}/${SRCNAME}"
    tar -czf "${SRCDIR}/${SRCNAME}.tar.gz" -C "$BUILD_DIR" "$SRCNAME"
    rm -rf "${BUILD_DIR}/${SRCNAME}"

    # Copy spec file
    cp "$SCRIPT_DIR/packaging/winamp.spec" "${RPMBUILD_DIR}/SPECS/"

    rpmbuild --define "_topdir ${RPMBUILD_DIR}" \
             -bb "${RPMBUILD_DIR}/SPECS/winamp.spec" || {
        echo -e "${YELLOW}⚠ rpmbuild failed — you may need to install build dependencies first${NC}"
        echo -e "${YELLOW}  Try: sudo dnf builddep packaging/winamp.spec${NC}"
        return 1
    }

    # Move RPM to output
    find "${RPMBUILD_DIR}/RPMS" -name "*.rpm" -exec cp {} "$OUTPUT_DIR/" \;
    echo -e "${GREEN}✓ RPM package built in ${OUTPUT_DIR}/${NC}"
}

build_appimage() {
    echo ""
    echo -e "${CYAN}── Building AppImage ──${NC}"
    bash "$SCRIPT_DIR/packaging/appimage/build-appimage.sh" && \
        cp "${SCRIPT_DIR}/build_appimage"/*.AppImage "$OUTPUT_DIR/" 2>/dev/null
    echo -e "${GREEN}✓ AppImage built in ${OUTPUT_DIR}/${NC}"
}

# ── Execute requested targets ─────────────────────────
for target in "${TARGETS[@]}"; do
    case "$target" in
        tarball)  build_tarball ;;
        deb)      build_deb ;;
        rpm)      build_rpm ;;
        appimage) build_appimage ;;
    esac
done

# ── Summary ───────────────────────────────────────────
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║          Packaging Complete! 🎉               ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BOLD}Release artifacts:${NC}"
ls -lh "$OUTPUT_DIR"/ 2>/dev/null
echo ""
echo -e "Upload these to your GitHub release page:"
echo -e "  ${CYAN}https://github.com/lord3nd3r/winamp-linux/releases/new${NC}"

#!/usr/bin/env bash
# Build Winamp-<version>-x86_64.AppImage on an old glibc (Ubuntu 22.04).
# Qt 6, its plugins, GStreamer, TagLib, and projectM are bundled.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT/build-appimage"}"
APPDIR="$BUILD_DIR/AppDir"
TOOLS="$BUILD_DIR/tools"

if [[ -z "${VERSION:-}" ]]; then
    if [[ "${GITHUB_REF_NAME:-}" == v* ]]; then
        VERSION="${GITHUB_REF_NAME#v}"
    else
        VERSION="$(sed -n 's/^inline constexpr const char \*kWinampVersion = "\([^"]*\)".*/\1/p' "$ROOT/src/constants.h")"
    fi
fi
if [[ -z "$VERSION" ]]; then
    echo "Could not determine the AppImage version" >&2
    exit 1
fi

OUTPUT="${OUTPUT:-"$BUILD_DIR/Winamp-${VERSION}-x86_64.AppImage"}"

echo "Building Winamp ${VERSION} AppImage"
echo "  source: $ROOT"
echo "  output: $OUTPUT"

cmake -S "$ROOT" -B "$BUILD_DIR/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD_DIR/build" --parallel "$(nproc)"
rm -rf "$APPDIR"
cmake --install "$BUILD_DIR/build" --prefix "$APPDIR/usr"

# projectM presets and a font, so Milkdrop works without host packages.
if [[ -d /usr/share/projectM/presets ]]; then
    mkdir -p "$APPDIR/usr/share/projectM"
    cp -a /usr/share/projectM/presets "$APPDIR/usr/share/projectM/"
fi
font_src=""
for candidate in \
    /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \
    /usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf
do
    if [[ -f "$candidate" ]]; then
        font_src="$candidate"
        break
    fi
done
if [[ -n "$font_src" ]]; then
    mkdir -p "$APPDIR/usr/share/fonts/truetype/dejavu"
    cp -a "$font_src" "$APPDIR/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
fi

# Filename version wins over the string baked in at configure time, so a
# v* tag still produces AppStream metadata for that version.
metainfo="$APPDIR/usr/share/metainfo/io.github.lord3nd3r.winamp.metainfo.xml"
if [[ -f "$metainfo" ]]; then
    sed -i "s/<release version=\"[^\"]*\"/<release version=\"${VERSION}\"/" "$metainfo"
fi

mkdir -p "$TOOLS"
download() {
    local url="$1" dest="$2"
    if [[ ! -f "$dest" ]]; then
        curl -fsSL "$url" -o "$dest"
        chmod +x "$dest"
    fi
}
download "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" \
    "$TOOLS/linuxdeploy-x86_64.AppImage"
download "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" \
    "$TOOLS/linuxdeploy-plugin-qt-x86_64.AppImage"
download "https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gstreamer/432af639725bc8b99df12351e86bb2e8a6b3585f/linuxdeploy-plugin-gstreamer.sh" \
    "$TOOLS/linuxdeploy-plugin-gstreamer.sh"

export APPIMAGE_EXTRACT_AND_RUN=1
export ARCH=x86_64
export QMAKE="${QMAKE:-qmake6}"
# offscreen is what headless smoke tests and QT_QPA_PLATFORM=offscreen use.
export EXTRA_PLATFORM_PLUGINS="${EXTRA_PLATFORM_PLUGINS:-libqwayland-egl.so;libqwayland-generic.so;libqoffscreen.so}"
export LDAI_OUTPUT="$OUTPUT"
export LDAI_UPDATE_INFORMATION="${LDAI_UPDATE_INFORMATION:-gh-releases-zsync|lord3nd3r|winamp-linux|latest|Winamp-*x86_64.AppImage.zsync}"
export UPDATE_INFORMATION="$LDAI_UPDATE_INFORMATION"
export VERSION

# OpenSSL is how Qt reaches https streams. Pass the SONAME explicitly so it
# is bundled even when linuxdeploy would otherwise leave it on the host.
ldconfig -p > "$BUILD_DIR/ldconfig.txt" || true
ssl_lib="$(awk '/libssl\.so\./ { print $NF; exit }' "$BUILD_DIR/ldconfig.txt")"
crypto_lib="$(awk '/libcrypto\.so\./ { print $NF; exit }' "$BUILD_DIR/ldconfig.txt")"
# HarfBuzz is pulled in by Qt's text stack and is not part of the graphics
# driver set linuxdeploy leaves on the host. FriBidi is the same story for
# the bundled libgstlibav decoder: without it, MP3 fails on a clean host.
harfbuzz_lib="$(awk '/libharfbuzz\.so\./ { print $NF; exit }' "$BUILD_DIR/ldconfig.txt")"
fribidi_lib="$(awk '/libfribidi\.so\./ { print $NF; exit }' "$BUILD_DIR/ldconfig.txt")"
extra_libs=()
[[ -n "$ssl_lib" ]] && extra_libs+=(--library "$ssl_lib")
[[ -n "$crypto_lib" ]] && extra_libs+=(--library "$crypto_lib")
[[ -n "$harfbuzz_lib" ]] && extra_libs+=(--library "$harfbuzz_lib")
[[ -n "$fribidi_lib" ]] && extra_libs+=(--library "$fribidi_lib")

(
    cd "$TOOLS"
    ./linuxdeploy-x86_64.AppImage \
        --appdir "$APPDIR" \
        --executable "$APPDIR/usr/bin/winamp" \
        --desktop-file "$ROOT/winamp.desktop" \
        --icon-file "$ROOT/icons/hicolor/256x256/apps/winamp.png" \
        "${extra_libs[@]}" \
        --plugin qt \
        --plugin gstreamer \
        --output appimage
)

if [[ ! -f "$OUTPUT" ]]; then
    # linuxdeploy sometimes names the file from the desktop Name= field.
    found="$(find "$TOOLS" "$BUILD_DIR" -maxdepth 2 -name 'Winamp*.AppImage' ! -name 'linuxdeploy*' -print -quit || true)"
    if [[ -n "$found" && "$found" != "$OUTPUT" ]]; then
        mv "$found" "$OUTPUT"
    fi
fi
if [[ ! -f "$OUTPUT" ]]; then
    echo "AppImage was not produced at $OUTPUT" >&2
    find "$BUILD_DIR" -name '*.AppImage' -print >&2 || true
    exit 1
fi
chmod +x "$OUTPUT"

# appimagetool writes the .zsync next to its working directory, which is the
# tools dir, not necessarily next to LDAI_OUTPUT.
if [[ ! -f "${OUTPUT}.zsync" ]]; then
    zsync_found="$(find "$TOOLS" "$BUILD_DIR" -name "$(basename "$OUTPUT").zsync" -print -quit || true)"
    if [[ -n "$zsync_found" ]]; then
        mv "$zsync_found" "${OUTPUT}.zsync"
    fi
fi
if [[ ! -f "${OUTPUT}.zsync" ]]; then
    echo "zsync file was not created next to $OUTPUT" >&2
    exit 1
fi
echo "zsync: ${OUTPUT}.zsync"

echo "Checking bundled runtime pieces"
python3 - "$APPDIR" <<'PY'
import os, sys
appdir = sys.argv[1]
required_substrings = [
    "libqxcb.so",
    "libqwayland-generic.so",
    "libqwayland-egl.so",
    "libtag.so",
    "libprojectM.so",
    "libssl.so",
    "libcrypto.so",
    "libfribidi.so",
]
# Image formats, multimedia backend, and a GStreamer decoder/sink.
want_any = [
    ("imageformats", ("libqjpeg.so", "libqico.so", "libqgif.so")),
    # Qt 6.4+ ships a multimedia plugin. Ubuntu 22.04's Qt 6.2 links
    # GStreamer straight into libQt6Multimedia instead.
    ("qt-multimedia", ("libQt6Multimedia.so.6", "libgstreamermediaplugin.so", "libgstmediaplugin.so", "libffmpegmediaplugin.so")),
    ("gstreamer-lib", ("libgstreamer-1.0.so.0",)),
    ("gstreamer-core", ("libgstcoreelements.so",)),
    ("gstreamer-audio", ("libgstaudioconvert.so", "libgstplayback.so")),
    ("gstreamer-sink", ("libgstalsa.so", "libgstpulseaudio.so", "libgstpipewire.so")),
    ("tls", ("libqopensslbackend.so", "libssl.so.3")),
]
found = []
for dirpath, _, files in os.walk(appdir):
    found.extend(files)
missing = [name for name in required_substrings if not any(name in f for f in found)]
for label, options in want_any:
    if not any(opt in found for opt in options):
        missing.append(label + " (" + " or ".join(options) + ")")
if missing:
    sys.stderr.write("AppDir is missing:\n  " + "\n  ".join(missing) + "\n")
    sys.exit(1)
print("AppDir contains Qt platforms, multimedia, GStreamer, TagLib, projectM, and OpenSSL")
PY

if [[ ! -f "$APPDIR/usr/share/applications/winamp.desktop" ]]; then
    echo "Desktop file was not installed into the AppDir" >&2
    exit 1
fi
if ! find "$APPDIR/usr/share/icons" -name 'winamp.png' | grep -q .; then
    echo "Themed winamp icon is missing from the AppDir" >&2
    exit 1
fi
metainfo="$APPDIR/usr/share/metainfo/io.github.lord3nd3r.winamp.metainfo.xml"
if [[ ! -f "$metainfo" ]]; then
    echo "AppStream metainfo is missing from the AppDir" >&2
    exit 1
fi
if ! grep -q "<release version=\"${VERSION}\"" "$metainfo"; then
    echo "AppStream metadata is not version ${VERSION}" >&2
    exit 1
fi

echo "Built $OUTPUT"
ls -lh "$OUTPUT" "${OUTPUT}.zsync" 2>/dev/null || ls -lh "$OUTPUT"

if [[ "${APPIMAGE_SMOKE:-}" == "1" ]]; then
    echo "Smoke test: --help / --version / offscreen / --cli"
    help_text="$("$OUTPUT" --help)"
    grep -q "Winamp ${VERSION}" <<<"$help_text"
    grep -q -- "--cli" <<<"$help_text"
    "$OUTPUT" --version | grep -q "Winamp ${VERSION}"

    export HOME="$BUILD_DIR/smoke-home"
    mkdir -p "$HOME"
    smoke_log="$BUILD_DIR/smoke-offscreen.log"
    set +e
    QT_QPA_PLATFORM=offscreen timeout 8 "$OUTPUT" >"$smoke_log" 2>&1
    smoke_rc=$?
    set -e
    if [[ "$smoke_rc" -ne 124 && "$smoke_rc" -ne 0 ]]; then
        echo "Offscreen launch exited $smoke_rc" >&2
        cat "$smoke_log" >&2 || true
        exit 1
    fi
    if grep -E "error while loading shared libraries|undefined symbol|^Aborted" "$smoke_log"; then
        echo "Offscreen launch failed" >&2
        exit 1
    fi

    tone="$BUILD_DIR/tone.wav"
    python3 - "$tone" <<'PY'
import math, struct, sys, wave
path = sys.argv[1]
rate = 44100
n = rate // 5
with wave.open(path, "w") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(rate)
    frames = b"".join(
        struct.pack("<h", int(32767 * 0.2 * math.sin(2 * math.pi * 440 * i / rate)))
        for i in range(n)
    )
    w.writeframes(frames)
PY
    cli_log="$BUILD_DIR/smoke-cli.log"
    set +e
    # 'q' quits the terminal UI. A missing multimedia backend shows up here.
    printf 'q' | QT_QPA_PLATFORM=offscreen timeout 20 "$OUTPUT" --cli "$tone" >"$cli_log" 2>&1
    cli_rc=$?
    set -e
    tui_log="$HOME/.cache/winamp/winamp-tui.log"
    if grep -E "error while loading shared libraries|undefined symbol|no decoder available|Could not find audio backend|Failed to create media backend" "$cli_log" "$tui_log" 2>/dev/null; then
        echo "CLI playback smoke test reported a broken audio backend" >&2
        cat "$cli_log" >&2 || true
        exit 1
    fi
    if [[ "$cli_rc" -ne 0 && "$cli_rc" -ne 124 ]]; then
        echo "CLI smoke test exited $cli_rc" >&2
        cat "$cli_log" >&2 || true
        cat "$tui_log" >&2 || true
        exit 1
    fi
    echo "Smoke tests passed"
fi

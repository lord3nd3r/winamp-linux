#!/bin/bash
# Build the Arch package from the git checkout. Run as root after pacman has
# installed base-devel, cmake, ninja, qt6-base, qt6-multimedia, qt6-svg,
# qt6-tools, taglib, ffmpeg, python, pkgconf, and projectm (3.1.12).
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
pkgver=$(sed -n 's/^pkgver=//p' "$root/packaging/arch/PKGBUILD")
work=$(mktemp -d)
cp "$root/packaging/arch/PKGBUILD" "$work/"
git -C "$root" archive --format=tar.gz --prefix="winamp-linux-${pkgver}/" \
  -o "$work/winamp-${pkgver}.tar.gz" HEAD

if ! id builder >/dev/null 2>&1; then
  useradd -m builder
fi
chown -R builder:builder "$work"

runuser -u builder -- env HOME=/home/builder \
  bash -lc "cd '$work' && makepkg --noconfirm --skipinteg --nodeps" \
  | tee /tmp/winamp-makepkg.log

grep -F "Using Qt6" /tmp/winamp-makepkg.log
grep -F "libavutil found - HAVE_AVUTIL" /tmp/winamp-makepkg.log

mkdir -p "$root/build"
rm -f "$root/build/winamp-${pkgver}-arch.pkg.tar.zst" \
  "$root/build/winamp-${pkgver}-arch.tar.gz"
cp "$work"/winamp-*.pkg.tar.zst "$root/build/winamp-${pkgver}-arch.pkg.tar.zst"
cp "$work"/src/build/winamp-${pkgver}-arch.tar.gz "$root/build/"
ls -l "$root/build/winamp-${pkgver}-arch.pkg.tar.zst" \
  "$root/build/winamp-${pkgver}-arch.tar.gz"

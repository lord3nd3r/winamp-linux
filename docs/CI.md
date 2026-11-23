# CI and release

Workflows live in `.github/workflows/`. How to build locally is in [BUILDING.md](BUILDING.md).

| Workflow | Path | Trigger | What it does |
|----------|------|---------|--------------|
| CI | `.github/workflows/ci.yml` | Push / PR to `master` | Build and `ctest` |
| Release | `.github/workflows/release.yml` | Tags `v*`, `workflow_dispatch` | Full package matrix. On a `v*` tag, attach the artifacts to the GitHub release |

## Release packages

| Distro ID | Image | Package |
|-----------|-------|---------|
| `ubuntu-22.04` | `ubuntu:22.04` | `.deb` + `.tar.gz` |
| `ubuntu-24.04` | `ubuntu:24.04` | `.deb` + `.tar.gz` |
| `ubuntu-resolute` | `ubuntu:resolute` | `.deb` + `.tar.gz` |
| `ubuntu-stonking` | `ubuntu:26.10` | `.deb` + `.tar.gz` (`libprojectm3` 3.1.12, not `libprojectm2v5`) |
| `debian-bookworm` | `debian:bookworm` | `.deb` + `.tar.gz` |
| `debian-trixie` | `debian:trixie` | `.deb` + `.tar.gz` |
| `fedora-43` | `fedora:43` | `.rpm` + `.tar.gz` |
| `fedora-44` | `fedora:44` | `.rpm` + `.tar.gz` |
| `fedora45` | `fedora:45` | `.rpm` + `.tar.gz` (`winamp-<version>-fedora45`) |
| `opensuse-tumbleweed` | `opensuse/tumbleweed` | `.rpm` + `.tar.gz` |
| `arch` | `archlinux:latest` | `.pkg.tar.zst` + `.tar.gz` (`projectm` 3.1.12) |

The `appimage` job (Ubuntu 22.04) builds `Winamp-<version>-x86_64.AppImage` and its `.zsync`. Details are in [BUILDING.md](BUILDING.md#appimage).

## What CI checks

The CI matrix is Ubuntu 22.04, Ubuntu 24.04, Ubuntu 26.10 (stonking), Debian Bookworm, Fedora 43, Fedora 44, Fedora 45, and openSUSE Tumbleweed, plus two extra jobs: `ubuntu-22.04-qt5` and `arch`.

Every job installs libavutil and fails configure unless the log contains `libavutil found - HAVE_AVUTIL`. Package names: `libavutil-dev` (Debian/Ubuntu), `libavutil-free-devel` (Fedora), `libavutil-devel` (openSUSE).

The Qt 5 job configures with `-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON` into `build-qt5` and also requires `Using Qt5`.

The Arch job installs Extra `projectm` (3.1.12), not `libprojectm` 4.x and not AUR `projectm-git`. It requires `Using Qt6` and `libprojectM` 3.x.

Ubuntu 26.10 installs `libprojectm-dev`, which depends on `libprojectm3` (= 3.1.12). `dpkg-shlibdeps` records that shared library as `libprojectm3` in the `.deb` Depends line.

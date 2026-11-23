# Contributing to Winamp for Linux

Small, reviewable changes. The deep references live next door:

| Doc | What it covers |
|-----|----------------|
| [docs/BUILDING.md](docs/BUILDING.md) | Dependencies, Qt 5 / Qt 6, CMake, tests, CPack, AppImage |
| [docs/CI.md](docs/CI.md) | CI and release workflows, distro matrix |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Components, CLI, terminal UI, paths, limits |
| [CONFIGURATION.md](CONFIGURATION.md) | `winamp.conf` |
| [PLUGIN_DEVELOPMENT.md](PLUGIN_DEVELOPMENT.md) | Python plugin API |

## Collaboration

- Commit messages say what changed and why.
- Do not commit agent or IDE private instruction stores, or vendor-named local harness directories.
- Match the surrounding C++. Do not reformat unrelated files.
- Do not change gapless playback unless that change is the point of the work.
- Classic UI fidelity beats a speculative restyle of the chrome.

## Build and test

Qt 6, Debug, from a clone:

```bash
cmake -S . -B build-qt6 -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja -C build-qt6
QT_QPA_PLATFORM=offscreen ctest --test-dir build-qt6 --output-on-failure
```

Package lists, the Qt 5 configure, CPack, and what each test binary covers are in [docs/BUILDING.md](docs/BUILDING.md). New tests: behavior over private snapshots, register them in `CMakeLists.txt`, and keep them off the network unless mocked.

## Coding standards

| Rule | Detail |
|------|--------|
| Language | C++17. Qt 5 and Qt 6 must both keep compiling |
| Warnings | None, with the flags in [BUILDING.md](docs/BUILDING.md#compiler-flags) |
| Strings | `QString` and Qt file APIs |
| Ownership | Qt parent/child (`new Widget(this)`). `WA_DeleteOnClose` for short-lived dialogs |
| Headers | Forward-declare in headers; include in the `.cpp` when you can |
| Audio | No unbounded locks and no heap allocation in the DSP loop |
| Skins | Classic stays the default. Do not turn modern skins on by default |
| Plugins | Keep public method names stable and note changes in `PLUGIN_DEVELOPMENT.md` |

Match the file you are editing. Skip drive-by renames. Prefer a new `.h` / `.cpp` over growing `winamp_window.h` when a change is really a new module.

## Where a change goes

| Kind of change | Primary files |
|----------------|---------------|
| Main window / playback orchestration | `src/winamp_window.h` |
| Playlist | `src/playlist.{h,cpp}` |
| EQ UI | `src/equalizer.{h,cpp}` |
| EQ math | `src/eq_dsp.h` and `tests/test_eq_dsp.cpp` |
| Plugin host or API | `src/python_plugin.{h,cpp}` and `PLUGIN_DEVELOPMENT.md` |
| Preferences | `src/preferences.{h,cpp}` |
| Network streams | `playHttpStream` in `src/winamp_window.h` |
| Skin constants / vis colors | `src/constants.h`, `src/winamp_bitmaps.h` |
| Build, deps, install | `CMakeLists.txt`, [docs/BUILDING.md](docs/BUILDING.md) |
| User-facing docs | `README.md`, `CONFIGURATION.md`, `docs/` |

## Before you push

- [ ] Qt 6 build (`build-qt6`)
- [ ] Qt 5 build if you touched `compat.h`, multimedia ifdefs, or shared headers
- [ ] `ctest --test-dir build-qt6 --output-on-failure`
- [ ] No new compiler warnings
- [ ] No secrets, personal absolute paths, or private agent files
- [ ] Docs updated when behavior or the plugin API changes
- [ ] A short test note when the change is on the audio or gapless path

## Debugging

```bash
QT_LOGGING_RULES="*.debug=true" ./build-qt6/winamp
./build-qt6/winamp 2>&1 | grep -E 'Python|Plugin'
mv ~/.config/winamp ~/.config/winamp.bak.$(date +%s)
```

The last command throws away local settings.

| Symptom | Check |
|---------|--------|
| No sound, or short formats fail | GStreamer plugins; try another file type |
| Plugins silent | `python3` on `PATH`, `~/.config/winamp/plugins/*.py`, `[Plugin Host]` lines |
| Media keys dead | Qt 6 build and `Qt6DBus` at configure time |
| Visualization blank | `libprojectM` and `projectm-data` |
| Skin wrong | Classic vs modern path; `WinampBitmaps::loadAll` |

## License of contributions

Contributions are licensed under the same **GPL-2.0** terms as the project (`LICENSE.md`).

# Python plugin development

Winamp for Linux extends itself through **user Python scripts** loaded in a **separate process**. This guide covers the sandbox model, lifecycle, full API reference, logging rules, packaging tips, and the shipped examples.

---

## Table of contents

1. [Architecture](#1-architecture)
2. [Requirements](#2-requirements)
3. [Install a plugin](#3-install-a-plugin)
4. [Lifecycle](#4-lifecycle)
5. [API reference](#5-api-reference)
6. [Logging & stdout rules](#6-logging--stdout-rules)
7. [Threading & performance](#7-threading--performance)
8. [Enable / disable / configure](#8-enable--disable--configure)
9. [Examples](#9-examples)
10. [Icecast DJ plugin](#10-icecast-dj-plugin)
11. [Security model](#11-security-model)
12. [Troubleshooting](#12-troubleshooting)

---

## 1. Architecture

```text
┌──────────────────────────┐         JSON-RPC (stdin/stdout)        ┌────────────────────────────┐
│  winamp (C++ / Qt)       │ ◄──────────────────────────────────── ► │  python3                   │
│  PythonPluginManager     │   notifications + requests/responses    │  winamp_plugin_host.py      │
│  (GUI thread handlers)   │                                         │  loads ~/.config/.../*.py   │
└──────────────────────────┘                                         └────────────────────────────┘
```

| Piece | Location | Notes |
|-------|----------|--------|
| Manager | `src/python_plugin.{h,cpp}` | Spawns host, dispatches methods |
| Host script | `~/.cache/winamp/winamp_plugin_host.py` | Regenerated at startup |
| Plugins | `~/.config/winamp/plugins/*.py` | Your code |

**Why out-of-process?**  
An exception, infinite loop, or native crash inside a plugin kills the host process, not the player. The UI and audio pipeline keep running (plugins simply stop until restart).

**RPC details (host):**

- Notifications (fire-and-forget): `play`, `pause`, `set_volume`, …
- Requests (wait for result, **5 s** timeout): `get_volume`, `is_playing`, …
- A dedicated stdin reader thread in the host avoids deadlocks when plugins call request APIs during `on_winamp_start`

---

## 2. Requirements

| Requirement | Notes |
|-------------|--------|
| `python3` on `PATH` | Invoked as `python3 /path/to/host.py` |
| Standard library | Host uses `json`, `threading`, `importlib`, `queue` only |
| Extra packages | Only if *your* plugin imports them (`pip install --user …`) |
| Optional tools | e.g. `ffmpeg` for the Icecast DJ example |

No pybind11 or embedded interpreter is required to *run* plugins.

---

## 3. Install a plugin

### Manual

```bash
mkdir -p ~/.config/winamp/plugins
cp plugins/examples/hello_winamp.py ~/.config/winamp/plugins/
# restart Winamp
```

### From Preferences

**Options → Preferences → Plug-ins → Add…** then restart when prompted.

### First plugin template

`~/.config/winamp/plugins/hello.py`:

```python
import sys

api = None

def on_winamp_start(winamp_api):
    global api
    api = winamp_api
    print("Plugin loaded", file=sys.stderr)
    print(f"volume={api.get_volume()}/255", file=sys.stderr)
    print(f"playlist={api.playlist_count()}", file=sys.stderr)

def on_winamp_exit():
    print("Plugin exit", file=sys.stderr)
```

Run the player from a terminal to see host and plugin logs:

```bash
./build-qt6/winamp 2>&1 | grep -E 'Python|Plugin'
```

---

## 4. Lifecycle

| Hook | When | Arguments |
|------|------|-----------|
| `on_winamp_start(api)` | Host finished starting; plugins imported | API proxy instance |
| `on_winamp_exit()` | Player shutting down (best effort) | none |

### Load rules

- Only files ending in `.py` are imported as plugins  
- `winamp_plugin_host.py` is never loaded as a plugin  
- Hidden files (leading `.`) are skipped  
- `name.py.disabled` is **not** a `.py` plugin file (disabled)  
- Import errors are reported on the host stderr and other plugins continue loading  

### Restart requirement

Enable/disable/add/remove in Preferences typically requires a **player restart** so the host rescans modules.

---

## 5. API reference

The object passed to `on_winamp_start` exposes the following methods. Names match the historical embedded API so older scripts keep working.

### Playback control

| Method | Signature | Behavior |
|--------|-----------|----------|
| `play_track` | `(path: str) -> None` | Play this path/URL immediately |
| `play` | `() -> None` | Resume / start playback |
| `pause` | `() -> None` | Toggle pause (pause if playing, play if paused) |
| `stop` | `() -> None` | Stop |
| `next_track` | `() -> None` | Next playlist item |
| `prev_track` | `() -> None` | Previous playlist item |
| `seek` | `(seconds: float) -> None` | Seek to position |

### Volume

| Method | Signature | Behavior |
|--------|-----------|----------|
| `get_volume` | `() -> int` | 0–255 |
| `set_volume` | `(level: int) -> None` | Clamped to 0–255 |

### Status

| Method | Signature | Behavior |
|--------|-----------|----------|
| `get_current_file` | `() -> str` | Current path/URL or empty |
| `is_playing` | `() -> bool` | Playing state |
| `is_paused` | `() -> bool` | Paused state |
| `get_position` | `() -> float` | Position in **seconds** |
| `get_duration` | `() -> float` | Duration in **seconds** |

### Playlist

| Method | Signature | Behavior |
|--------|-----------|----------|
| `playlist_count` | `() -> int` | Number of entries |
| `playlist_add` | `(path: str) -> None` | Append file or URL |
| `playlist_clear` | `() -> None` | Clear playlist |

### Return values & failures

- Request APIs return `None` if the host times out (5 s) or the player is unavailable  
- Unknown methods are logged on the C++ side as unimplemented RPC methods  
- There is no structured exception channel back to Python for player-side failures—check return values  

---

## 6. Logging & stdout rules

| Stream | Use |
|--------|-----|
| **stderr** | All plugin logs (`print(..., file=sys.stderr)`) |
| **stdout** | Reserved for JSON-RPC between host and player |

The host remaps plugin `sys.stdout` to stderr and keeps a private RPC pipe. Still write logs to **stderr** explicitly so behavior stays clear if a plugin spawns subprocesses.

Player-side lines look like:

```text
[Python Plugins] Host process started successfully (PID: …)
[Python Plugins] Hello Winamp Plugin loaded!
[Plugin Host] Loaded hello_winamp.py
```

---

## 7. Threading & performance

- Plugins run **off** the Winamp GUI process; blocking the plugin host no longer freezes the classic UI chrome.
- Blocking `on_winamp_start` still delays **other plugins** from loading—start worker threads for long work.
- Request APIs (`get_volume`, etc.) block the calling Python thread up to 5 seconds.
- Prefer **notifications** when you do not need a return value.
- Polling loops should sleep (`time.sleep`) and exit cleanly when `on_winamp_exit` runs.

---

## 8. Enable / disable / configure

### Preferences UI

**Plug-ins** page supports:

| Action | Effect |
|--------|--------|
| Enable / Disable | Rename `foo.py` ↔ `foo.py.disabled` |
| Configure | Opens `foo.json` if present, else the `.py` file |
| Add | Copy a `.py` into the plugins directory |
| Remove | Delete the selected plugin file |
| Refresh | Rescan directory listing |
| Open folder | File manager at the plugins path |

### Manual disable

```bash
mv ~/.config/winamp/plugins/hello.py ~/.config/winamp/plugins/hello.py.disabled
```

---

## 9. Examples

### 9.1 Hello plugin

Shipped as `plugins/examples/hello_winamp.py`. Prints volume and playlist length at start.

### 9.2 Track history logger

```python
import os
import sys
import threading
import time

api = None
_running = False
_last = ""

def _loop():
    global _last
    path = os.path.expanduser("~/winamp_history.txt")
    while _running:
        if api and api.is_playing():
            cur = api.get_current_file()
            if cur and cur != _last:
                _last = cur
                ts = time.strftime("%Y-%m-%d %H:%M:%S")
                try:
                    with open(path, "a", encoding="utf-8") as f:
                        f.write(f"[{ts}] {cur}\n")
                except OSError as e:
                    print(f"history write failed: {e}", file=sys.stderr)
        time.sleep(2)

def on_winamp_start(winamp_api):
    global api, _running
    api = winamp_api
    _running = True
    threading.Thread(target=_loop, daemon=True).start()
    print("history logger started", file=sys.stderr)

def on_winamp_exit():
    global _running
    _running = False
```

### 9.3 Auto-enqueue a directory on start

```python
import sys
from pathlib import Path

def on_winamp_start(api):
    root = Path.home() / "Music" / "Incoming"
    if not root.is_dir():
        print(f"missing {root}", file=sys.stderr)
        return
    for p in sorted(root.rglob("*")):
        if p.suffix.lower() in {".mp3", ".flac", ".ogg", ".m4a", ".wav"}:
            api.playlist_add(str(p))
    print("enqueued incoming music", file=sys.stderr)

def on_winamp_exit():
    pass
```

---

## 10. Icecast DJ plugin

Shipped example: `plugins/examples/icecast_dj.py`.

### What it does

- Watches the currently playing track  
- Streams to one or more Icecast mount points via **`ffmpeg`**  
- Optional metadata updates, reconnect, multi-mount configuration  

### Setup

```bash
sudo apt-get install -y ffmpeg
cp plugins/examples/icecast_dj.py ~/.config/winamp/plugins/
# first run writes ~/.config/winamp/plugins/icecast_dj.json
```

Edit `icecast_dj.json`:

1. Set `"enabled": true`  
2. Configure `server.host`, `port`, `password`, mounts  
3. Restart Winamp  

Never commit real Icecast passwords into git. Keep secrets only in your local JSON config.

### Config surface (summary)

| Area | Keys (high level) |
|------|-------------------|
| Server | host, port, username, password, protocol, tls |
| Mounts | mountpoint, format, bitrate, samplerate, channels, … |
| Reconnect | enabled, delay_seconds, max_attempts |
| Metadata | update_on_track_change, format string |
| Logging | enabled, file path |

See the defaults at the top of `icecast_dj.py` for the full schema.

---

## 11. Security model

| Layer | Protection |
|-------|------------|
| Process isolation | Plugin crashes do not tear down `winamp` |
| RPC allowlist | Only implemented methods in `handleRequest` run |
| **Not** provided | Filesystem jail, seccomp, network policy, memory quotas |

**Treat plugins as full user-level code.** A plugin can read your home directory, open network sockets, and spawn processes (as Icecast DJ does with `ffmpeg`). Only install plugins you trust.

---

## 12. Troubleshooting

| Symptom | Checks |
|---------|--------|
| No `[Python Plugins] Host process started` | Is `python3` installed and on `PATH`? |
| Host starts but no plugins load | Files under `~/.config/winamp/plugins/` ending in `.py`? Disabled suffix? |
| `JSON parse error` on player | Something wrote non-JSON to the RPC stdout path—use stderr for logs |
| API returns `None` | Player busy/timeout (5 s); call fewer request APIs during heavy UI work |
| Changes not applied | Restart Winamp after adding/editing plugins |
| Icecast silent | `ffmpeg -version`; JSON `enabled`; server credentials; firewall |

### Manual host test (advanced)

The host expects the parent to speak JSON. Prefer testing via the real player. If you experiment manually, send:

```json
{"type":"event","name":"start"}
```

followed by a newline on the host’s stdin, and read stdout for RPC frames only.

---

## See also

- [CONFIGURATION.md](CONFIGURATION.md) — paths and INI keys  
- [CONTRIBUTING.md](CONTRIBUTING.md) — changing the C++ host/API  
- [README.md](README.md) — product overview  

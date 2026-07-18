# Python Plugin Development Guide

Winamp for Linux loads user plugins in a **separate `python3` process** (JSON-RPC over stdin/stdout). Plugin crashes cannot take down the player process. The public API surface matches the original `winamp.Api` methods so existing example scripts keep working.

---

## 1. Quick Start

### Plugin Directory
Winamp scans `~/.config/winamp/plugins/` at startup for all `*.py` files (except internal host scripts) and loads them. Disable a plugin by renaming it to `name.py.disabled` (or use Preferences → Plug-ins).

### Requirements
- System `python3` on `PATH`
- Optional deps for specific plugins (e.g. `ffmpeg` for the Icecast DJ example)

### Create Your First Plugin
Create a file at `~/.config/winamp/plugins/hello.py`:

```python
# hello.py
api = None

def on_winamp_start(winamp_api):
    global api
    api = winamp_api
    print("Winamp Plugin Loaded Successfully!", file=__import__('sys').stderr)
    print(f"Current volume level: {api.get_volume()}/255", file=__import__('sys').stderr)

def on_winamp_exit():
    print("Winamp Plugin Shutdown!", file=__import__('sys').stderr)
```

Start Winamp from the terminal; host and plugin messages appear as `[Python Plugins]` / `[Plugin Host]` on stderr.

---

## 2. Lifecycle Callbacks

Your plugins should define these entry points:

- **`on_winamp_start(api)`**: Called when the sandbox host starts. Receives an API proxy with the methods below.
- **`on_winamp_exit()`**: Called when Winamp is shutting down. Close files, stop threads, flush sockets here.

> [!NOTE]
> **Isolation**: Plugins run out-of-process. Blocking inside `on_winamp_start` no longer freezes the Winamp UI, but it still delays other plugins from loading. Prefer `threading.Thread` for long work. API calls that return a value wait up to 5 seconds for a reply from the player.

---

## 3. API Reference (API proxy)

The `winamp_api` object passed to `on_winamp_start` exposes the following methods (same names as the former embedded `winamp.Api`):

### Playback Control
- `play_track(path: str)`: Clears the current player queue state and plays the given file path immediately.
- `play()`: Resumes playback if paused or stopped.
- `pause()`: Toggles the paused state.
- `stop()`: Stops playback.
- `next_track()`: Skips forward to the next item in the playlist.
- `prev_track()`: Skips backward to the previous item in the playlist.
- `seek(seconds: float)`: Jumps to the specified position in the track.

### Volume Control
- `get_volume() -> int`: Returns current volume level (0 to 255).
- `set_volume(level: int)`: Sets the volume level (0 to 255).

### Playback Status
- `get_current_file() -> str`: Returns the absolute file path of the currently loaded track (returns an empty string if nothing is loaded).
- `is_playing() -> bool`: Returns `True` if audio is currently playing.
- `is_paused() -> bool`: Returns `True` if audio is currently paused.
- `get_position() -> float`: Returns current playback position in seconds.
- `get_duration() -> float`: Returns total duration of the track in seconds.

### Playlist Control
- `playlist_count() -> int`: Returns the total number of items in the playlist.
- `playlist_add(path: str)`: Appends a local file or remote URL to the playlist.
- `playlist_clear()`: Clears all items from the playlist.

---

## 4. Example: Background Track History Logger

This script logs the path of every song you play to a text file in your home directory without blocking the user interface:

```python
import threading
import time
import os

api = None
logger_thread = None
running = False
last_track = ""

def track_monitor():
    global last_track, running
    
    log_path = os.path.expanduser("~/winamp_history.txt")
    
    while running:
        if api and api.is_playing():
            current = api.get_current_file()
            if current and current != last_track:
                last_track = current
                timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
                try:
                    with open(log_path, "a") as f:
                        f.write(f"[{timestamp}] Played: {current}\n")
                except Exception as e:
                    print(f"Error logging track: {e}")
        
        # Check every 2 seconds
        time.sleep(2)

def on_winamp_start(winamp_api):
    global api, logger_thread, running
    api = winamp_api
    running = True
    
    # Launch background thread to prevent GUI freezing
    logger_thread = threading.Thread(target=track_monitor, daemon=True)
    logger_thread.start()
    print("📋 Track history logger background thread started.")

def on_winamp_exit():
    global running
    running = False
    print("📋 Track history logger thread terminated.")
```

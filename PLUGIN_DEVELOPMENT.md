# Python Plugin Development Guide

Winamp for Linux features an embedded Python interpreter (powered by `pybind11`) that allows you to extend the player's functionality with Python scripts.

---

## 1. Quick Start

### Plugin Directory
Winamp scans `~/.config/winamp/plugins/` at startup for all `*.py` files and loads them.

### Create Your First Plugin
Create a file at `~/.config/winamp/plugins/hello.py`:

```python
# hello.py
api = None

def on_winamp_start(winamp_api):
    global api
    api = winamp_api
    print("🎵 Winamp Plugin Loaded Successfully!")
    print(f"Current volume level: {api.get_volume()}/255")

def on_winamp_exit():
    print("🎵 Winamp Plugin Shutdown!")
```

Start Winamp from the terminal to see the standard output prints.

---

## 2. Lifecycle Callbacks

Your plugins should define these entry points:

- **`on_winamp_start(api)`**: Called when Winamp is initialized. Receives a `winamp.Api` helper object to control the player.
- **`on_winamp_exit()`**: Called when the user exits the application. Use this to close files, flush network sockets, or stop threads.

> [!WARNING]
> **Thread Blocking**: Winamp runs the lifecycle callbacks directly on the main GUI thread.
> If your plugin blocks or performs long-running network or file operations inside `on_winamp_start`, the entire application GUI will freeze.
> Always run polling or background tasks in a helper `threading.Thread`.

---

## 3. API Reference (`winamp.Api`)

The `winamp_api` object passed to `on_winamp_start` exposes the following methods:

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

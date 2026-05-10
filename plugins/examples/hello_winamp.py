# hello_winamp.py — Example Winamp Linux Python Plugin
# Place this file in ~/.config/winamp/plugins/

api = None

def on_winamp_start(winamp_api):
    """Called when Winamp starts and loads this plugin."""
    global api
    api = winamp_api
    print("🎵 Hello Winamp Plugin loaded!")
    print(f"   Volume: {api.get_volume()}/255")
    print(f"   Playlist tracks: {api.playlist_count()}")

def on_winamp_exit():
    """Called when Winamp is shutting down."""
    print("🎵 Hello Winamp Plugin: Goodbye!")

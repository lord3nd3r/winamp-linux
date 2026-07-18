# hello_winamp.py — Example Winamp Linux Python Plugin
# Place this file in ~/.config/winamp/plugins/

api = None

def on_winamp_start(winamp_api):
    """Called when Winamp starts and loads this plugin."""
    import sys
    global api
    api = winamp_api
    # Prefer stderr: the host remaps stdout for JSON-RPC, but stderr always reaches the player log.
    print("Hello Winamp Plugin loaded!", file=sys.stderr)
    print(f"   Volume: {api.get_volume()}/255", file=sys.stderr)
    print(f"   Playlist tracks: {api.playlist_count()}", file=sys.stderr)

def on_winamp_exit():
    """Called when Winamp is shutting down."""
    import sys
    print("Hello Winamp Plugin: Goodbye!", file=sys.stderr)

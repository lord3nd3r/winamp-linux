# icecast_dj.py — Winamp Linux Icecast DJ Streaming Plugin
# Place in ~/.config/winamp/plugins/
#
# Streams the currently playing track to an Icecast server using ffmpeg.
# Supports multiple mount points, metadata updates, auto-reconnect,
# and full configuration via icecast_dj.json.
#
# Requirements: ffmpeg (apt install ffmpeg)

import json
import os
import subprocess
import threading
import time
import signal

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

DEFAULT_CONFIG = {
    "enabled": False,
    "autostart": False,

    # Icecast server connection
    "server": {
        "host": "localhost",
        "port": 8000,
        "username": "source",
        "password": "hackme",
        "protocol": "http",
        "tls": False
    },

    # Mount point configuration — you can define multiple mounts
    "mounts": [
        {
            "name": "primary",
            "enabled": True,
            "mountpoint": "/stream",
            "format": "mp3",
            "bitrate": 192,
            "samplerate": 44100,
            "channels": 2,
            "genre": "Various",
            "description": "Winamp DJ Stream",
            "url": "",
            "public": False
        }
    ],

    # DJ metadata
    "dj": {
        "name": "",
        "description": "",
        "url": ""
    },

    # Behavior
    "reconnect": {
        "enabled": True,
        "delay_seconds": 5,
        "max_attempts": 0
    },

    # Metadata
    "metadata": {
        "update_on_track_change": True,
        "format": "{artist} - {title}",
        "fallback_to_filename": True
    },

    # Logging
    "log": {
        "enabled": True,
        "file": ""
    }
}

# ---------------------------------------------------------------------------
# Globals
# ---------------------------------------------------------------------------

_api = None
_config = {}
_config_path = ""
_streams = {}
_monitor_thread = None
_running = False
_log_file = None


def _log(msg):
    """Log a message to console and optionally to file."""
    line = f"[Icecast DJ] {msg}"
    print(line)
    if _log_file:
        try:
            _log_file.write(f"{time.strftime('%Y-%m-%d %H:%M:%S')} {line}\n")
            _log_file.flush()
        except Exception:
            pass


def _load_config():
    """Load or create the configuration file."""
    global _config, _config_path
    config_dir = os.path.expanduser("~/.config/winamp/plugins")
    _config_path = os.path.join(config_dir, "icecast_dj.json")

    if os.path.exists(_config_path):
        try:
            with open(_config_path, "r") as f:
                _config = json.load(f)
            _log(f"Config loaded from {_config_path}")
        except Exception as e:
            _log(f"Error loading config: {e}, using defaults")
            _config = DEFAULT_CONFIG.copy()
    else:
        _config = DEFAULT_CONFIG.copy()
        _save_config()
        _log(f"Default config written to {_config_path}")
        _log("Edit the config file and set 'enabled' to true, then restart Winamp.")


def _save_config():
    """Save the current configuration to disk."""
    try:
        with open(_config_path, "w") as f:
            json.dump(_config, f, indent=4)
    except Exception as e:
        _log(f"Error saving config: {e}")


# ---------------------------------------------------------------------------
# Stream Management (ffmpeg-based)
# ---------------------------------------------------------------------------

class IcecastStream:
    """Manages an ffmpeg subprocess that streams audio to an Icecast mount."""

    def __init__(self, mount_cfg, server_cfg, dj_cfg):
        self.mount = mount_cfg
        self.server = server_cfg
        self.dj = dj_cfg
        self.process = None
        self.current_file = None
        self.reconnect_attempts = 0
        self.lock = threading.Lock()

    @property
    def name(self):
        return self.mount.get("name", "unnamed")

    @property
    def mountpoint(self):
        return self.mount.get("mountpoint", "/stream")

    def _build_icecast_url(self):
        """Build the Icecast URL for ffmpeg output."""
        proto = "https" if self.server.get("tls", False) else "http"
        user = self.server.get("username", "source")
        passwd = self.server.get("password", "hackme")
        host = self.server.get("host", "localhost")
        port = self.server.get("port", 8000)
        mount = self.mountpoint
        if not mount.startswith("/"):
            mount = "/" + mount
        return f"icecast://{user}:{passwd}@{host}:{port}{mount}"

    def _build_ffmpeg_cmd(self, input_file):
        """Build the ffmpeg command for streaming."""
        fmt = self.mount.get("format", "mp3")
        bitrate = self.mount.get("bitrate", 192)
        samplerate = self.mount.get("samplerate", 44100)
        channels = self.mount.get("channels", 2)

        cmd = [
            "ffmpeg",
            "-re",                          # Read input at native frame rate
            "-i", input_file,               # Input file
            "-vn",                          # No video
            "-ar", str(samplerate),         # Sample rate
            "-ac", str(channels),           # Channels
        ]

        if fmt == "mp3":
            cmd += [
                "-codec:a", "libmp3lame",
                "-b:a", f"{bitrate}k",
                "-f", "mp3",
            ]
        elif fmt == "ogg":
            cmd += [
                "-codec:a", "libvorbis",
                "-b:a", f"{bitrate}k",
                "-f", "ogg",
            ]
        elif fmt == "opus":
            cmd += [
                "-codec:a", "libopus",
                "-b:a", f"{bitrate}k",
                "-f", "ogg",
            ]
        elif fmt == "aac":
            cmd += [
                "-codec:a", "aac",
                "-b:a", f"{bitrate}k",
                "-f", "adts",
            ]
        elif fmt == "flac":
            cmd += [
                "-codec:a", "flac",
                "-f", "ogg",
            ]
        else:
            # Default to mp3
            cmd += [
                "-codec:a", "libmp3lame",
                "-b:a", f"{bitrate}k",
                "-f", "mp3",
            ]

        # Icecast content type header
        content_type_map = {
            "mp3": "audio/mpeg",
            "ogg": "audio/ogg",
            "opus": "audio/ogg",
            "aac": "audio/aac",
            "flac": "audio/ogg",
        }
        content_type = content_type_map.get(fmt, "audio/mpeg")

        # Icecast metadata headers
        ice_opts = []
        desc = self.mount.get("description", "")
        genre = self.mount.get("genre", "")
        url = self.mount.get("url", "")
        dj_name = self.dj.get("name", "")
        public = "1" if self.mount.get("public", False) else "0"

        if desc:
            ice_opts.append(f"ice_description={desc}")
        if genre:
            ice_opts.append(f"ice_genre={genre}")
        if url:
            ice_opts.append(f"ice_url={url}")
        if dj_name:
            ice_opts.append(f"ice_name={dj_name}")
        ice_opts.append(f"ice_public={public}")
        ice_opts.append(f"content_type={content_type}")

        icecast_url = self._build_icecast_url()

        cmd += [
            "-ice_name", dj_name or "Winamp DJ",
            "-ice_description", desc or "Winamp DJ Stream",
            "-ice_genre", genre or "Various",
            "-ice_url", url or "",
            "-legacy_icecast", "0",
            icecast_url
        ]

        return cmd

    def start(self, input_file):
        """Start streaming a file."""
        with self.lock:
            self.stop_internal()

            if not input_file or not os.path.isfile(input_file):
                _log(f"[{self.name}] File not found: {input_file}")
                return False

            self.current_file = input_file

            try:
                cmd = self._build_ffmpeg_cmd(input_file)
                _log(f"[{self.name}] Starting stream: {self.mountpoint}")
                _log(f"[{self.name}] File: {os.path.basename(input_file)}")

                self.process = subprocess.Popen(
                    cmd,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.PIPE,
                    stdin=subprocess.DEVNULL,
                    preexec_fn=os.setsid
                )
                self.reconnect_attempts = 0
                return True

            except FileNotFoundError:
                _log(f"[{self.name}] ffmpeg not found! Install with: sudo apt install ffmpeg")
                return False
            except Exception as e:
                _log(f"[{self.name}] Error starting stream: {e}")
                return False

    def stop_internal(self):
        """Stop the ffmpeg process (must hold lock)."""
        if self.process:
            try:
                os.killpg(os.getpgid(self.process.pid), signal.SIGTERM)
                self.process.wait(timeout=3)
            except Exception:
                try:
                    self.process.kill()
                except Exception:
                    pass
            self.process = None

    def stop(self):
        """Stop streaming."""
        with self.lock:
            self.stop_internal()
            _log(f"[{self.name}] Stream stopped")

    def is_alive(self):
        """Check if the ffmpeg process is still running."""
        with self.lock:
            return self.process is not None and self.process.poll() is None

    def update_metadata(self, title):
        """Update Icecast metadata via the admin API."""
        try:
            import urllib.request
            import urllib.parse

            host = self.server.get("host", "localhost")
            port = self.server.get("port", 8000)
            user = self.server.get("username", "source")
            passwd = self.server.get("password", "hackme")
            mount = self.mountpoint
            if not mount.startswith("/"):
                mount = "/" + mount

            proto = "https" if self.server.get("tls", False) else "http"
            encoded_title = urllib.parse.quote(title, safe="")
            url = f"{proto}://{host}:{port}/admin/metadata?mount={mount}&mode=updinfo&song={encoded_title}"

            # Create request with basic auth
            password_mgr = urllib.request.HTTPPasswordMgrWithDefaultRealm()
            password_mgr.add_password(None, url, user, passwd)
            auth_handler = urllib.request.HTTPBasicAuthHandler(password_mgr)
            opener = urllib.request.build_opener(auth_handler)

            req = urllib.request.Request(url)
            response = opener.open(req, timeout=5)
            _log(f"[{self.name}] Metadata updated: {title}")

        except Exception as e:
            _log(f"[{self.name}] Metadata update failed: {e}")


# ---------------------------------------------------------------------------
# Track Monitor Thread
# ---------------------------------------------------------------------------

def _monitor_loop():
    """Background thread that watches for track changes and manages streams."""
    global _running

    last_file = ""
    last_position = -1
    reconnect_cfg = _config.get("reconnect", {})
    meta_cfg = _config.get("metadata", {})

    while _running:
        try:
            if not _api:
                time.sleep(1)
                continue

            current = _api.get_current_file()
            playing = _api.is_playing()

            # Track changed — restart all streams with the new file
            if current and current != last_file and playing:
                last_file = current
                _log(f"Track changed: {os.path.basename(current)}")

                for name, stream in _streams.items():
                    if stream.mount.get("enabled", True):
                        stream.start(current)

                        # Update metadata
                        if meta_cfg.get("update_on_track_change", True):
                            basename = os.path.splitext(os.path.basename(current))[0]
                            title = basename
                            if meta_cfg.get("fallback_to_filename", True):
                                title = basename
                            # Give ffmpeg a moment to connect
                            threading.Timer(2.0, stream.update_metadata, args=[title]).start()

            # Auto-reconnect dead streams
            if playing and reconnect_cfg.get("enabled", True):
                max_attempts = reconnect_cfg.get("max_attempts", 0)
                for name, stream in _streams.items():
                    if stream.mount.get("enabled", True) and stream.current_file:
                        if not stream.is_alive():
                            if max_attempts == 0 or stream.reconnect_attempts < max_attempts:
                                stream.reconnect_attempts += 1
                                delay = reconnect_cfg.get("delay_seconds", 5)
                                _log(f"[{name}] Reconnecting (attempt {stream.reconnect_attempts})...")
                                time.sleep(delay)
                                stream.start(stream.current_file)

        except Exception as e:
            _log(f"Monitor error: {e}")

        time.sleep(1)


# ---------------------------------------------------------------------------
# Public Commands (can be called from other plugins or future UI)
# ---------------------------------------------------------------------------

def start_streaming():
    """Start streaming the current track on all enabled mounts."""
    if not _api:
        _log("API not available")
        return

    current = _api.get_current_file()
    if not current:
        _log("No track playing")
        return

    for name, stream in _streams.items():
        if stream.mount.get("enabled", True):
            stream.start(current)


def stop_streaming():
    """Stop all active streams."""
    for name, stream in _streams.items():
        stream.stop()
    _log("All streams stopped")


def get_status():
    """Return status of all streams."""
    status = {}
    for name, stream in _streams.items():
        status[name] = {
            "alive": stream.is_alive(),
            "mount": stream.mountpoint,
            "file": stream.current_file or "",
            "reconnect_attempts": stream.reconnect_attempts
        }
    return status


def reload_config():
    """Reload configuration from disk."""
    _load_config()
    _init_streams()
    _log("Configuration reloaded")


# ---------------------------------------------------------------------------
# Initialization
# ---------------------------------------------------------------------------

def _init_streams():
    """Initialize stream objects from configuration."""
    global _streams

    # Stop existing streams
    for name, stream in _streams.items():
        stream.stop()
    _streams.clear()

    server_cfg = _config.get("server", {})
    dj_cfg = _config.get("dj", {})

    for mount_cfg in _config.get("mounts", []):
        name = mount_cfg.get("name", f"mount_{len(_streams)}")
        _streams[name] = IcecastStream(mount_cfg, server_cfg, dj_cfg)
        state = "enabled" if mount_cfg.get("enabled", True) else "disabled"
        _log(f"Mount '{name}' ({mount_cfg.get('mountpoint', '/stream')}) — {state}")


def on_winamp_start(winamp_api):
    """Called by Winamp's plugin loader on startup."""
    global _api, _running, _monitor_thread, _log_file

    _api = winamp_api
    _load_config()

    # Setup logging
    log_cfg = _config.get("log", {})
    if log_cfg.get("enabled", True) and log_cfg.get("file"):
        try:
            _log_file = open(os.path.expanduser(log_cfg["file"]), "a")
        except Exception:
            pass

    _log("Icecast DJ plugin loaded")
    _log(f"Config: {_config_path}")

    if not _config.get("enabled", False):
        _log("Streaming is DISABLED. Edit icecast_dj.json to enable.")
        return

    _init_streams()

    # Start monitor thread
    _running = True
    _monitor_thread = threading.Thread(target=_monitor_loop, daemon=True)
    _monitor_thread.start()

    if _config.get("autostart", False):
        _log("Autostart enabled — will begin streaming on first track")
    else:
        _log("Ready. Streaming will begin when a track plays.")


def on_winamp_exit():
    """Called when Winamp is shutting down."""
    global _running, _log_file

    _running = False
    stop_streaming()

    if _log_file:
        _log_file.close()
        _log_file = None

    _log("Icecast DJ plugin unloaded")

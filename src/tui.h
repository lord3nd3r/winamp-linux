// tui.h — Dependency-free terminal (CLI/TUI) front-end for Winamp Linux
//
// A headless front-end that drives the existing WinampWindow engine while it
// runs hidden under the offscreen QPA platform. Renders a classic-Winamp-styled
// terminal UI (title, time, transport state, volume, ASCII spectrum) and maps
// keyboard hotkeys to the window's existing public playback API. No GUI window
// is shown; this is the whole user interface in --tui/--cli mode.
//
// Deliberately dependency-free: raw termios + ANSI escape codes only, so it
// adds no package to the multi-distro build matrix. Composes QTimer (render
// tick) and QSocketNotifier (stdin readable) rather than subclassing QObject,
// so no MOC is required for this class.
#pragma once

#include <QString>
#include <termios.h>
#include <string>

class WinampWindow;
class QTimer;
class QSocketNotifier;

class WinampTui {
public:
    explicit WinampTui(WinampWindow *win);
    ~WinampTui();

    // Enter raw terminal mode, paint the initial frame, and start the render
    // tick + stdin watcher. Safe to call once after construction.
    void start();

    // Restore the terminal to its original state (cooked mode, cursor shown,
    // main screen). Idempotent; also invoked from the destructor.
    void restoreTerminal();

private:
    void onKey();       // stdin readable: read into inbuf + parse
    void parseInput();  // consume complete key/mouse tokens from inbuf
    void render();      // build one ANSI frame and write it to stdout
    void handleByte(unsigned char c);
    // Mouse (xterm SGR): button code, 1-based column/row, press vs release.
    void onMouse(int cb, int cx, int cy, bool press);

    // Transport helpers built on WinampWindow's existing public API.
    void togglePlayPause();
    void startOrResume();  // resume if paused, else (re)start current/first track
    void doStop();
    void doNext();
    void doPrev();
    void nudgeVolume(int delta);

    static void queryTerminalSize(int &rows, int &cols);
    static QString formatTime(qint64 ms);

    WinampWindow *win = nullptr;
    QTimer *renderTimer = nullptr;
    QSocketNotifier *stdinNotifier = nullptr;

    struct termios savedTermios {};
    bool rawModeActive = false;
    bool started = false;

    int scrollPos = 0;   // marquee offset for the title line
    int scrollTick = 0;  // tick divider so the marquee scrolls slower than the frame rate

    std::string inbuf;   // pending stdin bytes (escape/mouse sequences may split across reads)
    int geoTop = 0;      // panel origin (0-based), updated each render() for mouse hit-testing
    int geoLeft = 0;
};

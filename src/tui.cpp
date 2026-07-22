// tui.cpp — implementation of the dependency-free terminal front-end.
// See tui.h for the design rationale.
#include "tui.h"
#include "winamp_window.h"

#include <QTimer>
#include <QSocketNotifier>
#include <QCoreApplication>
#include <QMediaPlayer>
#include <QFileInfo>

#include <unistd.h>
#include <sys/ioctl.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <array>

namespace {
// Classic Winamp green on black, plus a dim variant for chrome/accents.
const char *kGreen    = "\x1b[38;2;0;255;0m";
const char *kDimGreen = "\x1b[38;2;0;140;0m";
const char *kReset    = "\x1b[0m";
const char *kBold     = "\x1b[1m";

// Inner content width of the panel (excluding the two border columns).
constexpr int kContentW = 56;
constexpr int kSpectrumRows = 6;
constexpr int kClockW = 20;   // width reserved for the seven-segment clock column

// Eight vertical block glyphs (1/8 .. 8/8) for sub-cell bar heights.
const char *kBlocks[9] = { " ", "▁", "▂", "▃", "▄",
                           "▅", "▆", "▇", "█" };

// Classic Winamp spectrum-analyzer palette: a fixed vertical gradient, green at
// the bottom rising through yellow to red at the top (indexed bottom->top).
const char *kSpecColor[6] = {
    "\x1b[38;2;0;255;0m",    // green
    "\x1b[38;2;120;255;0m",
    "\x1b[38;2;200;255;0m",  // yellow-green
    "\x1b[38;2;255;220;0m",  // yellow
    "\x1b[38;2;255;150;0m",  // orange
    "\x1b[38;2;255;60;0m",   // red
};

// 3-row seven-segment LCD glyphs (the iconic Winamp time readout), 3 cols each.
struct Seg { const char *r0, *r1, *r2; };
Seg sevenSeg(QChar qc) {
    switch (qc.toLatin1()) {
        case '0': return { " _ ", "| |", "|_|" };
        case '1': return { "   ", "  |", "  |" };
        case '2': return { " _ ", " _|", "|_ " };
        case '3': return { " _ ", " _|", " _|" };
        case '4': return { "   ", "|_|", "  |" };
        case '5': return { " _ ", "|_ ", " _|" };
        case '6': return { " _ ", "|_ ", "|_|" };
        case '7': return { " _ ", "  |", "  |" };
        case '8': return { " _ ", "|_|", "|_|" };
        case '9': return { " _ ", "|_|", " _|" };
        case ':': return { " ",   "▪",   "▪"   };
        default:  return { "   ", "   ", "   " };
    }
}
// Build the three rows of a seven-segment "MM:SS" readout.
std::array<std::string, 3> buildClock(const QString &mmss) {
    std::array<std::string, 3> rows { "", "", "" };
    for (QChar c : mmss) {
        Seg g = sevenSeg(c);
        rows[0] += g.r0; rows[0] += " ";
        rows[1] += g.r1; rows[1] += " ";
        rows[2] += g.r2; rows[2] += " ";
    }
    return rows;
}
} // namespace

WinampTui::WinampTui(WinampWindow *w) : win(w) {}

WinampTui::~WinampTui() {
    restoreTerminal();
}

void WinampTui::start() {
    if (started) return;
    started = true;

    // Enter raw mode: no echo, no canonical line buffering, and crucially no
    // ISIG so Ctrl-C arrives as a 0x03 byte we handle ourselves (see onKey).
    if (tcgetattr(STDIN_FILENO, &savedTermios) == 0) {
        struct termios raw = savedTermios;
        raw.c_lflag &= ~(ICANON | ECHO | ISIG);
        raw.c_iflag &= ~(IXON | ICRNL);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        rawModeActive = true;
    }

    // Alternate screen buffer + hidden cursor, so quitting restores the shell
    // scrollback untouched. Also enable xterm mouse reporting: 1002 = button
    // press/release + drag motion, 1006 = SGR extended coordinates.
    const char *enter = "\x1b[?1049h\x1b[?25l\x1b[2J\x1b[?1002h\x1b[?1006h";
    ssize_t _w = ::write(STDOUT_FILENO, enter, std::char_traits<char>::length(enter));
    (void)_w;

    stdinNotifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, win);
    QObject::connect(stdinNotifier, &QSocketNotifier::activated, win, [this]() { onKey(); });

    renderTimer = new QTimer(win);
    QObject::connect(renderTimer, &QTimer::timeout, win, [this]() { render(); });
    renderTimer->start(50);  // ~20 fps

    render();
}

void WinampTui::restoreTerminal() {
    if (rawModeActive) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &savedTermios);
        rawModeActive = false;
    }
    // Disable mouse reporting, show cursor, leave the alternate screen buffer.
    const char *leave = "\x1b[?1002l\x1b[?1006l\x1b[?25h\x1b[?1049l";
    ssize_t _w = ::write(STDOUT_FILENO, leave, std::char_traits<char>::length(leave));
    (void)_w;
}

void WinampTui::onKey() {
    unsigned char buf[256];
    ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) return;
    inbuf.append(reinterpret_cast<char *>(buf), n);
    if (inbuf.size() > 4096) inbuf.clear();  // safety against a malformed flood
    parseInput();
}

// Consume complete tokens from inbuf. Escape/mouse sequences can be split
// across reads, so anything incomplete is left in inbuf for the next call.
void WinampTui::parseInput() {
    size_t i = 0;
    while (i < inbuf.size()) {
        unsigned char c = inbuf[i];
        if (c != 0x1b) { handleByte(c); i++; continue; }

        // ESC — need at least the next byte to know what it is.
        if (i + 1 >= inbuf.size()) break;      // wait for more
        if (inbuf[i + 1] != '[') { i++; continue; }  // lone ESC: drop it
        if (i + 2 >= inbuf.size()) break;      // wait for more

        char c2 = inbuf[i + 2];
        if (c2 == '<') {
            // SGR mouse: ESC [ < Cb ; Cx ; Cy (M|m)
            size_t j = i + 3;
            while (j < inbuf.size() && inbuf[j] != 'M' && inbuf[j] != 'm') j++;
            if (j >= inbuf.size()) break;      // incomplete: wait for more
            std::string body = inbuf.substr(i + 3, j - (i + 3));
            bool press = (inbuf[j] == 'M');
            int cb = 0, cx = 0, cy = 0;
            if (sscanf(body.c_str(), "%d;%d;%d", &cb, &cx, &cy) == 3)
                onMouse(cb, cx, cy, press);
            i = j + 1;
            continue;
        }
        // CSI arrow keys.
        switch (c2) {
            case 'A': nudgeVolume(+16); break;   // Up
            case 'B': nudgeVolume(-16); break;   // Down
            case 'C': {                          // Right = seek +5s
                QMediaPlayer *p = win->getPlayer();
                if (p->duration() > 0)
                    p->setPosition(qMin(p->duration(), p->position() + 5000));
                break;
            }
            case 'D': {                          // Left = seek -5s
                QMediaPlayer *p = win->getPlayer();
                if (p->duration() > 0)
                    p->setPosition(qMax((qint64)0, p->position() - 5000));
                break;
            }
            default: break;
        }
        i += 3;
    }
    inbuf.erase(0, i);
}

void WinampTui::onMouse(int cb, int cx, int cy, bool press) {
    // Scroll wheel adjusts volume anywhere on screen.
    if (cb == 64) { nudgeVolume(+16); return; }
    if (cb == 65) { nudgeVolume(-16); return; }

    // Only left button (base 0), press or drag-motion (motion sets bit 32, base
    // stays 0). Ignore releases and other buttons.
    if ((cb & 0x03) != 0 || !press) return;

    // Convert to panel-relative row and content-index column. Content char j
    // sits at absolute 1-based column geoLeft + 2 + j (see render()/borderRaw).
    int r = cy - geoTop - 1;
    int j = cx - geoLeft - 2;

    const int transportR = 5;
    const int seekR = 6 + kSpectrumRows;
    const int volR  = seekR + 1;

    if (r == transportR) {
        int t = j - 21;              // transport string starts at content-index 21
        if      (t >= 0  && t <= 3)  doPrev();
        else if (t >= 4  && t <= 6)  startOrResume();
        else if (t >= 7  && t <= 9)  togglePlayPause();
        else if (t >= 10 && t <= 12) doStop();
        else if (t >= 13 && t <= 16) doNext();
    } else if (r == seekR) {
        int s = j - 5;               // seek bar starts at content-index 5
        int seekW = kContentW - 6;
        if (s >= 0 && s <= seekW) {
            QMediaPlayer *p = win->getPlayer();
            if (p->duration() > 0)
                p->setPosition((qint64)((double)s / seekW * p->duration()));
        }
    } else if (r == volR) {
        int v = j - 5;               // volume bar starts at content-index 5
        const int volW = 14;
        if (v >= 0 && v <= volW)
            win->setPluginVolume((int)std::lround((double)v / volW * 255.0));
    }
}

void WinampTui::handleByte(unsigned char c) {
    switch (c) {
        case 'q': case 'Q': case 0x03:  // q / Ctrl-C
            restoreTerminal();
            QCoreApplication::quit();
            return;
        case ' ': togglePlayPause(); break;
        case 'z': case 'Z': doPrev(); break;
        case 'x': case 'X': startOrResume(); break;
        case 'c': case 'C': togglePlayPause(); break;
        case 'v': case 'V': doStop(); break;
        case 'b': case 'B': doNext(); break;
        case '+': case '=': nudgeVolume(+16); break;
        case '-': case '_': nudgeVolume(-16); break;
        default: break;
    }
}

void WinampTui::togglePlayPause() {
    QMediaPlayer *p = win->getPlayer();
    switch (p->playbackState()) {
        case QMediaPlayer::PlayingState: p->pause(); break;
        case QMediaPlayer::PausedState:  p->play();  break;
        default: {  // Stopped: start current or first track
            PlaylistWindow *pl = win->getPlaylistWindow();
            int idx = pl->currentTrackIndex();
            if (idx < 0 && pl->trackCount() > 0) idx = 0;
            if (idx >= 0) {
                pl->setCurrentTrackIndex(idx);
                win->playTrack(pl->trackAt(idx));
            }
            break;
        }
    }
}

void WinampTui::startOrResume() {
    QMediaPlayer *p = win->getPlayer();
    if (p->playbackState() == QMediaPlayer::PausedState) {
        p->play();
        return;
    }
    PlaylistWindow *pl = win->getPlaylistWindow();
    int idx = pl->currentTrackIndex();
    if (idx < 0 && pl->trackCount() > 0) idx = 0;
    if (idx >= 0) {
        pl->setCurrentTrackIndex(idx);
        win->playTrack(pl->trackAt(idx));
    }
}

void WinampTui::doStop() { win->getPlayer()->stop(); }
void WinampTui::doNext() { win->getPlaylistWindow()->nextTrack(); }
void WinampTui::doPrev() { win->getPlaylistWindow()->prevTrack(); }

void WinampTui::nudgeVolume(int delta) {
    win->setPluginVolume(win->getPluginVolume() + delta);
}

void WinampTui::queryTerminalSize(int &rows, int &cols) {
    struct winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        rows = ws.ws_row;
        cols = ws.ws_col;
    } else {
        rows = 24;
        cols = 80;
    }
}

QString WinampTui::formatTime(qint64 ms) {
    if (ms < 0) ms = 0;
    qint64 total = ms / 1000;
    return QString("%1:%2").arg(total / 60, 2, 10, QChar('0'))
                           .arg(total % 60, 2, 10, QChar('0'));
}

void WinampTui::render() {
    int rows = 0, cols = 0;
    queryTerminalSize(rows, cols);

    const int panelW = kContentW + 2;                 // + borders
    // top + header + sep + 3 clock/info rows + spectrum + seek + vol/bal +
    // bottom + legend
    const int panelH = kSpectrumRows + 10;
    const int left = qMax(0, (cols - panelW) / 2);
    const int top  = qMax(0, (rows - panelH) / 2);
    geoTop = top;    // remembered for mouse hit-testing in onMouse()
    geoLeft = left;

    QMediaPlayer *p = win->getPlayer();
    const auto state = p->playbackState();
    const bool playing = state == QMediaPlayer::PlayingState;
    const bool paused  = state == QMediaPlayer::PausedState;

    // --- Title marquee (right of the clock column) ---
    QString title = win->tuiDisplayTitle();
    if (title.isEmpty()) title = "— no track —";
    const int titleW = kContentW - kClockW - 2;
    QString titleLine;
    if (title.size() <= titleW) {
        titleLine = title.leftJustified(titleW, ' ');
    } else {
        QString padded = title + "    •    ";
        int off = scrollPos % padded.size();
        QString rolled = padded.mid(off) + padded.left(off);
        titleLine = rolled.left(titleW);
        if ((++scrollTick % 3) == 0) scrollPos++;  // slow the marquee down
    }

    // --- Spectrum (downsample 75 FFT bands -> content columns) ---
    // Full inner width so the analyzer spans border-to-border like Winamp's.
    const float *spec = win->tuiSpectrum();
    const int cols_ = kContentW;
    static std::array<float, 256> smooth {};  // per-column decay, single instance
    std::array<int, 256> level {};            // eighths of a cell, 0..(rows*8)
    const int maxEighths = kSpectrumRows * 8;
    for (int c = 0; c < cols_ && c < 256; c++) {
        int b0 = c * 75 / cols_;
        int b1 = qMax(b0 + 1, (c + 1) * 75 / cols_);
        float v = 0.f;
        for (int b = b0; b < b1 && b < 75; b++) v = qMax(v, spec[b]);
        v = qMin(1.0f, v * 1.15f);
        // Attack instantly, decay smoothly, so bars rise fast and fall gently.
        smooth[c] = (v > smooth[c]) ? v : smooth[c] * 0.82f;
        level[c] = (int)std::lround(smooth[c] * maxEighths);
    }

    // --- Compose the frame ---
    std::string out;
    out.reserve(4096);
    // Only clear the whole screen when the terminal is resized; otherwise the
    // panel overdraws its own fixed area each frame, which avoids the flicker a
    // per-frame \x1b[2J would cause at the render frame rate.
    static int lastRows = -1, lastCols = -1;
    if (rows != lastRows || cols != lastCols) {
        out += "\x1b[2J";
        lastRows = rows;
        lastCols = cols;
    }
    out += "\x1b[H";  // home

    auto moveTo = [&](int r, int c) {
        out += "\x1b[" + std::to_string(top + r + 1) + ";" +
               std::to_string(left + c + 1) + "H";
    };
    auto line = [&](int r, const QString &content, const char *color = kGreen) {
        moveTo(r, 0);
        out += color;
        out += content.toStdString();
        out += kReset;
    };

    const QString hbar = QString("═").repeated(kContentW);
    int r = 0;

    // Helper: emit one bordered inner row given a fully-styled inner body whose
    // *visible* width is exactly kContentW (caller is responsible for padding).
    auto borderRaw = [&](const std::string &styledInner) {
        moveTo(r, 0);
        out += kDimGreen; out += "║"; out += kReset;
        out += styledInner;
        out += kDimGreen; out += "║"; out += kReset;
        r++;
    };
    // Helper: a plain single-color inner row from a QString (padded to width).
    auto bordered = [&](const QString &inner, const char *color = kGreen) {
        std::string s = color;
        s += (" " + inner).leftJustified(kContentW, ' ').left(kContentW).toStdString();
        s += kReset;
        borderRaw(s);
    };

    // --- Titlebar with WINAMP wordmark + transport state ---
    QString stateStr = playing ? "▶ PLAYING" : paused ? "‖ PAUSED" : "■ STOPPED";
    line(r++, "╔" + hbar + "╗", kDimGreen);
    {
        QString wm = "  W I N A M P";
        QString right = stateStr + "  ";
        QString mid = wm.leftJustified(kContentW - right.size(), ' ') + right;
        std::string s = kGreen; s += kBold;
        s += mid.left(kContentW).leftJustified(kContentW, ' ').toStdString();
        s += kReset;
        borderRaw(s);
    }
    line(r++, "╟" + QString("─").repeated(kContentW) + "╢", kDimGreen);

    // --- Three rows: seven-segment clock | title / info / transport+time ---
    qint64 pos = p->position(), dur = p->duration();
    auto clock = buildClock(formatTime(pos));

    int br = win->tuiBitrateKbps();
    int kh = win->tuiSampleRateKHz();
    int ch = win->tuiChannels();
    QString infoLine = QString("%1 kbps   %2 kHz   %3")
                           .arg(br > 0 ? QString::number(br) : "--")
                           .arg(kh > 0 ? QString::number(kh) : "--")
                           .arg(ch >= 2 ? "stereo" : ch == 1 ? "mono" : "--");
    QString timeText = dur > 0 ? (formatTime(pos) + " / " + formatTime(dur))
                               : (formatTime(pos) + " / --:--");
    QString transport = QString("%1  %2  %3  %4  %5")
                            .arg("|◄")
                            .arg(playing ? "▶" : "▷")
                            .arg(paused ? "‖" : "ǁ")
                            .arg("■")
                            .arg("►|");
    const QString rightText[3] = {
        titleLine,
        infoLine,
        transport.leftJustified(22, ' ') + timeText
    };
    for (int k = 0; k < 3; k++) {
        QString cl = QString::fromStdString(clock[k]).leftJustified(kClockW, ' ');
        std::string s = kGreen; s += kBold;
        s += cl.left(kClockW).toStdString();
        s += kReset; s += kGreen;
        QString rt = (" " + rightText[k]).leftJustified(kContentW - kClockW, ' ');
        s += rt.left(kContentW - kClockW).toStdString();
        s += kReset;
        borderRaw(s);
    }

    // --- Colored spectrum analyzer (green->yellow->red vertical gradient) ---
    for (int sr = kSpectrumRows - 1; sr >= 0; sr--) {
        std::string s;
        int palIdx = (sr * 6) / kSpectrumRows;   // 0..5 bottom..top
        s += kSpecColor[qBound(0, palIdx, 5)];
        for (int c = 0; c < cols_; c++) {
            int e = level[c] - sr * 8;
            if (e >= 8)      s += kBlocks[8];
            else if (e <= 0) s += " ";
            else             s += kBlocks[e];
        }
        s += kReset;
        borderRaw(s);
    }

    // --- Seek bar ---
    {
        const int seekW = kContentW - 6;
        QString sb;
        if (dur > 0) {
            int filled = (int)std::lround((double)pos / dur * seekW);
            for (int i = 0; i < seekW; i++) sb += (i < filled) ? "█" : "─";
        } else {
            for (int i = 0; i < seekW; i++) sb += "─";
        }
        bordered(QString("POS ") + sb, kDimGreen);
    }

    // --- Volume + balance ---
    {
        int vol = win->getPluginVolume();
        const int volW = 14;
        int volFilled = (int)std::lround((double)vol / 255.0 * volW);
        QString volBar;
        for (int i = 0; i < volW; i++) volBar += (i < volFilled) ? "▓" : "░";
        // Balance is centered; show a simple centered marker (engine balance is
        // not exposed to the TUI yet, so this reflects center for now).
        QString balBar = "░░░░░▓░░░░░";
        bordered(QString("VOL ") + volBar + QString("  %1  BAL ").arg(vol, 3) + balBar);
    }

    line(r++, "╚" + hbar + "╝", kDimGreen);
    line(r++, QString::fromUtf8("  z prev  x play  c pause  v stop  b next  ←/→ seek  +/- vol  q quit"),
         kDimGreen);

    ssize_t _w = ::write(STDOUT_FILENO, out.data(), out.size());
    (void)_w;
}

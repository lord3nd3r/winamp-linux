#!/usr/bin/env python3
"""
Modular split for winamp_authentic.cpp.
Replaces extracted code sections with #include directives.
Processes replacements in reverse line order so indices stay valid.
"""

def read_file(path):
    with open(path, 'r') as f:
        return f.readlines()

def write_file(path, lines):
    with open(path, 'w') as f:
        f.writelines(lines)

lines = read_file('winamp_authentic.cpp')

# Each replacement: (start_1based, end_1based, replacement_lines)
# Process in REVERSE order to keep line numbers stable.
replacements = []

# --- Block 1: Qt5/Qt6 compat shims (lines 85-192) ---
# starts: #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
# ends:   #endif  (line 192)
replacements.append((85, 192, [
    '// Qt5/Qt6 compatibility layer\n',
    '#include "src/compat.h"\n',
]))

# --- Block 2: EQ DSP engine (lines 207-373) ---
# starts: // ========================================================================
# ends:   // End EQ10 DSP Engine + blank line (373)
replacements.append((207, 373, [
    '// EQ10 DSP engine (10-band graphic equalizer)\n',
    '#include "src/eq_dsp.h"\n',
]))

# --- Block 3: extractSkinArchive + SkinPlaylistColors + parsePleditTxt + g_plColors (lines 375-467) ---
replacements.append((375, 467, [
    '// Skin helpers, playlist colors, archive extraction\n',
    '#include "src/constants.h"\n',
    '\n',
    '// Global skin playlist colors (loaded when skin changes)\n',
    'static SkinPlaylistColors g_plColors;\n',
]))

# --- Block 4: globals + helpers that are now in constants.h (lines 2603-2685) ---
# isModernSkinDir, g_isModernSkin, g_modernSkin, kWinampMenuStyle,
# kAudioFileFilter, configPath, isRemoteMediaLocation, playlistEntryLabel,
# playlistSortLabel, winampDataRoots, winampSkinAndResourcePaths
# Need to find exact end. Let's check line 2685.
replacements.append((2603, 2685, [
    '// Detect modern skin (now in constants.h: isModernSkinDir)\n',
    '\n',
    '// Global modern skin state (accessible to playlist/EQ windows before WinampWindow is fully defined)\n',
    'static bool g_isModernSkin = false;\n',
    'static ModernSkinEngine *g_modernSkin = nullptr;\n',
]))

# Sort by start line DESCENDING
replacements.sort(key=lambda r: r[0], reverse=True)

for start, end, new_lines in replacements:
    # Convert to 0-based indices
    s = start - 1
    e = end  # end is 1-based inclusive, so lines[s:e] covers start..end
    print(f"Replacing lines {start}-{end} ({e-s} lines) with {len(new_lines)} lines")
    print(f"  First removed: {lines[s].rstrip()}")
    print(f"  Last removed:  {lines[e-1].rstrip()}")
    lines[s:e] = new_lines

write_file('winamp_authentic.cpp', lines)
print(f"\nDone. New file has {len(lines)} lines.")

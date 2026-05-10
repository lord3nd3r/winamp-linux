#!/usr/bin/env python3

def extract():
    with open('winamp_authentic.cpp', 'r') as f:
        lines = f.readlines()
        
    start = -1
    end = -1
    for i, line in enumerate(lines):
        if 'class PlaylistWindow :' in line:
            start = i
        if start != -1 and line == '};\n' and i > start:
            end = i
            break
            
    if start == -1 or end == -1:
        print("Could not find PlaylistWindow boundaries.")
        return
        
    print(f"PlaylistWindow: {start+1} to {end+1}")
        
    out = [
        '// playlist.h — Winamp Playlist Editor\n',
        '#pragma once\n\n',
        '#include <QWidget>\n',
        '#include <QPainter>\n',
        '#include <QMouseEvent>\n',
        '#include <QMenu>\n',
        '#include <QFileDialog>\n',
        '#include <QDropEvent>\n',
        '#include <QMimeData>\n',
        '#include <QUrl>\n',
        '#include <QScrollBar>\n',
        '#include <QApplication>\n',
        '#include <QClipboard>\n',
        '#include <QMessageBox>\n',
        '#include <QInputDialog>\n',
        '#include <QTimer>\n',
        '#include <QMediaMetaData>\n',
        '#include <QMediaPlayer>\n',
        '#include "constants.h"\n',
        '#include "translator.h"\n',
        '#include "bookmark_manager.h"\n',
        '#include "dialogs.h"\n',
        '\n'
    ]
    
    out.extend(lines[start:end+1])
    out.append('\n')
    
    with open('src/playlist.h', 'w') as f:
        f.writelines(out)
        
    lines[start:end+1] = ['#include "src/playlist.h"\n']
    
    with open('winamp_authentic.cpp', 'w') as f:
        f.writelines(lines)
        
    print(f"Extracted playlist.h. New winamp_authentic.cpp length: {len(lines)}")

extract()

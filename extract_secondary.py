#!/usr/bin/env python3
import os

def extract():
    with open('winamp_authentic.cpp', 'r') as f:
        lines = f.readlines()
        
    windows = [
        ('VideoWindow', 'src/video.h', [
            '// video.h — Winamp Video Player Window\n',
            '#pragma once\n\n',
            '#include <QWidget>\n',
            '#include <QPainter>\n',
            '#include <QMouseEvent>\n',
            '#include <QMenu>\n',
            '#include <QVideoWidget>\n',
            '#include "constants.h"\n',
            '\n'
        ]),
        ('MilkdropWindow', 'src/milkdrop.h', [
            '// milkdrop.h — Milkdrop Visualization Window (via projectM)\n',
            '#pragma once\n\n',
            '#include <QWidget>\n',
            '#include <QPainter>\n',
            '#include <QMouseEvent>\n',
            '#include <QMenu>\n',
            '#include <QTimer>\n',
            '#include <QOpenGLWidget>\n',
            '#include <libprojectM/projectM.hpp>\n',
            '#include "constants.h"\n',
            '\n'
        ]),
        ('MediaLibraryWindow', 'src/media_library.h', [
            '// media_library.h — Winamp Media Library\n',
            '#pragma once\n\n',
            '#include <QWidget>\n',
            '#include <QPainter>\n',
            '#include <QMouseEvent>\n',
            '#include <QMenu>\n',
            '#include <QTreeWidget>\n',
            '#include <QTableWidget>\n',
            '#include <QSplitter>\n',
            '#include <QVBoxLayout>\n',
            '#include <QHBoxLayout>\n',
            '#include <QLabel>\n',
            '#include <QHeaderView>\n',
            '#include "constants.h"\n',
            '\n'
        ])
    ]
    
    ranges = []
    
    for cls, h_file, header_lines in windows:
        start = -1
        end = -1
        for i, line in enumerate(lines):
            if f'class {cls} :' in line:
                start = i
            if start != -1 and line == '};\n' and i > start:
                end = i
                break
                
        if start == -1 or end == -1:
            print(f"Could not find {cls} boundaries.")
            continue
            
        ranges.append((start, end, h_file, header_lines))
        print(f"{cls}: {start+1} to {end+1}")
        
    for s, e, h_file, header_lines in ranges:
        out = header_lines[:]
        out.extend(lines[s:e+1])
        out.append('\n')
        with open(h_file, 'w') as f:
            f.writelines(out)
            
    ranges.sort(key=lambda x: x[0], reverse=True)
    first_replace = True
    for s, e, h_file, _ in ranges:
        if first_replace:
            # We want to replace each with its include, but doing it safely
            # Since ranges are sorted reverse, we can just replace each block
            # with its include.
            pass
        lines[s:e+1] = [f'#include "{h_file}"\n']

    with open('winamp_authentic.cpp', 'w') as f:
        f.writelines(lines)
        
    print(f"Extraction complete. New winamp_authentic.cpp length: {len(lines)}")

extract()

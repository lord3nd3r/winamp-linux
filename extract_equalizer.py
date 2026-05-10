#!/usr/bin/env python3

def extract():
    with open('winamp_authentic.cpp', 'r') as f:
        lines = f.readlines()
        
    start = -1
    end = -1
    for i, line in enumerate(lines):
        if 'class EqualizerWindow :' in line:
            start = i
        if start != -1 and line == '};\n' and i > start:
            end = i
            break
            
    if start == -1 or end == -1:
        print("Could not find EqualizerWindow boundaries.")
        return
        
    print(f"EqualizerWindow: {start+1} to {end+1}")
        
    out = [
        '// equalizer.h — Winamp Equalizer\n',
        '#pragma once\n\n',
        '#include <QWidget>\n',
        '#include <QPainter>\n',
        '#include <QMouseEvent>\n',
        '#include <QMenu>\n',
        '#include "constants.h"\n',
        '\n'
    ]
    
    out.extend(lines[start:end+1])
    out.append('\n')
    
    with open('src/equalizer.h', 'w') as f:
        f.writelines(out)
        
    lines[start:end+1] = ['#include "src/equalizer.h"\n']
    
    with open('winamp_authentic.cpp', 'w') as f:
        f.writelines(lines)
        
    print(f"Extracted equalizer.h. New winamp_authentic.cpp length: {len(lines)}")

extract()

#!/usr/bin/env python3

def extract():
    with open('winamp_authentic.cpp', 'r') as f:
        lines = f.readlines()
        
    start = 1033 # 0-indexed line 1034 (// ============================================================)
    end = 1358   # 0-indexed line 1359 (};)
    
    out = [
        '// modern_skin.h — Winamp 5 XML-based skin support\n',
        '#pragma once\n\n',
        '#include <QString>\n',
        '#include <QMap>\n',
        '#include <QImage>\n',
        '#include <QPixmap>\n',
        '#include <QXmlStreamReader>\n',
        '#include <QFile>\n',
        '#include <QDir>\n',
        '#include <QDebug>\n',
        '#include "constants.h"\n',
        '\n'
    ]
    
    out.extend(lines[start:end+1])
    
    with open('src/modern_skin.h', 'w') as f:
        f.writelines(out)
        
    lines[start:end+1] = ['#include "src/modern_skin.h"\n']
    
    with open('winamp_authentic.cpp', 'w') as f:
        f.writelines(lines)
        
    print(f"Extracted modern_skin.h. New winamp_authentic.cpp length: {len(lines)}")

extract()

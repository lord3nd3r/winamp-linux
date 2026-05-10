#!/usr/bin/env python3

def extract():
    with open('winamp_authentic.cpp', 'r') as f:
        lines = f.readlines()
        
    targets = [
        (280, 952, 'src/preferences.h', [
            '// preferences.h — Winamp Preferences Dialog\n',
            '#pragma once\n\n',
            '#include <QDialog>\n',
            '#include <QVBoxLayout>\n',
            '#include <QHBoxLayout>\n',
            '#include <QLabel>\n',
            '#include <QListWidget>\n',
            '#include <QPushButton>\n',
            '#include <QTabWidget>\n',
            '#include <QFormLayout>\n',
            '#include <QGroupBox>\n',
            '#include <QCheckBox>\n',
            '#include <QComboBox>\n',
            '#include <QSlider>\n',
            '#include <QDir>\n',
            '#include <QSettings>\n',
            '#include <QColorDialog>\n',
            '#include <QFileDialog>\n',
            '#include "constants.h"\n',
            '#include "translator.h"\n',
            '\n'
        ]),
        (2554, 2583, 'src/mpris2_adaptors.h', [
            '// mpris2_adaptors.h — DBus MPRIS2 Integration\n',
            '#pragma once\n\n',
            '#include <QObject>\n',
            '#include <QStringList>\n',
            '#include <QMap>\n',
            '#include <QVariant>\n',
            '#if defined(QT_DBUS_LIB)\n',
            '#include <QDBusAbstractAdaptor>\n',
            '#include <QDBusObjectPath>\n',
            '#endif\n',
            '\n'
        ]),
        (2585, 2702, 'src/mpris2_adaptors.h', [])
    ]
    
    # write headers
    with open('src/preferences.h', 'w') as f:
        f.writelines(targets[0][3])
        f.writelines(lines[targets[0][0]:targets[0][1]+1])
        f.write('\n')
        
    with open('src/mpris2_adaptors.h', 'w') as f:
        f.writelines(targets[1][3])
        f.writelines(lines[targets[1][0]:targets[1][1]+1])
        f.write('\n')
        f.writelines(lines[targets[2][0]:targets[2][1]+1])
        f.write('\n')
        
    # replace in reverse order
    targets.sort(key=lambda x: x[0], reverse=True)
    
    wrote_mpris = False
    for s, e, h_file, _ in targets:
        if 'mpris' in h_file:
            if not wrote_mpris:
                lines[s:e+1] = [f'#include "{h_file}"\n']
                wrote_mpris = True
            else:
                lines[s:e+1] = []
        else:
            lines[s:e+1] = [f'#include "{h_file}"\n']
            
    with open('winamp_authentic.cpp', 'w') as f:
        f.writelines(lines)

extract()

#!/usr/bin/env python3
def replace():
    with open('winamp_authentic.cpp', 'r') as f:
        lines = f.readlines()
    
    # 516 to 663 (inclusive is 515 to 663 in 0-based list)
    lines[515:663] = ['#include "src/translator.h"\n']
    
    with open('winamp_authentic.cpp', 'w') as f:
        f.writelines(lines)

replace()

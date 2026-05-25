#!/usr/bin/env python
# -*- coding: utf-8 -*-

f = r'D:\Development\IEGPerTest\external\IEG_Code\code\WLServerTest\WLServerTestDlg.cpp'
with open(f, 'r', encoding='utf-8') as fp:
    lines = fp.readlines()

print('L1500-1510:')
for i in range(1499, min(len(lines), 1510)):
    print(f'{i+1}: {lines[i].rstrip()[:100]}')

# 检查 L509
print('\nL505-515:')
for i in range(504, min(len(lines), 515)):
    print(f'{i+1}: {lines[i].rstrip()[:100]}')

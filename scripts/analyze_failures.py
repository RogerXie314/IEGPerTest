#!/usr/bin/env python3
"""分析 SimulatorApp 失败日志"""
import re, sys, os
from collections import Counter

logdir = r'C:\Users\admin\Pictures\SimulatorAppPublish\logs'

# 失败日志
fail_file = os.path.join(logdir, 'logsend-failures-https-20260301.log')
lines = open(fail_file, 'rb').read().decode('utf-8').splitlines()
total = len(lines)

port  = sum(1 for l in lines if '只允许使用一次' in l)
ssl   = sum(1 for l in lines if 'SSL' in l or 'TLS' in l)
rst   = sum(1 for l in lines if '强迫关闭' in l or 'forcibly closed' in l.lower())
abort = sum(1 for l in lines if 'TaskCanceled' in l or 'timed out' in l.lower())
other = total - port - ssl - rst - abort

print('=== 失败日志分析 ===')
print(f'文件: {os.path.basename(fail_file)}')
print(f'总失败行: {total}')
print(f'  端口耗尽 WSAEADDRINUSE (套接字地址只允许一次): {port}  ({port*100//total if total else 0}%)')
print(f'  SSL/TLS握手失败:                               {ssl}')
print(f'  远端RST强制关闭:                               {rst}')
print(f'  超时/取消 (TaskCanceled):                      {abort}')
print(f'  其他未分类:                                    {other}')

print()
print('--- 按分钟统计失败数 ---')
mins = []
for l in lines:
    m = re.match(r'\d{4}-\d{2}-\d{2}T(\d+:\d+)', l)
    if m: mins.append(m.group(1))
for k, v in sorted(Counter(mins).items()):
    print(f'  {k}: {v} 次')

# 成功日志
print()
ok_file = os.path.join(logdir, 'logsend-https-20260301.log')
if os.path.exists(ok_file):
    slines = open(ok_file, 'rb').read().decode('utf-8').splitlines()
    ok = sum(1 for l in slines if ' OK ' in l or 'status=200' in l or 'status=2' in l)
    print('=== 成功日志 (logsend-https) ===')
    print(f'总行数: {len(slines)}')
    print(f'前5行:')
    for l in slines[:5]:
        print(f'  {l[:160]}')

# logsend (non-https)
print()
gen_fail = os.path.join(logdir, 'logsend-failures-20260301.log')
if os.path.exists(gen_fail):
    glines = open(gen_fail, 'rb').read().decode('utf-8').splitlines()
    print('=== logsend-failures (非HTTPS) ===')
    print(f'总行数: {len(glines)}')
    for l in glines[:5]:
        print(f'  {l[:160]}')

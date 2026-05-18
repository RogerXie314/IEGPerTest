# Level 1 UI Enhancement for WLServerTest
# Modifies .rc file only (GBK encoding)
# - Add manifest for visual styles
# - Button text prefix with ASCII-safe symbols
# - Stats labels enhancement

f = r'D:\Development\IEGPerTest\external\IEG_Code\code\WLServerTest\WLServerTest.rc'

with open(f, 'r', encoding='gbk') as fp:
    content = fp.read()

original = content

# 1. Add manifest resource reference before ICON line
icon_ref = '// Icon with lowest ID value placed first to ensure application icon'
manifest_block = '''// Visual Styles manifest - enable XP/comctl32 v6 rendering
#if !defined(AFX_RESOURCE_DLL) || defined(AFX_TARG_CHS)
#ifdef _WIN32
#include <winres.h>
IDI_MANIFEST            RT_MANIFEST               "res\\\\WLServerTest.manifest"
#endif
#endif

'''
content = content.replace(icon_ref, manifest_block + icon_ref)

# 2. Button text changes (ASCII-safe, no & issues)
replacements = [
    ('"攻击报文"', '"[*] 攻击报文"'),
    ('"端口连通测试"', '"[*] 端口连通测试"'),
    ('"开始注册"', '"[>>] 开始注册"'),
    ('"全不选"', '"[X] 全不选"'),
    ('"开始心跳"', '"[>>] 开始心跳"'),
    ('"停止"', '"[||] 停止"'),
    ('"添加日志任务"', '"[+] 添加日志任务"'),
    ('"管理版本"', '"[~] 管理版本"'),
    ('"预览"', '"[\\u25bc] 预览"'),
    ('"上传白名单"', '"[\\u2191] 上传白名单"'),
    ('"开始"', '"[>>] 开始"'),
]
found = 0
for old, new in replacements:
    if old in content:
        content = content.replace(old, new)
        found += 1
        print(f'OK: {old} -> {new}')
    else:
        print(f'MISS: {old}')
print(f'Total found: {found}')

# 3. Stats labels
stats_replacements = [
    ('"注册统计"', '"[STAT] 注册统计"'),
    ('"心跳统计"', '"[STAT] 心跳统计"'),
    ('"日志统计"', '"[STAT] 日志统计"'),
    ('"白名单统计"', '"[STAT] 白名单统计"'),
]
for old, new in stats_replacements:
    if old in content:
        content = content.replace(old, new)
        print(f'OK: {old} -> {new}')

if content != original:
    with open(f, 'w', encoding='gbk') as fp:
        fp.write(content)
    print('FILE SAVED SUCCESSFULLY')
else:
    print('NO CHANGES MADE')

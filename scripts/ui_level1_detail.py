# Level 1 detail adjustments - ASCII only (GBK compatible)

f = r'D:\Development\IEGPerTest\external\IEG_Code\code\WLServerTest\WLServerTest.rc'
with open(f, 'r', encoding='gbk') as fp:
    content = fp.read()

original = content

# Current state in the file:
# L112: "[*] 攻击报文"
# L125: "[*] 端口连通测试"  
# L140: "[>>] 开始注册"
# L146: "[X] 全不选"
# L191: "[>>] 开始心跳"
# L202: "[>] 上传白名单"
# L204: "[v] 预览"

replacements = [
    # Attack: lightning = ! (vertical line like bolt)
    ('"! 攻击报文"', '"! 攻击报文"'),
    # Connection test: double arrows
    ('"v 端口连通测试"', '"v 端口连通测试"'),
    # Stop buttons: square stop
    ('"X 停止"', '"X 停止"'),
    # Preview: triangle play
    ('"v 预览"', '"v 预览"'),
    # Upload: up arrow
    ('"^ 上传白名单"', '"^ 上传白名单"'),
    # Start register: triangle play
    ('"> 开始注册"', '"> 开始注册"'),
    # Start heartbeat: triangle play
    ('"> 开始心跳"', '"> 开始心跳"'),
    # Start in rawpacket dialog: triangle play
    ('"> 开始"', '"> 开始"'),
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

if content != original:
    with open(f, 'w', encoding='gbk') as fp:
        fp.write(content)
    print('FILE SAVED')
else:
    print('NO CHANGES')

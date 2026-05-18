# Level 1 detail adjustments - line-based (GBK compatible)

f = r'D:\Development\IEGPerTest\external\IEG_Code\code\WLServerTest\WLServerTest.rc'
with open(f, 'r', encoding='gbk') as fp:
    lines = fp.readlines()

# Changes by line number (0-indexed):
changes = {
    111: '    CONTROL         "[!] 攻击报文",IDC_BUTTON_RAWPACKET,"Button",0x50010000,281,4,80,14\n',
    124: '    CONTROL         "[<><>] 端口连通测试",IDC_Btn_TestConn,"Button",0x50010000,13,75,80,14\n',
    201: '    CONTROL         "[^^] 上传白名单",IDC_BUTTON_WL_UPLOAD,"Button",0x50010000,288,232,64,14\n',
}

for lineno, new_line in changes.items():
    old = lines[lineno].strip()
    new = new_line.strip()
    print(f'L{lineno+1}:')
    print(f'  OLD: {repr(old)}')
    print(f'  NEW: {repr(new)}')
    lines[lineno] = new_line

with open(f, 'w', encoding='gbk') as fp:
    fp.writelines(lines)
print('DONE')

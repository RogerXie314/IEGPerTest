f = r'D:\Development\IEGPerTest\external\IEG_Code\code\WLServerTest\WLServerTest.rc'
with open(f, 'r', encoding='gbk') as fp:
    content = fp.read()

replacements = [
    ('"添加任务"', '"[+] 添加任务"'),
    ('"预  览"', '"[v] 预览"'),
    ('"立即上传全量"', '"[>] 上传白名单"'),
]
for old, new in replacements:
    if old in content:
        content = content.replace(old, new)
        print(f'OK: {old} -> {new}')
    else:
        print(f'MISS: {old}')

with open(f, 'w', encoding='gbk') as fp:
    fp.write(content)
print('SAVED')

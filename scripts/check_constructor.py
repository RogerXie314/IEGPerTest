f = r'D:\Development\IEGPerTest\external\IEG_Code\code\WLServerTest\WLServerTestDlg.cpp'
with open(f, 'rb') as fp:
    data = fp.read()

# 找到构造函数行
idx = data.find(b'CWLServerTestDlg::CWLServerTestDlg')
line_start = data.rfind(b'\n', 0, idx) + 1
line_end = data.find(b'\n', idx) + 1
line = data[line_start:line_end]
print(f'构造函数行: {repr(line[:250])}')

# 查找重复的初始化项
count = data.count(b'm_nHoverBtnID(0), m_bMouseInDlg(false)')
print(f'重复初始化项出现次数: {count}')

# 找到所有出现位置
pos = 0
while True:
    idx = data.find(b'm_nHoverBtnID(0), m_bMouseInDlg(false)', pos)
    if idx == -1:
        break
    # 查找行号
    line_num = data[:idx].count(b'\n') + 1
    print(f'位置 {idx}, 行号 {line_num}')
    # 显示上下文
    context_start = max(0, idx - 100)
    context_end = min(len(data), idx + 100)
    print(f'上下文: {repr(data[context_start:context_end][:150])}')
    print('-' * 60)
    pos = idx + 1

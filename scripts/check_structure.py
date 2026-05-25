f = r'D:\Development\IEGPerTest\external\IEG_Code\code\WLServerTest\WLServerTestDlg.cpp'
with open(f, 'r', encoding='utf-8') as fp:
    c = fp.read()

# Check for OnMouseMove and OnMouseLeave
mm = c.count('void CWLServerTestDlg::OnMouseMove')
ml = c.count('void CWLServerTestDlg::OnMouseLeave')
print('OnMouseMove:', mm)
print('OnMouseLeave:', ml)
print('END_MESSAGE_MAP:', c.count('END_MESSAGE_MAP()'))
print('OnCtlColor:', 'OnCtlColor' in c)
print('OnBnClickedRawPacket:', 'OnBnClickedRawPacket' in c)

# Find exact positions
lines = c.split('\n')
for i, line in enumerate(lines):
    if 'OnMouseMove' in line and 'void CWLServerTestDlg' in line:
        print(f'OnMouseMove at L{i+1}')
    if 'OnMouseLeave' in line and 'void CWLServerTestDlg' in line:
        print(f'OnMouseLeave at L{i+1}')
    if 'END_MESSAGE_MAP()' in line:
        print(f'END_MESSAGE_MAP at L{i+1}')
    if 'Level 2' in line:
        print(f'Level 2 at L{i+1}')
    if 'ON_WM_MOUSEMOVE' in line:
        print(f'ON_WM_MOUSEMOVE at L{i+1}')
    if 'ON_WM_MOUSELEAVE' in line:
        print(f'ON_WM_MOUSELEAVE at L{i+1}')

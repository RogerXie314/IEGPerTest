f = r'D:\Development\IEGPerTest\external\IEG_Code\code\WLServerTest\WLServerTestDlg.cpp'
with open(f, 'r', encoding='utf-8') as fp:
    lines = fp.readlines()

print('Before:', len(lines), 'lines')

# Delete L1395-1504 (0-indexed 1394-1503) - the entire duplicate block in message map
# L1395 = 0-indexed 1394
# L1504 = 0-indexed 1503, we want to KEEP this (END_MESSAGE_MAP)
# So delete 1394-1503
lines = lines[:1394] + lines[1504:]

print('After:', len(lines), 'lines')

with open(f, 'w', encoding='utf-8') as fp:
    fp.writelines(lines)

with open(f, 'r', encoding='utf-8') as fp:
    c = fp.read()

count1 = c.count('void CWLServerTestDlg::OnMouseMove')
count2 = c.count('void CWLServerTestDlg::OnMouseLeave')
has_ctlcolor = 'OnCtlColor' in c
has_rawpacket = 'OnBnClickedRawPacket' in c

print('OnMouseMove count:', count1)
print('OnMouseLeave count:', count2)
print('OnCtlColor present:', has_ctlcolor)
print('OnBnClickedRawPacket present:', has_rawpacket)
print('SAVED')

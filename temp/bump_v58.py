P = r'd:\Development\IEGPerTest\external\IEG_Code\code\WLServerTest\WLServerTest.rc'
data = open(P, 'rb').read()
data = data.replace(b'V5.7', b'V5.8')
data = data.replace(b'5,7,0,0', b'5,8,0,0')
data = data.replace(b'"5, 7, 0, 0"', b'"5, 8, 0, 0"')
open(P, 'wb').write(data)
print('OK', len(data))

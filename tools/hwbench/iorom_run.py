"""HW bench: iorom.asm on the real board: what IN returns with the high address byte 00h-03h (the boot ROM window)
against 08h, 10h, 80h, FFh, for port 56h (YM3812 status) and 9Ah (nobody).
python iorom_run.py [iorom.bin]"""
import os
import sys
import time

import pypicorom

import bench as B

ROM = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(B.WORK, 'iorom.bin'))
HIGHS = [0x00, 0x01, 0x02, 0x03, 0x08, 0x10, 0x80, 0xFF]
N = len(HIGHS) * 2 * 16 + 8


def digits(v):
    if b'*' not in v:
        return ''
    return ''.join(chr(x) for x in v[v.index(b'*') + 1:] if chr(x) in '0123456789ABCDEF')


img = open(ROM, 'rb').read()
p = pypicorom.open(B.DEVICE)
try:
    p.end_comms()
except Exception:
    pass
p.upload(img, 0x7FF)
p.set_parameter('reset', 'low')
time.sleep(0.3)
p.set_parameter('reset', 'z')
p.start_comms(0x400)
vals = bytearray()
end = time.time() + 5
while len(digits(vals)) < 2 * N and time.time() < end:
    b = p.read()
    if b:
        vals.extend(b)
p.end_comms()
d = digits(vals)
if len(d) < 2 * N:
    sys.exit('comms: %s' % vals.hex(' '))
v = bytes.fromhex(d[:2 * N])
i = 0
for h in HIGHS:
    for port in (0x56, 0x9A):
        inc, inn = v[i:i + 8], v[i + 8:i + 16]
        i += 16
        rom = ('ROM %04Xh = %02Xh' % (h * 256 + port, img[h * 256 + port])) if h < 4 else ''
        print('H %02Xh port %02Xh: IN A,(C) %s   IN A,(n) %s   %s' % (h, port, inc.hex(' '), inn.hex(' '), rom))
print('ROM read as memory: %s' % v[i:i + 8].hex(' '))

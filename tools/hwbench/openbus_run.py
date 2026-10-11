"""HW bench: openbus.asm on the real board (open bus reads, values over the PicoROM channel) and in SAPIemu.
python openbus_run.py [openbus.bin]"""
import os
import re
import subprocess
import sys
import time

import pypicorom

import bench as B

ROM = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(B.WORK, 'openbus.bin'))
N = 10
NAMES = ['IN A,(9Ah)', 'IN A,(A0h)', 'IN A,(C5h)', 'IN A,(C) C=9Ah B=5Ah', 'IN B,(C) C=A0h B=33h',
         'IN A,(C) C=C5h B=FFh', 'LD A,(C000h) hole', 'LD A,(C123h) hole', 'LD A,(HL) D0F0h hole',
         'IN A,(9Ah) after LD A,(8000h)=5Ah']


def digits(v):
    if b'*' not in v:
        return ''
    return ''.join(chr(x) for x in v[v.index(b'*') + 1:] if chr(x) in '0123456789ABCDEF')


p = pypicorom.open(B.DEVICE)
try:
    p.end_comms()
except Exception:
    pass
p.upload(open(ROM, 'rb').read(), 0x7FF)
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
hw = bytes.fromhex(d[:2 * N])
# the emulator: the same ROM, the values from RAM 9000h after the run
here = os.path.dirname(os.path.abspath(__file__))
r = subprocess.run([sys.executable, os.path.join(here, 'emu_run.py'), ROM, '0.1', '0', '--mem9000'],
                   capture_output=True, text=True, timeout=300)
m = re.search(r'MEM9000 ([0-9A-Fa-f ]+)', r.stdout)
emu = bytes.fromhex(m.group(1).replace(' ', '')) if m else None
print('%-32s %6s %6s' % ('', 'board', 'emu'))
for i in range(N):
    print('%-32s %6s %6s' % (NAMES[i], '%02Xh' % hw[i], ('%02Xh' % emu[i]) if emu else '-'))

"""HW bench: ymstat2.asm on the real board: histograms of the YM3812 status (MPH-1V, 56h), single reads with the
timer idle and after timer 1 ran out, and INIR blocks.
python ymstat2_run.py [ymstat2.bin]"""
import os
import sys
import time

import pypicorom

import bench as B

ROM = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(B.WORK, 'ymstat2.bin'))
NAMES = ['A single reads, timer idle', 'B single reads, timer 1 ran out', 'C INI blocks, timer 1 ran out']

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
end = time.time() + 30
while vals.count(b'.') < 3 and time.time() < end:
    b = p.read()
    if b:
        vals.extend(b)
    else:
        time.sleep(0.005)
p.end_comms()
if b'*' not in vals or vals.count(b'.') < 3:
    sys.exit('comms: %s' % vals.hex(' '))
text = ''.join(chr(x) for x in vals[vals.index(b'*') + 1:] if chr(x) in '0123456789ABCDEF.')
for name, part in zip(NAMES, text.split('.')[:3]):
    ent = [(int(part[i:i + 2], 16), int(part[i + 2:i + 6], 16)) for i in range(0, len(part) - 5, 6)]
    tot = sum(c for v, c in ent)
    print('%s: %d reads' % (name, tot))
    for v, c in sorted(ent, key=lambda e: -e[1]):
        print('   %02Xh %6d  (%.3f %%)' % (v, c, 100.0 * c / tot if tot else 0))

"""HW bench: zrmd.asm on the real board: two resets, then which address registers of the ZRMD-1 /RES cleared,
and the reads of the ports 61h-64h, 6Fh. Nothing is written to the RAM disk.
python zrmd_run.py [zrmd.bin]"""
import itertools
import os
import sys
import time

import pypicorom

import bench as B

ROM = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(B.WORK, 'zrmd.bin'))
N = 32 + 4 + 5


def reset(p):
    p.set_parameter('reset', 'low')
    time.sleep(0.3)
    p.set_parameter('reset', 'z')


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
reset(p)              # phase A (the flag in RAM is not there yet... unless left by an earlier run: then B first)
time.sleep(0.5)
reset(p)              # phase B
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
combos = list(itertools.product((0, 3), (0, 0x41), (0, 0x10)))
ref = {c: v[4 * i:4 * i + 4] for i, c in enumerate(combos)}
for c in combos:
    print('phase A %02X:%02X:%02X  %s' % (c + (ref[c].hex(' '),)))
got = v[32:36]
print('phase B after /RES, no address set: %s' % got.hex(' '))
match = [c for c in combos if ref[c] == got]
for c in match:
    print('   = %02X:%02X:%02X  ->  A16-21 %s, A8-15 %s, A0-7 %s' % (c + tuple(
        'cleared' if x == 0 else 'kept' for x in c)))
if not match:
    print('   matches no address of phase A')
print('IN 61h %02X, 62h %02X, 63h %02X, 64h %02X, 6Fh %02X' % tuple(v[36:41]))

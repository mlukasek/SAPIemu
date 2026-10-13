"""HW bench: ymres.asm on the real board: the YM3812 status read every ~5 us from the reset on.
python ymres_run.py [ymres.bin] [runs]"""
import os
import sys
import time

import pypicorom

import bench as B

args = [a for a in sys.argv[1:] if not a.startswith('--')]
ROM = os.path.abspath(args[0] if args else os.path.join(B.WORK, 'ymres.bin'))
RUNS = int(args[1]) if len(args) > 1 else 3
N = 96


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
runs = []
for r in range(RUNS):
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
    time.sleep(0.5)   # a reset right after end_comms is not taken
    d = digits(vals)
    if len(d) < 2 * N:
        sys.exit('run %d, comms: %s' % (r + 1, vals.hex(' ')))
    runs.append(bytes.fromhex(d[:2 * N]))
print('read  time from the first fetch (us, 4 MHz)  status in each run')
for k in range(N):
    t = (27 + 87 * (k // 4) + 16 * (k % 4) + 12) / 4.0   # + 12 T: the I/O read comes late in INI
    row = ' '.join('%02X' % run[k] for run in runs)
    if k < 24 or any(run[k] != runs[0][k] for run in runs) or k % 8 == 0:
        print('%3d  %7.2f  %s' % (k, t, row))

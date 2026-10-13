"""HW bench: intblock.asm on the real board: how long /INT0 waits for its INTA in loops of 40 equal instructions
(the INTA is the STSTB before the cycle of the first stack write after /INT0 falls).
python intblock_run.py [intblock.bin|intblock2.bin] [OUTDIR] [--load]"""
import bisect
import os
import sys

import bench as B

args = [a for a in sys.argv[1:] if not a.startswith('--')]
ROM = args[0] if args else os.path.join(B.WORK, 'intblock.bin')
OUT = args[1] if len(args) > 1 else os.path.join(B.WORK, 'cap_intblock')
NAMES = ['NOP', 'LD A,I', 'LD A,R', 'LD I,A', 'NEG', 'IM 1', 'LD A,(IX+0)', 'INC BC']
if 'intblock2' in ROM:
    NAMES = ['NOP', 'LD A,I', 'LD A,R', 'LD A,I + NOP', 'EI', 'LD A,R + NOP', 'LD A,(IX+0)', 'INC BC']
NINT = 8

if '--load' in sys.argv:
    c = B.load(OUT)
else:
    c = B.capture(ROM, OUT, after=1.2, send=b'U', channels=[B.RES, B.RDY, B.IOR, B.MR, B.MW, B.STSTB, B.INT0,
                                                              B.CLK])
ststb = [t for t, v in c.edges[B.STSTB] if v == 0]
mw = [t for t, v in c.edges[B.MW] if v == 0]
ints = [t for t, v in c.edges[B.INT0] if v == 0 and t > 0]
waits = []
for ti in ints:
    j = bisect.bisect_right(mw, ti)
    if j >= len(mw):
        break
    i = bisect.bisect_left(ststb, mw[j]) - 1
    waits.append(ststb[i - 1] - ti)
for n in range(len(NAMES)):
    w = waits[n * NINT:(n + 1) * NINT]
    if not w:
        break
    print('%-12s /INT0 -> INTA: %s us (max %.2f)' % (NAMES[n], ' '.join('%.2f' % (x * 1e6) for x in w),
                                                     max(w) * 1e6))

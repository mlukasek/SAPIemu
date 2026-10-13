"""HW bench: the INT1 pulses of the AND-1B (/INT1 on the probe D9, which is /INT0 otherwise) with and1b.asm running:
pulse widths and the intervals between them (the schematic: 10 VGA lines = 317 us from line 480; in the 50 Hz mode
5 pulses of 6 frames, 4x 16.64 ms and 1x 33.28 ms; 60 Hz every frame).
python and1b_int.py [and1b.bin] [OUTDIR] [--load]"""
import os
import sys

import bench as B

args = [a for a in sys.argv[1:] if not a.startswith('--')]
ROM = args[0] if args else os.path.join(B.WORK, 'and1b.bin')
OUT = args[1] if len(args) > 1 else os.path.join(B.WORK, 'cap_and1b_int')
INT1 = 9

c = B.load(OUT, [B.RES, INT1]) if '--load' in sys.argv else B.capture(ROM, OUT, after=0.4, rate=25_000_000,
                                                                     channels=[B.RES, INT1])
p = [x for x in B.lows(c, INT1) if x[0] > 0.0]
if len(p) < 3:
    sys.exit('INT1: %d pulses' % len(p))
w = [(x[1] - x[0]) * 1e6 for x in p]
gaps = [(p[i + 1][0] - p[i][0]) * 1e3 for i in range(len(p) - 1)]
print('INT1: %d pulses in %.0f ms, width %.1f .. %.1f us' % (len(p), (p[-1][0] - p[0][0]) * 1e3, min(w), max(w)))
print('intervals (ms): ' + ' '.join('%.3f' % g for g in gaps))
span = p[-1][0] - p[0][0]
print('mean rate %.3f Hz' % ((len(p) - 1) / span))

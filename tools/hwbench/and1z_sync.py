"""HW bench: the composite video of the AND-1Z/89 (X2 pin 30) on the analog channel of probe D13, with and1z.asm:
the sync pulses (below the black level), the line period (107 chars x 0.6 us = 64.2 us, or 108 = 64.8 us?), the
HSYNC width and the frame (312 lines = 20.0 ms by the PROMs).
python and1z_sync.py [and1z.bin] [OUTDIR]"""
import os
import sys

import bench as B

ROM = sys.argv[1] if len(sys.argv) > 1 else os.path.join(B.WORK, 'and1z.bin')
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(B.WORK, 'cap_and1z_sync')
VID = 13

c = B.capture(ROM, OUT, after=0.045, rate=250_000_000, channels=[B.RES], analog=[VID], analog_rate=12_500_000)
a = B.load_analog(OUT, c.t0)
t, v = a[VID]
sv = sorted(v)
n = len(sv)
lo, hi = sv[n // 1000], sv[n - 1 - n // 1000]
print('video: %.3f .. %.3f V (0.1 / 99.9 percentile)' % (lo, hi))
hist = {}
for x in v:
    k = round(x, 2)
    hist[k] = hist.get(k, 0) + 1
print('most common levels: ' + ', '.join('%.2f V (%d)' % kv for kv in sorted(hist.items(), key=lambda kv: -kv[1])[:6]))
thr = lo + 0.3 * (hi - lo) * 0.5   # sync tip to black: about the lowest fifth of the swing
thr = float(os.environ.get('SYNC_THR', thr))
pulses = []
start = None
for ti, x in zip(t, v):
    if x < thr and start is None:
        start = ti
    elif x >= thr and start is not None:
        pulses.append((start, ti - start))
        start = None
pulses = [p for p in pulses if p[0] > 0.001]
print('threshold %.3f V: %d sync pulses' % (thr, len(pulses)))
widths = {}
for s, w in pulses:
    k = round(w * 1e6, 1)
    widths[k] = widths.get(k, 0) + 1
print('pulse widths (us: count): ' + ', '.join('%.1f: %d' % kv for kv in sorted(widths.items())))
hs = [s for s, w in pulses if 3e-6 < w < 8e-6]
gaps = sorted(hs[i + 1] - hs[i] for i in range(len(hs) - 1))
if gaps:
    g = [x for x in gaps if x < 100e-6]
    print('line period: median %.3f us, min %.3f, max %.3f (%d lines)' % (g[len(g) // 2] * 1e6, g[0] * 1e6, g[-1] * 1e6,
                                                                           len(g)))
long = [s for s, w in pulses if w > 20e-6]
if len(long) > 1:
    print('long pulses (vertical sync) at %s ms; frame %.3f ms' % (' '.join('%.3f' % (x * 1e3) for x in long),
                                                                     (long[-1] - long[0]) / (len(long) - 1) * 1e3))

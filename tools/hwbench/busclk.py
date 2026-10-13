"""HW bench: the bus clock FI2 (on D13 through the DKS-1) against the CPU clock (D12, Z80 pin 6 on JPR-1V):
frequency, duty cycle and the delay of its edges after the CPU clock edges, also in analog (levels).
Run once with TURBO on and once off. python busclk.py [ROM] [OUTDIR]"""
import bisect
import os
import sys

import bench as B

ROM = sys.argv[1] if len(sys.argv) > 1 else os.path.join(B.WORK, 'wire.bin')
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(B.WORK, 'cap_busclk')
FI2 = 13

c = B.capture(ROM, OUT, after=0.005, channels=[B.RES, B.CLK, FI2], analog=[B.CLK, FI2])


def describe(k, name):
    e = [(t, v) for t, v in c.edges[k] if t > 0.0005]
    rise = [t for t, v in e if v == 1]
    fall = [t for t, v in e if v == 0]
    if len(rise) < 3:
        print('%s: no clock (%d edges)' % (name, len(e)))
        return None
    per = (rise[-1] - rise[0]) / (len(rise) - 1)
    highs = []
    for t in rise[:-1]:
        i = bisect.bisect_right(fall, t)
        if i < len(fall):
            highs.append(fall[i] - t)
    print('%s: %.4f MHz, period %.1f ns, high %.1f ns (%.0f %%)' % (name, 1e-6 / per, per * 1e9,
          sum(highs) / len(highs) * 1e9, 100 * sum(highs) / len(highs) / per))
    return rise, fall


cpu = describe(B.CLK, 'CPU CLK (D12)')
bus = describe(FI2, 'bus FI2 (D13)')
if cpu and bus:
    for name, a, b in (('FI2 rise after CPU rise', bus[0], cpu[0]), ('FI2 rise after CPU fall', bus[0], cpu[1]),
                       ('FI2 fall after CPU fall', bus[1], cpu[1]), ('FI2 fall after CPU rise', bus[1], cpu[0])):
        d = []
        for t in a[:2000]:
            i = bisect.bisect_right(b, t) - 1
            if i >= 0:
                d.append(t - b[i])
        d.sort()
        print('   %-26s %5.0f .. %5.0f ns (median %5.0f)' % (name, d[0] * 1e9, d[-1] * 1e9, d[len(d) // 2] * 1e9))
a = B.load_analog(OUT, c.t0)
for k, name in ((B.CLK, 'CPU CLK'), (FI2, 'FI2')):
    if k in a:
        v = sorted(a[k][1])
        n = len(v)
        print('%s analog: low %.2f V, high %.2f V (1st / 99th percentile)' % (name, v[n // 100], v[n * 99 // 100]))

"""HW bench: RDY in analog during the CGA-1V read window of cga1v.asm (W2). The rising edges of RDY (the end of
each wait): rise time and whether the voltage crosses the logic threshold more than once (a slow edge with noise
gives a false short pulse in the digital channel).
python rdy_analog.py [cga1v.bin] [OUTDIR] [--load]"""
import bisect
import os
import sys

import bench as B

args = [a for a in sys.argv[1:] if not a.startswith('--')]
ROM = args[0] if args else os.path.join(B.WORK, 'cga1v.bin')
OUT = args[1] if len(args) > 1 else os.path.join(B.WORK, 'cap_rdy_analog')
if '--load' in sys.argv:
    c = B.load(OUT)
else:
    c = B.capture(ROM, OUT, after=0.03, analog=[B.RDY], analog_rate=50_000_000)
an = B.load_analog(OUT, c.t0)[B.RDY]
ts, vs = an
dt = (ts[-1] - ts[0]) / (len(ts) - 1)
print('analog RDY: %d samples, %.1f ns apart' % (len(ts), dt * 1e9))
iow = B.lows(c, B.IOW)[1:]
clk = [t for t, v in c.edges[B.CLK] if v == 0]
for n, name in ((0, 'W1 writes'), (1, 'W2 reads')):
    a, b = iow[2 * n][1], iow[2 * n + 1][0]
    n_clk = bisect.bisect_left(clk, iow[2 * n + 1][0]) - bisect.bisect_left(clk, iow[2 * n][0])
    rdy = B.lows(c, B.RDY, a, b)
    short = [(s, e) for s, e in rdy if e - s < 100e-9]
    print('%s: %d clocks, %d RDY low pulses, %d shorter than 100 ns' % (name, n_clk, len(rdy), len(short)))
    rises, multi, lows_v, highs_v = [], 0, [], []
    for s, e in rdy:
        i0 = bisect.bisect_left(ts, e - 60e-9)
        i1 = bisect.bisect_left(ts, e + 200e-9)
        seg = vs[i0:i1]
        if len(seg) < 4:
            continue
        lo, hi = min(seg), max(seg)
        lows_v.append(lo)
        highs_v.append(hi)
        v10, v90 = lo + 0.1 * (hi - lo), lo + 0.9 * (hi - lo)
        t10 = next((ts[i0 + k] for k, v in enumerate(seg) if v >= v10), None)
        t90 = next((ts[i0 + k] for k, v in enumerate(seg) if v >= v90), None)
        if t10 is not None and t90 is not None:
            rises.append(t90 - t10)
        cross = sum(1 for k in range(1, len(seg)) if (seg[k] >= 1.65) != (seg[k - 1] >= 1.65))
        if cross > 1:
            multi += 1
    if rises:
        rises.sort()
        print('   rising edge 10-90 %%: median %.0f ns, max %.0f ns; low level %.2f V, high level %.2f V; '
              'edges crossing 1.65 V more than once: %d' % (rises[len(rises) // 2] * 1e9, rises[-1] * 1e9,
                                                            sum(lows_v) / len(lows_v), sum(highs_v) / len(highs_v),
                                                            multi))
    for s, e in short[:3]:
        i0 = bisect.bisect_left(ts, s - 100e-9)
        i1 = bisect.bisect_left(ts, e + 100e-9)
        print('   short pulse at %.3f us: analog %s' % ((s - a) * 1e6, ' '.join('%.2f' % v for v in vs[i0:i1])))

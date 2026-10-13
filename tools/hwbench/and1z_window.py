"""HW bench: the CPU window of the AND-1Z/89 with and1v.asm (accesses to F800h). Probes: D14 = QA of the Johnson
counter (DC2 pin 2, the window is QA high), D15 = Q of the WAIT flip-flop DD1A (pin 5, Q = 1 pulls RDY low through
the 7406). Checks the probes first (QA 1.667 MHz = 10 MHz / 6, WAIT high exactly while RDY is low), then for each
access: the request (/MR or /MW falling) to the next QA rise, WAIT set (from STSTB) and cleared against the QA fall,
RDY up, and the waits (CPU clock falls while RDY is low).
python and1z_window.py [and1v.bin] [OUTDIR] [--load]"""
import bisect
import os
import sys

import bench as B

args = [a for a in sys.argv[1:] if not a.startswith('--')]
ROM = args[0] if args else os.path.join(B.WORK, 'and1v.bin')
OUT = args[1] if len(args) > 1 else os.path.join(B.WORK, 'cap_and1z_window')
QA, WAIT = 14, 15

c = B.load(OUT) if '--load' in sys.argv else B.capture(ROM, OUT, after=0.06)
qr = [t for t, v in c.edges[QA] if v == 1 and t > 0]
qf = [t for t, v in c.edges[QA] if v == 0 and t > 0]
if len(qr) < 100:
    sys.exit('D14 (QA): %d rising edges, no clock' % len(qr))
per = (qr[-1] - qr[0]) / (len(qr) - 1)
hi = []
for t in qr[:5000]:
    i = bisect.bisect_right(qf, t)
    if i < len(qf):
        hi.append(qf[i] - t)
print('D14 QA: %.4f MHz (period %.1f ns), high %.1f ns (%.0f %%)' % (1e-6 / per, per * 1e9, sum(hi) / len(hi) * 1e9,
                                                                   100 * sum(hi) / len(hi) / per))
wl = B.highs(c, WAIT)
rl = B.lows(c, B.RDY)
print('D15 WAIT high %d times, RDY low %d times' % (len(wl), len(rl)))
d0, d1 = [], []
for s, e in wl:
    j = bisect.bisect_left([x[0] for x in rl], s - 200e-9)
    if j < len(rl) and abs(rl[j][0] - s) < 200e-9:
        d0.append(rl[j][0] - s)
        d1.append(rl[j][1] - e)
if d0:
    print('RDY falls %.0f..%.0f ns after WAIT rises, rises %.0f..%.0f ns after WAIT falls (%d matched)' % (
        min(d0) * 1e9, max(d0) * 1e9, min(d1) * 1e9, max(d1) * 1e9, len(d0)))

clkf = [t for t, v in c.edges[B.CLK] if v == 0]
clk_per = (clkf[-1] - clkf[0]) / (len(clkf) - 1)
print('CPU clock %.4f MHz' % (1e-6 / clk_per))
iow = [p for p in B.lows(c, B.IOW) if p[0] > 0]
names = ['W1 LDIR write', 'W2 LDIR read', 'W3 single writes', 'W4 single reads']
for w in range(4):
    a, b = iow[2 * w][0], iow[2 * w + 1][0]
    sig = B.MW if w in (0, 2) else B.MR
    rows = {}
    for s, e in B.lows(c, sig):
        if not a < s < b:
            continue
        k = bisect.bisect_right(qr, s)            # the first QA rise after the request
        g = qr[k]
        wi = [x for x in wl if x[0] < e and x[1] > s - 1e-6]
        ri = [x for x in rl if x[0] < e and x[1] > s - 1e-6]
        if not wi or not ri:
            continue
        wend, rup = wi[0][1], ri[0][1]
        qend = qf[bisect.bisect_right(qf, g)]
        n = bisect.bisect_left(clkf, rup - 10e-9) - bisect.bisect_left(clkf, ri[0][0])
        cf = clkf[bisect.bisect_right(clkf, s) - 1]   # the CPU clock fall before the request
        key = (round((s - cf) * 1e9 / 8) * 8, )
        rows.setdefault(n, []).append(((g - s) * 1e9, (wend - qend) * 1e9, (rup - wend) * 1e9, (s - cf) * 1e9,
                                       (wend - g) * 1e9))
    print(names[w] + ':')
    for n in sorted(rows):
        r = rows[n]
        f = lambda i: '%5.0f..%5.0f' % (min(x[i] for x in r), max(x[i] for x in r))
        print('  %d waits (%3d): request->QA rise %s, WAIT end - QA fall %s, RDY up - WAIT end %s, '
              'request - CLK fall %s, WAIT end - QA rise %s' % (n, len(r), f(0), f(1), f(2), f(3), f(4)))

"""HW bench: the CPU window of the CGA-1V (D15 = QA, IC7 pin 11; D14 = Q of IC7B "/CPU", IC7 pin 9) with
cga1v.asm: checks the probes, then for each access the chain /MR or /MW -> QA edge -> /CPU -> RDY up -> CLK.
python cga_window.py [cga1v.bin] [OUTDIR] [--load]"""
import bisect
import os
import sys

import bench as B

QA, CPU = 15, 14
args = [a for a in sys.argv[1:] if not a.startswith('--')]
ROM = args[0] if args else os.path.join(B.WORK, 'cga1v.bin')
OUT = args[1] if len(args) > 1 else os.path.join(B.WORK, 'cap_cga_window')
c = B.load(OUT) if '--load' in sys.argv else B.capture(ROM, OUT, after=0.03)

qa_up = [t for t, v in c.edges[QA] if v == 1]
qa_dn = [t for t, v in c.edges[QA] if v == 0]
if len(qa_up) < 10:
    print('D15 (QA): no clock, %d edges' % len(c.edges[QA]))
    sys.exit()
per = (qa_up[-1] - qa_up[0]) / (len(qa_up) - 1)
hi = [d - u for u, d in zip(qa_up, qa_dn[bisect.bisect_left(qa_dn, qa_up[0]):])]
print('D15 QA: %.4f MHz (period %.1f ns, 25.175 MHz / %.3f), high %.0f%% of the period' % (
    1e-6 / per, per * 1e9, per * 25.175e6, 100 * sum(hi) / len(hi) / per))
cpu = c.edges[CPU]
print('D14 /CPU: start %d, %d edges' % (c.init[CPU], len(cpu)))
iow = [p for p in B.lows(c, B.IOW) if p[0] > 0][1:]   # after the reset; the first is OUT 63h
clk = [t for t, v in c.edges[B.CLK] if v == 0]
for n, name, k in ((0, 'writes', B.MW), (1, 'reads', B.MR)):
    a, b = iow[2 * n][1], iow[2 * n + 1][0]
    acc = B.lows(c, k, a, b)
    e_cpu = [(t, v) for t, v in cpu if a < t < b]
    print('%s: %d accesses, D14 edges in the window %d' % (name, len(acc), len(e_cpu)))
    # D14 edges against the nearest QA rising edge
    off = []
    for t, v in e_cpu:
        j = bisect.bisect_left(qa_up, t)
        cand = [qa_up[x] for x in (j - 1, j) if 0 <= x < len(qa_up)]
        q = min(cand, key=lambda x: abs(x - t))
        off.append(((t - q) * 1e9, v))
    for lv in (0, 1):
        o = sorted(x for x, v in off if v == lv)
        if o:
            print('   D14 -> %d: from the nearest QA rise %+.0f .. %+.0f ns (median %+.0f)' % (lv, o[0], o[-1],
                                                                                          o[len(o) // 2]))
    rdy_up = [t for t, v in c.edges[B.RDY] if v == 1 and a < t < b]
    chain = []
    for s, e in acc:
        j = bisect.bisect_left(qa_up, s)
        q1 = qa_up[j] if j < len(qa_up) else None                   # first window start after the strobe
        r = rdy_up[bisect.bisect_left(rdy_up, s)] if bisect.bisect_left(rdy_up, s) < len(rdy_up) else None
        if q1 is None or r is None or r > e:
            continue
        chain.append(((q1 - s) * 1e9, (r - s) * 1e9))
    if chain:
        d1 = sorted(x for x, _ in chain)
        d2 = sorted(y for _, y in chain)
        print('   strobe -> first QA rise: %.0f .. %.0f ns; strobe -> RDY up: %.0f .. %.0f ns' % (d1[0], d1[-1], d2[0],
                                                                                              d2[-1]))
        # which QA rise grants the access: RDY up - that QA rise, by the strobe -> first QA rise
        rel = {}
        for x, y in chain:
            k2 = int(x // 40) * 40
            rel.setdefault(k2, []).append(y - x)
        for k2 in sorted(rel):
            v = sorted(rel[k2])
            print('      strobe->QA %3d..%3d ns: %4d accesses, RDY up after that QA rise %.0f .. %.0f ns' % (
                k2, k2 + 40, len(v), v[0], v[-1]))

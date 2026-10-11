"""HW bench: ststb.asm on the real board, the counts of the bus signals in each part and the INTA cycle.
python ststb_run.py [ststb.bin] [OUTDIR] [--load]  (--load: analyse a saved capture again)"""
import os
import sys

import bench as B

args = [a for a in sys.argv[1:] if not a.startswith('--')]
ROM = args[0] if args else os.path.join(B.WORK, 'ststb.bin')
OUT = args[1] if len(args) > 1 else os.path.join(B.WORK, 'cap_ststb')
c = B.load(OUT) if '--load' in sys.argv else B.capture(ROM, OUT, after=0.06, send=b'Z')

iow = B.lows(c, B.IOW)
marks = [p for p in iow]   # all writes; the markers are found by the program order below
# the program after the reset: 3 UART writes (13h, 10h, 11h), then P1..P4 = 2 markers each with the bodies,
# P3 has 4 more writes inside; then P5: OUT 10h (IER) and the marker
w = [s for s, e in iow]
we = [e for s, e in iow]
i = 3                                     # skip the UART init writes
parts = {}
for name, inner in (('P1 8x NOP', 0), ('P2 4x IN (F7h)', 0), ('P3 4x OUT (C) B=F8h', 4), ('P4 4x IN (C) B=F8h', 0)):
    parts[name] = (we[i], w[i + 1 + inner])
    i += 2 + inner


def count(k, a, b, positive=False):
    p = (B.highs if positive else B.lows)(c, k, a, b)
    return len(p), p


print('%-22s %6s %4s %5s %5s %4s %4s %6s  RDY low (ns)' % ('part', 'us', 'M1', 'STSTB', '/IOR', '/IOW', '/MR',
                                                            '/MW'))
for name, (a, b) in parts.items():
    n_m1, _ = count(B.M1, a, b)
    n_st, st = count(B.STSTB, a, b, positive=True)
    n_ior, _ = count(B.IOR, a, b)
    n_iow, _ = count(B.IOW, a, b)
    n_mr, _ = count(B.MR, a, b)
    n_mw, _ = count(B.MW, a, b)
    _, rdy = count(B.RDY, a, b)
    print('%-22s %6.2f %4d %5d %5d %4d %4d %6d  %s' % (name, (b - a) * 1e6, n_m1, n_st, n_ior, n_iow, n_mr, n_mw,
                                                     [round((e - s) * 1e9) for s, e in rdy]))
    st_w = sorted(set(round((e - s) * 1e9) for s, e in st))
    print('%22s STSTB widths (ns): %s' % ('', st_w[:6]))

# P5: the interrupt. The RST 38h pushes the PC: two /MW right after the INTA cycle.
ier = w[i]                                # OUT 10h,80h
mws = [p for p in B.lows(c, B.MW) if p[0] > ier]
if len(mws) >= 2:
    t_push = mws[0][0]
    print('\nP5 interrupt: events from 6 us before the first push write (times in us from it):')
    ev = []
    for k, nm, pos in ((B.M1, '/M1', False), (B.STSTB, 'STSTB', True), (B.MR, '/MR', False), (B.MW, '/MW', False),
                       (B.IOR, '/IOR', False), (B.IOW, '/IOW', False), (B.RDY, 'RDY', False), (B.INT0, '/INT0', False)):
        for s, e in (B.highs if pos else B.lows)(c, k, t_push - 6e-6, t_push + 1.5e-6):
            ev.append((s, e, nm))
    for s, e, nm in sorted(ev):
        print('   %-6s %8.3f .. %8.3f  (%4.0f ns)' % (nm, (s - t_push) * 1e6, (e - t_push) * 1e6, (e - s) * 1e9))
else:
    print('P5: no interrupt seen')

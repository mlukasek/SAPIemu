"""HW bench: wiring check of the data bus on the analyzer (since 2026-10-13: D3 = DB0, D4 = DB1, D5 = DB2,
D10 = DB3, D11 = DB4, D13 = DB5, D14 = DB6, D15 = DB7). Runs databus.asm and reads the data byte at each rising
/MW and /MR edge (sampled a few ns before the edge).
python databus_check.py [databus.bin] [OUTDIR]"""
import bisect
import os
import sys

import bench as B

ROM = sys.argv[1] if len(sys.argv) > 1 else os.path.join(B.WORK, 'databus.bin')
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(B.WORK, 'cap_databus')
DB = [3, 4, 5, 10, 11, 13, 14, 15]
PATS = [1 << i for i in range(8)] + [0xFF ^ (1 << i) for i in range(8)] + [0x55, 0xAA, 0x00, 0xFF]

c = B.capture(ROM, OUT, after=0.002)


def level(k, t):
    e = c.edges[k]
    i = bisect.bisect_right([x[0] for x in e], t) - 1
    return e[i][1] if i >= 0 else c.init[k]


def byte_at(t):
    return sum(level(k, t) << i for i, k in enumerate(DB))


def at_rising(k):
    return [t for t, v in c.edges[k] if v == 1 and t > 0]


mw = [byte_at(t - 10e-9) for t in at_rising(B.MW)]
mr = [byte_at(t - 10e-9) for t in at_rising(B.MR)]
print('writes: %s' % ' '.join('%02X' % x for x in mw))
print('reads:  %s' % ' '.join('%02X' % x for x in mr))
ok = True
for name, got in (('writes', mw), ('reads', mr)):
    if got[:len(PATS)] != PATS:
        ok = False
        bad = 0
        for want, g in zip(PATS, got):
            bad |= want ^ g
        print('%s differ, wrong bits: %s' % (name, ' '.join('DB%d (D%d)' % (i, DB[i]) for i in range(8) if bad >> i & 1)))
print('data bus wiring OK' if ok else 'data bus wiring NOT OK')

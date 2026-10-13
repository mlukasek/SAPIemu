"""HW bench: the data bus around INTA (data bus on the analyzer since 2026-10-13, see databus_check.py).
Runs an intavec ROM (default intavec_gap.bin: four loops, each ended by a DSM-1V receive interrupt in IM 2; the CPU
reports the vector it read as "T<n>=<hex>" on COM13) and prints, for each interrupt, the data bus byte from 2 us
before the INTA STSTB to 1 us after it (each change with its time), next to what the CPU reported.
python intabus.py [ROM] [OUTDIR] [--load]"""
import bisect
import os
import sys

import bench as B

args = [a for a in sys.argv[1:] if not a.startswith('--')]
ROM = args[0] if args else os.path.join(B.WORK, 'intavec_gap.bin')
OUT = args[1] if len(args) > 1 else os.path.join(B.WORK, 'cap_intabus')
DB = [3, 4, 5, 10, 11, 13, 14, 15]

if '--load' in sys.argv:
    c = B.load(OUT)
else:
    c = B.capture(ROM, OUT, after=0.12, send=b'U')
    print('serial: %r' % c.serial)

times = {k: [t for t, v in c.edges[k]] for k in DB}


def level(k, t):
    e = c.edges[k]
    i = bisect.bisect_right(times[k], t) - 1
    return e[i][1] if i >= 0 else c.init[k]


def byte_at(t):
    return sum(level(k, t) << i for i, k in enumerate(DB))


ststb = [t for t, v in c.edges[B.STSTB] if v == 0]
mw = [t for t, v in c.edges[B.MW] if v == 0]
clk = [t for t, v in c.edges[B.CLK] if v == 1]   # rising CPU clock edges
ints = [t for t, v in c.edges[B.INT0] if v == 0 and t > 0]
for n, ti in enumerate(ints):
    w1 = mw[bisect.bisect_right(mw, ti)]          # the first write after /INT0: PC high byte to the stack
    i = bisect.bisect_left(ststb, w1) - 1          # STSTB of that write cycle
    inta = ststb[i - 1]                            # the one before it: INTA
    print('interrupt %d: /INT0 at %.3f ms, INTA STSTB %.0f ns later, first stack write %.0f ns after INTA'
          % (n, ti * 1e3, (inta - ti) * 1e9, (w1 - inta) * 1e9))
    # the data bus changes from -2 us to +1 us around the INTA STSTB
    t0, t1 = inta - 2e-6, inta + 1e-6
    ch = sorted(set(t for k in DB for t in times[k] if t0 < t < t1))
    last = byte_at(t0)
    out = ['%+6.0f:%02X' % ((t0 - inta) * 1e9, last)]
    for t in ch:
        b = byte_at(t + 1e-12)
        if b != last:
            out.append('%+6.0f:%02X' % ((t - inta) * 1e9, b))
            last = b
    print('   bus: ' + ' '.join(out))
    # the byte at the rising clock edges after the INTA STSTB (the Z80 takes the vector on one of them)
    k = bisect.bisect_right(clk, inta)
    print('   at rising CLK: ' + ' '.join('%+5.0f:%02X' % ((clk[j] - inta) * 1e9, byte_at(clk[j] - 2e-9))
                                     for j in range(k - 1, min(k + 4, len(clk)))))

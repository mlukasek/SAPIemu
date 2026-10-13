"""HW bench: and1v.asm / cga1v.asm on the real board and in SAPIemu: CPU clocks per window (between the OUT (F7h) markers)
and the RDY low time of each access (in CPU clocks) on the board.
python and1v_run.py [and1v.bin|cga1v.bin] [OUTDIR] [--load] [--noemu] [--machine=FILE (for the emulator)]"""
import os
import re
import subprocess
import sys

import bench as B

args = [a for a in sys.argv[1:] if not a.startswith('--')]
ROM = args[0] if args else os.path.join(B.WORK, 'and1v.bin')
OUT = args[1] if len(args) > 1 else os.path.join(B.WORK, 'cap_and1v')
CARD = 'CGA-1V' if 'cga' in os.path.basename(ROM).lower() else 'AND-1V'
NAMES = ['W1 LDIR RAM->' + CARD, 'W2 LDIR %s->RAM' % CARD, 'W3 16x write', 'W4 16x read']
c = B.load(OUT) if '--load' in sys.argv else B.capture(ROM, OUT, after=0.4 if 'long' in os.path.basename(ROM) else 0.06)
BYTES = 0x3000 if 'long' in os.path.basename(ROM) else 256

clk = [t for t, v in c.edges[B.CLK] if v == 0]   # falling edges of the CPU clock
iow = [p for p in B.lows(c, B.IOW) if p[0] > 0]   # after the reset only (the capture holds 2 ms before)
if CARD == 'CGA-1V':
    iow = iow[1:]   # the first write is OUT 63h,C0h (the window on), not a marker


def clocks(a, b):
    import bisect
    return bisect.bisect_left(clk, b) - bisect.bisect_left(clk, a)


period = (clk[-1] - clk[0]) / (len(clk) - 1)
print('CPU clock %.4f MHz' % (1e-6 / period))
hw = []
for n in range(4):
    a, b = iow[2 * n][1], iow[2 * n + 1][0]          # end of the start marker .. start of the end marker
    rdy = B.lows(c, B.RDY, a, b)
    waits = [clocks(s, e) for s, e in rdy]
    hist = {}
    for x in waits:
        hist[x] = hist.get(x, 0) + 1
    n_clk = clocks(iow[2 * n][0], iow[2 * n + 1][0])   # marker write to marker write, as the emulator log
    hw.append(n_clk)
    print('%-22s %6d clocks, RDY low %3d times, clocks low: %s' % (NAMES[n], n_clk, len(rdy),
          dict(sorted(hist.items()))))
    if n < 2:
        print('%22s waits per byte (RDY low clocks / bytes): %.3f' % ('', sum(waits) / BYTES))

if '--noemu' not in sys.argv:
    # the same ROM in SAPIemu: the cycle of each OUT (F7h) from the I/O log
    here = os.path.dirname(os.path.abspath(__file__))
    slow = period > 0.4e-6   # the board runs at 2 MHz
    unit = 2 if slow else 1   # the emulator I/O log counts 4 MHz cycles
    r = subprocess.run([sys.executable, os.path.join(here, 'emu_run.py'), ROM, '0.4', '0', '--iolog']
                       + [a for a in sys.argv[1:] if a.startswith('--machine=')]
                       + (['--slow'] if slow else []),
                       capture_output=True, text=True, timeout=300)
    cyc = [int(m.group(1)) for m in re.finditer(r'^\s*(\d+) PC=\w+ OUT F7', r.stdout, re.M)]
    if len(cyc) >= 8:
        print('SAPIemu (cycles between the two marker writes, the board the same):')
        for n in range(4):
            e = (cyc[2 * n + 1] - cyc[2 * n]) // unit
            print('%-22s %6d clocks   (board %d, difference %+d)' % (NAMES[n], e, hw[n], e - hw[n]))
    else:
        print('SAPIemu: markers not found\n' + r.stdout[-2000:] + r.stderr[-2000:])

"""HW bench: vbi.asm: the VBI flag of the CGA-1V (marker /IOW) against VSYNC (D14) and QA (D15).
python vbi_run.py [vbi.bin] [OUTDIR] [--load]"""
import os
import sys

import bench as B

VS = B.VSYNC
args = [a for a in sys.argv[1:] if not a.startswith('--')]
ROM = args[0] if args else os.path.join(B.WORK, 'vbi.bin')
OUT = args[1] if len(args) > 1 else os.path.join(B.WORK, 'cap_vbi')
CH = [B.RES, B.IOW, VS]
c = B.load(OUT, CH) if '--load' in sys.argv else B.capture(ROM, OUT, after=0.12, channels=CH)
LINE = 800 / 25.175e6
iow = [s for s, e in B.lows(c, B.IOW) if s > 0][1:]       # the first is OUT 63h
ed = c.edges[VS]
fall = [t for t, v in ed if v == 0 and t > 0]
rise = [t for t, v in ed if v == 1 and t > 0]
print('VSYNC: start %d, %d falling, %d rising edges' % (c.init[VS], len(fall), len(rise)))
if len(fall) > 2:
    per = (fall[-1] - fall[0]) / (len(fall) - 1)
    print('VSYNC period %.4f ms (%.3f Hz, %.2f lines of 31.778 us)' % (per * 1e3, 1 / per, per / LINE))
    widths = [r - f for f in fall for r in rise if 0 < r - f < 1e-3][:3]
    print('VSYNC low for %s us (%s lines)' % ([round(w * 1e6, 2) for w in widths], [round(w / LINE, 2) for w in widths]))
print('VBI markers: %d, period %s ms' % (len(iow), [round((b - a) * 1e3, 4) for a, b in zip(iow, iow[1:])][:4]))
for m in iow[:5]:
    nf = min((f for f in fall if f > m), default=None)
    pf = max((f for f in fall if f < m), default=None)
    print('VBI marker at %.4f ms: next VSYNC fall after %s, previous VSYNC fall before %s' % (
        m * 1e3, ('%.1f us = %.2f lines' % ((nf - m) * 1e6, (nf - m) / LINE)) if nf else '-',
        ('%.1f us = %.2f lines' % ((m - pf) * 1e6, (m - pf) / LINE)) if pf else '-'))

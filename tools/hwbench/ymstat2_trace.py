"""HW bench: ymstat2.asm on the board WITHOUT a comms session (the program waits at ACTIVE and sends nothing),
recorded for 2.5 s: how far it gets, phase by phase (/IOR and /MR counts per 50 ms), /INT0 and channel 13
(/INT1 when the probe is moved there).
python ymstat2_trace.py [ymstat2.bin] [OUTDIR]"""
import os
import sys

import bench as B

ROM = sys.argv[1] if len(sys.argv) > 1 else os.path.join(B.WORK, 'ymstat2.bin')
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(B.WORK, 'cap_ymstat2')
CH = [B.RES, B.IOR, B.IOW, B.MR, B.MW, B.INT0, 13]

c = B.capture(ROM, OUT, after=2.5, rate=25_000_000, channels=CH)
STEP = 0.05
for k, name in ((B.IOR, '/IOR'), (B.IOW, '/IOW'), (B.MR, '/MR'), (B.MW, '/MW')):
    falls = [t for t, v in c.edges[k] if v == 0 and t >= 0]
    bins = [0] * int(2.5 / STEP + 1)
    for t in falls:
        bins[min(int(t / STEP), len(bins) - 1)] += 1
    print('%-5s %7d in total; per 50 ms: %s' % (name, len(falls), ' '.join(str(x) for x in bins)))
for k, name in ((B.INT0, '/INT0'), (13, 'D13')):
    e = c.edges[k]
    print('%-5s start %d, %d edges%s' % (name, c.init[k], len(e), (', first at %.3f ms' % (e[0][0] * 1e3)) if e else ''))

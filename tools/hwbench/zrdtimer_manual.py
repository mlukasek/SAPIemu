"""HW bench: zrdtimer.asm with a manual reset (the analyzer card with the /RTL wire out of the rack): uploads the
ROM, opens the comms channel and waits up to 3 minutes for the 82C54 counts after the author presses RESET.
python zrdtimer_manual.py [zrdtimer.bin]"""
import os
import sys
import time

import pypicorom

import bench as B

ROM = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(B.WORK, 'zrdtimer.bin'))


def digits(v):
    if b'*' not in v:
        return ''
    return ''.join(chr(x) for x in v[v.index(b'*') + 1:] if chr(x) in '0123456789ABCDEF')


p = pypicorom.open(B.DEVICE)
try:
    p.end_comms()
except Exception:
    pass
p.upload(open(ROM, 'rb').read(), 0x7FF)
p.start_comms(0x400)
print('ROM uploaded, waiting for RESET', flush=True)
vals = bytearray()
end = time.time() + 180
while len(digits(vals)) < 16 and time.time() < end:
    b = p.read()
    if b:
        vals.extend(b)
    else:
        time.sleep(0.01)
p.end_comms()
if len(digits(vals)) < 16:
    sys.exit('no result (RESET not pressed?)')
v = bytes.fromhex(digits(vals)[:16])
cnt = [v[2 * i] | (v[2 * i + 1] << 8) for i in range(4)]
names = ['calibration 26 T x 65536', 'LDIR memory -> memory', 'LDIR memory -> CGA-1V', 'LDIR CGA-1V -> memory']
print('82C54 counts: %s' % cnt)
for i in range(4):
    print('%-26s %8.3f T per unit' % (names[i], cnt[i] * 26.0 / cnt[0]))
print('CPU clock from the timer calibration: %.4f MHz' % (1703931 * 55930.4 / cnt[0] / 1e6))

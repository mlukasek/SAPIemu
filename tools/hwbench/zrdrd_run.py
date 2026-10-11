"""HW bench: zrdrd.asm on the real board (ZRD-1V rev 1.1 at 60h): what reads of 61h-63h return and where the
address goes.
python zrdrd_run.py [zrdrd.bin]"""
import os
import sys
import time

import pypicorom

import bench as B

ROM = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(B.WORK, 'zrdrd.bin'))
N = 4 + 3 * 5 + 9 * 4


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
p.set_parameter('reset', 'low')
time.sleep(0.3)
p.set_parameter('reset', 'z')
p.start_comms(0x400)
vals = bytearray()
end = time.time() + 5
while len(digits(vals)) < 2 * N and time.time() < end:
    b = p.read()
    if b:
        vals.extend(b)
p.end_comms()
d = digits(vals)
if len(d) < 2 * N:
    sys.exit('comms: %s' % vals.hex(' '))
v = bytes.fromhex(d[:2 * N])
ref = v[0:4]
tests = [('IN 61h', v[4], v[5:9]), ('IN 62h', v[9], v[10:14]), ('IN 63h', v[14], v[15:19])]
cn = ['03:41:FF', '03:41:61', '03:41:00', '03:FF:10', '03:62:10', '03:00:10', '3F:41:10', '23:41:10', '00:41:10']
cand = {cn[i]: v[19 + 4 * i:23 + 4 * i] for i in range(9)}
print('03:41:10 (reference): %s' % ref.hex(' '))
for name, a in cand.items():
    print('   candidate %s: %s' % (name, a.hex(' ')))
for name, val, after in tests:
    same = [k for k, a in cand.items() if a == after] + (['03:41:10 (unchanged)'] if after == ref else [])
    print('%s returned %02Xh; then 60h gave %s = %s' % (name, val, after.hex(' '), ', '.join(same) or 'none of the candidates'))

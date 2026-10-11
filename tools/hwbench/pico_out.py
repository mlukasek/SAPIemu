"""PicoROM comms, SAPI -> PC: counts the bytes of pico_out.bin (0, 1, 2 ...) and the gaps in the sequence."""
import sys
import time

import pypicorom

p = pypicorom.open('SAPI_P28-01')
try:
    p.end_comms()
except Exception:
    pass
p.upload(open(sys.argv[1], 'rb').read(), 0x7FF)
p.set_parameter('reset', 'low')
time.sleep(0.3)
p.set_parameter('reset', 'z')
p.start_comms(0x400)
got = bytearray()
t0 = time.time()
while time.time() - t0 < 5.0 and len(got) < 4096:
    b = p.read(4096)
    if b:
        got.extend(b)
dt = time.time() - t0
p.end_comms()
gaps = [(i, got[i - 1], got[i]) for i in range(1, len(got)) if got[i] != (got[i - 1] + 1) & 0xFF]
print('received %d bytes in %.1f s (%.0f B/s), first %s' % (len(got), dt, len(got) / dt, got[:8].hex(' ')))
print('gaps: %d' % len(gaps), gaps[:12])

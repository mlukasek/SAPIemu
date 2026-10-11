"""HW bench: the PicoROM comms channel. Uploads pico.bin (RAM only), opens the comms window at 0400h, resets the
SAPI (/RTL low 0.3 s, then z), reads the banner, sends a text and reads the echo.
python pico_run.py pico.bin [text]"""
import sys
import time

import pypicorom

ROM = sys.argv[1]
TEXT = (sys.argv[2] if len(sys.argv) > 2 else 'Ahoj Martine, tady PicoROM!').encode('ascii')
WINDOW = 0x400

data = open(ROM, 'rb').read()
p = pypicorom.open('SAPI_P28-01')
try:
    p.end_comms()   # a session left open by an earlier run
except Exception:
    pass
p.upload(data, 0x7FF)
p.set_parameter('reset', 'low')   # parameters only without a comms session
time.sleep(0.3)
p.set_parameter('reset', 'z')
p.start_comms(WINDOW)             # the program waits for it (active = 1)


def read_for(seconds):
    got = bytearray()
    end = time.time() + seconds
    while time.time() < end:
        b = p.read(256)
        if b:
            got.extend(b)
    return bytes(got)


print('banner:', read_for(0.5))
t0 = time.time()
p.write(list(TEXT))
echo = read_for(0.5)
print('sent  :', TEXT)
print('echo  :', echo)

# both ways, 2000 bytes: the echo comes in upper case
import random
random.seed(1)
data = bytes(random.choice(b'abcdefghijklmnopqrstuvwxyz0123456789 ') for _ in range(2000))
t0 = time.time()
got = bytearray()
for i in range(0, len(data), 32):
    p.write(list(data[i:i + 32]))
    b = p.read()
    if b:
        got.extend(b)
end = time.time() + 3
while len(got) < len(data) and time.time() < end:
    b = p.read()
    if b:
        got.extend(b)
dt = time.time() - t0
want = data.upper()
bad = sum(1 for x, y in zip(got, want) if x != y)
print('2000 bytes: echoed %d in %.2f s (%.0f B/s each way), differences %d' % (len(got), dt, len(got) / dt, bad))
p.end_comms()

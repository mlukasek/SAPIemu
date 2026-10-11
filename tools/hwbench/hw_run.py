"""HW bench run (docs: memory hw-bench): a timed Logic Pro 16 capture while the PicoROM gets a ROM (RAM only) and
the SAPI is reset (/RTL low 0.3 s, then z); COM13 is read meanwhile. Prints the serial text, the /RES pulse,
the /IOR bursts and the TxD bit time.
python hw_run.py ROM.bin OUTDIR [seconds]"""
import csv
import os
import subprocess
import sys
import threading
import time

import serial
from saleae import automation

PICOROM = 'E:/SAPI_GIT/PicoROM/picorom.exe'
NAME = 'SAPI_P28-01'
COM, BAUD = 'COM13', 9600
ROM, OUT = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
SECONDS = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0
CH = [0, 3, 4, 5]   # /RES, /IOR, TxD, RxD (FI2 left out: 2 MHz would make the export huge)


def picorom(*args):
    r = subprocess.run([PICOROM] + list(args), capture_output=True, text=True, timeout=60)
    print('picorom', ' '.join(args), '->', ((r.stdout + r.stderr).strip().splitlines() or [''])[-1], flush=True)
    if r.returncode:
        raise RuntimeError('picorom failed')


received = bytearray()
stop = threading.Event()


def reader(port):
    while not stop.is_set():
        received.extend(port.read(256))


os.makedirs(OUT, exist_ok=True)
port = serial.Serial(COM, BAUD, timeout=0.1)
th = threading.Thread(target=reader, args=(port,))
th.start()
try:
    with automation.Manager.connect(port=10430) as manager:
        dev = manager.get_devices()[0]
        dcfg = automation.LogicDeviceConfiguration(enabled_digital_channels=CH, digital_sample_rate=50_000_000,
                                                   digital_threshold_volts=3.3)
        ccfg = automation.CaptureConfiguration(capture_mode=automation.TimedCaptureMode(duration_seconds=SECONDS))
        with manager.start_capture(device_id=dev.device_id, device_configuration=dcfg,
                                   capture_configuration=ccfg) as cap:
            print('capture started', flush=True)
            time.sleep(0.3)
            picorom('upload', NAME, ROM, '16KBit')
            picorom('reset', NAME, 'low')
            time.sleep(0.3)
            picorom('reset', NAME, 'z')
            cap.wait()
            cap.export_raw_data_csv(directory=OUT, digital_channels=CH)
            cap.save_capture(filepath=os.path.join(OUT, 'capture.sal'))
    time.sleep(0.5)
finally:
    stop.set()
    th.join()
    port.close()

open(os.path.join(OUT, 'serial.bin'), 'wb').write(received)
print('--- %s received %d bytes:' % (COM, len(received)))
print(received.hex(' ') if any(b < 32 and b not in (10, 13) or b > 126 for b in received) else
      received.decode('ascii'))

rows = list(csv.reader(open(os.path.join(OUT, 'digital.csv'))))[1:]
t = [float(r[0]) for r in rows]
col = lambda k: [int(r[k]) for r in rows]
res, ior, txd, rxd = col(1), col(2), col(3), col(4)
edges = lambda v, to: [i for i in range(1, len(v)) if v[i] == to and v[i - 1] != to]
lows = edges(res, 0)
t0 = t[lows[0]] if lows else 0.0
for i in lows[:3]:
    h = next((j for j in edges(res, 1) if j > i), None)
    print('/RES low at %.6f s for %.2f us' % (t[i], (t[h] - t[i]) * 1e6 if h else -1))
falls = [t[i] - t0 for i in edges(ior, 0)]
bursts = []
for f in falls:
    if f < 0:
        continue
    if bursts and f - bursts[-1][1] < 0.005:
        bursts[-1][1] = f
        bursts[-1][2] += 1
    else:
        bursts.append([f, f, 1])
for b in bursts[:8]:
    print('  /IOR burst %9.3f .. %9.3f ms  %6d reads' % (b[0] * 1e3, b[1] * 1e3, b[2]))
for name, v in (('TxD', txd), ('RxD', rxd)):
    ch = [t[i] for i in range(1, len(v)) if v[i] != v[i - 1]]
    d = sorted(b - a for a, b in zip(ch, ch[1:]))
    print('%s: idle level %d, %d changes, shortest pulses (us): %s' % (name, v[0], len(ch),
          [round(x * 1e6, 1) for x in d[:5]]))

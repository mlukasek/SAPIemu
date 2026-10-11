"""HW bench wiring check with wire.bin (wire.asm): Logic Pro 16 capture of all 16 channels triggered by /RES,
then each channel against what the program does.
python wire_check.py [wire.bin] [OUTDIR]"""
import csv
import os
import subprocess
import sys
import threading
import time

import serial
from saleae import automation

WORK = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'work', 'hwbench')
ROM = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(WORK, 'wire.bin'))
OUT = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else os.path.join(WORK, 'cap_wire'))
PICOROM = 'E:/SAPI_GIT/PicoROM/picorom.exe'
NAMES = ['/RES', 'RDY', '/IOR', '/IOW', 'TxD', 'RxD', '/MR', '/MW', 'STSTB', '/INT0', 'MAP1', 'MAP2', 'CLK',
         '/M1', 'VSYNC', 'D15']


def picorom(*args):
    r = subprocess.run([PICOROM] + list(args), capture_output=True, text=True, timeout=60)
    if r.returncode:
        raise RuntimeError('picorom %s: %s' % (args, r.stdout + r.stderr))


stop = threading.Event()
rx = bytearray()


def sender(port):   # a byte every 3 ms for the receive test, and read what comes back
    while not stop.is_set():
        port.write(b'Z')
        rx.extend(port.read(64))


os.makedirs(OUT, exist_ok=True)
picorom('upload', 'SAPI_P28-01', ROM, '16KBit')   # the old program runs on until the reset
port = serial.Serial('COM13', 9600, timeout=0.003)
th = threading.Thread(target=sender, args=(port,))
th.start()
try:
    with automation.Manager.connect(port=10430) as m:
        dev = m.get_devices()[0]
        dcfg = automation.LogicDeviceConfiguration(enabled_digital_channels=list(range(16)),
                                                   digital_sample_rate=125_000_000, digital_threshold_volts=3.3)
        ccfg = automation.CaptureConfiguration(capture_mode=automation.DigitalTriggerCaptureMode(
            trigger_type=automation.DigitalTriggerType.FALLING, trigger_channel_index=0,
            after_trigger_seconds=0.06, trim_data_seconds=0.062))
        with m.start_capture(device_id=dev.device_id, device_configuration=dcfg, capture_configuration=ccfg) as cap:
            guard = threading.Timer(10.0, cap.stop)   # no /RES (SAPI off?): do not wait for ever
            guard.start()
            time.sleep(1.0)
            picorom('reset', 'SAPI_P28-01', 'low')
            time.sleep(0.3)
            picorom('reset', 'SAPI_P28-01', 'z')
            cap.wait()
            guard.cancel()
            cap.export_raw_data_csv(directory=OUT, digital_channels=list(range(16)))
            cap.save_capture(filepath=os.path.join(OUT, 'capture.sal'))
finally:
    time.sleep(0.2)
    stop.set()
    th.join()
    port.close()

# --- the transitions per channel
f = open(os.path.join(OUT, 'digital.csv'))
r = csv.reader(f)
head = next(r)
first = next(r)
t0row = float(first[0])
level = [int(x) for x in first[1:]]
init = list(level)
edges = [[] for _ in range(16)]   # (time, new level)
for row in r:
    t = float(row[0])
    for k in range(16):
        v = row[k + 1]
        if v != '0' and v != '1':
            continue
        v = int(v)
        if v != level[k]:
            edges[k].append((t, v))
            level[k] = v
res_fall = next((t for t, v in edges[0] if v == 0), None)
T0 = res_fall if res_fall is not None else t0row
print('columns:', head[1:])
print('reset at %.6f s (all times below from the reset)' % T0)


def lows(k):
    """(start, length) of the low pulses of channel k"""
    out, start = [], (t0row if init[k] == 0 else None)
    for t, v in edges[k]:
        if v == 0:
            start = t
        elif start is not None:
            out.append((start - T0, t - start))
            start = None
    return out


def groups(pulses, gap):
    g = []
    for s, ln in pulses:
        if g and s - g[-1][1] < gap:
            g[-1][1] = s
            g[-1][2] += 1
        else:
            g.append([s, s, 1])
    return g


def describe(k, gap=200e-6, n=8):
    p = [x for x in lows(k) if x[0] > -0.0005]
    g = groups(p, gap)
    txt = ', '.join('%d@%.2fms' % (c, a * 1e3) for a, b, c in g[:n])
    return '%-6s D%-2d start %d, %6d edges; low-pulse groups: %s%s' % (NAMES[k], k, init[k], len(edges[k]), txt,
                                                                    ' ...' if len(g) > n else '')


for k in range(16):
    if k in (12, 13, 8):
        continue
    print(describe(k))
# fast signals: rate in the first ms after the reset settles
for k in (12, 13, 8):
    e = [t for t, v in edges[k] if v == 0 and T0 + 0.001 < t < T0 + 0.002]
    print('%-6s D%-2d start %d, %6d edges; %d falling edges in 1 ms (%.3f MHz)' % (NAMES[k], k, init[k],
          len(edges[k]), len(e), len(e) / 1e3))
lo = lows(0)
print('/RES pulse: %s' % ['%.2f us' % (x[1] * 1e6) for x in lo[:2]])
print('serial back from the SAPI: %r' % bytes(rx))

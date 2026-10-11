"""HW bench: tbre.asm on the real board. Logic Pro 16 capture triggered by /RES (D0, D2 /IOR, D3 /IOW, D4 TxD at
125 MS/s), the ROM and the reset through pypicorom, the 99 values over the comms channel; prints each status read
with its time after the last data write.
python tbre_run.py [tbre.bin] [OUTDIR]"""
import csv
import os
import sys
import threading
import time

import pypicorom
from saleae import automation

WORK = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'work', 'hwbench')
ROM = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(WORK, 'tbre.bin'))
OUT = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else os.path.join(WORK, 'cap_tbre'))
CH = [0, 2, 3, 4]
N = 99

os.makedirs(OUT, exist_ok=True)
p = pypicorom.open('SAPI_P28-01')
try:
    p.end_comms()
except Exception:
    pass
p.upload(open(ROM, 'rb').read(), 0x7FF)
vals = bytearray()
with automation.Manager.connect(port=10430) as m:
    dev = m.get_devices()[0]
    dcfg = automation.LogicDeviceConfiguration(enabled_digital_channels=CH, digital_sample_rate=125_000_000,
                                               digital_threshold_volts=3.3)
    ccfg = automation.CaptureConfiguration(capture_mode=automation.DigitalTriggerCaptureMode(
        trigger_type=automation.DigitalTriggerType.FALLING, trigger_channel_index=0,
        after_trigger_seconds=0.05, trim_data_seconds=0.051))
    with m.start_capture(device_id=dev.device_id, device_configuration=dcfg, capture_configuration=ccfg) as cap:
        guard = threading.Timer(10.0, cap.stop)
        guard.start()
        time.sleep(1.0)
        p.set_parameter('reset', 'low')
        time.sleep(0.3)
        p.set_parameter('reset', 'z')
        p.start_comms(0x400)
        end = time.time() + 3
        while len(vals) < N and time.time() < end:
            b = p.read()
            if b:
                vals.extend(b)
        p.end_comms()
        cap.wait()
        guard.cancel()
        cap.export_raw_data_csv(directory=OUT, digital_channels=CH)
open(os.path.join(OUT, 'values.bin'), 'wb').write(vals)
print('values: %d bytes' % len(vals))

rows = list(csv.reader(open(os.path.join(OUT, 'digital.csv'))))[1:]
t = [float(r[0]) for r in rows]
lvl = [[int(r[k + 1]) for r in rows] for k in range(4)]
edge = lambda k, to: [t[i] for i in range(1, len(t)) if lvl[k][i] == to and lvl[k][i - 1] != to]
t0 = edge(0, 0)[0]
iow_end = [x - t0 for x in edge(2, 1)]        # end of each write (rising /IOW)
ior = [x - t0 for x in edge(1, 0)]            # start of each read
tx = [x - t0 for x in edge(3, 0)]
# the status reads of the program: the 96 reads after the last but 2 groups of writes
w = sorted(iow_end)
data_w = w[-3:]                               # 1 write (case 1), 2 writes (case 2)
cases = [(data_w[0], 32, vals[0:32]), (data_w[2], 64, vals[32:96])]
for n, (tw, cnt, v) in enumerate(cases):
    reads = [x for x in ior if x > tw][:cnt]
    print('case %d: last data write ends at %.3f ms' % (n + 1, tw * 1e3))
    prev = None
    for i, (tr, b) in enumerate(zip(reads, v)):
        if b != prev or i == len(v) - 1:
            print('   read %2d  %8.3f us after the write  %02Xh  TBRE %d' % (i, (tr - tw) * 1e6, b, (b >> 6) & 1))
        prev = b
    st = [x for x in tx if x > tw][:1]
    if st:
        print('   TxD start bit at %.3f us after the write' % ((st[0] - tw) * 1e6))
print('IN +3 = %02Xh, IN +0 = %02Xh, IN +1 = %02Xh' % (vals[96], vals[97], vals[98]) if len(vals) >= N else 'short')

"""HW bench: zrdtimer.asm on the real board: the 82C54 counts (as ZRDBENCH computes them) against the analyzer
(time between the OUT (F7h) markers x 4 MHz) in the same run.
python zrdtimer_run.py [zrdtimer.bin] [OUTDIR]"""
import os
import sys
import threading
import time

import pypicorom
from saleae import automation

import bench as B

ROM = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(B.WORK, 'zrdtimer.bin'))
OUT = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else os.path.join(B.WORK, 'cap_zrdtimer'))
os.makedirs(OUT, exist_ok=True)
CH = [B.RES, B.IOW]


def digits(v):
    """the hex digits after the first '*' (stray bytes of the channel dropped)"""
    if b'*' not in v:
        return ''
    return ''.join(chr(x) for x in v[v.index(b'*') + 1:] if chr(x) in '0123456789ABCDEF')

p = pypicorom.open(B.DEVICE)
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
        trigger_type=automation.DigitalTriggerType.FALLING, trigger_channel_index=B.RES,
        after_trigger_seconds=2.0, trim_data_seconds=2.002))
    with m.start_capture(device_id=dev.device_id, device_configuration=dcfg, capture_configuration=ccfg) as cap:
        guard = threading.Timer(15.0, cap.stop)
        guard.start()
        time.sleep(1.0)
        p.set_parameter('reset', 'low')
        time.sleep(0.3)
        p.set_parameter('reset', 'z')
        p.start_comms(0x400)
        end = time.time() + 6
        while len(digits(vals)) < 16:
            if time.time() > end:
                break
            b = p.read()
            if b:
                vals.extend(b)
        p.end_comms()
        cap.wait()
        guard.cancel()
        cap.export_raw_data_csv(directory=OUT, digital_channels=CH)
print('comms bytes: %s' % vals.hex(' '))
d = digits(vals)
if len(d) < 16:
    sys.exit('comms: no complete result')
v = bytes.fromhex(d[:16])
cnt = [v[2 * i] | (v[2 * i + 1] << 8) for i in range(4)]
c = B.load(OUT, CH)
iow = [q for q in B.lows(c, B.IOW) if q[0] > 0]
marks = [s for s, e in iow]
# /IOW after the reset: 4 timer set-up writes; each test: latch (OUT 53h), marker, marker, latch; OUT 63h before
# the CGA tests and at the end
print('/IOW after the reset: %d (expected 22)' % len(marks))
seq = [marks[i] for i in (5, 6, 9, 10, 14, 15, 18, 19)]
clocks = [(seq[2 * i + 1] - seq[2 * i]) * 4e6 for i in range(4)]
names = ['calibration 26 T x 65536', 'LDIR memory -> memory', 'LDIR memory -> CGA-1V', 'LDIR CGA-1V -> memory']
print('82C54 counts: %s' % cnt)
print('%-26s %10s %12s %12s' % ('', 'timer T/unit', 'analyzer T', 'analyzer T/unit'))
for i in range(4):
    t_timer = cnt[i] * 26.0 / cnt[0]
    print('%-26s %10.3f %12.0f %12.3f' % (names[i], t_timer, clocks[i], clocks[i] / 65536.0))
print('CPU clock from the timer calibration: %.4f MHz (counter 55930.4 Hz)' % (1703931 * 55930.4 / cnt[0] / 1e6))

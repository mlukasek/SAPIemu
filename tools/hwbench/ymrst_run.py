"""HW bench: ymrst.asm with several delays after /RES; the analyzer gives the time from the end of /RES to the
first and the last YM write, the status says whether the YM took them.
python ymrst_run.py [delays in DJNZ turns, default 0 4 6 8 9 10 11 12 14 20 30]"""
import os
import sys
import threading
import time

import pypicorom
from saleae import automation

import bench as B

DELAYS = [int(x) for x in sys.argv[1:]] or [0, 4, 6, 8, 9, 10, 11, 12, 14, 20, 30]
ROM = os.path.join(B.WORK, 'ymrst.bin')
OUT = os.path.join(B.WORK, 'cap_ymrst')
CH = [B.RES, B.IOW]


def digits(v):
    if b'*' not in v:
        return ''
    return ''.join(chr(x) for x in v[v.index(b'*') + 1:] if chr(x) in '0123456789ABCDEF')


base = bytearray(open(ROM, 'rb').read())
p = pypicorom.open(B.DEVICE)
print('delay  first YM write  last YM write  status  T1 flag')
for d in DELAYS:
    rom = bytearray(base)
    rom[0x3F0] = d
    try:
        p.end_comms()
    except Exception:
        pass
    p.upload(bytes(rom), 0x7FF)
    vals = bytearray()
    with automation.Manager.connect(port=10430) as m:
        dev = m.get_devices()[0]
        dcfg = automation.LogicDeviceConfiguration(enabled_digital_channels=CH, digital_sample_rate=125_000_000,
                                                   digital_threshold_volts=3.3)
        ccfg = automation.CaptureConfiguration(capture_mode=automation.DigitalTriggerCaptureMode(
            trigger_type=automation.DigitalTriggerType.FALLING, trigger_channel_index=B.RES,
            after_trigger_seconds=0.01, trim_data_seconds=0.012))
        with m.start_capture(device_id=dev.device_id, device_configuration=dcfg, capture_configuration=ccfg) as cap:
            guard = threading.Timer(10.0, cap.stop)
            guard.start()
            time.sleep(1.0)
            p.set_parameter('reset', 'low')
            time.sleep(0.3)
            p.set_parameter('reset', 'z')
            p.start_comms(0x400)
            end = time.time() + 4
            while len(digits(vals)) < 2 and time.time() < end:
                b = p.read()
                if b:
                    vals.extend(b)
            p.end_comms()
            cap.wait()
            guard.cancel()
            cap.export_raw_data_csv(directory=OUT, digital_channels=CH)
    c = B.load(OUT, CH)
    res_end = next(t for t, v in c.edges[B.RES] if v == 1 and t > 0)
    w = [s for s, e in B.lows(c, B.IOW) if s > res_end][:4]
    st = int(digits(vals)[:2], 16) if len(digits(vals)) >= 2 else None
    print('%5d  %10.2f us  %11.2f us  %s  %s' % (d, (w[0] - res_end) * 1e6, (w[3] - res_end) * 1e6,
          ('%02Xh' % st) if st is not None else ' -- ', ('set' if st & 0x40 else 'clear') if st is not None else '?'),
          flush=True)

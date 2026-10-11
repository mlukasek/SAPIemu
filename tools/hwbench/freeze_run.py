"""HW bench: freeze.asm (or freeze_and.asm) on the real board: the INTA with the PC in the card's window, what RDY,
/M1 and /CPU do after it, then a PicoROM reset: does the CPU start again?
python freeze_run.py [freeze|freeze_and]"""
import os
import sys
import threading
import time

from saleae import automation

import bench as B

NAME = sys.argv[1] if len(sys.argv) > 1 else 'freeze'
ROM = os.path.join(B.WORK, NAME + '.bin')
OUT = os.path.join(B.WORK, 'cap_' + NAME)
CPU = 14

c = B.capture(ROM, OUT, after=0.1, send=b'Z')
print('line: %r' % c.serial)
iow = [s for s, e in B.lows(c, B.IOW) if s > 0]
mark = iow[5] if len(iow) > 5 else None
print('/IOW after the reset: %d, the marker at %.4f ms' % (len(iow), mark * 1e3 if mark else -1))
inta = [(s, e) for s, e in B.lows(c, B.M1, mark or 0) if e - s > 800e-9]
if not inta:
    print('no INTA after the marker')
    sys.exit()
t = inta[0][0]
print('INTA (long /M1) at %.4f ms, %.2f ms after the marker' % (t * 1e3, (t - mark) * 1e3))
ev = []
for k, nm, pos in ((B.M1, '/M1', False), (B.STSTB, 'STSTB', True), (B.RDY, 'RDY', False), (B.MR, '/MR', False),
                   (B.MW, '/MW', False), (B.IOR, '/IOR', False), (B.INT0, '/INT0', False), (CPU, '/CPU', False)):
    for s, e in (B.highs if pos else B.lows)(c, k, t - 3e-6, t + 3e-6):
        ev.append((s, e, nm))
for s, e, nm in sorted(ev):
    print('   %-6s %9.3f .. %9.3f us' % (nm, (s - t) * 1e6, (e - t) * 1e6))
end = max(tt for e in c.edges for tt, v in e[-1:]) if any(c.edges) else t
m1_after = [s for s, e in B.lows(c, B.M1, t + 2e-6)]
rdy_ed = [(tt, v) for tt, v in c.edges[B.RDY] if tt > t - 2e-6]
clk_after = len([tt for tt, v in c.edges[B.CLK] if tt > t + 2e-6])
print('after the INTA: /M1 %d times, CLK edges %d, RDY edges %s' % (len(m1_after), clk_after,
      [('%.3f us' % ((tt - t) * 1e6), v) for tt, v in rdy_ed[:6]]))

# a reset through the PicoROM while it hangs
print('--- PicoROM reset while it hangs (timed capture 1.5 s)')
with automation.Manager.connect(port=10430) as m:
    dev = m.get_devices()[0]
    ch = [B.RES, B.RDY, B.M1, B.STSTB]
    dcfg = automation.LogicDeviceConfiguration(enabled_digital_channels=ch, digital_sample_rate=125_000_000,
                                               digital_threshold_volts=3.3)
    ccfg = automation.CaptureConfiguration(capture_mode=automation.TimedCaptureMode(duration_seconds=1.5))
    out2 = OUT + '_reset'
    os.makedirs(out2, exist_ok=True)
    with m.start_capture(device_id=dev.device_id, device_configuration=dcfg, capture_configuration=ccfg) as cap:
        time.sleep(0.4)
        B.picorom('reset', B.DEVICE, 'low')
        time.sleep(0.3)
        B.picorom('reset', B.DEVICE, 'z')
        cap.wait()
        cap.export_raw_data_csv(directory=out2, digital_channels=ch)
c2 = B.load(out2, ch)
res = B.lows(c2, B.RES, -10)
m1 = B.lows(c2, B.M1, -10)
print('/RES low pulses: %d %s' % (len(res), [round((e - s) * 1e6, 2) for s, e in res[:3]]))
print('/M1 cycles in 1.5 s: %d, RDY level at the end: %d, RDY edges: %d' % (len(m1), c2.edges[B.RDY][-1][1] if c2.edges[B.RDY] else c2.init[B.RDY], len(c2.edges[B.RDY])))

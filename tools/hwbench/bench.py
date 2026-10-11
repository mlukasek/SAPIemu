"""HW bench helpers: one capture of all 16 Logic Pro 16 channels (125 MS/s) triggered by /RES while a ROM runs on
the real SAPI (PicoROM upload to RAM, reset through /RTL), optionally sending bytes to COM13; the transitions per
channel for the analysis. Channel wiring: docs/zmeny-podle-hw.md."""
import csv
import os
import subprocess
import threading
import time

import serial
from saleae import automation

PICOROM = 'E:/SAPI_GIT/PicoROM/picorom.exe'
DEVICE = 'SAPI_P28-01'
COM = 'COM13'
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
WORK = os.path.join(ROOT, 'work', 'hwbench')
NAMES = ['/RES', 'RDY', '/IOR', '/IOW', 'TxD', 'RxD', '/MR', '/MW', 'STSTB', '/INT0', 'MAP1', 'MAP2', 'CLK',
         '/M1', 'VSYNC', 'D15']
RES, RDY, IOR, IOW, TXD, RXD, MR, MW, STSTB, INT0, MAP1, MAP2, CLK, M1, VSYNC = range(15)


def picorom(*args):
    r = subprocess.run([PICOROM] + list(args), capture_output=True, text=True, timeout=60)
    if r.returncode:
        raise RuntimeError('picorom %s: %s' % (args, r.stdout + r.stderr))


def capture(rom, out, after=0.06, send=None, rate=125_000_000, analog=None, analog_rate=50_000_000,
            channels=None):
    """Runs `rom` from a reset and records `after` seconds. send: bytes written to COM13 every 3 ms meanwhile.
    analog: channels also recorded in analog (the same probes), exported to analog.csv (load_analog).
    channels: the digital channels to record (default all 16; fewer for long captures).
    Returns Capture with .edges[k] = [(t, level)], .init[k], t = 0 at the falling /RES, .serial = received."""
    out = os.path.abspath(out)   # Logic 2 resolves relative paths against its own folder
    os.makedirs(out, exist_ok=True)
    picorom('upload', DEVICE, os.path.abspath(rom), '16KBit')
    stop = threading.Event()
    rx = bytearray()
    port = serial.Serial(COM, 9600, timeout=0.003)

    def sender():
        while not stop.is_set():
            if send:
                port.write(send)
            rx.extend(port.read(64))

    th = threading.Thread(target=sender)
    th.start()
    try:
        with automation.Manager.connect(port=10430) as m:
            dev = m.get_devices()[0]
            chans = list(channels) if channels else list(range(16))
            dcfg = automation.LogicDeviceConfiguration(enabled_digital_channels=chans,
                                                       digital_sample_rate=rate, digital_threshold_volts=3.3,
                                                       enabled_analog_channels=list(analog or []),
                                                       analog_sample_rate=analog_rate if analog else None)
            ccfg = automation.CaptureConfiguration(capture_mode=automation.DigitalTriggerCaptureMode(
                trigger_type=automation.DigitalTriggerType.FALLING, trigger_channel_index=RES,
                after_trigger_seconds=after, trim_data_seconds=after + 0.002))
            with m.start_capture(device_id=dev.device_id, device_configuration=dcfg,
                                 capture_configuration=ccfg) as cap:
                guard = threading.Timer(10.0, cap.stop)   # no /RES (SAPI off?): do not wait for ever
                guard.start()
                time.sleep(1.0)
                picorom('reset', DEVICE, 'low')
                time.sleep(0.3)
                picorom('reset', DEVICE, 'z')
                cap.wait()
                guard.cancel()
                cap.export_raw_data_csv(directory=out, digital_channels=chans,
                                        analog_channels=list(analog) if analog else None)
                cap.save_capture(filepath=os.path.join(out, 'capture.sal'))
    finally:
        time.sleep(0.2)
        stop.set()
        th.join()
        port.close()
    c = load(out, channels)
    c.serial = bytes(rx)
    return c


class Capture:
    pass


def load(out, channels=None):
    """The transitions of a saved capture (digital.csv), times from the falling /RES. channels: the recorded
    channels in the order of the columns (default all 16); missing ones stay empty."""
    f = open(os.path.join(out, 'digital.csv'))
    r = csv.reader(f)
    head = next(r)
    chans = list(channels) if channels else [int(h.split()[1]) for h in head[1:]]
    first = next(r)
    t_first = float(first[0])
    level = [int(x) for x in first[1:]]
    c = Capture()
    c.init = [1] * 16
    for i, k in enumerate(chans):
        c.init[k] = level[i]
    c.edges = [[] for _ in range(16)]
    for row in r:
        t = float(row[0])
        for i, k in enumerate(chans):
            v = row[i + 1]
            if v != str(level[i]):
                level[i] = int(v)
                c.edges[k].append((t, level[i]))
    t0 = next((t for t, v in c.edges[RES] if v == 0), t_first)
    c.t0 = t0                    # capture time of the falling /RES (for load_analog)
    c.edges = [[(t - t0, v) for t, v in e] for e in c.edges]
    c.serial = b''
    return c


def lows(c, k, t_from=-1.0, t_to=1e9):
    """Low pulses (start, end) of channel k that start inside [t_from, t_to)."""
    out, start = [], (-1e9 if c.init[k] == 0 else None)
    for t, v in c.edges[k]:
        if v == 0:
            start = t
        elif start is not None:
            if t_from <= start < t_to:
                out.append((start, t))
            start = None
    return out


def highs(c, k, t_from=-1.0, t_to=1e9):
    """High pulses (start, end) of channel k (STSTB is a positive pulse)."""
    out, start = [], (-1e9 if c.init[k] == 1 else None)
    for t, v in c.edges[k]:
        if v == 1:
            start = t
        elif start is not None:
            if t_from <= start < t_to:
                out.append((start, t))
            start = None
    return out


def load_analog(out, t0):
    """analog.csv of a capture: {channel: (times from t0, volts)}."""
    f = open(os.path.join(out, 'analog.csv'))
    r = csv.reader(f)
    head = next(r)
    cols = [int(h.split()[1]) for h in head[1:]]
    data = {k: ([], []) for k in cols}
    for row in r:
        t = float(row[0]) - t0
        for i, k in enumerate(cols):
            data[k][0].append(t)
            data[k][1].append(float(row[i + 1]))
    return data

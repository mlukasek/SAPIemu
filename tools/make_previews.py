#!/usr/bin/env python3
"""Preview pictures of the machines for the window "Choose a machine": machines/previews/<id>.png.

Usage: python tools/make_previews.py [--debug] [--instruments] [FILTER...]

Each machine runs in sapiemu-cli as in tools/check_machines.py (its scenario brings the machine to something to
see), over a clean copy of the data (git archive, no work/ as in a fresh install). The screen, or the graphics
named by the scenario ("shot:CGA-1V") or by PREVIEWS below, is saved, scaled down to at most 480 points wide and
compressed. Machines with an instrument (INSTRUMENTS) get the picture of its window in the GUI, in the dark look.
Run again after a machine or its software changes; the pictures are committed.

FILTER: only the machines whose name contains one of the filters; --instruments: only the machines of INSTRUMENTS;
--debug: the debug build (when the release one is older).
"""
import os
import struct
import subprocess
import sys
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_machines as cm  # noqa: E402

ROOT = cm.ROOT
OUT = os.path.join(ROOT, 'machines', 'previews')
MAX_W = 480

# machines with a better picture than the one of their check: (actions, display or None for the console)
PREVIEWS = {
    'sapi1v': (cm.ecmon('3') + ['--load-hex', 'media/cga-1v/castle.hex'] + cm.t('SAVE 64 CASTLE.COM\\r') +
               cm.run(1500) + cm.t('CASTLE\\r') + cm.run(5000), 'CGA-1V'),
    'sapi1v_hry': (cm.ecmon('3') + cm.t('FLAPPY\\r') + cm.run(8000), 'CGA-1V'),
    'zps_ZPS-2_MIKOS_V5': (cm.run(3000) + ['--load-hex', 'media/mikos/BASIC_EXP_V5.0G.hex'] + cm.t('B') + cm.run(3000) +
                           cm.t('CLSG:FOR I=0 TO 319 STEP 8:MOVE I,0:PLOT 319-I,239:NEXT I\\r') + cm.run(8000), 'DGD-1'),
    'zps_ZPS-1': (cm.w('READY', 10000) + cm.t('10 FOR I=1 TO 5\\r') + cm.run(400) + cm.t('20 PRINT I,I*I\\r') + cm.run(400) +
                  cm.t('30 NEXT I\\r') + cm.run(400) + cm.t('RUN\\r') + cm.run(6000), None),
    # SETDBS1: each F / B steps the colour by one (keys typed too fast get lost); foreground 7 -> 14 (yellow),
    # background 0 -> 1 (blue)
    'zps_ZPS-6': (cm.CPM + cm.t('SETDBS1\\r') + cm.run(5000) + sum([cm.t('F') + cm.run(500) for _ in range(7)], []) +
                  cm.t('B') + cm.run(1000), None),
    'jpr1s_bgp1s': (cm.MANIC, 'BGP-1S'),
}

# machines with an instrument: the picture of its window in the GUI (sapiemu --ui-snapshot --ui-crop) after the
# actions, which run in sapiemu-cli and save the state: (actions, window option, window id after ###). The windows
# are drawn in the dark Windows 11 look at 100 % (UI_LOOK), the same on any PC: the window "Choose a machine" shows
# the previews in a dark frame like a monitor.
UI_LOOK = ['--ui-theme', 'dark', '--ui-scale', '1']
INSTRUMENTS = {
    'aps5': (cm.run(5000), '--ui-aps5', 'APS-5: panel and furnaces'),
    'zps_ZPS-2_DPS-1_IMS-2': (cm.run(3000), '--ui-ims2', 'IMS-2 measuring bench'),
    'zps_ZPS-2_MIKOS_K': (cm.run(3000), '--ui-media', 'Media'),
    'zps_ZPS-2_MIKOS_V5_Minigraf': (cm.run(3000) + cm.t('G1800\\r') + cm.run(15000) + cm.t('G1809\\r') + cm.run(150000),
                                    '--ui-plotter', 'Minigraf'),
    # the driver has put the carriage to the left stop and MIKOS shows its dot (about 8 s); PEN, the test .G=181E of
    # docs/navod-xy4130.md: a key for the paper, the whole character set (about 93 x 58 mm in less than 3 minutes)
    'zps_ZPS-2_MIKOS_V5_XY4130': (cm.w('MONITOR V5.0', 15000) + cm.run(500) + cm.plotter('pen') + cm.t('G181E\\r') +
                                  cm.run(3000) + cm.t(' ') + cm.run(185000), '--ui-plotter', 'Minigraf'),
    # READY after the beep, the origin (about 5 s), the button Demo: media/ar1990/ukazka.txt (about 155 s, 170 x 144 mm
    # from x 21, y 27 mm). Then the pen up to x 106, y 88 mm (02H graphics mode, whose origin is the last line of the
    # text: x 21, y 23 mm). The window scrolls to the pen only when it is out of the middle 70 % of the view; with the
    # pen at y 82-95 mm (at 100 %) it scrolls so that the whole drawing is in sight, at the end of the demo (y 27 mm)
    # the top of the frame would be cut off.
    'plotter_ar1990': (cm.run(1000) + cm.plotter('ready') + cm.run(6000) + cm.plotter('demo') + cm.run(170000) +
                       cm.plotter('send', '\\x02M680,520') + cm.run(5000), '--ui-plotter', 'Minigraf'),
    'rmp1_jb_izot': (cm.MACHINES['rmp1_jb_izot'][0], '--ui-media', 'Media'),
}

# the size of the window (pixels at 100 %) where its default one cuts the content: put into the imgui.ini of the run
WINDOW_SIZE = {
    'aps5': (960, 800),           # the whole window with the legend; 960 = 2 x 480: shrunk 2x, sharp
    'rmp1_jb_izot': (820, 600),   # the RPD-1 drives above the RMP-1 tape: its lamps and the blocks of the tape
}


def instrument_picture(cli, gui, sb, rel, name, png, tmp):
    """The window of the instrument as PNG; '' when it went well, else what went wrong."""
    acts, opt, key = INSTRUMENTS[name]
    state = os.path.join(tmp, name + '.sapistate')
    out = cm.cli_run(cli, sb, rel, list(acts) + ['--save-state', state], os.path.join(tmp, name + '.txt'))
    bad = [l for l in out.splitlines() if l.startswith('timeout waiting')]
    if bad or not os.path.exists(state):
        return '; '.join(bad) or ' '.join(out.strip().splitlines()[-1:]) or 'no state saved'
    settings = os.path.join(tmp, name + '_settings.json')
    open(settings, 'w').write('{"guides": {"auto": false}}')   # not a first start, no guide over the window
    if name in WINDOW_SIZE:
        open(settings + '.imgui.ini', 'w').write('[Window][%s]\nPos=20,40\nSize=%d,%d\nCollapsed=0\n' %
                                                 ((key,) + WINDOW_SIZE[name]))
    args = [gui, '--data-root', sb, '--machine', rel, '--no-mcp', '--lang', 'cs', '--settings', settings,
            '--ui-state', state, opt, '--ui-crop', key, '--ui-size', '1400x900'] + UI_LOOK + ['--ui-snapshot', png]
    r = subprocess.run(args, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=600, cwd=sb)
    if 'no window' in r.stderr:   # the whole OS window instead of the instrument
        return r.stderr.strip()
    return ''


def read_png(path):
    """RGB rows of a PNG of the emulator: RGB/RGBA 8 bits or a palette of 1-8 bits, any filter."""
    d = open(path, 'rb').read()
    pos, w, h, depth, ct, idat, pal = 8, 0, 0, 8, 0, b'', b''
    while pos < len(d):
        ln = struct.unpack('>I', d[pos:pos + 4])[0]
        tp = d[pos + 4:pos + 8]
        body = d[pos + 8:pos + 8 + ln]
        if tp == b'IHDR':
            w, h, depth, ct = struct.unpack('>IIBB', body[:10])
        elif tp == b'PLTE':
            pal = body
        elif tp == b'IDAT':
            idat += body
        pos += 12 + ln
    raw = zlib.decompress(idat)
    chans = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ct]
    bits = chans * depth
    stride = (w * bits + 7) // 8
    bpp = max(1, bits // 8)
    prev = bytearray(stride)
    rows = []
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):   # undo the filter
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        prev = line
        out = bytearray()
        if ct == 3 or ct == 0:
            mask = (1 << depth) - 1
            for x in range(w):
                bit = x * depth
                v = (line[bit // 8] >> (8 - depth - bit % 8)) & mask
                if ct == 3:
                    out += pal[v * 3:v * 3 + 3]
                else:
                    g = v * 255 // mask
                    out += bytes((g, g, g))
        else:
            step = chans
            for x in range(w):
                px = line[x * step:x * step + step]
                out += px[:3] if ct in (2, 6) else bytes((px[0], px[0], px[0]))
        rows.append(bytes(out))
    return w, h, rows


def shrink(w, h, rows):
    """Integer box filter down to at most MAX_W wide."""
    f = 1
    while w // f > MAX_W:
        f += 1
    if f == 1:
        return w, h, rows
    nw, nh = w // f, h // f
    out = []
    for y in range(nh):
        r = bytearray(nw * 3)
        for x in range(nw):
            acc = [0, 0, 0]
            for yy in range(y * f, y * f + f):
                src = rows[yy]
                for xx in range(x * f, x * f + f):
                    for c in range(3):
                        acc[c] += src[xx * 3 + c]
            for c in range(3):
                r[x * 3 + c] = acc[c] // (f * f)
        out.append(bytes(r))
    return nw, nh, out


def write_png(path, w, h, rows):
    def chunk(tp, body):
        return struct.pack('>I', len(body)) + tp + body + struct.pack('>I', zlib.crc32(tp + body) & 0xFFFFFFFF)
    raw = b''.join(b'\x00' + r for r in rows)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    open(path, 'wb').write(png)


def sandbox():
    sb = os.path.join(ROOT, 'out', 'previews', time.strftime('%Y%m%d-%H%M%S'))
    os.makedirs(sb)
    arc = subprocess.run(['git', 'archive', 'HEAD', 'roms', 'media', 'machines', 'third_party/fonts'], cwd=ROOT, capture_output=True,
                         check=True).stdout
    open(os.path.join(sb, 'data.tar'), 'wb').write(arc)
    subprocess.run(['tar', '-xf', 'data.tar'], cwd=sb, check=True)
    os.remove(os.path.join(sb, 'data.tar'))
    return sb


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    only_instruments = '--instruments' in sys.argv
    preset = 'windows-debug' if '--debug' in sys.argv else 'windows-release'
    cli = os.path.join(ROOT, 'out', 'build', preset, 'sapiemu-cli.exe')
    if not os.path.exists(cli):
        cli = os.path.join(ROOT, 'out', 'build', 'windows-debug', 'sapiemu-cli.exe')
    gui = os.path.join(os.path.dirname(cli), 'sapiemu.exe')
    sb = sandbox()
    os.makedirs(OUT, exist_ok=True)
    tmp = os.path.join(sb, 'shots')   # the full pictures and the logs stay there
    os.makedirs(tmp)
    done = bad = 0
    for name in sorted(cm.MACHINES):
        if (args and not any(a in name for a in args)) or (only_instruments and name not in INSTRUMENTS):
            continue
        rel = 'machines/' + name.replace('zps_', 'zps/', 1) + '.sapi' if name.startswith('zps_') else 'machines/' + name + '.sapi'
        if not os.path.exists(os.path.join(sb, rel)):
            continue
        if name in PREVIEWS:
            acts, disp = PREVIEWS[name]
        else:
            acts, exp = cm.MACHINES[name]
            shots = [e[5:] for e in exp if e.startswith('shot:')]
            disp = shots[0] if shots else None
        png = os.path.join(tmp, name + '.png')
        out = ''
        if name in INSTRUMENTS:
            err = instrument_picture(cli, gui, sb, rel, name, png, tmp)
            if err:
                print('%-36s FAIL %s' % (name, err[:200]))
                bad += 1
                continue
        else:
            acts = list(acts) + (['--display', disp] if disp else []) + ['--screenshot', png]
            out = cm.cli_run(cli, sb, rel, acts, os.path.join(tmp, name + '.txt'))
        if not os.path.exists(png) or cm.png_colours(png) < 2:
            print('%-36s FAIL (no picture) %s' % (name, out.strip().splitlines()[-1:] if out.strip() else ''))
            bad += 1
            continue
        w, h, rows = shrink(*read_png(png))
        dst = os.path.join(OUT, name + '.png')
        write_png(dst, w, h, rows)
        print('%-36s %dx%d %d B' % (name, w, h, os.path.getsize(dst)))
        done += 1
    print('previews: %d, failed %d' % (done, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())

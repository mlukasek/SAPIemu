#!/usr/bin/env python3
"""Pictures of the GUI in the documentation: docs/img/<name>.png (the user guides docs/navod-*.md, the boards
docs/desky/*.md, docs/sestavy.md and the README).

One recipe per picture in PICTURES: the machine, the actions of sapiemu-cli that bring it to what the text around
the picture describes (saved as a state, as tools/make_previews.py and tools/ui_gallery.py do), what a person would
set by hand in a window meanwhile (a knob, a load: written into the state between two runs of the cli), the
windows of the GUI (sapiemu --ui-snapshot --ui-state, the Windows 11 dark look at 100 %, new settings for every
picture) and the frame: the windows without the menu bar (its speed is "--" in a snapshot), or one window. The PNG
is kept small: a palette when it holds all the colours or changes them only a little (the antialiased text),
compressed by PIL.

Everything runs over a clean copy of the data (git archive of HEAD, as a new installation, no work/), so the working
copies and battery files of work/ stay untouched.

  python tools/docs_images.py [--out DIR] [--work DIR] [--debug] [--keep] [NAME...]

NAME: only the pictures whose name contains one of them. --out: where the pictures go (default docs/img; another
folder to look at them first). --work: the folder of the data copy, the states and the logs (default
out/docs-images/<time>). --debug: the debug build. --keep: also the whole snapshot of each picture in --work.

Run again after a change of the GUI or of a machine, look at every picture (the same story as the text, no local
paths, readable) and commit the ones that are right. The drawings of the plotters (ar1990-*.png, minigraf-kres.png,
navod-minigraf-kres.png) and the logo and icon (tools/make_icon.py) are not made here.
"""
import os
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_machines as cm  # noqa: E402

ROOT = cm.ROOT
LOOK = ['--ui-theme', 'win11-dark', '--ui-scale', '1', '--lang', 'cs']
MAX_MEAN_ERROR = 0.6   # the mean change of a channel (0-255) a palette of 256 colours may make, else RGB
w, t, run = cm.w, cm.t, cm.run


# --- the states ------------------------------------------------------------------------------------------------------

def lines(*text):
    """Lines typed one by one with Enter, each with the time to be taken: the keyboard of MIKOS V4 takes about 12
    characters a second (V5 faster), and BASIC drops a key that comes while it prints (it looks for Ctrl+C)."""
    acts = []
    for line in text:
        acts += t(line + '\\r') + run(800 + 100 * len(line))
    return acts


def aps5_operator():
    """APS-5: the button DEMO of the APS-5 window (src/plant/aps5_demo.cpp, docs/aps5-ukazka.md) as panel keys of
    the cli: calibration, program 1 (20 min to 700 degC, 20 min hold, 20 min down to 300 degC), furnaces 1, 2 and 5
    regulated by it, 4 and 6 heated by hand, the table of the furnaces; then 25 minutes of heating. Each step waits
    for the answer of the firmware as the demo does (the menu stays on the screen: a pause first)."""
    menu = 'ZOBRAZENI TABULKY PORUCH'
    steps = [('1', 'VSECHNY PECE VYPNUTY'), ('2', menu), ('6R', 'U [MV]'), ('9990', menu), ('2', '(1-32)'),
             ('1', 'REZERVA'), ('7R', None)]   # 7R: deletes program 1; empty in a new unit (CHYBA PARAMETRU)
    for i, (minutes, temp) in enumerate(((20, 700), (20, 700), (20, 300))):
        steps += [('3R', 'CAS [MIN]'), (str(minutes), 'TEPLOTA:'), (str(temp), '%d%5d%9d P' % (i + 1, minutes, temp))]
    steps.append(('M', menu))
    for n, tvz in ((1, 6), (2, 9), (5, 8)):   # regulated by program 1
        steps += [('0', '(1-8)'), (str(n), menu), ('8R', 'PTRH30'), ('3', menu), ('4R', 'TVZ [10S] ='),
                  (str(tvz), menu), ('7R', 'TVYP [MIN] ='), ('60', menu), ('9R', '(1-32)'), ('1', menu),
                  ('1', 'AKTUALNI PEC ZAPNUTA'), ('1', menu)]
    for n, tc, power in ((4, 4, 70), (6, 2, 50)):   # by hand
        steps += [('0', '(1-8)'), (str(n), menu), ('8R', 'PTRH30'), (str(tc), menu), ('3', 'P[%] ='),
                  (str(power), menu)]
    steps.append(('11', 'VYVOLANI MENU'))
    acts = w(menu, 20000)
    for keys, expect in steps:
        acts += t(keys + '\\r') + run(2500)
        if expect:
            acts += w(expect, 20000)
        acts += run(400)
    return acts + run(25 * 60 * 1000)


def bench(u1=None, load1=None, u2=None, load2=None):
    """IMS-2: the voltage knobs of the BS 525 and the loads of its outputs, as set in the window (src/plant/bs525.h:
    the block "plant" of the state: started, input, power, then per output u_set, i_lim, meter_a, load as an index
    of BS525_LOADS 0, 1k, 220, 100, 47, 22, 10 ohms)."""
    loads = [0, 1000, 220, 100, 47, 22, 10]

    def patch(data):
        name = b'plant'
        at = data.find(struct.pack('<I', len(name)) + name)
        if at < 0:
            raise RuntimeError('no plant in the state')
        p = at + 4 + len(name) + 4 + 1 + 4 + 1   # name, block length, started, input, power
        for k, (u, load) in enumerate(((u1, load1), (u2, load2))):
            o = p + k * 21   # u_set 8, i_lim 8, meter_a 1, load 4
            if u is not None:
                data[o:o + 8] = struct.pack('<d', u)
            if load is not None:
                data[o + 17:o + 21] = struct.pack('<i', loads.index(load))
        return data
    return patch


def program(path):
    return [l.rstrip('\r') for l in open(os.path.join(ROOT, path), encoding='ascii').read().split('\n') if l.strip()]


HEX = '--load-hex'
READY = w('READY', 10000) + run(1500)   # BASIC reads the keyboard while it prints (Ctrl+C): type after its prompt
BASIC_V5 = [HEX, 'media/mikos/BASIC_EXP_V5.0.hex'] + t('B') + READY
BASIMS = run(3000) + [HEX, 'media/dps-1/BASIMS.hex'] + t('G4400\\r') + READY
G1800 = run(3000) + t('G1800\\r') + run(8000)   # Minigraf: the origin (docs/navod-minigraf.md)
NEW_SHEET = cm.plotter('new-sheet')
SQUARE = ['10 REM CTVEREC 50 MM, KRUZNICE A NAPIS', '20 A=CALL(6189,-80,80)',
          '30 A=CALL(6195,-80,480):A=CALL(6195,-480,480)', '40 A=CALL(6195,-480,80):A=CALL(6195,-80,80)',
          '50 A=CALL(6189,-280,480)', '60 FOR I=0 TO 360 STEP 10:W=I*3.14159/180',
          '70 A=CALL(6195,INT(-280+200*SIN(W)),INT(280+200*COS(W)))', '80 NEXT I',
          '90 A=CALL(6189,-600,80):A=CALL(6231,7,7)', '100 T$="SAPI-1"',
          '110 FOR I=1 TO LEN(T$):A=CALL(6228,ASC(MID$(T$,I,1))):NEXT I', '120 A=CALL(6189,0,0)', '130 END']
LOOP = ['10 IMS# N', '20 IMS# O3;"R4D0E"', '30 IMS# T3', '40 IMS# I A$', '50 U=VAL(MID$(A$,2))', '60 PRINT A$,U',
        '70 GOTO 20']
# KZD-1: BASIC on the cassette and back, a block of memory, the directory (docs/navod-kzd1.md, chapters 1-4)
KZD1 = (run(3000) + t('KI') + w('NAZEV:', 60000) + t('MOJE\\r') + run(5000) + BASIC_V5 +
        lines('10 PRINT "AHOJ ZE ZAZNAMNIKU"', '20 FOR I=1 TO 3:PRINT I*I:NEXT I') + t('SAVE "POZDRAV"\\r') +
        run(30000) + lines('NEW') + t('LOAD ".POZDRAV"\\r') + run(40000) + lines('RUN') + t('BYE\\r') + run(2000) +
        t('KS') + w('NAZEV:', 10000) + t('DATA1\\r') + run(20000) + t('5000 50FF\\r') + run(20000) + t('KD') +
        run(60000))
# DPS-1: BASIMS and the program SMYCKA from the cassette of the machine, PLAY (docs/navod-dps1.md, chapters 2 and 4)
SMYCKA = (run(3000) + ['--tape', 'media/dps-1/BASIMS_IMS2.wav'] + t('KL') + w('NAME', 10000) + t('BASIMS\\r') +
          w('DONE?', 10000) + t('Y') + run(120000) + t('J') + READY + t('LOAD\\r') + run(1500) + t('SMYCKA\\r') +
          run(1500) + t('Y') + run(150000) + t('LIST\\r') + run(4000) + t('RUN\\r') + run(900))
# DSE-1: F fills 5000-50FF, P programs it into the blank 2716 from 0000; 5 s into it (docs/navod-dse1.md, chapter 4)
DSE_P = (cm.DSE + t('F') + w('OBSAH:', 5000) + run(500) + t('5000,50FF,55\\r') + run(1500) + t('P') +
         w('DOLNI ADRESA RAM=', 5000) + run(500) + t('5000\\r') + w('HORNI ADRESA RAM=', 5000) + run(500) + t('50FF\\r') +
         w('DOLNI ADRESA EPROM=', 5000) + run(500) + t('0\\r') + w('VLOZ EPROM', 5000) + run(500) + t(' ') +
         w('2732?', 5000) + run(500) + t('N') + run(5000))

# name: machine (edit: (the machine it is a copy of, [(text, new text)]) in the data copy), cli (the actions; several
# lists with a function between them that changes the state saved by the one before), the options of the GUI, the
# size of its window, the frame ('windows' = all windows below the menu bar with a margin; ('window', key, height,
# width) = the top left part of one window), the windows placed by imgui.ini {key: (x, y, w, h)} (else where the GUI
# puts them: the tool windows go to the right edge, so the size of the main window puts them beside the display),
# relative: the data root given as "." (a window that shows a full path shows it from the data folder, not the
# folder of this run)
FRONT = ['--ui-front', 'screen']   # the display has the keyboard: no hint "Click here and type" over it
NO_DGD = ['--ui-hide-display', 'gfx0']   # View -> Displays: the empty DGD-1 of the machine hidden
PICTURES = {
    # docs/aps5-ukazka.md, docs/desky/APS-5.md, README: the result of the demo after half an hour
    'aps5-pece': dict(
        machine='machines/aps5.sapi', cli=[aps5_operator()], gui=['--ui-aps5'] + FRONT, size='1530x790'),
    # README: BASIMS from the cassette measures in a loop; output I turned down from 12.564 V to 9.5 V and 5 V, a 47 ohm
    # load on it; output II turned up to 12 V with a 10 ohm load limits the current (0.5 A, 5 V)
    'dps1-ims2-mereni': dict(
        machine='machines/zps/ZPS-2_DPS-1_IMS-2.sapi',
        cli=[SMYCKA, bench(u1=9.5, load1=47, u2=12.0, load2=10), run(1500), bench(u1=5.0), run(700)],
        gui=FRONT, size='1180x960'),
    # docs/desky/Minigraf.md, README: the demo of the IQ 151 documentation (docs/navod-minigraf.md, chapter 3)
    'minigraf-ukazka': dict(
        machine='machines/zps/ZPS-2_MIKOS_V5_Minigraf.sapi',
        cli=[G1800 + BASIC_V5 + NEW_SHEET + lines(*program('media/mikos/MINIGRAF_DEMO.bas')) + t('RUN\\r') +
             run(240000)],
        gui=['--ui-plotter'] + FRONT, size='1016x860'),
    # docs/navod-minigraf.md: the square, the circle and the text of chapter 4, then LIST
    'navod-minigraf': dict(
        machine='machines/zps/ZPS-2_MIKOS_V5_Minigraf.sapi',
        cli=[G1800 + BASIC_V5 + NEW_SHEET + lines(*SQUARE) + t('RUN\\r') + run(40000) + t('LIST\\r') + run(4000)],
        gui=['--ui-plotter'] + FRONT, size='1018x600'),
    # docs/navod-xy4130.md: PEN, BASIC and the demo XY4130_DEMO from a program (chapter 3)
    'navod-xy4130': dict(
        machine='machines/zps/ZPS-2_MIKOS_V5_XY4130.sapi',
        cli=[w('MONITOR V5.0', 15000) + run(500) + cm.plotter('pen') + [HEX, 'media/mikos/XY4130_DEMO.hex'] + BASIC_V5 +
             lines('10 REM UKAZKA XY 4130', '20 A=CALL(32768)', '30 END') + t('RUN\\r') + run(30000)],
        gui=['--ui-plotter'] + NO_DGD + FRONT, size='980x700', ini={'Minigraf': (528, 34, 444, 658)}),
    # docs/navod-ar1990.md: READY, the button Demo, the drawing half done
    'navod-ar1990': dict(
        machine='machines/plotter_ar1990.sapi',
        cli=[run(1000) + cm.plotter('ready') + run(6000) + cm.plotter('demo') + run(100000)],
        gui=['--ui-plotter'], size='780x1060', ini={'Minigraf': (8, 34, 764, 1010)}),
    # docs/navod-dps1.md: the loop of chapter 4 typed in, LIST, RUN
    'navod-dps1': dict(
        machine='machines/zps/ZPS-2_DPS-1_IMS-2.sapi',
        cli=[BASIMS + lines(*LOOP) + t('LIST\\r') + run(4000) + t('RUN\\r') + run(1500)], gui=FRONT, size='1180x960'),
    # docs/navod-dse1.md: P programs the blank 2716, the Media window shows LED1, Vpp 25 V and the 50 ms pulse
    'navod-dse1': dict(
        machine='machines/zps/ZPS-2_DSE-1.sapi', cli=[DSE_P], gui=['--ui-media'] + FRONT, size='1170x640',
        ini={'Media': (528, 34, 634, 372)}),
    # docs/navod-kzd1.md: a new cassette moje.kzd, chapters 1 to 4
    'navod-kzd1': dict(
        machine='machines/zps/_docs_kzd1.sapi',
        edit=('machines/zps/ZPS-2_MIKOS_K.sapi', [('work/kzd-1/kazeta.kzd', 'work/kzd-1/moje.kzd')]),
        cli=[KZD1], gui=['--ui-media'] + NO_DGD + FRONT, size='1170x600', ini={'Media': (528, 34, 634, 446)}),
    # docs/navod-rkd1.md: the shell SH after the login of chapter 2 and DIR A:; Media shows the whole path of the fixed
    # disk (the other media by their names): relative, from the data folder
    'navod-rkd1': dict(
        machine='machines/rkd1_tesla_cpm.sapi', cli=[cm.MACHINES['rkd1_tesla_cpm'][0]], gui=FRONT, size='1500x700',
        relative=True),
    # docs/navod-rmp1.md: DIRMT reads the second file of the tape (chapter 1)
    'navod-rmp1': dict(
        machine='machines/rmp1_jb_izot.sapi',
        cli=[cm.CPM + t('B:DIRMT\\r') + w('Paska:', 20000) + t('0\\r') + w('MTDATMAN', 60000) + run(300)],
        gui=FRONT, size='1840x700', ini={'Media': (816, 34, 1016, 640)}),   # unit 1 below: scrolled
    # docs/sestavy.md, docs/navod-dps1.md: the configurator, the DPS-1 in X6 selected: its title, buttons and the rack
    'panel-dps1': dict(
        machine='machines/zps/ZPS-2_DPS-1_IMS-2.sapi', gui=['--ui-rack', '6'], size='1400x900',
        frame=('window', 'rack', 500, 710)),
    # docs/sestavy.md: the configurator, the DPP-1 in X6 selected
    'panel-dpp1': dict(
        machine='machines/zps/ZPS-2_MIKOS_K.sapi', gui=['--ui-rack', '6'], size='1400x900',
        frame=('window', 'rack', 500, 710)),
    # README (earlier): CP/M on the AND-1V, a picture on the CGA-1V (REMBRAN1 of L. Lasota, media/cga-1v)
    'sapi1v-cpm-cga': dict(
        machine='machines/sapi1v.sapi',
        cli=[cm.ecmon('3') + [HEX, 'media/cga-1v/rembran1.hex'] + lines('SAVE 64 REMBRAN1.COM') + t('REMBRAN1\\r') +
             run(6000) + t('DIR *.COM\\r') + run(8000)],
        gui=FRONT, size='1540x600'),
    # README (earlier): RAT PLAYER plays SMJOOP.RAD on the MPH-1V, the configurator with the MPH-1V in X8 beside it;
    # the graphic displays of the machine hidden (View -> Displays) as on the V machine of the picture before
    'sapi1v-rat-sestava': dict(
        machine='machines/sapi1v_hry.sapi', cli=[cm.ecmon('3') + t('RAT SMJOOP.RAD\\r') + run(10000)],
        gui=['--ui-rack', '8', '--ui-hide-display', 'gfx0', '--ui-hide-display', 'gfx1'] + FRONT, size='1440x900',
        ini={'screen': (8, 34, 672, 538), 'rack': (688, 34, 744, 858)}),
    # README (earlier): BASIC EXP V5.0/G draws on the DGD-1
    'zps2-mikos-dgd': dict(
        machine='machines/zps/ZPS-2_MIKOS_V5.sapi',
        cli=[run(3000) + [HEX, 'media/mikos/BASIC_EXP_V5.0G.hex'] + t('B') + READY + lines('CLSG') +
             t('FOR I=0 TO 319 STEP 8:MOVE I,0:PLOT 319-I,239:NEXT I\\r') + run(10000) +
             t('FOR J=0 TO 239 STEP 8:MOVE 0,J:PLOT 319,239-J:NEXT J\\r') + run(10000)],
        gui=FRONT, size='1300x700'),
}


# --- making them -----------------------------------------------------------------------------------------------------

def option(name, default=None):
    if name in sys.argv:
        i = sys.argv.index(name)
        if i + 1 < len(sys.argv):
            return sys.argv[i + 1]
    return default


def sandbox(work):
    """The data of HEAD as a new installation (no work/), the copies of machines the recipes need."""
    sb = os.path.join(work, 'data')
    os.makedirs(sb)
    arc = subprocess.run(['git', 'archive', 'HEAD', 'roms', 'media', 'machines', 'third_party/fonts', 'docs'], cwd=ROOT,
                         capture_output=True, check=True).stdout
    tar = os.path.join(work, 'data.tar')
    open(tar, 'wb').write(arc)
    subprocess.run(['tar', '-xf', 'data.tar', '-C', 'data'], cwd=work, check=True)
    os.remove(tar)
    for pic in PICTURES.values():
        if pic.get('edit'):
            src, changes = pic['edit']
            text = open(os.path.join(sb, src), encoding='utf-8').read()
            for old, new in changes:
                if old not in text:
                    raise SystemExit('%s: no %r' % (src, old))
                text = text.replace(old, new)
            open(os.path.join(sb, pic['machine']), 'w', encoding='utf-8').write(text)
    return sb


def make_state(cli, sb, work, name, pic):
    """The state of the picture: the runs of the cli, each saving the state the next one loads."""
    state, k = None, 0
    for step in pic['cli']:
        if callable(step):   # what a person sets in a window: into the state saved last
            data = bytearray(open(state, 'rb').read())
            open(state, 'wb').write(step(data))
            continue
        k += 1
        nxt = os.path.join(work, '%s_%d.sapistate' % (name, k))
        acts = (['--load-state', state] if state else []) + list(step) + ['--print-screen', '--save-state', nxt]
        args = [cli, '--data-root', '.' if pic.get('relative') else sb, '--machine', pic['machine']] + acts
        r = subprocess.run(args, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=1800,
                           cwd=sb)
        out = r.stdout + r.stderr
        open(os.path.join(work, '%s_%d.txt' % (name, k)), 'w', encoding='utf-8').write(out)
        bad = [l for l in out.splitlines() if l.startswith('timeout waiting')]
        if bad or not os.path.exists(nxt):
            raise RuntimeError('; '.join(bad) or ' '.join(out.strip().splitlines()[-1:]) or 'no state saved')
        state = nxt
    return state


def ini_windows(path):
    """The windows of an imgui.ini: {key: (x, y, w, h)}."""
    wins, key, pos = {}, None, None
    for line in open(path, encoding='utf-8', errors='replace'):
        line = line.strip()
        if line.startswith('[Window]['):
            key, pos = line[9:-1], None
        elif key and line.startswith('Pos='):
            pos = [int(v) for v in line[4:].split(',')]
        elif key and pos and line.startswith('Size='):
            wins[key] = tuple(pos + [int(v) for v in line[5:].split(',')])
    return wins


def snapshot(gui, sb, work, name, pic, state):
    d = os.path.join(work, 'settings', name)
    os.makedirs(d)
    settings = os.path.join(d, 'sapiemu-settings.json')
    open(settings, 'w').write('{"guides": {"auto": false}}')   # not a first start, no guide over the windows
    if pic.get('ini'):   # windows placed: as moved there before (the window id after ###)
        open(settings + '.imgui.ini', 'w').write(''.join('[Window][%s]\nPos=%d,%d\nSize=%d,%d\nCollapsed=0\n\n' %
                                                         ((key,) + tuple(v)) for key, v in pic['ini'].items()))
    png = os.path.join(work, name + '_full.png')
    args = [gui, '--data-root', '.' if pic.get('relative') else sb, '--settings', settings, '--no-mcp',
            '--machine', pic['machine'], '--ui-size', pic['size']] + LOOK + \
        (['--ui-state', state] if state else []) + list(pic.get('gui', [])) + ['--ui-snapshot', png]
    r = subprocess.run(args, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=600, cwd=sb)
    if not os.path.exists(png):
        raise RuntimeError('no snapshot: ' + (r.stderr or '').strip()[-300:])
    ini = settings + '.imgui.ini'
    return png, ini_windows(ini) if os.path.exists(ini) else {}


def frame(im, how, wins, margin=8):
    """'windows': the windows below the menu bar (the work area of the dock space) with a margin of the desktop;
    ('window', key, height, width): that window, its top left part (the configurator draws the rack smaller in a
    narrower window, without the types of the cards)."""
    from PIL import ImageChops
    if how[0] == 'window':
        x, y, ww, wh = wins[how[1]]
        return im.crop((x, y, x + min(ww, how[3]), y + min(wh, how[2])))
    top = next((v[1] for k, v in wins.items() if k.startswith('WindowOverViewport')), 0)
    body = im.crop((0, top, im.width, im.height))
    edge = [body.getpixel((x, y)) for x in range(0, body.width, 7) for y in (0, body.height - 1)]
    desk = max(set(edge), key=edge.count)   # the colour of the desktop behind the windows
    bg = body.copy()
    bg.paste(desk, (0, 0, bg.width, bg.height))
    box = ImageChops.difference(body, bg).getbbox()
    if not box:
        return body
    x0, y0, x1, y1 = box
    return body.crop((max(0, x0 - margin), max(0, y0 - margin), min(body.width, x1 + margin),
                      min(body.height, y1 + margin)))


def save_small(im, path):
    """A palette when 256 colours hold the picture (exactly, or with a small mean change), else RGB; optimized.
    Returns what it chose."""
    from PIL import Image, ImageChops, ImageStat
    im = im.convert('RGB')
    colours = im.getcolors(256)
    if colours:
        pal = Image.new('P', (1, 1))
        flat = [c for _, rgb in colours for c in rgb]
        pal.putpalette(flat + [0] * (768 - len(flat)))
        out, how = im.quantize(palette=pal, dither=Image.Dither.NONE), 'palette %d colours' % len(colours)
    else:
        out = im.quantize(256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        err = max(ImageStat.Stat(ImageChops.difference(im, out.convert('RGB'))).mean)
        how = 'palette 256, mean change %.2f' % err
        if err > MAX_MEAN_ERROR:
            out, how = im, 'RGB'
    out.save(path, optimize=True)
    return how


def main():
    from PIL import Image
    values = [option('--out'), option('--work')]
    only = [a for a in sys.argv[1:] if not a.startswith('--') and a not in values]
    names = [n for n in PICTURES if not only or any(o in n for o in only)]
    preset = 'windows-debug' if '--debug' in sys.argv else 'windows-release'
    cli = os.path.join(ROOT, 'out', 'build', preset, 'sapiemu-cli.exe')
    gui = os.path.join(ROOT, 'out', 'build', preset, 'sapiemu.exe')
    if not os.path.exists(gui) or not os.path.exists(cli):
        raise SystemExit('no %s build (tools/build.cmd)' % preset)
    out = option('--out', os.path.join(ROOT, 'docs', 'img'))
    work = option('--work', os.path.join(ROOT, 'out', 'docs-images', time.strftime('%Y%m%d-%H%M%S')))
    os.makedirs(out, exist_ok=True)
    os.makedirs(work, exist_ok=True)
    sb = sandbox(work)
    print('data', sb)
    bad = 0
    for name in names:
        pic = PICTURES[name]
        try:
            state = make_state(cli, sb, work, name, pic) if pic.get('cli') else None
            png, wins = snapshot(gui, sb, work, name, pic, state)
            im = frame(Image.open(png).convert('RGB'), pic.get('frame', ('windows',)), wins)
            dst = os.path.join(out, name + '.png')
            how = save_small(im, dst)
            if '--keep' not in sys.argv:
                os.remove(png)
            print('%-20s %dx%d %d KB, %s' % (name, im.width, im.height, os.path.getsize(dst) // 1024, how))
        except (RuntimeError, OSError, KeyError, subprocess.TimeoutExpired) as ex:
            bad += 1
            print('%-20s FAIL %s' % (name, ex))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())

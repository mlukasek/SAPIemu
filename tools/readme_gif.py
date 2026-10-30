#!/usr/bin/env python3
"""The animated GIF at the top of the README (docs/img/readme/sapiemu-ukazka.gif, README.en.md
sapiemu-demo-en.gif): Choose a machine -> Start ZPS-1 -> its guide -> Type into SAPI -> the BASIC program runs.

Every key frame is a GUI snapshot of the release sapiemu.exe (--ui-snapshot) over a clean copy of the data (a git
archive of HEAD as tools/docs_images.py makes it, no work/) with new settings per picture (a settings file that does
not exist = the first start). The machine states while the program is typed come from sapiemu-cli (--type as the
guide's button types, --save-state), loaded by --ui-state. Over the pictures only a mouse pointer and a click ring are
drawn, and the speed item of the status bar is left out (a snapshot has run less than the second the first
measurement needs, it would say "-- %"; the UX review: leave it out, do not pretend).

  python tools/readme_gif.py [cs|en ...] [--work DIR] [--colors=128] [--palette=global|local[,...]] [--try]
                             [--install]

Default both languages, one global palette of 128 colours (made from the key frames; half the size of palettes per
frame at the same look, see --palette=global,local --try). --work: the folder of the data copy, the states, the key
frames and the GIFs (default out/readme-gif/<time>): <lang>/*.png (key frames, key_*.png after the status bar fix,
without the pointer), sapiemu-<lang>-<palette>-<colours>.gif. --install copies the GIF to docs/img/readme. Look at
both GIFs before committing them (the pointer on the buttons, the program typed and run).
"""
import json
import os
import shutil
import subprocess
import sys
import time

from PIL import Image, ImageChops, ImageDraw, ImageStat

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import docs_images  # noqa: E402

REPO = docs_images.ROOT
BUILD = os.path.join(REPO, 'out', 'build', 'windows-release')
GUI = os.path.join(BUILD, 'sapiemu.exe')
CLI = os.path.join(BUILD, 'sapiemu-cli.exe')
DATA = None   # the data copy, set by main()
ZPS1 = 'machines/zps/ZPS-1.sapi'
SIZE = (1200, 675)
ACCENT = (76, 194, 255)   # the accent buttons of the Windows 11 dark look
BAR_BG = (32, 32, 32)     # the menu bar
TARGET = {'cs': os.path.join(REPO, 'docs', 'img', 'readme', 'sapiemu-ukazka.gif'),
          'en': os.path.join(REPO, 'docs', 'img', 'readme', 'sapiemu-demo-en.gif')}
PROGRAM = '10 FOR I=1 TO 5\\r20 PRINT I,I*I\\r30 NEXT I\\rRUN\\r'   # the code block of the guide, as its button types it
STEP_MS = 400


def run(args, log):
    r = subprocess.run(args, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=600, cwd=DATA)
    open(log, 'w', encoding='utf-8').write(r.stdout + r.stderr)
    return r


def make_states(out):
    """s0 = READY after power on, tNN = NN x 400 ms after the program started being typed."""
    d = os.path.join(out, 'states')
    os.makedirs(d)
    args = [CLI, '--data-root', DATA, '--machine', ZPS1, '--timeout-ms', '10000', '--wait-text', 'READY',
            '--save-state', os.path.join(d, 's0.sapistate'), '--print-screen', '--type', PROGRAM]
    for i in range(1, 13):
        args += ['--run-ms', str(STEP_MS), '--save-state', os.path.join(d, 't%02d.sapistate' % i), '--print-screen']
    run(args, os.path.join(d, 'cli.txt'))
    return d


def snapshot(out, lang, name, opts, first=False, settings=None):
    sd = os.path.join(out, lang, 'settings', name)
    os.makedirs(sd)
    sf = os.path.join(sd, 'sapiemu-settings.json')
    if not first:
        open(sf, 'w', encoding='utf-8').write(json.dumps(settings or {'guides': {'auto': True}}))
    png = os.path.join(out, lang, name + '.png')
    args = [GUI, '--data-root', DATA, '--settings', sf, '--no-mcp', '--lang', lang, '--ui-theme', 'win11-dark',
            '--ui-size', '%dx%d' % SIZE, '--ui-scale', '1'] + opts + ['--ui-snapshot', png]
    run(args, os.path.join(out, lang, name + '.log'))
    if not os.path.exists(png):
        raise SystemExit('no picture ' + png)
    return Image.open(png).convert('RGB')


def accent_buttons(im, box):
    """Filled accent buttons inside box (x0, y0, x1, y1): their rectangles, found as runs of the accent colour that
    make a solid block (not the window borders, not the check boxes)."""
    x0, y0, x1, y1 = box
    px = im.load()

    def acc(x, y):
        r, g, b = px[x, y]
        return abs(r - ACCENT[0]) < 12 and abs(g - ACCENT[1]) < 12 and abs(b - ACCENT[2]) < 12

    seen = set()
    found = []
    for y in range(y0, y1):
        for x in range(x0, x1):
            if (x, y) in seen or not acc(x, y):
                continue
            # flood fill the accent area
            stack = [(x, y)]
            seen.add((x, y))
            n = 0
            minx, miny, maxx, maxy = x, y, x, y
            while stack:
                cx, cy = stack.pop()
                n += 1
                minx, miny, maxx, maxy = min(minx, cx), min(miny, cy), max(maxx, cx), max(maxy, cy)
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if x0 <= nx < x1 and y0 <= ny < y1 and (nx, ny) not in seen and acc(nx, ny):
                        seen.add((nx, ny))
                        stack.append((nx, ny))
            w, h = maxx - minx + 1, maxy - miny + 1
            if w >= 30 and w >= 1.6 * h and 12 <= h <= 40 and n > 0.5 * w * h:
                found.append((minx, miny, maxx, maxy))
    return found


def aim(r):
    """Where the pointer rests on a button: inside it near the lower right corner, the arrow below the label."""
    return (r[2] - 4, r[3] - 2)


def drop_speed(im):
    """The status bar without its last item (the speed): the items before it moved right to the edge."""
    im = im.copy()
    px = im.load()
    w = im.size[0]
    cols = [max(max(px[x, y]) for y in range(3, 23)) > 70 for x in range(400, w - 3)]
    runs, s = [], None
    for i, c in enumerate(cols + [False]):
        if c and s is None:
            s = i
        if not c and s is not None:
            runs.append((s + 400, i + 400 - 1))
            s = None
    # the status starts after the widest gap (from the menus); items are apart by 8 px or more, words by less
    gaps = [(runs[i][0] - runs[i - 1][1], i) for i in range(1, len(runs))]
    first = max(gaps)[1] if gaps and max(gaps)[0] > 40 else 0
    status = runs[first:]
    items = [[status[0]]]
    for a, b in status[1:]:
        if a - items[-1][-1][1] - 1 >= 8:
            items.append([])
        items[-1].append((a, b))
    if len(items) < 2:
        return im
    left = items[0][0][0]
    keep_end = items[-2][-1][1]
    shift = items[-1][-1][1] - keep_end
    part = im.crop((left, 1, keep_end + 1, 25))
    ImageDraw.Draw(im).rectangle((left, 1, items[-1][-1][1] + 1, 24), fill=BAR_BG)
    im.paste(part, (left + shift, 1))
    return im


# the arrow pointer of Windows (hot spot at 0,0), in pixels at 100 %
ARROW = [(0, 0), (0, 17), (4, 13), (7, 20), (10, 19), (7, 12), (12, 12)]


def with_pointer(im, pos, ring=False):
    if pos is None:
        return im
    k = 4
    x, y = pos
    pad = 24
    box = (x - pad, y - pad, x + pad + 1, y + pad + 1)
    big = Image.new('RGBA', ((2 * pad + 1) * k, (2 * pad + 1) * k), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    if ring:
        r = 10 * k
        c = pad * k
        d.ellipse((c - r, c - r, c + r, c + r), outline=(0, 0, 0, 160), width=4 * k)
        d.ellipse((c - r + k, c - r + k, c + r - k, c + r - k), outline=(255, 255, 255, 255), width=2 * k)
    pts = [((pad + px) * k, (pad + py) * k) for px, py in ARROW]
    d.polygon(pts, fill=(255, 255, 255, 255), outline=(0, 0, 0, 255), width=k)
    small = big.resize((2 * pad + 1, 2 * pad + 1), Image.LANCZOS)
    out = im.copy()
    out.paste(small, box[:2], small)
    return out


def glide(a, b, n):
    """n points from a to b (b included), eased."""
    pts = []
    for i in range(1, n + 1):
        t = i / n
        t = t * t * (3 - 2 * t)
        pts.append((round(a[0] + (b[0] - a[0]) * t), round(a[1] + (b[1] - a[1]) * t)))
    return pts


def frames_for(out, lang, states):
    st = lambda n: ['--machine', ZPS1, '--ui-state', os.path.join(states, n + '.sapistate'), '--ui-guide']
    probe = snapshot(out, lang, '00_probe', [], first=True)
    start = accent_buttons(probe, (SIZE[0] // 3, SIZE[1] // 2, SIZE[0] - 10, SIZE[1] - 10))
    if len(start) != 1:
        raise SystemExit('%s: Start button not found once: %s' % (lang, start))
    p_start = aim(start[0])
    a = snapshot(out, lang, '01_choose', ['--ui-mouse', '%d,%d' % p_start], first=True)
    b = snapshot(out, lang, '02_started', ['--ui-click', '%d,%d' % p_start], first=True)
    typ = accent_buttons(b, (SIZE[0] // 3, 60, SIZE[0] - 10, SIZE[1] - 10))
    if len(typ) != 1:
        raise SystemExit('%s: Type into SAPI not found once: %s' % (lang, typ))
    p_type = aim(typ[0])
    probe = snapshot(out, lang, '03_probe', st('s0'))   # not hovered: the button in its accent colour
    if accent_buttons(probe, (SIZE[0] // 3, 60, SIZE[0] - 10, SIZE[1] - 10)) != typ:
        raise SystemExit('%s: the guide of the state is elsewhere' % lang)
    c = snapshot(out, lang, '03_ready', st('s0') + ['--ui-mouse', '%d,%d' % p_type])
    typing = [snapshot(out, lang, '04_typing_%s' % n, st(n) + ['--ui-front', 'screen', '--ui-mouse', '%d,%d' % p_type])
              for n in ('t02', 't05', 't08')]
    p_rest = (330, 470)   # the pointer rests on the screen of SAPI
    e = snapshot(out, lang, '05_run', st('t10') + ['--ui-front', 'screen', '--ui-mouse', '%d,%d' % p_rest])
    a, b, c, e = (drop_speed(x) for x in (a, b, c, e))
    typing = [drop_speed(x) for x in typing]
    for name, im in (('01', a), ('02', b), ('03', c), ('04a', typing[0]), ('04b', typing[1]), ('04c', typing[2]),
                     ('05', e)):
        im.save(os.path.join(out, lang, 'key_%s.png' % name))

    seq = []   # (image, ms)
    for p in glide(p_rest, p_start, 5):
        seq.append((with_pointer(a, p), 40))
    seq.append((with_pointer(a, p_start), 1500))
    seq.append((with_pointer(a, p_start, ring=True), 200))
    for i in range(1, 3):
        seq.append((with_pointer(Image.blend(a, b, i / 3), p_start), 60))
    seq.append((with_pointer(b, p_start), 400))
    for p in glide(p_start, p_type, 5):
        seq.append((with_pointer(b, p), 40))
    seq.append((with_pointer(c, p_type), 1100))
    seq.append((with_pointer(c, p_type, ring=True), 200))
    for im in typing:
        seq.append((with_pointer(im, p_type), 900))
    for p in glide(p_type, p_rest, 5):
        seq.append((with_pointer(e, p), 40))
    seq.append((with_pointer(e, p_rest), 2800))
    for i in range(1, 5):
        seq.append((with_pointer(Image.blend(e, a, i / 5), p_rest), 70))
    return seq


def error(rgb, pimg):
    """Mean absolute difference per channel between a frame and its palette version."""
    d = ImageChops.difference(rgb, pimg.convert('RGB'))
    st = ImageStat.Stat(d)
    return sum(st.mean) / 3


def save_gif(seq, path, colors, mode):
    """global: one palette for all frames, made from the key frames (the blends take the nearest colours);
    local: every frame its own palette (adaptive, the size of the GIF grows with the local colour tables)."""
    if mode == 'global':
        keys = [im for im, ms in seq if ms >= 400]
        w, h = SIZE
        sheet = Image.new('RGB', (w, h * len(keys)))
        for i, im in enumerate(keys):
            sheet.paste(im, (0, h * i))
        pal = sheet.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        frames = [im.quantize(palette=pal, dither=Image.Dither.NONE) for im, _ in seq]
    else:
        frames = [im.quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
                  for im, _ in seq]
    worst = max(error(im, f) for (im, ms), f in zip(seq, frames) if ms >= 400)
    frames[0].save(path, save_all=True, append_images=frames[1:], duration=[ms for _, ms in seq], loop=0,
                   optimize=False, disposal=1)
    return os.path.getsize(path), sum(ms for _, ms in seq), worst


def main():
    global DATA
    langs = [a for a in sys.argv[1:] if a in ('cs', 'en')] or ['cs', 'en']
    colors, modes = 128, ['global']
    for a in sys.argv[1:]:
        if a.startswith('--colors='):
            colors = int(a.split('=')[1])
        if a.startswith('--palette='):
            modes = a.split('=')[1].split(',')
    if not os.path.exists(GUI) or not os.path.exists(CLI):
        raise SystemExit('no windows-release build (tools/build.cmd release)')
    out = docs_images.option('--work', os.path.join(REPO, 'out', 'readme-gif', time.strftime('%Y%m%d-%H%M%S')))
    os.makedirs(out, exist_ok=True)
    DATA = docs_images.sandbox(out)
    states = make_states(out)
    print('states', states)
    for lang in langs:
        os.makedirs(os.path.join(out, lang))
        seq = frames_for(out, lang, states)
        for mode in modes:
            for c in ([colors] if '--try' not in sys.argv else [64, 96, 128, 192, 256]):
                gif = os.path.join(out, 'sapiemu-%s-%s-%d.gif' % (lang, mode, c))
                size, ms, worst = save_gif(seq, gif, c, mode)
                print('%s %s %3d: %d frames, %.1f s, %.2f MB, worst error %.2f  %s' %
                      (lang, mode, c, len(seq), ms / 1000, size / 1e6, worst, gif))
        if '--install' in sys.argv:
            shutil.copyfile(gif, TARGET[lang])
            print('  ->', TARGET[lang])


if __name__ == '__main__':
    main()

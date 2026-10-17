#!/usr/bin/env python3
"""RAM disk of the machine "SAPI-1 V: hry a dema" (machines/sapi1v_hry.sapi): media/zrmd-1/hry-a-dema.bin.

Usage: python tools/make_hry_disk.py [--check]

The base is the Bad Apple!! disk media/zrmd-1/ba-ramdisk.bin: CP/M (ECMON boots it with choice 3) and BADAPPLE.DAT
at the place the demo reads it from without the directory. The games, the demos of Libor Lasota and the music go
into the free blocks (tools/cpmdisk.py), the rest of the files does not care where it lies.

The sources are the builds of the game repos and the folders of the boards next to SAPIemu (..\\SAPI-FXS4,
..\\SAPI-Flappy, ..\\SAPI-Bomberman, ..\\Games, ..\\SAPI-karty\\MPH-1V) and media/ of this repo; the list is FILES
below. --check only reports where each source is and how big it is. The result is committed (docs/vydani/sestavy.md,
media/README.md); the script is run again when a game gets a new version.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cpmdisk  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UP = os.path.dirname(ROOT)
MPH = os.path.join(UP, 'SAPI-karty', 'MPH-1V')
BASE = os.path.join(ROOT, 'media', 'zrmd-1', 'ba-ramdisk.bin')
OUT = os.path.join(ROOT, 'media', 'zrmd-1', 'hry-a-dema.bin')

# CP/M name, source (.hex/.txt = Intel HEX from 0100h, else the file as it is), what it is
FILES = [
    ('FXS4.COM', os.path.join(UP, 'SAPI-FXS4', 'build', 'fxs4.com'), 'Fuxoft Soundtrack IV (port M. Lukasek)'),
    ('FLAPPY.COM', os.path.join(UP, 'SAPI-Flappy', 'build', 'flappy.com'), 'Flappy pro CGA-1V (port M. Lukasek)'),
    ('FLAPDGD.COM', os.path.join(UP, 'Games', 'flappy_dgd.hex'), 'Flappy pro DGD-1V (port L. Lasota)'),
    ('BOMBER.COM', os.path.join(UP, 'SAPI-Bomberman', 'build', 'bomber.com'), 'Bomberman (port M. Lukasek)'),
    ('BKP.COM', os.path.join(UP, 'Games', 'Bratrstvo', 'bkp.com'), 'Bratrstvo kocici pracky (R. Borik, SAPI -LAILA-)'),
    ('Y1STDEMO.COM', os.path.join(ROOT, 'media', 'mph-1v', 'y1stdemo_2025_10_26.hex'), 'demo YM3812 (L. Lasota)'),
    ('SNDDEMON.COM', os.path.join(MPH, 'snddemon.hex'), 'zvukove demo (L. Lasota)'),
    ('MPHTEST.COM', os.path.join(ROOT, 'media', 'mph-1v', 'mph_test_0v99.hex'), 'test karty MPH-1V (L. Lasota)'),
    ('SPLAY.COM', os.path.join(ROOT, 'media', 'mph-1v', 'splay.hex'), 'prehravac VGM (L. Lasota)'),
    ('RAT.COM', os.path.join(ROOT, 'media', 'mph-1v', 'rat_0_99_93.txt'), 'prehravac RAD (L. Lasota)'),
    ('CGATEST.COM', os.path.join(ROOT, 'media', 'cga-1v', 'cga_test.hex'), 'test karty CGA-1V (L. Lasota)'),
    ('CASTLE.COM', os.path.join(ROOT, 'media', 'cga-1v', 'castle.hex'), 'obrazek hradu na CGA-1V (L. Lasota)'),
    ('REMBRAN1.COM', os.path.join(ROOT, 'media', 'cga-1v', 'rembran1.hex'), 'Rembrandt, EGA (L. Lasota)'),
    ('REMBRAN2.COM', os.path.join(ROOT, 'media', 'cga-1v', 'rembran2.hex'), 'Rembrandt, CGA (L. Lasota)'),
]
# the tunes of SPLAY (VGM converted by L. Lasota) and RAT (RAD v1 modules of 3eality.com)
VGM = ['birdie', 'bubble', 'demon', 'dinoisle', 'fury', 'hellfire', 'monkisla', 'olmang', 'supaplex', 'tyrian', 'vimana']
for v in VGM:
    FILES.append((v.upper() + '.VGM', os.path.join(MPH, 'SPLAY', v + '.hex'), 'skladba pro SPLAY'))
RAD = [('Void - Raster.rad', 'VOIDRAST.RAD'), ('Void - Terrania.rad', 'VOIDTERR.RAD'), ('adlib2.rad', 'ADLIB2.RAD'),
       ('blue_ad.rad', 'BLUEAD.RAD'), ('ergos.rad', 'ERGOS.RAD'), ('hubbard.rad', 'HUBBARD.RAD'),
       ('mop32k.rad', 'MOP32K.RAD'), ('ooahh.rad', 'OOAHH.RAD'), ('ork.rad', 'ORK.RAD'), ('pipintr2.rad', 'PIPINTR2.RAD'),
       ('rip1.rad', 'RIP1.RAD'), ('rip2.rad', 'RIP2.RAD'), ('smjoop.rad', 'SMJOOP.RAD'), ('sunny_ad.rad', 'SUNNYAD.RAD'),
       ('zap.rad', 'ZAP.RAD')]
for src, name in RAD:
    FILES.append((name, os.path.join(MPH, 'RATplayer', src), 'skladba pro RAT'))


def hex2bin(path):
    """An Intel HEX program from 0100h as a .COM file."""
    mem = {}
    for line in open(path, errors='replace'):
        line = line.strip()
        if not line.startswith(':'):
            continue
        n, a, t = int(line[1:3], 16), int(line[3:7], 16), int(line[7:9], 16)
        if t == 1:
            break
        if t == 0:
            for i in range(n):
                mem[a + i] = int(line[9 + 2 * i:11 + 2 * i], 16)
    if not mem or min(mem) != 0x100:
        raise SystemExit('%s: not a program from 0100h' % path)
    return bytes(mem.get(a, 0) for a in range(0x100, max(mem) + 1))


def load(path):
    if path.endswith('.hex') or path.endswith('.txt'):
        return hex2bin(path)
    return open(path, 'rb').read()


def main():
    missing = [src for _, src, _ in FILES if not os.path.exists(src)]
    for _, src, _ in FILES:
        print('%-8s %s' % ('ok' if os.path.exists(src) else 'MISSING', os.path.relpath(src, ROOT)))
    if missing:
        raise SystemExit('%d sources missing' % len(missing))
    if '--check' in sys.argv:
        return
    img = bytearray(open(BASE, 'rb').read())
    for name, src, what in FILES:
        data = load(src)
        blocks = cpmdisk.add(img, data, name)
        print('%-13s %6d B  %3d blocks from %d  %s' % (name, len(data), len(blocks), blocks[0], what))
    open(OUT, 'wb').write(img)
    print('%s: free %d KB' % (os.path.relpath(OUT, ROOT), (cpmdisk.DSM + 1 - len(cpmdisk.used_blocks(img))) * 4))


if __name__ == '__main__':
    main()

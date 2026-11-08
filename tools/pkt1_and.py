"""ROM BIOS of the PKT-1 for the displays AND-1Z and AND-1V: roms/pkt-1/PKT-1_ROMBIOS_AND.bin.

    python tools/pkt1_and.py [--src DIR] [--m80 DIR] [--cpmrun PATH]

Takes the ROM BIOS of the SAPI-1.5 in the version of Melsoft (pkt1-rombios.asm in ..\\SAPI-karty\\PKT-1\\System, byte
exact to ROMBIOS.MAC of the disks 7 and 8), replaces its screen driver for the DBS-1 (6845, colour attributes, video
RAM paged in at B000h) with tools/pkt1_and.mac (video RAM at F800h, 64 x 20, the same control codes, a cursor in
software) and assembles it with M80/L80 under cpmrun (built with the emulator). The CP/M of the Melsoft disks needs no
change: its CBIOS calls only the vectors at F000h. The build files are in out/pkt1_and/.
"""
import glob
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DRIVER = os.path.join(ROOT, 'tools', 'pkt1_and.mac')
OUT_ROM = os.path.join(ROOT, 'roms', 'pkt-1', 'PKT-1_ROMBIOS_AND.bin')
BUILD = os.path.join(ROOT, 'out', 'pkt1_and')
NAME = 'PKT1AND'
BANNER = 'PKT-1 62K MON-V100690*LCH'
BANNER_NEW = 'PKT-1 62K MON-V100690*LCH/AND'
TERMINAL = 'TERMINAL DBS-1 + CONSUL 262.3'
TERMINAL_NEW = 'TERMINAL AND-1Z/1V + CONSUL 262.3'


def find_dir(arg, what, rel):
    for d in ([arg] if arg else []) + [os.path.join(ROOT, '..', r) for r in rel]:
        if d and os.path.exists(os.path.join(d, what)):
            return d
    sys.exit('%s not found, give the folder on the command line' % what)


def find_cpmrun(arg):
    if arg:
        return arg
    found = sorted(glob.glob(os.path.join(ROOT, 'out', 'build', '*', 'cpmrun*')), key=os.path.getmtime, reverse=True)
    found = [f for f in found if os.path.basename(f) in ('cpmrun', 'cpmrun.exe')]
    if not found:
        sys.exit('cpmrun not found (build the emulator), give --cpmrun')
    return found[0]


def run(cpmrun, args, stdin=''):
    r = subprocess.run([cpmrun] + args, cwd=BUILD, input=stdin.encode('ascii'), stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=120)
    return r.stdout.decode('latin1')


def load_hex(path):
    mem = {}
    for line in open(path, 'rb').read().decode('latin1').splitlines():
        line = line.strip()
        if not line.startswith(':'):
            continue
        n, addr, typ = int(line[1:3], 16), int(line[3:7], 16), int(line[7:9], 16)
        if typ == 1 or n == 0:
            break
        for i in range(n):
            mem[addr + i] = int(line[9 + 2 * i:11 + 2 * i], 16)
    return mem


def replace_once(text, old, new):
    if text.count(old) != 1:
        sys.exit('pkt1-rombios.asm: expected exactly one %r' % old)
    return text.replace(old, new)


def splice(src, driver):
    """The source for M80 with the new driver: LF line ends here, CRLF is made by the caller."""
    lines = src.split('\n')
    a = [i for i, l in enumerate(lines) if 'vystup znaku na konzolu - doska DBS-1' in l]
    b = [i for i, l in enumerate(lines) if l.startswith('TCONS:')]
    if len(a) != 1 or len(b) != 1 or not lines[a[0] - 1].startswith(';---'):
        sys.exit('pkt1-rombios.asm: the DBS-1 driver section not found as expected')
    lines = lines[:a[0] - 1] + driver.rstrip('\n').split('\n') + lines[b[0]:]
    text = '\n'.join(lines)
    text = replace_once(text, 'AS\tEQU\t1', 'AS\tEQU\t0')
    text = replace_once(text, BANNER, BANNER_NEW)
    text = replace_once(text, TERMINAL, TERMINAL_NEW)
    if len(re.findall(r'\tCALL\tVDRI0[^\n]*\n', text)) != 1:
        sys.exit('pkt1-rombios.asm: CALL VDRI0 not found')
    text = re.sub(r'\tCALL\tVDRI0[^\n]*\n', '', text)
    # the cursor blinks where the drive motors time out: every 16 x 256 calls of CONS0
    stop = re.compile(r'(\tCALL\tDSTP0[^\n]*\n)(CONS1:)')
    if len(stop.findall(text)) != 1:
        sys.exit('pkt1-rombios.asm: CALL DSTP0 before CONS1 not found')
    text = stop.sub(r'\1\tCALL\tBLIK0\t\t; blikanie kurzora AND\n\2', text)
    # the rest of the EPROM stays FFh (no padding to a fixed size, the size is checked below)
    pad = re.compile(r'\tDS\t07FFH-\(MOVERL\+ROMSZ\)[^\n]*\n\tDB\t255[^\n]*\n')
    if len(pad.findall(text)) != 1:
        sys.exit('pkt1-rombios.asm: the padding at the end not found')
    return pad.sub('', text)


def main():
    a = sys.argv[1:]
    opt = lambda n: a[a.index(n) + 1] if n in a else None
    src = find_dir(opt('--src'), 'pkt1-rombios.asm', [os.path.join('SAPI-karty', 'PKT-1', 'System')])
    m80 = find_dir(opt('--m80'), 'M80.COM', [os.path.join('SAPI-karty', 'ZRMD-1', 'SRC')])
    cpmrun = find_cpmrun(opt('--cpmrun'))
    shutil.rmtree(BUILD, ignore_errors=True)
    os.makedirs(BUILD)
    for f in ('M80.COM', 'L80.COM'):
        shutil.copy2(os.path.join(m80, f), BUILD)
    data = open(os.path.join(src, 'pkt1-rombios.asm'), 'rb').read().decode('cp1250', 'replace')
    text = splice(data.replace('\r\n', '\n').replace('\r', '\n'), open(DRIVER, 'rb').read().decode('ascii').replace('\r\n', '\n'))
    text = ''.join(c if ord(c) < 128 else '?' for c in text)   # M80 reads 7-bit ASCII (only comments are affected)
    open(os.path.join(BUILD, NAME + '.MAC'), 'wb').write(text.replace('\n', '\r\n').encode('ascii') + b'\x1a')
    out = run(cpmrun, ['M80', '=%s/L' % NAME])
    if 'No Fatal error(s)' not in out:
        sys.exit('M80:\n' + out)
    out = run(cpmrun, ['L80', '/P:0,%s,%s/N/X/E' % (NAME, NAME)], 'N\n')
    hexf = os.path.join(BUILD, NAME + '.HEX')
    if not os.path.exists(hexf) or '?' in out.replace('(Y or N)?', ''):
        sys.exit('L80:\n' + out)
    rom = bytearray([0xFF]) * 2048
    used = 0
    for addr, v in load_hex(hexf).items():
        if addr >= 2048:
            sys.exit('the code does not fit into the 2 KB EPROM (byte at %04Xh)' % addr)
        rom[addr] = v
        used = max(used, addr + 1)
    open(OUT_ROM, 'wb').write(rom)
    print('%s: %d bytes used, %d free' % (os.path.relpath(OUT_ROM, ROOT), used, 2048 - used))


if __name__ == '__main__':
    main()

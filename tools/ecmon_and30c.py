"""ECMON for the display AND-30C (MC6845, 80 x 21): roms/jpr-1zi/JPR-1ZI_ECMON_2k_AND-30C.bin.

    python tools/ecmon_and30c.py [--src DIR] [--cpmrun PATH]

Takes the monitor ECMON "JPR-1Z 62K MON CZ 05082015JB" of J. Biba (ECMONM.MAC, the sources of the system
ZRMD-1 next to SAPIemu: ..\\SAPI-karty\\ZRMD-1\\SRC, see docs/software/ECMON.md), replaces its screen driver for the
AND-1Z (64 x 20) with tools/ecmon_and30c.mac (80 x 21, 6845 at FFE0h/FFF0h, the same control codes), adds the set-up
of the 6845 to CINIT and assembles it with M80/L80 under cpmrun (built with the emulator). The CP/M of the RAM disk
needs no change: its CBIOS calls CONOUT of the monitor. The build files are in out/ecmon_and30c/.
"""
import glob
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DRIVER = os.path.join(ROOT, 'tools', 'ecmon_and30c.mac')
OUT_ROM = os.path.join(ROOT, 'roms', 'jpr-1zi', 'JPR-1ZI_ECMON_2k_AND-30C.bin')
BUILD = os.path.join(ROOT, 'out', 'ecmon_and30c')
NAME = 'ECMON30C'
STACK_ROOM = 64   # bytes left under F800h for the stack of the monitor (LXI SP,F800h)


def find_src(arg):
    for d in ([arg] if arg else []) + [os.path.join(ROOT, '..', 'SAPI-karty', 'ZRMD-1', 'SRC'),
                                          os.path.join(ROOT, '..', 'ZRMD-1', 'SRC')]:
        if d and os.path.exists(os.path.join(d, 'ECMONM.MAC')):
            return d
    sys.exit('ECMONM.MAC not found (..\\SAPI-karty\\ZRMD-1\\SRC), give --src')


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


def splice(src_text, driver):
    lines = src_text.splitlines()
    a = [i for i, l in enumerate(lines) if 'OVLADAC DISPLEJE AND-1Z' in l]
    b = [i for i, l in enumerate(lines) if 'OVLADAC KLAVESNICE CONSUL' in l]
    if len(a) != 1 or len(b) != 1 or not lines[a[0] - 2].startswith(';****') or not lines[b[0] - 2].startswith(';****'):
        sys.exit('ECMONM.MAC: the AND-1Z driver section not found as expected')
    out = lines[:a[0] - 2] + driver.splitlines() + [';'] + lines[b[0] - 2:]
    text = '\r\n'.join(out) + '\r\n'
    old = '\tLXI\tH,VRAM\r\n\tSHLD\tCUR.ADR\r\n'
    if text.count(old) != 1:
        sys.exit('ECMONM.MAC: CINIT (LXI H,VRAM / SHLD CUR.ADR) not found')
    text = text.replace(old, '\tCALL\tCRTINI\t\t;6845 AND-30C\r\n' + old)
    # CONST blinked the cursor character of the AND-1Z every 16 x 256 calls: the 6845 blinks its cursor itself
    blink = re.compile(r'\tLDA\tBLINK\t[^\r\n]*\r\n(?:[^\r\n]*\r\n){7}\tCZ\tQBLINK\r\n')
    if len(blink.findall(text)) != 1:
        sys.exit('ECMONM.MAC: the cursor blinking in CONST not found')
    return blink.sub(';KURZOR BLIKA 6845 (AND-30C)\r\n', text)


def main():
    a = sys.argv[1:]
    src = find_src(a[a.index('--src') + 1] if '--src' in a else None)
    cpmrun = find_cpmrun(a[a.index('--cpmrun') + 1] if '--cpmrun' in a else None)
    shutil.rmtree(BUILD, ignore_errors=True)
    os.makedirs(BUILD)
    for f in ('M80.COM', 'L80.COM'):
        shutil.copy2(os.path.join(src, f), BUILD)
    text = splice(open(os.path.join(src, 'ECMONM.MAC'), 'rb').read().decode('latin1').replace('\r\n', '\n')
                  .replace('\n', '\r\n'), open(DRIVER, 'rb').read().decode('latin1').replace('\r\n', '\n'))
    open(os.path.join(BUILD, NAME + '.MAC'), 'wb').write(text.encode('latin1'))
    out = run(cpmrun, ['M80', '=%s/L' % NAME])
    if 'No Fatal error(s)' not in out:
        sys.exit('M80:\n' + out)
    out = run(cpmrun, ['L80', '/P:0,%s,%s/N/X/E' % (NAME, NAME)], 'N\n')
    hexf = os.path.join(BUILD, NAME + '.HEX')
    if not os.path.exists(hexf) or '?' in out.replace('(Y or N)?', ''):
        sys.exit('L80:\n' + out)
    prn = open(os.path.join(BUILD, NAME + '.PRN'), 'rb').read().decode('latin1')
    m = re.search(r'([0-9A-F]{4})\s+MONIT\$SIZE\s+EQU', prn)
    c = re.search(r'([0-9A-F]{4})\s+CHECK\s+EQU', prn)
    if not m or not c:
        sys.exit('MONIT$SIZE or CHECK not in the listing')
    size, check = int(m.group(1), 16), int(c.group(1), 16)
    if 0x16 + size > 2048:
        sys.exit('the monitor does not fit into the 2 KB EPROM: %d bytes too many' % (0x16 + size - 2048))
    if check + 3 > 0xF800 - STACK_ROOM:
        sys.exit('the monitor leaves less than %d bytes for its stack (signature at %04Xh)' % (STACK_ROOM, check))
    rom = bytearray([0xFF]) * 2048
    for addr, v in load_hex(hexf).items():
        if addr >= 2048:
            sys.exit('code outside the EPROM at %04Xh' % addr)
        rom[addr] = v
    open(OUT_ROM, 'wb').write(rom)
    print('%s: monitor %04Xh bytes (F000-%04X), signature at %04Xh, %d bytes of stack, EPROM %d bytes free'
          % (os.path.relpath(OUT_ROM, ROOT), size, 0xF000 + size - 1, check, 0xF800 - check - 3, 2048 - 0x16 - size))


if __name__ == '__main__':
    main()

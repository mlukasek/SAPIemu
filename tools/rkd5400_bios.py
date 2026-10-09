"""Boot disk of the CP/M of J. Biba with the RKD-1 and the Bulgarian drive CM 5400 (docs/desky/RKD-1.md).

Not an original: no CP/M for the RKD-1 with the CM 5400 was found, so the BIOS media/rkd-1/cm5400/BIRKD54.MAC
is made from the 3K BIOS of J. Biba (tools/rkd5400_bios_src.py writes it from the sources in SAPI_DOKU).
This script assembles it with M80 and L80 under cpmrun and puts it on the boot disk:
  - the files of media/sapiman/r1_zrmd4m_271111.img (the boot disk of rkd1_jb_cm5400.sapi before),
  - the system tracks 0 and 1 and the 6 BIOS sectors of track 76 of media/sapiman/r3_mfc512x8_261211.img (the
    3K system of J. Biba: loader bo2608CE, CCP CE00h, BDOS D606h), the BIOS put in (E400h-EA7Fh).
The loader reads track 0 sectors 2-26 and track 1 sectors 1-26 from CE00h on, then sectors 18, 24, 4, 10, 16,
22 of track 76 (never allocated with DSM 242) from E780h on.

python tools/rkd5400_bios.py [--m80 DIR]    (DIR with M80.COM, L80.COM; default ../SAPI-karty/ZRMD-1/SRC)
Writes media/rkd-1/jb_cm5400_boot.img, keeps the listing in media/rkd-1/cm5400/BIRKD54.PRN."""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'media', 'rkd-1', 'cm5400', 'BIRKD54.MAC')
FILES = os.path.join(ROOT, 'media', 'sapiman', 'r1_zrmd4m_271111.img')
SYS3K = os.path.join(ROOT, 'media', 'sapiman', 'r3_mfc512x8_261211.img')
OUT = os.path.join(ROOT, 'media', 'rkd-1', 'jb_cm5400_boot.img')
T76 = [18, 24, 4, 10, 16, 22]
LOAD_END = 0xCE00 + (25 + 26 + 6) * 128   # EA80h


def slot(addr):
    """the (track, sector id, offset) where the loader puts the byte of `addr`"""
    slots = [(0, i) for i in range(2, 27)] + [(1, i) for i in range(1, 27)] + [(76, i) for i in T76]
    t, sid = slots[(addr - 0xCE00) // 128]
    return t, sid, (addr - 0xCE00) % 128


def cpmrun_exe():
    for preset in ('windows-release', 'windows-debug', 'linux-release', 'linux-debug'):
        for name in ('cpmrun.exe', 'cpmrun'):
            p = os.path.join(ROOT, 'out', 'build', preset, name)
            if os.path.exists(p):
                return p
    sys.exit('cpmrun not built (tools\\build.cmd release)')


def main():
    m80 = os.path.join(os.path.dirname(ROOT), 'SAPI-karty', 'ZRMD-1', 'SRC')
    if len(sys.argv) > 2 and sys.argv[1] == '--m80':
        m80 = sys.argv[2]
    tmp = tempfile.mkdtemp(prefix='rkd5400_')
    for f in ('M80.COM', 'L80.COM'):
        shutil.copy(os.path.join(m80, f), tmp)
    shutil.copy(SRC, os.path.join(tmp, 'BIRKD54.MAC'))
    run = cpmrun_exe()
    a = subprocess.run([run, '-d', tmp, 'M80', '=BIRKD54/L'], stdin=subprocess.DEVNULL, capture_output=True, text=True)
    if 'No Fatal error' not in a.stdout:
        sys.exit('M80:\n' + a.stdout)
    l = subprocess.run([run, '-d', tmp, 'L80', 'BIRKD54,BIRKD54/N/E'], stdin=subprocess.DEVNULL, capture_output=True,
                       text=True)
    print('L80:', l.stdout.strip().splitlines()[-1])
    bios = open(os.path.join(tmp, 'BIRKD54.COM'), 'rb').read()
    if bios[:3] == b'\xC3\x00\xE4':   # L80 puts JP E400H at 0100H
        bios = bios[3:]
    shutil.copy(os.path.join(tmp, 'BIRKD54.PRN'), os.path.join(os.path.dirname(SRC), 'BIRKD54.PRN'))
    shutil.rmtree(tmp)
    n = LOAD_END - 0xE400
    if any(bios[n:]):
        sys.exit('the BIOS has code or data beyond %04Xh: the loader does not load it' % (LOAD_END - 1))
    img = bytearray(open(FILES, 'rb').read())
    sysimg = open(SYS3K, 'rb').read()
    assert len(img) == len(sysimg) == 77 * 26 * 128
    img[0:2 * 26 * 128] = sysimg[0:2 * 26 * 128]
    for sid in T76:
        o = (76 * 26 + sid - 1) * 128
        img[o:o + 128] = sysimg[o:o + 128]
    for k in range(min(n, len(bios))):
        t, sid, o = slot(0xE400 + k)
        img[(t * 26 + sid - 1) * 128 + o] = bios[k]
    open(OUT, 'wb').write(img)
    print('%s: BIOS E400h-%04Xh (%d bytes of %d loaded)' % (os.path.relpath(OUT, ROOT), LOAD_END - 1,
                                                           len(bios.rstrip(b'\0')), n))


main()

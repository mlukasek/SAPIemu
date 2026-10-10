"""Makes the fixed copy of the tape copy program MTCOPY of J. Biba (MTCOPY V041287JB) from the untouched disk
media/rmp-1/a_lad_izo.img:

    python tools/mtcopy_oprava.py [--m80 DIR]    (DIR with M80.COM, L80.COM; default ../SAPI-karty/ZRMD-1/SRC)

Writes media/rmp-1/MTCOPYO.COM and media/rmp-1/a_lad_izo_opraveny.img, the disk with MTCOPYO.COM in place of
MTCOPY.COM (the disk has 2K free only).

The bug (docs/desky/RMP-1.md): MTCOPY.COM is linked with an older tape library MTKAN that writes the last, short
block of a file at the close but does not count it, so the block count in EOF1 is one less. The MTKAN.MAC on the
same disk (V060588JB, the one MTDAT.COM is linked with) adds it ("pripocti posledni blok").

The fix keeps the code of MTCOPY (0100h-3D6Fh) as it is and replaces the library from 3D70h on (MTKAN with the
driver RMP.REL), as if MTCOPY were linked again with the newer MTKAN. Its other changes against the old one are
the ones MTDAT has: 8 read retries instead of 5, HDR2 column 37 a blank instead of 'A', no check of 'B' in HDR2
column 39 at the read, error 30 instead of 26 at a close without an open file. The references from MTCOPY into
the library (14x CALL MTKAN, NA_MT_OP_FIL, SZ_RD_BLOCK, the record size) are set to the new addresses. The menu
title says OPRAVENY so the copy is recognisable on screen.

M80 and L80 take 6 characters of a global name: SZ_RD_BLOCK and SZ_RD_REC of MTKAN are both SZ_RD_ for them
("Mult. Def. Global"), so the record size is renamed to RD_REC_SZ in the copy that is assembled (the code is the
same). MTCOPY.MAC on the disk is reverse engineered and does not give the same bytes, so it is not used."""
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'media', 'rmp-1', 'a_lad_izo.img')
OUT_COM = os.path.join(ROOT, 'media', 'rmp-1', 'MTCOPYO.COM')
OUT_IMG = os.path.join(ROOT, 'media', 'rmp-1', 'a_lad_izo_opraveny.img')
NEW_NAME = 'MTCOPYO.COM'

LIB = 0x3D70   # the library in MTCOPY.COM starts here (CALL MTKAN goes to 3D70h)
# the library symbols in the original MTCOPY.COM and where MTCOPY uses them (address of the 16-bit operand)
REFS = [
    ('MTKAN', 0x3D70, [0x010E, 0x0162, 0x017E, 0x01A0, 0x01EB, 0x0212, 0x0266, 0x02A6, 0x031E, 0x033D, 0x0352,
                       0x0376, 0x038B, 0x044A]),
    ('NA_MT_OP_FIL', 0x4AC9, [0x01AC, 0x021E]),
    ('SZ_RD_BLOCK', 0x4ACB, [0x01B3, 0x0225]),
    ('RD_REC_SZ', 0x4ACD, [0x01BB, 0x022D]),
]
OLD_TITLE = b'\x1d\x1f\n===  F U N K C E   M T C O P Y  ==='
NEW_TITLE = b'\x1d\x1f\n===  FUNKCE  MTCOPY (OPRAVENY)  ==='

# 8" single density CP/M disk (IBM 3740): 77 x 26 x 128 B, 2 system tracks, skew 6, 1K blocks, 64 entries
SKEW = [1, 7, 13, 19, 25, 5, 11, 17, 23, 3, 9, 15, 21, 2, 8, 14, 20, 26, 6, 12, 18, 24, 4, 10, 16, 22]
DSM = 242   # last block


def sector_offset(n):   # logical 128-byte sector n from the start of track 2
    t = 2 + n // 26
    return (t * 26 + SKEW[n % 26] - 1) * 128


def read_block(img, b):
    return b''.join(img[sector_offset(b * 8 + i):sector_offset(b * 8 + i) + 128] for i in range(8))


def write_block(img, b, data):
    data = data.ljust(1024, b'\x1a')
    for i in range(8):
        o = sector_offset(b * 8 + i)
        img[o:o + 128] = data[i * 128:i * 128 + 128]


def directory(img):
    d = read_block(img, 0) + read_block(img, 1)
    return [bytearray(d[e * 32:e * 32 + 32]) for e in range(64)]


def write_directory(img, ents):
    d = b''.join(ents)
    write_block(img, 0, d[:1024])
    write_block(img, 1, d[1024:])


def entry_name(x):
    n = bytes(c & 0x7F for c in x[1:9]).decode().strip()
    e = bytes(c & 0x7F for c in x[9:12]).decode().strip()
    return n + '.' + e


def read_file(img, name):
    ex = sorted((x[12], x[15], [b for b in x[16:32] if b]) for x in directory(img)
                if x[0] != 0xE5 and entry_name(x) == name)
    if not ex:
        sys.exit('%s not on the disk' % name)
    data = b''
    for _, rc, al in ex:
        data += b''.join(read_block(img, b) for b in al)[:rc * 128]
    return data


def delete_file(img, name):
    ents = directory(img)
    for x in ents:
        if x[0] != 0xE5 and entry_name(x) == name:
            x[0] = 0xE5
    write_directory(img, ents)


def add_file(img, name, data):
    ents = directory(img)
    if any(x[0] != 0xE5 and entry_name(x) == name for x in ents):
        sys.exit('%s is on the disk already' % name)
    used = {0, 1}
    for x in ents:
        if x[0] != 0xE5:
            used.update(b for b in x[16:32] if b)
    free = [b for b in range(DSM + 1) if b not in used]
    recs = (len(data) + 127) // 128
    nblk = (recs + 7) // 8
    if nblk > len(free):
        sys.exit('no room on the disk')
    blocks = free[:nblk]
    for k, b in enumerate(blocks):
        write_block(img, b, data[k * 1024:k * 1024 + 1024])
    slots = [k for k, x in enumerate(ents) if x[0] == 0xE5]
    n, e = name.split('.')
    for k in range((nblk + 15) // 16):   # one entry = one 16K extent
        if not slots:
            sys.exit('no free directory entry')
        x = bytearray(32)
        x[1:9] = n.ljust(8).encode()
        x[9:12] = e.ljust(3).encode()
        x[12] = k
        x[15] = min(128, recs - k * 128)
        al = blocks[k * 16:k * 16 + 16]
        x[16:16 + len(al)] = bytes(al)
        ents[slots.pop(0)] = x
    write_directory(img, ents)


def cpmrun_exe():
    for preset in ('windows-release', 'windows-debug', 'linux-release', 'linux-debug'):
        for name in ('cpmrun.exe', 'cpmrun'):
            p = os.path.join(ROOT, 'out', 'build', preset, name)
            if os.path.exists(p):
                return p
    sys.exit('cpmrun not built (tools\\build.cmd release)')


def build_library(img, m80):
    """MTKAN.MAC (record size renamed) + RMP.REL linked at 3D70h; returns the COM and the symbol addresses."""
    tmp = tempfile.mkdtemp(prefix='mtcopy_')
    for f in ('M80.COM', 'L80.COM'):
        shutil.copy(os.path.join(m80, f), tmp)
    src = read_file(img, 'MTKAN.MAC')
    if b'V060588JB' not in src or src.count(b'RD_REC_SZ'):
        sys.exit('MTKAN.MAC is not the expected V060588JB')
    open(os.path.join(tmp, 'MTKAN.MAC'), 'wb').write(src.replace(b'SZ_RD_REC', b'RD_REC_SZ'))
    open(os.path.join(tmp, 'RMP.REL'), 'wb').write(read_file(img, 'RMP.REL'))
    # a module that gives the addresses of the library symbols (at 0200h)
    adr = '\t.Z80\r\n\tEXT\t' + ','.join(r[0] for r in REFS) + '\r\n\tASEG\r\n\tORG\t200H\r\n'
    adr += ''.join('\tDW\t%s\r\n' % r[0] for r in REFS) + '\tEND\r\n\x1a'
    open(os.path.join(tmp, 'ADR.MAC'), 'wb').write(adr.encode())
    run = cpmrun_exe()
    for mod in ('MTKAN', 'ADR'):
        a = subprocess.run([run, '-d', tmp, 'M80', '=' + mod], stdin=subprocess.DEVNULL, capture_output=True,
                           text=True)
        if 'No Fatal error(s)' not in a.stdout:
            sys.exit('M80 %s:\n%s' % (mod, a.stdout))
    l = subprocess.run([run, '-d', tmp, 'L80', 'ADR,/P:%X,MTKAN,RMP,LIB/N/E' % LIB], stdin=subprocess.DEVNULL,
                       capture_output=True, text=True)
    if 'Mult. Def.' in l.stdout or 'Undefined' in l.stdout or not os.path.exists(os.path.join(tmp, 'LIB.COM')):
        sys.exit('L80:\n' + l.stdout)
    com = open(os.path.join(tmp, 'LIB.COM'), 'rb').read()
    shutil.rmtree(tmp)
    syms = {}
    for k, r in enumerate(REFS):
        syms[r[0]] = com[0x100 + 2 * k] | com[0x101 + 2 * k] << 8
    return com, syms


def counter_increments(code):
    """LD HL,(nn) / INC HL / LD (nn),HL with the same nn, by address."""
    hits = {}
    for m in re.finditer(rb'\x2a(..)\x23\x22(..)', code, re.S):
        if m.group(1) == m.group(2):
            a = int.from_bytes(m.group(1), 'little')
            hits[a] = hits.get(a, 0) + 1
    return hits


def main():
    m80 = os.path.join(os.path.dirname(ROOT), 'SAPI-karty', 'ZRMD-1', 'SRC')
    if len(sys.argv) > 2 and sys.argv[1] == '--m80':
        m80 = sys.argv[2]
    img = bytearray(open(SRC, 'rb').read())
    assert len(img) == 77 * 26 * 128
    orig = read_file(img, 'MTCOPY.COM')
    if len(orig) != 21632 or orig[LIB - 0x100:LIB - 0x100 + 4] != b'\xdd\xe5\xfd\xe5':
        sys.exit('MTCOPY.COM is not the expected V041287JB')
    prog = bytearray(orig[:LIB - 0x100])
    for name, old, at in REFS:
        for a in at:
            if prog[a - 0x100] | prog[a - 0xFF] << 8 != old:
                sys.exit('MTCOPY.COM: %04Xh does not refer to %s' % (a, name))
    if prog.count(OLD_TITLE) != 1 or len(OLD_TITLE) != len(NEW_TITLE):
        sys.exit('MTCOPY.COM: the menu title not found')

    com, syms = build_library(img, m80)
    if syms['MTKAN'] != LIB:
        sys.exit('MTKAN linked at %04Xh, not %04Xh' % (syms['MTKAN'], LIB))
    for name, old, at in REFS:
        for a in at:
            prog[a - 0x100] = syms[name] & 0xFF
            prog[a - 0xFF] = syms[name] >> 8
    k = prog.find(OLD_TITLE)
    prog[k:k + len(NEW_TITLE)] = NEW_TITLE
    lib = com[LIB - 0x100:]
    new = bytes(prog) + lib
    new = new[:(len(new) + 127) // 128 * 128].ljust((len(new) + 127) // 128 * 128, b'\0')

    # the old library counts the blocks once, the new one twice (a full block and the last one at the close)
    cnt_old, cnt_new = counter_increments(orig[LIB - 0x100:]), counter_increments(lib)
    blk = [a for a in cnt_old if cnt_old[a] == 1 and cnt_new.get(a + syms['RD_REC_SZ'] - 0x4ACD) == 2]
    if len(blk) != 1:
        sys.exit('the block counter increments not as expected: %s / %s' % (cnt_old, cnt_new))

    open(OUT_COM, 'wb').write(new)
    delete_file(img, 'MTCOPY.COM')
    add_file(img, NEW_NAME, new)
    if read_file(img, NEW_NAME) != new:
        sys.exit('the disk does not read back')
    open(OUT_IMG, 'wb').write(img)
    print('%s: %d bytes, library %04Xh-%04Xh, symbols %s' % (os.path.relpath(OUT_COM, ROOT), len(new), LIB,
          LIB + len(lib.rstrip(b'\0')) - 1, ' '.join('%s=%04Xh' % (n, a) for n, a in syms.items())))
    print('%s: %s with %s in place of MTCOPY.COM' % (os.path.relpath(OUT_IMG, ROOT), os.path.basename(SRC),
                                                      NEW_NAME))


main()

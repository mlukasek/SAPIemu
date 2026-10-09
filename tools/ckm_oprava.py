"""Makes media/mikos/CKM_opraveny.hex, the patched copy of the copy program CKM (TESLA ELTOS DIZ,
24. 3. 1988), from the untouched original media/mikos/CKM.hex:

    python tools/ckm_oprava.py

The bug (docs/desky/KZD-1.md): after a read from the KZD-1, DIRSTP stores 0 into the MIKOS/K block
length DEBL (427Fh) instead of FFh (MIKOS/K does XRA A, CMA, STA DEBL; CKM lacks the CMA). Every
later write to the KZD-1 then makes 1-byte blocks and runs out of tape.

The fix keeps every address: at 47D4h XRA A (AFh) becomes SBB A (9Fh). Just before it, ANI 0CH and
CPI 0CH with JZ to the recorder branch leave A < 0Ch on the KZD-1 branch, so the carry is always set
and SBB A gives FFh. The title line at 4467h says OPRAVENY so the copy is recognisable on screen.

The script checks the original bytes first and writes the records in the original layout (CRLF)."""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'media', 'mikos', 'CKM.hex')
DST = os.path.join(ROOT, 'media', 'mikos', 'CKM_opraveny.hex')

OLD_TITLE = b'\r\n\n\nKOPIROVACI PROGRAM  MGF - KZD-1\r\n\x8a'
NEW_TITLE = b'\r\nOPRAVENY KOPIR. PROGRAM MGF-KZD-1\r\n\x8a'

# (address, original bytes, patched bytes)
PATCHES = [
    (0x47D4, bytes([0xAF, 0x32, 0x7F, 0x42]), bytes([0x9F, 0x32, 0x7F, 0x42])),  # SBB A; STA DEBL
    (0x4467, OLD_TITLE, NEW_TITLE),
]


def main():
    assert len(OLD_TITLE) == len(NEW_TITLE)
    with open(SRC, 'rb') as f:
        lines = f.read().split(b'\r\n')
    mem = {}
    for line in lines:
        if line.startswith(b':'):
            b = bytes.fromhex(line[1:].decode('ascii'))
            if b[3] == 0:
                a = b[1] << 8 | b[2]
                for i in range(b[0]):
                    mem[a + i] = b[4 + i]
    for addr, old, new in PATCHES:
        cur = bytes(mem.get(addr + i, -1) & 0xFF for i in range(len(old)))
        if cur != old:
            sys.exit('CKM.hex at %04Xh is %s, expected %s' % (addr, cur.hex(' '), old.hex(' ')))
        for i, v in enumerate(new):
            mem[addr + i] = v
    out = []
    for line in lines:
        if line.startswith(b':'):
            b = bytearray(bytes.fromhex(line[1:].decode('ascii')))
            if b[3] == 0:
                a = b[1] << 8 | b[2]
                for i in range(b[0]):
                    b[4 + i] = mem[a + i]
                b[-1] = (-sum(b[:-1])) & 0xFF
            line = b':' + bytes(b).hex().upper().encode('ascii')
        out.append(line)
    with open(DST, 'wb') as f:
        f.write(b'\r\n'.join(out))
    print('written', os.path.relpath(DST, ROOT))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Generate a SUBSTITUTE character generator image for the DBS-1A colour display board.

THIS IS NOT A DUMP. The real character generator of the SAPI-1.5 DBS-1A board
(Tesla M2732A EPROM labelled "59", 4 KB) has not been preserved. This script
builds a clearly marked replacement so the board can be emulated:

    roms/dbs-1a/DBS-1A_CharGen_nahrada.bin   (exactly 4096 bytes)

Board addressing (2732, jumper XT5 27-28, the standard setting):
    EPROM A0-A2  = 6845 RA0-RA2 (scan line within the character)
    EPROM A3     = character code bit 7
    EPROM A4-A10 = character code bits 0-6
    EPROM A11    = 6845 RA3
    address = (ra & 7) | ((c >> 7) << 3) | ((c & 0x7F) << 4) | ((ra >> 3) << 11)
Each byte is one pixel row, bit 7 = leftmost pixel, 1 = lit (foreground).
Character cell 8 x 12: scan lines 0-11 are used, 12-15 are 0. In 80-column
mode only bits 7..2 are shown, so text glyphs stay in bits 7..3 (as in the
source font) and bits 1..0 are the inter-character gap in 40-column mode.

Character set (Tesla manual: "KOI-8 cs2 plus semigraphic and special symbols";
ROM BIOS comments: 20-7Fh ASCII, 80-BFh semigraphics, C0-FFh KOI-8 cs letters):
    00-1F  blank
    20-7E  ASCII glyphs from the source font
    7F     checkerboard over the whole 8 x 12 cell
    80-BF  2x3 mosaic semigraphics (ASSUMPTION, see below)
    C0-FF  Czech letters, KOI-8 cs2 order, glyphs from the source font

Sources:
    roms/and-1z-89/AND-1Z_CharGen.bin  (AND-1Z-89 character generator, SAPI-1 period
        font, 6 x 12 cell: address code*16 + L, L = 0..11 where L = 11 is the TOP
        line, i.e. stored bottom-up; pixels D7..D3/D2; same code layout as above,
        C0-FF in KOI-8 cs2 order). Rows are flipped into top-down scan line order.
    src/core/charset.cpp, table k_czech: the Unicode letter of each code C0-FF
        (that table was read off the same AND-1Z ROM). Used only for labels and
        a consistency check in --show; the AND-1Z order already is KOI-8 cs2, so
        no remapping is needed. Codes the AND-1Z ROM leaves undefined (a solid
        6-pixel block there) become a solid 8-pixel block here (FFh is the
        MikroMon cursor).

SEMIGRAPHICS ASSUMPTION: code 80h + n is a 2x3 mosaic, bit 0 = top-left,
bit 1 = top-right, bit 2 = middle-left, bit 3 = middle-right, bit 4 = bottom-left,
bit 5 = bottom-right. Each block is 4 pixels wide (bits 7..4 / 3..0) and 4 scan
lines high (0-3 / 4-7 / 8-11). This follows the AND-1Z bit order (whose blocks
are 3 x 4 in a 6-pixel cell), the real DBS-1A shapes are unknown.

Usage:
    python tools/make_dbs1a_font.py                 write the image
    python tools/make_dbs1a_font.py --show 41 g ý   print glyphs as ASCII art
        (CODE = hex number like 41, 9A, 0xBF, 80h, or a single character)
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_FONT = os.path.join(ROOT, "roms", "and-1z-89", "AND-1Z_CharGen.bin")
CHARSET = os.path.join(ROOT, "src", "core", "charset.cpp")
OUT = os.path.join(ROOT, "roms", "dbs-1a", "DBS-1A_CharGen_nahrada.bin")

LINES = 12  # visible scan lines of the cell


def address(code, ra):
    return (ra & 7) | ((code >> 7) << 3) | ((code & 0x7F) << 4) | ((ra >> 3) << 11)


def load_czech_table():
    """Return list of 64 strings (letter per code C0-FF) from charset.cpp."""
    with open(CHARSET, encoding="utf-8") as f:
        text = f.read()
    m = re.search(r"k_czech\[64\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        sys.exit("k_czech table not found in " + CHARSET)
    body = re.sub(r"//[^\n]*", "", m.group(1))
    items = re.findall(r'"([^"]*)"', body)
    if len(items) != 64:
        sys.exit("k_czech: expected 64 entries, got %d" % len(items))
    return items


def source_glyph(src, code):
    """AND-1Z glyph as 12 rows top-down (L = 11 is the top line)."""
    return [src[code * 16 + (11 - r)] for r in range(LINES)]


def build_glyphs():
    with open(SRC_FONT, "rb") as f:
        src = f.read()
    if len(src) != 4096:
        sys.exit("unexpected size of " + SRC_FONT)
    glyphs = [[0] * LINES for _ in range(256)]
    for c in range(0x20, 0x7F):
        glyphs[c] = source_glyph(src, c)
    glyphs[0x7F] = [0xAA if r % 2 == 0 else 0x55 for r in range(LINES)]
    for n in range(64):
        rows = []
        for band in range(3):
            v = 0
            if n >> (band * 2) & 1:
                v |= 0xF0
            if n >> (band * 2 + 1) & 1:
                v |= 0x0F
            rows += [v] * 4
        glyphs[0x80 + n] = rows
    for c in range(0xC0, 0x100):
        g = source_glyph(src, c)
        if all(b == 0xFC for b in g):   # undefined position in the AND-1Z ROM
            g = [0xFF] * LINES
        glyphs[c] = g
    return glyphs


def build_image(glyphs):
    img = bytearray(4096)
    for c in range(256):
        for ra in range(16):
            img[address(c, ra)] = glyphs[c][ra] if ra < LINES else 0
    return img


def parse_code(s):
    if len(s) == 1:
        o = ord(s)
        if o < 0x80:
            return o
        czech = load_czech_table()
        if s in czech:
            return 0xC0 + czech.index(s)
        sys.exit("character %r is not in the KOI-8 cs2 table" % s)
    t = s.lower()
    if t.endswith("h"):
        t = t[:-1]
    return int(t, 16)


def label(code, czech):
    if code < 0x20:
        return "control (blank)"
    if code == 0x5E:
        return "up arrow (AND-1Z glyph for '^')"
    if code < 0x7F:
        return repr(chr(code))
    if code == 0x7F:
        return "checkerboard"
    if code < 0xC0:
        return "semigraphics n=%02Xh" % (code - 0x80)
    return "'%s' (charset.cpp)" % czech[code - 0xC0]


def show(img, codes):
    czech = load_czech_table()
    for code in codes:
        print("%02Xh %s" % (code, label(code, czech)))
        for ra in range(16):
            b = img[address(code, ra)]
            print("  %2d %s" % (ra, "".join("#" if b >> (7 - i) & 1 else "." for i in range(8))))
        print()


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    glyphs = build_glyphs()
    img = build_image(glyphs)
    args = sys.argv[1:]
    if args and args[0] == "--show":
        show(img, [parse_code(a) for a in args[1:]])
        return
    if args:
        sys.exit(__doc__)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(img)
    print("wrote %s (%d bytes)" % (OUT, len(img)))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Icon of SAPIemu: src/res/sapiemu.ico (the exe, sizes 16-256) and docs/img/ikona.png (256 px,
the window icon on Linux).

Usage: python tools/make_icon.py   (needs Pillow and a bold sans font: Arial Bold or DejaVu Sans Bold)

A dark blue screen with "SAPI" in green phosphor letters and an orange "-1" below; 16 and 24 px get a
simpler picture ("S1"), the four letters are not readable there.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = ["C:/Windows/Fonts/arialbd.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"]
BG = (24, 38, 72)
EDGE = (120, 140, 180)
GREEN = (110, 235, 120)
ORANGE = (255, 150, 40)


def font(size):
    for f in FONTS:
        if os.path.exists(f):
            return ImageFont.truetype(f, size)
    sys.exit("no bold font found (" + ", ".join(FONTS) + ")")


def centred(d, y, text, f, fill, w):
    box = d.textbbox((0, 0), text, font=f)
    d.text(((w - (box[2] - box[0])) / 2 - box[0], y - box[1]), text, font=f, fill=fill)


def picture(size):
    big = 1024 if size >= 32 else 256   # drawn larger and scaled down: smooth edges
    s = big / 256.0
    im = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((8 * s, 8 * s, 248 * s, 248 * s), radius=36 * s, fill=BG, outline=EDGE, width=int(8 * s))
    if size >= 32:
        f = font(int(86 * s))
        centred(d, 40 * s, "SAPI", f, GREEN, big)
        centred(d, 140 * s, "-1", font(int(80 * s)), ORANGE, big)
    else:
        f = font(int(150 * s))
        box = d.textbbox((0, 0), "S", font=f)
        d.text((40 * s - box[0], 54 * s - box[1]), "S", font=f, fill=GREEN)
        d.text((140 * s - box[0], 54 * s - box[1]), "1", font=f, fill=ORANGE)
    return im.resize((size, size), Image.LANCZOS)


def main():
    sizes = [16, 24, 32, 48, 64, 128, 256]
    pics = [picture(n) for n in sizes]
    ico = os.path.join(ROOT, "src", "res", "sapiemu.ico")
    os.makedirs(os.path.dirname(ico), exist_ok=True)
    pics[-1].save(ico, format="ICO", sizes=[(n, n) for n in sizes], append_images=pics[:-1])
    png = os.path.join(ROOT, "docs", "img", "ikona.png")
    pics[-1].save(png, optimize=True)
    print(ico)
    print(png)


if __name__ == "__main__":
    main()

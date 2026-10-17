#!/usr/bin/env python3
"""Icon of SAPIemu: src/res/sapiemu.ico (the exe, sizes 16-256) and docs/img/ikona.png (256 px,
the window icon on Linux); the logo "SAPI 1" docs/img/logo.png (About, README).

Usage: python tools/make_icon.py   (needs Pillow; the logo a bold sans font: Arial Bold or DejaVu Sans Bold)

The digit "1" of the SAPI-1 logo (Tesla manuals): a track of parallel lines around a skeleton (the stem
from the sharp tip down to the base, the flag from the tip to the rounded hook, the wide base with rounded
ends): the outline, a middle line and the centre line, which is a single line in the base. The skeleton
was measured on a photo of the logo (units of that photo). Smaller sizes have fewer lines: from 48 px all
five across the stem, 24 and 32 px the outline and the centre, 16 px a solid digit.
"""
import math
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

T = (510.0, 193.0)   # the tip of the skeleton (the stem and the flag meet here)
H = (340.0, 278.0)   # the end of the flag (rounded hook)
B = (510.0, 520.0)   # the foot of the stem, on the base
L = (245.0, 520.0)   # the ends of the base (rounded)
R = (795.0, 520.0)
S = 32.5             # spacing of the lines; the half width of the digit is 2 * S
X0, Y0, X1, Y1 = 180.0, 85.0, 860.0, 585.0   # bounding box of the outline
BG = (20, 20, 20, 255)
FG = (245, 245, 245, 255)


def unit(a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    n = math.hypot(dx, dy)
    return dx / n, dy / n


def segment(d, a, b, r, fill):
    ux, uy = unit(a, b)
    nx, ny = -uy * r, ux * r
    d.polygon([(a[0] + nx, a[1] + ny), (b[0] + nx, b[1] + ny), (b[0] - nx, b[1] - ny), (a[0] - nx, a[1] - ny)],
              fill=fill)


def disc(d, p, r, fill):
    d.ellipse((p[0] - r, p[1] - r, p[0] + r, p[1] + r), fill=fill)


def layer(d, r, fill, tf):
    """The digit as a stroke of half width r around the skeleton, with the sharp tip."""
    t, h, b, l, rr = (tf(p) for p in (T, H, B, L, R))
    segment(d, t, b, r, fill)
    segment(d, t, h, r, fill)
    segment(d, l, rr, r, fill)
    for p in (h, l, rr):
        disc(d, p, r, fill)
    d1, d2 = unit(t, b), unit(t, h)
    bx, by = d1[0] + d2[0], d1[1] + d2[1]
    bl = math.hypot(bx, by)
    half = math.acos(max(-1.0, min(1.0, d1[0] * d2[0] + d1[1] * d2[1]))) / 2
    m = r / math.sin(half)
    tip = (t[0] - bx / bl * m, t[1] - by / bl * m)
    n1 = (abs(d1[1]) * r, 0.0)               # the right side of the (vertical) stem
    n2 = (d2[1] * r, -d2[0] * r)             # the upper side of the flag
    if n2[1] > 0:
        n2 = (-n2[0], -n2[1])
    d.polygon([t, (t[0] + n1[0], t[1] + n1[1]), tip, (t[0] + n2[0], t[1] + n2[1])], fill=fill)


def draw_one(img, scale, ox, oy, lines, width):
    d = ImageDraw.Draw(img)
    tf = lambda p: (ox + (p[0] - X0) * scale, oy + (p[1] - Y0) * scale)
    s, w = S * scale, width * scale
    if lines == 1:
        layer(d, 2 * s, FG, tf)
        return
    radii = [2 * s + w / 2, 2 * s - w / 2, s + w / 2, s - w / 2, w / 2] if lines == 3 else \
            [2 * s + w / 2, 2 * s - w / 2, w / 2]
    for k, r in enumerate(radii):
        layer(d, r, FG if k % 2 == 0 else BG, tf)


def picture(px):
    big = px * 8   # drawn larger and scaled down: smooth edges
    im = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    ImageDraw.Draw(im).rounded_rectangle((0, 0, big - 1, big - 1), radius=big * 0.16, fill=BG)
    w, h = X1 - X0, Y1 - Y0
    scale = big * 0.86 / w
    lines = 3 if px >= 48 else 2 if px >= 24 else 1
    draw_one(im, scale, (big - w * scale) / 2, (big - h * scale) / 2, lines, 12.0 if px >= 128 else 16.0)
    return im.resize((px, px), Image.LANCZOS)


FONTS = ["C:/Windows/Fonts/arialbd.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"]


def logo(height):
    """The logo as in the manuals: white "SAPI" and the digit 1 of the same height on a black band."""
    font = next((f for f in FONTS if os.path.exists(f)), None)
    if not font:
        raise SystemExit("no bold font for the logo (" + ", ".join(FONTS) + ")")
    h = height * 4
    f = ImageFont.truetype(font, int(h * 1.08))
    im = Image.new("RGBA", (h * 6, h), BG)
    d = ImageDraw.Draw(im)
    cap = d.textbbox((0, 0), "S", font=f)
    cap_h = cap[3] - cap[1]
    top = (h - cap_h) / 2
    x = h * 0.12
    for ch in "SAPI":   # letter by letter: tight like the logo
        box = d.textbbox((0, 0), ch, font=f)
        d.text((x - box[0], top - cap[1]), ch, font=f, fill=FG)
        x += box[2] - box[0] + h * 0.035
    x += h * 0.28
    scale = cap_h / (Y1 - Y0)
    draw_one(im, scale, x, top, 3, 9.0)
    x += (X1 - X0) * scale + h * 0.12
    im = im.crop((0, 0, int(x), h))
    return im.resize((im.width // 4, height), Image.LANCZOS)


def main():
    sizes = [16, 24, 32, 48, 64, 128, 256]
    pics = [picture(n) for n in sizes]
    ico = os.path.join(ROOT, "src", "res", "sapiemu.ico")
    os.makedirs(os.path.dirname(ico), exist_ok=True)
    pics[-1].save(ico, format="ICO", sizes=[(n, n) for n in sizes], append_images=pics[:-1])
    png = os.path.join(ROOT, "docs", "img", "ikona.png")
    pics[-1].save(png, optimize=True)
    lg = os.path.join(ROOT, "docs", "img", "logo.png")
    logo(160).convert("RGB").save(lg, optimize=True)
    print(ico)
    print(png)
    print(lg)


if __name__ == "__main__":
    main()

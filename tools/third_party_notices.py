#!/usr/bin/env python3
"""THIRD-PARTY-NOTICES.txt: the libraries in sapiemu.exe and sapiemu-cli.exe with their licences.

Usage: python tools/third_party_notices.py   (after a library update, with third_party/README.md)

The licence texts are taken from third_party/ (SDL3 is fetched by CMake, its LICENSE.txt is kept in
third_party/sdl3/). The file goes into the release zip (tools/package.py).
"""
import io
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# name, version, use, licence, source, licence file
LIBS = [
    ("SDL 3", "3.4.18", "window, graphics, sound, input", "zlib",
     "https://github.com/libsdl-org/SDL", "third_party/sdl3/LICENSE.txt"),
    ("Dear ImGui", "1.92.9b (docking)", "user interface", "MIT",
     "https://github.com/ocornut/imgui", "third_party/imgui/LICENSE.txt"),
    ("cpp-httplib", "0.59.0", "MCP server, network of the card DPI-1L", "MIT",
     "https://github.com/yhirose/cpp-httplib", "third_party/httplib/LICENSE"),
    ("nlohmann/json", "3.12.0", "machine files, settings, MCP", "MIT",
     "https://github.com/nlohmann/json", "third_party/json/LICENSE.MIT"),
    ("floooh/chips z80.h", "master @ 9e88298, modified (flag cmos)", "Z80 CPU", "zlib",
     "https://github.com/floooh/chips", "third_party/chips/LICENSE"),
    ("superzazu/8080", "master @ 274ffd7, modified (8085 mode)", "8080 and 8085 CPU", "MIT",
     "https://github.com/superzazu/8080", "third_party/i8080/LICENSE"),
    ("Nuked-OPL3", "1.8 @ 765ec96", "YM3812 sound of the card MPH-1V", "LGPL 2.1 or later",
     "https://github.com/nukeykt/Nuked-OPL3", "third_party/nuked-opl3/LICENSE"),
    ("minimp3", "master @ ea99364", "MP3 cassette recordings", "CC0 1.0",
     "https://github.com/lieff/minimp3", "third_party/minimp3/LICENSE"),
    ("DejaVu fonts", "2.37", "fonts of the user interface (third_party/fonts)", "Bitstream Vera / DejaVu",
     "https://github.com/dejavu-fonts/dejavu-fonts", "third_party/fonts/DejaVu-LICENSE"),
]

HEAD = """SAPIemu - third party software
==============================

SAPIemu (Copyright (C) 2026 Martin Lukasek) is free software under the GNU General Public License
version 3, see LICENSE. Its source code including the libraries below is at
https://github.com/mlukasek/SAPIemu (the folder third_party, SDL is fetched by CMake).

The programs sapiemu.exe and sapiemu-cli.exe contain these libraries:

"""


def main():
    out = [HEAD]
    for name, ver, use, lic, url, _ in LIBS:
        out.append("  %-20s %-40s %s\n  %-20s %s\n" % (name, ver, lic, "", use + ", " + url))
    out.append("\nThe ROM, PROM and media images in roms/ and media/ belong to their authors, their sources are\n"
               "described in roms/README.md and media/README.md.\n")
    for name, ver, use, lic, url, path in LIBS:
        with io.open(os.path.join(ROOT, path), encoding="utf-8", errors="replace") as f:
            text = f.read().replace("\r\n", "\n").strip("\n")
        title = "%s %s (%s)" % (name, ver, lic)
        out.append("\n\n" + "=" * 100 + "\n" + title + "\n" + url + "\n" + "=" * 100 + "\n\n" + text + "\n")
    path = os.path.join(ROOT, "THIRD-PARTY-NOTICES.txt")
    with io.open(path, "w", encoding="utf-8", newline="\r\n") as f:
        f.write("".join(out))
    print(path)


if __name__ == "__main__":
    main()

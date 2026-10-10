#!/usr/bin/env python3
"""The EPROM of the plotter of AR 1990 (docs/desky/AR1990.md) from its listing.

roms/ar1990/PLOTTER.MAC is the listing of Amaterske radio 1990 (pages 25-34 of the supplement, V. Julius
OK1IVJ) as printed, for M80 with .RADIX 16 and .PHASE. The printed addresses are those of the author's
development system; the comments give the JPR-1 ones (program at 0, RAM 2000h, ports 2400h, 2800h,
2C00h). This makes a source for tools/asm8080.py with them and writes roms/ar1990/PLOTTER_AR1990.bin
(0000h up to the end of the program, for two 2716).

Usage: python tools/ar1990_rom.py
"""
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, "roms", "ar1990", "PLOTTER.MAC")
OUT = os.path.join(REPO, "roms", "ar1990", "PLOTTER_AR1990.bin")
JPR1 = {"SYKORA": "1000H", "NEWP": "0", "RAM": "2000H", "RDSI1": "2C00H", "RDSI": "2400H",
        "RDSO": "2800H", "DSHI": "2800H", "MOTR": "2400H"}


def convert(lines):
    out = []
    for ln in lines:
        code, sep, com = ln.partition(";")
        s = code.strip()
        if s in (".8080", ".RADIX 16", ".DEPHASE"):
            out.append(";" + ln)
            continue
        if s.startswith(".PHASE"):
            out.append("\tORG\t" + ("0" if "NEWP" in s else "2000H") + "\t;" + ln)
            continue
        m = re.match(r"^(\w+)\s+EQU\s+(\S+)", code)
        if m:
            out.append("%s\tEQU\t%s\t;%s" % (m.group(1), JPR1[m.group(1)], ln))
            continue
        if s.startswith("END"):
            out.append("\tEND")
            continue
        m = re.match(r"^(\S*)(\s+)(\S+)(\s*)(.*)$", code)
        if not m:
            out.append(ln)
            continue
        label, sp, op, sp2, args = m.groups()
        args = re.sub(r"\bLOW\s+(\w+)", r"(\1-(\1/100H)*100H)", args)
        args = re.sub(r"\bHIGH\s+(\w+)", r"(\1/100H)", args)
        args = re.sub(r"(?<![\w$])([0-9][0-9A-Fa-f]*)\b", r"\1H", args)   # .RADIX 16
        out.append(label + sp + op + sp2 + args + (sep + com if sep else ""))
    return out


def main():
    with open(SRC, encoding="ascii") as f:
        lines = f.read().splitlines()
    with tempfile.TemporaryDirectory() as tmp:
        asm = os.path.join(tmp, "plotter.asm")
        hexf = os.path.join(tmp, "plotter.hex")
        with open(asm, "w", encoding="ascii") as f:
            f.write("\n".join(convert(lines)) + "\n")
        r = subprocess.run([sys.executable, os.path.join(REPO, "tools", "asm8080.py"), asm, hexf])
        if r.returncode:
            return r.returncode
        mem = {}
        with open(hexf) as f:
            for ln in f:
                ln = ln.strip()
                if not ln.startswith(":") or int(ln[7:9], 16) != 0:
                    continue
                n, a = int(ln[1:3], 16), int(ln[3:7], 16)
                for i in range(n):
                    mem[a + i] = int(ln[9 + 2 * i:11 + 2 * i], 16)
    end = max(a for a in mem if a < 0x2000) + 1
    with open(OUT, "wb") as f:
        f.write(bytes(mem.get(a, 0xFF) for a in range(end)))
    print("%s: 0000-%04X (%d bytes)" % (OUT, end - 1, end))
    return 0


if __name__ == "__main__":
    sys.exit(main())

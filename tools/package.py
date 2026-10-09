#!/usr/bin/env python3
"""Release package: out/package/SAPIemu-<version>-win64.zip

Usage: python tools/package.py [--no-ide]   (after tools\\build.cmd release test)

The IDE disk of this PC (work/ide/*.img) goes in as work/ide/: the machine V and cpm_ide.sapi
start with its content (C: with the programs) instead of an empty disk. --no-ide leaves it out
(the disk holds music too, not for a public release).

The zip holds the release build (sapiemu.exe, sapiemu-cli.exe) and the data the emulator
finds next to the exe: roms/, media/, machines/, third_party/fonts, the licences of the
libraries, docs/, README.md, LICENSE and PRVNI-KROKY.md. Only files tracked by git are taken
(no work/, no local leftovers). The zip is checked: unpacked into a clean folder,
sapiemu-cli boots MikroMon from there.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "out", "build", "windows-release")
DATA = ["roms", "media", "machines", "docs", "third_party/fonts"]
FILES = ["README.md", "LICENSE", "third_party/README.md"]
# in the repository only: the data of the game Prince of Persia (Jordan Mechner, Broderbund) is not passed on
REPO_ONLY = {"media/zrmd-1/pop-ramdisk.bin", "media/zrmd-1/ramdisk_pop-fxs4-flappy-bomberman-bkp.bin",
             "machines/sapi1v_PoP.sapi"}


def git_files(path):
    out = subprocess.run(["git", "ls-files", "-z", "--", path], cwd=ROOT, capture_output=True, check=True).stdout
    return [f for f in out.decode("utf-8").split("\0") if f]


def main():
    cli = os.path.join(BUILD, "sapiemu-cli.exe")
    gui = os.path.join(BUILD, "sapiemu.exe")
    for exe in (cli, gui):
        if not os.path.exists(exe):
            print("missing " + exe + " (tools\\build.cmd release test first)")
            return 1
    # a packaged exe must not fall back to the data of this working copy (CMake SAPIEMU_SOURCE_FALLBACK)
    cache = os.path.join(BUILD, "CMakeCache.txt")
    with open(cache, encoding="utf-8", errors="replace") as f:
        if "SAPIEMU_SOURCE_FALLBACK:BOOL=OFF" not in f.read():
            print("the release build has SAPIEMU_SOURCE_FALLBACK on: reconfigure it with the windows-release preset")
            return 1
    version = subprocess.run([cli, "--version"], capture_output=True, text=True).stdout.split()[-1]
    name = "SAPIemu-%s-win64" % version
    out_dir = os.path.join(ROOT, "out", "package")
    os.makedirs(out_dir, exist_ok=True)
    with_ide = "--no-ide" not in sys.argv[1:]
    zip_path = os.path.join(out_dir, name + ".zip")

    files = []
    for d in DATA:
        files += git_files(d)
    files += FILES
    files += [f for f in git_files("third_party") if os.path.basename(f).upper().startswith(("LICENSE", "COPYING"))]
    files = sorted(set(files) - REPO_ONLY)

    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.write(gui, name + "/sapiemu.exe")
        z.write(cli, name + "/sapiemu-cli.exe")
        z.write(os.path.join(ROOT, "docs", "prvni-kroky.md"), name + "/PRVNI-KROKY.md")
        for f in files:
            z.write(os.path.join(ROOT, f), name + "/" + f)
        if with_ide:
            ide = os.path.join(ROOT, "work", "ide")
            for f in sorted(os.listdir(ide)) if os.path.isdir(ide) else []:
                if f.lower().endswith(".img"):
                    z.write(os.path.join(ide, f), name + "/work/ide/" + f)
                    print("  + work/ide/" + f)
    size = os.path.getsize(zip_path)
    print("%s: %d files, %.1f MB" % (zip_path, len(files) + 3, size / 1e6))

    # check: unpacked elsewhere, the emulator finds its data next to the exe and boots
    with tempfile.TemporaryDirectory() as tmp:
        with zipfile.ZipFile(zip_path) as z:
            z.extractall(tmp)
        exe = os.path.join(tmp, name, "sapiemu-cli.exe")
        r = subprocess.run([exe, "--machine", "machines/mikromon.sapi", "--wait-text", "V1.3", "--print-screen"],
                           cwd=tmp, capture_output=True, text=True, timeout=120)
        ok = r.returncode == 0 and "MikroMon" in r.stdout
        print("check (MikroMon from the unpacked zip): " + ("OK" if ok else "FAILED\n" + r.stdout + r.stderr))
        shutil.rmtree(os.path.join(tmp, name, "work"), ignore_errors=True)
        return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

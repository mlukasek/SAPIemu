#!/usr/bin/env python3
"""Release package: out/package/SAPIemu-<version>-win64.zip

Usage: python tools/package.py [--no-ide | --public]   (after tools\\build.cmd release test)

The IDE disk of this PC (work/ide/*.img) goes in as work/ide/: the machine V and cpm_ide.sapi
start with its content (C: with the programs) instead of an empty disk. --no-ide leaves it out
(the disk holds music too, not for a public release).

--public: the package for everybody (beta and release, docs/vydani/): no IDE disk of this PC, no notes
of the development (DEV_DOCS); the zip is checked not to hold them.

The zip holds the release build (sapiemu.exe, sapiemu-cli.exe) and the data the emulator
finds next to the exe: roms/, media/, machines/, third_party/fonts, the licences of the
libraries, docs/, README.md, README.en.md, LICENSE, PRVNI-KROKY.md (Czech first steps) and GETTING-STARTED.md
(English). Only files tracked by git are taken (no work/, no local leftovers). The zip is checked:
unpacked into a clean folder, sapiemu-cli boots MikroMon from there.

Next to the zip goes <zip name>.sha256 with its SHA-256 in the format of sha256sum ("<hex>  <name>",
check: sha256sum -c, or certutil -hashfile <zip> SHA256 on Windows); the hash is printed too. It is written
only after the checks passed (the checksum of an older zip is removed first): a zip without it is not ready.
The first steps (GUIDES) must be tracked by git, as the rest of docs/ (a new file not added yet is refused).
The documents the program shows in its window Help (HELP_DOCS: First steps and the Glossary in Czech and
English) must be tracked too and are checked to be in docs/ of the zip.
"""
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "out", "build", "windows-release")
DATA = ["roms", "media", "machines", "docs", "third_party/fonts"]
FILES = ["README.md", "README.en.md", "LICENSE", "THIRD-PARTY-NOTICES.txt", "third_party/README.md"]
# --public: notes of the development (state, decisions, the release plan and consents) stay in the repository
DEV_DOCS = ("docs/SOUHRN.md", "docs/rozhodnuti.md", "docs/vydani/")
# in the repository only (none now: Prince of Persia goes into the release since 2026-10-28, the author's decision)
REPO_ONLY = set()
# the first steps at the top of the zip: (source in the repository, name in the zip)
GUIDES = [("docs/prvni-kroky.md", "PRVNI-KROKY.md"), ("docs/getting-started.md", "GETTING-STARTED.md")]
# Help -> First steps and Glossary drawn in the program (src/frontend/help.cpp k_help) from docs/ of the zip
HELP_DOCS = ["docs/prvni-kroky.md", "docs/getting-started.md", "docs/slovnicek.md", "docs/glossary.md"]


def git_files(path):
    out = subprocess.run(["git", "ls-files", "-z", "--", path], cwd=ROOT, capture_output=True, check=True).stdout
    return [f for f in out.decode("utf-8").split("\0") if f]


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def write_sha256(path):
    """Write <path>.sha256 as sha256sum does ("<hex>  <file name>", LF) and return that line."""
    line = "%s  %s" % (sha256_file(path), os.path.basename(path))
    with open(path + ".sha256", "wb") as f:
        f.write((line + "\n").encode("utf-8"))
    return line


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
    public = "--public" in sys.argv[1:]
    with_ide = "--no-ide" not in sys.argv[1:] and not public
    zip_path = os.path.join(out_dir, name + ".zip")

    # the first steps go to the top of the zip from the working tree, docs/ from git: an untracked one would be at the
    # top but missing in docs/ (Help -> First steps in English would fall back to the Czech file)
    untracked = [src for src in [g for g, _ in GUIDES] + HELP_DOCS if not git_files(src)]
    if untracked:
        print("not tracked by git (git add first): " + ", ".join(untracked))
        return 1
    sha_path = zip_path + ".sha256"
    if os.path.exists(sha_path):   # the checksum of an older zip must not sit next to a new one that fails its checks
        os.remove(sha_path)

    files = []
    for d in DATA:
        files += git_files(d)
    files += FILES
    files += [f for f in git_files("third_party") if os.path.basename(f).upper().startswith(("LICENSE", "COPYING"))]
    files = sorted(set(files) - REPO_ONLY)
    if public:
        files = [f for f in files if not f.startswith(DEV_DOCS)]

    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        z.write(gui, name + "/sapiemu.exe")
        z.write(cli, name + "/sapiemu-cli.exe")
        for src, dst in GUIDES:
            z.write(os.path.join(ROOT, src), name + "/" + dst)
        for f in files:
            z.write(os.path.join(ROOT, f), name + "/" + f)
        if with_ide:
            ide = os.path.join(ROOT, "work", "ide")
            for f in sorted(os.listdir(ide)) if os.path.isdir(ide) else []:
                if f.lower().endswith(".img"):
                    z.write(os.path.join(ide, f), name + "/work/ide/" + f)
                    print("  + work/ide/" + f)
    size = os.path.getsize(zip_path)
    print("%s: %d files, %.1f MB" % (zip_path, len(files) + 2 + len(GUIDES), size / 1e6))
    not_ready = "NOT READY: do not publish " + os.path.basename(zip_path) + " (no .sha256 written)"

    with zipfile.ZipFile(zip_path) as z:   # the window Help reads them from docs/
        names = set(z.namelist())
    missing = [d for d in HELP_DOCS if name + "/" + d not in names]
    if missing:
        print("the package lacks the documents of Help: " + ", ".join(missing))
        print(not_ready)
        return 1

    if public:
        with zipfile.ZipFile(zip_path) as z:
            inner = [n[len(name) + 1:] for n in z.namelist()]
        bad = [n for n in inner if n.startswith(("work/",) + DEV_DOCS) or n in REPO_ONLY]
        if bad:
            print("the public package holds files it must not: " + ", ".join(bad[:5]))
            print(not_ready)
            return 1
        print("public: no work/, no notes of the development, no REPO_ONLY files")

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
    if not ok:
        print(not_ready)
        return 1
    # the checksum only for a zip that passed its checks
    print("SHA-256 (" + os.path.basename(sha_path) + "): " + write_sha256(zip_path))
    return 0


if __name__ == "__main__":
    sys.exit(main())

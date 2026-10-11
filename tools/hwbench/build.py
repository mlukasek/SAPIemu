"""Assembles the bench test ROMs (tools/hwbench/*.asm) with pasmo into work/hwbench/*.bin.
python tools/hwbench/build.py [pasmo.exe]"""
import glob
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.join(os.path.dirname(os.path.dirname(HERE)), 'work', 'hwbench')
PASMO = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(HERE))),
                                                            'Tools', 'pasmo-0.5.3', 'pasmo.exe')
os.makedirs(WORK, exist_ok=True)
for src in sorted(glob.glob(os.path.join(HERE, '*.asm'))):
    out = os.path.join(WORK, os.path.splitext(os.path.basename(src))[0] + '.bin')
    r = subprocess.run([PASMO, src, out], capture_output=True, text=True)
    print('%-14s %s' % (os.path.basename(src), 'ok' if r.returncode == 0 else 'ERROR ' + r.stdout + r.stderr))

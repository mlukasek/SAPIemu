"""Decodes the values the diagnostic ROM (diag.asm) taps on /IOR from a Logic 2 CSV export.
python decode_ior.py digital.csv [ior_column]"""
import csv
import sys

rows = list(csv.reader(open(sys.argv[1])))
head, rows = rows[0], rows[1:]
k = int(sys.argv[2]) if len(sys.argv) > 2 else head.index('Channel 3')
t = [float(r[0]) for r in rows]
v = [int(r[k]) for r in rows]
falls = [t[i] for i in range(1, len(v)) if v[i] == 0 and v[i - 1] == 1]
groups = []
for f in falls:
    if groups and f - groups[-1][1] < 30e-6:
        groups[-1][1] = f
        groups[-1][2] += 1
    else:
        groups.append([f, f, 1])
values = []
i = 0
while i < len(groups):
    if groups[i][2] == 5 and i + 8 < len(groups) and all(g[2] in (1, 3) for g in groups[i + 1:i + 9]):
        b = 0
        for g in groups[i + 1:i + 9]:
            b = (b << 1) | (1 if g[2] == 3 else 0)
        values.append((groups[i][0], b))
        i += 9
    else:
        i += 1
names = ['status after /RES', 'modem after /RES', 'status after UART reset', 'status after 8N1',
         'modem after 8N1', 'status after 2 chars back to back', 'status 3 ms later']
for n, (tt, b) in enumerate(values):
    print('%d  t=%.6f s  %02Xh  %s  %s' % (n + 1, tt, b, format(b, '08b'), names[n] if n < len(names) else ''))

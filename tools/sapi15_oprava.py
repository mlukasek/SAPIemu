"""Repaired copies of the Melsoft disks 2, 3 and 4 of the SAPI-1.5: media/sapi15/SAPI15_{2,3,4}_opr.sp2.

    python tools/sapi15_oprava.py [--archive DIR ...] [--list]

The surviving images of these disks have damaged sectors (docs/software/SAPI-1.5.md): a sector is right up to some byte
and garbage from there to its end, or has a few wrong bytes, nearly always on the same sector numbers of neighbouring
cylinders. Every file of the disks exists elsewhere in the archives (..\\SAPI_DOKU_FTP, ..\\SAPI_DOKU), so a damaged
sector is replaced by the content most copies of the file agree on. The sector must then give a file equal to a real
copy, otherwise the script stops. Kept as Melsoft has them: SYSGEN.COM (its own version) and the screen size in WM.COM.
The disk 3 also gets its damaged BDOS sector (C0 H0 R14) from the disk 1. The originals stay untouched.
"""
import collections
import hashlib
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MEDIA = os.path.join(ROOT, 'media', 'sapi15')
BLOCK = 2048            # BLS of the .sp2 format, DSM = 155: one byte per block in a directory entry
RPB = BLOCK // 128
DISKS = (1, 2, 3, 4, 7, 8)
REPAIR = {2: 41, 3: 6, 4: 27}   # sectors of files expected to be replaced per disk
KEEP_FILE = {'SYSGEN.COM'}      # the version of Melsoft
KEEP_SECTOR = {('WM.COM', 0)}   # 01B9h, 01BAh: the screen size 20 x 64 set by Melsoft
KEEP_BYTES = {'WM.COM': (0xB9, 0xBA)}


def physical_sectors():
    """The skew of the CBIOS (TRANS0): logical sector -> physical sector of the cylinder (32 sectors of 256 B)."""
    return [s for start in (0, 1, 2) for s in range(start, 32, 3)]


SKEW = physical_sectors()


def rec_off(track, rec):
    """Image offset of the CP/M record rec (0-63) of the CP/M track (a cylinder)."""
    return track * 8192 + SKEW[rec // 2] * 256 + (rec & 1) * 128


def block_offsets(b):
    r = b * RPB
    return [rec_off(1 + (r + i) // 64, (r + i) % 64) for i in range(RPB)]    # OFF = 1


def read_dir(img):
    data = b''.join(img[o:o + 128] for b in (0, 1) for o in block_offsets(b))
    files = {}
    for i in range(0, 4096, 32):
        e = data[i:i + 32]
        if e[0] > 15:
            continue
        name = bytes(c & 0x7F for c in e[1:9]).decode('latin1').strip()
        ext = bytes(c & 0x7F for c in e[9:12]).decode('latin1').strip()
        files.setdefault(name + ('.' + ext if ext else ''), []).append((e[12] | e[14] << 5, e[15], list(e[16:32])))
    return files


def file_records(entries):
    """Image offsets of the records of a file: EXM = 1, an entry holds up to two logical extents."""
    out = []
    for ex, rc, blocks in sorted(entries):
        recs = [o for b in blocks if b for o in block_offsets(b)]
        out += recs[:(ex & 1) * 128 + rc]
    return out


def walk_archives(dirs):
    index = collections.defaultdict(list)
    for r in dirs:
        for base, _, names in os.walk(r):
            if 'MELSOFT_DISKETY' in base:   # the images under repair and the files extracted from them
                continue
            for f in names:
                index[f.upper()].append(os.path.join(base, f))
    return index


def repair_disk(n, imgs, files, index, listing):
    out = bytearray(imgs[n])
    done, names = 0, []
    for name, offs in sorted(files[n].items()):
        data = b''.join(imgs[n][o:o + 128] for o in offs)
        if name in KEEP_FILE or not data:
            continue
        cands = [open(q, 'rb').read() for q in index.get(name.upper(), []) if os.path.getsize(q) == len(data)]
        cands += [b''.join(imgs[m][o:o + 128] for o in files[m][name]) for m in DISKS
                  if m != n and name in files[m] and len(files[m][name]) * 128 == len(data)]
        if not cands:
            sys.exit('disk %d: no other copy of %s' % (n, name))
        fixed = bytearray(data)
        hit = 0
        for i in range(0, len(data), 256):
            cur = data[i:i + 256]
            votes = collections.Counter(c[i:i + 256] for c in cands)
            top, cnt = votes.most_common(1)[0]
            if cur == top or (name, i // 256) in KEEP_SECTOR:
                continue
            if listing:
                first = next(j for j in range(len(cur)) if cur[j] != top[j])
                diff = [j for j in range(len(cur)) if cur[j] != top[j]]
                lba = offs[i // 128] // 256
                print('disk %d %-13s sector %3X  C%02d H%d R%02d  bytes %3d from %02X to %02X  agree %d/%d' % (
                    n, name, i // 256, lba // 32, (lba // 16) & 1, lba % 16 + 1, len(diff), first, diff[-1], cnt, len(cands)))
                if len(diff) <= 5:
                    print('      ', ' '.join('%02X:%02X>%02X' % (j, cur[j], top[j]) for j in diff))
            fixed[i:i + 256] = top
            for r in range(len(top) // 128):
                o = offs[i // 128 + r]
                out[o:o + 128] = top[r * 128:r * 128 + 128]
            hit += 1
        if hit:
            # the result must be a real copy, not a mixture of different versions
            keep = KEEP_BYTES.get(name, ())
            def same(c):
                return all(fixed[k] == c[k] or k in keep for k in range(len(fixed)))
            if not any(same(c) for c in cands):
                sys.exit('disk %d: the repaired %s is not equal to any copy' % (n, name))
            done += hit
            names.append(name)
    return out, done, names


def main():
    a = sys.argv[1:]
    dirs = [a[i + 1] for i, x in enumerate(a) if x == '--archive']
    if not dirs:
        dirs = [os.path.join(ROOT, '..', d) for d in ('SAPI_DOKU_FTP', 'SAPI_DOKU')]
    dirs = [d for d in dirs if os.path.isdir(d)]
    if not dirs:
        sys.exit('archive not found (..\\SAPI_DOKU_FTP), give --archive')
    imgs = {n: open(os.path.join(MEDIA, 'SAPI15_%d.sp2' % n), 'rb').read() for n in DISKS}
    files = {n: {k: file_records(v) for k, v in read_dir(imgs[n]).items()} for n in DISKS}
    index = walk_archives(dirs)
    for n in REPAIR:
        out, done, names = repair_disk(n, imgs, files, index, '--list' in a)
        if done != REPAIR[n]:
            sys.exit('disk %d: %d sectors to repair, expected %d' % (n, done, REPAIR[n]))
        if n == 3:      # the BDOS sector of the system cylinder
            o = 13 * 256
            if imgs[3][o:o + 256] == imgs[1][o:o + 256]:
                sys.exit('disk 3: the BDOS sector is not damaged?')
            out[o:o + 256] = imgs[1][o:o + 256]
        path = os.path.join(MEDIA, 'SAPI15_%d_opr.sp2' % n)
        open(path, 'wb').write(out)
        print('%s: %d sectors in %d files (%s)%s, md5 %s' % (os.path.relpath(path, ROOT), done, len(names), ' '.join(names),
                                                            ' + BDOS' if n == 3 else '', hashlib.md5(out).hexdigest()))


if __name__ == '__main__':
    main()

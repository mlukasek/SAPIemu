"""Collects the open questions for the real hardware ("Nejasnosti" sections of docs/**/*.md) into
docs/hw-otazky.md: one list per board / topic, resolved items (struck through ~~...~~ or marked
"vyřešeno") left out, each with a link to its source. Run after editing the Nejasnosti:

    python tools/hw_otazky.py

The questions themselves stay in the board documents; docs/hw-otazky.md is only generated."""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'docs', 'hw-otazky.md')
HEAD = re.compile(r'^(#{2,4})\s+(.*)$')
ITEM = re.compile(r'^(\s*)(?:[-*]|\d+[.)])\s+')


def resolved(text):
    t = text.strip()
    body = ITEM.sub('', t, count=1)
    if body.startswith('~~'):
        return True
    low = body.lower()
    return low.startswith('vyřešeno') or low.startswith('**vyřešeno')


def items_of(lines):
    """Top-level items of a section (with their continuation lines and sub-items)."""
    out, cur = [], None
    for line in lines:
        m = ITEM.match(line)
        if m and len(m.group(1)) == 0:
            if cur:
                out.append(cur)
            cur = [line]
        elif cur is not None and (line.startswith(' ') or line.startswith('\t')) and line.strip():
            cur.append(line)
        elif cur is not None and not line.strip():
            out.append(cur)
            cur = None
        elif cur is None and line.strip() and not line.startswith('|'):
            out.append([line])   # a paragraph without a bullet
    if cur:
        out.append(cur)
    return out


def sections(path):
    with open(path, encoding='utf-8') as f:
        lines = f.read().replace('\r\n', '\n').split('\n')
    res = []
    i = 0
    while i < len(lines):
        m = HEAD.match(lines[i])
        if m and 'nejasnost' in m.group(2).lower():
            level = len(m.group(1))
            j = i + 1
            while j < len(lines):
                n = HEAD.match(lines[j])
                if n and len(n.group(1)) <= level:
                    break
                j += 1
            res.append((m.group(2).strip(), lines[i + 1:j]))
            i = j
        else:
            i += 1
    return res


def title_of(path):
    with open(path, encoding='utf-8') as f:
        for line in f:
            if line.startswith('# '):
                return line[2:].strip()
    return os.path.basename(path)


def main():
    files = []
    for d, _, names in os.walk(os.path.join(ROOT, 'docs')):
        for n in names:
            p = os.path.join(d, n)
            if n.endswith('.md') and os.path.abspath(p) != OUT and 'release-notes' not in p:
                files.append(p)
    files.sort(key=lambda p: (not p.replace('\\', '/').split('/docs/')[1].startswith('desky/'), p.lower()))
    parts, total = [], 0
    for p in files:
        rel = os.path.relpath(p, os.path.join(ROOT, 'docs')).replace('\\', '/')
        for name, body in sections(p):
            items = [it for it in items_of(body) if not resolved(it[0])]
            if not items:
                continue
            total += len(items)
            parts.append('## [%s](%s) – %s (%d)\n' % (title_of(p), rel, name, len(items)))
            for it in items:
                parts.append('\n'.join(it).rstrip() + '\n')
            parts.append('')
    text = ('# Otázky k ověření na skutečném HW\n\n'
            '**Generováno** skriptem `python tools/hw_otazky.py` ze sekcí „Nejasnosti“ v `docs/` (needitovat ručně,\n'
            'otázky se doplňují a odškrtávají v dokumentech desek a skript se spustí znovu). Vyřešené body (přeškrtnuté\n'
            'nebo „vyřešeno“) jsou vynechané. Podklad pro společné dávkové ověřování na skutečném SAPI-1.\n\n'
            'Otevřených bodů: **%d** v %d sekcích. Desky jsou první, pak ostatní témata (SOUHRN, software, tiskárna…).\n\n'
            % (total, sum(1 for x in parts if x.startswith('## '))) + '\n'.join(parts))
    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text.rstrip() + '\n')
    print('%s: %d open items' % (os.path.relpath(OUT, ROOT), total))
    return 0


if __name__ == '__main__':
    sys.exit(main())

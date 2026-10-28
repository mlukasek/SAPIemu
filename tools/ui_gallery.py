"""A gallery of the SAPIemu GUI in all looks (docs/vydani/plan-vzhled.md, checkpoint A): the same screens of the
emulator in every look of src/frontend/theme_data.cpp, on a page out/ui-gallery/<time>/index.html (rows = screens,
columns = looks, a click opens the whole picture).

The states of the machines are made by sapiemu-cli, the pictures by sapiemu --ui-snapshot --ui-theme over a clean
copy of the data (as a new installation); every picture gets new settings, so no imgui.ini of a previous one
moves its windows.

  python tools/ui_gallery.py [--themes win11-light,sapi-dark] [--screens 01,16] [--lang en] [--ref DIR] [--debug]

--ref DIR: older pictures with the same names (out/ux-review-20261028) as the first column "before".
"""
import concurrent.futures
import html
import json
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_machines as cm  # noqa: E402
import make_previews as mp  # noqa: E402

ROOT = cm.ROOT
THEMES = ['win11-light', 'win11-dark', 'classic-light', 'classic-dark', 'sapi-light', 'sapi-dark']
THEME_NAMES = {'win11': 'Windows 11', 'classic': 'Windows klasický', 'sapi': 'SAPI průmyslový'}

ZPS1 = 'machines/zps/ZPS-1.sapi'
HRY = 'machines/sapi1v_hry.sapi'
V = 'machines/sapi1v.sapi'
STATES = {
    'zps1': (ZPS1, mp.PREVIEWS['zps_ZPS-1'][0]),
    'hry': (HRY, cm.ecmon('3') + cm.t('FLAPPY\\r') + cm.run(8000)),
    'fdc1': ('machines/cpm_zrmd_fdc1.sapi', cm.MACHINES['cpm_zrmd_fdc1'][0]),
    'kzd1': ('machines/zps/ZPS-2_MIKOS_K.sapi', cm.run(3000)),
    'aps5': ('machines/aps5.sapi', cm.run(5000)),
    'ims2': ('machines/zps/ZPS-2_DPS-1_IMS-2.sapi', cm.run(3000)),
    'minigraf': ('machines/zps/ZPS-2_MIKOS_V5_Minigraf.sapi', mp.INSTRUMENTS['zps_ZPS-2_MIKOS_V5_Minigraf'][0]),
    'mikromon': ('machines/mikromon.sapi', cm.MACHINES['mikromon'][0]),
    'cpm': ('machines/cpm_zrmd.sapi', cm.MACHINES['cpm_zrmd'][0]),
}
DEBUG_WINDOWS = {'windows': {'debugger': True, 'memory': True}}
# name, machine, state, options, size, first start (no settings), more settings, what it shows
SCREENS = [
    ('01_first_start', None, None, [], '1600x900', True, None, 'první start nové instalace: Vybrat sestavu nad ZPS-1'),
    ('03_first_start_1280', None, None, [], '1280x720', True, None, 'první start v malém okně 1280 × 720'),
    ('04_zps1_guide', ZPS1, 'zps1', ['--ui-guide'], '1600x900', False, None, 'ZPS-1 s programem v BASICu a průvodcem'),
    ('07_zps1_screen_menu', ZPS1, 'zps1', ['--ui-screen-menu'], '1600x900', False, None, 'nabídka pravého tlačítka na obrazovce'),
    ('08_hry_flappy', HRY, 'hry', [], '1600x900', False, None, 'herní sestava: Flappy na CGA-1V, víc oken displejů'),
    ('11_rack_sapi1v', V, None, ['--ui-rack', '1'], '1600x900', False, None, 'konfigurátor (rám s kartami), pozice X1'),
    ('13_map', V, None, ['--ui-map'], '1600x900', False, None, 'konfigurátor: mapa portů a paměti'),
    ('14_media_fdc1', 'machines/cpm_zrmd_fdc1.sapi', 'fdc1', ['--ui-media'], '1600x900', False, None, 'Média: disketové jednotky'),
    ('14b_media_kzd1', 'machines/zps/ZPS-2_MIKOS_K.sapi', 'kzd1', ['--ui-media'], '1600x900', False, None, 'Média: záznamník dat KZD-1'),
    ('15_terminal', 'machines/mikromon.sapi', 'mikromon', ['--ui-terminal', '--ui-term-text', 'MikroMon 1.3\\n> ?\\n'], '1600x900',
     False, None, 'sériový terminál'),
    ('16_settings', ZPS1, 'zps1', ['--ui-settings'], '1600x900', False, None, 'Zobrazení → Nastavení'),
    ('18_about', ZPS1, 'zps1', ['--ui-about'], '1600x900', False, None, 'Nápověda → O programu'),
    ('20_aps5', 'machines/aps5.sapi', 'aps5', ['--ui-aps5'], '1600x900', False, None, 'APS-5: panel a pece'),
    ('21_ims2', 'machines/zps/ZPS-2_DPS-1_IMS-2.sapi', 'ims2', ['--ui-ims2'], '1600x900', False, None, 'měřicí pracoviště IMS-2'),
    ('22_minigraf', 'machines/zps/ZPS-2_MIKOS_V5_Minigraf.sapi', 'minigraf', ['--ui-plotter'], '1600x900', False, None,
     'zapisovač Minigraf po kreslení'),
    ('25_cpm_printer_debug', 'machines/cpm_zrmd.sapi', 'cpm', ['--ui-printer'], '1600x900', False, DEBUG_WINDOWS,
     'CP/M s tiskárnou, debugger a paměť'),
]


def option(name, default=None):
    if name in sys.argv:
        i = sys.argv.index(name)
        if i + 1 < len(sys.argv):
            return sys.argv[i + 1]
    return default


def sandbox(out):
    """The data of HEAD as a new installation, the fonts and docs of the working tree (new fonts, the logo)."""
    sb = mp.sandbox()
    for d in ('third_party/fonts', 'docs/img'):
        shutil.copytree(os.path.join(ROOT, d), os.path.join(sb, d), dirs_exist_ok=True)
    print('data', sb)
    return sb


def picture(job):
    gui, sb, out, lang, theme, (name, machine, state, opts, size, first, extra, _), state_file = job
    d = os.path.join(out, 'settings', theme, name)
    os.makedirs(d, exist_ok=True)
    settings = os.path.join(d, 'sapiemu-settings.json')
    if not first:
        s = {'guides': {'auto': False}}
        s.update(extra or {})
        open(settings, 'w', encoding='utf-8').write(json.dumps(s))
    png = os.path.join(out, theme, name + '.png')
    args = [gui, '--data-root', sb, '--settings', settings, '--no-mcp', '--lang', lang, '--ui-size', size,
            '--ui-theme', theme]
    if machine:
        args += ['--machine', machine]
    if state and state_file.get(state):
        args += ['--ui-state', state_file[state]]
    args += list(opts) + ['--ui-snapshot', png]
    r = subprocess.run(args, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=600, cwd=sb)
    return theme, name, os.path.exists(png), (r.stderr or '').strip()[:150]


def web_copies(out, themes, names, ref):
    """JPEG copies for the page (the PNGs of 1600 x 900 have 3 MB): a thumbnail and the whole picture."""
    try:
        from PIL import Image
    except ImportError:
        return False
    cols = (['before'] if ref else []) + themes
    for col in cols:
        os.makedirs(os.path.join(out, 'web', col), exist_ok=True)
        for name in names:
            src = os.path.join(ref, name + '_cs.png') if col == 'before' else os.path.join(out, col, name + '.png')
            if col == 'before' and not os.path.exists(src):
                src = os.path.join(ref, name + '.png')
            if not os.path.exists(src):
                continue
            im = Image.open(src).convert('RGB')
            im.save(os.path.join(out, 'web', col, name + '.jpg'), quality=88)
            th = im.copy()
            th.thumbnail((480, 480))
            th.save(os.path.join(out, 'web', col, name + '_t.jpg'), quality=85)
    return True


def page(out, themes, screens, ref, jpeg):
    cols = (['before'] if ref else []) + themes

    def title(col):
        if col == 'before':
            return 'dnes (před úpravou)'
        v, _, m = col.partition('-')
        return '%s<br><small>%s</small>' % (THEME_NAMES.get(v, v), {'light': 'světlý', 'dark': 'tmavý'}.get(m, m))

    rows = []
    for name, *_rest in screens:
        what = _rest[-1]
        cells = ['<th scope="row"><b>%s</b><br><small>%s</small></th>' % (html.escape(name), html.escape(what))]
        for col in cols:
            if jpeg:
                full, thumb = 'web/%s/%s.jpg' % (col, name), 'web/%s/%s_t.jpg' % (col, name)
            else:
                full = thumb = '%s/%s.png' % (col, name)
            if os.path.exists(os.path.join(out, thumb)):
                cells.append('<td><a href="%s"><img src="%s" loading="lazy" alt=""></a></td>' % (full, thumb))
            else:
                cells.append('<td class="none">–</td>')
        rows.append('<tr>' + ''.join(cells) + '</tr>')
    head = ''.join('<th>%s</th>' % title(c) for c in cols)
    doc = '''<!doctype html>
<html lang="cs"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Galerie vzhledů SAPIemu</title>
<style>
:root { --bg: #f6f6f6; --fg: #1b1b1b; --line: #d0d0d0; --cell: #ffffff; }
@media (prefers-color-scheme: dark) { :root { --bg: #1c1c1c; --fg: #eeeeee; --line: #3a3a3a; --cell: #262626; } }
body { margin: 16px; background: var(--bg); color: var(--fg); font: 15px "Segoe UI", system-ui, sans-serif; }
h1 { font-size: 22px; }
.wrap { overflow-x: auto; }
table { border-collapse: collapse; }
th, td { border: 1px solid var(--line); padding: 6px; vertical-align: top; background: var(--cell); }
thead th { position: sticky; top: 0; z-index: 1; }
th[scope=row] { text-align: left; min-width: 160px; max-width: 200px; font-weight: normal; }
td img { width: 300px; display: block; }
td.none { text-align: center; color: #888; }
</style></head><body>
<h1>Galerie vzhledů SAPIemu</h1>
<p>Stejné obrazovky emulátoru v každém vzhledu (sapiemu --ui-snapshot --ui-theme, %s). Klik na obrázek ho otevře celý.</p>
<div class="wrap"><table><thead><tr><th>obrazovka</th>%s</tr></thead><tbody>
%s
</tbody></table></div></body></html>
''' % (time.strftime('%Y-%m-%d %H:%M'), head, '\n'.join(rows))
    open(os.path.join(out, 'index.html'), 'w', encoding='utf-8').write(doc)


def main():
    themes = option('--themes', ','.join(THEMES)).split(',')
    only = option('--screens')
    screens = [s for s in SCREENS if not only or any(s[0].startswith(p) for p in only.split(','))]
    lang = option('--lang', 'cs')
    ref = option('--ref')
    # the build made last (the other one may be older than the code), --debug: the debug one
    builds = [os.path.join(ROOT, 'out', 'build', p, 'sapiemu.exe') for p in ('windows-debug', 'windows-release')]
    builds = [b for b in builds if os.path.exists(b) and ('--debug' not in sys.argv or 'debug' in b)]
    if not builds:
        raise SystemExit('no sapiemu.exe in out/build (tools/build.cmd debug)')
    gui = max(builds, key=os.path.getmtime)
    print('gui', gui)
    cli = os.path.join(os.path.dirname(gui), 'sapiemu-cli.exe')
    out = os.path.join(ROOT, 'out', 'ui-gallery', time.strftime('%Y%m%d-%H%M%S'))
    os.makedirs(out)
    sb = sandbox(out)
    state_file = {}
    for name in sorted({s[2] for s in screens if s[2]}):
        machine, acts = STATES[name]
        path = os.path.join(out, 'states', name + '.sapistate')
        os.makedirs(os.path.dirname(path), exist_ok=True)
        cm.cli_run(cli, sb, machine, list(acts) + ['--save-state', path], os.path.join(out, 'states', name + '.txt'))
        state_file[name] = path if os.path.exists(path) else None
        print('state %-10s %s' % (name, 'OK' if state_file[name] else 'MISSING'))
    for theme in themes:
        os.makedirs(os.path.join(out, theme), exist_ok=True)
    jobs = [(gui, sb, out, lang, theme, s, state_file) for theme in themes for s in screens]
    bad = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex:
        for theme, name, ok, err in ex.map(picture, jobs):
            if not ok:
                bad += 1
            print('%-14s %-22s %s %s' % (theme, name, 'OK' if ok else 'FAIL', '' if ok else err))
    jpeg = web_copies(out, themes, [s[0] for s in screens], ref)
    page(out, themes, screens, ref, jpeg)
    print('gallery', os.path.join(out, 'index.html'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())

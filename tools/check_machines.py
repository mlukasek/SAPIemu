"""Check before a release: every machine of machines/ is started and something is run in it, the result checked on the
screen (or a screenshot, the serial line). --games also runs the ports and demos on the V machine: Bad Apple!! to its
end, all tunes of FXS4 (Fuxoft Soundtrack IV) with the sound measured, Prince of Persia, Bomberman, Flappy (CGA-1V and
DGD-1V).

It works on a clean copy of the data as a fresh package has it (git archive of roms/, media/, machines/ plus the IDE
disk work/ide/sapi_hdd.img when present) in out/check/<time>/, so the working copies and the battery files of work/
stay untouched. The sapiemu-cli of the release build is used (else the debug one).

python tools/check_machines.py [--games | --games-only] [--debug] [name-filter]"""
import glob
import math
import os
import shutil
import struct
import subprocess
import sys
import time
import wave
import re
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def w(text, ms=30000):
    return ['--timeout-ms', str(ms), '--wait-text', text]


def t(text):
    return ['--type', text]


def run(ms):
    return ['--run-ms', str(ms)]


def cpm_dir(drive=''):
    return t('DIR %s\\r' % drive) + run(12000)


def ecmon(choice):
    return w('Zadej 0-3', 10000) + t(choice) + w('A>', 30000)


CPM = w('A>', 30000)
UCPM = w('U0A>', 30000)
BASIC = w('READY', 10000) + t('PRINT 6*7\\r') + run(1500)
MIKOS = run(3000) + t('D0 7\\r') + run(1500)
MANIC = run(2000) + ['--load-hex', 'media/jpr-1s/manic_miner.hex'] + t('JUMP 4400\\r') + run(8000)

# machine (file name without .sapi, zps/ as zps_): (actions, checks). A check is a text on the screen,
# 'shot:<display>' (a screenshot that is not one colour), 'serial:<text>' or 'log-clean'.
MACHINES = {
    'mikromon': (run(1500) + t('?\\r') + run(1500), ['M adr']),
    'aps5': (w('ZOBRAZENI TABULKY PORUCH', 20000) + t('11\\r') + run(6000), ['VYVOLANI MENU', 'I    0:00']),
    'cpm_zrmd': (ecmon('3') + cpm_dir(), ['WM1Z']),
    'cpm_ide': (ecmon('3') + cpm_dir('B:'), ['.COM']),
    'cpm_zrmd_fdc1': (ecmon('0') + cpm_dir('B:'), ['.']),
    'cpm_zrmd_mfc1a': (ecmon('2') + cpm_dir('B:'), ['.']),
    'cpm_ec1045': (ecmon('1') + cpm_dir(), ['.COM']),
    'cpm_zrmd_dpp1': (ecmon('3') + cpm_dir('D:'), ['.']),
    'cpm_zrmd_rtc_dsp': (ecmon('3') + cpm_dir('E:'), ['RTCZ']),
    'sapi1v': (ecmon('3') + cpm_dir('B:'), ['.']),
    'sapi1v_BomberNet': (ecmon('3') + cpm_dir('B:'), ['.']),
    'libor_doma': (ecmon('3') + cpm_dir(), ['.COM']),
    'sapi1v_PoP': (ecmon('3') + t('POP\\r') + run(15000), ['shot:CGA-1V']),
    'sapi1v_hry': (ecmon('3') + t('FLAPDGD\\r') + run(8000), ['shot:DGD-1V']),
    'cpm_boot_dlouhe_rpd1_fdc1': (CPM + cpm_dir('B:'), ['.']),
    'cpm_boot_dlouhe_rpd1_fdc1_35': (CPM + cpm_dir('B:'), ['.']),
    'cpm_boot_fdc1': (CPM + cpm_dir(), [' MAC']),
    'cpm_boot_mfc1a': (CPM + cpm_dir(), [' MAC']),
    'cpm_boot_rpd1': (CPM + cpm_dir(), [' MAC']),
    'cpm_boot_fdc1_ss256': (UCPM + cpm_dir(), [' COM']),
    'cpm_boot_rpd1_ss256': (UCPM + cpm_dir(), [' COM']),
    'cpm_boot_mfc1a_rmd': (CPM + cpm_dir(), [' COM']),
    'cpm_boot_fdc1_irmd': (CPM + t('SAVE 2 C:X.COM\\r') + run(2000) + cpm_dir('C:'), ['X        COM']),
    'cpm_boot_fdc1_rmd': (CPM + cpm_dir('B:'), ['IRMD40']),
    'cpm_boot_rpd1_rmd': (CPM + cpm_dir('B:'), ['IRMD40']),
    'cpm_boot_rpd1_mfc256': (CPM + cpm_dir('B:'), ['IRMD40']),
    'cpm_boot_rpd1_mfc512': (CPM + cpm_dir('B:'), ['IRMD40']),
    'cpm_boot_rpd1_fdc1_rmd': (CPM + cpm_dir('B:'), ['IRMD40']),
    'cpm_pmd_fdc1': (UCPM + cpm_dir('C:'), ['TURBO']),
    'cpm_pmd_rpd1': (UCPM + cpm_dir('C:'), ['TURBO']),
    'cpm_term64k_fdc1': (run(8000) + ['--serial-send', 'DIR\\r'] + run(3000) + ['--print-serial'], ['serial: COM']),
    'rkd1_tesla_cpm': (w('(1-15)', 30000) + run(2000) + t('1\\r') + w('oblasti:', 20000) + run(1000) +
                       t('MARTIN\\r') + w('b1$', 30000) + t('DIR A:\\r') + run(8000), ['MARTIN', 'b1$', ' COM']),
    'rkd1_jb_cm5400': (CPM + cpm_dir('B:'), ['FORMRKD']),
    'rmp1_jb_izot': (CPM + t('B:DIRMT 0\\r') + run(20000), ['SAPI01']),
    'plotter_ar1990': (run(3000), ['log-clean']),
    'jpr1s_bgp1s': (MANIC, ['shot:BGP-1S']),
    'jpr1x_bgp1s': (MANIC, ['shot:BGP-1S']),
    'zps_MIKROBASIC_AR1983': (BASIC, ['42']),
    'zps_ZPS-1': (BASIC, ['42']),
    'zps_ZPS-1_JPR-1S_AND-1B': (BASIC, ['42']),
    'zps_ZPS-1_JPR-1X': (BASIC, ['42']),
    'zps_ZPS-2': (MIKOS, ['0000']),
    'zps_ZPS-2_DPS-1_IMS-2': (MIKOS, ['0000']),
    'zps_ZPS-2_MIKOS_D': (run(5000) + MIKOS, ['0000']),
    'zps_ZPS-2_MIKOS_K': (run(3000) + t('KI') + w('NAZEV:', 60000) + t('MOJE\\r') + run(4000) + t('KD') + run(30000),
                          ['00 MOJE']),
    'zps_ZPS-2_MIKOS_V5': (MIKOS, ['0000']),
    'zps_ZPS-2_MIKOS_V5_DGD-1V': (MIKOS, ['0000']),
    'zps_ZPS-2_MIKOS_V5_Minigraf': (MIKOS, ['0000']),
    'zps_ZPS-2_MIKOS_V5_XY4130': (run(5000) + MIKOS, ['L=XY4130', '0000']),   # the carriage to the left stop first
    'zps_ZPS-3': (CPM + cpm_dir(), [' COM']),
    'zps_ZPS-3_CPM48K': (CPM + cpm_dir(), [' COM']),
    'zps_ZPS-3_CPM48K_FDC-1': (CPM + cpm_dir(), [' COM']),
    'zps_ZPS-3_CPM48K_IMA': (CPM + cpm_dir('B:'), ['.']),
    'zps_ZPS-3_CPM62K': (CPM + cpm_dir(), [' COM']),
    'zps_ZPS-3_CPM62K_FDC-1': (CPM + cpm_dir(), [' COM']),
    'zps_ZPS-3_CPM64K_IMA': (CPM + t('B:\\r') + run(1500) + t('MBASIC\\r') + w('BYTES FREE', 90000) +
                             t('PRINT 6*7\\r') + run(2000), [' 42']),
    'zps_ZPS-4': (CPM + cpm_dir(), [' COM']),
    'zps_ZPS-4_ELIS': (CPM + cpm_dir(), [' COM']),
    'zps_ZPS-4_FDC-1': (CPM + cpm_dir(), [' COM']),
    'zps_ZPS-6': (CPM + cpm_dir(), ['SETDBS1']),
}


def png_colours(path):
    """Distinct colours of an 8-bit RGB/RGBA PNG of the emulator (sampled, up to 5)."""
    d = open(path, 'rb').read()
    pos, wd, ht, ct, idat = 8, 0, 0, 0, b''
    while pos < len(d):
        ln = struct.unpack('>I', d[pos:pos + 4])[0]
        tp = d[pos + 4:pos + 8]
        body = d[pos + 8:pos + 8 + ln]
        if tp == b'IHDR':
            wd, ht, _, ct = struct.unpack('>IIBB', body[:10])
        elif tp == b'IDAT':
            idat += body
        pos += 12 + ln
    raw = zlib.decompress(idat)
    bpp = 4 if ct == 6 else 3
    stride = wd * bpp + 1
    seen = set()
    for y in range(0, ht, 7):
        row = raw[y * stride + 1:(y + 1) * stride]
        for x in range(0, wd * bpp, bpp * 5):
            seen.add(row[x:x + 3])
            if len(seen) > 4:
                return len(seen)
    return len(seen)


def cli_run(cli, sb, machine, actions, log):
    args = [cli, '--data-root', sb, '--machine', machine] + actions
    try:
        r = subprocess.run(args, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=1800, cwd=sb)
        out = r.stdout + r.stderr
    except subprocess.TimeoutExpired:
        out = 'timeout waiting for the process'
    open(log, 'w', encoding='utf-8').write(out)
    return out


def problems(out):
    """Timeouts of --wait-text and warnings of the log; the output without the cli's own messages."""
    lines = out.splitlines()
    bad = [l for l in lines if l.startswith('timeout waiting')]
    bad += [l for l in lines if l.startswith('[warn]') or l.startswith('[error]')]
    return bad, '\n'.join(l for l in lines if not l.startswith('timeout waiting'))


def make_sandbox(debug):
    stamp = time.strftime('%Y%m%d-%H%M%S')
    sb = os.path.join(ROOT, 'out', 'check', stamp)
    os.makedirs(sb)
    arc = subprocess.run(['git', 'archive', 'HEAD', 'roms', 'media', 'machines'], cwd=ROOT, capture_output=True,
                         check=True).stdout
    tar = os.path.join(sb, 'data.tar')
    open(tar, 'wb').write(arc)
    subprocess.run(['tar', '-xf', 'data.tar'], cwd=sb, check=True)
    os.remove(tar)
    ide = os.path.join(ROOT, 'work', 'ide', 'sapi_hdd.img')
    if os.path.exists(ide):
        os.makedirs(os.path.join(sb, 'work', 'ide'))
        shutil.copy(ide, os.path.join(sb, 'work', 'ide'))
    return sb


def check_machines(cli, sb, filt):
    out_dir = os.path.join(sb, 'check')
    os.makedirs(out_dir, exist_ok=True)
    files = sorted(glob.glob(os.path.join(sb, 'machines', '*.sapi')) + glob.glob(os.path.join(sb, 'machines', 'zps', '*.sapi')))
    ok = bad = 0
    for f in files:
        rel = os.path.relpath(f, sb).replace('\\', '/')
        name = rel.replace('machines/', '').replace('/', '_').replace('.sapi', '')
        if filt and filt not in name:
            continue
        if name not in MACHINES:
            print('%-36s NO SCENARIO (add it to tools/check_machines.py)' % name)
            bad += 1
            continue
        acts, exp = MACHINES[name]
        acts = list(acts)
        shot = [e[5:] for e in exp if e.startswith('shot:')]
        png = os.path.join(out_dir, name + '.png')
        if shot:
            acts += ['--display', shot[0], '--screenshot', png]
        out = cli_run(cli, sb, rel, acts + ['--print-screen'], os.path.join(out_dir, name + '.txt'))
        fails, text = problems(out)
        for e in exp:
            if e.startswith('shot:'):
                n = png_colours(png) if os.path.exists(png) else 0
                if n < 3:
                    fails.append('screenshot %s: %d colours' % (e[5:], n))
            elif e.startswith('serial:'):
                if e[7:] not in text:
                    fails.append('serial line lacks %r' % e[7:])
            elif e == 'log-clean':
                pass   # warnings are in fails already
            elif e not in text:
                fails.append('screen lacks %r' % e)
        if fails:
            bad += 1
            print('%-36s FAIL %s' % (name, '; '.join(fails)[:200]))
        else:
            ok += 1
            print('%-36s OK' % name)
    return ok, bad


def variant(sb, src, dst, image=None):
    """A machine of the V games with another RAM disk image, never saved."""
    s = open(os.path.join(sb, 'machines', src), encoding='utf-8').read()
    s = s.replace('"mode": "rw"', '"mode": "nosave"')
    if image:
        s = re.sub(r'"image": "media/zrmd-1/[^"]*"', '"image": "%s"' % image, s, count=1)
    open(os.path.join(sb, 'machines', dst), 'w', encoding='utf-8').write(s)
    return 'machines/' + dst


def check_games(cli, sb):
    out_dir = os.path.join(sb, 'games')
    os.makedirs(out_dir, exist_ok=True)
    # the machine of the release with all games and demos; Prince of Persia only from the disk of the repository
    hry = variant(sb, 'sapi1v_hry.sapi', 'check_hry.sapi')
    pop = variant(sb, 'sapi1v_hry.sapi', 'check_pop.sapi', 'media/zrmd-1/ramdisk_pop-fxs4-flappy-bomberman-bkp.bin')
    ok = bad = 0

    def report(name, fails):
        nonlocal ok, bad
        if fails:
            bad += 1
            print('%-10s FAIL %s' % (name, '; '.join(fails)[:200]))
        else:
            ok += 1
            print('%-10s OK' % name)

    # Bad Apple!! to its end (3:39): it comes back to CP/M, the picture changes until then
    acts = ecmon('3') + t('BADAPPLE\\r')
    for k in range(1, 9):
        acts += run(30000) + ['--display', 'CGA-1V', '--screenshot', os.path.join(out_dir, 'badapple_%03d.png' % (k * 30))]
    out = cli_run(cli, sb, hry, acts + ['--print-screen'], os.path.join(out_dir, 'badapple.txt'))
    fails, text = problems(out)
    tail = text[text.find('A>BADAPPLE'):] if 'A>BADAPPLE' in text else ''
    if tail.count('A>') < 2:
        fails.append('not back in CP/M after 4 minutes')
    shots = [open(os.path.join(out_dir, 'badapple_%03d.png' % (k * 30)), 'rb').read() for k in range(1, 7)]
    if len(set(shots)) < 5:
        fails.append('the picture does not change')
    report('BADAPPLE', fails)

    # FXS4: tunes A-Z for 20 s each, the sound of each must not be silent
    wav = os.path.join(out_dir, 'fxs4.wav')
    letters = [chr(c) for c in range(ord('A'), ord('Z') + 1)]
    acts = ecmon('3') + t('FXS4\\r') + run(5000) + ['--audio-wav', wav]
    for L in letters:
        acts += t(L) + run(20000)
    acts += ['--display', 'CGA-1V', '--screenshot', os.path.join(out_dir, 'fxs4.png')]
    out = cli_run(cli, sb, hry, acts, os.path.join(out_dir, 'fxs4.txt'))
    fails, text = problems(out)
    try:
        wf = wave.open(wav)
        rate, ch = wf.getframerate(), wf.getnchannels()
        data = wf.readframes(wf.getnframes())
        smp = struct.unpack('<%dh' % (len(data) // 2), data)[::ch]
        seg = rate * 20
        quiet = []
        for i, L in enumerate(letters):
            part = smp[i * seg:(i + 1) * seg:7]
            rms = math.sqrt(sum(x * x for x in part) / len(part)) if part else 0.0
            if rms < 100:
                quiet.append(L)
        if quiet:
            fails.append('silent tunes: ' + ''.join(quiet))
    except Exception as ex:
        fails.append('no sound: %s' % ex)
    report('FXS4', fails)

    # the games: out of the title with Enter and space (Flappy on DGD-1V: the fire of the joystick), the picture
    # changes, no warnings
    keys = t('\\r') + run(1000) + t(' ') + run(4000)
    fire = (['--joystick', 'fire1'] + run(300) + ['--joystick', '-'] + run(1500) +   # the title: SELECT SCENE
            ['--joystick', 'fire1'] + run(300) + ['--joystick', '-'] + run(2900))    # scene 1
    for name, machine, disp, cmd, start in (('POP', pop, 'CGA-1V', 'POP', keys),
                                            ('BOMBER', hry, 'CGA-1V', 'BOMBER', keys),
                                            ('FLAPPY', hry, 'CGA-1V', 'FLAPPY', keys),
                                            ('FLAPDGD', hry, 'DGD-1V', 'FLAPDGD', fire)):
        pngs = [os.path.join(out_dir, '%s_%02d.png' % (name.lower(), s)) for s in (5, 15, 30, 60)]
        acts = (ecmon('3') + t(cmd + '\\r') + run(5000) + ['--display', disp, '--screenshot', pngs[0]] + run(5000) +
                start + ['--display', disp, '--screenshot', pngs[1]] + run(15000) +
                ['--display', disp, '--screenshot', pngs[2]] + run(30000) + ['--display', disp, '--screenshot', pngs[3]])
        out = cli_run(cli, sb, machine, acts, os.path.join(out_dir, name.lower() + '.txt'))
        fails, text = problems(out)
        imgs = [open(p, 'rb').read() for p in pngs if os.path.exists(p)]
        if len(imgs) < 4 or len(set(imgs)) < 3 or png_colours(pngs[2]) < 3:
            fails.append('the picture does not change')
        report(name, fails)

    # the demos of L. Lasota on the games disk: pictures on CGA-1V and DGD-1V, the music players sound
    for name, cmd, disp, ms in (('CASTLE', 'CASTLE', 'CGA-1V', 5000), ('REMBRAN2', 'REMBRAN2', 'CGA-1V', 6000),
                                ('Y1STDEMO', 'Y1STDEMO', 'DGD-1V', 8000)):
        png = os.path.join(out_dir, name.lower() + '.png')
        out = cli_run(cli, sb, hry, ecmon('3') + t(cmd + '\\r') + run(ms) + ['--display', disp, '--screenshot', png],
                      os.path.join(out_dir, name.lower() + '.txt'))
        fails, text = problems(out)
        if not os.path.exists(png) or png_colours(png) < 2:
            fails.append('no picture on ' + disp)
        report(name, fails)
    for name, cmd in (('SPLAY', 'SPLAY HELLFIRE.VGM'), ('RAT', 'RAT SMJOOP.RAD'), ('SNDDEMON', 'SNDDEMON')):
        wav = os.path.join(out_dir, name.lower() + '.wav')
        out = cli_run(cli, sb, hry, ecmon('3') + ['--audio-wav', wav] + t(cmd + '\\r') + run(10000),
                      os.path.join(out_dir, name.lower() + '.txt'))
        fails, text = problems(out)
        try:
            wf = wave.open(wav)
            data = wf.readframes(wf.getnframes())
            smp = struct.unpack('<%dh' % (len(data) // 2), data)[::7]
            if not smp or max(abs(x) for x in smp) < 1000:
                fails.append('silent')
        except Exception as ex:
            fails.append('no sound: %s' % ex)
        report(name, fails)
    return ok, bad


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    debug = '--debug' in sys.argv
    preset = 'windows-debug' if debug else 'windows-release'
    cli = os.path.join(ROOT, 'out', 'build', preset, 'sapiemu-cli.exe')
    if not os.path.exists(cli):
        cli = os.path.join(ROOT, 'out', 'build', 'windows-debug', 'sapiemu-cli.exe')
    print('cli: ' + cli)
    sb = make_sandbox(debug)
    print('data: ' + sb)
    ok = bad = 0
    if '--games-only' not in sys.argv:
        ok, bad = check_machines(cli, sb, args[0] if args else '')
        print('machines: OK %d, FAIL %d' % (ok, bad))
    if '--games' in sys.argv or '--games-only' in sys.argv:
        gok, gbad = check_games(cli, sb)
        print('games and demos: OK %d, FAIL %d' % (gok, gbad))
        bad += gbad
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())

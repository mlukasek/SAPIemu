"""Records the cassette of the DPS-1 machine (docs/navod-dps1.md) in the emulator, with the original MIKOS V4.0F
and BASIMS V4.0 doing the writing:
  file 1 BASIMS   BASIMS V4.0 (4400h-635Fh) saved by the monitor: .K_A, .K_S 4400 635F (load: .K_L BASIMS, .J)
  file 2 PRIKLAD  the example of the BASIMS manual, saved by BASIC SAVE (load: LOAD, PRIKLAD)
  file 3 SMYCKA   measuring in a loop, the value as a number
The BASIC programs are text files (MIKOS blocks 'A'), written behind BASIMS as on a real cassette:
BYE, .K_A (open the next file), .G4403 (the warm start of BASIMS: the program stays; .B and .G4400 are the cold
start and clear it), SAVE.

python tools/dps1_kazeta.py [OUT.wav]   (default work/dps1/BASIMS_IMS2.wav; needs sapiemu-cli built)
media/dps-1/BASIMS_IMS2.wav is this WAV at 22050 Hz, 8 bits (ffmpeg -ar 22050 -acodec pcm_u8), 1.3 MB; an MP3 breaks
the splice between two recordings (the header of the next file is found, its data not)."""
import json
import os
import subprocess
import sys
import time
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MACHINE = os.path.join(ROOT, 'machines', 'zps', 'ZPS-2_DPS-1_IMS-2.sapi')
PORT = 8597

PRIKLAD = [
    '10 REM IMS-2 M1T330 ADR 3',
    '20 IMS# N',
    '30 IMS# O3;"R4D0E"',
    '40 IMS# T3',
    '50 IMS# I A$',
    '60 PRINT A$',
]
SMYCKA = [
    '10 REM MERENI VE SMYCCE, CTRL+C KONEC',
    '20 IMS# N',
    '30 IMS# O3;"R4D0E"',
    '40 IMS# T3',
    '50 IMS# I A$',
    '60 U=VAL(MID$(A$,2))',
    '70 PRINT A$,U',
    '80 GOTO 30',
]


def cli_exe():
    for preset in ('windows-release', 'windows-debug', 'linux-release', 'linux-debug'):
        for name in ('sapiemu-cli.exe', 'sapiemu-cli'):
            p = os.path.join(ROOT, 'out', 'build', preset, name)
            if os.path.exists(p):
                return p
    sys.exit('sapiemu-cli not built (tools\\build.cmd release)')


sid = None
nid = 0


def rpc(method, params=None):
    global sid, nid
    nid += 1
    body = json.dumps({'jsonrpc': '2.0', 'id': nid, 'method': method, 'params': params or {}}).encode()
    req = urllib.request.Request('http://127.0.0.1:%d/mcp' % PORT, body,
                                 {'Content-Type': 'application/json', 'Accept': 'application/json, text/event-stream'})
    if sid:
        req.add_header('Mcp-Session-Id', sid)
    with urllib.request.urlopen(req, timeout=600) as r:
        sid = r.headers.get('Mcp-Session-Id') or sid
        t = r.read().decode('utf-8')
    if t.startswith('event:') or t.startswith('data:'):
        t = [l[5:] for l in t.splitlines() if l.startswith('data:')][-1]
    return json.loads(t)


def tool(name, **args):
    r = rpc('tools/call', {'name': name, 'arguments': args})
    res = r.get('result', {})
    text = ''.join(c.get('text', '') for c in res.get('content', []))
    if res.get('isError'):
        raise RuntimeError('%s: %s' % (name, text))
    return text


def until(text, ms=20000):
    r = tool('run_until', screen_text=text, timeout_ms=ms)
    if '"success": false' in r.replace(' ', ' ') or '"success":false' in r:
        raise RuntimeError('no "%s" on the screen:\n%s' % (text, tool('screen_text')))


def answer(prompt, keys, ms=20000):
    until(prompt, ms)
    tool('run_for', ms=600)   # MIKOS takes a key only a while after the question
    tool('type_text', text=keys)


def command(keys, echo, tries=5):
    """A monitor command: typed until its echo shows (MIKOS loses keys while it is busy)."""
    before = tool('screen_text').count(echo)
    for _ in range(tries):
        tool('run_for', ms=1000)
        tool('type_text', text=keys)
        tool('run_for', ms=800)
        if tool('screen_text').count(echo) > before:
            return
    raise RuntimeError('MIKOS does not take %s:\n%s' % (keys, tool('screen_text')))


def motor(start):
    st = json.loads(tool('tape_status'))
    for r in st.get('recorders', []):
        t = r.get('tape', r)
        if t.get('motor'):
            return True
    return False


def motor_stops():
    """The program runs the recorder (it may start a while after the command) until it stops it."""
    for _ in range(10):
        if motor(True):
            break
        tool('run_for', ms=300)
    for _ in range(600):
        if not motor(False):
            return
        tool('run_for', ms=500)
    raise RuntimeError('the recorder runs on')


def basic_lines(lines):
    for l in lines:
        tool('type_text', text=l + '\r')
        until(l[:8], 20000)
        tool('run_for', ms=400)


def save_basic(name):
    tool('type_text', text='BYE\r')
    until('\n.', 5000)
    command('KA', '.K_A')
    answer('RECORD  DONE?', 'Y')
    answer('NEW  NAME', name + '\r')
    motor_stops()
    tool('run_for', ms=500)
    command('G', '.G')   # the warm start of BASIMS keeps the program
    tool('type_text', text='4403' + chr(13))
    r = tool('run_until', screen_text='READY', timeout_ms=5000)
    if '"success": true' not in r and '"success":true' not in r:
        raise RuntimeError('no BASIC after .B:\n' + tool('screen_text'))
    tool('run_for', ms=500)
    if os.environ.get('KAZETA_DEBUG'):
        tool('type_text', text='LIST\r')
        tool('run_for', ms=3000)
        print('--- before SAVE:\n' + tool('screen_text'))
    tool('type_text', text='SAVE\r')
    tool('run_for', ms=1500)
    motor_stops()
    until('READY', 20000)
    tool('run_for', ms=500)


def main():
    out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'work', 'dps1', 'BASIMS_IMS2.wav'))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    if os.path.exists(out):
        sys.exit('%s exists: give another name' % out)
    log = open(out + '.log', 'w', encoding='utf-8')
    proc = subprocess.Popen([cli_exe(), '--machine', MACHINE, '--mcp', '--mcp-port', str(PORT)], cwd=ROOT,
                            stdout=log, stderr=subprocess.STDOUT)
    try:
        for _ in range(100):
            try:
                rpc('initialize', {'protocolVersion': '2025-03-26', 'capabilities': {},
                                   'clientInfo': {'name': 'dps1_kazeta', 'version': '1'}})
                break
            except Exception:
                time.sleep(0.2)
        tool('pause')
        until('SAPI-1 V4.0', 10000)
        tool('run_for', ms=500)
        tool('load_hex', path=os.path.join(ROOT, 'media', 'dps-1', 'BASIMS.hex'))
        tool('tape_insert', path=out, new=True, write_protect=False)
        tool('tape_control', action='record')
        # file 1: BASIMS by the monitor
        command('KA', '.K_A')
        answer('RECORD  DONE?', 'Y')
        answer('NEW  NAME', 'BASIMS\r')
        motor_stops()
        command('KS', '.K_S')
        answer('=', '4400 635F\r')
        tool('run_for', ms=1500)
        motor_stops()
        tool('run_for', ms=500)
        # BASIMS runs, the programs behind it
        command('G', '.G')
        tool('type_text', text='4400\r')
        until('READY', 5000)
        tool('run_for', ms=500)
        basic_lines(PRIKLAD)
        save_basic('PRIKLAD')
        tool('type_text', text='NEW\r')
        tool('run_for', ms=1000)
        basic_lines(SMYCKA)
        save_basic('SMYCKA')
        tool('tape_control', action='stop')
        tool('tape_control', action='save')
        print(tool('screen_text'))
        print(tool('tape_status'))
        tool('tape_control', action='eject')
    finally:
        try:
            tool('quit')
        except Exception:
            pass
        try:
            proc.wait(10)
        except Exception:
            proc.kill()
    print('written', out)


main()

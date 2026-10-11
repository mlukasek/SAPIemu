"""Runs a test ROM in SAPIemu the way the PicoROM serves it on the real board (2 KB repeated over the 28C64 of the
JPR-1V) in hwbench_v.sapi, and prints what came on the serial channel (the ROM goes to work/hwbench/rom_8k.bin).
python emu_run.py ROM.bin [seconds] [channel: 0 = A 10h, 1 = B 14h] [--hex] [--iolog: the I/O log instead]
[--regs] [--slow: TURBO off, CPU 2 MHz]"""
import json
import os
import subprocess
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
WORK = os.path.join(ROOT, 'work', 'hwbench')
args = [a for a in sys.argv[1:] if not a.startswith('--')]
ROM = args[0]
SECONDS = float(args[1]) if len(args) > 1 else 0.5
CHANNEL = int(args[2]) if len(args) > 2 else 0
HEX = '--hex' in sys.argv
PORT = 8599
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
    if 'data:' in t:
        t = [l[5:] for l in t.splitlines() if l.startswith('data:')][-1]
    return json.loads(t)


def tool(name, **a):
    r = rpc('tools/call', {'name': name, 'arguments': a})
    res = r.get('result', {})
    text = ''.join(c.get('text', '') for c in res.get('content', []))
    if res.get('isError'):
        raise RuntimeError('%s: %s' % (name, text))
    return text


data = open(ROM, 'rb').read()
os.makedirs(WORK, exist_ok=True)
open(os.path.join(WORK, 'rom_8k.bin'), 'wb').write((data * (8192 // len(data)))[:8192])
machine = os.path.join(HERE, 'hwbench_v.sapi')
if '--slow' in sys.argv:   # TURBO off: CPU 2 MHz
    machine = os.path.join(WORK, 'hwbench_v_2mhz.sapi')
    open(machine, 'w', encoding='utf-8').write(
        open(os.path.join(HERE, 'hwbench_v.sapi'), encoding='utf-8').read().replace('"turbo": true', '"turbo": false'))
proc = subprocess.Popen([os.path.join(ROOT, 'out', 'build', 'windows-debug', 'sapiemu-cli.exe'), '--machine',
                         machine, '--mcp', '--mcp-port', str(PORT)], cwd=ROOT,
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        try:
            rpc('initialize', {'protocolVersion': '2025-03-26', 'capabilities': {},
                               'clientInfo': {'name': 'emu_run', 'version': '1'}})
            break
        except Exception:
            time.sleep(0.2)
    tool('pause')                       # the cli runs from its start: begin again from a reset
    tool('serial_receive', channel=CHANNEL)
    tool('io_log', enable='--iolog' in sys.argv, clear=True)
    tool('reset')
    tool('run_for', ms=int(SECONDS * 1000))
    if '--mem9000' in sys.argv:   # 16 bytes of results a test ROM left at 9000h
        t = tool('read_memory', address='9000', length=16, format='hex')
        print('MEM9000 ' + ''.join(ch for ch in t if ch in '0123456789abcdefABCDEF'))
    if '--regs' in sys.argv:
        print(tool('get_registers'))
    if '--iolog' in sys.argv:
        print(tool('io_log', count=5000))
    else:
        print(tool('serial_receive', channel=CHANNEL, hex=HEX))
finally:
    try:
        tool('quit')
    except Exception:
        pass
    proc.wait(10)

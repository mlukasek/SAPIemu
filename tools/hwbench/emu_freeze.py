"""freeze.asm / freeze_and.asm in SAPIemu (hwbench_v.sapi): a byte comes on DSM-1V channel A 50 ms after the reset;
prints what came back on the line ("NOFREEZE" = the interrupt was handled) and where the CPU is.
python emu_freeze.py [freeze freeze_and]"""
import json
import os
import subprocess
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
WORK = os.path.join(ROOT, 'work', 'hwbench')
PORT = 8603
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
    return ''.join(c.get('text', '') for c in r.get('result', {}).get('content', []))


for rom in sys.argv[1:] or ['freeze', 'freeze_and']:
    data = open(os.path.join(WORK, rom + '.bin'), 'rb').read()
    open(os.path.join(WORK, 'rom_8k.bin'), 'wb').write(data * 4)
    sid = None
    proc = subprocess.Popen([os.path.join(ROOT, 'out', 'build', 'windows-debug', 'sapiemu-cli.exe'), '--machine',
                             os.path.join(HERE, 'hwbench_v.sapi'), '--mcp', '--mcp-port', str(PORT)], cwd=ROOT,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(100):
            try:
                rpc('initialize', {'protocolVersion': '2025-03-26', 'capabilities': {},
                                   'clientInfo': {'name': 'freeze', 'version': '1'}})
                break
            except Exception:
                time.sleep(0.2)
        tool('pause')
        tool('serial_receive', channel=0)
        tool('reset')
        tool('run_for', ms=50)
        tool('serial_send', channel=0, text='Z')
        tool('run_for', ms=50)
        regs = json.loads(tool('get_registers'))
        print('%-11s line: %r  PC %s, next %s' % (rom, tool('serial_receive', channel=0), regs.get('pc'),
                                                  regs.get('next_instruction')))
    finally:
        try:
            tool('quit')
        except Exception:
            pass
        proc.wait(10)

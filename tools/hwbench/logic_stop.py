"""Stops captures left running in Logic 2 (a killed script leaves a trigger capture waiting, which blocks new ones).
Capture ids are small integers; tries 0..30 through the Logic 2 MCP server."""
import json
import urllib.request

sid = None


def rpc(method, params, nid):
    global sid
    body = json.dumps({'jsonrpc': '2.0', 'id': nid, 'method': method, 'params': params}).encode()
    req = urllib.request.Request('http://127.0.0.1:10530/mcp', body,
                                 {'Content-Type': 'application/json', 'Accept': 'application/json, text/event-stream'})
    if sid:
        req.add_header('Mcp-Session-Id', sid)
    with urllib.request.urlopen(req, timeout=10) as r:
        sid = r.headers.get('Mcp-Session-Id') or sid
        t = r.read().decode('utf-8')
    if 'data:' in t:
        t = [l[5:] for l in t.splitlines() if l.startswith('data:')][-1]
    return json.loads(t) if t.strip() else {}


rpc('initialize', {'protocolVersion': '2025-03-26', 'capabilities': {}, 'clientInfo': {'name': 'stop', 'version': '1'}}, 1)
for i in range(31):
    r = rpc('tools/call', {'name': 'stop_capture', 'arguments': {'captureId': i}}, 10 + i)
    if not r.get('result', {}).get('isError'):
        print('stopped capture', i)

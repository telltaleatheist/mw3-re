# readaemon.py <log-prefix>: keep one `rea mcp` session open on Mech3.exe and serve tool calls on 127.0.0.1:47123.
# Each connection sends one JSON line {"name":..., "arguments":...} and gets one JSON line back.
import json, os, socket, subprocess, sys, threading
sock_path = sys.argv[1]
env = dict(os.environ,
    PATH=os.path.expanduser('~/.nvm/versions/node/v24.22.0/bin:') + os.environ['PATH'],
    GHIDRA_INSTALL_DIR='/opt/homebrew/opt/ghidra/libexec',
    JAVA_HOME='/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home',
    REA_GHIDRA_STARTUP_TIMEOUT_MS='1800000')
p = subprocess.Popen(['nice', '-n', '15', os.path.expanduser('~/.nvm/versions/node/v24.22.0/bin/rea'), 'mcp'],
                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=open(sock_path + '.stderr', 'w'), env=env, text=True)
lock = threading.Lock(); n = 0
def rpc(method, params=None, notify=False):
    global n
    msg = {'jsonrpc': '2.0', 'method': method}
    if params is not None: msg['params'] = params
    if not notify: n += 1; msg['id'] = n
    p.stdin.write(json.dumps(msg) + '\n'); p.stdin.flush()
    if notify: return None
    while True:
        line = p.stdout.readline()
        if not line: raise SystemExit('rea mcp exited')
        r = json.loads(line)
        if r.get('id') == n: return r
rpc('initialize', {'protocolVersion': '2025-06-18', 'capabilities': {}, 'clientInfo': {'name': 'mw3-re', 'version': '0'}})
rpc('notifications/initialized', notify=True)
print(rpc('tools/call', {'name': 'open_binary', 'arguments': {'path': str(__import__('pathlib').Path(__file__).resolve().parents[2] / 'game' / 'mw3-1.2' / 'Mech3.exe'), 'provider_id': 'ghidra'}}), flush=True)
srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); srv.bind(('127.0.0.1', 47123)); srv.listen(4)
print('ready', flush=True)
while True:
    c, _ = srv.accept()
    f = c.makefile('rw')
    req = json.loads(f.readline())
    if req.get('name') == '__quit__':
        f.write('{}\n'); f.flush(); c.close(); break
    with lock:
        r = rpc('tools/call', req)
    f.write(json.dumps(r.get('result', r.get('error'))) + '\n'); f.flush(); c.close()
p.stdin.close(); p.wait(timeout=120)

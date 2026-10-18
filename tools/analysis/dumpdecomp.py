# dumpdecomp.py <procs.json> <outdir>: decompile every non-external, non-thunk procedure through the REA daemon
import json, os, socket, sys, time
procs = json.load(open(sys.argv[1])); out = sys.argv[2]; os.makedirs(out, exist_ok=True)
addrs = [p['address'] for p in procs if not p['address'].startswith('EXTERNAL') and not p['procedure']['thunk']]
todo = [a for a in addrs if not os.path.exists(os.path.join(out, a + '.c'))]
print(len(addrs), 'total', len(todo), 'to do', flush=True)
def call(req):
    s = socket.socket(); s.connect(('127.0.0.1', 47123)); f = s.makefile('rw')
    f.write(json.dumps(req) + '\n'); f.flush(); res = json.loads(f.readline()); s.close()
    return json.loads(''.join(c.get('text', '') for c in res.get('content') or []))
fails = open(os.path.join(out, '_failures.txt'), 'a')
t0 = time.time()
for i in range(0, len(todo), 100):
    chunk = todo[i:i + 100]
    d = call({'name': 'batch_decompile', 'arguments': {'addresses': chunk}})
    for it in (d.get('normalized_result') or {}).get('items', []):
        if it.get('status') == 'ok':
            open(os.path.join(out, it['address'] + '.c'), 'w').write(f"// Mech3.exe 1.2 @ {it['address']} {it['procedure'].get('name')}\n" + it['pseudocode'])
        else:
            fails.write(json.dumps(it)[:500] + '\n'); fails.flush()
    print(f'{i + len(chunk)}/{len(todo)} {time.time() - t0:.0f}s', flush=True)
print('done', flush=True)

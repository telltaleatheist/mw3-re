# filemap.py: map each original source-file path string to the functions that reference it
import json, re, socket, bisect
def call(name, args):
    s = socket.socket(); s.connect(('127.0.0.1', 47123)); f = s.makefile('rw')
    f.write(json.dumps({'name': name, 'arguments': args}) + '\n'); f.flush(); res = json.loads(f.readline()); s.close()
    d = json.loads(''.join(c.get('text', '') for c in res.get('content') or []))
    r = d.get('raw_result', d) if isinstance(d, dict) else d
    return r
strings = json.load(open('strings.json'))
procs = json.load(open('procs.json'))
entries = sorted(int(p['address'], 16) for p in procs if not p['address'].startswith('EXTERNAL'))
def containing(a):  # nearest function entry at or below the address (fast approximation)
    i = bisect.bisect_right(entries, a) - 1
    return entries[i] if i >= 0 else None
pat = re.compile(r'(?:[A-Za-z]:\\|\.\.\\)[^\s]*?\\([^\\]+\\)?([^\\]+\.(?:c|cpp|h))$', re.I)
out = {}
for s in strings:
    m = re.search(r'([A-Za-z]:\\zipper\\.*\.(?:c|cpp|h)|\.\.\\.*\.(?:c|cpp|h))$', s['value'], re.I)
    if not m: continue
    path = m.group(1).lower().replace('c:\\zipper\\', '')
    refs = call('xrefs', {'address': s['address']}) or []
    for r in refs:
        a = int(r, 16); fn = containing(a)
        out.setdefault(path, set()).add(fn)
res = {k: sorted(hex(x) for x in v if x) for k, v in out.items()}
json.dump(res, open('filemap.json', 'w'), indent=1)
for k, v in sorted(res.items(), key=lambda kv: int(kv[1][0], 16) if kv[1] else 0):
    print(f'{len(v):4d}  {v[0] if v else "-":>10} .. {v[-1] if v else "-":>10}  {k}')

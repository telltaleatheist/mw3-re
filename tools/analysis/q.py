# q.py <tool> [json-args] : one call to the REA daemon; prints the tool's raw_result (or full text with --full)
import json, socket, sys
args = [a for a in sys.argv[1:] if a != '--full']
req = {'name': args[0], 'arguments': json.loads(args[1]) if len(args) > 1 else {}}
s = socket.socket(); s.connect(('127.0.0.1', 47123)); f = s.makefile('rw')
f.write(json.dumps(req) + '\n'); f.flush()
res = json.loads(f.readline())
text = ''.join(c.get('text', '') for c in (res.get('content') or [])) if isinstance(res, dict) else str(res)
if '--full' in sys.argv: print(text); sys.exit()
try:
    d = json.loads(text)
    out = d.get('raw_result', d) if isinstance(d, dict) else d
    if isinstance(out, dict) and set(out) <= {'value', 'limitations', 'relationship_coverage'} and 'value' in out: out = out['value']
    print(out if isinstance(out, str) else json.dumps(out, indent=1))
except Exception:
    print(text)

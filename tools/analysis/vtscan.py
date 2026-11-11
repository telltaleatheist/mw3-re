# vtscan.py: find code pointers stored in .rdata/.data (vtables, callback tables) whose targets Ghidra did not make functions
#   vtscan.py [undiscovered.json]
# Reads procs.json from analysis-output/ and writes there (git-ignored) unless a path is given: never into the
# folder it happens to be run from.
import json, os, struct, sys
AO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'analysis-output')
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(AO, 'undiscovered.json')
sys.path.insert(0, os.path.dirname(__file__)); sys.argv = sys.argv[:1]; import pe  # noqa
b = pe.b
procs = json.load(open(os.path.join(AO, 'procs.json')))
known = {int(p['address'], 16) for p in procs if not p['address'].startswith('EXTERNAL')}
text_lo, text_hi = 0x401000, 0x5bb000
targets = {}
for sva, raw, rs in pe.secs:
    if sva + 0x400000 < 0x5bb000: continue  # .rdata, .data, .rsrc only
    for off in range(0, rs - 3, 4):
        v = struct.unpack_from('<I', b, raw + off)[0]
        if text_lo <= v < text_hi:
            targets.setdefault(v, []).append(sva + 0x400000 + off)
missing = {t: refs for t, refs in targets.items() if t not in known}
# a missing target that starts with a plausible prologue is a real undiscovered function
def code(va): 
    for sva, raw, rs in pe.secs:
        if sva <= va - 0x400000 < sva + rs: return b[va - 0x400000 - sva + raw: va - 0x400000 - sva + raw + 3]
starts = {t: r for t, r in missing.items() if code(t)[:1] in (b'\x55', b'\x56', b'\x53', b'\x57', b'\x83', b'\x81', b'\x8b', b'\x6a', b'\x64', b'\xa1', b'\x33', b'\xb8', b'\xc3', b'\xd9', b'\x51', b'\x68', b'\xe9', b'\xc2', b'\x8a', b'\x0f')}
print('code pointers in data:', len(targets), 'distinct targets')
print('targets that are known functions:', len(targets) - len(missing))
print('targets Ghidra did not mark as functions:', len(missing), ' with a plausible start:', len(starts))
json.dump({hex(t): [hex(x) for x in r] for t, r in sorted(starts.items())}, open(OUT, 'w'), indent=0)
print('written to', os.path.normpath(OUT))
from collections import Counter
fm = json.load(open(os.path.join(AO, 'func2file.json')))

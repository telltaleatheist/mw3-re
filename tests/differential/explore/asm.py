# asm.py <addr> [...]: print each function's disassembly from mech3.asm (to the next function start).
# Run from the repo root. Calls are annotated with their target's source file.
import bisect, json, re, sys

fm = json.load(open('analysis-output/func2file.json'))
starts = sorted(int(k, 16) for k in fm)
lines = open('analysis-output/mech3.asm').read().split('\n')
index = {}
for i, line in enumerate(lines):
    m = re.match(r'^\s+([0-9a-f]+):', line)
    if m:
        index[int(m.group(1), 16)] = i
for a in sys.argv[1:]:
    s = int(a, 16)
    j = bisect.bisect_right(starts, s)
    end = starts[j] if j < len(starts) else s + 0x1000
    print(f'==== {s:#x} ({fm.get(hex(s), "?")})')
    i = index[s]
    while i < len(lines):
        m = re.match(r'^\s+([0-9a-f]+):\s*(.*)', lines[i])
        if m:
            if int(m.group(1), 16) >= end:
                break
            body = re.sub(r'\s*<\.text\+0x[0-9a-f]+>', '', m.group(2)).replace('\t', ' ')
            if body.strip() not in ('nop', 'int3'):
                print(f'{m.group(1)}: {body}')
        i += 1

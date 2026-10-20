# strs_in.py <lo> <hi>: for each function in the range, the strings its pseudocode names and the functions it calls.
# Run from the repo root.
import json, os, re, sys

fm = json.load(open('analysis-output/func2file.json'))
lo, hi = int(sys.argv[1], 16), int(sys.argv[2], 16)
for k in sorted(fm, key=lambda x: int(x, 16)):
    a = int(k, 16)
    if not (lo <= a < hi):
        continue
    p = f'analysis-output/decomp/{k}.c'
    body = open(p).read() if os.path.exists(p) else ''
    strs = []
    for m in re.finditer(r'\bs_([A-Za-z0-9_]+?)_00[0-9a-f]{6}', body):
        if m.group(1) not in strs:
            strs.append(m.group(1))
    calls = []
    for m in re.finditer(r'FUN_00([0-9a-f]{6})', body):
        c = '0x' + m.group(1)
        if c != k and c not in calls:
            calls.append(c)
    vts = sorted(set(re.findall(r'PTR_(?:FUN|LAB)_00([0-9a-f]{6})', body)))
    print(f'{k}: strs={" ".join(strs)[:160]} | calls={" ".join(calls)[:200]} | vt={" ".join(vts)}')

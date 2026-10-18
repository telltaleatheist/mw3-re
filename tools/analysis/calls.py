# calls.py <addr>: list the functions a function calls, in order, with original file and strings used
import json, re, sys, os
fm = json.load(open('func2file.json'))
src = open(f'decomp/{sys.argv[1]}.c').read()
seen = []
for m in re.finditer(r'FUN_00([0-9a-f]{6})', src):
    a = '0x' + m.group(1)
    if a in seen or a == sys.argv[1]: continue
    seen.append(a)
for a in seen:
    p = f'decomp/{a}.c'
    body = open(p).read() if os.path.exists(p) else ''
    strs = re.findall(r'\bs_([A-Za-z0-9_]+?)_00[0-9a-f]{6}', body)
    imports = sorted(set(re.findall(r'\b([A-Z][a-zA-Z]+[AW]?)\(', body)) & {'CreateFileA','RegOpenKeyExA','RegQueryValueExA','GetDriveTypeA','GetLogicalDriveStringsA','DirectDrawCreate','DirectDrawEnumerateA','DirectInputCreateA','mciSendCommandA','CoCreateInstance','LoadLibraryA','GetProcAddress','CreateThread','timeGetTime','GetTickCount','QueryPerformanceCounter','AVIFileInit','FindFirstFileA','GetVersionExA','GlobalMemoryStatus','MessageBoxA','CreateWindowExA'})
    lines = body.count('\n')
    f = (fm.get(a) or '?').replace('gamezmech3\\', 'gz/').replace('mech3\\', 'm3/')
    print(f'{a} {lines:5d}L  {f[:60]:60s} {" ".join(strs[:4])[:90]} {" ".join(imports)}')

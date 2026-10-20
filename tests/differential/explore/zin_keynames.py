# zin_keynames.py: the key-name table 0x0051c6b0 fills (0x0063cdd4, by DIK code)
# and the mouse button names (0x0051d080), from the exe: each 0x0051c5a0 call's
# ECX (message key) and EDX (fallback). Run from the repo root.
import re
import struct
import subprocess
import sys

sys.path.insert(0, 'tests/differential')
from original import EXE  # noqa: E402

data = open(EXE, 'rb').read()
pe = struct.unpack_from('<I', data, 0x3c)[0]
n = struct.unpack_from('<H', data, pe + 6)[0]
opt = struct.unpack_from('<H', data, pe + 20)[0]
secs = []
for i in range(n):
    o = pe + 24 + opt + i * 40
    vs, va, rs, raw = struct.unpack_from('<IIII', data, o + 8)
    secs.append((0x400000 + va, rs, raw))


def cstr(a):
    for va, rs, raw in secs:
        if va <= a < va + rs:
            o = raw + a - va
            return data[o:data.index(b'\0', o)].decode('latin-1')


out = subprocess.run([sys.executable, 'tests/differential/explore/asm.py', '0x51c6b0', '0x51d080'],
                     capture_output=True, text=True).stdout
ecx = edx = None
for line in out.split('\n'):
    m = re.search(r'mov (ecx|edx), 0x([0-9a-f]+)$', line)
    if m:
        if m.group(1) == 'ecx':
            ecx = int(m.group(2), 16)
        else:
            edx = int(m.group(2), 16)
    if 'call 0x51c5a0' in line:
        pending = (ecx, edx)
    m = re.search(r'mov dword ptr \[0x([0-9a-f]+)\], eax', line)
    if m:
        a = int(m.group(1), 16)
        print(f'{a:#x} {(a - 0x63cdd4) // 4:#x} {cstr(pending[0])!r} {cstr(pending[1])!r}')
print('joy', cstr(0x5ed868), cstr(0x5ed85c), 'default map name', cstr(0x5ece58))

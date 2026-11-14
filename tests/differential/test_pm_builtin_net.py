"""The built-in net on Pirate's Moon's exe (0x627c68, made by 0x4c7300, which 0x410ec0 runs at start-up; MW3's
0x604008 / 0x4c0310 / 0x410400, test_builtin_net.py): its id asked of znet (0x5acbe0, cdecl here) from [0x616458]
= 10000 in [10000, 10100), the compiled net's base (0x4c67f0) holding one, its vtable 0x5df998, and its one step the
default behaviour 0x627cc8. The port is shared (BuiltinNet, zai_brain.cpp); test_builtin_net.py checks its side
with the same expectations, which this test holds PM's exe to.

    python3 -P tests/differential/test_pm_builtin_net.py
"""
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_PM_MOVED'] = '1'  # this test's addresses are PM's own
from original import Original  # noqa: E402

ALLOC_ID, OBJECT_CTOR, START, NEW, DELETE = 0x5acbe0, 0x5ac920, 0x410ec0, 0x5ce8c6, 0x5ce8c0
BUILTIN, DEFAULT_BEHAVIOUR, VTABLE, KEPT = 0x627c68, 0x627cc8, 0x5df998, 0x616458


def main():
    cpu = Original(exe='pm')
    asked = []
    cpu.stub_function(NEW, lambda c: c.alloc(max(1, c.arg(0))))
    cpu.stub_function(DELETE, lambda c: 0)

    def alloc_id(c):  # cdecl: first, end, start
        asked.append((c.arg(0), c.arg(1), c.arg(2)))
        return 10000 + len(asked) * 3

    def object_ctor(c):  # znet's object: its id at +4, the element list empty
        cpu.write(c.ecx + 4, struct.pack('<i', c.arg(0)) + bytes(0x14))
        return c.ecx
    cpu.stub_function(ALLOC_ID, alloc_id)
    cpu.stub_function(OBJECT_CTOR, object_ctor, pop_bytes=4)
    kept = struct.unpack('<i', cpu.read(KEPT, 4))[0]
    assert kept == 10000, kept
    cpu.write(BUILTIN, bytes(0x60))
    cpu.call(START)
    vt, oid = struct.unpack('<Ii', cpu.read(BUILTIN, 8))
    begin, end, cap = struct.unpack('<III', cpu.read(BUILTIN + 0xc, 12))
    steps = [struct.unpack('<I', cpu.read(begin + 4 * i, 4))[0] for i in range((end - begin) // 4)]
    flag = cpu.read(BUILTIN + 0x18, 1)[0]
    # MW3 asks (start, start, start + 100) through ECX/EDX; PM pushes (start, start + 100, start): the same range.
    want = {'asked': [(10000, 10100, 10000)], 'id': 10003, 'vtable': VTABLE, 'steps': [DEFAULT_BEHAVIOUR],
            'capacity': 1, 'flag bit 0': 0}
    got = {'asked': asked, 'id': oid, 'vtable': vt, 'steps': steps, 'capacity': (cap - begin) // 4,
           'flag bit 0': flag & 1}
    if got != want:
        raise SystemExit(f'PM exe: {got} != {want}')
    print(f'ok: PM\'s built-in net as MW3\'s (id asked from 10000 in [10000, 10100), one step, the default '
          f'behaviour {DEFAULT_BEHAVIOUR:#x}, vtable {VTABLE:#x})')


if __name__ == '__main__':
    main()

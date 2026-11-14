"""The built-in net 0x604008 (0x4c0310, run at start-up by 0x410400; docs/ai.md, The built-in net) against the
original x86: its id asked of znet from 10000 in [10000, 10100) (0x58b1c0 over [0x5f2a48]), the compiled net's base
(0x4bf910) holding one, its vtable 0x5bcc68, and its one step the default behaviour 0x604068. On the port's side the
same with znet holding other kept nets (the first free id taken), and BrainHooks' default builtin_net giving it.

    cmake --build build && python3 tests/differential/test_builtin_net.py
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_builtin_net.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p]
ALLOC_ID, OBJECT_CTOR, CTOR, START = 0x58b1c0, 0x58aec0, 0x4c0310, 0x410400
BUILTIN, DEFAULT_BEHAVIOUR, VTABLE, KEPT = 0x604008, 0x604068, 0x5bcc68, 0x5f2a48


def main():
    cpu = Original()
    asked = []
    cpu.stub_function(0x5abd26, lambda c: c.alloc(max(1, c.arg(0))))  # operator new
    cpu.stub_function(0x5abd1a, lambda c: 0)  # operator delete

    def alloc_id(c):
        asked.append((c.arg(0), c.ecx, c.edx))
        return 10000 + len(asked) * 3

    def object_ctor(c):  # znet's object (0x58aec0): its id at +4, the element list empty
        cpu.write(c.ecx + 4, struct.pack('<i', c.arg(0)) + bytes(0x14))
        return c.ecx
    cpu.stub_function(ALLOC_ID, alloc_id, pop_bytes=4)
    cpu.stub_function(OBJECT_CTOR, object_ctor, pop_bytes=4)
    kept = struct.unpack('<i', cpu.read(KEPT, 4))[0]
    assert kept == 10000, kept
    cpu.write(BUILTIN, bytes(0x60))
    cpu.call(START)
    vt, oid = struct.unpack('<Ii', cpu.read(BUILTIN, 8))
    begin, end, cap = struct.unpack('<III', cpu.read(BUILTIN + 0xc, 12))
    steps = [struct.unpack('<I', cpu.read(begin + 4 * i, 4))[0] for i in range((end - begin) // 4)]
    flag = cpu.read(BUILTIN + 0x18, 1)[0]
    want = {'asked': [(10000, 10000, 10100)], 'id': 10003, 'vtable': VTABLE, 'steps': [DEFAULT_BEHAVIOUR],
            'capacity': 1, 'flag bit 0': 0}
    got = {'asked': asked, 'id': oid, 'vtable': vt, 'steps': steps, 'capacity': (cap - begin) // 4,
           'flag bit 0': flag & 1}
    if got != want:
        raise SystemExit(f'exe: {got} != {want}')
    # The port: the first free id from 10000 with znet holding these.
    cases = 0
    states = ([], [10000], [10000, 10001, 10003], list(range(10000, 10099)), [5, 300, 10050])
    for k, taken in enumerate(states):
        expect = next((i for i in range(10000, 10100) if i not in taken), -1)
        out = (ctypes.c_int32 * 5)()
        probe.probe_builtin_net((ctypes.c_int32 * max(1, len(taken)))(*taken), len(taken), int(k == len(states) - 1),
                               out)
        if list(out) != [expect, 1, 1, 1, 1]:
            raise SystemExit(f'port with {len(taken)} taken: {list(out)} (id {expect} expected)')
        cases += 1
    print(f'ok: the built-in net as the original (id asked from 10000 in [10000, 10100), one step, the default '
          f'behaviour, vtable {VTABLE:#x}); the port over {cases} znet states')


if __name__ == '__main__':
    main()

"""The net editor's version-105 build (0x4bed90, with its list's push 0x4bf4e0; docs/ai.md 9, 17.9) against the
original x86: the editor's version and its file's tree checked, the vehicles on zEntity's list ([0x73655c]: each
entry's vt+0x2c, then vt+0x40) whose controls (vt+0x140) use the net built before (vt+0x64) kept in order, that net
let go (0x4bf4b0: its vt+0), the reader (0x402590) asked with the header's number, the file's tree and its name, and
the kept vehicles' controls handed the new net (vt+0x04). The reader is the same stand-in on both sides (the reader
itself is test_net105.py's); the port's vehicles are its hooks. Compared: the log of those steps, the result and
the editor's +0x34.

    cmake --build build --target mw3probe && python3 tests/differential/test_net105_reload.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_build_105.argtypes = [ctypes.c_int32] * 5 + [ctypes.c_void_p, ctypes.c_int32, ctypes.c_void_p,
                                                           ctypes.c_int]
probe.probe_build_105.restype = ctypes.c_int
BUILD_105, READER, NEW, DELETE = 0x4bed90, 0x402590, 0x5abd26, 0x5abd1a
ENTITIES_BEGIN, ENTITIES_END = 0x73655c, 0x736560
NAME = b'net105test.zrd'


def main():
    cpu = Original()
    log, state = [], {}
    cpu.stub_function(NEW, lambda c: c.alloc(max(1, c.arg(0))))
    cpu.stub_function(DELETE, lambda c: 0)

    def reader(c):
        n = cpu.read(c.arg(2), 64)
        log.extend([3, c.arg(0) - (1 << 32) if c.arg(0) >= 1 << 31 else c.arg(0), n.index(0)])
        state['read'] = 0 if state['fails'] else cpu.alloc(0x40)
        return state['read']
    cpu.stub_function(READER, reader, pop_bytes=0xc)
    # The vehicles' and the old net's virtual calls.
    self_at, uses_at, give_at, drop_at = (cpu.code(b'\xc3') for _ in range(4))
    cpu.stub_function(self_at, lambda c: c.ecx)

    def uses(c):
        i = state['vehicles'].index(c.ecx)
        if state['uses'][i]:
            log.extend([1, i])
        return state['uses'][i]
    cpu.stub_function(uses_at, uses, pop_bytes=4)
    cpu.stub_function(give_at, lambda c: log.extend([4, state['vehicles'].index(c.ecx)]) and 0, pop_bytes=4)
    cpu.stub_function(drop_at, lambda c: log.append(2) and 0, pop_bytes=4)
    vtable = cpu.alloc(0x200)
    for off in range(0, 0x200, 4):
        cpu.u32(vtable + off, self_at)
    cpu.u32(vtable + 0x64, uses_at), cpu.u32(vtable + 0x4, give_at)
    net_vtable = cpu.alloc(8)
    cpu.u32(net_vtable, drop_at)
    rng = random.Random(23)
    cases = 0
    for _ in range(300):
        version = rng.choice([0x69, 0x69, 0x69, 0x6a, 0x68])
        has_tree, had_net = int(rng.random() < 0.9), int(rng.random() < 0.7)
        x18 = rng.choice([0, 5, 99, -1, rng.getrandbits(15)])
        n = rng.randrange(0, 6)
        use = [rng.choice([0, 1, 2]) for _ in range(n)]
        fails = int(rng.random() < 0.1)
        # The exe's editor (+0x18 the header's number, +0x1c its version, +0x34 the net, +0x3c the file's ref).
        edit, ref, fobj, name, tree = cpu.alloc(0x44), cpu.alloc(4), cpu.alloc(0x20), cpu.alloc(16), cpu.alloc(16)
        cpu.write(edit, bytes(0x44)), cpu.write(fobj, bytes(0x20)), cpu.write(name, NAME + b'\0')
        cpu.u32(edit + 0x18, x18), cpu.u32(edit + 0x1c, version), cpu.u32(edit + 0x3c, ref)
        cpu.u32(ref, fobj), cpu.u32(fobj + 4, name), cpu.u32(fobj + 0xc, tree if has_tree else 0)
        old = 0
        if had_net:
            old = cpu.alloc(0x20)
            cpu.u32(old, net_vtable)
            cpu.u32(edit + 0x34, old)
        vehicles = [cpu.alloc(8) for _ in range(n)]
        for v in vehicles:
            cpu.u32(v, vtable)
        table = cpu.alloc(4 * max(1, n))
        for i, v in enumerate(vehicles):
            cpu.u32(table + 4 * i, v)
        cpu.u32(ENTITIES_BEGIN, table), cpu.u32(ENTITIES_END, table + 4 * n)
        state.update(vehicles=vehicles, uses=use, fails=fails, read=0)
        log.clear()
        cpu.call(BUILD_105, ecx=edit)
        r = cpu.uc.reg_read(UC_X86_REG_EAX)
        net = struct.unpack('<I', cpu.read(edit + 0x34, 4))[0]
        want = log + [5, int(r != 0 and r == state['read']), int(net == r)]
        out = (ctypes.c_int32 * 64)()
        k = probe.probe_build_105(version, has_tree, had_net, x18, n, (ctypes.c_int32 * max(1, n))(*use), fails, out,
                                  64)
        got = list(out[:k])
        if got != want:
            raise SystemExit(f'case {cases} (version {version:#x}, tree {has_tree}, net {had_net}, x18 {x18}, uses '
                             f'{use}, fails {fails}): {got} != {want}')
        cases += 1
    print(f'ok: {cases} reloads as the original (the kept vehicles, the old net let go, the reader asked, the '
          f'hand-overs, the result)')


if __name__ == '__main__':
    main()

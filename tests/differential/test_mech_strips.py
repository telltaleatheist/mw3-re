"""The hardware mode's triangle stripper 0x552540 (docs/mechlib.md, 0x4fc8b0) against the exe on every
mechlib model: each read by the original's reader in the emulator (its materials world-like records, one per
distinct library material, flag byte +1 from the file), the original's stripper run on it (edx 0x7fffffff, 4,
as 0x4fc8b0 calls it), and the port's zmod::strip_model on the same model as the reader left it
(probe_strips.cpp strip_port). The whole polygon array after is compared: the count, each polygon's flags,
priority, material, zone and its vertex, normal, UV and colour lists, byte for byte.

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_mech_strips.py [--verbose]

BREAK=1 runs the port with a strip limit of 63 vertices (one fewer than the exe's 64), which must change
some model.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT  # noqa: E402
from test_mechlib import MECHLIB, READ_MODEL, HANDLE, Emulated, entries, walk_tree  # noqa: E402

STRIP, NEW, DELETE, LOCK, UNLOCK = 0x552540, 0x5abd26, 0x5abd1a, 0x5bb4f8, 0x5bb4f4

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libmw3probe.*'))))
probe.strip_load.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p,
                             ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
probe.strip_port.argtypes = [ctypes.c_int, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_int,
                             ctypes.POINTER(ctypes.c_int)]
CAP = 1 << 24
OUT = (ctypes.c_uint8 * CAP)()


def dump(emu, m, table, nmat):
    r, u = emu.cpu.read, emu.u32
    polys = u(m + 0x10)
    t = u(m + 0x34)
    out = bytearray()
    for i in range(polys):
        p = t + 0x24 * i
        flags, prio, vi, ni, uv, col = struct.unpack('<IiIIII', r(p, 24))
        mat, zone = struct.unpack('<II', r(p + 0x1c, 8))
        mi = (mat - table) // 0x2c if table <= mat < table + 0x2c * nmat else -1
        k = flags & 0xff
        out += struct.pack('<IiiI', flags, prio, mi, zone)
        for ptr, size, cond in ((vi, 4, True), (ni, 4, bool(flags & 0x200)), (uv, 8, True), (col, 12, True)):
            on = bool(ptr) and cond and k > 0
            out += bytes([on])
            if on:
                out += r(ptr, size * k)
    return polys, bytes(out)


def main():
    verbose = '--verbose' in sys.argv
    limit = 63 if os.environ.get('BREAK') == '1' else 0x7fffffff
    data = MECHLIB.read_bytes()
    emu = Emulated(data)
    cpu = emu.cpu
    cpu.import_code(IAT_FTOL, FTOL)
    cpu.stub_function(NEW, lambda c: emu.zeroed(c.arg(0)))
    cpu.stub_function(DELETE, lambda c: 0)
    cpu.stub_import(LOCK, lambda c: 0)
    cpu.stub_import(UNLOCK, lambda c: 0)
    mats = []
    for name, start, length in entries(data):
        if name == 'materials':
            count, = struct.unpack_from('<I', data, start)
            pos = start + 4
            for _ in range(count):
                want = emu.material(pos)
                mats.append(want[4:4 + 0x28])
                pos += struct.unpack_from('<i', want)[0]
    distinct, of = [], []
    for raw in mats:
        if raw not in distinct:
            distinct.append(raw)
        of.append(distinct.index(raw))
    table = cpu.alloc(0x2c * len(distinct))
    for i, raw in enumerate(distinct):
        cpu.write(table + 0x2c * i, raw + bytes(4))
    flags = bytes(raw[1] for raw in distinct)

    models = changed = bad = polys0 = polys1 = 0
    for name, start, length in entries(data):
        if not name.endswith('.flt'):
            continue
        offsets = []
        walk_tree(data, start, offsets)
        for off in offsets:
            emu.next, emu.pos = 0x40000000, off
            node = emu.alloc(0xd0)
            cpu.write(node, bytes(0xd0))
            cpu.u32(node + 0x3c, 1)
            cpu.call(READ_MODEL, stack_args=(HANDLE, node))
            m = emu.u32(node + 0x3c)
            t = emu.u32(m + 0x34)
            for i in range(emu.u32(m + 0x10)):
                p = t + 0x24 * i + 0x1c
                cpu.u32(p, table + 0x2c * of[emu.u32(p)])
            header = cpu.read(m, 0x5c)
            nv = emu.u32(m + 0x14)
            verts = struct.unpack(f'<{3 * nv}f', cpu.read(emu.u32(m + 0x38), 12 * nv)) if nv else ()
            n0, before = dump(emu, m, table, len(distinct))
            cpu.call(STRIP, ecx=m, edx=0x7fffffff, stack_args=(4,), count=2_000_000_000)
            n1, after = dump(emu, m, table, len(distinct))
            vbuf = (ctypes.c_float * max(len(verts), 1))(*verts)
            used = probe.strip_load(0, header, vbuf, nv, before, n0, flags, len(flags))
            assert used == len(before), (name, off, used, len(before))
            np_ = ctypes.c_int()
            w = probe.strip_port(0, limit, 4, OUT, CAP, ctypes.byref(np_))
            mine = bytes(OUT[:w])
            models += 1
            changed += n1 != n0
            polys0 += n0
            polys1 += n1
            if np_.value != n1 or mine != after:
                bad += 1
                if verbose or bad <= 5:
                    print(f'FAIL {name} +{off - start:#x}: polygons {n0} -> exe {n1}, port {np_.value}')
    print(f'{models} models, {changed} stripped by the exe ({polys0} polygons -> {polys1})')
    if bad:
        print(f'{bad} differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: every model\'s polygon array identical to the exe\'s')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()

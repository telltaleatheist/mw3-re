"""PM's LIGHT_STATE anim event (pm/gamez/zeffect/zeff_anim_events, 0x51b910) against PM's exe: every shipped
event-4 record in PM's anim.zbd files (0x98 bytes each), run on random lights by PM's offsets, in the world or
not, with the place from ref 1, from the node the anim was started at (with its offset) or from nothing; then
fuzzed flags and fields. Compared: the light's 0x100 bytes, its node's flags, whether it is in the world after and
the world's change, the event's bytes, and whether the bake was marked stale (0x4ff750).

    cmake --build build --target pmprobe
    .venv/bin/python -P tests/differential/test_pm_light_state.py [runs per record]
"""
import collections
import ctypes
import glob
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_gamez as g  # noqa: E402
import test_pm_zar  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
EVENT, STALE, POINT_COLOUR = 0x51b910, 0x4ff750, 0x5898c0
WORLD_ADD, WORLD_REMOVE, WORLD_POINT, WORLD = 0x506160, 0x506200, 0x4ff3c0, 0x8d3f20
F = ctypes.POINTER(ctypes.c_float)


def shipped():
    """Event 4's records: the 12-byte header (type 4, start 1..3, size 0x98) and a light's name at +0x0c."""
    out = []
    for f in sorted(glob.glob(str(ROOT / 'game' / 'pm' / 'zbd' / '*' / 'anim.zbd'))):
        d = open(f, 'rb').read()
        for m in re.finditer(rb'\x04[\x01\x02\x03]\x00\x00\x98\x00\x00\x00', d):
            name = d[m.start() + 12:m.start() + 44].split(b'\0')[0]
            if name and all(32 < c < 127 for c in name):
                out.append(bytes(d[m.start():m.start() + 0x98]))
    return out


def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 6
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_light_state.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_int),
                                       ctypes.POINTER(ctypes.c_uint32), F, ctypes.c_int, F, ctypes.c_int, F]
    emu = g.Emulated()
    emu.cpu.fpu_control(0x7f)
    seen = {'stale': 0, 'add': 0, 'remove': 0}
    places = {}
    emu.native_stub(STALE, lambda c: seen.__setitem__('stale', seen['stale'] + 1) or 0)
    emu.native_stub(POINT_COLOUR, lambda c: 0)
    emu.native_stub(WORLD_ADD, lambda c: seen.__setitem__('add', seen['add'] + 1) or 0)
    emu.native_stub(WORLD_REMOVE, lambda c: seen.__setitem__('remove', seen['remove'] + 1) or 0)

    def world_point(c):  # identity rotation: the point plus the node's place (its place when the point is 0)
        node, at = c.arg(0), c.arg(1)
        t = places[node]
        q = struct.unpack('<3f', emu.read(at, 12))
        q = t if q == (0.0, 0.0, 0.0) else tuple(f32(q[k] + t[k]) for k in range(3))
        emu.cpu._uc.mem_write(at, struct.pack('<3f', *q))
        return 0
    emu.native_stub(WORLD_POINT, world_point)
    node, data, ref_node, at_node = emu.alloc(0xd4), emu.alloc(0x100), emu.alloc(0xd4), emu.alloc(0xd4)
    emu.w32(node + 0x38, data), emu.w32(node + 0x34, 9)
    defn, lights, nodes, ev = emu.alloc(0x140), emu.alloc(0x2c * 2), emu.alloc(0x2c * 2), emu.alloc(0x98)
    emu.w32(defn + 0xec, lights), emu.w32(defn + 0xe8, nodes), emu.w32(lights + 0x2c + 0x24, node)
    emu.w32(WORLD, emu.alloc(0x10))
    rng = random.Random(0x51b910)
    records = shipped()
    work = [(r, False) for r in records for _ in range(runs)] + [(rng.choice(records), True) for _ in range(len(records) * 2)]
    kinds = collections.Counter()
    for n, (rec, fuzz) in enumerate(work):
        e = bytearray(rec)
        struct.pack_into('<i', e, 0x2c, 1)
        at = struct.unpack_from('<i', e, 0x54)[0]
        if fuzz:
            struct.pack_into('<I', e, 0x30, rng.getrandbits(14))
            for off in range(0x58, 0x98, 4):
                struct.pack_into('<f', e, off, rng.choice((0.0, 1.0, 150.0, rng.uniform(-50, 200))))
            for off in (0x34, 0x38, 0x3c, 0x40, 0x44, 0x48, 0x4c, 0x50):
                struct.pack_into('<i', e, off, rng.choice((0, 1, 1, 2, -1)))
            at = rng.choice((1, -200, 0, 3))
        if at > 0:
            at = 1
        struct.pack_into('<i', e, 0x54, at)
        fl = rng.choice((0, 0x100, 0x200, 0x300, 0x400, 0x500, 0x700, 0x120, 0x140, 0x80, 0x280)) | rng.choice((0, 1, 2, 0x10, 8))
        floats = [rng.choice((1.0, 0.0, rng.uniform(-2, 3))) for _ in range(0x100 // 4)]
        raw = bytearray(struct.pack(f'<{len(floats)}f', *floats))
        struct.pack_into('<I', raw, 0xe0, fl)
        struct.pack_into('<II', raw, 0xf8, 0, 0)
        if rng.random() < 0.5:
            struct.pack_into('<3f', raw, 0xa4, 1.0, 1.0, 1.0)
        in_world = rng.random() < 0.5
        nflags = rng.choice((0, 4)) | 0x100
        ref_t = tuple(f32(rng.uniform(-3000, 3000)) for _ in range(3))
        at_t = tuple(f32(rng.uniform(-3000, 3000)) for _ in range(3))
        off = tuple(rng.choice((0.0, f32(rng.uniform(-20, 20)))) for _ in range(3))
        ref_none, at_none = rng.random() < 0.2, rng.random() < 0.2
        # the exe
        emu.cpu._uc.mem_write(data, bytes(raw))
        emu.cpu._uc.mem_write(ev, bytes(e))
        emu.w32(node + 0x24, nflags)
        emu.w32(lights + 0x2c + 0x28, int(in_world))
        emu.w32(nodes + 0x2c + 0x28, 0 if ref_none else ref_node)
        emu.w32(defn + 0x7c, 0 if at_none else at_node)
        emu.cpu._uc.mem_write(defn + 0x80, struct.pack('<3f', *off))
        places[ref_node], places[at_node] = ref_t, at_t
        seen.update(stale=0, add=0, remove=0)
        r_exe = emu.call(EVENT, args=[defn, 0, ev])
        want = emu.read(data, 0x100)
        want_e = emu.read(ev, 0x98)
        want_flags = struct.unpack('<I', emu.read(node + 0x24, 4))[0]
        want_in = struct.unpack('<i', emu.read(lights + 0x2c + 0x28, 4))[0]
        want_delta = seen['add'] - seen['remove']
        # the port
        got = ctypes.create_string_buffer(bytes(raw), 0x100)
        got_e = ctypes.create_string_buffer(bytes(e), 0x98)
        w = ctypes.c_int(int(in_world))
        nf = ctypes.c_uint32(nflags)
        r = p.probe_pm_light_state(got, got_e, ctypes.byref(w), ctypes.byref(nf), (ctypes.c_float * 3)(*ref_t), int(ref_none),
                                   (ctypes.c_float * 3)(*at_t), int(at_none), (ctypes.c_float * 3)(*off))
        where = f'case {n} ({"fuzzed" if fuzz else "shipped"}) flags {struct.unpack_from("<I", e, 0x30)[0]:#x} light {fl:#x} at {at}'
        if (r & 0xf) != r_exe:
            raise SystemExit(f'{where}: result {r & 0xf}, the exe {r_exe}')
        if bool(r >> 8) != bool(seen['stale']):
            raise SystemExit(f'{where}: stale {r >> 8}, the exe {seen["stale"]}')
        if ((r >> 4) & 0xf) - 1 != want_delta or w.value != want_in:
            raise SystemExit(f'{where}: world change {((r >> 4) & 0xf) - 1} in {w.value}, the exe {want_delta} in {want_in}')
        if (nf.value & 4) != (want_flags & 4):
            raise SystemExit(f'{where}: node flags {nf.value:#x}, the exe {want_flags:#x}')
        if want != got.raw:
            i = next(i for i in range(0, 0x100, 4) if want[i:i + 4] != got.raw[i:i + 4])
            raise SystemExit(f'{where} +{i:#x}: {got.raw[i:i + 4].hex()}, the exe {want[i:i + 4].hex()}')
        if want_e != got_e.raw:
            raise SystemExit(f'{where}: the event changed differently')
        f = struct.unpack_from('<I', e, 0x30)[0]
        kinds['stale'] += bool(seen['stale'])
        kinds['added'] += want_delta > 0
        kinds['removed'] += want_delta < 0
        kinds['placed at a node'] += bool(f & 2) and not (at == 1 and ref_none) and not (at == -200 and at_none) and at in (1, -200)
        for bit in range(14):
            kinds[f'bit {1 << bit:#x}'] += bool(f & (1 << bit))
    pm_floors.floors(records=(len(records), 800), cases=(len(work), 6000), stale=(kinds['stale'], 500),
                     added=(kinds['added'], 300), removed=(kinds['removed'], 300), at_node=(kinds['placed at a node'], 300),
                     **{f'b{1 << b:x}': (kinds[f'bit {1 << b:#x}'], 100) for b in range(14)})
    print(f'ok: {len(records)} shipped records x {runs} and {len(work) - len(records) * runs} fuzzed, bit for bit; '
          f'stale {kinds["stale"]}, added {kinds["added"]}, removed {kinds["removed"]}, placed at a node {kinds["placed at a node"]}')


if __name__ == '__main__':
    main()

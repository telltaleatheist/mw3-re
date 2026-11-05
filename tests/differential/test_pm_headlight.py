"""Pirate's Moon's headlight (mech3/mech_headlight: the config 0x479330 and 0x479440, the object 0x479510, 0x4795c0,
0x479780, the toggle 0x4799b0, the level 0x479a40, the lod 0x4799f0, the fade 0x479b50, detach and attach 0x479c30,
0x479c60, active 0x479950) against PM's exe, on a mechlib tree over c1 (docs/pm-map.md, Headlights).

    cmake --build build --target pmprobe
    .venv/bin/python -P tests/differential/test_pm_headlight.py [steps]

After every random step: the object's fields, the config, its nodes' flags and links, the light's data; after the
create and at the end every model and every node.
The x87 at 24 bits, as in play where it matters here (the fade's exponent is built from a float product).
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_gamez as g  # noqa: E402
import test_pm_mechlib as ml  # noqa: E402
import test_pm_zar  # noqa: E402

DEFAULTS, READ, CREATE, TURN, LEVEL, LOD, FADE, DETACH, ATTACH, ACTIVE = (
    0x479330, 0x479440, 0x479510, 0x4799b0, 0x479a40, 0x4799f0, 0x479b50, 0x479c30, 0x479c60, 0x479950)
D3D_ON, LIB, WORLD_P = 0x8c4b70, 0x62d874, 0x62ca90
FIND, VEC3, VEC2 = 0x578950, 0x5790d0, 0x5790f0
STAGE = 0x5329e0  # after the beam's load, an unrelated module's reset
FORMAT_ORD = 0x5ce8de  # CString::Format, through its thunk
SET_POINT = 0x5898c0


def fbits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Emulated(ml.Emulated):
    def __init__(self):
        super().__init__()
        for a in (FIND, VEC3, VEC2, STAGE, SET_POINT):
            self.native_stub(a, lambda c: 0)
        self.native_stub(FORMAT_ORD, self.format)
        for a, fn in ((0x5ce8e4, self.cstring_new), (0x5ce8d8, lambda c: 0)):
            self.native_stub(a, fn)

    def cstring_new(self, c):
        self.w32(c.ecx, self.alloc(64))
        return c.ecx

    def format(self, c):  # "%s%d"-free: "light%d" with an int
        s = self.cstr(c.arg(1)).decode('latin1').replace('%d', str(c.arg(2)))
        self.cpu._uc.mem_write(self.i32(c.ecx) & 0xffffffff, s.encode() + b'\0')
        return 0

    def world_slot(self):
        blocks = self.i32(g.N_BLOCKS) & 0xffffffff
        for i in range(2500):
            n = (self.i32(blocks) & 0xffffffff) + 0xd4 * i
            if self.i32(n + 0x34) == 2:
                return i, n
        raise SystemExit('no world node')

    def create(self, root):
        self.cfg = self.alloc(0x40)
        self.call(DEFAULTS, ecx=self.cfg)
        self.call(READ, ecx=self.cfg, args=[0])
        slot, world = self.world_slot()
        wrap = self.alloc(8)
        self.w32(wrap + 4, world)
        self.w32(WORLD_P, wrap)
        self.w32(D3D_ON, 1)
        self.w32(LIB, self.lib)
        self.obj = self.call(CREATE, args=[root, self.cfg]) & 0xffffffff
        return slot

    def dump(self, whole=False):
        o = self.obj
        on = 1.0 if self.read(o + 8, 1)[0] else 0.0
        out = struct.pack('<f', on) + self.read(o + 0xc, 12) + self.read(o + 0x34, 4)
        cfg = self.read(self.cfg, 0x3c)
        nodes = b''
        for n in (self.i32(o + 4), self.i32(o + 0x2c), self.i32(o + 0x24), self.loaded[0]):
            n &= 0xffffffff
            nodes += struct.pack('<iii', self.i32(n + 0x24), *struct.unpack('<hh', self.read(n + 0x54, 4)))
        light = self.i32((self.i32(o + 0x2c) & 0xffffffff) + 0x38) & 0xffffffff
        out = out + cfg + nodes + self.read(light, 0xf8)
        if not whole:
            return out, b''
        self.unresolved = self.m_unread = self.n_unread = 1 << 30
        return out, self.model_dump() + self.node_dump()


def main():
    steps = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_mechlib.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int]
    p.probe_pm_mechlib_copies.argtypes = [ctypes.c_void_p, ctypes.c_int]
    p.probe_pm_headlight_op.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_float, ctypes.c_void_p, ctypes.c_int]
    buf = ctypes.create_string_buffer(1 << 27)
    c1 = ml.ZBD / 'c1' / 'gamez.zbd'
    names = ['mech_madcat_1.flt']
    emu = Emulated()
    emu.run(c1, ml.MECHLIB, names)
    p.probe_pm_mechlib_copies((ctypes.c_int32 * 1)(0), 0)
    if p.probe_pm_mechlib(str(c1).encode(), str(ml.MECHLIB).encode(), '|'.join(names).encode(), buf, len(buf)) < 0:
        raise SystemExit('probe_pm_mechlib failed')
    emu.cpu.fpu_control(0x7f)
    slot = emu.create(emu.loaded[0])
    if not emu.obj:
        raise SystemExit('the exe made no headlight')

    def compare(step, n, whole=False):
        head, rest = emu.dump(whole)
        got = buf.raw[:n]
        if got[:len(head)] != head:
            at = next(i for i in range(len(head)) if head[i] != got[i])
            raise SystemExit(f'{step}: fields +{at}: exe {head[at & ~3:(at & ~3) + 4].hex()} port {got[at & ~3:(at & ~3) + 4].hex()}\n exe {head.hex()}\n port {got[:len(head)].hex()}')
        if got[len(head):] != rest:
            r = got[len(head):]
            at = next((i for i in range(min(len(rest), len(r))) if rest[i] != r[i]), min(len(rest), len(r)))
            marks = [(o, k) for o, k in getattr(emu, 'node_marks', []) if o <= at]
            raise SystemExit(f'{step}: dump differs at {at} of {len(rest)}/{len(r)}, nodes from {marks[-1:]}: exe {rest[at:at + 16].hex()} port {r[at:at + 16].hex()}')

    n = p.probe_pm_headlight_op(0, slot, 0, 0.0, buf, len(buf))
    if n < 0:
        raise SystemExit('probe failed')
    compare('create', n, True)
    rng = random.Random(0x479510)
    o = emu.obj
    counts = {}
    detached = False
    for s in range(steps):
        op = rng.choice((1, 1, 2, 3, 4, 4, 4, 4, 5, 6, 7))
        a, b, f = 0, 0, 0.0
        if op == 1:
            a, b = rng.randrange(2), rng.randrange(2)
            emu.call(TURN, ecx=o, args=[a, b])
        elif op == 2:
            f = rng.choice((0.0, 1.0, rng.random()))
            emu.call(LEVEL, ecx=o, args=[fbits(f)])
        elif op == 3:
            f = rng.choice((0.0, rng.uniform(0, 300) ** 2, 22500.0, 40000.0))
            emu.call(LOD, ecx=o, args=[fbits(f)])
        elif op == 4:
            f = rng.choice((0.0, 0.033, 0.033, rng.uniform(0, 0.2)))
            emu.call(FADE, ecx=o, args=[fbits(f)])
        elif op == 5:
            if detached:
                continue
            detached = True
            emu.call(DETACH, ecx=o)
        elif op == 6:
            if not detached:
                continue
            detached = False
            f = rng.choice((0.0, 0.033))
            emu.call(ATTACH, ecx=o, args=[fbits(f)])
        elif op == 7:
            if detached:
                continue
            a = rng.randrange(2)
            emu.call(ACTIVE, ecx=o, args=[a])
        n = p.probe_pm_headlight_op(op, a, b, f, buf, len(buf))
        compare(f'step {s} op {op} ({a}, {b}, {f})', n)
        counts[op] = counts.get(op, 0) + 1
    # A whole fade up and down, and the lod past its ends, with nothing between.
    if detached:
        emu.call(ATTACH, ecx=o, args=[fbits(0.0)])
        p.probe_pm_headlight_op(6, 0, 0, 0.0, buf, len(buf))
    for on in (1, 0, 1):
        emu.call(TURN, ecx=o, args=[on, 1])
        compare(f'turn {on}', p.probe_pm_headlight_op(1, on, 1, 0.0, buf, len(buf)))
        for k in range(50):
            emu.call(FADE, ecx=o, args=[fbits(0.033)])
            compare(f'turn {on} fade {k}', p.probe_pm_headlight_op(4, 0, 0, 0.033, buf, len(buf)))
    for f in (0.0, 22500.0, 30000.0, 40000.0, 90000.0, 1e6):
        emu.call(LOD, ecx=o, args=[fbits(f)])
        compare(f'lod {f}', p.probe_pm_headlight_op(3, 0, 0, f, buf, len(buf)))
        emu.call(FADE, ecx=o, args=[fbits(0.033)])
        compare(f'lod {f} fade', p.probe_pm_headlight_op(4, 0, 0, 0.033, buf, len(buf)))
    compare('the end', p.probe_pm_headlight_op(10, 0, 0, 0.0, buf, len(buf)), True)
    pm_floors.floors(steps=(sum(counts.values()), 500), ops=(len(counts), 7))
    print(f'ok: {sum(counts.values())} steps identical, by op {sorted(counts.items())}')


if __name__ == '__main__':
    main()

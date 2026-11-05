"""PM's firepoint fix-up 0x46ea40 and aim cone 0x4a6ee0 against the exe. Floats within 1e-5.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_firepoints.py [trials]
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402

probe = test_pm_zar.load_pm_probe()
probe.probe_firepoints.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
FIXUP, CONE_CTOR, MECH_VT = 0x46ea40, 0x4a6ec0, 0x5dfaac
IN, OUT = 11, 28
IDS = list(range(1, 0x12)) + [0x11, 1, 2, 3, 9, 10, 6, 7, 8, -1, 0, 0x12, 0x20]


def fb(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


class Bench:
    def __init__(self):
        e = self.e = test_pm_zar.Emulated()
        self.uc = e.cpu._uc
        self.mech, self.mvt = e.alloc(0x800), e.alloc(0x200)
        self.uc.mem_write(self.mvt, bytes(self.uc.mem_read(MECH_VT, 0x200)))
        self.w(self.mech, self.mvt)
        self.torso, self.arm0, self.arm1 = e.alloc(0x1c0), e.alloc(0xd0), e.alloc(0xd0)
        self.w(self.mech + 0x3ac, self.torso), self.w(self.mech + 0x3a4, self.arm0), self.w(self.mech + 0x3a8, self.arm1)
        self.mnode = e.alloc(0x60)
        self.w(self.mech + 4, self.mnode)
        self.pname = e.alloc(16)
        name = self.e.cpu.code(b'\xc3')
        self.w(self.mvt + 0xb0, name)
        e.native_stub(name, lambda c: self.pname)  # the caller pops its argument with the report's
        self.dfn, self.vec = e.alloc(0x40), e.alloc(0x10)
        self.w(self.dfn + 0x24, self.vec)
        self.fps = [e.alloc(0x80) for _ in range(8)]
        self.parts = [e.alloc(0x40) for _ in range(8)]
        self.nodes = [e.alloc(0x60) for _ in range(8)]
        self.names = [e.alloc(8) for _ in range(8)]
        self.arr = e.alloc(32)
        e.native_stub(0x50da50, lambda c: self.nodes[self.names.index(c.arg(1))])
        e.native_stub(0x40f230, lambda c: self.owner(e.r32(c.ecx)))
        e.native_stub(0x510270, self.place)
        e.native_stub(0x486f10, lambda c: self.pname)
        e.cpu.stubs[0x406cb0] = (self.report, 0)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def owner(self, node):
        i = self.nodes.index(node)
        return self.parts[i] if self.rec[IN * i + 1] & 1 else 0

    def place(self, c):
        i = self.nodes.index(c.arg(0))
        for k in range(3):
            self.w(c.arg(1 + k), self.rec[IN * i + 8 + k])
        return 0

    def report(self, c):
        self.log += [c.arg(0), c.arg(2)]
        return 0

    def run(self, rec, n, limits):
        self.rec, self.log = rec, []
        u = self.uc
        u.mem_write(self.torso + 0x148, struct.pack('<3f', *limits[:3]))
        for i, a in enumerate((self.arm0, self.arm1)):
            u.mem_write(a + 0xb8, struct.pack('<4f', *limits[3 + 4 * i:7 + 4 * i]))
        for i in range(n):
            r = rec[IN * i:IN * (i + 1)]
            fp = self.fps[i]
            u.mem_write(fp, bytes(0x80))
            self.e.call(CONE_CTOR, ecx=fp + 0x14)
            u.mem_write(fp, struct.pack('<5I', *r[3:8]))
            self.w(fp + 0x5c, self.names[i])
            u.mem_write(fp + 0x7c, bytes([1 if r[1] & 4 else 0]))
            self.w(fp + 0x6c, 0xffffffff)
            self.w(self.arr + 4 * i, fp)
            p = self.parts[i]
            self.w(p + 8, self.nodes[i] if r[1] & 2 else 0)
            self.w(self.nodes[i] + 0x28, r[0])
        self.w(self.vec + 4, self.arr), self.w(self.vec + 8, self.arr + 4 * n)
        self.e.call(FIXUP, ecx=self.dfn, args=(self.mech,))
        out = []
        for i in range(n):
            fp = self.fps[i]
            rd = lambda a, k: list(struct.unpack(f'<{k}I', bytes(u.mem_read(a, k * 4))))  # noqa: E731
            out += rd(fp + 0x6c, 4) + rd(fp, 5) + rd(fp + 0x14, 18) + [u.mem_read(fp + 0x7c, 1)[0]]
        return out + self.log


def near(a, b):
    if a == b:
        return True
    x, y = struct.unpack('<2f', struct.pack('<2I', a, b))
    return all(v == 0 or 1e-10 < abs(v) < 1e30 for v in (x, y)) and abs(x - y) <= 1e-5 * max(1, abs(x))


def main():
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
    b = Bench()
    rng = random.Random(46)
    reports, bands = set(), set()
    for t in range(trials):
        n = rng.randint(1, 8)
        rec = []
        for _ in range(n):
            flags = rng.choice([3, 3, 3, 3, 3, 3, 1, 0, 7])
            rec += [rng.choice(IDS) & 0xffffffff, flags, 0]
            lim = rng.choice([0.0, 0.5, 2.0])
            yaw = sorted([rng.uniform(-lim, lim), rng.uniform(-lim, lim)])
            if rng.random() < 0.3:
                yaw = [rng.uniform(0.01, 1.5), rng.uniform(1.6, 3)] if rng.random() < 0.5 else \
                    [rng.uniform(-3, -1.6), rng.uniform(-1.5, -0.01)]
            if rng.random() < 0.05:
                yaw = [rng.uniform(-1, 0), rng.uniform(6.5, 9)]  # a band wider than a half turn each way
            if rng.random() < 0.05:
                yaw.reverse()
            pitch = sorted([rng.uniform(-lim, lim), rng.uniform(-lim, lim)])
            rec += [fb(0.47), fb(pitch[0]), fb(pitch[1]), fb(yaw[0]), fb(yaw[1])]
            rec += [fb(rng.uniform(-9, 9)) for _ in range(3)]
        limits = [rng.uniform(-0.8, 0), rng.uniform(0, 0.8), rng.uniform(0, 1.5)]
        for _ in range(2):
            limits += [rng.uniform(-0.8, 0), rng.uniform(0, 0.8), rng.uniform(-1, 0), rng.uniform(0, 1)]
        want = b.run(rec, n, limits)
        out = (ctypes.c_uint32 * 2048)()
        k = probe.probe_firepoints((ctypes.c_uint32 * len(rec))(*rec), n, (ctypes.c_float * 11)(*limits), out, 2048)
        got = list(out[:k])
        where = next((i for i in range(min(len(want), len(got))) if not near(want[i], got[i])), None)
        if where is not None or len(want) != len(got):
            raise SystemExit(f'trial {t} n={n}: at {where} ({where % OUT if where is not None else ""}), lengths '
                             f'{len(want)} {len(got)}\n  exe  {[hex(x) for x in want[:OUT]]}\n  port {[hex(x) for x in got[:OUT]]}\n'
                             f'  rec {[hex(x) for x in rec[:IN]]}')
        reports.update(b.log[1::2])
        for i in range(n):
            bands.add(rec[IN * i])
    print(f'ok: {trials} definitions agree; part ids {len(bands)}, reports {sorted(hex(x) for x in reports)}')


if __name__ == '__main__':
    main()

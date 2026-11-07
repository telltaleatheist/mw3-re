"""PM's commander panel's new lines against the exe: 0x469a70 (a lancemate's line: the test, the key 1..4 bound in
the mode's keys, its name in place of the number) and 0x469bf0 over the table 0x606f30 (the orders' lines, made for
each panel, a key without a name left blank, an action without one hidden). The key calls are stubbed and logged,
the item's constructors and the line's add too. The rest of the panel is test_pocket_panel's, not moved.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_pocket_lines.py
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_X87'] = '53'  # the reference's precision: PM's AI, docs/pm-tests.md
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

MATE, ROW, TABLE, CALLBACK = 0x469a70, 0x469bf0, 0x606f30, 0x629a28
BLANK = 0x618754  # the exe's empty string


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.w = lambda a, v: self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
        self.mem = z.alloc(0x2000)
        a = iter(range(self.mem, self.mem + 0x2000, 0x100))
        self.rec, self.h, self.player, pvt = next(a), next(a), next(a), next(a)
        self.ent, evt, self.m, mvt = next(a), next(a), next(a), next(a)
        self.text, self.name, self.item = next(a), next(a), next(a)
        self.w(self.rec, 0x1111), self.w(self.rec + 8, 0x2222), self.w(self.rec + 12, self.h)
        self.w(self.rec + 20, self.player)
        self.w(self.player, pvt), self.w(pvt + 0x28, self.ret(lambda c: c.ecx))
        self.w(self.ent, evt), self.w(evt + 0x2c, self.ret(lambda c: self.m))
        self.w(self.m, mvt), self.w(mvt + 0xb0, self.ret(lambda c: self.name if self.inp[5] else 0))
        self.uc.mem_write(self.name, b'N\0')
        self.w(CALLBACK, 0x428970)
        for addr, fn, pop in ((0x5ce8c6, lambda c: self.item, 0), (0x469190, lambda c: c.ecx, 0x14),
                              (0x469510, self.order_item, 0), (0x469980, self.added, 0xc),
                              (0x596e50, self.message, 0), (0x536c00, self.bound, 0), (0x536c30, self.callback, 0),
                              (0x536a60, self.key_of, 0), (0x536c90, self.key_name, 0), (0x536c70, self.action_name, 0)):
            z.native_stub(addr, fn, pop)

    def ret(self, fn):
        t = self.z.alloc(4)
        self.uc.mem_write(t, b'\xc3')
        self.z.native_stub(t, fn)
        return t

    def message(self, c):
        self.msg = c.arg(0)
        return self.text

    def bound(self, c):
        self.log += [0x61, c.arg(0), self.msg if c.arg(1) == self.text else 0, c.arg(2)]
        assert (c.arg(3), c.arg(4), c.arg(5)) == (0, 0, 0)
        return c.arg(0)

    def callback(self, c):
        assert c.arg(1) == 0x428970
        self.log += [0x62, c.arg(0)]
        return 0

    def key_of(self, c):
        self.log += [0x63, c.arg(0)]
        return c.arg(0) + 0x100 if self.inp[6] else 0

    def key_name(self, c):
        self.log += [0x64, c.arg(0), c.arg(2)]
        if not self.inp[7]:
            return BLANK
        self.uc.mem_write(c.arg(1), b'K\0')
        return c.arg(1)

    def action_name(self, c):
        self.log += [0x66, c.arg(0), c.arg(2)]
        if not self.inp[8]:
            return 0
        self.uc.mem_write(c.arg(1), b'A\0')
        return c.arg(1)

    def order_item(self, c):
        return self.item

    def added(self, c):
        key, name = c.arg(1), c.arg(2)
        k = bytes(self.uc.mem_read(key, 1))[0] if key else 0
        self.result = (k, bytes(self.uc.mem_read(name, 1))[0] if name else 0)
        return 0

    def run(self, inp):
        z, w = self.z, self.w
        self.inp, self.log, self.result = inp, [], None
        w(self.rec + 24, inp[0])
        self.uc.mem_write(self.m + 8, bytes([inp[1]]))
        self.uc.mem_write(self.m + 0xbc, bytes([inp[2]]))
        w(self.m + 0xc4, inp[3]), w(self.player + 0xc0, inp[4])
        z.call(MATE, self.rec, [self.ent])
        r = self.result
        self.log += [0x65, 1 if r else 0, r[1] if r else 0, r[0] if r else 0]
        rows = [6]
        for i in range(6):
            self.result = None
            z.call(ROW, self.rec, [TABLE + 16 * i])
            r = self.result
            rows += [0x67, r[0], r[1], z.r32(TABLE + 16 * i + 12)]
        return self.log + rows


def main():
    p = test_pm_zar.load_pm_probe()
    P = ctypes.c_void_p
    p.probe_pm_pocket_lines.argtypes = [P, P, ctypes.c_int]
    rng = random.Random(1)
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 1500
    emu = Exe()
    out = (ctypes.c_uint32 * 96)()
    lines = bound = numbered = 0
    for k in range(n):
        inp = [rng.choice([0, 1, 2, 3, 4, 5]), int(rng.random() < 0.9), rng.choice([0x18, 0x18, 0x18, 0x10, 8, 0x19, 0]),
               rng.choice([2, 2, 2, 6, 4, 0]), rng.choice([2, 2, 6]), int(rng.random() < 0.9), int(rng.random() < 0.8),
               int(rng.random() < 0.8), int(rng.random() < 0.8)]
        a = emu.run(inp)
        m = p.probe_pm_pocket_lines((ctypes.c_int32 * 9)(*inp), out, 96)
        b = list(out[:m])
        if a != b:
            print(f'case {k}: exe {[hex(x) for x in a]}\n        port {[hex(x) for x in b]}\n  in {inp}')
            return 1
        i = a.index(0x65)
        lines += a[i + 1]
        bound += a[i + 3] != 0
        numbered += a[i + 1] and not a[i + 3]
    print(f'ok: {n} cases, {lines} lines, {bound} with a key, {numbered} with their number: identical')
    pm_floors.floors(cases=(n, 500), lines=(lines, 200), bound=(bound, 100), numbered=(numbered, 50))
    return 0


if __name__ == '__main__':
    sys.exit(main())

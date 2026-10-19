"""The ported anim.zbd reader (0x50a0e0, docs/anim.md) against the original
x86 on every mission's anim.zbd, each read against its world.

    cmake --build build && .venv/bin/python tests/differential/test_anim.py

Both first read the mission's gamez.zbd (the ported and the original
0x4ff520), then the animations, whose lookups search that world's nodes and
make new light and sound nodes. The calls into parts not ported yet (puffers,
zSound's lookup, the effect table) are recorded in both and answer fake ids
(0x70000000 + 0x10 * call number). A full run compares that trace, every
def, list, blob and sequence read (nodes as pool slots), and the whole node
pool and lists after. Mismatched runs (an anim file against another
mission's world) and runs where one recorded call answers "not found"
compare the result, the reports and the trace.
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT  # noqa: E402
from test_gamez import WORLDS, ZBD, Emulated as WorldEmu, probe  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe.probe_anim.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p,
                             ctypes.c_int]
READ, SET_PATH, ALLOW = 0x50a0e0, 0x5090f0, 0x509150
IAT_SPRINTF, IAT_STRNCPY = 0x5bb658, 0x5bb648
DEFS, BLOCK = 0x73f72c, 0x73f720
FRAME, EXPIRY, UPDATE_FRAME, FROZEN, WALK = 0x72f338, 0x73f3a8, 0x5e9e2c, 0x60b94c, 0x60be40
HOOKS = {0x4f9b50: 3, 0x54a710: 7, 0x56f6f0: 10, 0x50d9e0: 11}


class Emulated(WorldEmu):
    def __init__(self):
        super().__init__()
        cpu = self.cpu
        cpu.stub_import(IAT_SPRINTF, self.sprintf)
        cpu.stub_import(IAT_STRNCPY, self.strncpy)
        for addr, kind in HOOKS.items():
            cpu.stub_function(addr, lambda c, a=addr, k=kind: self.hook(a, k, c))

    def cstr(self, p):
        out = bytearray()
        while (b := self.cpu.read(p + len(out), 1)) != b'\0':
            out += b
        return bytes(out)

    def sprintf(self, c):
        assert self.cstr(c.arg(1)) == b'%s'
        s = self.cstr(c.arg(2))
        c.write(c.arg(0), s + b'\0')
        return len(s)

    def strncpy(self, c):
        dst, src, n = c.arg(0), self.cstr(c.arg(1)), c.arg(2)
        c.write(dst, (src + bytes(n))[:n])
        return dst

    def hook(self, addr, kind, c):
        if kind == 3:
            self.trace += struct.pack('<Ii', 3, self.index(c.ecx))
        elif kind == 7:
            self.trace += struct.pack('<I', 7)
        else:
            self.trace += struct.pack('<I', kind) + self.cstr(c.ecx) + b'\0'
        k = self.calls
        self.calls += 1
        if k == self.fail_at:
            return 0xffffffff if addr == 0x50d9e0 else 0
        return 0x70000000 + 0x10 * k

    def blocks(self):
        return [self.u(self.u(0x60bf38) + 4 * b) for b in range(self.i32(0x60bf30))]

    def index(self, p):
        bs = self.i32(0x5ea830)
        for b, base in enumerate(self.blocks()):
            if base <= p < base + bs * 0xd4:
                return b * bs + (p - base) // 0xd4
        return -1 if p == 0 else -2

    def u(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def run(self, world, anim, fail_at=-1, full=False):
        cpu = self.cpu
        for a, v in ((FRAME, 0), (EXPIRY, 0), (UPDATE_FRAME, 1), (FROZEN, 0), (WALK, 0), (WALK + 4, 0)):
            cpu.u32(a, v)
        wdata = world.read_bytes()
        head = self.read_zbd(wdata, str(world).encode())
        assert struct.unpack_from('<i', head)[0] == 0
        self.reports = []
        self.data, self.pos, self.open_ok = anim.read_bytes(), 0, True
        self.trace, self.calls, self.fail_at = b'', 0, fail_at
        cpu.write(BLOCK, bytes(0x44))
        cpu.write(0x60c570, bytes(0x80))
        cpu.call(ALLOW, ecx=1)
        p = self.alloc(len(str(anim)) + 1)
        cpu.write(p, str(anim).encode() + b'\0')
        cpu.call(SET_PATH, ecx=p)
        cpu.call(READ, count=20_000_000_000)
        res = struct.unpack('<i', struct.pack('<I', cpu.uc.reg_read(UC_X86_REG_EAX)))[0]
        out = [struct.pack('<iii', res, self.calls, len(self.reports))]
        out += [struct.pack('<ii', *x) for x in self.reports]
        out.append(self.trace)
        if full and res == 0:
            out.append(self.dump())
            out.append(self.dump_world(self.i32(0x60bf34)))
        return b''.join(out)

    def dump(self):
        r, u, ix = self.cpu.read, self.u, self.index
        out = []

        def raw(addr, n, ptrs=()):
            b = bytearray(r(addr, n))
            for o in ptrs:
                b[o:o + 4] = bytes(4)
            out.append(bytes(b))

        raw(BLOCK, 0x44, (0x0c, 0x14, 0x18))
        count = struct.unpack('<h', r(BLOCK + 0x0a, 2))[0]
        defs = u(DEFS)
        for i in range(count):
            d = defs + 0x13c * i
            rec = r(d, 0x13c)
            fixed = i != 0 and rec[0x98] != 5
            raw(d, 0x13c, (0x40, 0x64, 0xc4, 0x100) + tuple(range(0x114, 0x138, 4)))
            out.append(struct.pack('<ii', ix(u(d + 0x40)) if fixed else 0, ix(u(d + 0x64)) if fixed else 0))
            for cnt, ptr, size, nodes in ((0x109, 0x114, 0x60, 1), (0x10a, 0x118, 0x28, 1), (0x10b, 0x11c, 0x2c, 1),
                                          (0x10c, 0x120, 0x2c, 0), (0x10d, 0x124, 0x2c, 1)):
                lst = u(d + ptr)
                for k in range(rec[cnt]):
                    raw(lst + size * k, size, (0x24,))
                    v = u(lst + size * k + 0x24)
                    out.append(struct.pack('<I', (ix(v) & 0xffffffff if nodes else v) if fixed and k else 0))
            lst = u(d + 0x128)
            for k in range(rec[0x10e]):
                raw(lst + 0x24 * k, 0x24, (0x20,))
                out.append(struct.pack('<I', u(lst + 0x24 * k + 0x20) if fixed and k else 0))
            lst = u(d + 0x12c)
            for k in range(rec[0x10f]):
                raw(lst + 0x24 * k, 0x24)
            lst = u(d + 0x130)
            for k in range(rec[0x110]):
                e = lst + 0x30 * k
                raw(e, 0x30, (0x28, 0x2c))
                if not fixed:
                    continue
                t = r(e + 4, 1)[0]
                if t == 1:
                    p = u(e + 0x28)
                    out.append(struct.pack('<i', -1 if p == 0 else (p - defs) // 0x13c))
                if t in (2, 3):
                    out.append(struct.pack('<i', ix(u(e + 0x2c))))
            lst = u(d + 0x134)
            for k in range(rec[0x112]):
                raw(lst + 0x48 * k, 0x48)
            size = struct.unpack_from('<i', rec, 0x104)[0]
            if size > 0:
                out.append(r(u(d + 0x100), size))
            seqs = u(d + 0xc4)
            for k in range(rec[0x108]):
                s = seqs + 0x40 * k
                raw(s, 0x40, (0x38,))
                size = self.i32(s + 0x3c)
                if size > 0:
                    out.append(r(u(s + 0x38), size))
        return b''.join(out)


def compare(what, want, n, buf):
    got = buf.raw[:max(n, 0)]
    if got != want:
        at = next((i for i in range(min(n, len(want))) if got[i] != want[i]), None)
        raise SystemExit(f'{what}: original {len(want)} bytes, port {n}, first difference at {at}')


def main():
    emu = Emulated()
    buf = ctypes.create_string_buffer(64 << 20)
    calls = live = 0
    for w in WORLDS:
        world, anim = ZBD / w / 'gamez.zbd', ZBD / w / 'anim.zbd'
        want = emu.run(world, anim, full=True)
        compare(f'{w} full', want, probe.probe_anim(str(world).encode(), str(anim).encode(), -1, 1, buf, len(buf)), buf)
        calls += struct.unpack_from('<i', want, 4)[0]
        live += emu.i32(0x60bf3c)
    runs = 0
    for w, other in (('t1', 'c1'), ('c1', 't1'), ('c4b', 'c4')):
        world, anim = ZBD / other / 'gamez.zbd', ZBD / w / 'anim.zbd'
        want = emu.run(world, anim)
        compare(f'{w} anim on {other} world', want,
                probe.probe_anim(str(world).encode(), str(anim).encode(), -1, 0, buf, len(buf)), buf)
        runs += 1
    world, anim = ZBD / 't1' / 'gamez.zbd', ZBD / 't1' / 'anim.zbd'
    total = struct.unpack_from('<i', emu.run(world, anim), 4)[0]
    for k in sorted({0, 1, 2, total // 3, total // 2, total - 2, total - 1}):
        want = emu.run(world, anim, fail_at=k)
        compare(f't1 fail at {k}', want, probe.probe_anim(str(world).encode(), str(anim).encode(), k, 0, buf, len(buf)),
                buf)
        runs += 1
    print(f'ok: {len(WORLDS)} anim.zbd read against their worlds identical ({calls} unported calls, '
          f'{live} live nodes after); {runs} mismatched and failing runs agree')


if __name__ == '__main__':
    main()

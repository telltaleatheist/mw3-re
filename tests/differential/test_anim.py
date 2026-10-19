"""The ported anim.zbd reader (0x50a0e0, docs/anim.md) against the original
x86 on every mission's anim.zbd.

    cmake --build build && .venv/bin/python tests/differential/test_anim.py

The scene calls the loader makes (node lookups, creating lights, puffers and
sound nodes) are not ported yet: in both, each call is recorded with its
arguments and answers a fake id (0x70000000 + 0x10 * call number). A full
run compares the call trace and every def, list, blob and sequence read.
Failure runs make call N answer 0 (-1 for an effect index), and truncated
files cut the read short; those compare the result, the reports and the trace.
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_anim.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
ZBD = ROOT / 'game' / 'mw3-1.2' / 'zbd'
WORLDS = ['c1', 'c2', 'c3', 'c4', 'c4b', 't1']
READ, SET_PATH, ALLOW = 0x50a0e0, 0x5090f0, 0x509150
IAT = {'fopen': 0x5bb628, 'fread': 0x5bb594, 'fclose': 0x5bb630, 'malloc': 0x5bb664, 'free': 0x5bb65c}
ZERR = 0x49bfd0
DEFS, BLOCK = 0x73f72c, 0x73f720
HANDLE = 0x55
BIG = 0x40000000


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.uc.mem_map(BIG, 0x8000000)
        cpu.stub_import(IAT['fopen'], lambda c: HANDLE if self.exists else 0)
        cpu.stub_import(IAT['fread'], self.fread)
        cpu.stub_import(IAT['fclose'], lambda c: 0)
        cpu.stub_import(IAT['malloc'], lambda c: self.alloc(c.arg(0)))
        cpu.stub_import(IAT['free'], lambda c: 0)
        cpu.stub_function(ZERR, lambda c: self.reports.append((c.arg(0), c.arg(2))))
        s = self.string
        hooks = {
            0x4f8bd0: lambda c: (1, s(c.ecx), u32(c.edx)),
            0x4ed8b0: lambda c: (2, u32(c.ecx)),
            0x4f9b50: lambda c: (3, u32(c.ecx)),
            0x509640: lambda c: (4, u32(self.def_index(c.ecx)), s(c.edx)),
            0x4fa720: lambda c: (5,),
            0x4eb300: lambda c: (6, u32(c.ecx), s(c.edx)),
            0x54a710: lambda c: (7,),
            0x4fd280: lambda c: (8,),
            0x4fd480: lambda c: (9, u32(c.ecx), s(c.edx)),
            0x56f6f0: lambda c: (10, s(c.ecx)),
            0x50d9e0: lambda c: (11, s(c.ecx)),
            0x4f9d60: lambda c: (12, u32(c.ecx), s(c.edx)),
            0x575f90: lambda c: (13,),
        }
        for addr, args in hooks.items():
            cpu.stub_function(addr, lambda c, a=addr, f=args: self.hook(a, f(c)))

    def hook(self, addr, parts):
        self.trace += struct.pack('<I', parts[0]) + b''.join(parts[1:])
        k = self.calls
        self.calls += 1
        if k == self.fail_at:
            return 0xffffffff if addr == 0x50d9e0 else 0
        return 0x70000000 + 0x10 * k

    def string(self, ptr):
        if ptr == 0:
            return u32(0xffffffff)
        out = bytearray()
        while True:
            b = self.cpu.read(ptr + len(out), 1)
            out += b
            if b == b'\0':
                return bytes(out)

    def def_index(self, ptr):
        return 0xffffffff if ptr == 0 else (ptr - self.i32(DEFS)) // 0x13c

    def alloc(self, n):
        a = self.next
        self.next += (max(n, 1) + 0xf) & ~0xf
        return a

    def fread(self, c):
        buf, size, count = c.arg(0), c.arg(1), c.arg(2)
        assert c.arg(3) == HANDLE
        chunk = self.data[self.pos:self.pos + size * count]
        chunk = chunk[:len(chunk) - len(chunk) % size] if size else b''
        self.pos += len(chunk)
        c.write(buf, chunk)
        return len(chunk) // size if size else 0

    def i32(self, a):
        return struct.unpack('<i', self.cpu.read(a, 4))[0]

    def u(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def run(self, data, path, fail_at=-1, full=False, exists=True):
        cpu = self.cpu
        self.data, self.pos, self.next, self.exists = data, 0, BIG, exists
        self.trace, self.calls, self.fail_at, self.reports = b'', 0, fail_at, []
        cpu.write(BLOCK, bytes(0x44))
        cpu.write(0x60c570, bytes(0x80))
        cpu.call(ALLOW, ecx=1)
        p = self.alloc(len(path) + 1)
        cpu.write(p, path + b'\0')
        cpu.call(SET_PATH, ecx=p)
        cpu.call(READ)
        res = struct.unpack('<i', struct.pack('<I', cpu.uc.reg_read(UC_X86_REG_EAX)))[0]
        out = [struct.pack('<iii', res, self.calls, len(self.reports))]
        out += [struct.pack('<ii', *x) for x in self.reports]
        out.append(self.trace)
        if full and res == 0:
            out.append(self.dump())
        return b''.join(out)

    def dump(self):
        r, u = self.cpu.read, self.u
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
            out.append(struct.pack('<II', u(d + 0x40) if fixed else 0, u(d + 0x64) if fixed else 0))
            for cnt, ptr, size in ((0x109, 0x114, 0x60), (0x10a, 0x118, 0x28), (0x10b, 0x11c, 0x2c),
                                   (0x10c, 0x120, 0x2c), (0x10d, 0x124, 0x2c)):
                lst = u(d + ptr)
                for k in range(rec[cnt]):
                    raw(lst + size * k, size, (0x24,))
                    out.append(struct.pack('<I', u(lst + size * k + 0x24) if fixed and k else 0))
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
                    out.append(struct.pack('<I', u(e + 0x2c)))
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


def u32(v):
    return struct.pack('<I', v & 0xffffffff)


def compare(what, want, n, buf):
    got = buf.raw[:max(n, 0)]
    if got != want:
        at = next((i for i in range(min(n, len(want))) if got[i] != want[i]), None)
        raise SystemExit(f'{what}: original {len(want)} bytes, port {n}, first difference at {at}')


def main():
    emu = Emulated()
    buf = ctypes.create_string_buffer(64 << 20)
    tmp = ROOT / 'build' / 'test-anim'
    tmp.mkdir(exist_ok=True)
    calls = 0
    for w in WORLDS:
        path = ZBD / w / 'anim.zbd'
        data = path.read_bytes()
        want = emu.run(data, str(path).encode(), full=True)
        compare(f'{w} full', want, probe.probe_anim(str(path).encode(), -1, 1, buf, len(buf)), buf)
        calls += struct.unpack_from('<i', want, 4)[0]
    # Failure runs on t1: every kind of scene call answering "not found" once.
    path = ZBD / 't1' / 'anim.zbd'
    data = path.read_bytes()
    total = struct.unpack_from('<i', emu.run(data, str(path).encode()), 4)[0]
    fails = sorted({*range(0, 40), *range(total - 40, total), *range(0, total, max(total // 200, 1))})
    for k in fails:
        want = emu.run(data, str(path).encode(), fail_at=k)
        compare(f't1 fail at call {k}', want, probe.probe_anim(str(path).encode(), k, 0, buf, len(buf)), buf)
    cuts = [0, 0x0b, 0x0c + 0x54 * 3, len(data) // 3, len(data) - 1]
    for cut in cuts:
        f = tmp / f'cut{cut}'
        f.write_bytes(data[:cut])
        compare(f't1 cut at {cut}', emu.run(data[:cut], str(f).encode()),
                probe.probe_anim(str(f).encode(), -1, 0, buf, len(buf)), buf)
    bad = tmp / 'version'
    bad.write_bytes(data[:4] + b'\x26' + data[5:])
    compare('bad version', emu.run(bad.read_bytes(), str(bad).encode()),
            probe.probe_anim(str(bad).encode(), -1, 0, buf, len(buf)), buf)
    print(f'ok: {len(WORLDS)} anim.zbd identical with {calls} scene calls; {len(fails)} failure runs, '
          f'{len(cuts)} truncated files and a bad version agree')


if __name__ == '__main__':
    main()

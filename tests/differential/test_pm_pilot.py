"""PM's pilot file: PM's real constructor 0x479d80, header 0x47af50, read 0x47b2f0 and
write 0x47b610 over a fake file system, against the port's on a real folder.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_pilot.py

Each file goes in, is read and written back on both sides, and the bytes must match:
a new pilot; a wrong version (PM wipes the file); crafted files with campaigns and
the second campaign's ("standalone") record, whose bytes must also come back
unchanged, since saving must not lose it while the mode is not ported; and a real
pilot of Owen's where there is one, played on PM (its standalone record is empty, so
that record rests on the crafted files and what the exe's read and write do). Reading
merges a campaign's salvage (0x435990), so a read and write is not byte-equal in
general: the exe is the reference. The port's copy before a wipe (FIX (bug)) is beside the file, outside
this comparison, and has its own cases, port only. Damaged files run on the port only: no crash, no
hang; the exe has no stops at a file's end (FIX (bug), fixes.md). The whole `test_pilot`
(the screens' calls on stores and games) stays to move.
"""
import ctypes
import os
import random
import shutil
import struct
import sys
import tempfile
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import ai_world  # noqa: E402
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402
from original import FTOL, IAT_FTOL, ROOT  # noqa: E402
from twod_world import IAT  # noqa: E402

probe = test_pm_zar.load_pm_probe()
probe.probe_pm_pilot_round.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
probe.probe_pm_pilot_new.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
key = test_pm_zar.key
OWEN = Path('/Volumes/Callisto/claude-files/keep/mw3-re/pm-save')
NAME = 'Pilot'
HEADER = struct.pack('<3i', 1, 0x3e, 0)
STANDALONE = 263  # the header and four games' records in a pilot with no configurations


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        cpu = z.cpu
        u = cpu._uc
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.import_code(ai_world.IAT_FLOOR, ai_world.FLOOR)
        cpu.import_code(0x5bb61c, ai_world.FLOOR.replace(bytes.fromhex('660d0004'), bytes.fromhex('660d0008')))
        cpu.stub_import(IAT['malloc'], lambda c: z.alloc(c.arg(0)))
        self.w32 = lambda a, v: u.mem_write(a, struct.pack('<I', v & 0xffffffff))
        empty = z.string('')
        z.native_stub(0x5ce8c6, lambda c: z.alloc(c.arg(0)))   # new
        z.native_stub(0x5ce8c0, lambda c: 0)                   # delete
        z.native_stub(0x5ce8e4, lambda c: (self.w32(c.ecx, empty), c.ecx)[1])  # CString
        z.native_stub(0x5ce8d8, lambda c: 0)
        z.native_stub(0x5ce8de, self.fmt)
        z.native_stub(0x596e50, lambda c: z.string('MSG_%d' % c.arg(0)))
        z.native_stub(0x4732b0, lambda c: 0)                   # the store's fill
        z.native_stub(0x4752c0, lambda c: 1, 8)                # a new campaign's mechs
        z.native_stub(0x596ea0, lambda c: key(z.cstr(c.arg(0)).decode('latin1')) in z.files)
        z.native_stub(0x5ceb0c, lambda c: (u.mem_write(c.ecx, bytes(0x1c)), c.ecx)[1], 4)  # CStringList
        z.native_stub(0x5ceb18, self.add_tail, 4)
        for a in (0x5ceb06, 0x5ceb12, 0x5cee12):
            z.native_stub(a, lambda c: 0)

    def fmt(self, c):
        z = self.z
        f, out, n, i = z.cstr(c.arg(1)).decode('latin1'), '', 2, 0
        while i < len(f):
            if f[i] != '%':
                out += f[i]
                i += 1
                continue
            j = i + 1
            while f[j] in '0123456789':
                j += 1
            if f[j] == 's':
                out += z.cstr(c.arg(n)).decode('latin1')
            else:
                out += f[i:j + 1] % struct.unpack('<i', struct.pack('<I', c.arg(n)))[0]
            n, i = n + 1, j + 1
        self.w32(c.arg(0), z.string(out))
        return c.arg(0)

    def add_tail(self, c):
        z = self.z
        node, tail = z.alloc(12), z.r32(c.ecx + 8)
        self.w32(node, 0), self.w32(node + 4, tail), self.w32(node + 8, z.string(z.cstr(c.arg(0)).decode('latin1')))
        if tail:
            self.w32(tail, node)
        else:
            self.w32(c.ecx + 4, node)
        self.w32(c.ecx + 8, node), self.w32(c.ecx + 0xc, z.r32(c.ecx + 0xc) + 1)
        return node

    def path(self, name):
        return key(f'pilots\\{name}.mw3')

    def new(self, name):
        z = self.z
        z.files.pop(self.path(name), None)
        p = z.alloc(0x90)
        z.call(0x479d80, ecx=p, args=(z.string(name),))
        return p

    def round(self, name, data, games=()):
        z = self.z
        z.files[self.path(name)] = bytearray(data)
        for g in games:
            z.files[key(f'pilots//games//{g}.mw3')] = bytearray(b'x')
        p = z.alloc(0x90)
        z.files[self.path(name)] = bytearray(data)
        z.call(0x479d80, ecx=p, args=(z.string(name),))
        if not z.call(0x47af50, ecx=p) & 0xff:
            return -1, b''
        z.call(0x47b2f0, ecx=p)
        kept = (z.r32(p + 0x64) - z.r32(p + 0x60)) // 4
        z.call(0x47a1f0, ecx=p)
        z.call(0x47b610, ecx=p)
        for g in games:
            z.files.pop(key(f'pilots//games//{g}.mw3'), None)
        return kept, bytes(z.files[self.path(name)])

    def fresh(self, name):
        z = self.z
        p = self.new(name)
        z.call(0x47af50, ecx=p)
        z.call(0x47a1f0, ecx=p)
        z.call(0x47b610, ecx=p)
        return bytes(z.files[self.path(name)])


class Port:
    def __init__(self):
        self.dir = Path(tempfile.mkdtemp(prefix='pm_pilot_', dir=ROOT / 'build')).resolve()
        (self.dir / 'pilots' / 'games').mkdir(parents=True)

    def file(self, name):
        return self.dir / 'pilots' / f'{name}.mw3'

    def round(self, name, data, games=()):
        self.file(name).write_bytes(data)
        for g in games:
            (self.dir / 'pilots' / 'games' / f'{g}.mw3').write_bytes(b'x')
        kept = probe.probe_pm_pilot_round(str(self.dir).encode(), name.encode())
        for g in games:
            (self.dir / 'pilots' / 'games' / f'{g}.mw3').unlink(missing_ok=True)
        return kept, self.file(name).read_bytes()

    def fresh(self, name):
        self.file(name).unlink(missing_ok=True)
        probe.probe_pm_pilot_new(str(self.dir).encode(), name.encode())
        return self.file(name).read_bytes()


def i32s(*v):
    return b''.join(struct.pack('<i', x) for x in v)


def slot(rng):
    return i32s(rng.choice([-1, 3, 7, 23]), rng.randrange(3), rng.randrange(4), 0)


def store(rng):
    items = []
    for _ in range(rng.randrange(5)):
        kind = rng.randrange(4)
        ident = rng.randrange(1, 20)
        b = i32s(kind, rng.randint(1, 9), ident, rng.randrange(3), rng.randrange(-1, 3))
        # Equipment 1 (standard armor) reads as tech 3 whatever the file says, a real file has that.
        items.append(b + (i32s(3 if kind == 3 and ident == 1 else rng.randrange(1, 3)) if kind in (1, 3) else b''))
    return i32s(len(items)) + b''.join(items)


def campaign(rng, mission):
    b = slot(rng) + store(rng) + i32s(mission) + bytes([rng.randrange(2)])
    b += i32s(rng.randrange(4), rng.randrange(4), rng.randrange(50000))
    b += i32s(*[rng.randrange(40) for _ in range(4)])
    b += store(rng) + store(rng) + store(rng) + store(rng)
    mates = rng.randrange(3)
    b += i32s(mates) + b''.join(slot(rng) for _ in range(mates))
    texts = [bytes(rng.choices(b'abcxyz', k=rng.randrange(0, 9))) for _ in range(rng.randrange(3))]
    return b + i32s(len(texts)) + b''.join(i32s(len(t)) + t for t in texts)


def standalone(rng):
    b = slot(rng) + store(rng) + i32s(rng.randrange(11), rng.randrange(11))
    missions = rng.randrange(5)
    b += i32s(missions)
    for _ in range(missions):
        k = rng.randrange(3)
        b += i32s(k) + b''.join(slot(rng) for _ in range(k))
    return b


def keeps(rng):
    """A file the header check refuses leaves a byte-identical copy; none of it reaches the exe's comparison."""
    d = Path(tempfile.mkdtemp(prefix='pm_keep_', dir=ROOT / 'build')).resolve()
    pil = d / 'pilots'
    (pil / 'games').mkdir(parents=True)
    out = ctypes.create_string_buffer(4096)
    listing = lambda: (probe.probe_pm_pilot_list(str(d).encode(), out, 4096), out.value.decode().split())  # noqa: E731
    count = 0
    try:
        for what, data in (('another version', struct.pack('<3i', 1, 0x2f, 0) + rng.randbytes(1000)),
                           ('a damaged header', struct.pack('<3i', 77, 0x3e, 0) + rng.randbytes(1000)),
                           ('a short file', b'\x01\0\0')):
            f = pil / 'Old.mw3'
            f.write_bytes(data)
            n, names = listing()
            if names != ['Old'] or n != 1:
                raise SystemExit(f'{what}: listed {names}')
            if (pil / 'Old.old-1').read_bytes() != data:
                raise SystemExit(f'{what}: the copy is not the file')
            if f.read_bytes()[:12] != HEADER:
                raise SystemExit(f'{what}: not started empty')
            second = data[:12] + rng.randbytes(40) if len(data) > 12 else data + b'x'
            f.write_bytes(second)
            listing()
            if (pil / 'Old.old-1').read_bytes() != data or (pil / 'Old.old-2').read_bytes() != second:
                raise SystemExit(f'{what}: a second wipe touched the first copy')
            n, names = listing()  # now a good file: no third copy, and the copies are no pilots
            if names != ['Old'] or (pil / 'Old.old-3').exists():
                raise SystemExit(f'{what}: copies listed or made again: {names}')
            (pil / 'Old.old-1').unlink(), (pil / 'Old.old-2').unlink()
            count += 1
        # A good file makes no copy at all.
        f = pil / 'Old.mw3'
        good = f.read_bytes()
        listing()
        if f.read_bytes() != good or list(pil.glob('Old.old-*')):
            raise SystemExit('a good file was copied or changed')
        # No copy, no wipe: the folder refuses the new file.
        data = struct.pack('<3i', 1, 0x2f, 0) + rng.randbytes(500)
        f.write_bytes(data)
        os.chmod(pil, 0o555)
        try:
            n, names = listing()
            kept = f.read_bytes()
        finally:
            os.chmod(pil, 0o755)
        if kept != data or list(pil.glob('Old.old-*')):
            raise SystemExit('a failed copy: the file was not left alone')
        if names:
            raise SystemExit(f'a failed copy: listed {names}')
        count += 1
    finally:
        shutil.rmtree(d, ignore_errors=True)
    return count


def check(what, a, b):
    if a == b:
        return
    k = next((i for i in range(min(len(a[1]), len(b[1]))) if a[1][i] != b[1][i]), min(len(a[1]), len(b[1])))
    raise SystemExit(f'{what}: kept {a[0]}, {b[0]}; bytes differ at {k} ({len(a[1])}, {len(b[1])}):\n'
                     f'  exe  {a[1][k:k + 24].hex()}\n  port {b[1][k:k + 24].hex()}')


def main():
    rng = random.Random(0x47b2f0)
    exe, port = Exe(), Port()
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    kept = dropped = records = owen = damaged = wiped = 0
    try:
        new = exe.fresh(NAME)
        check('a new pilot', (0, new), (0, port.fresh(NAME)))
        prefix = new[:STANDALONE]

        for n in range(runs):
            rec = standalone(rng)
            missions = [rng.randint(1, 10) for _ in range(rng.randint(1, 4))]
            data = prefix + rec + i32s(len(missions)) + b''.join(campaign(rng, m) for m in missions)
            games = [f'{NAME}{m:02d}' for m in missions if rng.random() < 0.5]
            a, b = exe.round(NAME, data, games), port.round(NAME, data, games)
            check(f'file {n}, missions {missions}, saved {games}', a, b)
            if a[1][STANDALONE:STANDALONE + len(rec)] != rec:
                raise SystemExit(f'file {n}: the standalone record changed in the exe')
            records += rec[-4:] != bytes(4) or len(rec) > 32
            kept += a[0]
            dropped += len(missions) - a[0]

        # MW3's version, on a file with something to lose: PM wipes it
        data = new[:4] + struct.pack('<i', 0x2f) + prefix[8:] + standalone(rng) + i32s(1) + campaign(rng, 1)
        a, b = exe.round(NAME, data), port.round(NAME, data)
        check('another version', a, b)
        wiped = int(a[1][:12] == HEADER and a[1][STANDALONE:] != data[STANDALONE:])

        if (OWEN / 'Deathstalker.mw3').exists():
            data = (OWEN / 'Deathstalker.mw3').read_bytes()
            a = exe.round('Deathstalker', data, ['Deathstalker02'])
            b = port.round('Deathstalker', data, ['Deathstalker02'])
            check("Owen's save", a, b)
            owen = a[0]
        else:
            print("no real pilot here: crafted files only")

        base = prefix + standalone(rng) + i32s(2) + campaign(rng, 1) + campaign(rng, 3)
        for n in range(runs):
            d = bytearray(base)
            if n % 2:
                del d[rng.randrange(12, len(d)):]
            else:
                for _ in range(rng.randint(1, 4)):
                    d[rng.randrange(12, len(d))] = rng.randrange(256)
            port.round(NAME, bytes(d))  # no crash, no hang: exit_on_fault and the cap say so
            damaged += 1
    finally:
        shutil.rmtree(port.dir, ignore_errors=True)
    copies = keeps(rng)
    pm_floors.floors(copies=(copies, 4), kept=(kept, 100), dropped=(dropped, 50), records=(records, 100), damaged=(damaged, 100),
                     wiped=(wiped, 1))
    print(f'ok: {runs} crafted files the same as the exe ({kept} campaigns kept, {dropped} dropped, {records} with '
          f'standalone progress, unchanged), a new pilot, another version wiped; Owen\'s save: {owen} campaigns; '
          f'{damaged} damaged files read by the port; {copies} cases of a file kept before its wipe')
    return 0


if __name__ == '__main__':
    sys.exit(main())

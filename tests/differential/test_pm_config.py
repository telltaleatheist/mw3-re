"""PM's loadout rules against mech3/mech_config, with the weapons' masses from PM's weapons.zrd on both sides: weight 0x427130
(every case, 0x427020's categories), validate 0x427360 (for real, in the reader too, so which loadouts survive is compared),
0x427310 the slots left, 0x425c60, 0x427df0, 0x427db0; on all 25 definitions' loadouts and random changes to each.

    cmake --build build --target pmprobe
    .venv/bin/python -P tests/differential/test_pm_config.py [changes per loadout]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools' / 'analysis'))
import test_pm_mechdef as md  # noqa: E402
import test_pm_zar  # noqa: E402
import zrd  # noqa: E402
from original import ROOT  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402

WEIGHT, VALIDATE, SLOTS, UNPLACED, JETS, LIMIT, FIND_WEAPON, WEAPON_NAME = (
    0x427130, 0x427360, 0x427310, 0x425c60, 0x427df0, 0x427db0, 0x5a3550, 0x486a30)
ODS_SLOT = 0x5de0c4  # OutputDebugStringA
OP_NONE, OP_TONS, OP_SINKS, OP_DOUBLE, OP_ENDO, OP_UNPLACE, OP_ARMOR, OP_COUNT, OP_ENGINE, OP_XL, OP_NEAR, OP_JETS, OP_BULK, OP_JETS_NEAR = range(14)


def labs(data):
    start, n = next((s, k) for name, s, k in md.entries(data) if name.lower() == 'weapons.zrd')
    tree, _ = zrd.node(data[start:start + n], 0)
    defs = {}

    def walk(lst):
        for i, item in enumerate(lst):
            if isinstance(item, str) and i + 1 < len(lst) and isinstance(lst[i + 1], list) and 'MASS' in lst[i + 1]:
                defs[item.lower()] = lst[i + 1]
            elif isinstance(item, list):
                walk(item)
    walk(tree)

    def get(lst, key):
        return lst[lst.index(key) + 1] if key in lst and lst.index(key) + 1 < len(lst) else None

    out = {}
    for name, d in defs.items():
        mass = get(d, 'MASS')
        clan, sphere, ammo = get(mass, 'clan') or [0, 0], get(mass, 'sphere') or [0, 0], get(mass, 'ammo')
        salvo = get(d, 'SALVO')
        true = (get(salvo, 'truecount') or get(salvo, 'count')) if isinstance(salvo, list) else None  # as the reader's default
        per_ton = (ammo[0] if ammo else 0) * (true[0] if true else 1)
        out[name] = (float(clan[0]), int(clan[1]), float(sphere[0]), int(sphere[1]), 1.0 / per_ton if per_ton else 0.0)
    return out


class Emulated(md.Emulated):
    def __init__(self, table):
        super().__init__()
        del self.cpu.stubs[0x427360]  # the real validate here
        self.weapons = {}
        for name, lab in table.items():
            obj, blk = self.alloc(0x210), self.alloc(0x40)
            self.cpu._uc.mem_write(blk + 0x1c, struct.pack('<fi', lab[0], lab[1]))
            self.cpu._uc.mem_write(blk + 0x28, struct.pack('<fi', lab[2], lab[3]))
            self.cpu._uc.mem_write(blk + 0x34, struct.pack('<f', lab[4]))
            self.w32(obj + 0x200, blk)
            self.weapons[name] = obj
        self.dropped = []
        f = self.cpu.code(b'\xc2\x04\x00')
        self.native_stub(f, lambda c: self.dropped.append(self.cstr(c.arg(0))), 4)
        self.cpu._uc.mem_write(ODS_SLOT, struct.pack('<I', f))
        self.native_stub(FIND_WEAPON, lambda c: self.weapons.get(self.cstr(c.arg(0)).decode().lower(), 0))
        self.out = self.alloc(8)

    def weight(self, cfg, k):
        # A thiscall that returns on the x87: push [k]; mov ecx, [cfg]; call; fstp [out]; ret.
        if not hasattr(self, 'wthunk'):
            self.k_cell, self.c_cell = self.alloc(4), self.alloc(4)
            self.wthunk = self.cpu.code(b'\xff\x35' + struct.pack('<I', self.k_cell) + b'\x8b\x0d' + struct.pack('<I', self.c_cell)
                                        + b'\xb8' + struct.pack('<I', WEIGHT) + b'\xff\xd0\xd9\x1d' + struct.pack('<I', self.out) + b'\xc3')
        self.w32(self.k_cell, k), self.w32(self.c_cell, cfg)
        self.cpu.call(self.wthunk, count=2_000_000_000)
        return self.r32(self.out)

    def eval(self, c):
        out = [self.weight(c, k) for k in range(-1, 10)]
        out.append(self.call(UNPLACED, ecx=c) & 0xff)
        out.append(self.call(JETS, ecx=c))
        out.append(self.call(LIMIT, ecx=c))
        out.append(self.call(SLOTS, ecx=c))
        out.append(self.call(VALIDATE, ecx=c, args=[0]) & 0xff)
        return [v & 0xffffffff for v in out]

    def items(self, c):
        sentinel, out, node = self.r32(c + 8), [], None
        node = self.r32(sentinel)
        while node != sentinel:
            out.append(self.r32(node + 8))
            node = self.r32(node)
        return out

    def change(self, c, op, v):
        w, b = self.w32, self.wb
        if op == OP_TONS:
            self.cpu._uc.mem_write(c + 0x2c, struct.pack('<f', v / 2))
        elif op == OP_SINKS:
            w(c + 0x68, v)
        elif op == OP_DOUBLE:
            b(c + 0x6c, v & 1)
        elif op == OP_ENDO:
            b(c + 0x20, v & 1)
        elif op == OP_UNPLACE:
            its = self.items(c)
            if its:
                w(its[v % len(its)] + 0xc, 0xffffffff)
        elif op == OP_ARMOR:
            w(c + 0x38 + 4 * (v % 11), v * 3)
        elif op == OP_COUNT:
            its = self.items(c)
            if its:
                w(its[v % len(its)] + 4, 1 + v % 4)
        elif op == OP_ENGINE:
            w(c + 0x24, v % 79)
        elif op == OP_XL:
            b(c + 0x28, v & 1)
        elif op == OP_NEAR:  # armor tons so that the total is the maximum plus a little: v in hundredths
            total = struct.unpack('<f', struct.pack('<I', self.weight(c, -1)))[0]
            limit = struct.unpack('<f', struct.pack('<I', self.weight(c, 9)))[0]
            tons = struct.unpack('<f', self.raw(c + 0x2c, 4))[0]
            self.cpu._uc.mem_write(c + 0x2c, struct.pack('<f', tons + limit - total + v / 100))
        elif op == OP_JETS:  # every equipment item becomes jump jets
            for it in self.items(c):
                if self.r32(it) == 0x5dfc70:
                    w(it + 0x18, 11)
        elif op == OP_JETS_NEAR:  # too many jets alone: the weight put back in range
            self.change(c, OP_JETS, 0)
            for it in self.items(c):  # placed: an unplaced jet is its own failure
                if self.r32(it) == 0x5dfc70:
                    self.w32(it + 0xc, 1)
            self.change(c, OP_NEAR, 0)
        elif op == OP_BULK:
            b(c + 0x34, 1), b(c + 0x20, 1), b(c + 0x28, 1)

    def wb(self, a, v):
        self.cpu._uc.mem_write(a, bytes([v]))


def main():
    per = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    rel = 'zbd/reader.zbd'
    data = (ROOT / 'game' / 'pm' / rel).read_bytes()
    table = labs(data)
    emu = Emulated(table)
    emu.files[test_pm_zar.key(rel)] = bytearray(data)
    emu.cpu.import_code(md.IAT_FLOOR, md.FLOOR)
    emu.cpu.import_code(0x5bb61c, md.FLOOR.replace(bytes.fromhex('660d0004'), bytes.fromhex('660d0008')))  # ceil
    if not emu.call(md.zr.OPEN_ARCHIVE, args=[emu.string(rel.replace('/', '\\')), 1]):
        raise SystemExit('PM does not open reader.zbd')
    emu.call(0x475860)
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_cfg_lab.argtypes = [ctypes.c_char_p, ctypes.c_float, ctypes.c_int, ctypes.c_float, ctypes.c_int, ctypes.c_float]
    p.probe_pm_cfg_load.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_cfg_load.restype = ctypes.c_int
    p.probe_pm_cfg_eval.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_uint32)]
    for name, lab in table.items():
        p.probe_pm_cfg_lab(name.encode(), *lab)
    rng = random.Random(0x427130)
    out = (ctypes.c_uint32 * 16)()
    n = bad = kept = valid = invalid = 0
    for index, mech in enumerate(md.MECHS):
        name = f'dfn_{mech}.zrd'
        emu.reports = []
        root = emu.call(md.zr.LOAD, args=[emu.string(name), 0, 0])
        dfn = emu.alloc(md.SIZE)
        emu.call(md.CTOR, ecx=dfn, args=[index, root])
        try:
            emu.call(md.READ, ecx=dfn)
        except Exception as ex:
            uc = emu.cpu._uc
            raise SystemExit(f'{name}: eip {uc.reg_read(UC_X86_REG_EIP):#x} ret {emu.r32(uc.reg_read(UC_X86_REG_ESP)):#x}: {ex}')
        cl = emu.r32(dfn + 0x20)
        first, last = emu.r32(cl + 0xc), emu.r32(cl + 0x10)
        count = (last - first) // 4
        got = p.probe_pm_cfg_load(str(ROOT / 'game' / 'pm').encode(), name.encode(), index)
        if got != count:
            print(f'{mech}: exe keeps {count} loadouts, port {got}')
            bad += 1
            continue
        kept += count
        for k in range(count):
            c = emu.r32(first + 4 * k)
            for t in range(per + 1):
                op, v = (OP_NONE, 0) if t == 0 else (rng.randrange(1, 14), rng.randrange(0, 200))
                if op == OP_NEAR:
                    v = rng.choice((0, 10, 20, 23, 24, 25, 26, 30, 50))
                raw, its = emu.raw(c, 0xd8), [(i, emu.raw(i, 0x20)) for i in emu.items(c)]
                emu.change(c, op, v)
                want = emu.eval(c)
                emu.cpu._uc.mem_write(c, raw)
                for i, r in its:
                    emu.cpu._uc.mem_write(i, r)
                p.probe_pm_cfg_eval(k, op, v, out)
                have = list(out)[:16]
                n += 1
                valid += want[15] == 1
                invalid += want[15] == 0
                # weights within a few places: x87 sums against floats
                ok = all(abs(struct.unpack('<i', struct.pack('<I', a))[0] - struct.unpack('<i', struct.pack('<I', b))[0]) <= 4
                         for a, b in zip(want[:11], have[:11])) and want[11:] == have[11:]
                if not ok:
                    bad += 1
                    if bad <= 6:
                        fw = [round(struct.unpack('<f', struct.pack('<I', x))[0], 4) for x in want[:11]]
                        fh = [round(struct.unpack('<f', struct.pack('<I', x))[0], 4) for x in have[:11]]
                        print(f'{mech} {k} step {t} op {op} {v}:\n  exe  {fw} {want[11:]}\n  port {fh} {have[11:]}')
    pm_floors.floors(loadouts=(kept, 113), evaluations=(n, 3000), valid=(valid, 600), invalid=(invalid, 1000))
    print(f'{kept} loadouts kept on both sides; {n} evaluations ({valid} valid, {invalid} not), {bad} differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as ex:
        if isinstance(ex.code, str):
            print(ex.code)
        code = 0 if ex.code in (None, 0) else 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    os._exit(code)

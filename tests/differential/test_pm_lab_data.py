"""The lab's two data panels against the exe, on every shipped loadout: MECH_DATA 0x458380
(the speeds at the mech's most tons, never under 18 and 13.5, the total, the most) and
CHASSIS_DATA 0x458a50 (the tons by kind, structure with shielding). PM's weight function
moved the most tons from MW3's mode 8 to 9. The panels' text and placing are stood in for;
the numbers are the ones the exe hands to sprintf.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_lab_data.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_config as tc  # noqa: E402
import test_pm_mechdef as md  # noqa: E402
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402

MECH_DATA, CHASSIS_DATA = 0x458380, 0x458a50
SPRINTF_SLOT, TIDY, MESSAGE, ENDO, SET_FONT = 0x5de658, 0x403ac0, 0x596de0, 0x458180, 0x5c1230
FONTS = {'label': 0x11, 'value': 0x22, 'alt': 0x33}


class Bench(tc.Emulated):
    def stand_in(self, addr, fn, pop=0):
        if addr in self.cpu.stubs:  # the bench's own: a second hook would run both
            self.cpu.stubs[addr] = (fn, pop)
        else:
            self.native_stub(addr, fn, pop)

    def stand_ins(self):
        self.seen = []
        self.fonts = []
        ret0 = self.cpu.code(b'\xc3')
        self.native_stub(ret0, self.on_sprintf)
        self.w32(SPRINTF_SLOT, ret0)
        self.stand_in(TIDY, lambda c: 0)
        self.stand_in(MESSAGE, lambda c: 0)
        self.stand_in(ENDO, lambda c: 0, 4)
        self.stand_in(SET_FONT, lambda c: 0, 4)
        vt = self.alloc(0x200)
        for slot in range(0, 0x200, 4):
            f = self.cpu.code(b'\xc3')
            pop = {0x90: 4, 0x8c: 8, 0xc: 8}.get(slot, 0)
            self.native_stub(f, (lambda c: self.fonts.append(c.arg(0))) if slot == 0x90 else (lambda c: 0), pop)
            self.w32(vt + slot, f)
        self.panel = self.alloc(0x400)
        self.w32(self.panel, vt)
        rows = self.alloc(0x40)
        for k in range(10):
            row = self.alloc(0x800)
            self.w32(row, vt), self.w32(row + 0x4b8, vt)
            self.w32(rows + 4 * k, row)
        self.w32(self.panel + 0x178, rows)
        self.w32(self.panel + 0x14c, FONTS['label'])
        self.w32(self.panel + 0x150, FONTS['value'])
        self.w32(self.panel + 0x154, FONTS['alt'])

    def on_sprintf(self, c):
        self.seen.append(struct.unpack('<d', struct.pack('<II', c.arg(2), c.arg(3)))[0])
        return 0

    def panels(self, c):
        self.seen, self.fonts = [], []
        self.call(MECH_DATA, ecx=self.panel, args=[c])
        speeds = self.seen
        over = FONTS['alt'] in self.fonts
        self.seen = []
        self.call(CHASSIS_DATA, ecx=self.panel, args=[c])
        return speeds + self.seen, over


def main():
    rel = 'zbd/reader.zbd'
    data = (ROOT / 'game' / 'pm' / rel).read_bytes()
    table = tc.labs(data)
    emu = Bench(table)
    emu.stand_ins()
    emu.files[test_pm_zar.key(rel)] = bytearray(data)
    emu.cpu.import_code(md.IAT_FLOOR, md.FLOOR)
    emu.cpu.import_code(0x5bb61c, md.FLOOR.replace(bytes.fromhex('660d0004'), bytes.fromhex('660d0008')))
    if not emu.call(md.zr.OPEN_ARCHIVE, args=[emu.string(rel.replace('/', '\\')), 1]):
        raise SystemExit('PM does not open reader.zbd')
    emu.call(0x475860)
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_cfg_lab.argtypes = [ctypes.c_char_p, ctypes.c_float, ctypes.c_int, ctypes.c_float, ctypes.c_int, ctypes.c_float]
    p.probe_pm_cfg_load.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    p.probe_pm_cfg_load.restype = ctypes.c_int
    p.probe_pm_cfg_ptr.restype = ctypes.c_void_p
    p.probe_pm_lab_numbers.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_float, ctypes.c_int, ctypes.POINTER(ctypes.c_float)]
    for name, lab in table.items():
        p.probe_pm_cfg_lab(name.encode(), *lab)
    rng = random.Random(0x458380)
    out = (ctypes.c_float * 14)()
    n = bad = floored = 0
    for index, mech in enumerate(md.MECHS):
        name = f'dfn_{mech}.zrd'
        emu.reports = []
        root = emu.call(md.zr.LOAD, args=[emu.string(name), 0, 0])
        dfn = emu.alloc(md.SIZE)
        emu.call(md.CTOR, ecx=dfn, args=[index, root])
        emu.call(md.READ, ecx=dfn)
        cl = emu.r32(dfn + 0x20)
        first, last = emu.r32(cl + 0xc), emu.r32(cl + 0x10)
        count = (last - first) // 4
        if p.probe_pm_cfg_load(str(ROOT / 'game' / 'pm').encode(), name.encode(), index) != count:
            raise SystemExit(f'{mech}: loadouts differ')
        for k in range(count):
            c = emu.r32(first + 4 * k)
            mine = p.probe_pm_cfg_ptr(k)
            for t in range(12):
                engine = struct.unpack('<i', emu.raw(c + 0x24, 4))[0] if t == 0 else rng.randrange(79)
                xl, endo = (emu.raw(c + 0x28, 1)[0], emu.raw(c + 0x20, 1)[0]) if t == 0 else (rng.randrange(2), rng.randrange(2))
                tons = struct.unpack('<f', emu.raw(c + 0x2c, 4))[0] if t == 0 else rng.randrange(0, 80) / 2
                raw = emu.raw(c, 0xd8)
                emu.w32(c + 0x24, engine), emu.wb(c + 0x28, xl), emu.wb(c + 0x20, endo)
                emu.cpu._uc.mem_write(c + 0x2c, struct.pack('<f', tons))
                theirs, over = emu.panels(c)
                emu.cpu._uc.mem_write(c, raw)
                p.probe_pm_lab_numbers(mine, engine, xl, tons, endo, out)
                ours = list(out)
                n += 1
                floored += abs(theirs[0] - 18) < 1e-3
                same = len(theirs) == 14 and all(abs(a - b) < 2e-3 for a, b in zip(theirs, ours))
                if same and over != (ours[2] > ours[3]):
                    same = False
                if not same:
                    bad += 1
                    if bad <= 6:
                        print(mech, k, t, 'exe', [round(x, 3) for x in theirs], over, '\n  port', [round(x, 3) for x in ours])
    pm_floors.floors(cases=(n, 1300), floored=(floored, 1))
    print(f'{n} loadout states, {floored} at the running floor: {bad} differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as ex:
        if isinstance(ex.code, str):
            print(ex.code)
        code = 0 if ex.code in (None, 0) else 1
    sys.stdout.flush()
    import os
    os._exit(code)

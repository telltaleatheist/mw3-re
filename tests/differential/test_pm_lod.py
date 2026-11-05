"""PM's level of detail and the AI's headlights: the table of 0x414430, the budgets of 0x41f3d5 and 0x41f4d0 (with
0x41f8d0, 0x41f460, 0x41ade0, 0x41da90) against mech3/mech_lod, on made-up mechs over both tables (docs/pm-map.md,
Headlights).

    cmake --build build --target pmprobe
    .venv/bin/python -P tests/differential/test_pm_lod.py [cases]

Compared: the table, the detail bits, both budgets, the model swap asked for (0x41f7a0 stubbed), the turn (0x465da0
stubbed) and the distance the light is given (0x465e00 stubbed).
"""
import ctypes
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

SETUP, SETUP_END, BUDGET, LOD = 0x414430, 0x4147d2, 0x41f3d0, 0x41f4d0
SWAP, TURN, DIST, TAIL, TARGET_OF, OPTION, CLIP = 0x41f7a0, 0x465da0, 0x465e00, 0x4b2c90, 0x4f8d80, 0x40b1e0, 0x506f80
BRAIN = 0x4b2af0
GAME_P, VIEW, MECH_VT = 0x62c40c, 0x61874c, 0x5dfaac
SPENT, LIGHTS, LOW, MISSION_FLAG, MAP_OPTION, MAX_TEX, MAX_STAGES = (
    0x6293a0, 0x6293a4, 0x6293a8, 0x62da38, 0x8ccc64, 0x8ccc68, 0x8ccc6c)
TABLE = (LOW, 0x6040c8, 0x6040cc, 0x6040d0, 0x6040d4, 0x6040d8, 0x6040dc, 0x6040e0, 0x6040b4)
ASM = Path(__file__).parents[2] / 'analysis-output' / 'pm.asm'


def u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def f(x):
    return struct.unpack('<f', struct.pack('<I', x))[0]


def pops():
    """Each call target of the set-up, with what its `ret` pops."""
    lines = ASM.read_text(errors='replace').split('\n')
    index = {}
    for i, l in enumerate(lines):
        m = re.match(r'\s+([0-9a-f]+):', l)
        if m:
            index[int(m.group(1), 16)] = i
    out = {}
    for a in range(SETUP, SETUP_END):
        if a not in index:
            continue
        m = re.search(r'call\s+0x([0-9a-f]+)', lines[index[a]])
        if not m:
            continue
        t = int(m.group(1), 16)
        pop = 0
        for l in lines[index[t]:index[t] + 600]:
            r = re.search(r'\bret\s*(0x[0-9a-f]+)?', l)
            if r:
                pop = int(r.group(1), 16) if r.group(1) else 0
                break
        out[t] = pop
    return out


class Emulated(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        self.option, self.far, self.target = 0, 800.0, 0
        self.swap, self.turn, self.dist = -1, -1, 0
        for t, pop in pops().items():
            if t != CLIP and t != OPTION:
                self.native_stub(t, self.new_like, pop)
        self.native_stub(OPTION, lambda c: self.option)
        self.native_stub(CLIP, self.clip, 0)
        self.native_stub(SWAP, self.on_swap, 4)
        self.native_stub(TURN, self.on_turn, 4)
        self.native_stub(DIST, self.on_dist, 4)
        self.native_stub(TAIL, lambda c: 1)
        self.native_stub(BRAIN, lambda c: 0)
        self.native_stub(TARGET_OF, lambda c: self.target)
        self.mech, self.player = self.alloc(0x800), self.alloc(0x800)
        self.w32(self.mech, MECH_VT), self.w32(self.player, MECH_VT)
        game = self.alloc(0x20)
        self.game = game
        self.w32(GAME_P, game)
        self.contact, self.data = self.alloc(0x40), self.alloc(0x10)
        self.w32(self.mech + 0x6a8, self.data)
        self.view, self.camera, self.cam_data = self.alloc(0x10), self.alloc(0x40), self.alloc(0x100)
        self.w32(VIEW, self.view), self.w32(self.view + 4, self.camera), self.w32(self.camera + 0x38, self.cam_data)
        # the throttle's virtual base: controls +4 the base table, whose +4 is the base's offset, then vt+0x1c.
        self.controls, table, vt, self.throttle = self.alloc(0x20), self.alloc(8), self.alloc(0x20), self.alloc(4)
        self.w32(self.player + 0x38c, self.controls), self.w32(self.controls + 4, table), self.w32(table + 4, 8)
        self.w32(self.controls + 4 + 8, vt)
        code = self.cpu.code(b'\xd9\x05' + struct.pack('<I', self.throttle) + b'\xc3')  # fld [throttle]; ret
        self.w32(vt + 0x1c, code)

    def new_like(self, c):
        return self.alloc(0x40)

    def clip(self, c):
        near_p, far_p = c.arg(1), c.arg(2)
        self.w32(near_p, u(1.0)), self.w32(far_p, u(self.far))
        return 0

    def on_swap(self, c):
        self.swap = c.arg(0)
        return 0

    def on_turn(self, c):
        self.turn = c.arg(0) & 0xff
        return 0

    def on_dist(self, c):
        self.dist = c.arg(0)
        return 0

    def setup(self, option, far):
        self.option, self.far = option, far
        self.w32(self.player + 0, MECH_VT)
        self.call(SETUP)
        low = self.r32(LOW) & 0xff
        return [low] + [self.r32(a) for a in TABLE[1:]]

    def budget(self, throttle, flag, option):
        self.w32(self.game + 0x14, self.player)
        self.w32(self.throttle, throttle)
        self.wb(MISSION_FLAG, flag)
        self.w32(MAP_OPTION, option), self.w32(MAX_TEX, 2), self.w32(MAX_STAGES, 2)
        self.call(BUDGET)
        return [self.r32(SPENT), self.r32(LIGHTS)]

    def lod(self, v):
        w, m, p, b = self.w32, self.mech, self.player, self.wb
        w(self.game + 0x14, m if v[0] else p)
        w(m + 0x98, self.contact if v[1] else 0), w(self.contact + 0xc, v[2])
        w(p + 0x6c, 1 if v[3] else 0)
        self.target = m if v[4] else p
        w(m + 0x6b4, v[5]), b(m + 8, v[6]), w(m + 0x59c, v[7])
        w(m + 0xc4, v[8]), b(m + 0xe2, v[9]), b(m + 0xe0, v[10]), w(p + 0xc0, v[11])
        w(SPENT, v[12]), w(LIGHTS, v[13])
        w(self.data, v[14]), w(self.data + 4, v[15])
        for i in range(3):
            w(self.contact + 0x10 + 4 * i, v[16 + i])
            w(self.cam_data + 0x74 + 4 * i, v[19 + i])
        w(self.contact + 0x1c, u(-1.0))
        w(m + 0x6ac, 0x12345678)
        self.swap, self.turn, self.dist = 0xffffffff, 0xffffffff, 0
        self.call(LOD, ecx=m)
        return [self.r32(m + 0x6ac), self.r32(SPENT), self.r32(LIGHTS), self.swap & 0xffffffff, self.turn, self.dist]

    def w32(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def wb(self, a, v):
        self.cpu._uc.mem_write(a, bytes([v]))


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_lod.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint32)]
    p.probe_pm_lod_setup.argtypes = [ctypes.c_int32, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)]
    p.probe_pm_lod_budget.argtypes = [ctypes.c_uint32, ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(ctypes.c_uint32)]
    emu = Emulated()
    rng = random.Random(0x41f4d0)
    out = (ctypes.c_uint32 * 9)()

    tables = 0
    for option in (0, 1, 2):
        for far in (40.0, 100.0, 200.0, 250.0, 300.0, 600.0, 800.0, 1200.0):
            want = emu.setup(option, far)
            p.probe_pm_lod_setup(option, u(far), out)
            if want != list(out):
                raise SystemExit(f'table {option} {far}\n exe  {want}\n port {list(out)}')
            tables += 1

    budgets = 0
    for throttle in (0.0, -0.0, 0.5, -0.5):
        for flag in (0, 1):
            for option in (0, 1, 2, -1):
                want = emu.budget(u(throttle), flag, option)
                p.probe_pm_lod_budget(u(throttle), flag, option, out)
                if want != list(out)[:2]:
                    raise SystemExit(f'budget {throttle} {flag} {option}\n exe  {want}\n port {list(out)[:2]}')
                budgets += 1

    seen = dict(lit=0, off=0, swap=0, player=0, sound=0, faced_away=0, low=0)
    for n in range(cases):
        if n % 200 == 0:  # a new table now and then, the same on both sides, low every other time
            option = 2 if n % 400 else 0
            far = rng.choice((40.0, 200.0, 600.0, 800.0, 1200.0))
            emu.setup(option, far)
            p.probe_pm_lod_setup(option, u(far), out)
        d = rng.choice((rng.uniform(0, 7000), rng.uniform(0, 70000), rng.uniform(60000, 800000), 2500.0, 5625.0, 62500.0))
        delta = [rng.uniform(-200, 200) for _ in range(3)]
        axis = [rng.uniform(-1, 1) for _ in range(3)]
        v = [int(rng.random() < 0.05), int(rng.random() < 0.9), u(d), rng.randrange(2), rng.randrange(2), rng.randrange(2),
             int(rng.random() < 0.9), rng.choice((1, 1, 1, 10, 12)), rng.choice((0, 0, 2, 4)),
             int(rng.random() < 0.1), int(rng.random() < 0.1), rng.choice((1, 2, 4)),
             rng.randrange(3), rng.randrange(2), u(rng.choice((22500.0, 90000.0, 250000.0))),
             u(rng.choice((10000.0, 40000.0)))] + [u(x) for x in delta] + [u(x) for x in axis]
        want = emu.lod(v)
        p.probe_pm_lod((ctypes.c_uint32 * 22)(*v), out)
        got = list(out)[:6]
        if want != got:
            raise SystemExit(f'case {n}: {v}\n exe  {[hex(x) for x in want]}\n port {[hex(x) for x in got]}')
        seen['player'] += v[0]
        seen['low'] += emu.r32(LOW) & 0xff
        seen['lit'] += want[4] == 1
        seen['off'] += want[4] == 0
        seen['swap'] += want[3] != 0xffffffff
        seen['sound'] += want[0] & 1
        # In the low table's bands only facing away clears the bit, a powered-down mech aside.
        if emu.r32(LOW) & 0xff and not (v[0] or v[9] or v[10]) and v[1] and d < f(emu.r32(TABLE[2])) and not want[0] & 0x10:
            seen['faced_away'] += 1
    pm_floors.floors(tables=(tables, 24), budgets=(budgets, 32), cases=(cases, 20000))
    print(f'ok: {tables} tables, {budgets} budgets, {cases} cases identical; {seen}')


if __name__ == '__main__':
    main()

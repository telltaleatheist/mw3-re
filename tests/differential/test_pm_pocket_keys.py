"""The pocket commander's own keys against PM's exe: test_pocket_keys moved. 0x46cc50 on the mode the dispatcher
0x46b4d0 makes (test_pm_pocket.py's world), with its mouse look 0x46cae0 and the smoothing 0x4285d0 after, at 24 bits.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_pocket_keys.py

PM's keys are MW3's 0x46b0a0 with PM's offsets: the owner at the IControls' +0x20, the mech's vt+0xac (powered down),
vt+0x150 (airborne), its jets at +0x3e8 and rates at +0x544..+0x54c, the HUD's zoom box at +0x1520 and its lock at
+0x1694, the look at +0x158..+0x164. The commands are read by 0x536c50 (cdecl), the trigger is 0x3e (0x468ff0), and
the directional jets ask the game's vt+0x44 ([0x62c40c]+0x6c), which answers !0x40afd0 (the MW2 option) in single
player. The smoothing's powered-down reset is the controls' vt+0x18 (0x4289a0: the look's fields too). Stood in:
the commands, the mouse (0x534fe0) and its set (0x534c50, logged), the pilot's tick (0x4e8da0, logged), the zoom
box (logged), the game's vt+0x44. Each case sets the mode's fields at random and runs one frame; the log and the
fields after must match the port's PocketControls::keys (tests/probe_pocket.cpp's probe_pocket_keys).
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
X87 = os.environ.get('MW3_X87', '24')  # the controls' port, as test_pm_actions; at 53 see docs/pm-tests.md
import pm_floors  # noqa: E402
import test_pm_pocket as tpp  # noqa: E402
import test_pm_zar  # noqa: E402
from original import FTOL  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

os.environ['MW3_X87'] = X87  # test_pm_pocket's import sets 53

KEYS, PILOT = 0x46cc50, 0x62c40c
CMDS = [0x18, 0x19, 2, 3, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x14, 8, 9, 4, 5, 0x3e]
COVER = {0x46cb88: 'mouse: view 0', 0x46cb42: 'mouse: view 1', 0x46cbc2: 'mouse: view 2', 0x46cc06: 'mouse: view 3',
         0x46cff6: 'throttle returns', 0x46d040: 'joystick throttle', 0x46ce60: 'airborne turn',
         0x46ceb7: 'zoom in', 0x46cecc: 'zoom out'}
REACHED = set()


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


class Exe(tpp.Exe):
    def __init__(self):
        super().__init__()
        z, w = self.z, self.w
        del z.cpu.stubs[KEYS]  # test_pm_pocket.py's stand-in: the code under test here
        w(0x5de644, z.cpu.code(FTOL))  # MSVCRT's _ftol, 0x5cef9a's import
        mvt = z.r32(self.mech)
        w(mvt + 0x150, self.ret(lambda c: self.k['air']))
        z.native_stub(0x536c50, lambda c: self.k['cmd'].get(c.arg(0), 0))
        self.mouse = self.zalloc(0x40)
        z.native_stub(0x534fe0, lambda c: self.mouse)
        z.native_stub(0x534c50, lambda c: self.log(0x33, c.arg(0), c.arg(1)))
        z.native_stub(0x4e8da0, lambda c: self.log(0x31))
        zvt = self.zalloc(0x80)
        w(zvt + 0x60, self.ret(lambda c: self.log(0x32, 1), 4))
        w(zvt + 0x64, self.ret(lambda c: self.log(0x32, 0)))
        self.zvt = zvt
        # The game's vt+0x44: 0x435070, !0x40afd0 (the MW2 option).
        self.pilot, game, gvt = self.zalloc(0x80), self.zalloc(0x10), self.zalloc(0x80)
        w(self.pilot + 0x6c, game), w(game, gvt)
        w(gvt + 0x44, self.ret(lambda c: 0 if self.k['mw2'] else 1))
        for addr in COVER:
            self.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def keys(self, ints, fl):
        z, w, r32 = self.z, self.w, self.z.r32
        self.at = self.arena + 0x20000 if self.at > self.arena + 0x300000 else self.at
        self.k = {'air': ints[2], 'mw2': ints[3], 'cmd': dict(zip(CMDS, ints[13:30]))}
        # The mode, made as test_pm_pocket.py makes it, not powered down; viewed with a HUD or not.
        self.inp, self.out, self.deleted, self.maps = [640, 0, 1, ints[4], 0, 0, 0, 0, 0, 0], [], [], [0x7000]
        w(self.view + 0x58, self.camera)
        w(self.camera + 4, 0)
        w(self.view + 0x64, self.mech)
        w(self.mech + 0x59c, 0)
        prev = self.zalloc(0x1c0)
        z.call(tpp.PLAYER, prev, [1, 1])
        w(prev + 0x188, self.mech)
        w(tpp.INSTANCE, prev)
        p = z.call(tpp.DISPATCH, args=[prev, 2, 1])
        assert p not in (0, prev)
        self.inp[4] = ints[0]  # the mech's vt+0xac
        self.uc.mem_write(self.mech + 0xe2, bytes([ints[0]]))  # the smoothing's own read
        self.uc.mem_write(self.hud + 0x1694, bytes([ints[5]]))
        w(self.hud + 0x1520, self.zvt)
        w(self.mech + 0x3e8, ints[1])
        for i, off in enumerate((0x544, 0x548, 0x54c)):
            self.uc.mem_write(self.mech + off, struct.pack('<f', fl[18 + i]))
        self.uc.mem_write(self.mouse + 0xc, struct.pack('<2f', fl[1], fl[2]))
        self.uc.mem_write(self.mouse + 0x24, struct.pack('<2f', fl[3], fl[4]))
        w(p + 8, ints[6])
        w(p + 0xc, ints[7])
        for off, v in ((0x3c, ints[8]), (0x3d, ints[9]), (0x3e, ints[10]), (0x3f, ints[11]), (0x50, ints[12])):
            self.uc.mem_write(p + off, bytes([v]))
        for off, v in ((0x160, fl[5]), (0x164, fl[6]), (0x158, fl[7]), (0x15c, fl[8]), (0x10, fl[9]), (0x14, fl[10]),
                       (0x18, fl[11]), (0x1c, fl[12]), (0x20, fl[13]), (0x44, fl[14]), (0x48, fl[15]), (0x4c, fl[16]),
                       (0x40, fl[17])):
            self.uc.mem_write(p + off, struct.pack('<f', v))
        was = r32(PILOT)
        w(PILOT, self.pilot)
        self.out = []
        z.call(KEYS, p, [fb(fl[0])])
        w(PILOT, was)
        out = self.out + [0x40]
        for off in (0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x44, 0x48, 0x4c, 0x160, 0x164, 0x158, 0x15c):
            out.append(r32(p + off))
        out += [bytes(self.uc.mem_read(p + off, 1))[0] for off in (0x34, 0x3c, 0x3e, 0x50)]
        return out


def case(rng):
    ints = [int(rng.random() < 0.1), rng.choice([0, 0, 2, 4]), rng.randrange(2), rng.randrange(2),
            int(rng.random() < 0.8), int(rng.random() < 0.3), rng.choice([0, 1, 4, 5]), rng.randrange(4)]
    ints += [int(rng.random() < 0.2), rng.randrange(2), rng.randrange(2), rng.randrange(2), rng.randrange(2)]
    ints += [rng.choice([0, 0, 0, 1, 2, 4, 5]) for _ in CMDS]
    dt = rng.choice([1 / 30, 1 / 60, 0.1, 0.004])
    fl = [dt] + [rng.uniform(-1, 1) for _ in range(2)] + [rng.uniform(-30, 30) for _ in range(2)]
    fl += [rng.uniform(-1, 1) for _ in range(4)]
    fl += [rng.uniform(-1, 1) for _ in range(4)] + [rng.uniform(0, 1)]
    fl += [rng.choice([0.0, rng.uniform(-1, 1), 0.03, -0.04]) for _ in range(3)] + [rng.uniform(-1, 1)]
    fl += [rng.uniform(0.5, 4) for _ in range(3)]
    return ints, [f32(x) for x in fl]


def main():
    p = test_pm_zar.load_pm_probe()
    P = ctypes.c_void_p
    p.probe_pocket_keys.argtypes = [P, P, P, ctypes.c_int]
    rng = random.Random(int(os.environ.get('SEED', '1')))
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 1500
    emu = Exe()
    out = (ctypes.c_uint32 * 128)()
    zooms = mouse_sets = ticks = 0
    for k in range(n):
        ints, fl = case(rng)
        a = emu.keys(ints, fl)
        m = p.probe_pocket_keys((ctypes.c_int32 * len(ints))(*ints), (ctypes.c_float * len(fl))(*fl), out, 128)
        b = list(out[:m])
        if a != b:
            i = next((j for j, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
            print(f'case {k}: differ at {i}\n  exe  {a[max(0, i - 4):i + 6]}\n  port {b[max(0, i - 4):i + 6]}\n'
                  f'  in {ints}\n  f {fl}')
            return 1
        zooms += 0x32 in a[:a.index(0x40)]
        mouse_sets += 0x33 in a[:a.index(0x40)]
        ticks += 0x31 in a[:a.index(0x40)]
    never = [COVER[x] for x in COVER if x not in REACHED]
    if never:
        print(f'branches never reached: {never}')
        return 1
    print(f'ok: {n} cases identical; {len(COVER)} branches reached; {zooms} zooms, {mouse_sets} mouse sets, {ticks} ticks')
    pm_floors.floors(cases=(n, 500), zooms=(zooms, 100), mouse_sets=(mouse_sets, 100), ticks=(ticks, 300))
    return 0


if __name__ == '__main__':
    sys.exit(main())

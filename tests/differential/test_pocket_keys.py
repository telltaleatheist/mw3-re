"""The pocket commander's own keys (docs/ai.md 17.6d) against the original
x86: 0x46b0a0 on the mode made by the dispatcher (test_pocket.py's world),
with its mouse look 0x46af30 and the smoothing 0x426960 after.

    cmake --build build && .venv/bin/python tests/differential/test_pocket_keys.py

Added to test_pocket.py's world: the commands' states (0x51d300 for the 17
the mode reads, 0x467480's 0x3d included), the mouse (0x51b7b0: fx, fy,
fdx, fdy), its set (0x51b440, logged), the MW2 jets option (0x40d1e0), the
pilot's tick (0x4d8990, logged), the mech's jets (+0x3d8), airborne
(vt+0x144) and rates (+0x530..+0x538), the viewed HUD's zoom box (+0x1508
vt+0x60 / +0x64, logged) and its lock (+0x167c). Each case sets the mode's
fields at random and runs one frame; the log and the fields after must
match the port's PocketControls::keys (tests/probe_pocket.cpp). The x87
runs at 24 bits; the per-second options are off (the original's path).
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import test_pocket as tpk  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe = tpk.probe
P = ctypes.c_void_p
probe.probe_pocket_keys.argtypes = [P, P, P, ctypes.c_int]
CMDS = [0x18, 0x19, 2, 3, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x14, 8, 9, 4, 5, 0x3d]
COVER = {0x46afd8: 'mouse: view 0', 0x46af92: 'mouse: view 1', 0x46b012: 'mouse: view 2', 0x46b056: 'mouse: view 3',
         0x46b437: 'throttle returns', 0x46b490: 'joystick throttle', 0x46b2bc: 'airborne turn',
         0x46b302: 'zoom in', 0x46b31c: 'zoom out'}
REACHED = set()


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


class Emulated(tpk.Emulated):
    def __init__(self):
        super().__init__()
        cpu = self.cpu
        del cpu.stubs[0x46b0a0]  # test_pocket.py's stand-in: the code under test here
        mvt = self.r32(self.mech)
        cpu.u32(mvt + 0x144, self.stub(lambda c: self.k['air']))
        cpu.stub_function(0x51d300, lambda c: self.k['cmd'].get(c.ecx, 0))
        self.mouse = self.zalloc(0x40)
        cpu.stub_function(0x51b7b0, lambda c: self.mouse)
        cpu.stub_function(0x51b440, lambda c: self.log(0x33, c.arg(0), c.arg(1)), 8)
        cpu.stub_function(0x40d1e0, lambda c: self.k['mw2'])
        cpu.stub_function(0x4d8990, lambda c: self.log(0x31))
        zoom, zvt = self.zalloc(0x10), self.zalloc(0x80)
        cpu.u32(zoom, zvt)
        cpu.u32(zvt + 0x60, self.stub(lambda c: self.log(0x32, 1), 4))
        cpu.u32(zvt + 0x64, self.stub(lambda c: self.log(0x32, 0)))
        self.zoom = zoom
        for addr in COVER:
            cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, u: REACHED.add(ad), begin=addr, end=addr)

    def keys(self, ints, fl):
        cpu = self.cpu
        self.k = {'air': ints[2], 'mw2': ints[3], 'cmd': dict(zip(CMDS, ints[13:30]))}
        # The mode, made as test_pocket.py makes it: viewed with a HUD or not.
        inp = [640, 0, 1, ints[4], ints[0], 0, 0, 0]
        self.inp, self.out, self.deleted, self.n59 = inp, [], [], 0
        cpu.u32(self.view + 0x58, self.camera)
        cpu.u32(self.view + 0x64, self.mech)
        cpu.write(self.hud + 0x167c, bytes([ints[5]]))
        cpu.u32(self.hud + 0x1508, self.r32(self.zoom))  # the zoom box's vtable in place at +0x1508
        cpu.write(self.mech + 0xde, bytes([ints[0]]))
        cpu.u32(self.mech + 0x3d8, ints[1])
        for i, off in enumerate((0x530, 0x534, 0x538)):
            cpu.write(self.mech + off, struct.pack('<f', fl[18 + i]))
        prev = self.zalloc(0x160)
        cpu.call(0x427320, ecx=prev, stack_args=(1, 1))
        cpu.u32(prev + 0x15c, self.mech)
        cpu.u32(0x6056ec, prev)
        cpu.u32(0x609980, 0)
        cpu.call(0x469940, ecx=prev, edx=2, stack_args=(1,))
        p = cpu.uc.reg_read(UC_X86_REG_EAX)
        self.out = []
        cpu.write(self.mouse + 0xc, struct.pack('<2f', fl[1], fl[2]))
        cpu.write(self.mouse + 0x24, struct.pack('<2f', fl[3], fl[4]))
        cpu.u32(p + 8, ints[6])
        cpu.u32(p + 0xc, ints[7])
        for off, v in ((0x3c, ints[8]), (0x3d, ints[9]), (0x3e, ints[10]), (0x3f, ints[11]), (0x50, ints[12])):
            cpu.write(p + off, bytes([v]))
        for off, v in ((0x140, fl[5]), (0x144, fl[6]), (0x138, fl[7]), (0x13c, fl[8]), (0x10, fl[9]), (0x14, fl[10]),
                       (0x18, fl[11]), (0x1c, fl[12]), (0x20, fl[13]), (0x44, fl[14]), (0x48, fl[15]), (0x4c, fl[16]),
                       (0x40, fl[17])):
            cpu.write(p + off, struct.pack('<f', v))
        cpu.call(0x46b0a0, ecx=p, stack_args=(fb(fl[0]),))
        out = self.out + [0x40]
        for off in (0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x44, 0x48, 0x4c, 0x140, 0x144, 0x138, 0x13c):
            out.append(self.r32(p + off))
        out += [self.cpu.read(p + off, 1)[0] for off in (0x34, 0x3c, 0x3e, 0x50)]
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
    rng = random.Random(int(os.environ.get('SEED', '1')))
    n = int(os.environ.get('POCKET_KEYS_CASES', '2000'))
    emu = Emulated()
    out = (ctypes.c_uint32 * 128)()
    for k in range(n):
        ints, fl = case(rng)
        a = emu.keys(ints, fl)
        m = probe.probe_pocket_keys((ctypes.c_int32 * len(ints))(*ints), (ctypes.c_float * len(fl))(*fl), out, 128)
        b = list(out[:m])
        if a != b:
            i = next((j for j, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
            print(f'case {k}: differ at {i}\n  exe  {a[max(0, i - 4):i + 6]}\n  port {b[max(0, i - 4):i + 6]}\n'
                  f'  in {ints}\n  f {fl}')
            sys.stdout.flush(); os._exit(1)
    never = [COVER[x] for x in COVER if x not in REACHED]
    if never:
        print(f'branches never reached: {never}')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {n} cases identical; {len(COVER)} branches reached')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()

"""The animations' screen wave against the original x86 (docs/anim-run.md,
The screen wave): event 37 0x5076c0 (FBFX_CSINWAVE_FROM_TO) into the weather
manager's waves 0x5a6760 -> 0x5a6650 -> 0x5a5400 (set_pos 0x409a40, show_for
0x59ff10), then the placed wave's draw 0x5a5360 -> 0x5a5440 with the ripple
0x558dd0 stubbed and recorded.

    cmake --build build --target mw3probe && .venv/bin/python tests/differential/test_screen_wave.py [runs]
    MW3_PRECISION=53 .venv/bin/python tests/differential/test_screen_wave.py

Stubbed on the exe's side and answered by hooks on the port's: the distance to
the player 0x507340, the node's point to the world 0x4ed650 and the projection
0x53cf10. The focal length ([0x73fb9c], 0x51e8d0) and the screen's size
(0x576cb0 / 0x576cc0) are set on both. Each run: random flags (screen place,
at a node with an index -1..7 and its ref set or not, world radius), values,
a duration (some 0), the sequence's time before, at or after it, its state,
the projection on or off the screen, the wave count before (0..4) and each
slot's flags and timer; at 24 bits (play), or 53 with MW3_PRECISION=53. Compared: the
result, the time left, the event's +0x3c..+0x44, the count, all five slots,
the calls out and the ripple drawn into each manager rect. Where the exe uses
its uninitialised x and y (no screen place and no node), x and y are left out
(the port's FIX (A) gives 0), and a NaN's sign is not compared (x87's is
negative, the host's positive: a duration of 0).
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import FTOL, IAT_FTOL, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

MANAGER, LEFT, FX, BACK, DIRTY = 0x736328, 0x73f760, 0x73fb9c, 0x8027a4, 0x5f49b8
SLOT_VT = 0x5d04a8
U, I = ctypes.c_uint32, ctypes.c_int32


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Emulated:
    def __init__(self):
        self.cpu = cpu = Original()
        cpu.fpu_control(0x007f)  # play precision (MW3_PRECISION=53: the reference runtime's)
        cpu.import_code(IAT_FTOL, FTOL)
        self.d2 = cpu.alloc(4)
        cpu.write(0x507340, b'\xd9\x05' + struct.pack('<I', self.d2) + b'\xc3')  # fld dword [d2]; ret
        cpu.stub_function(0x4ed650, self.world_point, 0)
        cpu.stub_function(0x53cf10, self.project, 0)
        cpu.stub_function(0x558dd0, self.ripple, 0x18)
        self.ev, self.seq, self.rects = cpu.alloc(0x70), cpu.alloc(0x40), cpu.alloc(0x20)
        self.defn, self.refs = cpu.alloc(0x13c), cpu.alloc(0x28 * 8)
        self.node = self.refs + 0x28 * 7

    def world_point(self, c):
        p = c.edx
        self.asked += [1] + list(struct.unpack('<3I', c.read(p, 12)))
        c.write(p, struct.pack('<3I', *self.moved))
        return 0

    def project(self, c):
        self.asked += [2] + list(struct.unpack('<3I', c.read(c.ecx, 12)))
        c.write(c.edx, struct.pack('<3I', self.screen[0], self.screen[1], 0))
        return self.code

    def ripple(self, c):
        rect = c.arg(5)
        r0, r1 = self.r32(MANAGER + 0x10), self.r32(MANAGER + 0x14)
        self.draws += [c.ecx, c.edx] + [c.arg(i) for i in range(5)] + [1 if rect and rect == r0 else 2 if rect and rect == r1 else 0]
        return 0

    def r32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def run(self, ev, now, state, left, has_node, d2, fx, w, h, moved, code, screen, count, slots, rects, r8):
        cpu = self.cpu
        self.moved, self.code, self.screen = moved, code, screen
        self.asked, self.draws = [], []
        cpu.u32(self.d2, d2)
        cpu.u32(FX, fx)
        cpu.u32(BACK, w), cpu.u32(BACK + 4, h)
        cpu.write(self.ev, ev)
        cpu.write(self.seq, bytes(0x40))
        cpu.u32(self.seq + 0x28, now)
        cpu.write(self.seq + 0x20, bytes([state]))
        cpu.u32(LEFT, left)
        cpu.write(self.defn, bytes(0x13c))
        cpu.write(self.refs, bytes(0x28 * 8))
        for i in range(8):
            cpu.u32(self.refs + 0x28 * i + 0x24, self.node if has_node else 0)
        cpu.u32(self.defn + 0x118, self.refs)
        cpu.u32(self.defn + 0x64, self.node)
        cpu.write(self.rects, struct.pack('<8i', *r8))
        cpu.u32(MANAGER + 0x10, self.rects if rects & 1 else 0)
        cpu.u32(MANAGER + 0x14, self.rects + 16 if rects & 2 else 0)
        cpu.u32(MANAGER + 0x18, 0)  # no 2D text target
        cpu.u32(MANAGER + 0x1ec, count)
        for i in range(5):
            s = MANAGER + 0x70 + 0x4c * i
            cpu.write(s, bytes(0x4c))
            cpu.u32(s, SLOT_VT)
            cpu.u32(s + 0x08, MANAGER)
            cpu.u32(s + 0x0c, slots[2 * i])
            cpu.u32(s + 0x10, slots[2 * i + 1])
        cpu.call(0x5076c0, ecx=self.defn, edx=self.seq, stack_args=(self.ev,))
        out = [cpu.uc.reg_read(UC_X86_REG_EAX), self.r32(LEFT)] + [self.r32(self.ev + o) for o in (0x3c, 0x40, 0x44)]
        out.append(self.r32(MANAGER + 0x1ec))
        for i in range(5):
            s = MANAGER + 0x70 + 0x4c * i
            out += [self.r32(s + o) for o in (0x14, 0x18, 0x38, 0x3c, 0x40, 0x44, 0x48, 0x0c, 0x10)]
        cpu.call(0x5a5360, ecx=MANAGER + 0x70 + 0x4c * min(count, 4))
        return out + [len(self.asked)] + self.asked + [len(self.draws)] + self.draws


def ftf(r, lo, hi):
    return r.uniform(lo, hi) if r.random() < 0.85 else float(r.randint(int(lo), int(hi))) + r.choice([0.5, -0.5, 0.0])


def event(r):
    flags = r.choice([1, 2, 3, 8, 9, 10, 11, 0, 4, 5])
    idx = r.choice([-1, 0, 1, 2, 3, 7]) if flags & 2 else r.randint(-1, 7)
    ev = bytearray(0x70)
    ev[0], ev[1] = 37, 0x70
    struct.pack_into('<Hh', ev, 0x0c, flags, idx)
    struct.pack_into('<3f', ev, 0x10, *[r.uniform(-20, 20) for _ in range(3)])
    for off, (lo, hi) in ((0x1c, (0, 640)), (0x28, (0, 480))):
        struct.pack_into('<3f', ev, off, ftf(r, lo, hi), ftf(r, lo, hi), r.uniform(-300, 300))
    struct.pack_into('<2f', ev, 0x34, r.uniform(0.2, 60), r.uniform(0.2, 60))
    struct.pack_into('<3f', ev, 0x3c, ftf(r, 0, 200), ftf(r, 0, 200), r.uniform(-100, 100))
    for off in (0x48, 0x54, 0x60):
        struct.pack_into('<3f', ev, off, ftf(r, -50, 50), ftf(r, -50, 50), r.uniform(-40, 40))
    dur = r.choice([r.uniform(0.05, 5), 1.0, 0.25, 0.0])
    struct.pack_into('<f', ev, 0x6c, dur)
    return bytes(ev), flags, idx, dur


def mask_xy(out, flags, idx, count):
    """x and y of the slot the call went to and of its draw, where the exe's are uninitialised."""
    if flags & 1 or (flags & 2 and idx > 0) or out[0] == 2 and flags & 1:
        return out
    out = list(out)
    s = 6 + 9 * min(count, 4)
    out[s] = out[s + 1] = 0
    k = 6 + 45
    k += 1 + out[k]
    if out[k]:
        for d in range(out[k] // 8):
            out[k + 1 + 8 * d] = out[k + 2 + 8 * d] = 0
    return out


def main():
    probe = load_probe()
    P = ctypes.POINTER
    probe.probe_wave_run.argtypes = [ctypes.c_char_p, U, I, U, I, U, U, I, I, P(U), U, P(U), I, P(U), I, P(I), P(U)]
    probe.probe_wave_run.restype = I
    emu = Emulated()
    dirty = emu.r32(DIRTY)
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    r = random.Random(37)
    seen = {'running': 0, 'done': 0, 'off screen': 0, 'world radius': 0, 'clamped': 0, 'placed at node': 0,
            'draws': 0, 'fifth reused': 0}
    for i in range(runs):
        ev, flags, idx, dur = event(r)
        now = r.choice([r.uniform(0, dur), dur, r.uniform(dur, dur + 2), 0.0])
        state = r.choice([0, 0, 0, 1])
        left = fb(r.uniform(0, 0.1))
        has_node = r.random() < 0.7
        d2 = fb(r.choice([r.uniform(1, 1e6), r.uniform(0.01, 10), 0.0]))
        fx = fb(r.uniform(200, 900))
        w, h = r.choice([(640, 480), (1024, 768), (800, 600), (480, 640), (1920, 1080)])
        moved = [fb(r.uniform(-500, 500)) for _ in range(3)]
        code = r.choice([0, 0, 0, 1, 4, 8])
        screen = [fb(ftf(r, -10, 1000)), fb(ftf(r, -10, 800))]
        count = r.choice([0, 1, 2, 3, 4, 4])
        slots = []
        for _ in range(5):
            slots += [r.choice([0x10, 0x10, 0x00, 0x01, 0x11, 0x1c, 0x02]), fb(r.choice([0.0, 0.5, -1.0]))]
        rects = r.randrange(4)
        r8 = [r.randrange(-50, 700) for _ in range(8)]
        want = emu.run(ev, fb(now), state, left, has_node, d2, fx, w, h, moved, code, screen, count, slots, rects, r8)
        buf = (U * 512)()
        n = probe.probe_wave_run(ev, fb(now), state, left, int(has_node), d2, fx, w, h, (U * 3)(*moved), code,
                                 (U * 2)(*screen), count, (U * 10)(*slots), rects, (I * 8)(*r8), buf)
        got = list(buf[:n])
        want, got = mask_xy(want, flags, idx, count), mask_xy(got, flags, idx, count)
        # A NaN's sign (x87 makes 0xffc00000, the host 0x7fc00000: a duration of 0).
        want = [0x7fc00000 if x == 0xffc00000 else x for x in want]
        if want != got:
            k = next((j for j in range(min(len(want), len(got))) if want[j] != got[j]), min(len(want), len(got)))
            print(f'run {i}: differs at word {k} (flags {flags:#x} idx {idx} dur {dur} now {now} '
                  f'state {state} count {count})')
            print('  original', [hex(x) for x in want[max(0, k - 3):k + 6]])
            print('  port    ', [hex(x) for x in got[max(0, k - 3):k + 6]])
            raise SystemExit(1)
        seen['running' if want[0] == 1 else 'done'] += 1
        seen['off screen'] += bool(flags & 2 and idx > 0 and code)
        seen['world radius'] += bool(flags & 8 and state == 0)
        seen['clamped'] += bool(flags & 8 and state == 0 and struct.unpack('<f', struct.pack('<I', want[2]))[0] == max(w, h) / 4)
        seen['placed at node'] += bool(flags & 2 and idx > 0 and not code)
        seen['fifth reused'] += count == 4
        k = 6 + 45
        k += 1 + want[k]
        seen['draws'] += want[k] // 8
    print(f'screen wave: {runs} events identical (dirty bits {dirty:#x}); {seen}')
    short = runs >= 3000 and min(seen.values()) < runs // 100
    if short:
        print('too little reached')
        raise SystemExit(1)


if __name__ == '__main__':
    main()

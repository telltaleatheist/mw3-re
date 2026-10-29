"""The animations' screen tint against the original x86 (docs/anim-run.md,
The screen tint): event 36 0x5074b0 with its colour pack 0x579370 (565) and
the weather manager's tint 0x5a6730 -> 0x5a6620 -> show_for 0x59ff10, then
the tint's draw 0x5a5360 -> 0x5a53c0 -> 0x558540 (Direct3D), with the quad
0x57dd80 stubbed and recorded.

    cmake --build build && .venv/bin/python -P tests/differential/test_screen_tint.py [runs]

Each run: an event with random starts, ends and rates (some past 0..1, some
on a byte's rounding edge), a duration, the sequence's time before, at or
after it, the time left, the tint's flags and timer, and the manager's rects
(none, either, both). The FPU at 24 bits (play). Compared:
- the result and the time left after;
- the tint's colour, alpha, flags and timer;
- each quad's colour, rect and alpha, and which rect +0x34 keeps.
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

MANAGER, TINT, LEFT, D3D = 0x736328, 0x736350, 0x73f760, 0x72f348
U = ctypes.c_uint32


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def channel(r):
    """A start, end and rate: mostly in 0..1, some past it, some on a byte's edge."""
    def value():
        k = r.random()
        if k < 0.6:
            return r.random()
        if k < 0.8:
            return r.uniform(-0.5, 1.5)
        return (r.randrange(256) + 0.5) / 255 + r.choice([-1e-7, 0, 1e-7])
    return value(), value(), r.uniform(-3, 3) if r.random() < 0.8 else 0.0


class Emulated:
    def __init__(self):
        self.cpu = cpu = Original()
        cpu.fpu_control(0x007f)  # play precision
        cpu.import_code(IAT_FTOL, FTOL)
        for a, v in ((0x802738, 8), (0x80273c, 3), (0x802740, 3), (0x802744, 0xf8), (0x802748, 0xfc), (D3D, 1)):
            cpu.u32(a, v)
        self.quads = []
        cpu.stub_function(0x57dd80, self.quad, 8)
        self.ev, self.seq, self.rects = cpu.alloc(0x40), cpu.alloc(0x40), cpu.alloc(0x20)
        self.defn = cpu.alloc(0x100)

    def quad(self, c):
        rect = struct.unpack('<4i', c.read(c.edx, 16)) if c.edx else (0, 0, 0, 0)
        self.quads.append([c.ecx & 0xffff, int(c.edx != 0)] + [v & 0xffffffff for v in rect] + [c.arg(0), c.arg(1)])
        return 0

    def run(self, ev, now, left, flags, timer, rects, r8):
        cpu = self.cpu
        cpu.write(self.ev, ev)
        cpu.write(self.seq, bytes(0x40))
        cpu.u32(self.seq + 0x28, now)
        cpu.u32(LEFT, left)
        cpu.write(self.rects, struct.pack('<8i', *r8))
        cpu.u32(MANAGER + 0x10, self.rects if rects & 1 else 0)
        cpu.u32(MANAGER + 0x14, self.rects + 16 if rects & 2 else 0)
        cpu.u32(MANAGER + 0x18, 0)  # no 2D text target: no finish target
        cpu.write(TINT, bytes(0x48))
        cpu.u32(TINT, 0x5d0290)
        cpu.u32(TINT + 0x08, MANAGER)
        cpu.u32(TINT + 0x0c, flags)
        cpu.u32(TINT + 0x10, timer)
        cpu.call(0x5074b0, ecx=self.defn, edx=self.seq, stack_args=(self.ev,))
        r32 = lambda a: struct.unpack('<I', cpu.read(a, 4))[0]  # noqa: E731
        out = [cpu.uc.reg_read(UC_X86_REG_EAX), r32(LEFT),
               r32(TINT + 0x38) & 0xffff, r32(TINT + 0x40), r32(TINT + 0x44), r32(TINT + 0x0c), r32(TINT + 0x10)]
        self.quads = []
        cpu.call(0x5a5360, ecx=TINT)
        out.append(len(self.quads))
        for q in self.quads:
            out += q
        kept = r32(TINT + 0x34)
        out.append(0 if kept == 0 else 1 if kept == self.rects else 2)
        return out


def main():
    probe = load_probe()
    probe.probe_tint_run.argtypes = [ctypes.c_char_p, U, U, U, U, ctypes.c_int, ctypes.POINTER(ctypes.c_int32),
                                     ctypes.POINTER(U)]
    emu = Emulated()
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    r = random.Random(36)
    seen = {'running': 0, 'done': 0, 'clamped': 0, 'quads': 0, 'no rect': 0}
    for i in range(runs):
        chans = [channel(r) for _ in range(4)]
        dur = r.choice([r.uniform(0.05, 5), 1.0, 0.25])
        now = r.choice([r.uniform(0, dur), dur, r.uniform(dur, dur + 2), 0.0])
        ev = bytearray(0x40)
        ev[0], ev[1] = 36, r.choice([1, 2, 3])
        for k, (a, b, c) in enumerate(chans):
            struct.pack_into('<3f', ev, 0xc + 0xc * k, a, b, c)
        struct.pack_into('<f', ev, 0x3c, dur)
        flags = r.choice([0x10, 0x10, 0x00, 0x01, 0x11, 0x1c, 0x02])
        timer = fb(r.choice([0.0, 0.5, -1.0]))
        rects = r.randrange(4)
        r8 = [r.randrange(-50, 700) for _ in range(8)]
        args = (bytes(ev), fb(now), fb(r.uniform(0, 0.1)), flags, timer, rects)
        want = emu.run(*args, r8)
        got_buf = (U * 64)()
        probe.probe_tint_run(*args, (ctypes.c_int32 * 8)(*r8), got_buf)
        got = list(got_buf[:len(want)])
        if want != got:
            names = ['result', 'left', 'colour', 'alpha lo', 'alpha hi', 'flags', 'timer', 'quads']
            k = next(j for j in range(len(want)) if want[j] != got[j])
            print(f'run {i}: differs at {names[k] if k < len(names) else k}: original {want}, port {got}')
            print(f'  event {[round(x, 6) for c in chans for x in c]} dur {dur} now {now} flags {flags:#x} rects {rects}')
            raise SystemExit(1)
        seen['running' if want[0] == 1 else 'done'] += 1
        a = struct.unpack('<d', struct.pack('<II', want[3], want[4]))[0]
        seen['clamped'] += a in (0.0, 1.0)
        seen['quads'] += want[7]
        seen['no rect'] += want[7] == 0
    print(f"screen tint: {runs} events identical ({seen['running']} running, {seen['done']} done, "
          f"{seen['clamped']} alphas clamped), {seen['quads']} quads drawn, {seen['no rect']} draws without a rect")


if __name__ == '__main__':
    main()

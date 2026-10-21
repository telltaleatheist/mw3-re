"""The model texture cycle against the port (docs/cockpit-damage.md): the
interp's CycleTexture* wrappers 0x0053bc30 (on: 0x005375d0), 0x0053bd10 (map:
0x00537680), 0x0053bcd0 (looping: 0x005377a0), 0x0053bcf0 (speed:
0x005377e0), the frame step 0x005376c0 the draw runs, the reset 0x0053bc10
and the frame set 0x0053bc90, in random sequences on one model, the x87 at
24 bits. After each call the return value and the material and its cycle
(flags, texture, looping, frame, index, speed, count, added, slots) must
match. A null model and the default material are among the cases.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import FTOL, IAT_FTOL, Original  # noqa: E402

DEFAULT_MAT, DEFAULT_ENTRY, FRAME, DT = 0x63fb98, 0x5ecbf8, 0x72f338, 0x72c52c
REALLOC, ZERR = 0x5bb610, 0x49bfd0

probe = load_probe()
probe.cy_reset.argtypes = [ctypes.c_int32] * 4
probe.cy_on.argtypes = [ctypes.c_int32]
probe.cy_map.argtypes = [ctypes.c_int32]
probe.cy_loop.argtypes = [ctypes.c_uint32]
probe.cy_speed.argtypes = [ctypes.c_float]
probe.cy_step.argtypes = [ctypes.c_uint32, ctypes.c_float]
probe.cy_frame.argtypes = [ctypes.c_int32]
probe.cy_state.argtypes = [ctypes.c_void_p]


def bits(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Emu:
    def __init__(self):
        o = self.o = Original()
        o.import_code(IAT_FTOL, FTOL)
        o.fpu_control(0x007f)
        self.sizes = {}

        def realloc(c):
            old, size = c.arg(0), c.arg(1)
            new = o.alloc(max(size, 4))
            if old:
                o.write(new, o.read(old, min(self.sizes.get(old, 0), size)))
            self.sizes[new] = size
            return new
        o.stub_import(REALLOC, realloc)
        o.stub_function(ZERR, lambda c: 0)
        # 0x0053bc30's null-model message: sprintf, fprintf on _iob[2].
        o.stub_import(0x5bb658, lambda c: 0)
        o.stub_import(0x5bb62c, lambda c: 0)
        o.u32(0x5bb5e8, o.alloc(0x60))

    def reset(self, n, flags, use_default, null_model):
        o = self.o
        self.tex = o.alloc(0x28 * max(n, 1))
        self.mat = DEFAULT_MAT if use_default else o.alloc(0x2c)
        if not use_default:
            o.write(self.mat, bytes(0x2c))
            o.write(self.mat + 1, bytes([flags]))
            o.u32(self.mat + 0x10, self.tex if n else DEFAULT_ENTRY)
        poly = o.alloc(0x24)
        o.write(poly, bytes(0x24))
        o.u32(poly + 0x1c, self.mat)
        self.model = 0 if null_model else o.alloc(0x60)
        if self.model:
            o.write(self.model, bytes(0x60))
            o.u32(self.model + 0x34, poly)
        self.n = n

    def ret(self):
        from unicorn.x86_const import UC_X86_REG_EAX
        return s32(self.o.uc.reg_read(UC_X86_REG_EAX))

    def on(self, count):
        self.o.call(0x53bc30, ecx=self.model, edx=count & 0xffffffff)
        return self.ret()

    def map(self, k):
        self.o.call(0x53bd10, ecx=self.model, edx=DEFAULT_ENTRY if k < 0 else self.tex + 0x28 * k)
        return self.ret()

    def loop(self, v):
        self.o.call(0x53bcd0, ecx=self.model, edx=v)
        return self.ret()

    def speed(self, v):
        self.o.call(0x53bcf0, ecx=self.model, stack_args=(bits(v),))
        return self.ret()

    def step(self, frame, dt):
        self.o.u32(FRAME, frame)
        self.o.u32(DT, bits(dt))
        self.o.call(0x5376c0, ecx=self.mat)
        return 0

    def frame(self, n):
        self.o.call(0x53bc90, ecx=self.model, edx=n & 0xffffffff)
        return 0

    def restart(self):
        self.o.call(0x53bc10, ecx=self.model)
        return 0

    def u(self, a):
        return struct.unpack('<I', self.o.read(a, 4))[0]

    def tex_index(self, p):
        if p == 0:
            return -3
        if p == DEFAULT_ENTRY:
            return -1
        if self.tex <= p < self.tex + 0x28 * self.n and (p - self.tex) % 0x28 == 0:
            return (p - self.tex) // 0x28
        return -2

    def state(self):
        m = self.mat
        out = [self.o.read(m + 1, 1)[0], self.tex_index(self.u(m + 0x10))]
        c = self.u(m + 0x24)
        out.append(int(c != 0))
        if not c:
            return out
        count = self.u(c + 0x10)
        out += [s32(self.u(c)), s32(self.u(c + 4)), s32(self.u(c + 8)), s32(self.u(c + 0xc)), s32(count),
                s32(self.u(c + 0x14))]
        slots = self.u(c + 0x18)
        out += [self.tex_index(self.u(slots + 4 * i)) for i in range(count)]
        return out


class Port:
    def __init__(self):
        self.buf = (ctypes.c_int32 * 4096)()

    def reset(self, n, flags, use_default, null_model):
        probe.cy_reset(n, flags, use_default, null_model)

    def on(self, count):
        return probe.cy_on(count)

    def map(self, k):
        return probe.cy_map(k)

    def loop(self, v):
        return probe.cy_loop(v)

    def speed(self, v):
        return probe.cy_speed(v)

    def step(self, frame, dt):
        probe.cy_step(frame, dt)
        return 0

    def frame(self, n):
        probe.cy_frame(n)
        return 0

    def restart(self):
        probe.cy_restart()
        return 0

    def state(self):
        k = probe.cy_state(self.buf)
        return list(self.buf[:k])


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def main():
    n_cases = int(os.environ.get('CYCLE_CASES', '400'))
    rng = random.Random(0x53bc30)
    emu, port = Emu(), Port()
    failed = 0
    seen = {'cycled': 0, 'wrapped': 0, 'clamped': 0, 'null': 0, 'default': 0, 'steps': 0}
    for case in range(n_cases):
        n = rng.randrange(0, 20)
        use_default = rng.random() < 0.05
        null_model = rng.random() < 0.05
        flags = rng.choice((0x11, 0x11, 0x10, 0x01))  # flag 4 without a cycle crashes the map (no file has it)
        seen['null'] += null_model
        seen['default'] += use_default
        for s in (emu, port):
            s.reset(n, flags, use_default, null_model)
        frame = rng.randrange(1, 1000)
        log = []
        for step in range(rng.randrange(1, 40)):
            k = rng.random()
            if k < 0.12:
                op = ('on', rng.choice((0, 1, rng.randrange(1, 30), rng.randrange(1, 30))))
            elif k < 0.4:
                if not n:
                    continue
                op = ('map', rng.randrange(-1, n))
            elif k < 0.47:
                op = ('loop', rng.choice((0, 1, 1, 7)))
            elif k < 0.55:
                op = ('speed', f32(rng.choice((0.0, 1.0, 3.0, 15.0, -2.0, rng.uniform(-20, 40)))))
            elif k < 0.9:
                if null_model or (not use_default and not (rng.random() < 0.97)):
                    continue
                frame += rng.choice((0, 1, 1, 1, 2))
                op = ('step', frame, f32(rng.choice((1 / 30, 1 / 60, 0.05, 0.25, 1.0))))
            elif k < 0.95:
                op = ('frame', rng.randrange(-5, 40))
            else:
                op = ('restart',)
            # The exe's 0x0053bc90 / 0x0053bc10 / 0x005376c0 need a material
            # with a cycle (and a model); a cycle's absence crashes the first two.
            if op[0] in ('frame', 'restart') and (null_model or emu.state()[2] == 0):
                continue
            if op[0] == 'step' and emu.state()[2] == 0:
                continue
            # A cycle of 0 slots divides by zero in the step and the frame set
            # (the exe's crash; no shipped script makes one).
            if op[0] in ('step', 'frame') and len(emu.state()) > 7 and emu.state()[7] == 0:
                continue
            if use_default and op[0] != 'on':
                continue
            want = getattr(emu, op[0])(*op[1:])
            got = getattr(port, op[0])(*op[1:])
            log.append(op)
            if op[0] == 'step':
                seen['steps'] += 1
            ws, gs = emu.state(), port.state()
            if use_default:
                ws, gs = ws[2:3], gs[2:3]  # the default material: never cycled
            if want != got or ws != gs:
                failed += 1
                if failed <= 5:
                    print(f'FAIL case {case} after {op}: ret {want} vs {got}\n  want {ws}\n  got  {gs}\n  ops {log}')
                break
        st = emu.state()
        if len(st) > 3:
            seen['cycled'] += 1
            index, count = struct.unpack('<f', struct.pack('<i', st[5]))[0], st[7]
            seen['wrapped'] += st[3] != 0 and index >= count
            seen['clamped'] += st[3] == 0 and count and index == count - 1
    print(f'ok: {n_cases} sequences identical ({seen["cycled"]} cycled, {seen["steps"]} steps, '
          f'{seen["wrapped"]} looped past the end, {seen["clamped"]} held at the last, {seen["null"]} null models, '
          f'{seen["default"]} on the default material)' if not failed else f'{failed} of {n_cases} differ')
    sys.stdout.flush()
    os._exit(1 if failed else 0)


main()

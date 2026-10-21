"""The controls' smoothing (MechControls::Update 0x426960, SetThrottle 0x426b60)
against the original x86 (docs/movement.md, Throttle and turn).

    cmake --build build && .venv/bin/python tests/differential/test_controls.py

A controls object and its owner mech in emulator memory, laid out as the
original's (the virtual base at +0x60, the owner at +0x70). Random rates,
targets and frame steps over many frames, with the throttle commanded through
SetThrottle as the player and AI code do; after each call the whole object
must match bit for bit at the game's 24-bit x87 precision.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_controls_update.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_float]
probe.probe_controls_set_throttle.argtypes = [ctypes.c_char_p, ctypes.c_float]
probe.probe_controls_halt.argtypes = [ctypes.c_char_p]

UPDATE, SET_THROTTLE, STOP = 0x426960, 0x426b60, 0x426c10
ICONTROLS_VTABLE = 0x5bd428  # MechControls' IControls vtable, at +0x60
CTL_SIZE, MECH_SIZE = 0x74, 0x6d8
# The fields the two functions touch; the rest of the image must stay as it was.
FLOATS = [0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x44, 0x48, 0x4c]


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        self.ctl, self.vbt, self.mech = cpu.alloc(CTL_SIZE), cpu.alloc(8), cpu.alloc(MECH_SIZE)

    def load(self, ctl, mech):
        cpu = self.cpu
        cpu.write(self.ctl, ctl)
        cpu.u32(self.ctl + 4, self.vbt)
        cpu.u32(self.vbt + 4, 0x5c)   # vbtable: the virtual base is at +0x60
        cpu.u32(self.ctl + 0x60, ICONTROLS_VTABLE)  # Stop calls the setters through it
        cpu.u32(self.ctl + 0x70, self.mech)
        cpu.write(self.mech, mech)

    def image(self):
        b = bytearray(self.cpu.read(self.ctl, CTL_SIZE))
        b[4:8] = bytes(4)
        b[0x60:0x64] = bytes(4)
        b[0x70:0x74] = bytes(4)
        return bytes(b)

    def update(self, dt):
        self.cpu.call(UPDATE, ecx=self.ctl + 0x60, stack_args=(f2u(dt),))
        return self.cpu.uc.reg_read(UC_X86_REG_EAX)

    def stop(self):
        self.cpu.call(STOP, ecx=self.ctl + 0x60, stack_args=(1,))

    def set_throttle(self, v):
        self.cpu.call(SET_THROTTLE, ecx=self.ctl + 0x60, stack_args=(f2u(v),))


def random_value(rng):
    return f32(rng.choice([0.0, 0.0, 1.0, -1.0, rng.uniform(-1.2, 1.2), rng.uniform(-0.02, 0.02),
                           rng.uniform(-3, 3)]))


def main():
    emu = Emulated()
    rng = random.Random(3025)
    runs, frames, calls = 400, 120, 0
    snapped = moved = stops = 0
    for run in range(runs):
        ctl = bytearray(rng.randbytes(CTL_SIZE))   # untouched fields keep their bytes
        for off in FLOATS:
            ctl[off:off + 4] = struct.pack('<f', random_value(rng))
        ctl[0x08:0x0c] = struct.pack('<I', rng.choice([0, 4, 0xff, 0xfb]))
        ctl[0x50] = rng.randrange(2)
        mech = bytearray(MECH_SIZE)
        mech[0xde] = rng.random() < 0.08
        for off in (0x530, 0x534, 0x538):
            mech[off:off + 4] = struct.pack('<f', f32(rng.choice([0.5, 1.0, 1.5, 2.0, 3.0, 4.5,
                                                                  rng.uniform(0, 10)])))
        dt = f32(rng.choice([1 / 30, 1 / 60, 1 / 85, 0.1, rng.uniform(0.001, 0.25)]))
        ctl[4:8] = ctl[0x5c:0x60] = ctl[0x60:0x64] = ctl[0x70:0x74] = bytes(4)  # +0x5c: the thunks' vtordisp, 0
        image = bytes(ctl)
        emu.load(image, bytes(mech))
        for frame in range(frames):
            if rng.random() < 0.15:
                v = random_value(rng)
                emu.set_throttle(v)
                port = ctypes.create_string_buffer(image, CTL_SIZE)
                probe.probe_controls_set_throttle(port, v)
                want = emu.image()
                if want != port.raw:
                    raise SystemExit(f'run {run} frame {frame}: SetThrottle({v}) differs')
                image = want
                calls += 1
            if rng.random() < 0.03:  # Stop, as Mech::Power powering down or up
                emu.stop()
                port = ctypes.create_string_buffer(image, CTL_SIZE)
                probe.probe_controls_halt(port)
                want = emu.image()
                if want != port.raw:
                    diffs = [hex(o) for o in range(0, CTL_SIZE, 4) if want[o:o + 4] != port.raw[o:o + 4]]
                    raise SystemExit(f'run {run} frame {frame}: Stop differs at {diffs}')
                image = want
                calls += 1
                stops += 1
            if rng.random() < 0.1:
                for off in (0x48, 0x4c):
                    v = random_value(rng)
                    emu.cpu.write(emu.ctl + off, struct.pack('<f', v))
                image = emu.image()
            before = image
            r = emu.update(dt)
            port = ctypes.create_string_buffer(image, CTL_SIZE)
            pr = probe.probe_controls_update(port, bytes(mech), dt)
            want = emu.image()
            if want != port.raw or r != pr:
                diffs = [hex(o) for o in range(0, CTL_SIZE, 4) if want[o:o + 4] != port.raw[o:o + 4]]
                raise SystemExit(f'run {run} frame {frame}: update differs at {diffs} (result {r} vs {pr})')
            for off in (0x10, 0x14, 0x20):
                t = {0x10: 0x44, 0x14: 0x48, 0x20: 0x4c}[off]
                if before[off:off + 4] != want[off:off + 4]:
                    moved += 1
                    snapped += want[off:off + 4] == want[t:t + 4]
            image = want
            calls += 1
    if snapped < 100 or moved - snapped < 1000:
        raise SystemExit(f'coverage: {moved} moves, {snapped} snaps')
    if stops < 100:
        raise SystemExit(f'coverage: {stops} stops')
    print(f'ok: {runs} runs x {frames} frames, {calls} calls identical bit for bit '
          f'({moved} axis moves, {snapped} snapped to the target, {stops} stops)')


if __name__ == '__main__':
    main()

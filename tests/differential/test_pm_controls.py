"""PM's controls' smoothing (MechControls::Update 0x4285d0, SetThrottle 0x4287f0)
against PM's x86 (docs/movement.md, Throttle and turn). PM calls the zeroing
through vt+0x18 (0x428910) and its IControls is larger: the owner at +0x7c.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_controls.py

A controls object and its owner mech in emulator memory, laid out as the
original's (the virtual base at +0x60, the owner at +0x7c). Random rates,
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
from original import FTOL, IAT_FTOL  # noqa: E402
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

probe = test_pm_zar.load_pm_probe()
probe.probe_controls_update.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_float]
probe.probe_controls_set_throttle.argtypes = [ctypes.c_char_p, ctypes.c_float]
probe.probe_controls_halt.argtypes = [ctypes.c_char_p]

UPDATE, SET_THROTTLE, STOP, ZERO = 0x4285d0, 0x4287f0, 0x4288a0, 0x428910
CTL_SIZE, MECH_SIZE = 0x80, 0x748
# The fields the two functions touch; the rest of the image must stay as it was.
FLOATS = [0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x44, 0x48, 0x4c]


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


class Emulated:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        cpu = self.cpu = z.cpu
        cpu.import_code(IAT_FTOL, FTOL)
        self.iface = cpu.tr(0x5bd428, 4)  # MechControls' IControls vtable, at +0x60
        self.ctl, self.vbt, self.mech, self.vt = z.alloc(CTL_SIZE), z.alloc(8), z.alloc(MECH_SIZE), z.alloc(0x40)
        self.wr(self.vt + 0x18, struct.pack('<I', ZERO))

    def wr(self, a, b):
        self.cpu._uc.mem_write(a, bytes(b))

    def load(self, ctl, mech):
        self.wr(self.ctl, ctl)
        self.wr(self.ctl, struct.pack('<I', self.vt))
        self.wr(self.ctl + 4, struct.pack('<I', self.vbt))
        self.wr(self.vbt + 4, struct.pack('<I', 0x5c))   # vbtable: the virtual base is at +0x60
        self.wr(self.ctl + 0x60, struct.pack('<I', self.iface))  # Stop calls the setters through it
        self.wr(self.ctl + 0x7c, struct.pack('<I', self.mech))
        self.wr(self.mech, mech)

    def image(self):
        b = bytearray(self.cpu._uc.mem_read(self.ctl, CTL_SIZE))
        b[0:8] = bytes(8)
        b[0x60:0x64] = bytes(4)
        b[0x7c:0x80] = bytes(4)
        return bytes(b)

    def update(self, dt):
        return self.z.call(UPDATE, ecx=self.ctl + 0x60, args=(f2u(dt),))

    def stop(self):
        self.z.call(STOP, ecx=self.ctl + 0x60, args=(1,))

    def set_throttle(self, v):
        self.z.call(SET_THROTTLE, ecx=self.ctl + 0x60, args=(f2u(v),))


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
        mech[0xe2] = rng.random() < 0.08
        for off in (0x544, 0x548, 0x54c):
            mech[off:off + 4] = struct.pack('<f', f32(rng.choice([0.5, 1.0, 1.5, 2.0, 3.0, 4.5,
                                                                  rng.uniform(0, 10)])))
        dt = f32(rng.choice([1 / 30, 1 / 60, 1 / 85, 0.1, rng.uniform(0.001, 0.25)]))
        ctl[0:8] = bytes(8)
        ctl[0x5c:0x60] = ctl[0x60:0x64] = ctl[0x7c:0x80] = bytes(4)  # +0x5c: the thunks' vtordisp, 0
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
                    emu.wr(emu.ctl + off, struct.pack('<f', v))
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
    pm_floors.floors(calls=(calls, 50000), moves=(moved, 1000), stops=(stops, 100))
    print(f'ok: {runs} runs x {frames} frames, {calls} calls identical bit for bit '
          f'({moved} axis moves, {snapped} snapped to the target, {stops} stops)')


if __name__ == '__main__':
    main()

"""The ported frame timer against the original x87 code, frame by frame.

    cmake --build build && .venv/bin/python tests/differential/test_ztimer.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libmw3probe.*'))))
GET_TICK_COUNT = 0x00575d30  # the import thunk 0x00575a00 calls
f32 = lambda cpu, a: struct.unpack('<f', cpu.read(a, 4))[0]  # noqa: E731


def original(ticks, scale, scale_frame):
    cpu = Original()
    now = {'t': 0}

    def fake_tick_count(uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
        uc.reg_write(UC_X86_REG_EAX, now['t'])
        uc.reg_write(UC_X86_REG_ESP, esp + 4)
        uc.reg_write(UC_X86_REG_EIP, ret)

    cpu.uc.hook_add(UC_HOOK_CODE, fake_tick_count, begin=GET_TICK_COUNT, end=GET_TICK_COUNT)
    dts, raws = [], []
    for i, t in enumerate(ticks):
        if i == scale_frame:
            cpu.write(0x005f0970, struct.pack('<f', scale))
        now['t'] = t
        cpu.call(0x00575a00)
        dts.append(f32(cpu, 0x0072c52c))
        raws.append(f32(cpu, 0x0072c534))
    return dts, raws


def rebuilt(ticks, scale, scale_frame):
    n = len(ticks)
    t = (ctypes.c_uint32 * n)(*ticks)
    dt, raw = (ctypes.c_float * n)(), (ctypes.c_float * n)()
    probe.probe_ztimer(t, n, ctypes.c_float(scale), scale_frame, dt, raw)
    return list(dt), list(raw)


def main():
    rng = random.Random(61)
    frames = 0
    for run in range(40):
        t, ticks = rng.randint(0, 50_000), []
        for _ in range(60):
            t += rng.choice([rng.randint(0, 40), rng.randint(0, 400), 0])
            ticks.append(t)
        scale, scale_frame = rng.choice([0.5, 0.25, 2.0, 1.0]), rng.randint(0, 59)
        a, b = original(ticks, scale, scale_frame), rebuilt(ticks, scale, scale_frame)
        if a != b:
            raise SystemExit(f'ztimer differs on run {run}:\n{a}\n{b}')
        frames += len(ticks)
    print(f'{frames} frames: the rebuilt timer matches the original x87 code bit for bit')


if __name__ == '__main__':
    main()

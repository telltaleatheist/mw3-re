"""The vehicles' movement helpers against the original x86 (docs/vehicles.md,
The movement): the turn to a heading 0x4a28c0, the up vector 0x4a2890, the
speed 0x4a2b10 with the slide 0x4a2dd0 and the side force 0x4a2c90, and the
turn rate's steps vt+0x14 (the base's 0x4a2a00 and the AI's 0x4a2ae0).

    cmake --build build && .venv/bin/python tests/differential/test_veh_move.py [runs]

Each run: a vehicle with the AI controls' vtables (their IControls getters
and setters are the real thunks), a random matrix and forward, throttle and
turn (0 often, NaN now and then), speeds, a slide or none, a heading to turn
to or none, the motion's rates, turns, dampings and frictions, the frame
time ([0x5e8110], 1/dt at [0x5e8114]; 0 now and then). The x87 at 24 bits.
Compared: the controls' turn rate, local velocity, throttle, turn, yaw, up,
slide and heading flags (floats within 4 ulps or 1e-6), the side force's
answer, the refresh call.
"""
import ctypes
import math
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Emu, Replay, Trace, call_abs, compare, load_probe, mov_ecx, push_imm, rnd  # noqa: E402

AI_VT, AI_ICTL_VT = 0x5cd880, 0x5cd778
DT, INV_DT = 0x5e8110, 0x5e8114
NAMES = ['turn rate', 'x70.x', 'x70.y', 'x70.z', 'throttle', 'turn', 'yaw', 'up.x', 'up.y', 'up.z']
CALLS = [(0x4a28c0, 'v'), (0x4a2890, 'v'), (0x4a2b10, 'v'), (0x4a2c90, 'v'), (0x4a2a00, 'ctl'), (0x4a2ae0, 'ctl')]


def fb(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def bf(b):
    return struct.unpack('<f', struct.pack('<I', b & 0xffffffff))[0]


def same(a, b):
    if a == b or (math.isnan(a) and math.isnan(b)):
        return True
    if math.isnan(a) or math.isnan(b) or math.isinf(a) or math.isinf(b):
        return False
    return abs(a - b) <= max(1e-6, 4 * abs(a) * 2 ** -23)


def value(r, lo, hi, zero=0.2):
    c = r.random()
    if c < zero:
        return 0.0
    if c < zero + 0.02:
        return float('nan')
    return r.uniform(lo, hi)


def run(probe, seed):
    r = rnd(seed, 'move')
    which = r.randrange(len(CALLS))
    addr, on = CALLS[which]
    t = Trace()
    e = Emu(t)
    e.heap_stubs()
    e.cpu.fpu_control(0x007f)
    dt = r.choice([0.033, 0.016, 0.1, r.uniform(0, 0.2), 0.0])
    e.w32(DT, fb(dt))
    e.w32(INV_DT, fb(1.0 / dt if dt else 10000.0))
    v, ctl, mat = e.world_alloc(0x100), e.world_alloc(0xd00), e.world_alloc(0x30)
    e.cpu.write(v, b'\0' * 0x100)
    e.cpu.write(ctl, b'\0' * 0xd00)
    e.w32(v + 0xc, mat)
    e.w32(v + 0xe4, ctl)
    e.w32(ctl, AI_VT)
    vb = e.world_alloc(0x10)
    e.w32(ctl + 4, vb)
    e.w32(vb + 4, 0xcc0)
    e.w32(ctl + 0xcc4, AI_ICTL_VT)
    a = r.uniform(-math.pi, math.pi)
    forward = [-math.sin(a), -math.cos(a)]
    m = [r.uniform(-1, 1) for _ in range(9)] + [r.uniform(-500, 500) for _ in range(3)]
    h = r.uniform(-math.pi, math.pi)
    near = r.random() < 0.3  # a heading within a frame's turn
    if near:
        h = a + r.uniform(-0.02, 0.02)
    f = forward + m + [value(r, -1, 1, 0.3), value(r, -1, 1, 0.3), -math.sin(h), -math.cos(h), value(r, -3, 3)]
    f += [r.uniform(-40, 40) for _ in range(3)] + [value(r, -5, 5, 0.3), 0.0, value(r, -40, 40)]
    f += [r.uniform(-1, 1) for _ in range(3)]
    f += [r.choice([10.0, 30.0, 65.0]), r.choice([0.6, 2.0, 3.5]), r.uniform(-math.pi, math.pi)]
    f += [r.choice([10.0, 40.0]), r.choice([0.6, 2.5]), r.choice([2.0, 3.5]), r.choice([30.0, 20.0])]
    f += [r.choice([30.0, 20.0]), r.choice([30.0, 20.0]), r.choice([10000.0, 5.0, 40.0]), r.choice([10.0, 2.0])]
    f = [bf(fb(x)) for x in f]
    u = [int(r.random() < 0.3), int(r.random() < 0.6)]
    put = lambda off, x: e.w32(off, fb(x))  # noqa: E731
    put(v + 0x24, f[0])
    put(v + 0x2c, f[1])
    for i in range(12):
        put(mat + 4 * i, f[2 + i])
    k = 14
    for off in (8, 0xc, 0x748, 0x750, 0x54, 0x64, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x314, 0x318, 0x31c, 0x88, 0x8c, 0x330):
        put(ctl + off, f[k])
        k += 1
    mb = ctl + 0x764
    for off in (4, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x58, 0x5c):
        put(mb + off, f[k])
        k += 1
    e.w32(ctl + 0x34, u[0])
    e.w32(ctl + 0x760, u[1])
    e.stub(0x5a85c0, spec='c')
    probe.probe_vm_setup(v, ctl, (ctypes.c_float * len(f))(*f), (ctypes.c_uint32 * 2)(*u))
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    want_f = None
    if addr == 0x4a2c90:  # a float answer: through a fresh thunk
        slot, cw, here = e.alloc(16), e.alloc(4), e.alloc(64)
        e.cpu.write(cw, struct.pack('<H', 0x007f))
        code = b'\xdb\xe3\xd9\x2d' + struct.pack('<I', cw) + mov_ecx(v)
        code += call_abs(here + len(code), addr) + b'\xd9\x1d' + struct.pack('<I', slot) + b'\xc3'
        e.cpu.write(here, code)
        e.call(here)
        want_f = bf(e.r32(slot))
    elif addr == 0x4a2a00:
        e.call(addr, ecx=ctl, stack=(mb,))
    else:
        e.call(addr, ecx=v if on == 'v' else ctl)
    got_f = probe.probe_vm_call(which)
    name = f'move seed {seed} ({addr:#x}, dt {dt:.3f})'
    if want_f is not None and not same(want_f, got_f):
        print(f'{name}: answer original {want_f} rebuild {got_f}')
        return False
    want = [bf(e.r32(ctl + o)) for o in (0x54, 0x70, 0x74, 0x78, 8, 0xc, 0x330, 0x308, 0x30c, 0x310)]
    got_a, got_u = (ctypes.c_float * 10)(), (ctypes.c_uint32 * 2)()
    probe.probe_vm_image(got_a, got_u)
    for i, (x, y) in enumerate(zip(want, got_a)):
        if not same(x, y):
            print(f'{name}: {NAMES[i]} original {x} rebuild {y}')
            return False
    wu = [e.r32(ctl + 0x34), e.r32(ctl + 0x760)]
    if wu != list(got_u):
        print(f'{name}: sliding/turning original {wu} rebuild {list(got_u)}')
        return False
    return compare(name, t.log, rep.log)


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_vm_setup.argtypes = [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_vm_call.argtypes = [ctypes.c_int]
    probe.probe_vm_call.restype = ctypes.c_float
    probe.probe_vm_image.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    probe.probe_vb_hooks(1)
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 1200
    for seed in range(runs):
        if not run(probe, seed):
            sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} movement steps as the original')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()

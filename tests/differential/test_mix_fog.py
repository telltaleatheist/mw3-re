"""The lighting's fog mix to the device and a mech's TARGET tint against the
original x86 (docs/render-lighting.md, the fog mix; docs/vehicles.md, TARGET's
tint). The x87 at 24 bits, as in play.

    cmake --build build && .venv/bin/python tests/differential/test_mix_fog.py [cases]

The mix 0x5680c0 (with 0x579950 and the ramp 0x56c170, run as they are):
the mix colour before (random, some within 0.01 of the new one), the device
up or not, the new colour (in range, beyond, negative, NaN), the software
format's masks and shifts; compared: the colour clamped in place, the mix
record (its rgb, the ramp's r, g, b, packed, doubled and 32 steps) and the
device's copy [0x802798]. The send 0x579980: the sent colour and the mix
the same or not in each channel; compared: the sent colour and whether the
device's call ([0x72f3f0]) ran. The tint 0x41b9d0 (the parts' own vt+0x54
through their real vtables: the thighs' 0x463aa0, the arms' and head's
0x462040, the torso's 0x4643d0) on a mech active or not, its parts' nodes
present or not (an object3d, or another class), internals random, on or
off; 0x4fb8c0 / 0x4fb840 run as they are on the nodes; compared: every
node's data.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, compare, load_probe  # noqa: E402

F = ctypes.c_float
U32 = ctypes.c_uint32
I32 = ctypes.c_int32
MIX, ACTIVE, DEVMIX, SENT, SEND = 0x800688, 0x72f348, 0x802798, 0x72f400, 0x72f3f0
FMT = (0x74a740, 0x74a744, 0x74a74c, 0x74a750, 0x74a754)
VT = {'thigh': 0x5c2978, 'arm': 0x5c2a48, 'head': 0x5c2b80, 'torso': 0x5c29e0, 'hip': 0x5c28a8, 'foot': 0x5c2910}
PARTS = ['thigh', 'thigh', 'arm', 'arm', 'head', 'torso', 'hip', 'foot', 'foot']
OFF = [0x38c, 0x390, 0x394, 0x398, 0x3a4, 0x39c, 0x3a0]
COUNT = {'mixes': 0, 'kept (within 0.01)': 0, 'sends': 0, 'sent': 0, 'tints': 0, 'nodes tinted': 0}


def die():
    sys.stdout.flush()
    os._exit(1)


def f2u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def setup_probe(p):
    p.probe_mf_set.argtypes = [ctypes.POINTER(F), ctypes.POINTER(F), ctypes.POINTER(U32), ctypes.POINTER(U32)]
    p.probe_mf_send.argtypes = [ctypes.POINTER(F), ctypes.POINTER(F)]
    p.probe_mf_mech.argtypes = [I32, I32, I32, ctypes.POINTER(I32), ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(I32),
                                ctypes.POINTER(F)]


def emu_new():
    t = Trace()
    e = Emu(t)
    e.cpu.fpu_control(0x007f)
    e.stub(0x49bfd0, '@zerr', '')
    return e


def colour(r):
    return r.choice([r.uniform(0, 1), r.uniform(-1, 2), 0.0, 1.0, float('nan'), r.uniform(0.99, 1.0)])


def bits(xs):
    return [f2u(x) for x in xs]


def run_mix(probe, e, r, seed):
    old = [r.uniform(0, 1) for _ in range(3)]
    new = [colour(r) for _ in range(3)]
    if r.random() < 0.3:
        new = [o + r.uniform(-0.012, 0.012) for o in old]
    active = r.random() < 0.7
    devmix = [r.uniform(0, 255) for _ in range(3)]
    ramp_u = [r.getrandbits(32) for _ in range(5 + 32)]
    ramp_u[3] &= 0xffff
    fmt = [r.choice([0xf800, 0x7c00, 0xff0000]), r.choice([0x7e0, 0x3e0, 0xff00]), r.choice([8, 7, 16]),
           r.choice([3, 2, 8]), r.choice([3, 0])]
    # The original's.
    e.cpu.write(MIX, struct.pack('<3f3IHxxI32i', *old, ramp_u[0], ramp_u[1], ramp_u[2], ramp_u[3], ramp_u[4],
                                 *[struct.unpack('<i', struct.pack('<I', v))[0] for v in ramp_u[5:]]))
    e.w32(ACTIVE, 1 if active else 0)
    e.cpu.write(DEVMIX, struct.pack('<3f', *devmix))
    for a, v in zip(FMT, fmt):
        e.w32(a, v)
    p = e.world_alloc(12)
    e.cpu.write(p, struct.pack('<3f', *new))
    e.call(0x5680c0, ecx=p)
    rgb_a = list(struct.unpack('<3I', bytes(e.cpu.read(p, 12))))
    raw = bytes(e.cpu.read(MIX, 0xa0))
    mix_a = list(struct.unpack('<3I3I', raw[:0x18])) + [struct.unpack_from('<H', raw, 0x18)[0]] + \
        [struct.unpack_from('<I', raw, 0x1c)[0]] + list(struct.unpack_from('<32I', raw, 0x20))
    dev_a = list(struct.unpack('<3I', bytes(e.cpu.read(DEVMIX, 12))))
    # The port's.
    rgb = (F * 3)(*new)
    fs = (F * 10)(*old, 1.0 if active else 0.0, *devmix, 0, 0, 0)
    us = (U32 * 37)(*ramp_u)
    fm = (U32 * 5)(*fmt)
    probe.probe_mf_set(rgb, fs, us, fm)
    rgb_b = bits(rgb)
    mix_b = bits(fs[0:3]) + [us[0], us[1], us[2], us[3] & 0xffff, us[4]] + [us[5 + i] for i in range(32)]
    dev_b = bits(fs[4:7])
    if rgb_a != rgb_b or mix_a != mix_b or dev_a != dev_b:
        what = 'rgb' if rgb_a != rgb_b else 'mix record' if mix_a != mix_b else 'device copy'
        k = next(i for i, (x, y) in enumerate(zip(*{'rgb': (rgb_a, rgb_b), 'mix record': (mix_a, mix_b),
                                                    'device copy': (dev_a, dev_b)}[what])) if x != y)
        print(f'mix seed {seed} (old {old}, new {new}, active {active}): {what} differs at {k}')
        die()
    COUNT['mixes'] += 1
    COUNT['kept (within 0.01)'] += mix_a[:3] == bits(old)


def run_send(probe, e, r, seed):
    sent = [r.uniform(0, 255) for _ in range(3)]
    mix = [s if r.random() < 0.6 else r.uniform(0, 255) for s in sent]
    if r.random() < 0.05:
        mix[r.randrange(3)] = float('nan')
    e.cpu.write(SENT, struct.pack('<3f', *sent))
    e.cpu.write(DEVMIX, struct.pack('<3f', *mix))
    e.w32(SEND, e.new_stub('send', '', 0))
    e.t.reset()
    e.call(0x579980)
    a = list(struct.unpack('<3I', bytes(e.cpu.read(SENT, 12))))
    t = Trace()
    probe.probe_app_set_callback(t.callback())
    s = (F * 3)(*sent)
    probe.probe_mf_send(s, (F * 3)(*mix))
    if a != bits(s) or not compare(f'send seed {seed}', e.t.log, t.log):
        print(f'send seed {seed}: sent {a} / {bits(s)}')
        die()
    COUNT['sends'] += 1
    COUNT['sent'] += len(e.t.log)


def run_tint(probe, e, r, seed):
    active = 1 if r.random() < 0.85 else 0
    on = r.randrange(2)
    n_nodes = 0
    part_node = []
    for i, _ in enumerate(PARTS):
        k = r.random()
        part_node.append(n_nodes if k < 0.85 else -1)
        if k < 0.85:
            n_nodes += 1
    cls = [5 if r.random() < 0.9 else 2 for _ in range(n_nodes)]
    data = bytearray(r.randbytes(0x90 * max(n_nodes, 1)))
    internal = [r.choice([r.uniform(0, 1), 1.0, 0.0, r.uniform(-0.5, 1.5)]) for _ in PARTS]
    # The original's mech and parts.
    m = e.world_alloc(0x800)
    e.cpu.write(m + 8, bytes([active]))
    nodes = []
    for i in range(n_nodes):
        nd, d = e.world_alloc(0xd4), e.world_alloc(0x90)
        e.w32(nd + 0x34, cls[i])
        e.w32(nd + 0x38, d)
        e.cpu.write(d, bytes(data[0x90 * i:0x90 * (i + 1)]))
        nodes.append((nd, d))
    parts = []
    for i, kind in enumerate(PARTS):
        o = e.world_alloc(0x200)
        e.w32(o, VT[kind])
        e.w32(o + 4, m)  # its mech (the torso's lookup skipped)
        e.w32(o + 8, nodes[part_node[i]][0] if part_node[i] >= 0 else 0)
        e.cpu.write(o + 0x38, struct.pack('<f', internal[i]))
        parts.append(o)
    for i in range(7):
        e.w32(m + OFF[i], parts[i])
    e.w32(parts[0] + 0x78, parts[7])
    e.w32(parts[1] + 0x78, parts[8])
    e.call(0x41b9d0, ecx=m, stack=(on,))
    da = b''.join(bytes(e.cpu.read(d, 0x90)) for _, d in nodes)
    # The port's.
    db = (ctypes.c_uint8 * len(data))(*data)
    probe.probe_mf_mech(active, on, n_nodes, (I32 * max(n_nodes, 1))(*cls), db, (I32 * len(PARTS))(*part_node),
                        (F * len(PARTS))(*internal))
    if da != bytes(db)[:len(da)]:
        k = next(i for i in range(len(da)) if da[i] != db[i])
        print(f'tint seed {seed} (active {active}, on {on}, nodes {part_node}, internal {internal}): node {k // 0x90} '
              f'data differs at {k % 0x90:#x}')
        die()
    COUNT['tints'] += 1
    COUNT['nodes tinted'] += sum(1 for i in range(n_nodes) if da[0x90 * i:0x90 * (i + 1)] != bytes(data[0x90 * i:0x90 * (i + 1)]))


def main():
    probe = load_probe()
    setup_probe(probe)
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    e = emu_new()
    for seed in range(count):
        if seed % 200 == 0:
            e = emu_new()
        r = random.Random(seed)
        run_mix(probe, e, r, seed)
        run_send(probe, e, r, seed)
        run_tint(probe, e, r, seed)
    print(f'ok: {count} cases ({", ".join(f"{v} {k}" for k, v in COUNT.items())}), as the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()

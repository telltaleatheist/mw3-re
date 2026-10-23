"""A basic vehicle's damage and death against the original x86 (docs/vehicles.md,
Damage and death): the damage vt+0x160 0x4ad3f0 (the heat and freeze glow,
the frozen extra, the kill and its credit, the health after, the jolt
0x4accf0 along the shot), the death vt+0x70 0x4ad2d0 (through the real
vtable 0x5c88e8, with the weapons' free 0x4b48c0), zWeapon's hit callback
0x4af550 (by difficulty) and the target view's tint vt+0x68 0x4ad390.

    cmake --build build && .venv/bin/python tests/differential/test_veh_damage.py [runs]

Each run: a vehicle alive or not, its controls destroyed or not, a weapon or
none (no flags, impact heat, freeze, other bits), the glow's answers, health
and amounts from a few to past the kill (NaN and infinities now and then),
an attacker with an object or not, a beam on, weapons or none; the node's
matrix a random rotation (the jolt runs the original's matrix stack). The
x87 at 24 bits, as in play. Compared: the traces, the answer, and the
vehicle's and controls' fields the calls write.
"""
import ctypes
import math
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402

U = ctypes.c_uint32
VTABLE = 0x5c88e8
FIELDS = ['node', 'active', 'ammo', 'engine', 'frozen', 'state', 'health_max', 'health', 'glow_x08',
          'beam_on', 'beam', 'inv_mass', 'destroy_anim', 'angles.x', 'angles.z', 'x70.x', 'x70.z']
FLOATS = {6, 7, 8, 11, 13, 14, 15, 16}


def fb(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def bf(b):
    return struct.unpack('<f', struct.pack('<I', b))[0]


def close(a, b):
    if a == b:
        return True
    fa, fbb = bf(a), bf(b)
    if math.isnan(fa) or math.isnan(fbb):
        return math.isnan(fa) and math.isnan(fbb)
    return abs(fa - fbb) <= 1e-5 * max(1.0, abs(fa))


def rotation(r):
    """A random rotation's 3x3 (rows) and a translation."""
    q = [r.uniform(-1, 1) for _ in range(4)]
    n = math.sqrt(sum(x * x for x in q)) or 1
    w, x, y, z = (v / n for v in q)
    m = [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w),
         2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
         2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]
    if r.random() < 0.15:
        m = [1, 0, 0, 0, 1, 0, 0, 0, 1]
    return m + [r.uniform(-500, 500) for _ in range(3)]


def amount(r):
    c = r.random()
    if c < 0.03:
        return 5.0
    if c < 0.04:
        return r.choice([float('nan'), float('inf'), -float('inf')])
    if c < 0.2:
        return r.uniform(0, 6)
    return r.uniform(0, 150)


def run(probe, seed):
    r = rnd(seed, 'damage')
    mode = r.choice([0, 0, 0, 1, 1, 2, 3])
    t = Trace()
    script = t.script
    e = Emu(t)
    e.heap_stubs()
    e.cpu.fpu_control(0x007f)

    v = e.world_alloc(0x100)
    ctl = e.world_alloc(0xa70)
    e.cpu.write(v, b'\0' * 0x100)
    e.cpu.write(ctl, b'\0' * 0xa70)
    node = e.world_alloc(0x40)
    ammo = e.world_alloc(0x40) if r.random() < 0.6 else 0
    beam = e.world_alloc(0x40)
    slot = ctl + 0x3b8
    anim = e.world_alloc(0x10) if r.random() < 0.8 else 0
    hmax = r.choice([100.0, 40.0, 250.0])
    vals = [node, int(r.random() < 0.9), ammo, int(r.random() < 0.7), int(r.random() < 0.2), r.choice([0, 0, 4]),
            fb(hmax), fb(r.choice([float('nan'), 0.0, -0.0]) if r.random() < 0.06 else r.uniform(-5, hmax)), fb(r.uniform(-1, 1)),
            int(r.random() < 0.3), beam, fb(r.uniform(0, 0.02)), anim,
            fb(r.uniform(-1, 1)), fb(r.uniform(-1, 1)), fb(r.uniform(-5, 5)), fb(r.uniform(-5, 5))]
    names = dict(zip(FIELDS, vals))
    e.w32(v, VTABLE)
    e.w32(v + 4, node)
    e.cpu.write(v + 8, bytes([names['active']]))
    e.w32(v + 0xd4, ammo)
    e.w32(v + 0xe4, ctl)
    e.w32(v + 0xec, e.world_alloc(0xc) if names['engine'] else 0)
    e.w32(ctl + 0x28, names['frozen'])
    e.w32(ctl + 0x38, names['state'])
    e.w32(ctl + 0x740, names['health_max'])
    e.w32(ctl + 0x744, names['health'])
    e.w32(ctl + 0x3a0, names['glow_x08'])
    e.w32(ctl + 0x394, names['beam_on'])
    e.w32(ctl + 0x3b4, slot)
    e.w32(slot + 0x34, beam)
    e.w32(ctl + 0x764 + 0x24c, names['inv_mass'])
    e.w32(ctl + 0x764 + 0x29c, anim)
    e.w32(ctl + 0x32c, names['angles.x'])
    e.w32(ctl + 0x334, names['angles.z'])
    e.w32(ctl + 0x70, names['x70.x'])
    e.w32(ctl + 0x78, names['x70.z'])
    # The controls: vt+0x1c (destroyed), IControls (vbtable[1] = 0x20) vt+0xa8.
    vt = e.world_alloc(0x40)
    e.w32(ctl, vt)
    e.w32(vt + 0x1c, e.new_stub('ctl.vt+0x1c', '', 0))
    vb = e.world_alloc(0x10)
    e.w32(ctl + 4, vb)
    e.w32(vb + 4, 0x20)
    ivt = e.world_alloc(0x100)
    e.w32(ctl + 4 + 0x20, ivt)
    e.w32(ivt + 0xa8, e.new_stub('ctl.ictl.vt+0xa8', '', 0))
    script['ctl.vt+0x1c'] = r.choice([0, 0, 0, 0, 1, 2])

    # The weapon and the glow.
    weapon = 0
    if r.random() < 0.85:
        weapon = e.world_alloc(0x80)
        flags = r.choice([0, 0x400040, 0x400040, 0x400400, 0x400440, 0x400, 0x2, 0x20000])
        e.w32(weapon + 0x6c, flags | (r.getrandbits(32) & 0x0000_0001 if r.random() < 0.2 else 0))
    e.stub(0x586a30, spec='c,d,s0')
    script['0x00586a30'] = lambda n, a: r.choice([0, 0, 1, 2])
    e.stub(0x586af0, spec='c')
    # zWeapon's hit state.
    attacker = e.world_alloc(0x10) if r.random() < 0.7 else 0
    obj = e.object('obj', slots={0x80: ('c,s0', 4)}) if r.random() < 0.7 else 0
    e.stub(0x581e40, spec='')
    script['0x00581e40'] = attacker
    holder = {}

    def node_object(em, key, a, pop):
        n = em.r32(a[0])  # ECX holds the node's address
        em.t.handle(key, (n,))
        em._ret(obj, pop)
        holder['n'] = holder.get('n', 0) + 1
    e.stub(0x40eee0, spec='c', fn=node_object)
    script['0x0040eee0'] = obj
    line = e.world_alloc(0x18)
    p0 = [r.uniform(-300, 300) for _ in range(3)]
    p1 = [x + r.uniform(-50, 50) for x in p0]
    for i, f in enumerate(p0 + p1):
        e.w32(line + 4 * i, fb(f))
    e.stub(0x581e30, spec='')
    script['0x00581e30'] = line
    e.stub(0x583730, spec='s0')
    e.stub(0x583710, spec='c,d')
    e.stub(0x581400, spec='c')
    mat = e.world_alloc(0x30)
    for i, f in enumerate(rotation(r)):
        e.w32(mat + 4 * i, fb(f))
    e.stub(0x4fc3f0, spec='c')
    # The matrix stack's tops (identity flags, matrix pointers; the start-up's).
    e.w32(0x5eda8c, e.world_alloc(0x40))
    e.w32(0x5eda90, e.world_alloc(0x40))
    script['0x004fc3f0'] = mat
    # The death's.
    e.stub(0x402510, spec='c')
    e.stub(0x508c10, spec='c,d,s0,s1,s2')
    e.stub(0x4b1110, spec='c')
    e.stub(0x4b01d0, spec='c')
    e.stub(0x40cdf0, spec='')
    script['0x0040cdf0'] = r.choice([0, 1, 2, 2, 3, -1])

    def tint(em, key, a, pop):
        em.t.handle(key, (a[0], em.r32(a[1]), em.r32(a[1] + 4), em.r32(a[1] + 8), a[2]))
        em._ret(0, pop)
    e.stub(0x4fb8c0, spec='c,d,s0', fn=tint)
    e.stub(0x4fb840, spec='c,d')

    x = r.getrandbits(32)
    amt = fb(amount(r))
    probe.probe_vb_dmg_setup(v, ctl, (U * 17)(*vals))
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    if mode == 0:
        want = e.call(0x4ad3f0, ecx=v, stack=(weapon, x, amt))
    elif mode == 1:
        want = 0
        e.call(0x4af550, ecx=v, edx=weapon, stack=(x, amt))
    else:
        want = 0
        x = r.choice([0, 1, 1, 2, 0xff]) if mode == 2 else 0
        e.call(0x4ad390, ecx=v, stack=(x,))
    mine = probe.probe_vb_dmg_call(mode, weapon, x, amt) & 0xffffffff
    name = f'seed {seed} (mode {mode}, weapon {weapon and hex(e.r32(weapon + 0x6c))}, amount {bf(amt):.3f})'
    if not compare(name, t.log, rep.log):
        return False, 0
    if want != mine:
        print(f'{name}: answer original {want} rebuild {mine}')
        return False, 0
    after = [e.r32(v + 4), e.r32(v + 8) & 0xff, e.r32(v + 0xd4), int(e.r32(v + 0xec) != 0), e.r32(ctl + 0x28),
             e.r32(ctl + 0x38), e.r32(ctl + 0x740), e.r32(ctl + 0x744), e.r32(ctl + 0x3a0), e.r32(ctl + 0x394),
             e.r32(slot + 0x34), e.r32(ctl + 0x764 + 0x24c), e.r32(ctl + 0x764 + 0x29c), e.r32(ctl + 0x32c),
             e.r32(ctl + 0x334), e.r32(ctl + 0x70), e.r32(ctl + 0x78)]
    got = (U * 17)()
    probe.probe_vb_dmg_image(got)
    for i, (a, b) in enumerate(zip(after, got)):
        if not (close(a, b) if i in FLOATS else a == b):
            print(f'{name}: {FIELDS[i]} original {a:#010x} ({bf(a)}) rebuild {b:#010x} ({bf(b)})')
            return False, 0
    return True, len(t.log)


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    probe.probe_vb_dmg_setup.argtypes = [U, U, ctypes.c_void_p]
    probe.probe_vb_dmg_call.argtypes = [ctypes.c_int, U, U, U]
    probe.probe_vb_dmg_call.restype = ctypes.c_int32
    probe.probe_vb_dmg_image.argtypes = [ctypes.c_void_p]
    probe.probe_vb_hooks(1)
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 600
    calls = 0
    for seed in range(runs):
        ok, n = run(probe, seed)
        if not ok:
            sys.stdout.flush(); os._exit(1)
        calls += n
    print(f'ok: {runs} damages, hits, deaths and tints as the original ({calls} calls)')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()

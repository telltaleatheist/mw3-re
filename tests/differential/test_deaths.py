"""A mech's death's small pieces (c1m1 item 2) against the original x86
(docs/damage.md, The death): the parts' effect-end callback 0x461c90; the
canned effects 0x432830 (the eject seat, the jump jets' flames) and their
node copies freed by 0x431800; the death's preparation 0x41d070 (the eject,
the hip's sink, the sound, the player's loss); zEffect's node checks
0x506ae0 / 0x506b60. Outside calls stood in on both sides; the x87 at 24
bits.

    cmake --build build && .venv/bin/python tests/differential/test_deaths.py

BREAK=<n> breaks the port's inputs in a known way (see main); each must fail.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P, I, U, F = ctypes.c_void_p, ctypes.c_int32, ctypes.c_uint32, ctypes.c_float
probe.probe_effect_ended.argtypes = [I, I, P, U, I, P, I]
probe.probe_canned_setup.argtypes = [P]
probe.probe_canned.argtypes = [I, U, P, P, P, U, P, I]
probe.probe_canned_free.argtypes = [P, I]
probe.probe_death_prep.argtypes = [I, U, U, I, P, U, U, P, I]
probe.probe_anim_nodes.argtypes = [U, U, P, I, P, I, P, I, P, I]
CAP = 1 << 16
BUF = ctypes.create_string_buffer(CAP)
ZERO = 0x63d870
LIST, LIST_COUNT, PILOT_GAME = 0x605714, 0x605718, 0x60861c
OP_NEW, OP_DELETE = 0x5abd26, 0x5abd1a


def fb(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


def arr(t, vals):
    return (t * max(len(vals), 1))(*vals)


class Emulated:
    def __init__(self, stub_canned):
        cpu = self.cpu = Original()
        cpu.fpu_control(0x007f)
        assert cpu.read(ZERO, 12) == bytes(12), 'the zero vector'
        cpu.stub_function(OP_NEW, lambda c: self.zalloc(c.arg(0)))
        cpu.stub_function(OP_DELETE, lambda c: 0)
        cpu.stub_function(0x4f9af0, self.copy, 4)
        cpu.stub_function(0x4fc0b0, self.place, 0xc)
        cpu.stub_function(0x508aa0, self.start, 0x24)
        cpu.stub_function(0x4ebb40, self.unparent)
        cpu.stub_function(0x506bc0, self.anims_off, 4)
        cpu.stub_function(0x4f9050, self.destroy)
        cpu.stub_function(0x4ed530, self.world_pos)
        if stub_canned:
            cpu.stub_function(0x432830, self.canned, 0x10)
        cpu.stub_function(0x4622a0, self.duck, 4)
        cpu.stub_function(0x432990, self.sound, 0x14)
        cpu.stub_function(0x48f9e0, self.player_died)
        head = self.zalloc(12)
        cpu.write(head, struct.pack('<II', head, head))
        cpu.u32(LIST, head)
        cpu.u32(LIST_COUNT, 0)
        self.game = self.zalloc(0x80)
        cpu.u32(PILOT_GAME, self.game)
        self.pilot = self.zalloc(0x234)
        self.ids = {}
        self.out = bytearray()
        self.bad = None

    def zalloc(self, n):
        p = self.cpu.alloc(max(n, 4))
        self.cpu.write(p, bytes(max(n, 4)))
        return p

    def log(self, *v):
        for x in v:
            self.out += struct.pack('<I', x & 0xffffffff)

    def logf(self, a, n=1):
        self.out += self.cpu.read(a, 4 * n)

    def r32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    # --- the canned effects' calls ---
    def copy(self, c):
        r = self.copies.pop(0)
        node = 0
        if r:
            node = self.zalloc(0x60)
            parents = self.rng.randint(0, 2)
            arr_ = self.zalloc(8)
            self.cpu.write(arr_, struct.pack('<II', 0x7100, 0x7200))
            self.cpu.write(node + 0x54, struct.pack('<II', parents, arr_))
            self.ids[node] = (r, parents)
        self.log(1, c.ecx, r)
        return node

    def nid(self, node):
        return self.ids[node][0] if node in self.ids else node

    def place(self, c):
        self.log(2, self.nid(c.ecx), c.arg(0), c.arg(1), c.arg(2))

    def start(self, c):
        self.log(3, c.ecx, self.nid(c.edx), *[c.arg(i) for i in range(9)])
        return self.started

    def unparent(self, c):
        node = c.edx
        n, a = struct.unpack('<II', self.cpu.read(node + 0x54, 8))
        if c.ecx != self.r32(a) or n == 0:
            self.bad = 'unparent'
        self.cpu.write(node + 0x54, struct.pack('<I', n - 1))
        self.cpu.u32(a, self.r32(a + 4))
        self.removed[node] = self.removed.get(node, 0) + 1

    def anims_off(self, c):
        if c.edx != 1 or c.arg(0) != 1:
            self.bad = 'anims_off args'
        self.stopped.append(c.ecx)

    def destroy(self, c):
        node = c.ecx
        if self.removed.get(node, 0) != self.ids[node][1] or self.stopped[-1:] != [node]:
            self.bad = 'free order'
        self.log(4, self.nid(node))

    # --- the death's calls ---
    def world_pos(self, c):
        self.log(0x10, c.ecx)
        self.cpu.write(c.edx, struct.pack('<3f', *self.hip_at))

    def canned(self, c):
        at, vel = c.arg(2), c.arg(3)
        self.log(0x11, c.arg(0), c.arg(1))
        self.logf(at, 3)
        self.log(0 if vel == ZERO else 1)
        if at == ZERO or c.ecx != self.pilot:
            self.bad = 'canned args'
        return self.canned_ret

    def duck(self, c):
        self.log(0x12, c.arg(0))

    def sound(self, c):
        if (c.arg(1), c.arg(2), c.arg(3), c.arg(4)) != (fb(1.0), 0, 0, fb(-1.0)) or c.ecx != self.pilot:
            self.bad = 'sound args'
        self.log(0x13, c.arg(0))
        return self.sound_ret

    def player_died(self, c):
        if c.ecx != 0x6098d0:
            self.bad = 'player_died'
        self.log(0x14)

    # --- the cases ---
    def effect_ended(self, has_node, nid, handles, effect, value):
        self.out = bytearray()
        part = self.zalloc(0x1b4)
        if has_node:
            node = self.zalloc(0x60)
            self.cpu.u32(node + 0x28, nid)
            self.cpu.u32(part + 8, node)
        self.cpu.write(part + 0x6c, struct.pack('<4I', *handles))
        self.cpu.call(0x461c90, ecx=effect, edx=part, stack_args=(value,))
        self.logf(part + 0x6c, 4)
        return bytes(self.out)

    def canned_setup(self, slots):
        for i in range(5):
            self.cpu.write(self.pilot + 0x14c + 0x14 * i, struct.pack('<5I', *slots[5 * i:5 * i + 5]))

    def canned_case(self, slot, node, at, vel, copies, started, rng):
        self.out = bytearray()
        self.copies, self.started, self.rng = list(copies), started, rng
        def vec(v):
            if v is None:
                return ZERO
            a = self.zalloc(12)
            self.cpu.write(a, struct.pack('<3f', *v))
            return a
        self.cpu.call(0x432830, ecx=self.pilot, stack_args=(slot, node, vec(at), vec(vel)))
        self.log(self.cpu.uc.reg_read(UC_X86_REG_EAX))
        return bytes(self.out)

    def canned_free(self):
        self.out = bytearray()
        self.removed, self.stopped = {}, []
        self.cpu.call(0x431800)
        self.log(0xf)
        if self.r32(LIST_COUNT) != 0:
            self.bad = 'list count'
        return bytes(self.out)

    def death_prep(self, state, detail, x634, player, hip_at, canned, sound):
        self.out = bytearray()
        self.hip_at, self.canned_ret, self.sound_ret = hip_at, canned, sound
        m = self.zalloc(0x6d8)
        hip = self.zalloc(0xb0)
        self.cpu.u32(hip + 8, 0x6000)
        self.cpu.u32(m + 0x3a0, hip)
        self.cpu.u32(m + 0x684, self.pilot)
        self.cpu.u32(m + 0x688, detail)
        self.cpu.u32(m + 0x634, x634)
        self.cpu.u32(m + 0x578, state)
        self.cpu.u32(self.game + 0x14, m if player else 0)
        self.cpu.call(0x41d070, ecx=m)
        self.log(0xf, self.r32(m + 0x634), self.r32(m + 0x5e0))
        return bytes(self.out)

    def anim_nodes(self, node, root, objects, nodes, queries):
        self.out = bytearray()
        d = self.zalloc(0x13c)
        o = self.zalloc(0x60 * max(len(objects), 1))
        n = self.zalloc(0x28 * max(len(nodes), 1))
        for i, v in enumerate(objects):
            self.cpu.u32(o + 0x60 * i + 0x24, v)
        for i, v in enumerate(nodes):
            self.cpu.u32(n + 0x28 * i + 0x24, v)
        self.cpu.u32(d + 0x40, node), self.cpu.u32(d + 0x64, root)
        self.cpu.write(d + 0x109, bytes([len(objects), len(nodes)]))
        self.cpu.u32(d + 0x114, o), self.cpu.u32(d + 0x118, n)
        for q in queries:
            self.cpu.call(0x506ae0, ecx=d, edx=q)
            self.log(1 if self.cpu.uc.reg_read(UC_X86_REG_EAX) else 0)
        self.cpu.call(0x506b60, ecx=d)
        for i in range(len(objects)):
            self.logf(o + 0x60 * i + 0x24)
        for i in range(len(nodes)):
            self.logf(n + 0x28 * i + 0x24)
        return bytes(self.out)


def main():
    rng = random.Random(int(os.environ.get('SEED', '1')))
    brk = int(os.environ.get('BREAK', '0'))
    emu = Emulated(False)
    dying = Emulated(True)
    runs = fails = 0
    kinds = {}

    def check(kind, mine, theirs):
        nonlocal runs, fails
        runs += 1
        kinds[kind] = kinds.get(kind, 0) + 1
        for e in (emu, dying):
            if e.bad:
                print(f'FAIL {kind}: the original called out wrongly ({e.bad})')
                e.bad = None
                fails += 1
        if mine != theirs:
            fails += 1
            if fails <= 3:
                print(f'FAIL {kind}: port {mine.hex()}\n     original {theirs.hex()}')

    # The effect-end callback: which handle the ending effect was.
    for _ in range(400):
        handles = [rng.choice((0, 0x11, 0x22, 0x33, 0x44)) for _ in range(4)]
        effect = rng.choice((0x11, 0x22, 0x33, 0x44, 0x55, 0))
        has_node, nid, value = rng.randint(0, 1), rng.choice((1, 1, 2, 0x101, -1)), rng.choice((0, 0, 1, 7))
        mine_value = (1 - value if value in (0, 1) else value) if brk == 1 else value
        n = probe.probe_effect_ended(has_node, nid, arr(U, handles), effect, mine_value, BUF, CAP)
        check('end', BUF.raw[:n], emu.effect_ended(has_node, nid, handles, effect, value))

    # The canned effects, a mission's worth of node copies, then their free.
    for _ in range(30):
        slots = []
        for i in range(5):
            slots += [rng.choice((0, 0x900 + i)), fb(rng.uniform(-2, 2)), fb(rng.choice((0.0, 3.6))),
                      fb(rng.uniform(-2, 2)), rng.choice((0, 0x800 + i, 0x800 + i))]
        emu.canned_setup(slots)
        probe.probe_canned_setup(arr(U, slots))
        for _ in range(rng.randint(0, 8)):
            slot = rng.randint(0, 4)
            node = rng.choice((0, 0, 0x4321))
            at = rng.choice((None, (rng.uniform(-500, 500), rng.uniform(0, 50), rng.uniform(-500, 500))))
            vel = rng.choice((None, (1.0, -2.5, 0.25)))
            copies = [rng.choice((0x1000 + rng.randint(0, 0xfff), 0x1000 + rng.randint(0, 0xfff), 0))]
            started = rng.choice((0, 0x5555))
            mine_at = (at[0] + 1.0, at[1], at[2]) if brk == 2 and at else at
            fa = arr(F, mine_at) if mine_at else None
            fv = arr(F, vel) if vel else None
            n = probe.probe_canned(slot, node, fa, fv, arr(U, copies), started, BUF, CAP)
            check('canned', BUF.raw[:n], emu.canned_case(slot, node, at, vel, copies, started, rng))
        n = probe.probe_canned_free(BUF, CAP)
        check('free', BUF.raw[:n], emu.canned_free())

    # The death's preparation.
    probe.probe_canned_setup(arr(U, [0] * 25))
    for _ in range(600):
        state = rng.choice((1, 2, 10, 11, 12, 13, 14))
        detail, x634 = rng.choice((0, 1, 0x3e, 0x3f)), rng.choice((0, 1, 0xff, 0x80000001))
        player = rng.randint(0, 1)
        hip_at = (rng.uniform(-900, 900), rng.uniform(0, 60), rng.uniform(-900, 900))
        canned, sound = rng.choice((0, 0x7000)), rng.choice((0, 0x7100))
        mine_state = 1 if brk == 3 and state in (12, 13) else state
        n = probe.probe_death_prep(mine_state, detail, x634, player, arr(F, hip_at), canned, sound, BUF, CAP)
        check('death_prep', BUF.raw[:n], dying.death_prep(state, detail, x634, player, hip_at, canned, sound))

    # zEffect's node checks.
    for _ in range(400):
        pool = [0x100 * k for k in range(1, 9)]
        objects = [rng.choice(pool + [0]) for _ in range(rng.randint(0, 5))]
        nodes = [rng.choice(pool + [0]) for _ in range(rng.randint(0, 5))]
        node, root = rng.choice(pool + [0]), rng.choice(pool + [0])
        queries = [rng.choice(pool + [0]) for _ in range(6)]
        mine_q = [q + (0x100 if brk == 4 and q == node else 0) for q in queries]
        n = probe.probe_anim_nodes(node, root, arr(U, objects), len(objects), arr(U, nodes), len(nodes),
                                   arr(U, mine_q), len(mine_q), BUF, CAP)
        check('anim_nodes', BUF.raw[:n], emu.anim_nodes(node, root, objects, nodes, queries))

    if fails:
        print(f'{fails} of {runs} runs differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} runs identical ({", ".join(f"{k} {v}" for k, v in sorted(kinds.items()))}); '
          f'{len(emu.ids)} node copies made and freed, {sum(p for _, p in emu.ids.values())} parents taken off')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()

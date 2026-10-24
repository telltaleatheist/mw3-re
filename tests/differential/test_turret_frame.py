"""The turrets in combat against the original x86 (docs/vehicles.md, The
turrets' frame): the frame 0x4a6be0, the pick 0x4a70d0 with AiTarget's
0x4d72b0 and 0x4a7b40 / 0x4a7ba0, the aim 0x4a71c0 (its bit-trick square
root, the turn limits' within 0x4a4180 / 0x4a4330 / 0x4a45b0 / 0x4a4830),
the shot, the burst, the beam, the effect, the AMS turret and the mine.

    cmake --build build && .venv/bin/python tests/differential/test_turret_frame.py [runs]

Each run loads turrets through the original's loader (test_turret.py's
world, with every PARTS node found), gives each a list of contacts (the
player among them or not, of the enemy team or not, in sight or not), then
runs 40 to 120 frames: the frame count, clock and dt moving (1/60 to 1/15
s), the count of turrets made since the start raised now and then as if
from earlier missions, nodes hidden, hits, idle messages. The matrix stack,
the weapon's lead, the AMS's radar and the shot are stubbed from one script.
Compared after every frame: the traces, each turret's 0x284 bytes, its
turret's and gun's matrices, the pick's turn. The x87 at 24 bits; probe
bugs off (the count kept).
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from unicorn import UC_HOOK_CODE  # noqa: E402
from calltrace import Trace, compare, load_probe, ret_pop  # noqa: E402
from test_turret import (SIZE, Original, World, f2u, f32, h32, made_tree, nan_free, port_state,  # noqa: E402
                         prefixes_for, script_for, setup_probe, shipped, turret_names, u2f)

U32, I32 = ctypes.c_uint32, ctypes.c_int32
FRAME, CLOCK, DT = 0x72f338, 0x72c530, 0x72c52c
COUNT, TURN = 0x609a48, 0x609a4c
SURE = ('h', 'f', 'named')
COVER = {
    0x4a6bfe: 'dead: shut down', 0x4a6c45: 'awake', 0x4a70b3: 'a node hidden', 0x4a6c78: 'a mine',
    0x4a6c88: 'a mine goes off', 0x4a6cd7: 'the effect animated', 0x4a6cef: 'a target picked',
    0x4a6d05: 'the aim took', 0x4a6d2a: 'ready to fire', 0x4a6d3a: 'the effect lit',
    0x4a6d81: 'a burst begun', 0x4a6da9: 'the firepoints alternate', 0x4a6df0: 'one firepoint',
    0x4a6ebc: 'a lead', 0x4a6edb: 'the AMS fires', 0x4a6ef8: 'the fire sound', 0x4a6f1d: 'a beam on',
    0x4a6f6b: 'fired from the firepoint', 0x4a6fd3: 'a shot', 0x4a6ff9: 'an ONFIRE_ANIM',
    0x4a7045: 'a burst over', 0x4a7065: 'the fire rate', 0x4a709e: 'a beam off',
    0x4a7113: 'the nearest enemy asked', 0x4a7123: 'the turn wrapped', 0x4a7131: 'its target\'s place',
    0x4a713a: 'the AMS looks', 0x4a715c: 'a shot to stop', 0x4a71a3: 'nothing picked',
    0x4d72ef: 'an enemy contact', 0x4d72fd: 'one in sight', 0x4d7319: 'the enemy targeted',
    0x4a7b55: 'the player already the target', 0x4a7bbc: 'the player\'s contact', 0x4a7bcb: 'no player contact',
    0x4a72f3: 'inside the range', 0x4a7335: 'aimed', 0x4a73bd: 'the limits refuse', 0x4a73e4: 'a turret node',
    0x4a745a: 'a gun alone', 0x4a4349: 'fixed: the pitch',
    0x4a464b: 'narrow: inside', 0x4a4655: 'narrow: outside', 0x4a48d3: 'wide: inside',
}
REACHED = set()
XF = (3.0, 1.5, -2.0)


def frame_script(w, state):
    """The frame's answers: positions move with the frame, the matrix stack
    adds an offset to a point and turns a direction a quarter about y."""
    def k():
        return state['k']

    def wpos(n, a):
        return f2u(f32(((h32(w.seed, 'wp', a[0], a[1]) % 2000) - 1000) / 10.0 + 0.25 * k()))

    def xf(n, a):
        return f2u(f32(u2f(a[a[3]]) + XF[a[3]]))

    def ixf(n, a):
        x, y, z = (u2f(v) for v in a[:3])
        return f2u([z, y, -x][a[3]])

    def lead(n, a):
        return f2u(f32(u2f(a[a[3]]) + 0.01 * (a[3] + 1)))

    def slope(n, a):
        return f2u(random.Random(h32(w.seed, 'sl', k(), a)).choice([-1.0, 0.25, 1.5]))

    def vehicle(n, a):
        return state['veh'].get(a[0], 0)

    def target_of(n, a):
        return state['tgt'].get(a[0], 0)

    def place(n, a):
        return state['pos'].get(a[0], 0)

    return {
        '@wpos': wpos, '@xf': xf, '@ixf': ixf, '@lead': lead, '0x00581ff0': slope,
        '0x004e6fe0': vehicle, 'veh.vt+0x28': target_of, '0x005a8630': place,
        '0x004d5980': lambda n, a: h32(w.seed, 'los', k() // 7, a) % 3 != 0,
        '0x004b3ae0': lambda n, a: h32(w.seed, 'ams', k()) % 3 == 0,
        '0x004b3ba0': lambda n, a: state['shot'],
        'obj.vt+0x24': lambda n, a: state['pv'],
        'pv.vt+0x28': lambda n, a: state['pteam'],
    }


class FrameOriginal(Original):
    def __init__(self, tree, seed, prefixes):
        super().__init__(tree, seed, prefixes, True, sure=SURE)
        e, t, w = self.e, self.t, self.w
        self.fs = {'k': 0, 'veh': {}, 'tgt': {}, 'pos': {}}
        st = self.fs
        # The player's vehicle and its team, a shot on the radar.
        pv = e.object('pv', slots={0x28: ('c', 0)})
        q = w.block()
        w.put(q + 0xc0, 1 << random.Random(seed).choice([1, 2, 3]))
        st['pv'], st['pteam'] = pv, q
        st['shot'] = self.vec(seed, 'shot')
        pvt = e.r32(w.player)
        e.w32(pvt + 0x24, e.new_stub('obj.vt+0x24', 'c', 0))
        for addr, spec in {0x541ad0: 'c', 0x5820c0: 'c,d,s0', 0x51e9d0: '', 0x51fde0: '', 0x4ed280: 'c,d',
                           0x51ea00: '', 0x4fc300: 'c,d', 0x581f60: 'c,d', 0x581fb0: 'c,d', 0x581230: 'c,d',
                           0x4b4a40: 'c,d,s0,s1,s2,s3,s4,s5,s6', 0x56e7e0: 'c,d,s0,s1', 0x4b3ae0: 's1',
                           0x4b3ba0: '', 0x4b3bb0: 's0', 0x4e6fe0: 'c', 0x4d5980: 'c,s0,s1', 0x5a8630: 'c'}.items():
            e.stub(addr, spec=spec)

        def writer(key):
            def fn(em, k, a, pop):
                v = t.handle(k, a)
                out = em.args('d')[0]
                for i in range(3):
                    em.w32(out + 4 * i, t.handle(key, [a[0], i]))
                em._ret(v, pop)
            return fn
        e.stub(0x4ed530, spec='c', fn=writer('@wpos'))

        def through(key):
            def fn(em, k, a, pop):
                v = t.handle(k, em.args('d'))
                p = em.args('c')[0]
                ins = [em.r32(p + 4 * i) for i in range(3)]
                for i in range(3):
                    em.w32(p + 4 * i, t.handle(key, ins + [i]))
                em._ret(v, pop)
            return fn
        e.stub(0x5227a0, spec='d', fn=through('@xf'))
        e.stub(0x522640, spec='d', fn=through('@ixf'))

        def lead(em, k, a, pop):
            v = t.handle(k, a)
            p = a[0]
            ins = [em.r32(p + 4 * i) for i in range(3)]
            for i in range(3):
                em.w32(p + 4 * i, t.handle('@lead', ins + [i]))
            em._ret(v, pop)
        e.stub(0x4abea0, spec='c,s0', fn=lead)

        fbuf = e.alloc(8)
        pop = ret_pop(0x581ff0)
        fthunk = e.thunk(b'\xd9\x05' + struct.pack('<I', fbuf) + (b'\xc2' + struct.pack('<H', pop) if pop else b'\xc3'))

        def slope(em, k, a, p):
            em.w32(fbuf, t.handle(k, a))
            em._ret(0, 0, then=fthunk)
        e.stub(0x581ff0, spec='c,d,s0,s1', fn=slope)

        def set_target(em, k, a, p):
            t.handle(k, a)
            em.w32(a[0] + 0x6c, a[1])
            em._ret(0, p)
        e.stub(0x4d5e40, spec='c,s0,s1', fn=set_target)

        def hit(uc, address, size, user):
            REACHED.add(address)
        for a in COVER:
            e.uc.hook_add(UC_HOOK_CODE, hit, begin=a, end=a)

    def vec(self, *key):
        w = self.w
        a = w.block(0x10)
        r = random.Random(h32(key))
        for i in range(3):
            w.putf(a + 4 * i, round(r.uniform(-60, 60), 2))
        return a


def framed(tree, r):
    """Every turret with PARTS of two to four names (the exe's frame reads
    the healthy node and the aim the gun's matrix unchecked: the port's
    FIX (bug) guards are test_turret.py's crash files')."""
    lst = tree[0][tree[0].index('TURRET') + 1]
    for d in lst[1::2]:
        if 'PARTS' in d:
            i = d.index('PARTS')
            del d[i:i + 2]
        d += ['PARTS', r.sample(['turret', 'gun', 'firepoint1', 'firepoint2'], r.choice([2, 3, 4, 4]))]
    return tree


def contacts_for(o, r, p):
    """Turret p's contacts, nearest first: the player or other vehicles,
    their vehicles' teams the enemy's (the turret's team 2) or not."""
    w, st = o.w, o.fs
    n = r.choice([0, 1, 2, 3, 4])
    addrs, seen = [], []
    arr = w.block(4 * max(n, 1))
    for i in range(n):
        c = w.block(0x30)
        ent = w.player if r.random() < 0.4 else w.handle('ent', p, i)
        w.put(c + 4, ent)
        veh = o.e.object('veh', slots={0x28: ('c', 0)})
        tgt = w.block()
        w.put(tgt + 0xc0, r.choice([1, 2, 4, 8, 6]))
        st['veh'][c], st['tgt'][veh] = veh, tgt
        st['pos'].setdefault(ent, o.vec('pos', ent))
        w.put(arr + 4 * i, c)
        addrs.append(c)
        seen.append(ent)
    o.e.w32(p + 0xac, arr)
    o.e.w32(p + 0xb0, arr + 4 * n)
    return addrs, seen


class Run:
    def __init__(self, probe):
        self.probe = probe
        self.fails = self.cases = self.frames = self.shots = 0

    def fail(self, label, msg):
        self.fails += 1
        if self.fails <= 6:
            print(f'MISMATCH {label}: {msg}')

    def run(self, label, tree, seed):
        r = random.Random(seed)
        prefixes = prefixes_for(r, turret_names(tree))
        self.cases += 1
        o = FrameOriginal(tree, seed, prefixes)
        e, w, st = o.e, o.w, o.fs
        at = 0x2b000000
        if not o.load(at) or not o.turrets:
            return
        probe = self.probe
        t = Trace()
        probe.probe_app_set_callback(t.callback())
        probe.probe_turret_reset()
        t.reset()
        script = script_for(w, prefixes, o.root)
        script.update(frame_script(w, st))
        t.script = script
        o.t.script.update(frame_script(w, st))
        handles = o.turrets
        probe.probe_turret_handles((U32 * len(handles))(*handles), len(handles))
        probe.probe_turret_node_args((U32 * len(handles))(*[e.r32(p + 4) for p in handles]), len(handles))
        contacts = [contacts_for(o, r, p) for p in handles]
        for a, v in w.mem.items():
            probe.probe_turret_poke(a, v)
        for a, nm in w.names.items():
            probe.probe_turret_name(a, nm.encode())
        from test_turret import ser
        data = ser(tree)
        if probe.probe_turret_load(data, len(data), at) != len(handles):
            self.fail(label, 'the load differs')
            return
        for i, (addrs, seen) in enumerate(contacts):
            n = len(addrs)
            probe.probe_turret_contacts(i, (U32 * max(n, 1))(*addrs), (U32 * max(n, 1))(*seen), n)
        count = len(handles) + r.choice([0, 0, 0, 3, 40])
        turn = r.randrange(count)
        e.w32(COUNT, count)
        e.w32(TURN, turn)
        probe.probe_turret_counts(count, turn)
        o.t.log.clear()
        t.log.clear()
        clock, frame = 1.0 + r.random(), r.randrange(1000)
        for k in range(r.randrange(40, 121)):
            st['k'] = k
            frame += r.choice([1, 1, 1, 2])
            dt = r.choice([1 / 60, 1 / 30, 1 / 30, 1 / 20, 1 / 15])
            clock = f32(clock + dt)
            for a, v in ((FRAME, frame), (CLOCK, f2u(clock)), (DT, f2u(f32(dt)))):
                w.put(a, v)
                probe.probe_turret_poke(a, v)
            self.events(r, o, probe, k)
            for i, p in enumerate(handles):
                if r.random() < 0.15:
                    continue
                e.call(0x4a6be0, ecx=p, stack=(0,))
                probe.probe_turret_frame(i)
            self.frames += 1
            if not compare(f'{label} frame {k}', o.t.log, t.log):
                self.fail(label, f'trace at frame {k}')
                return
            self.shots += sum(1 for key, _ in o.t.log if key in ('0x004b4a40', '0x00581230', '0x005820c0'))
            o.t.log.clear()
            t.log.clear()
            if not self.compare_state(f'{label} frame {k}', o, probe):
                return
        probe.probe_turret_delete_all()

    def events(self, r, o, probe, k):
        """Now and then a node hidden or shown, a hit, an idle message."""
        e, w = o.e, o.w
        if not o.turrets or r.random() > 0.2:
            return
        i = r.randrange(len(o.turrets))
        p = o.turrets[i]
        ev = r.choice(['flag', 'flag', 'hit', 'idle', 'wake'])
        if ev == 'flag':
            n = e.r32(p + r.choice([0x4, 0xdc, 0xf8, 0x130]))
            if n:
                w.put(n + 0x24, e.r32(n + 0x24) ^ 4)
                probe.probe_turret_poke(n + 0x24, e.r32(n + 0x24))
        elif ev == 'hit':
            dmg = f2u(r.choice([1.0, 4.0, 12.0]))
            o.op(p, 0, 0x55, 0x66, dmg)
            probe.probe_turret_op(i, 0, 0x55, 0x66, dmg)
        else:
            msg = 0x33 if ev == 'idle' else 0x32
            o.op(p, 3, 0x77, msg, 0)
            probe.probe_turret_op(i, 3, 0x77, msg, 0)

    def compare_state(self, label, o, probe):
        e = o.e
        if e.r32(TURN) != probe.probe_turret_turn():
            self.fail(label, f'the turn {e.r32(TURN)} vs {probe.probe_turret_turn()}')
            return False
        for i, p in enumerate(o.turrets):
            mine, _ = o.state(p)
            port, _ = port_state(probe, i)
            mine, port = nan_free(mine), nan_free(port)
            mine = mine[:0xac] + bytes(12) + mine[0xb8:]  # the contacts' vector (each side's own)
            if mine != port:
                diffs = [k for k in range(SIZE) if mine[k] != port[k]]
                d0 = diffs[0] & ~3
                self.fail(label, f'turret {i} bytes differ at {[hex(d) for d in diffs[:10]]}: '
                          f'{mine[d0:d0 + 8].hex()} vs {port[d0:d0 + 8].hex()}')
                return False
            for off in (0x100, 0x120):
                m = e.r32(p + off)
                if not m:
                    continue
                a = nan_free(b''.join(struct.pack('<I', e.r32(m + 4 * j)) for j in range(12)))
                b = nan_free(b''.join(struct.pack('<I', probe.probe_turret_peek(m + 4 * j)) for j in range(12)))
                if a != b:
                    self.fail(label, f'turret {i} matrix +{off:#x}: {a.hex()} vs {b.hex()}')
                    return False
        return True


def main():
    probe = load_probe()
    setup_probe(probe)
    probe.probe_turret_contacts.argtypes = [I32, ctypes.c_void_p, ctypes.c_void_p, I32]
    probe.probe_turret_frame.argtypes = [I32]
    probe.probe_turret_counts.argtypes = [I32, I32]
    probe.probe_turret_peek.argtypes = [U32]
    probe.probe_turret_peek.restype = U32
    probe.probe_turret_turn.restype = I32
    run = Run(probe)
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 120
    tree = shipped()
    for seed in range(runs // 3):
        run.run(f'shipped {seed}', tree, 7000 + seed)
    rng = random.Random(9)
    for k in range(runs - runs // 3):
        run.run(f'made-up {k}', framed(made_tree(rng), rng), 8000 + k)
    if run.fails:
        raise SystemExit(f'{run.fails} of {run.cases} differ')
    missed = [v for a, v in COVER.items() if a not in REACHED]
    if missed:
        raise SystemExit(f'not reached: {missed}')
    print(f'ok: {run.cases} runs, {run.frames} frames, {run.shots} shots, {len(COVER)} branches, as the original')


if __name__ == '__main__':
    main()

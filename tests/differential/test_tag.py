"""TAG end to end (docs/weapons.md, "TAG, end to end"): scenarios run on the
original x86 and on the port side by side, through the harnesses of
test_weapons_run.py (the weapon manager and the beam weapon) and
test_weapons_fly.py (zWeapon's beam frame, the airstrike and the missiles).
Every operation is compared the way those tests compare it (every call out,
the manager, the weapons, the pool slots, the beams); the numbers printed are
the original's.

    cmake --build build && .venv/bin/python tests/differential/test_tag.py

1. The hold: TAG mounted by the configuration step, group 5 selected, the
   trigger held at 30, 60 and 144 fps: the frames the beam lasts, the charge
   it leaves, the frames until it can fire again (cool and hot), letting go
   early.
2. The designation: a TAG beam in zWeapon's frame on a scripted world (a
   target with a damage handler, ground without one, the sky): how many
   frames on the target call the strike, and what one or two frames off it
   do, against the frames the beam lasts (part 1).
3. The strike: the four TAG_MISSILEs the airstrike spawns, flown frame by
   frame over flat ground against a mech (a capsule with a damage handler)
   standing still or walking away: arrival time, where they land, the damage.

The world's answers come from Python for both sides: the segment query
meets the ground plane and the capsule, the sphere query (0x4f0ac0, not
ported) reports the mech with its distance to the capsule's surface (an
assumption about what the engine measures). The flights compare within 2e-3
of at least 100 (TAG_FLIGHT_TOL): the CRT's acos and the x87's part in the
last bits and the difference grows over a thousand homing frames.

TAG_TEST_BREAK=<n> runs break test n: the port's side is changed in a way a
bug would change it (see BREAKS) and the run must fail.
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
import test_weapons_fly as fly  # noqa: E402
import test_weapons_run as run  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

f2i, i2f, f32, s32 = run.f2i, run.i2f, run.f32, run.s32
probe = run.probe
TAG, TAG_MISSILE = 32, None  # mech-lab index; the missile's def index is looked up
FPS = (30, 60, 144)
BREAK = int(os.environ.get('TAG_TEST_BREAK', '0'))
BREAKS = {
    1: 'the port\'s frames are 1% longer in the hold (part 1)',
    2: 'the port gets one more frame of the trigger held (part 1)',
    3: 'the port sees the mech where the original sees ground (part 2)',
    4: 'the port\'s mech stands 2 m further (part 3)',
    5: 'zWeapon\'s frame is 1% longer on the port (parts 2 and 3)',
}


# --- part 1: the hold ---------------------------------------------------------------
class TagRun(run.Run):
    def compare(self, label, a_res, b_res):
        self.last = self.emu.events_with_radar()
        return super().compare(label, a_res, b_res)

    def tag_manager(self, heat=0.0):
        """A mech with TAG as its only equipment and no weapons, the world held still."""
        emu, rng, cpu = self.emu, self.rng, self.emu.cpu
        self.world = w = run.World(emu, rng, True)
        v = w.v
        v.update(player=emu.M, mission_player=emu.M, part_mech=emu.M, owner_x58=0, fcs=0, control=0, screen=1,
                 view=emu.VO, view_kind=2, torso_look=0, spawn=emu.spawn_base, beam=0x7c000000, stats=0,
                 heat=f2i(f32(heat)), highlight=-1)
        for p in w.parts:
            p[3] = p[4] = 0  # no part under water, none failing
            p[12] = 1  # the model nodes carry the firepoint names
            p[1] = p[1] or emu.nodes[w.parts.index(p)]
        w.fps = [[0.5, 0.5, 3, 3, 0.0, 0.0, 0.0], [0.5, 0.5, 2, 2, 0.0, 0.0, 0.0]]
        run.build_dfn(emu, w)
        w.items = [(3, 1, 0, 7, 0, 13)]  # ("tag" "ltorso")
        run.build_config(emu, w.items)
        emu.vfp = [w.parts[3][1], w.parts[2][1]]
        probe.probe_wm_vehicle_fp(s32(emu.vfp[0]), s32(emu.vfp[1]))
        for k in range(emu.ndefs):
            cpu.write(emu.defs_base + 0x1f4 * k + 0xd8, bytes(8))
        emu.mgr = emu.zalloc(0x94)
        emu.spawn_n = emu.beam_n = emu.sound_n = 0
        emu.weapon_items = []
        w.push()
        seed = 1
        emu.rand = run.MsvcRand(seed)
        flat = [x for fp in w.fps for x in fp + [0.0]]
        fbuf = (ctypes.c_float * len(flat))()
        for i, x in enumerate(flat):
            fbuf[i] = x if i % 8 not in (2, 3) else i2f(x)
        iflat = [x for it in w.items for x in it]
        probe.probe_wm_new(fbuf, len(w.fps), (ctypes.c_int32 * len(iflat))(*iflat), len(w.items), seed)
        emu.events, emu.spawned = [], []
        cpu.call(run.F['manager_ctor'], ecx=emu.mgr, stack_args=(emu.V,))
        probe.probe_wm_reset_fire()
        for addr, val in ((0x9a0ee0, 0), (0x9a0ee4, 0), (0x9a17cc, 1), (0x9a17d0, 0x3f800000), (0x9a17d4, 0x3f800000),
                          (0x9a17d8, 0x3f800000), (0x9a17dc, 0x3f800000), (0x9a17e0, 0)):
            cpu.u32(addr, val)
        self.compare('new manager', 0, 0)
        self.op(32, (0x6100,))  # the configuration step mounts TAG
        self.op(3, (4,))        # F5: group 5
        assert self.emu.u32(self.emu.mgr + 0x28) == 4, 'group 5 is not selectable'
        self.op(11, (1, f2i(0.0), f2i(0.0), f2i(-300.0)))

    def tag(self):
        u = self.emu.u32
        w = u(self.emu.mgr + 0x40)
        b = u(w + 0x58)
        return {'firing': self.emu.cpu.read(w + 0x98, 1)[0], 'amount': s32(u(b + 4)), 'max': s32(u(b + 8)),
                'firepoint': u(w + 0x80)}

    def frame(self, down, dt):
        self.op(2, (int(down), f2i(f32(dt))))
        return [e[0] for e in self.last if e[0] in (15, 16)]  # beam start, beam stop


def part1(stats):
    if BREAK == 1:  # the port's frames 1% longer
        real = probe.probe_wm_op

        def longer(op, a):
            if op == 2:
                a[1] = s32(f2i(f32(i2f(a[1]) * 1.01)))
            return real(op, a)
        probe.probe_wm_op = longer
    print('1. the hold (original 0x4b0c30 / 0x4b3fc0 / 0x4b4230 / 0x4b4180 / 0x4b41b0, the port beside it)')
    emu = run.Emulated()
    r = TagRun(emu, random.Random(7))
    out = {}
    for fps in FPS:
        dt = f32(1.0 / fps)
        r.tag_manager()
        t = r.tag()
        assert t['firepoint'] != 0, 'TAG mounted without a firepoint'
        assert t['amount'] == t['max'] == 10000, t
        if BREAK == 2:  # the port's side gets one more frame
            probe.probe_wm_op(2, (ctypes.c_int32 * 16)(1, s32(f2i(dt))))
        # held from a full charge: the frame of the start, the frames it fires, the frame it stops
        start = stop = None
        ticks = 0
        for k in range(10 * fps):
            ev = r.frame(True, dt)
            if 15 in ev:
                start = k
            if 16 in ev:
                stop = k
                break
            if start is not None:
                ticks += 1
        assert start == 0 and stop is not None, (start, stop)
        # zWeapon's tick runs after the mech's update in a frame (priority 3 after 2):
        # the beam is cast on frames start..stop-1
        beam_frames = stop - start
        # let go, then count the frames until a press fires again
        r.frame(False, dt)
        refill = 1
        while r.tag()['amount'] < 10000:
            r.frame(False, dt)
            refill += 1
            assert refill < 1000 * fps
        ev = r.frame(True, dt)
        assert 15 in ev, 'a full charge does not fire'
        out[fps] = (beam_frames, refill)
        print(f'   {fps:3d} fps: the beam is cast on {beam_frames} frames ({beam_frames * dt:.4f} s); '
              f'it can fire again after {refill} frames ({refill * dt:.1f} s)')
        stats['p1'][fps] = beam_frames
    # letting go early: the beam stops at once, the charge already spent stays spent
    dt = f32(1 / 30)
    r.tag_manager()
    for k in range(10):
        r.frame(True, dt)
    ev = r.frame(False, dt)
    left = r.tag()['amount']
    assert 16 in ev, 'letting go does not stop TAG'
    pressed = 0
    while 15 not in r.frame(True, dt):
        pressed += 1
    print(f'   let go after 10 frames at 30 fps: the beam stops, {left} of 10000 left; held down again it '
          f'refires only after {pressed} more frames ({pressed / 30:.1f} s)')
    # hot: the recharge slows, and at 144 fps it truncates to nothing
    for fps, heat in ((30, 1.0), (144, 0.6), (144, 1.0)):
        dt = f32(1.0 / fps)
        r.tag_manager(heat)
        while 16 not in r.frame(True, dt):
            pass
        a0 = r.tag()['amount']
        for _ in range(fps * 10):
            r.frame(False, dt)
        a1 = r.tag()['amount']
        rate = (a1 - a0) / 10.0
        when = f'{10000 / rate:.0f} s to refill' if rate > 0 else 'never refills'
        print(f'   heat {heat} at {fps} fps: {rate:.0f} charge a second, {when}')
        stats['hot'][(fps, heat)] = rate
    return out


# --- parts 2 and 3: the designation, the strike, the missiles --------------------------
H, R = 12.0, 3.0  # the target mech: a capsule of radius R round its axis from the ground to H


def capsule_hit(a, b, c):
    """The first point of segment a-b inside the capsule over c (x, z on the ground): (t, point, normal)."""
    lo, hi = (c[0], R, c[2]), (c[0], H - R, c[2])
    d = [b[i] - a[i] for i in range(3)]
    best = None
    # the side: the infinite vertical cylinder, kept between the caps' centres
    A = d[0] ** 2 + d[2] ** 2
    if A > 0:
        ox, oz = a[0] - c[0], a[2] - c[2]
        B = 2 * (ox * d[0] + oz * d[2])
        C = ox * ox + oz * oz - R * R
        disc = B * B - 4 * A * C
        if disc >= 0:
            t = (-B - math.sqrt(disc)) / (2 * A)
            y = a[1] + t * d[1]
            if 0 <= t <= 1 and lo[1] <= y <= hi[1]:
                p = [a[i] + t * d[i] for i in range(3)]
                best = (t, p, [(p[0] - c[0]) / R, 0.0, (p[2] - c[2]) / R])
    for s_ in (lo, hi):  # the caps
        o = [a[i] - s_[i] for i in range(3)]
        A2 = sum(x * x for x in d)
        B2 = 2 * sum(o[i] * d[i] for i in range(3))
        C2 = sum(x * x for x in o) - R * R
        disc = B2 * B2 - 4 * A2 * C2
        if A2 > 0 and disc >= 0:
            t = (-B2 - math.sqrt(disc)) / (2 * A2)
            if 0 <= t <= 1 and (best is None or t < best[0]):
                p = [a[i] + t * d[i] for i in range(3)]
                best = (t, p, [(p[i] - s_[i]) / R for i in range(3)])
    return best


def capsule_distance(p, c):
    """From p to the capsule's surface (0 inside) and the nearest surface point."""
    y = min(max(p[1], R), H - R)
    ax = (c[0], y, c[2])
    v = [p[i] - ax[i] for i in range(3)]
    n = math.sqrt(sum(x * x for x in v)) or 1e-9
    return max(0.0, n - R), [ax[i] + v[i] * R / n for i in range(3)]


class TagFly(fly.Fly):
    """The fly harness on a scripted world: flat ground (y = 0, no handler) and one mech."""

    def setup(self, rng):
        cpu = self.cpu
        self.new_session(rng)
        cpu.u32(0x9a0ec0, 0)
        cpu.u32(0x9a178c, f2i(1.0))
        cpu.write(0x9a0ec4, bytes(0x1c))
        for a in (0x9a17a8, 0x9a17ac, 0x9a0ebc):
            cpu.u32(a, 0)
        cpu.u32(0x72fcac, 0)
        cpu.u32(0x72fcb4, 0)
        probe.probe_fly_net(0, 0)
        self.T, self.G, self.S = self.fresh.pop(), self.fresh.pop(), self.fresh.pop()
        self.hd = self.zalloc(0x10)
        cpu.u32(self.hd, self.hd)
        cpu.u32(self.hd + 4, self.handler_fn)
        self.rows[self.hd] = (0, 0, f2i(1.0), True)
        for nd, hd, fl in ((self.T, self.hd, 0), (self.G, 0, 0x10000), (self.S, 0, 0)):
            cpu.u32(nd + 0x24, fl)
            cpu.u32(nd + 0xbc, hd)
            cpu.u32(nd + 0xc4, 0)
            pn = self.scene['port'].get(nd)
            pn.handler, pn.frame, pn.flags = hd, 0, fl
        self.world_nodes = [self.T, self.G]
        self.target = {'orig': [0.0, 0.0, -300.0], 'port': [0.0, 0.0, -300.0]}
        self.angles = [0, 0, 0]
        self.frame_no = 100
        self.side = 'orig'
        self.seed_rng = random.Random(11)
        out = (ctypes.c_int32 * 8)()
        self.globals(f32(1 / 30))
        self.both(lambda: cpu.call(0x5847c0, ecx=1),  # the pool's nodes and the impact lights, as at load
                  lambda seed: probe.probe_fly_op(5, (ctypes.c_int32 * 1)(1), seed, out), 'pool objects')

    def answer_rng(self, side, kind):
        self.side = side
        return super().answer_rng(side, kind)

    def make_list(self, rng, frm, to, kind):
        a, b = [i2f(x) for x in frm], [i2f(x) for x in to]
        c = self.target[self.side]
        hits = []
        h = capsule_hit(a, b, c)
        if h:
            hits.append((h[0], h[1], h[2], self.T))
        if (a[1] >= 0) != (b[1] >= 0):
            t = a[1] / (a[1] - b[1])
            hits.append((t, [a[0] + t * (b[0] - a[0]), 0.0, a[2] + t * (b[2] - a[2])], [0.0, 1.0, 0.0], self.G))
        if not hits:
            return 1, 0, 0, []
        recs = []
        for t, pt, nrm, node in hits:
            pt = [f32(x) for x in pt]
            d2 = f32(sum((pt[i] - a[i]) ** 2 for i in range(3)))
            recs.append([f2i(f32(x)) for x in nrm] + [f2i(x) for x in pt] + [0, f2i(d2), 0, node])
        near = min(range(len(recs)), key=lambda i: hits[i][0])
        if kind == 38:
            return 0, len(recs), None, recs
        return 0, near, len(recs), recs

    def splash_answer(self, rng, at, radius):
        p = [i2f(x) for x in at]
        d, pt = capsule_distance(p, self.target[self.side])
        if d > i2f(radius):
            return 1, []
        rec = [0, 0, 0] + [f2i(f32(x)) for x in pt] + [0, f2i(f32(d * d)), 0, self.T]
        return 0, [rec]

    def ground_answer(self, rng, at):
        return [1, 0, 0, f2i(1.0), 0, at[0], f2i(0.0), at[2], 0, 0, 0, 0]

    # --- one operation on both sides, compared -------------------------------------------
    def globals(self, dt):
        self.frame_no += 1
        g = [0] * 75
        g[0], g[1], g[2], g[3] = f2i(f32(dt)), self.frame_no, f2i(5.0), 3
        g[5], g[6], g[7] = f2i(1.0), f2i(1.0), f2i(1.0)
        g[10] = 1
        g[11] = self.T
        g[18], g[19] = 1, 1
        g[22], g[23] = f2i(30.0), f2i(f32(10.0 + self.frame_no * dt))
        for i in range(7):
            g[24 + 7 * i] = self.T
        g[73], g[74] = f2i(1.0), 1
        port = list(g)
        if BREAK == 5:
            port[0] = f2i(f32(dt * 1.01))
        probe.probe_fly_globals((ctypes.c_int32 * 75)(*[s32(x) for x in port]))
        cpu = self.cpu
        for addr, v in ((0x72c52c, g[0]), (0x72f338, g[1]), (0x72c538, g[2]), (0x74a328, g[3]),
                        (0x9a0ec0, g[4]), (0x9a17d0, g[5]), (0x9a17d8, g[6]), (0x9a17dc, g[7]), (0x9a17e0, g[8]),
                        (0x9a0eec, g[18]), (0x9a0ee8, g[19]), (0x9a17c4, g[20]), (0x9a17c0, g[21]),
                        (0x9a17a0, g[22]), (0x72c530, g[23]), (0x9a1788, 0), (0x9a1790, 0),
                        (0x9a17d4, g[73]), (0x9a17cc, g[74]), (0x9a0ee0, 0), (0x9a0ee4, 0)):
            cpu.u32(addr, v)

    def both(self, orig, port, label):
        seed = self.seed_rng.randrange(1 << 31)
        self.events = {'orig': [], 'port': []}
        self.calls = {}
        self.op_seed = seed
        self.rand = fly.MsvcRand(seed)
        orig()
        if getattr(self, 'orig_only', False):
            return self.events['orig']
        self.rand = fly.MsvcRand(seed)
        probe.probe_fly_seed(seed)
        port(seed)
        a_ev, b_ev = self.events['orig'], self.events['port']
        tol = FLIGHT_TOL
        first = fly.compare_events(a_ev, b_ev, tol, self.stats)
        if first is not None:
            raise SystemExit(f'{label}: the calls differ at {first} ({len(a_ev)} vs {len(b_ev)}):\n  original:\n    '
                             f'{fly.show(a_ev[max(0, first - 4):first + 4])}\n  port:\n    '
                             f'{fly.show(b_ev[max(0, first - 4):first + 4])}')
        buf = (ctypes.c_int32 * 4000)()
        for i in self.live_slots():
            a, fl = self.slot_fields(i)
            nb = probe.probe_fly_slot(i, buf)
            err = fly.compare_lists(f'{label}: slot {i}', a, list(buf[:nb]), fl, tol, self.stats)
            if err:
                raise SystemExit(err)
        for slot in self.beams:
            a, fl = self.beam_fields(slot)
            nb = probe.probe_fly_beam_dump(slot, buf)
            err = fly.compare_lists(f'{label}: beam {slot}', a, list(buf[:nb]), fl, fly.TOL, self.stats)
            if err:
                raise SystemExit(err)
        self.stats['calls'] += len(a_ev)
        return a_ev

    def live_slots(self):
        d = self.defs_base + 0x1f4 * self.names['TAG_MISSILE']
        out, p = [], self.u32(d + 0x7c)
        while p:
            out.append((p - self.pool_base) // 0x5d8)
            p = self.u32(p)
        return out

    def beam_make(self, slot=0):
        d = self.names['TAG']
        at, dp, zp = self.beam_at[slot]
        args = [d, self.S, 0, 3, slot, 1]

        def o():
            self.cpu.call(0x584a70, ecx=self.defs_base + 0x1f4 * d, edx=self.S, stack_args=(0, 0x77, at, dp, 1))
            self.beams[slot] = self.cpu.uc.reg_read(UC_X86_REG_EAX)
            self.beam_state[slot] = 0
        self.both(o, lambda seed: probe.probe_fly_beam(0, (ctypes.c_int32 * 6)(*args)), 'beam make')

    def beam_op(self, op, slot=0):  # 18 start, 19 stop, 20 free
        def o():
            self.cpu.call({18: 0x581230, 19: 0x581400, 20: 0x584b40}[op], ecx=self.beams[slot])
            if op == 20:  # gone before the beams are compared
                del self.beams[slot]
                del self.beam_state[slot]
            else:
                self.beam_state[slot] = 1 if op == 18 else 0
        self.both(o, lambda seed: probe.probe_fly_beam(op - 17, (ctypes.c_int32 * 1)(slot)), f'beam {op}')

    def aim(self, muzzle, to, slot=0):
        d = [to[i] - muzzle[i] for i in range(3)]
        n = math.sqrt(sum(x * x for x in d))
        v = [f2i(f32(x)) for x in muzzle] + [f2i(f32(x / n)) for x in d]
        at, dp, _ = self.beam_at[slot]
        self.cpu.write(at, struct.pack('<3I', *v[:3]))
        self.cpu.write(dp, struct.pack('<3I', *v[3:]))
        probe.probe_fly_beam_at(slot, (ctypes.c_int32 * 6)(*[s32(x) for x in v]))

    def tick(self, dt):
        self.globals(dt)
        out = (ctypes.c_int32 * 8)()
        return self.both(lambda: self.cpu.call(0x5823c0),
                         lambda seed: probe.probe_fly_op(1, (ctypes.c_int32 * 1)(0), seed, out), 'tick')

    def clear(self):
        """Every shot in the air gone (0x584fe0), between scenarios."""
        out = (ctypes.c_int32 * 8)()
        self.both(lambda: self.cpu.call(0x584fe0), lambda seed: probe.probe_fly_op(4, (ctypes.c_int32 * 1)(0), seed, out),
                  'kill all')

    def position(self, i):
        return struct.unpack('<3f', self.cpu.read(self.pool_base + 0x5d8 * i + 0x48, 12))

    def missiles(self):
        out = []
        for i in self.live_slots():
            b = self.pool_base + 0x5d8 * i
            out.append((i, struct.unpack('<3f', self.cpu.read(b + 0x48, 12)),
                        struct.unpack('<f', self.cpu.read(b + 0x58c, 4))[0]))
        return out


MUZZLE = (0.0, 8.0, 0.0)
# The CRT's acos against the x87's (test_weapons_fly.py) differ in the last bits;
# over hundreds of homing frames the difference grows to about 1e-3 of the value.
FLIGHT_TOL = float(os.environ.get('TAG_FLIGHT_TOL', '2e-3'))
_close = fly.close
# ...measured against at least 100 (metres, m/s): a position's or a velocity's
# small component carries the error of the whole vector (hundreds).
fly.close = lambda a, b, tol, floor=1.0: _close(a, b, tol, max(floor, 100.0))


def designate(f, fps, pattern, dist=300.0):
    """A fresh TAG beam cast frame by frame: 'T' on the mech, 'G' the ground in front of
    it, 'N' the sky. The frame (from 0) whose cast calls the strike, or None."""
    dt = f32(1.0 / fps)
    f.clear()
    f.target = {'orig': [0.0, 0.0, -dist], 'port': [0.0, 0.0, -dist]}
    f.beam_make()
    f.aim(MUZZLE, (0.0, 6.0, -dist))
    f.beam_op(18)
    hit = None
    for k, c in enumerate(pattern):
        to = {'T': (0.0, 6.0, -dist), 'G': (0.0, 0.0, -dist * 0.8), 'N': (0.0, 300.0, -dist)}[c]
        f.aim(MUZZLE, to)
        if BREAK == 3 and c == 'G':
            f.target['port'] = [0.0, 0.0, -dist * 0.8]  # the port's mech stands where the ground was
        f.tick(dt)
        f.target['port'] = list(f.target['orig'])
        if f.live_slots():
            hit = k
            break
    f.beam_op(19)
    f.beam_op(20)
    return hit


def fly_strike(f, fps, speed, heading, dist=300.0, max_s=30.0):
    """TAG held on a mech until the strike, then the four missiles flown while the mech
    moves at `speed` m/s along `heading` (x, z). The flights and the damage it took."""
    dt = f32(1.0 / fps)
    k = designate(f, fps, 'T' * 200, dist)
    assert k is not None
    unit = None
    if getattr(f, 'orig_only', False):
        # The fix's lock: the missiles home on the mech's live position and
        # velocity (the exe's homing, flown by the original's code).
        unit = f.zalloc(0x18)
        for i in f.live_slots():
            b = f.pool_base + 0x5d8 * i
            f.cpu.u32(b + 0x6c, 1)
            f.cpu.u32(b + 0x70, unit)
            f.cpu.u32(b + 0x74, unit + 12)
    spawned = {i: {'start': pos, 'path': [pos]} for i, pos, _ in f.missiles()}
    damage = []
    t = 0.0
    start = list(f.target['orig'])
    while f.live_slots() and t < max_s:
        t += dt
        for side in ('orig', 'port'):
            c = f.target[side]
            c[0] += speed * heading[0] * dt
            c[2] += speed * heading[1] * dt
        if BREAK == 4:
            f.target['port'][2] = f.target['orig'][2] - 2.0
        if unit:
            c = f.target['orig']
            f.cpu.write(unit, struct.pack('<6f', c[0], 0.0, c[2], speed * heading[0], 0.0, speed * heading[1]))
        ev = f.tick(dt)
        for e in ev:
            if e[0] == 44 and (e[1] & 0xffffffff) == f.hd:
                damage.append((t, e[2], i2f(e[-1])))
        now = {i: (pos, sp) for i, pos, sp in f.missiles()}
        for i, rec in spawned.items():
            if 'end' in rec:
                continue
            if i in now:
                rec['path'].append(now[i][0])
                rec['speed'] = now[i][1]
            else:
                rec['end'] = t
                rec['at'] = f.position(i)  # the hit point (0x586350 puts the shot there)
    return k, spawned, damage, start


def part23(stats, beam_frames):
    print('2. the designation (original 0x5814b0 / 0x5826b0 / 0x4afea0 / 0x4afed0, the port beside it)')
    f = TagFly()
    f.stats = stats
    f.setup(random.Random(5))
    for fps in FPS:
        L = beam_frames[fps]
        k = designate(f, fps, 'T' * L)
        assert k is not None, 'no strike'
        line = [f'   {fps:3d} fps: {k + 1} frames on the mech call the strike ({L} cast, {L - k - 1} to spare)']
        for name, pat in (('1 frame on the ground', 'T' * 5 + 'G' + 'T' * L),
                          ('2 frames on the ground', 'T' * 5 + 'GG' + 'T' * L),
                          ('3 frames on the ground', 'T' * 5 + 'GGG' + 'T' * L),
                          ('1 frame on the sky', 'T' * 5 + 'N' + 'T' * L)):
            k2 = designate(f, fps, pat[:L])
            line.append(f'{name}: ' + (f'strike on frame {k2 + 1}' if k2 is not None else 'no strike'))
        print(';\n      '.join(line))
        stats['need'][fps] = k + 1
    print('3. the strike (original 0x4afed0 / 0x4b4a40 / 0x585010 / 0x5823c0 / 0x585900 / 0x585cc0, the port beside it)')
    cases = [('standing', 0.0, (0.0, -1.0)), ('walking away at 5 m/s', 5.0, (0.0, -1.0)),
             ('walking away at 7 m/s', 7.0, (0.0, -1.0)), ('walking away at 10 m/s', 10.0, (0.0, -1.0)), ('crossing at 10 m/s', 10.0, (1.0, 0.0)),
             ('running away at 20 m/s', 20.0, (0.0, -1.0))]
    for fps in FPS:
        for name, speed, heading in cases:
            if fps != 30 and name not in ('standing', 'walking away at 10 m/s'):
                continue
            k, sp, dmg, start = fly_strike(f, fps, speed, heading)
            ends = [r.get('end') for r in sp.values()]
            ats = [r.get('at') for r in sp.values()]
            c = f.target['orig']
            miss = [capsule_distance(list(a), c)[0] if a else -1.0 for a in ats]
            total = sum(d for _, _, d in dmg)
            direct = sum(1 for _, _, d in dmg if d == 18.0)
            landed = [e for e in ends if e]
            print(f'   {fps:3d} fps, {name}: {len(landed)} of {len(sp)} missiles land after '
                  f'{min(landed, default=0):.2f}..{max(landed, default=0):.2f} s, '
                  f'{min(miss):.1f}..{max(miss):.1f} m from the mech; '
                  f'{len(dmg)} hits, {direct} direct, {total:.1f} damage')
            stats['strike'][(fps, name)] = (ends, miss, total)
            if name == 'standing' and fps == 30:
                r0 = next(iter(sp.values()))
                print(f'      spawned at {tuple(round(x) for x in r0["start"])} (the mech at {tuple(start)}); '
                      f'ends at {tuple(round(x, 1) for x in r0["at"])}')


def unit_strike():
    """TAG_UNIT=1: the strike's FIX (bug) measured with the original's own flight code:
    the four missiles' lock moved from the marked point to the mech's live position
    and velocity (what the port's fixed strike gives them), the mech walking."""
    print('the strike locked on the unit (the fix; flown by the original 0x5823c0 / 0x585900 / 0x585cc0)')
    f = TagFly()
    f.stats = {'p1': {}, 'hot': {}, 'need': {}, 'strike': {}, 'calls': 0, 'near': 0}
    f.setup(random.Random(5))
    f.orig_only = True
    for fps in FPS:
        for name, speed, heading in (('standing', 0.0, (0.0, -1.0)), ('walking away at 10 m/s', 10.0, (0.0, -1.0)),
                                     ('crossing at 10 m/s', 10.0, (1.0, 0.0)),
                                     ('running away at 20 m/s', 20.0, (0.0, -1.0))):
            k, sp, dmg, start = fly_strike(f, fps, speed, heading)
            ends = [r.get('end') for r in sp.values() if r.get('end')]
            direct = sum(1 for _, _, d in dmg if d == 18.0)
            print(f'   {fps:3d} fps, {name}: {len(ends)} of {len(sp)} land after {min(ends, default=0):.2f}..'
                  f'{max(ends, default=0):.2f} s; {len(dmg)} hits, {direct} direct, {sum(d for _, _, d in dmg):.1f} damage')
            if len(dmg) < 4 or sum(d for _, _, d in dmg) < 4 * 18.0 * 0.99:
                raise SystemExit(f'{fps} fps, {name}: not every missile hit the marked mech')
    print('ok: every missile hits the marked mech, standing or moving')


def main():
    if os.environ.get('TAG_UNIT') == '1':
        unit_strike()
        return
    stats = {'p1': {}, 'hot': {}, 'need': {}, 'strike': {}, 'calls': 0, 'near': 0}
    if os.environ.get('TAG_PARTS', '123') != '23':
        part1(stats)
    else:
        stats['p1'] = {30: 32, 60: 62, 144: 146}
    part23(stats, stats['p1'])
    if BREAK:
        raise SystemExit(f'break test {BREAK} ({BREAKS[BREAK]}) was not caught')
    print(f'ok: original and port agree ({stats["calls"]} calls out of zWeapon matched)')


if __name__ == '__main__':
    # Out with os._exit: with both harnesses' emulators and the probe's
    # callbacks in one process, the interpreter's own shutdown hangs on macOS.
    code = 0
    try:
        main()
    except SystemExit as e:
        if e.code not in (None, 0):
            print(e.code if isinstance(e.code, str) else f'exit {e.code}')
            code = 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(code)

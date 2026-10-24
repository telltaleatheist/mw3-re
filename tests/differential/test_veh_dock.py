"""The repair dock's states and the field bases' drive against the original
x86 (docs/vehicles.md, The repair dock; mech3/mech_fieldbase_dock): every
slot of the seven states in the registry at 0x605ae0 (idle 0x45a740..,
contact 0x45acb0.., places 0x45ab50.., deploy 0x45af00.., repair 0x45b2f0..,
close 0x45b460.., reset 0x45b6c0..; their bases' slots 0x45b7a0 / 0x45b7b0 /
0x45b9b0, the line 0x45b7d0, the unwinds), the registry's helpers 0x45a720,
0x45b740, 0x45a500, 0x45a540, and the drive 0x4598d0 / 0x459990 / 0x459ac0
/ 0x459af0 / 0x45b9c0 / 0x459a80 with the arms' aim 0x4589a0 and the
direction 0x51e3d0.

    cmake --build build && .venv/bin/python tests/differential/test_veh_dock.py [runs]

The registry, its three bases and their step lists are laid out in the
original's memory as the port's; the calls out are stubbed (the radio line
0x4e6d50, 0x47a720, the valves, positions 0x5a85e0, the dock point's
vt+0x18, the requester's vt+0xa0, the view's camera 0x40ae70, the mech's
rest 0x418e90 and repair 0x4154e0, the effects 0x508c10 / 0x506ca0, the dock
request 0x45a780 (step 4b2), a base's stopping distance vt+0x124). Each run
sets a random registry and calls a random sequence of slots; compared: the
traces, the answers, the registry (its list of states, fields, each state's
own), the globals [0x605c4c] [0x60b4cc] [0x60b4d0], each base's steps (the
drives by base and way) and the drive's fields, throttle and arms.

The departures (FIX (bug)): a missing base (fewer than three in a mission;
none shipped) is followed by the exe in the drives' queueing and the places'
check; the port passes it. The runs keep three bases.
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import CALLBACK, Emu, Replay, Trace, compare, load_probe, rnd  # noqa: E402

U, F = ctypes.c_uint32, ctypes.c_float
REG = 0x605ae0
STATES = [(0xbc, 0x5c25e0), (0xd0, 0x5c25c0), (0xe0, 0x5c2620), (0xf4, 0x5c2640), (0x110, 0x5c25a0),
          (0x124, 0x5c2580), (0x140, 0x5c2560)]
NAMES = ['idle', 'contact', 'places', 'deploy', 'repair', 'close', 'reset']
BASE_VT = 0x5c23d8
COVER = {}


def cover(label, cond=True):
    COVER.setdefault(label, 0)
    if cond:
        COVER[label] += 1


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def bf(u):
    return struct.unpack('<f', struct.pack('<I', u & 0xffffffff))[0]


class World:
    """The registry, three bases and the stubs, in the original's memory."""

    def __init__(self, seed):
        r = self.r = rnd(seed, 'dock')
        t = self.t = Trace(seed=seed)
        e = self.e = Emu(t)
        e.heap_stubs()
        e.cpu.fpu_control(0x007f)
        e.cpu.write(REG, b'\0' * 0x160)
        self.sent = e.world_alloc(0x10)
        e.w32(REG + 4, self.sent)
        for off, vt in STATES:
            e.w32(REG + off, vt)
            e.w32(REG + off + 8, REG)
        # A float answer (st0) for a stub: the thunk loads it and returns.
        self.fscratch = e.world_alloc(0x10)
        self.fthunk = e.thunk(bytes([0xd9, 0x05]) + struct.pack('<I', self.fscratch) + bytes([0xc3]))
        # The three bases: a copy of the field base's vtable with vt+0x124 a
        # float stub; controls with the base's SetThrottle; arms; a step list.
        vt = e.world_alloc(0x170)
        e.cpu.write(vt, bytes(e.cpu.read(BASE_VT, 0x16c)))

        def stopping(em, key, a, pop):
            v = em.t.handle(key, a)
            em.w32(self.fscratch, v)
            em._ret(0, pop, then=self.fthunk)
        e.w32(vt + 0x124, e.new_stub('veh.vt+0x124', 'c', 0, fn=stopping))
        self.bases = []
        for i in range(3):
            b = e.world_alloc(0x160)
            e.cpu.write(b, b'\0' * 0x160)
            e.w32(b, vt)
            ctl = e.world_alloc(0xa70)
            e.cpu.write(ctl, b'\0' * 0xa70)
            vb, ivt = e.world_alloc(0x10), e.world_alloc(0x100)
            e.w32(ctl + 4, vb)
            e.w32(vb + 4, 0xa04)
            e.w32(ctl + 0xa08, ivt)
            e.w32(ivt + 0x20, 0x4a5b70)
            e.w32(ivt + 0x54, e.new_stub(f'ictl{i}.vt+0x54', 'c', 0))
            e.w32(b + 0xe4, ctl)
            arms = e.world_alloc(0x90)
            e.cpu.write(arms, b'\0' * 0x90)
            e.w32(b + 0x13c, arms)
            ls = e.world_alloc(0xc)
            e.w32(ls, ls)
            e.w32(ls + 4, ls)
            e.w32(b + 0x100, ls)
            inn = e.world_alloc(0x10)
            e.w32(b + 0x110, inn)
            pos = e.world_alloc(0x10)
            self.bases.append(dict(addr=b, ctl=ctl, ictl=ctl + 0xa08, arms=arms, list=ls, inn=inn, pos=pos))
        self.placeholder = e.world_alloc(0x10)
        # The valves: each holder's value object with its get (vt+0x04).
        self.valves = []
        for i in range(3):
            v, vv = e.world_alloc(0x10), e.object(f'vv{i}', slots={4: ('', 0)})
            e.w32(v + 4, vv)
            self.valves.append(v)
        self.stubs()

    def stubs(self):
        e, t = self.e, self.t
        e.stub(0x4e6d50, spec='c,s0')
        e.stub(0x47a720, spec='c')
        e.stub(0x4d3c00, spec='c,s0')
        pos = {b['addr']: b['pos'] for b in self.bases}

        def position(em, key, a, pop):
            em.t.handle(key, a)
            p = pos.get(a[0], self.req_pos)
            em.t.override(key, p)
            em._ret(p, pop)
        e.stub(0x5a85e0, spec='c', fn=position)
        self.point = e.object('point', slots={0x18: ('c', 0), 0x24: ('c', 0)})
        self.point_name = e.world_string('mcb_point_a')
        t.script['point.vt+0x24'] = self.point_name
        # The request's: the waynet ([0x60b544], its element +0x20), the
        # brains (an order deque's finish iterator at +0x174 past one order whose
        # part +0x0c answers its net), nets, elements, behaviours.
        self.waynet = e.world_alloc(0x40)
        e.stub(0x58b060, spec='c,s0')
        e.stub(0x4d3d20, spec='zc')
        e.stub(0x4040d0, spec='c')
        e.stub(0x4c84b0, spec='c,s0,s1')
        part_vt, net_vt, elem_vt, beh_vt = (e.world_alloc(0x100) for _ in range(4))
        e.w32(part_vt + 8, e.new_stub('part.vt+0x8', 'c', 0))
        e.w32(net_vt + 8, e.new_stub('net.vt+0x8', 'c,zs0', 4))
        e.w32(elem_vt + 0x60, e.new_stub('elem.vt+0x60', 'c', 0))
        e.w32(elem_vt + 0x74, e.new_stub('elem.vt+0x74', 'c', 0))
        e.w32(beh_vt + 4, e.new_stub('beh.vt+0x4', 'c', 0))
        e.w32(beh_vt + 8, e.new_stub('beh.vt+0x8', 'c,s0', 4))
        self.vts = dict(part=part_vt, net=net_vt, elem=elem_vt, beh=beh_vt)
        self.brains, self.parts = [], []
        for i in range(3):
            br, blk, mp, part = e.world_alloc(0x300), e.world_alloc(0xf70), e.world_alloc(0x10), e.world_alloc(0x10)
            e.w32(mp, blk)
            e.w32(br + 0x174, blk)           # the finish iterator: first, last, cur, node
            e.w32(br + 0x178, blk + 0xf70)
            e.w32(br + 0x17c, blk + 0x98)
            e.w32(br + 0x180, mp)
            e.w32(blk + 0xc, part)
            e.w32(part, part_vt)
            self.brains.append(br)
            self.parts.append(part)
        self.nets = []
        for i in range(2):
            n = e.world_alloc(0x10)
            e.w32(n, net_vt)
            self.nets.append(n)
        self.elems = []
        for i in range(6):
            x = e.world_alloc(0x10)
            e.w32(x, elem_vt)
            self.elems.append(x)
        self.behs = []
        for i in range(3):
            x = e.world_alloc(0x10)
            e.w32(x, beh_vt)
            self.behs.append(x)
        # The multiplayer's: its game, the stations, a mech.
        self.mpgame = e.world_alloc(0x100)
        e.stub(0x479480, spec='')
        t.script['0x00479480'] = self.mpgame
        self.stations = e.object('stations', slots={0x14: ('c,s0', 4)})
        self.station = e.object('station', slots={0x18: ('c', 0)})
        self.station_pos = e.world_alloc(0x10)
        t.script['station.vt+0x18'] = self.station_pos
        e.w32(self.station + 0x2c, e.world_string('mfb_point_1'))
        e.stub(0x46ff90, spec='c')

        def nearest(em, key, a, pop):
            st, p = em.args('c,s0')
            v = em.t.handle(key, (st, em.r32(p), em.r32(p + 4), em.r32(p + 8)))
            em._ret(v, pop)
        e.stub(0x4d51d0, spec='c,s0', fn=nearest)
        e.stub(0x4d6b90, spec='c,s0')
        e.stub(0x4e6ff0, spec='c')

        def message(em, key, a, pop):
            fmt, arg = em.args('zs1,zs2')
            text = fmt.replace('%s', arg if isinstance(arg, str) else '') if '%s' in fmt else fmt
            em._ret(em.t.handle(key, (text,)), pop)
        e.stub(0x40fe20, spec='', pop=0, fn=message)
        self.mech = e.object('mech', slots={0xa4: ('c', 0), 0x5c: ('c', 0)})
        e.stub(0x50b450, spec='zc')
        e.stub(0x4f7fe0, spec='c,zd')
        e.stub(0x508f00, spec='c,d,s0')
        e.stub(0x41cc30, spec='c,s0')
        self.mech_pos = e.world_alloc(0x10)
        pos[self.mech] = self.mech_pos
        self.pilot_blk = e.world_alloc(0x100)
        e.w32(self.pilot_blk + 0x6c, e.object('game', slots={0x2c: ('', 0)}))
        e.w32(0x60861c, self.pilot_blk)
        self.point_pos = e.world_alloc(0x10)
        t.script['point.vt+0x18'] = self.point_pos
        self.req = e.object('req', slots={0xa0: ('c', 0)})
        self.req_pos = e.world_alloc(0x10)
        self.other = e.world_alloc(0x100)
        e.w32(self.other, e.r32(self.req))  # another mech: the same slots
        self.view = e.world_alloc(0x100)
        e.w32(0x5f4c8c, self.view)
        self.cam = e.world_alloc(0x10)
        e.stub(0x40ae70, spec='c,s0')

        def rest(em, key, a, pop):
            c, p = em.args('c,s0')
            em._ret(em.t.handle(key, (c, em.r32(p), em.r32(p + 4), em.r32(p + 8))), pop)
        e.stub(0x418e90, spec='c,s0', fn=rest)
        e.stub(0x4154e0, spec='c,s0')
        e.stub(0x508c10, spec='c,d,s0,s1,s2')
        t.script['0x00508c10'] = e.world_alloc(0x10)
        e.stub(0x506ca0, spec='c')

    def randomize(self):
        """A random registry, bases and globals, the same on both sides."""
        r, e, t = self.r, self.e, self.t
        bw, bf_, u = [], [], []
        for i, b in enumerate(self.bases):
            active = int(r.random() < 0.85)
            node = e.world_alloc(0x10)
            pre = r.choice([0, 0, 1, 2])
            nodes = []
            prev = b['list']
            for _ in range(pre):
                n = e.world_alloc(0xc)
                e.w32(n + 8, self.placeholder)
                nodes.append(n)
            chain = [b['list']] + nodes
            for j, n in enumerate(chain):
                e.w32(n, chain[(j + 1) % len(chain)])
                e.w32(n + 4, chain[j - 1])
            del prev
            e.w32(b['addr'] + 0x104, pre)
            bn = int(r.random() < 0.3)
            e.w8(b['addr'] + 8, active)
            e.w32(b['addr'] + 4, node)
            e.w8(b['addr'] + 0x108, bn)
            e.w32(b['addr'] + 0x15c, 0)
            at = [f32(r.uniform(-50, 50)) for _ in range(3)]
            inn = [f32(r.uniform(-50, 50)) for _ in range(3)]
            p = [f32(at[k] + r.uniform(-25, 25)) for k in range(3)]
            for k in range(3):
                e.wf(b['addr'] + 0x114 + 4 * k, at[k])
                e.wf(b['inn'] + 4 * k, inn[k])
                e.wf(b['pos'] + 4 * k, p[k])
            now = f32(r.choice([0.0, 1.0, r.uniform(0, 1), 0.5, float('nan')]))
            tgt = r.choice([0.0, 1.0])
            vz = f32(r.uniform(-3, 3))
            thr = f32(r.uniform(-1, 1))
            e.wf(b['arms'] + 0x8c, now)
            e.wf(b['arms'] + 0x88, tgt)
            e.wf(b['ctl'] + 0x78, vz)
            e.wf(b['ctl'] + 8, thr)
            bw += [b['addr'], active, node, pre, bn]
            bf_ += at + inn + [now, tgt, vz, thr]
            e.w32(REG + 0x14 + 4 * i, b['addr'])
        count = r.choice([3, 3, 3, 2])
        valves = [v if r.random() < 0.8 else 0 for v in self.valves]
        for i in range(3):
            e.w32(REG + 0x20 + 4 * i, valves[i])
            t.script[f'vv{i}.vt+0x4'] = r.choice([0, 0, 1])
        e.w32(REG + 0x10, count)
        requester = r.choice([self.req, self.req, 0])
        e.w32(REG + 0x74, requester)
        t.script['req.vt+0xa0'] = r.choice([0, 1, 1])
        busy = int(r.random() < 0.3)
        e.w8(REG + 0x78, busy)
        e.w32(REG + 0x7c, self.point)
        camera = r.choice([-1, 0, 2, 4])
        e.w32(REG + 0x80, camera & 0xffffffff)
        dv = e.world_alloc(0x10)
        e.w32(REG + 0x84, dv)
        sounds = [e.world_alloc(0x10) if r.random() < 0.85 else 0 for _ in range(11)]
        for i, s in enumerate(sounds):
            e.w32(REG + 0x8c + 4 * i, s)
        effects = e.world_alloc(0x10) if r.random() < 0.8 else 0
        e.w32(0x605c44, effects)
        idle_on = int(r.random() < 0.5)
        e.w8(REG + 0xb8, idle_on)
        begin_next = int(r.random() < 0.3)
        e.w8(REG + 0xc, begin_next)
        # The states' list.
        n = r.choice([0, 1, 2, 3, 6, 7])
        ids = [r.randrange(7) for _ in range(n)]
        if n >= 6:
            ids = sorted(r.sample(range(7), n), key=lambda i: [0, 2, 1, 3, 4, 5, 6][i])
        nodes = [e.world_alloc(0xc) for _ in ids]
        chain = [self.sent] + nodes
        for j, nd in enumerate(chain):
            e.w32(nd, chain[(j + 1) % len(chain)])
            e.w32(nd + 4, chain[j - 1])
        for nd, i in zip(nodes, ids):
            e.w32(nd + 8, REG + STATES[i][0])
        e.w32(REG + 8, n)
        u = [count] + valves + [requester, busy, self.point, camera & 0xffffffff, dv] + sounds + [effects, idle_on,
                                                                                                  begin_next, n]
        u += ids + [0] * (8 - n)
        for off, _ in STATES:
            fin = int(r.random() < 0.3)
            snd = r.choice(sounds)
            e.w8(REG + off + 4, fin)
            e.w32(REG + off + 0xc, snd)
            u += [fin, snd]
        asked = int(r.random() < 0.5)
        e.w8(REG + 0xbc + 0x10, asked)
        until = r.choice([0, 5, 100, 0x7fffffff])
        e.w32(REG + 0xe0 + 0x10, until)
        timer = f32(r.choice([12.0, 0.05, r.uniform(0, 12)]))
        e.wf(REG + 0x110 + 0x10, timer)
        now = f32(r.uniform(0, 300))
        e.wf(0x60991c, now)
        e.w32(0x72f338, r.choice([0, 3, 50, 200]))
        quiet = f32(r.choice([0.0, now - 1, now, now, now, now + 2, 3.4028235e38]))
        e.wf(0x605c4c, quiet)
        contact, drive = int(r.random() < 0.5), int(r.random() < 0.5)
        e.w8(0x60b4cc, contact)
        e.w8(0x60b4d0, drive)
        u += [asked, until, fb(timer), fb(quiet), contact, drive]
        e.w32(0x60b544, e.world_alloc(0x10))
        # The req's place and the dock point's: near or not.
        pp = [f32(r.uniform(-50, 50)) for _ in range(3)]
        dist = r.choice([r.uniform(0, 9), r.uniform(9, 13), r.uniform(13, 50)])
        dv_ = [r.uniform(-1, 1) for _ in range(3)]
        n_ = sum(x * x for x in dv_) ** 0.5 or 1.0
        rr = [f32(pp[k] + dv_[k] / n_ * dist) for k in range(3)]
        for k in range(3):
            e.wf(self.point_pos + 4 * k, pp[k])
            e.wf(self.req_pos + 4 * k, rr[k])
        # The view: the requester's or another's, with or without its camera.
        e.w32(self.view + 0x64, r.choice([requester, self.other, self.mech]))
        e.w32(self.view + 0x58, self.cam if r.random() < 0.8 else 0)
        e.w32(self.cam + 4, r.choice([0, 2, 4, 4]))
        # Drives already made (deploy's, close's).
        for off in (0xf4, 0x124):
            mask = 0
            for i in range(3):  # all three: the exe's step and abort follow them unchecked
                if True:
                    d = e.world_alloc(0x18)
                    e.cpu.write(d, b'\0' * 0x18)
                    e.w32(d, 0x5c2540)
                    e.w32(d + 8, self.bases[i]['addr'])
                    e.w8(d + 0xc, int(off == 0xf4))
                    fin = int(r.random() < 0.4)
                    e.w8(d + 4, fin)
                    mask |= (1 << i) | (fin << (4 + i))
                    e.w32(REG + off + 0x10 + 4 * i, d)
                else:
                    e.w32(REG + off + 0x10 + 4 * i, 0)
            u.append(mask)
        # The request's answers.
        e.w32(0x60b544, self.waynet if r.random() < 0.85 else 0)
        e.w32(self.waynet + 0x20, r.randrange(4))
        t.script['0x0058b060'] = [r.choice([self.point, self.point, 0]) for _ in range(4)]
        t.script['point.vt+0x24'] = self.point_name if r.random() < 0.9 else 0
        t.script['0x004d3d20'] = r.choice([0, e.world_alloc(0x10)])
        self.brain_on = [r.random() < 0.85 for _ in range(3)]
        for i in range(3):
            t.script[f'ictl{i}.vt+0x54'] = self.brains[i] if self.brain_on[i] else 0
        t.script['part.vt+0x8'] = [r.choice(self.nets) for _ in range(3)]  # never none: the exe follows it
        t.script['net.vt+0x8'] = [r.choice(self.elems + [0]) for _ in range(3)]
        t.script['elem.vt+0x60'] = [r.choice(self.elems + [self.elems[0], 0]) for _ in range(8)]
        t.script['elem.vt+0x74'] = [r.choice(self.behs + [0]) for _ in range(8)]
        t.script['beh.vt+0x4'] = [r.choice([0, 1, 2, 3, 3]) for _ in range(3)]
        t.script['beh.vt+0x8'] = [r.choice(self.elems + [0]) for _ in range(3)]
        # The multiplayer's state.
        e.w8(self.mpgame + 0xa8, int(r.random() < 0.8))
        pv = self.mech
        e.w32(self.pilot_blk + 0x14, pv if r.random() < 0.9 else 0)
        e.w8(pv + 8, int(r.random() < 0.9))
        e.w8(pv + 0xde, int(r.random() < 0.8))
        e.w8(pv + 0xdd, int(r.random() < 0.5))
        e.w32(0x609980, self.stations if r.random() < 0.85 else 0)
        t.script['0x004d51d0'] = self.station if r.random() < 0.85 else 0
        sp = [f32(r.uniform(-50, 50)) for _ in range(3)]
        dist = r.choice([r.uniform(0, 6), r.uniform(6.4, 6.6), r.uniform(7, 18), r.uniform(18.5, 20.5), 30.0])
        dv_ = [r.uniform(-1, 1) for _ in range(3)]
        n_ = sum(x * x for x in dv_) ** 0.5 or 1.0
        for k in range(3):
            e.wf(self.station_pos + 4 * k, sp[k])
            e.wf(self.mech_pos + 4 * k, f32(sp[k] + dv_[k] / n_ * dist))
        self.contact = e.world_alloc(0x20)
        e.wf(self.contact + 0xc, f32(r.choice([50.0, 9500.0, 9999.0, 10001.0, 1e6])))
        t.script['0x004d6b90'] = self.contact if r.random() < 0.75 else 0
        t.script['0x004e6ff0'] = self.mech if r.random() < 0.8 else 0
        t.script['mech.vt+0xa4'] = e.world_string('Owens')
        t.script['mech.vt+0x5c'] = r.choice([0, 0, 1])
        t.script['0x0050b450'] = e.world_alloc(0x10) if r.random() < 0.9 else 0
        t.script['0x004f7fe0'] = e.world_alloc(0x10) if r.random() < 0.9 else 0
        t.script['game.vt+0x2c'] = r.choice([1, 3])
        e.w32(self.view + 0x5c, r.choice([0, 2]))
        anim = r.choice([0, e.world_alloc(0x10)])
        e.w32(0x605c48, anim)
        self.anim0 = anim
        return bw, bf_, u

    def points_image(self):
        e = self.e
        out = [e.r32(REG + 0x2c + 4 * i) for i in range(18)]
        for b in self.bases:
            a = b['addr']
            out += [e.r32(a + 0x114 + 4 * k) for k in range(3)]
            for off in (0x10c, 0x110):
                q = e.r32(a + off)
                i = (q - (REG + 0x2c)) // 12
                out.append(i + 1 if 0 <= i < 6 and (q - (REG + 0x2c)) % 12 == 0 else 0)
        out += [e.r32(0x605c48), e.r8(self.mech + 0xdd)]
        return out

    def setup_probe(self, probe, bw, bfl, u):
        probe.probe_dk_setup((U * 15)(*bw), (F * 30)(*bfl), (U * len(u))(*u))
        probe.probe_dk_brains((U * 3)(*self.brains), (U * 3)(*self.parts), (U * 3)(*[b['ictl'] for b in self.bases]))
        probe.probe_dk_set_anim(self.anim0)

    def image(self):
        e = self.e
        out = []
        ids = {REG + off: i for i, (off, _) in enumerate(STATES)}
        lst = []
        nd = e.r32(self.sent)
        while nd != self.sent:
            lst.append(ids.get(e.r32(nd + 8), 99))
            nd = e.r32(nd)
        out.append(e.r32(REG + 8))
        out += (lst + [0xffffffff] * 8)[:8]
        out += [e.r8(REG + 0xc), e.r32(REG + 0x74), e.r8(REG + 0x78), e.r32(REG + 0x80), e.r8(REG + 0xb8)]
        for off, _ in STATES:
            out += [e.r8(REG + off + 4), e.r32(REG + off + 0xc)]
        out += [e.r8(REG + 0xcc), e.r32(REG + 0xf0), e.r32(REG + 0x120)]
        addrs = [b['addr'] for b in self.bases]
        for off in (0xf4, 0x124):
            mask = 0
            for i in range(3):
                d = e.r32(REG + off + 0x10 + 4 * i)
                if d:
                    b = e.r32(d + 8)
                    mask |= (1 << i) | ((addrs.index(b) + 1 if b in addrs else 0) << (4 + 2 * i))
            out.append(mask)
        out += [e.r32(0x605c4c), e.r8(0x60b4cc), e.r8(0x60b4d0)]
        for b in self.bases:
            a = b['addr']
            out += [e.r32(a + 0x104), e.r8(a + 0x108), e.r32(a + 0x15c)]
            steps = []
            nd = e.r32(b['list'])
            while nd != b['list']:
                s = e.r32(nd + 8)
                if s == self.placeholder:
                    steps.append(0xff)
                else:
                    sb = e.r32(s + 8)
                    steps.append(0x100 | (0x10 if e.r8(s + 0xc) else 0) | (addrs.index(sb) if sb in addrs else 0xf))
                nd = e.r32(nd)
            out += (steps + [0xffffffff] * 4)[:4]
        return out


SLOTS = {0: 0x0, 1: 0x4, 2: 0xc, 3: 0x10, 4: 0x14, 5: 0x18}
SPECS = {0: (), 1: ('dt',), 2: (), 3: ('m',), 4: (), 5: ()}


def call_slot(w, which, slot, dt, m):
    e = w.e
    this = REG + STATES[which][0]
    fn = e.r32(e.r32(this) + SLOTS[slot])
    stack = (fb(dt),) if slot == 1 else (m,) if slot == 3 else ()
    v = e.call(fn, ecx=this, stack=stack)
    return v & (0xffffffff if slot == 3 else 0xff)


def run_states(probe, seed):
    w = World(seed)
    r, e, t = w.r, w.e, w.t
    bw, bfl, u = w.randomize()
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    w.setup_probe(probe, bw, bfl, u)
    seq = []
    for _ in range(r.choice([1, 2, 3, 5])):
        kind = r.random()
        if kind < 0.85:
            seq.append(('s', r.randrange(7), r.randrange(6)))
        else:
            seq.append(('h', r.randrange(4)))
    name = f'states seed {seed} {seq}'

    def unmade(i):  # the deploy's or close's drives freed by its end: the exe never runs it then
        return i in (3, 5) and e.r32(REG + STATES[i][0] + 0x10) == 0

    def front():
        nd = e.r32(w.sent)
        return next((i for i, (off, _) in enumerate(STATES) if nd != w.sent and e.r32(nd + 8) == REG + off), None)
    for k, step in enumerate(seq):
        if step[0] == 's' and step[2] in (1, 4) and unmade(step[1]):
            step = ('s', step[1], 5)
        if step[0] == 's' and (step[1], step[2]) in ((1, 1), (3, 0)) and not e.r32(REG + 0x7c):
            step = ('s', step[1], 5)  # only run after a request found the dock point
        if step[0] == 'h' and step[1] == 3 and front() is not None and unmade(front()):
            step = ('h', 0)
        dt = f32(r.choice([0.033, 0.1, 1.0, 13.0]))
        m = r.choice([w.req, w.other, 0])
        if step[0] == 's':
            want = call_slot(w, step[1], step[2], dt, m)
            got = probe.probe_dk_call(step[1], step[2], dt, m)
            if step[2] in (0, 2, 4):  # no answer
                want = got = 0
            cover(f'{NAMES[step[1]]} slot {step[2]}')
            if step[2] in (1, 5):
                cover(f'{NAMES[step[1]]} slot {step[2]} -> {int(want != 0)}')
        else:
            fn = {0: 0x45a720, 1: 0x45b740, 2: 0x45a500, 3: 0x45a540}[step[1]]
            want = e.call(fn, ecx=REG, stack=(m,) if step[1] in (1, 2) else ()) & (0xff if step[1] < 2 else 0)
            got = probe.probe_dk_helper(step[1], m) & (0xff if step[1] < 2 else 0)
            cover(f'helper {step[1]}')
        if want != got:
            print(f'{name}: call {k} answers original {want:#x} rebuild {got:#x}')
            return False
    if not compare(name, t.log, rep.log):
        return False
    want = w.image()
    out = (U * 80)()
    n = probe.probe_dk_image(out)
    got = list(out)[:n]
    if want != got:
        bad = next((i for i in range(min(len(want), len(got))) if want[i] != got[i]), None)
        print(f'{name}: image word {bad} original {[hex(x) for x in want]}\n  rebuild {[hex(x) for x in got]}')
        return False
    if not points_match(w, probe, name):
        return False
    keys = {k for k, _ in t.log}
    cover('request: sent a base', '0x004c84b0' in keys)
    cover('request: queued', any(x == 6 for x in want[1:9]))
    cover('said', '0x004e6d50' in keys)
    cover('view switched', '0x0040ae70' in keys)
    cover('mech at rest', '0x00418e90' in keys)
    cover('repaired', '0x004154e0' in keys)
    cover('effects', '0x00508c10' in keys)
    cover('powered-down asked', 'req.vt+0xa0' in keys)
    cover('valve read', any(k.startswith('vv') for k in keys))
    return True


def points_match(w, probe, name):
    want = w.points_image()
    out = (U * 64)()
    n = probe.probe_dk_points(w.mech, out)
    got = list(out)[:n]
    if want != got:
        bad = next((i for i in range(min(len(want), len(got))) if want[i] != got[i]), None)
        print(f'{name}: points word {bad} original {[hex(x) for x in want]}\n  rebuild {[hex(x) for x in got]}')
        return False
    return True


def run_more(probe, seed):
    """The frame (both paths), the dock points, the station's repair, its
    message and its end."""
    w = World(seed)
    r, e, t = w.r, w.e, w.t
    bw, bfl, u = w.randomize()
    which = r.choice([0, 0, 0, 1, 2, 3, 4])
    dt = f32(r.choice([0.033, 0.1]))
    at = [f32(r.uniform(-50, 50)) for _ in range(3)]
    use_at = r.random() < 0.7
    atb = e.world_alloc(0x10)
    for k in range(3):
        e.wf(atb + 4 * k, at[k])
    sid = r.randrange(5)
    if which == 3:  # the station by its id
        t.script['0x0058b060'] = [r.choice([w.station, w.station, 0])]
    anim = r.choice([w.anim0, e.world_alloc(0x10)])
    stopped = r.choice([0, 0, 1])
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    w.setup_probe(probe, bw, bfl, u)
    name = f'more seed {seed} (op {which})'
    if which == 0:
        e.call(0x45a270, ecx=REG, stack=(fb(dt),))
        probe.probe_dk_more(0, dt, None, 0, 0, 0)
        cover('frame: states', t.script['game.vt+0x2c'] != 3)
        cover('frame: multiplayer', t.script['game.vt+0x2c'] == 3)
    elif which == 1:
        e.call(0x45a560, ecx=REG, stack=(atb if use_at else 0,))
        probe.probe_dk_more(1, 0, (F * 3)(*at) if use_at else None, 0, 0, 0)
        cover('points', True)
    elif which == 2:
        if not e.r32(0x609980):
            return True  # the exe tells no stations (vt+0x14 on null); only stations ask
        st = r.choice([w.station, w.station, 0])
        m = r.choice([w.mech, w.mech, 0])
        want = e.call(0x45a180, ecx=m, edx=st) & 0xff
        got = probe.probe_dk_more(2, 0, None, m, st, 0) & 0xff
        if want != got:
            print(f'{name}: repair answered original {want} rebuild {got}')
            return False
        cover(f'station repair -> {want}')
    elif which == 3:
        if not e.r32(0x609980):
            return True  # the exe follows no stations (0x0058b060 on null)
        e.call(0x45a140, ecx=w.mech, edx=sid)
        probe.probe_dk_more(3, 0, None, w.mech, sid, 0)
        cover('station message')
    else:
        e.call(0x45a0f0, ecx=anim, edx=w.mech, stack=(stopped,))
        probe.probe_dk_more(4, 0, None, anim, w.mech, stopped)
        cover('station done', anim == w.anim0 and anim and not stopped)
    if not compare(name, t.log, rep.log):
        return False
    keys = {k for k, _ in t.log}
    cover('mp: too far', any(k == '0x0040fe20' and a[0].startswith('too far') for k, a in t.log))
    cover('mp: too close', any(k == '0x0040fe20' and 'too close' in a[0] for k, a in t.log))
    cover('mp: repaired at a station', '0x00508f00' in keys)
    want = w.image()
    out = (U * 80)()
    n = probe.probe_dk_image(out)
    if want != list(out)[:n]:
        print(f'{name}: image original {[hex(x) for x in want]}\n  rebuild {[hex(x) for x in list(out)[:n]]}')
        return False
    return points_match(w, probe, name)


def run_drive(probe, seed):
    w = World(seed)
    r, e, t = w.r, w.e, w.t
    bw, bfl, u = w.randomize()
    i = r.randrange(3)
    b = w.bases[i]
    d = e.world_alloc(0x18)
    e.cpu.write(d, b'\0' * 0x18)
    e.w32(d, 0x5c2540)
    e.w32(d + 8, b['addr'])
    opn = int(r.random() < 0.5)
    fin, reached = int(r.random() < 0.15), int(r.random() < 0.2)
    way = r.choice([-1.0, 1.0])
    travel = f32(r.choice([r.uniform(-20, 20), r.uniform(-0.3, 0.3), r.choice([-0.22, 0.22, -0.18, 0.18])]))
    e.w8(d + 4, fin)
    e.w8(d + 0xc, opn)
    e.w8(d + 0xd, reached)
    e.wf(d + 0x10, way)
    e.wf(d + 0x14, travel)
    # The drive at the front of its base's steps.
    head = b['list']
    nd = e.world_alloc(0xc)
    first = e.r32(head)
    e.w32(nd, first)
    e.w32(nd + 4, head)
    e.w32(nd + 8, d)
    e.w32(first + 4, nd)
    e.w32(head, nd)
    e.w32(b['addr'] + 0x104, e.r32(b['addr'] + 0x104) + 1)
    t.script['veh.vt+0x124'] = [fb(f32(r.choice([r.uniform(0, 8), r.uniform(0, 0.1), 0.0]))) for _ in range(20)]
    rep = Replay(t, e, e.snapshot())
    probe.probe_app_set_callback(rep.callback())
    w.setup_probe(probe, bw, bfl, u)
    probe.probe_dv_make(i, opn, fin, reached, way, travel)
    seq = [r.choice([0, 0, 1, 2, 1, 2, 5, 4 if r.random() < 0.2 else 1]) for _ in range(r.choice([1, 3, 6, 10]))]
    name = f'drive seed {seed} (base {i}, open {opn}, {seq})'
    vts = {0: 0x0, 1: 0x4, 2: 0x8, 4: 0x14, 5: 0x18}
    for k, slot in enumerate(seq):
        dt = f32(r.choice([0.033, 0.1, 0.5]))
        if slot == 2:  # the speed changes between frames
            vz = f32(r.uniform(-3, 3))
            e.wf(b['ctl'] + 0x78, vz)
            probe.probe_dv_speed(i, vz)
        if slot == 1 and r.random() < 0.3:  # the arms moved on
            now = f32(r.choice([0.0, 1.0, r.uniform(0, 1)]))
            e.wf(b['arms'] + 0x8c, now)
            probe.probe_dv_arms(i, now)
        fn = e.r32(0x5c2540 + vts[slot])
        want = e.call(fn, ecx=d, stack=(fb(dt),) if slot in (1, 2) else ()) & 0xff
        got = probe.probe_dv_call(slot, dt) & 0xff
        if slot in (1, 5) and want != got:
            print(f'{name}: slot {slot} at {k} original {want} rebuild {got}')
            return False
        cover(f'drive slot {slot}')
        if slot == 4:
            break
    if not compare(name, t.log, rep.log):
        return False
    want = [e.r8(d + 4), e.r8(d + 0xd), e.r32(d + 0x10), e.r32(d + 0x14), e.r32(b['arms'] + 0x88),
            e.r32(b['ctl'] + 8), e.r32(b['addr'] + 0x104), e.r8(b['addr'] + 0x108)]
    got = (U * 8)()
    probe.probe_dv_image(i, got)
    if want != list(got):
        print(f'{name}: original {[hex(x) for x in want]} rebuild {[hex(x) for x in got]}')
        return False
    cover('drive reached', bool(e.r8(d + 0xd)))
    cover('drive out', bool(opn))
    cover('drive in', not opn)
    return True


def main():
    probe = load_probe()
    probe.probe_app_set_callback.argtypes = [CALLBACK]
    P = ctypes.c_void_p
    probe.probe_dk_setup.argtypes = [P, P, P]
    probe.probe_dk_call.argtypes = [ctypes.c_int32, ctypes.c_int32, F, U]
    probe.probe_dk_call.restype = U
    probe.probe_dk_helper.argtypes = [ctypes.c_int32, U]
    probe.probe_dk_helper.restype = U
    probe.probe_dk_image.argtypes = [P]
    probe.probe_dv_make.argtypes = [ctypes.c_int32] * 4 + [F, F]
    probe.probe_dv_call.argtypes = [ctypes.c_int32, F]
    probe.probe_dv_call.restype = U
    probe.probe_dv_image.argtypes = [ctypes.c_int32, P]
    probe.probe_dv_speed.argtypes = [ctypes.c_int32, F]
    probe.probe_dv_arms.argtypes = [ctypes.c_int32, F]
    probe.probe_dk_brains.argtypes = [P, P, P]
    probe.probe_dk_more.argtypes = [ctypes.c_int32, F, P, U, U, U]
    probe.probe_dk_more.restype = U
    probe.probe_dk_set_anim.argtypes = [U]
    probe.probe_dk_points.argtypes = [U, P]
    probe.probe_dk_hooks(1)
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    runs = int(args[0]) if args else 600
    for seed in range(runs):
        if not run_states(probe, seed):
            sys.stdout.flush()
            os._exit(1)
    for seed in range(runs // 2):
        if not run_drive(probe, seed):
            sys.stdout.flush()
            os._exit(1)
    for seed in range(runs):
        if not run_more(probe, seed):
            sys.stdout.flush()
            os._exit(1)
    missed = sorted(k for k, n in COVER.items() if not n)
    want = [f'{NAMES[i]} slot {s}' for i in range(7) for s in range(6)] + [f'helper {i}' for i in range(4)]
    want += [f'{NAMES[i]} slot {s} -> {v}' for i in (1, 2, 3, 4, 5) for s in (1, 5) for v in (0, 1)]
    want += [f'{NAMES[0]} slot 1 -> {v}' for v in (0, 1)] + ['reset slot 1 -> 1']
    missed += [k for k in want if k not in COVER]
    if missed:
        print(f'not reached: {missed}')
        sys.stdout.flush()
        os._exit(1)
    print(f'ok: {runs} state runs and {runs // 2} drive runs as the original ({len(COVER)} branches reached)')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()

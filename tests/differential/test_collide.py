"""Mech collision against the original x86 (docs/movement-collision.md):
Collide 0x4225c0 whole, with the probe sweep 0x422d20, the query and sort
0x422ef0 / 0x423010, the responses (wall 0x4231c0 with reflect 0x51e280,
ceiling 0x423600, vehicle 0x4237c0, mech 0x423a20, touch 0x423c90), the bump
off a nearby mech, the network's collision record, TurnRate 0x418f70; and
the probe setup 0x422360.

    cmake --build build && .venv/bin/python tests/differential/test_collide.py [trials]

A mech in emulator memory with a copy of the real vtable and IControls
vtable, its skis, parts that log their damage, other mechs, vehicles,
touchable nodes and AiTarget contacts. The segment query (ported and
checked in test_di) answers both sides with scripted crossings: walls,
floors, ceilings, other mechs' and vehicles' nodes, touchable objects, at
the probes the sweep picked. Everything outside the mech code is a logging
stub on both sides. Each trial is one Collide (or probe setup); the mech's
fields, the probes, the other mechs and vehicles it shoved, the skis and
every call out must match bit for bit at 24-bit precision.
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
from original import FTOL, IAT_FTOL, ROOT, Original  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P, F, I, U = ctypes.c_void_p, ctypes.c_float, ctypes.c_int32, ctypes.c_uint32
probe.probe_collide_setup.argtypes = [P, P, P, I, P, P, P, P, I, I]
probe.probe_collide_record.argtypes = [I, I, I, P]
probe.probe_collide.argtypes = [P, I, F, P, P, U, P, U, P, P, P, P, P]
probe.probe_collider_setup.argtypes = [P, I, F, F, P, P]

COLLIDE, SETUP = 0x4225c0, 0x422360
MECH_VT, CTL_VT, MECH = 0x5bcef8, 0x5bd428, 0x700
IAT_MALLOC, IAT_FREE = 0x5bb664, 0x5bb65c
FRAME = 0x72f338
NNODES = 12
MECH_NODE = 11


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def u2f(x):
    return struct.unpack('<f', struct.pack('<I', x & 0xffffffff))[0]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


class Emulated:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.import_code(IAT_FTOL, FTOL)
        a = cpu.alloc
        self.heap = a(0x400000)
        self.heap_next = self.heap
        cpu.stub_import(IAT_MALLOC, lambda c: self.malloc(c.arg(0)))
        cpu.stub_import(IAT_FREE, lambda c: 0)
        self.mech, self.mvt, self.ctl, self.vbt, self.ivt = a(MECH), a(0x200), a(0x74), a(8), a(0x60)
        self.pilot, self.skis, self.transform, self.pos, self.footv = a(0x40), a(0x300), a(48), a(12), a(12)
        self.parts = {k: a(0x100) for k in ('torso', 'head', 'hip', 'leg0', 'leg1', 'foot0')}
        self.nodes = [a(0xd4) for _ in range(NNODES)]
        self.handlers = [a(0x10) for _ in range(NNODES)]
        self.others = [a(MECH) for _ in range(2)]
        self.other_pilots = [a(0x40) for _ in range(2)]
        self.other_torsos = [a(0x100) for _ in range(2)]
        self.veh = [a(0x40) for _ in range(2)]
        self.veh_target = [a(0x20) for _ in range(2)]
        self.veh_ai = [a(0x20) for _ in range(2)]
        self.veh_body = [a(0xa00) for _ in range(2)]
        self.veh_team = [a(0x100) for _ in range(2)]
        self.veh_mat = [a(48) for _ in range(2)]
        self.contacts = [a(0x40) for _ in range(4)]
        self.cvec = a(0x20)
        self.cobj, self.cobj2 = a(0x20), a(0x20)
        self.ai = a(0x20)
        self.net = a(0x40)
        self.view, self.player_obj, self.world_obj, self.ffobj = a(0x80), a(0x20), a(0x10), a(0x10)
        self.fbuf = a(0x10)   # floats the float-returning stubs load
        self.recs = a(13 * 0x504)

        def vt(n=0x200):
            return a(n)

        def vstub(table, off, fn, pop):
            f = cpu.code(b'\xc3')
            cpu.u32(table + off, f)
            cpu.stub_function(f, fn, pop)

        cpu.write(self.mvt, cpu.read(MECH_VT, 0x200))
        vstub(self.mvt, 0x18, self.set_position, 4)
        vstub(self.mvt, 0x140, lambda c: self.ai if self.s['ai'] else 0, 0)
        aivt = vt(0x100)
        cpu.u32(self.ai, aivt)
        vstub(aivt, 0x70, lambda c: self.s['ai_rev'], 0)
        cpu.write(self.ivt, cpu.read(CTL_VT, 0x60))
        # parts: apply_damage (vt +0x38) logs
        codes = {'torso': 1, 'head': 2, 'leg0': 3, 'leg1': 4}
        for k, code in codes.items():
            t = vt(0x80)
            cpu.u32(self.parts[k], t)
            vstub(t, 0x38, lambda c, code=code: self.log(20, code, c.arg(0), c.arg(1)), 8)
        for i in range(2):
            t = vt(0x80)
            cpu.u32(self.other_torsos[i], t)
            vstub(t, 0x38, lambda c, i=i: self.log(20, 5 + i, c.arg(0), c.arg(1)), 8)
            ovt = vt(0x200)
            cpu.u32(self.others[i], ovt)
            vstub(ovt, 0x28, lambda c: c.ecx, 0)
            vstub(ovt, 0x58, lambda c, i=i: self.s['other_remote'][i], 0)
        # vehicles
        for i in range(2):
            evt = vt(0x200)
            cpu.u32(self.veh[i], evt)
            vstub(evt, 0xec, lambda c, i=i: self.veh_target[i] if self.s['veh'][i]['target'] else 0, 0)
            vstub(evt, 0x140, lambda c, i=i: self.veh_ai[i], 0)
            vstub(evt, 0xf4, lambda c, i=i: self.veh_team[i], 0)
            avt = vt(0x80)
            cpu.u32(self.veh_ai[i], avt)
            vstub(avt, 0x5c, lambda c, i=i: self.veh_body[i] if self.s['veh'][i]['body'] else 0, 0)
            tvt = vt(0x200)
            cpu.u32(self.veh_target[i], tvt)
            vstub(tvt, 0x160, lambda c, i=i: self.log(11, i, c.arg(2)) or self.s['veh'][i]['killed'], 0xc)
            mvt = vt(0x80)
            cpu.u32(self.veh_team[i], mvt)
            vstub(mvt, 0x28, lambda c: c.ecx, 0)
            cpu.u32(self.veh[i] + 0xc, self.veh_mat[i])
        # contacts
        ovt, ovt2 = vt(0x100), vt(0x100)
        cpu.u32(self.cobj, ovt)
        cpu.u32(self.cobj2, ovt2)
        vstub(ovt, 0x40, lambda c: self.cobj2, 0)
        vstub(ovt2, 0xf4, lambda c: self.cur_target, 0)
        stubs = {
            0x40ef10: (self.node_mech, 0),
            0x40ef00: (self.node_entity, 0),
            0x4efcd0: (self.segments, 8),
            0x5a85e0: (lambda c: self.pos, 0),
            0x432990: (lambda c: self.log(9, c.arg(0), c.arg(1), c.arg(4)) or 0x9000, 0x14),
            0x4335d0: (lambda c: self.log(8, *struct.unpack('<3I', c.read(c.arg(0), 12)), c.arg(1)), 8),
            0x4622a0: (lambda c: self.log(7, c.arg(0)), 4),
            0x470320: (lambda c: self.log(10, self.others.index(c.arg(0)), c.arg(1), c.arg(2), c.arg(3)), 0x10),
            0x4e6ff0: (self.contact_mech, 0),
            0x4e6fe0: (self.contact_target, 0),
            0x4ecf70: (lambda c: 0, 0),
        }
        for addr, (fn, pop) in stubs.items():
            cpu.stub_function(addr, fn, pop)
        # float-returning stand-ins: fld [fbuf + k]; ret
        cpu.write(0x4e7030, bytes.fromhex('d905') + struct.pack('<I', self.fbuf) + b'\xc3')
        cpu.uc.hook_add(UC_HOOK_CODE, self.contact_dist, begin=0x4e7030, end=0x4e7030)
        cpu.write(0x5836b0, bytes.fromhex('d905') + struct.pack('<I', self.fbuf + 4) + bytes.fromhex('c20400'))
        cpu.uc.hook_add(UC_HOOK_CODE, self.touch, begin=0x5836b0, end=0x5836b0)
        cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, ad, sz, d: self.log(14, self.cpu.edx), begin=0x4eb6d0, end=0x4eb6d0)
        cpu.u32(0x608c84, self.world_obj)
        cpu.u32(0x5f4c8c, self.view)
        cpu.u32(0x60861c, self.player_obj)

    def malloc(self, n):
        p = self.heap_next
        self.heap_next += (n + 0xf) & ~0xf
        return p

    def log(self, *e):
        self.events.append(len(e))
        self.events.extend(v & 0xffffffff for v in e)
        return 0

    def set_position(self, c):
        x = c.read(c.arg(0), 12)
        self.log(15, *struct.unpack('<3I', x))
        c.write(self.pos, x)
        return 0

    def node_index(self, p):
        return self.nodes.index(p) if p in self.nodes else -1

    def node_mech(self, c):
        n = self.node_index(struct.unpack('<I', c.read(c.ecx, 4))[0])
        return self.others[self.s['nodes'][n]['idx']] if n >= 0 and self.s['nodes'][n]['kind'] == 1 else 0

    def node_entity(self, c):
        n = self.node_index(struct.unpack('<I', c.read(c.ecx, 4))[0])
        return self.veh[self.s['nodes'][n]['idx']] if n >= 0 and self.s['nodes'][n]['kind'] == 2 else 0

    def segments(self, c):
        pts, out = c.arg(0), c.arg(1)
        segs = struct.unpack(f'<{3 * pts}I', c.read(c.edx, 12 * pts))
        self.log(13, pts, *segs)
        k = min(self.calls, 1)
        self.calls += 1
        for i in range(pts // 2):
            rec = bytearray(0x504)
            hits = self.s['recs'][k][i]
            rec[0] = len(hits)
            for j, (n, nrm, pt) in enumerate(hits):
                struct.pack_into('<3f', rec, 4 + 0x28 * j, *nrm)
                struct.pack_into('<3f', rec, 4 + 0x28 * j + 0xc, *pt)
                struct.pack_into('<I', rec, 4 + 0x28 * j + 0x24, self.nodes[n] if n >= 0 else 0)
            c.write(out + 0x504 * i, bytes(rec))
        return 0

    def contact_mech(self, c):
        k = self.s['contacts'][self.contacts.index(c.ecx)]['mech']
        return 0 if k < 0 else self.mech if k == 0 else self.others[k - 1]

    def contact_target(self, c):
        t = self.s['contacts'][self.contacts.index(c.ecx)]['target']
        self.cur_target = t
        return self.cobj if t else 0

    def contact_dist(self, uc, ad, sz, d):
        k = self.contacts.index(self.cpu.ecx)
        self.cpu.write(self.fbuf, struct.pack('<f', self.s['contacts'][k]['dist']))

    def touch(self, uc, ad, sz, d):
        c = self.cpu
        e = c.edx
        n = self.node_index(struct.unpack('<I', c.read(e + 0x24, 4))[0])
        frm = struct.unpack('<3I', c.read(c.ecx, 12))
        self.log(12, n, *frm, c.arg(0))
        c.write(self.fbuf + 4, struct.pack('<f', self.s['nodes'][n]['touch']))

    def run(self, s):
        cpu = self.cpu
        self.s, self.events, self.calls, self.cur_target = s, [], 0, 0
        self.heap_next = self.heap
        m = bytearray(s['mech'])
        cpu.write(self.mech, bytes(m))
        cpu.u32(self.mech, self.mvt)
        cpu.u32(self.mech + 0x4, self.nodes[MECH_NODE])
        cpu.u32(self.mech + 0xc, self.transform)
        cpu.write(self.transform, struct.pack('<12f', *s['transform']))
        cpu.write(self.pos, struct.pack('<3f', *s['pos']))
        cpu.u32(self.mech + 0x37c, self.ctl)
        cpu.u32(self.mech + 0x684, self.pilot)
        cpu.u32(self.pilot + 0xc, s['roster'])
        cpu.u32(self.mech + 0x41c, self.skis)
        cpu.write(self.skis + 0x2f0, struct.pack('<f', s['half']))
        cpu.write(self.skis + 0x140, struct.pack('<7f', *s['skis_vy']))
        for off, k in ((0x39c, 'torso'), (0x3a4, 'head'), (0x3a0, 'hip'), (0x38c, 'leg0'), (0x390, 'leg1')):
            cpu.u32(self.mech + off, self.parts[k])
        cpu.u32(self.parts['leg0'] + 0x78, self.parts['foot0'])
        cpu.u32(self.parts['foot0'] + 0x18, self.footv)
        cpu.write(self.footv, struct.pack('<3f', *s['foot']))
        # the controls (throttle in the image's spare slot 0x6d0)
        cpu.write(self.ctl, bytes(0x74))
        cpu.write(self.ctl + 0x10, struct.pack('<f', u2f(struct.unpack_from('<I', m, 0x6d0)[0])))
        cpu.u32(self.ctl + 4, self.vbt)
        cpu.u32(self.vbt + 4, 0x5c)
        cpu.u32(self.ctl + 0x60, self.ivt)
        cpu.u32(self.ctl + 0x70, self.mech)
        # nodes
        for i, n in enumerate(s['nodes']):
            nb = bytearray(0xd4)
            flags = 0x100 if n['has_box'] else 0
            if i == MECH_NODE:
                flags = struct.unpack_from('<I', m, 0x6d4)[0]
            struct.pack_into('<I', nb, 0x24, flags)
            struct.pack_into('<6f', nb, 0x74, *n['box'])
            if n['kind'] == 3:
                struct.pack_into('<I', nb, 0xbc, self.handlers[i])
                cpu.u32(self.handlers[i] + 0xc, 1)
            cpu.write(self.nodes[i], bytes(nb))
        # other mechs
        for i in range(2):
            o = self.others[i]
            cpu.write(o + 4, bytes(MECH - 4))
            cpu.u32(o + 0x684, self.other_pilots[i])
            cpu.u32(self.other_pilots[i] + 0xc, s['other_roster'][i])
            cpu.u32(o + 0xc0, s['other_team'][i])
            cpu.write(o + 0x30, struct.pack('<3f', *s['other_vel'][i]))
            cpu.u32(o + 0x39c, self.other_torsos[i])
        # vehicles
        for i, v in enumerate(s['veh']):
            cpu.write(self.veh[i] + 8, bytes([v['alive']]))
            cpu.u32(self.veh_team[i] + 0xc0, v['team'])
            cpu.write(self.veh_body[i] + 0x64, struct.pack('<6f', *v['velocity'], *v['spin']))
            cpu.write(self.veh_body[i] + 0x9b0, struct.pack('<f', v['mass']))
            cpu.write(self.veh_mat[i], struct.pack('<12f', *v['matrix']))
        # contacts
        cs = s['contacts']
        for i, ct in enumerate(cs):
            cpu.write(self.contacts[i] + 0xc, struct.pack('<f', ct['dist2']))
            cpu.write(self.contacts[i] + 0x20, struct.pack('<3f', *ct['dir']))
            cpu.u32(self.cvec + 4 * i, self.contacts[i])
        cpu.u32(self.mech + 0xac, self.cvec if cs else 0)
        cpu.u32(self.mech + 0xb0, self.cvec + 4 * len(cs) if cs else 0)
        # the network record
        if s['flags'] & 8:
            cpu.write(self.net, s['net'])
            cpu.u32(self.mech + 0xd8, self.net)
        else:
            cpu.u32(self.mech + 0xd8, 0)
        cpu.u32(FRAME, s['frame'])
        cpu.u32(self.view + 0x64, self.mech if s['flags'] & 1 else 0)
        cpu.u32(self.player_obj + 0x14, self.mech if s['flags'] & 2 else 0)
        cpu.u32(0x608620, self.ffobj if s['flags'] & 4 else 0)
        for off in (0xfc, 0x100, 0x104, 0x108, 0x10c, 0x110, 0x114):
            cpu.u32(self.mech + off, 0)
        if s['op'] == 0:
            cpu.call(COLLIDE, ecx=self.mech)
            r = cpu.uc.reg_read(19) & 0xff
        else:
            cpu.write(self.nodes[MECH_NODE] + 0xa4, struct.pack('<6f', *s['box']))
            cpu.call(SETUP, ecx=self.mech + 0xfc, stack_args=(self.mech,))
            r = 0
        img = cpu.read(self.mech, MECH)
        img = bytearray(img)
        struct.pack_into('<I', img, 0x6d0, struct.unpack('<I', cpu.read(self.ctl + 0x10, 4))[0])
        struct.pack_into('<I', img, 0x6d4, struct.unpack('<I', cpu.read(self.nodes[MECH_NODE] + 0x24, 4))[0])
        att = struct.unpack_from('<I', img, 0x664)[0]
        struct.pack_into('<I', img, 0x664, 1 if att == self.others[0] else 2 if att == self.others[1] else 0)
        others = []
        for o in self.others:
            v = list(struct.unpack('<3I', cpu.read(o + 0x30, 12)))
            a = struct.unpack('<I', cpu.read(o + 0x664, 4))[0]
            others += v + [1 if a == self.mech else 0]
        vehs = []
        for b in self.veh_body:
            vehs += list(struct.unpack('<6I', cpu.read(b + 0x64, 24)))
        extra = list(struct.unpack('<7I', cpu.read(self.skis + 0x140, 28))) + \
            list(struct.unpack('<3I', cpu.read(self.pos, 12)))
        return r, bytes(img), others, vehs, extra, self.events


FIELDS = ([0x30 + 4 * k for k in range(3)] + [0x3c + 4 * k for k in range(3)] + [0x58 + 4 * k for k in range(3)] +
          [0x5a0 + 4 * k for k in range(3)] + [0x594 + 4 * k for k in range(3)] + [0xfc] +
          [0x124 + 4 * k for k in range(33)] +
          [0x1d8 + 4 * k for k in range(45)] + [0x28c + 4 * k for k in range(45)] + [0x5ec, 0x664, 0x6d0, 0x6d4])


def compare_image(img):
    return [struct.unpack_from('<I', img, o)[0] for o in FIELDS] + list(img[0x374:0x378])


def port(s):
    nodes = []
    for n in s['nodes']:
        nodes += [float(n['kind']), float(n['idx']), n['touch'], float(n['has_box'])] + list(n['box'])
    veh = []
    for v in s['veh']:
        veh += [float(v['alive']), float(v['target']), float(v['body']), float(v['killed']),
                u2f(v['team'])] + list(v['velocity']) + list(v['spin']) + [v['mass']] + list(v['matrix'])
    cts = []
    for c in s['contacts']:
        cts += [float(c['mech']), float(c['target']), c['dist2'], c['dist']] + list(c['dir']) + [0.0]
    fa = lambda xs: (ctypes.c_float * max(1, len(xs)))(*xs)  # noqa: E731
    ia = lambda xs: (ctypes.c_int32 * len(xs))(*xs)  # noqa: E731
    ua = lambda xs: (ctypes.c_uint32 * len(xs))(*xs)  # noqa: E731
    probe.probe_collide_setup(fa(nodes), fa(veh), fa(cts), len(s['contacts']), ia(s['other_roster']),
                              ua(s['other_team']), fa([x for v in s['other_vel'] for x in v]),
                              ia(s['other_remote']), s['ai'], s['ai_rev'])
    for k in range(2):
        for i, hits in enumerate(s['recs'][k]):
            flat = []
            for (n, nrm, pt) in hits:
                flat += [float(n)] + list(nrm) + list(pt)
            probe.probe_collide_record(k, i, len(hits), fa(flat))
    img = ctypes.create_string_buffer(s['mech'], MECH)
    tr, pos, vy = fa(s['transform']), fa(s['pos']), fa(s['skis_vy'])
    others, vehs = (ctypes.c_float * 8)(), (ctypes.c_float * 12)()
    out, n = (ctypes.c_int32 * 20000)(), ctypes.c_int32(20000)
    netb = ctypes.create_string_buffer(s['net'], 0x40)
    if s['op'] == 0:
        r = probe.probe_collide(img, s['roster'], s['half'], tr, pos, s['flags'], netb, s['frame'], others, vehs, vy,
                                out, ctypes.byref(n))
    else:
        r = probe.probe_collider_setup(img, s['roster'], s['half'], 0.0, fa(s['foot']),
                                       fa([x for b in [s['box']] for x in b]))
        n.value = 0
    ob = [struct.unpack('<I', struct.pack('<f', x))[0] for x in others]
    for i in (3, 7):
        ob[i] = struct.unpack('<I', struct.pack('<f', others[i]))[0]
    vb = [struct.unpack('<I', struct.pack('<f', x))[0] for x in vehs]
    extra = [struct.unpack('<I', struct.pack('<f', x))[0] for x in list(vy) + list(pos)]
    return r, img.raw, ob, vb, extra, [v & 0xffffffff for v in out[:n.value]]


def scenario(rng):
    mech = bytearray(MECH)
    def put(off, v):
        struct.pack_into('<f', mech, off, f32(v))
    yaw = rng.uniform(-math.pi, math.pi)
    for k in range(3):
        put(0x30 + 4 * k, rng.choice([0.0, rng.uniform(-30, 30)]))
        put(0x3c + 4 * k, rng.choice([0.0, rng.uniform(-30, 30)]))
        put(0x5a0 + 4 * k, rng.choice([0.0, 0.0, rng.uniform(-5, 5)]))
        put(0x594 + 4 * k, rng.choice([0.0, rng.uniform(0, 3)]))
    put(0x5c, rng.choice([0.0, rng.uniform(-1.5, 1.5)]))
    mech[0x584] = rng.random() < 0.3
    put(0x518, rng.choice([60.0, 86.4, 108.0]))
    put(0x520, rng.choice([0.8, 1.2]))
    put(0x6b0, rng.choice([1 / 30, 0.05, 0.1]))
    put(0x118, 0.1)
    put(0x11c, 0.01)
    struct.pack_into('<I', mech, 0x120, 11)
    struct.pack_into('<I', mech, 0xc4, rng.choice([0, 1, 2, 3]))
    struct.pack_into('<I', mech, 0x5ec, rng.choice([0, 0x7777]))
    put(0x6d0, rng.uniform(-1, 1))   # the controls' throttle
    struct.pack_into('<I', mech, 0x6d4, rng.choice([0x1c, 0x0c, 0x31c]))   # the mech node's flags
    c, s_ = math.cos(yaw), math.sin(yaw)
    pos = [f32(rng.uniform(-1000, 1000)), f32(rng.uniform(0, 60)), f32(rng.uniform(-1000, 1000))]
    transform = [f32(c), 0.0, f32(-s_), 0.0, 1.0, 0.0, f32(s_), 0.0, f32(c)] + pos
    half = f32(rng.choice([6.0, 2.0]))
    # the probes as the setup leaves them, and where last frame's ended
    box = [f32(rng.uniform(-4, -2)), 0.0, f32(rng.uniform(-6, -3)), f32(rng.uniform(2, 4)), f32(rng.uniform(8, 16)),
           f32(rng.uniform(3, 6))]
    foot = [f32(rng.uniform(1, 2.5)), 0.0, 0.0]
    cx, cy, cz = (box[0] + box[3]) * 0.5, (box[1] + box[4]) * 0.4, (box[2] + box[5]) * 0.5
    zf, zb = -half * 1.1, half * 1.1
    local = [(box[0], cy, zf), (cx, cy, zf), (box[3], cy, zf), (box[0], cy, zb), (cx, cy, zb), (box[3], cy, zb),
             (foot[0], 0.75, box[2]), (-foot[0], 0.75, box[2]), (foot[0], 0.75, -box[2]), (-foot[0], 0.75, -box[2]),
             (cx, box[4], cz)]
    for i, p in enumerate(local):
        for k in range(3):
            put(0x124 + 12 * i + 4 * k, p[k])
    move = [rng.uniform(-1.5, 1.5), rng.uniform(-0.5, 0.5), rng.uniform(-1.5, 1.5)]
    for i, p in enumerate(local):
        w = [p[0] * transform[0] + p[2] * transform[6] + pos[0], p[1] + pos[1], p[0] * transform[2] + p[2] * transform[8] + pos[2]]
        for k in range(3):
            put(0x28c + 12 * i + 4 * k, w[k] - move[k])
    nodes = []
    for i in range(NNODES):
        kind = rng.choice([0, 0, 0, 1, 2, 3]) if i < MECH_NODE else 0
        b = [f32(rng.uniform(-5, 0)), f32(rng.uniform(-5, 0)), f32(rng.uniform(-5, 0)),
             f32(rng.uniform(0, 5)), f32(rng.uniform(0, 5)), f32(rng.uniform(0, 5))]
        nodes.append({'kind': kind, 'idx': rng.randrange(2), 'touch': f32(rng.choice([0.0, 20.0, -1.0, 5.0])),
                      'has_box': int(rng.random() < 0.7), 'box': b})
    def hit():
        n = rng.choice(list(range(MECH_NODE)) + [-1])
        kind = rng.choice(['wall', 'wall', 'floor', 'ceiling', 'slope'])
        if kind == 'wall':
            a = rng.uniform(-math.pi, math.pi)
            nrm = [math.cos(a), rng.uniform(-0.5, 0.5), math.sin(a)]
        elif kind == 'floor':
            nrm = [rng.uniform(-0.2, 0.2), 1.0, rng.uniform(-0.2, 0.2)]
        elif kind == 'ceiling':
            nrm = [rng.uniform(-0.2, 0.2), -1.0, rng.uniform(-0.2, 0.2)]
        else:
            nrm = [rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)]
        if rng.random() < 0.05:
            nrm = [0.0, nrm[1], 0.0]
        ln = math.sqrt(sum(x * x for x in nrm)) or 1.0
        nrm = [f32(x / ln) for x in nrm]
        pt = [f32(pos[0] + rng.uniform(-8, 8)), f32(pos[1] + rng.uniform(0, 15)), f32(pos[2] + rng.uniform(-8, 8))]
        return (n, nrm, pt)
    recs = [[[hit() for _ in range(rng.choice([0, 0, 0, 1, 1, 2, 3]))] for _ in range(12)] for _ in range(2)]
    if rng.random() < 0.3:
        recs[0] = [[] for _ in range(12)]
    if rng.random() < 0.5:
        recs[1] = [[] for _ in range(12)]
    veh = []
    for i in range(2):
        a = rng.uniform(-math.pi, math.pi)
        veh.append({'alive': int(rng.random() < 0.8), 'target': int(rng.random() < 0.9), 'body': int(rng.random() < 0.9),
                    'killed': int(rng.random() < 0.2), 'team': rng.choice([0, 1, 2, 4]),
                    'velocity': [f32(rng.uniform(-5, 5)) for _ in range(3)],
                    'spin': [f32(rng.uniform(-1, 1)) for _ in range(3)], 'mass': f32(rng.choice([0.5, 1.0, 2.0])),
                    'matrix': [f32(math.cos(a)), 0.0, f32(-math.sin(a)), 0.0, 1.0, 0.0, f32(math.sin(a)), 0.0,
                               f32(math.cos(a)), 0.0, 0.0, 0.0]})
    contacts = []
    for _ in range(rng.choice([0, 0, 1, 2, 3, 4])):
        d = rng.uniform(1, 12)
        a = rng.uniform(-math.pi, math.pi)
        contacts.append({'mech': rng.choice([-1, 0, 1, 2]), 'target': int(rng.random() < 0.5), 'dist2': f32(d * d),
                         'dist': f32(d), 'dir': [f32(math.cos(a)), 0.0, f32(math.sin(a))]})
    flags = sum(b << i for i, b in enumerate([rng.random() < 0.4, rng.random() < 0.4, rng.random() < 0.5,
                                               rng.random() < 0.3]))
    if rng.random() < 0.3:
        flags |= 8
    frame = rng.randrange(100, 1000)
    net = bytearray(0x40)
    struct.pack_into('<II', net, 0, rng.choice([frame, frame - 1]), rng.choice([frame - 1, frame - 3, frame - 6, frame]))
    net[8] = rng.random() < 0.8
    net[0xb] = rng.random() < 0.8
    a = rng.uniform(-math.pi, math.pi)
    struct.pack_into('<3f', net, 0x24, f32(math.cos(a)), 0.0, f32(math.sin(a)))
    struct.pack_into('<3f', net, 0x30, pos[0] + 2, pos[1], pos[2] + 2)
    if rng.random() < 0.3:   # flush with the wall: N.D is exactly 0 (the reflect's -v case)
        struct.pack_into('<3f', net, 0x24, 1.0, 0.0, 0.0)
        struct.pack_into('<3f', net, 0x30, pos[0], pos[1], pos[2] + 2)
    if rng.random() < 0.15:  # only a ceiling over the top probe (the last segment)
        vz, yawr = struct.unpack_from('<f', mech, 0x44)[0], struct.unpack_from('<f', mech, 0x5c)[0]
        use = [False] * 11
        if abs(vz) < abs(f32(yawr * f32(3.3))):
            use[0] = use[2] = use[3] = use[5] = True
        v = struct.unpack_from('<3f', mech, 0x3c)
        p = struct.unpack_from('<3f', mech, 0x5a0)
        if sum(x * x for x in v) + sum(x * x for x in p) > 0:
            pl = flags & 2
            if vz < 0:
                use[0] = use[1] = use[2] = True
                if pl:
                    use[6] = use[7] = True
            else:
                use[3] = use[4] = use[5] = True
                if pl:
                    use[8] = use[9] = True
        use[10] = True
        last = sum(use) - 1
        recs[0] = [[] for _ in range(12)]
        recs[0][last] = [(rng.choice(list(range(MECH_NODE)) + [-1]), [0.0, -1.0, 0.0],
                          [pos[0], f32(pos[1] + 14), pos[2]])]
    return {'op': 0 if rng.random() < 0.95 else 1, 'mech': bytes(mech), 'transform': transform, 'pos': pos,
            'roster': rng.randrange(19), 'half': half, 'skis_vy': [f32(rng.uniform(-5, 5)) for _ in range(7)],
            'foot': foot, 'box': box, 'nodes': nodes, 'recs': recs, 'veh': veh, 'contacts': contacts,
            'other_roster': [rng.randrange(19), rng.randrange(19)], 'other_team': [rng.choice([0, 1, 2]) for _ in range(2)],
            'other_vel': [[f32(rng.uniform(-10, 10)) for _ in range(3)] for _ in range(2)],
            'other_remote': [int(rng.random() < 0.3) for _ in range(2)], 'ai': int(rng.random() < 0.5),
            'ai_rev': int(rng.random() < 0.5), 'flags': flags, 'frame': frame, 'net': bytes(net)}


def main():
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
    emu = Emulated()
    rng = random.Random(4225)
    seen = set()
    blocked = stuck = 0
    for t in range(trials):
        s = scenario(rng)
        want, got = emu.run(s), port(s)
        if s['op'] == 1:
            if want[1][0x124:0x1a8] != got[1][0x124:0x1a8]:
                raise SystemExit(f'trial {t}: the probe setup differs')
            seen.add('setup')
            continue
        names = ('result', 'mech', 'other mechs', 'vehicles', 'skis and position', 'calls')
        for k, name in enumerate(names):
            a, b = want[k], got[k]
            if k == 1:
                a, b = compare_image(a), compare_image(b)
            if a != b:
                if k == 1:
                    allf = FIELDS + [0x374, 0x375, 0x376, 0x377]
                    diff = [hex(allf[i]) for i in range(len(a)) if a[i] != b[i]]
                    raise SystemExit(f'trial {t}: {name} differ at {diff[:12]}')
                raise SystemExit(f'trial {t}: {name} differ\n  original {a}\n  port     {b}')
        log = want[5]
        i = 0
        while i < len(log):
            seen.add(log[i + 1])
            i += log[i] + 1
        img = want[1]
        blocked += any(img[0x374:0x378])
        stuck += want[0]
    need = {7, 8, 9, 10, 11, 12, 13, 14, 15, 20}
    need.add('setup')
    if not need <= seen:
        raise SystemExit(f'coverage: missing {sorted(need - seen)}')
    print(f'ok: {trials} Collide calls identical bit for bit; {blocked} resolved something, {stuck} stayed blocked; '
          f'{len(seen)} kinds of call out')


if __name__ == '__main__':
    main()

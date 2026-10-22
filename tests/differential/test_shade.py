"""The mechs' sun shading against the original x86 (docs/shade.md; census
#9, the options' "shadow parts"): the shade's set-up 0x4e61e0 (0x4e6250,
0x4e66a0), the frame 0x4e6480 with the ray 0x4e6420 / 0x4e6270, the reset
0x4e67c0, the switch 0x4e6820 / 0x4e6900 / 0x4e6910, the option 0x4e6920,
the scene walk's before and after calls 0x4e6720 / 0x4e6770 (0x4e66f0,
0x4e6710).

    cmake --build build && .venv/bin/python tests/differential/test_shade.py

Random scripts over 1..5 mechs (with or without parts): shades made, the
option set in and out of a mission, switched on and off, frames with the
world changing between them (the sun, its diffuse and direction, centroids
moving and going stale, the rays' answers), nodes drawn, shades reset.
Both sides log their calls and write their state; they must match. The x87
at the in-play 24 bits.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_shade_run.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p,
                                  ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
CAP = 1 << 20
WORLD, FRAME, TIME, ENTS, STATIC, FROZEN, QWORLD = 0x6098d4, 0x72f338, 0x60991c, 0x73655c, 0x5ef740, 0x60b94c, 0x608c84
GLOBALS = 0x60b7dc
NEW, DELETE = 0x5abd26, 0x5abd1a
BIG = 0x40000000
COVER = {0x4e64d4: 'its turn', 0x4e6556: 'fade down', 0x4e656e: 'fade up', 0x4e6566: 'clamp down',
         0x4e657e: 'clamp up', 0x4e6375: 'shaded', 0x4e637e: 'lit', 0x4e6437: 'stale centroid',
         0x4e6732: 'part level', 0x4e6744: 'mech level', 0x4e67ef: 'parts reset', 0x4e685a: 'on',
         0x4e6881: 'off', 0x4e6939: 'option in a mission', 0x4e64b9: 'round wraps'}


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def u2f(u):
    return struct.unpack('<f', struct.pack('<I', u))[0]


def h(v):
    return f'{v & 0xffffffff:x}'


class Exe:
    def __init__(self):
        cpu = self.cpu = Original()
        cpu.fpu_control(0x007f)
        cpu.uc.mem_map(BIG, 0x1000000)
        self.next = BIG
        stubs = {
            NEW: (lambda c: self.alloc(c.arg(0)), 0),
            DELETE: (lambda c: 0, 0),
            0x4eb6d0: (self.intersect, 0),
            0x4ed9c0: (lambda c: self.ev(f'mode {h(c.ecx)}'), 0),
            0x4eda00: (lambda c: self.ev(f'exclude {h(c.ecx)}'), 0),
            0x4eed90: (self.segment, 0x18),
            0x4eb8a0: (self.flags_a, 0),
            0x4eb8e0: (self.flags_b, 0),
            0x51e9d0: (lambda c: 0, 0), 0x51fde0: (lambda c: 0, 0), 0x51ea00: (lambda c: 0, 0),
            0x4ed280: (self.refresh, 0),
            0x4f7630: (lambda c: self.ev(f'walk {int(c.ecx != 0)}'), 0),
            0x4f7650: (lambda c: 0, 0),
            0x40ef30: (self.part_of, 0),
            0x40ef10: (self.mech_of, 0),
        }
        for a, (fn, pop) in stubs.items():
            cpu.stub_function(a, fn, pop)
        self.reached = set()

        def hit(uc, address, size, user):
            self.reached.add(address)
        from unicorn import UC_HOOK_CODE
        for a in COVER:
            cpu.uc.hook_add(UC_HOOK_CODE, hit, begin=a, end=a)
        self.vt_ent = self.alloc(0x100)
        self.vt_obj = self.alloc(0x100)
        r1, r2 = cpu.code(b'\xc3'), cpu.code(b'\xc3')
        cpu.stub_function(r1, lambda c: c.ecx, 0)                  # ent vt+0x2c: itself
        cpu.stub_function(r2, lambda c: self.ent_mech[c.ecx], 0)   # vt+0x50: its mech
        cpu.u32(self.vt_ent + 0x2c, r1)
        cpu.u32(self.vt_ent + 0x50, r2)
        self.heap0 = self.next

    def alloc(self, n):
        a = self.next
        self.next += (max(n, 1) + 0xf) & ~0xf
        self.cpu.write(a, bytes((max(n, 1) + 0xf) & ~0xf))
        return a

    def u32(self, a):
        return struct.unpack('<I', self.cpu.read(a, 4))[0]

    def ev(self, s):
        self.lines.append(s)
        return 0

    # --- stubs ------------------------------------------------------------------
    def intersect(self, c):
        node = self.id_of[c.ecx]
        self.ev(f'intersect {h(node)} {h(c.edx)}')
        if c.edx == 0:
            self.last = node
        return 0

    def segment(self, c):
        assert c.ecx == self.u32(self.u32(QWORLD) + 4)
        args = [c.arg(i) for i in range(6)]
        self.ev('segment ' + ' '.join(h(a) for a in args))
        ans = self.hits.get(self.last, 0)
        self.cpu.write(c.edx, bytes([1 if ans == 1 else 0]))
        return 1 if ans == 0 else 0

    def flags_a(self, c):
        self.ev(f'flags {h(self.id_of[c.ecx])} {h(c.edx & 0xff)}')
        self.flag_a = (c.ecx, c.edx & 0xff)
        return 0

    def flags_b(self, c):
        assert (c.ecx, c.edx & 0xff) == self.flag_a
        return 0

    def refresh(self, c):
        node = self.id_of[c.ecx]
        self.ev(f'refresh {h(node)}')
        d = self.u32(c.ecx + 0x38)
        self.cpu.write(d, bytes([self.cpu.read(d, 1)[0] & ~0x20]))
        return 0

    def part_of(self, c):
        return self.part_by_node.get(self.u32(c.ecx), 0)

    def mech_of(self, c):
        return self.mech_by_node.get(self.u32(c.ecx), 0)

    # --- a script -----------------------------------------------------------------
    def run(self, mechs, ops):
        cpu = self.cpu
        self.next = self.heap0
        self.lines, self.id_of, self.hits, self.last = [], {}, {}, 0
        self.part_by_node, self.mech_by_node, self.ent_mech = {}, {}, {}
        cpu.write(GLOBALS, bytes(0x1c))
        cpu.u32(0x60b7ec, 0)
        cpu.u32(STATIC, 1)
        cpu.u32(FROZEN, 0)
        cpu.u32(TIME, 0)
        q = self.alloc(0x10)
        cpu.u32(QWORLD, q)
        cpu.u32(q + 4, 0x5150)
        w, o, light = self.alloc(0x10), self.alloc(0x40), self.alloc(0xd0)
        cpu.u32(WORLD, w)
        cpu.u32(w + 4, o)
        self.light, self.obj = light, o
        self.addr = {}
        recs = []
        ents = self.alloc(4 * max(1, len(mechs)))
        for i, (mid, node, centroid, nparts) in enumerate(mechs):
            m, n, cn, d = self.alloc(0x700), self.alloc(0x40), self.alloc(0x40), self.alloc(0xd0)
            cpu.u32(m + 4, n)
            cpu.u32(m + 0x14, cn)
            cpu.u32(cn + 0x38, d)
            self.id_of[n], self.id_of[cn], self.id_of[m] = node, centroid, mid
            self.mech_by_node[n] = m
            parts = []
            if nparts >= 0:
                lst, head = self.alloc(0x10), self.alloc(0x10)
                cpu.u32(m + 0x380 + 4, head)
                cpu.u32(head, head)
                cpu.u32(head + 4, head)
                prev = head
                for k in range(nparts):
                    p, pn, lv, el = self.alloc(0x80), self.alloc(0x40), self.alloc(0x10), self.alloc(0x10)
                    cpu.u32(p + 8, pn)
                    cpu.u32(p + 0x68, lv)
                    cpu.write(lv, struct.pack('<3f', 0.5, 0.25, 0.75))
                    self.id_of[pn] = node + 1 + k
                    self.part_by_node[pn] = p
                    self.mech_by_node[pn] = m  # 0x40e9d0: a part's node gives its mech
                    cpu.u32(el + 8, p)
                    cpu.u32(el, head)
                    cpu.u32(el + 4, prev)
                    cpu.u32(prev, el)
                    cpu.u32(head + 4, el)
                    prev = el
                    parts.append(lv)
            ent = self.alloc(0x10)
            cpu.u32(ent, self.vt_ent)
            self.ent_mech[ent] = m
            cpu.u32(ents + 4 * i, ent)
            recs.append({'m': m, 'n': n, 'cn': cn, 'd': d, 'parts': parts, 'has': nparts >= 0, 'shade': 0,
                         'id': mid})
            self.addr[node] = n
            for k in range(max(0, nparts)):
                self.addr[node + 1 + k] = [a for a, v in self.id_of.items() if v == node + 1 + k][0]
        cpu.u32(ENTS, ents)
        cpu.u32(ENTS + 4, ents + 4 * len(mechs))
        self.done = []
        for op in ops:
            code = op[0]
            # The draw's calls are only set while on with a sun (the exe reads
            # through the null light otherwise): skipped, on both sides.
            if code in (5, 6) and not self.u32(GLOBALS):
                continue
            self.done.append(op)
            if code == 0:
                r = recs[op[1]]
                s = self.alloc(0x24)
                cpu.call(0x4e61e0, ecx=s, stack_args=[r['m'], r['m'] + 0x380 if r['has'] else 0])
                cpu.u32(r['m'] + 0x68c, s)
                r['shade'] = s
                self.ev(f'made {h(r["id"])}')
            elif code == 1:
                cpu.call(0x4e6920, ecx=op[1])
            elif code == 2:
                cpu.call(0x4e6900)
            elif code == 3:
                cpu.call(0x4e6910)
            elif code == 4:
                cpu.u32(FRAME, op[1])
                for r in recs:
                    if r['shade']:
                        cpu.call(0x4e6480, ecx=r['shade'], stack_args=[op[2]])
            elif code in (5, 6):
                cpu.call(0x4e6720 if code == 5 else 0x4e6770, ecx=self.addr.get(op[1], op[1]))
                diffuse = self.u32(light + 0x9c)
                self.ev(f'drawn {h(op[1])} {h(diffuse)} {h(self.u32(STATIC))} {h(self.u32(FROZEN))}')
            elif code == 7:
                cpu.call(0x4e67c0, ecx=recs[op[1]]['shade'])
            elif code == 8:
                cpu.u32(o + 0x38, light if op[1] else 0)
                cpu.u32(light + 0x9c, op[2])
                cpu.write(light + 0x84, struct.pack('<3I', op[3], op[4], op[5]))
            elif code == 9:
                r = [x for x in recs if self.id_of[x['cn']] == op[1]][0]
                cpu.write(r['d'], bytes([0x20 if op[2] else 0]))
                cpu.write(r['d'] + 0x84, struct.pack('<3I', op[3], op[4], op[5]))
            elif code == 10:
                self.hits[op[1]] = op[2]
            elif code == 11:
                cpu.u32(TIME, op[1])
        g = [self.u32(GLOBALS + o_) for o_ in range(0, 0x1c, 4)]
        sun = g[0]
        text = (f'globals {h(1 if sun else 0)} {h(g[1] & 0xff)} {h(g[2])} {h(g[3])} {h(g[4])} {h(g[5] & 0xff)} '
                f'{h(g[6] & 0xff)}\n')
        text += f'sun {h(self.u32(light + 0x9c))} lights {h(self.u32(STATIC))} frozen {h(self.u32(FROZEN))}\n'
        for r in recs:
            text += f'mech {h(r["id"])}'
            s = r['shade']
            if s:
                lv = s + 0x10
                text += (f' {h(self.cpu.read(s + 0xc, 1)[0])} {self.level(lv)} turn {h(self.u32(s + 0x20))}')
            for lv in r['parts']:
                text += ' part ' + self.level(lv)
            text += '\n'
        return text, '\n'.join(self.lines) + ('\n' if self.lines else '')

    def level(self, a):
        return '/'.join(h(self.u32(a + k)) for k in (0, 4, 8))


def port(mechs, ops):
    flat_m = [v & 0xffffffff for m in mechs for v in m]
    flat_o = [v & 0xffffffff for o in ops for v in (list(o) + [0] * 8)[:8]]
    am = (ctypes.c_uint32 * max(1, len(flat_m)))(*flat_m)
    ao = (ctypes.c_uint32 * max(1, len(flat_o)))(*flat_o)
    out, log = ctypes.create_string_buffer(CAP), ctypes.create_string_buffer(CAP)
    r = probe.probe_shade_run(am, len(mechs), ao, len(ops), out, CAP, log, CAP)
    assert r == 0, r
    return out.value.decode(), log.value.decode()


def script(r):
    n = r.randint(1, 5)
    mechs = [(0x100 * (k + 1), 0x1000 * (k + 1), 0x1000 * (k + 1) + 0x800, r.choice([-1, 0, 1, 2, 3]))
             for k in range(n)]
    ops, frame, made = [], 1, set()

    def world():
        d = [r.uniform(-1, 1) for _ in range(3)]
        return (8, int(r.random() < 0.85), f2u(r.choice([1.0, 0.8, 0.5, r.uniform(0, 1)])), *map(f2u, d))
    ops.append(world())
    for m in mechs:
        ops.append((10, m[1], r.choice([0, 1, 1, 2])))
    for _ in range(r.randint(5, 40)):
        k = r.random()
        if k < 0.15 and len(made) < n:
            i = r.choice([x for x in range(n) if x not in made])
            made.add(i)
            ops.append((0, i))
        elif k < 0.22:
            ops.append((11, f2u(r.choice([0.0, 0.0, 5.0, -1.0]))))
            ops.append((1, int(r.random() < 0.7)))
        elif k < 0.27:
            ops.append((2,))
        elif k < 0.3:
            ops.append((3,))
        elif k < 0.6:
            if r.random() < 0.8:
                frame += 1
            ops.append((4, frame, f2u(r.choice([1 / 30, 1 / 60, 0.25, 0.3, 0.0, r.uniform(0, 0.7)]))))
        elif k < 0.7 and made:
            i = r.choice(sorted(made))
            m = mechs[i]
            node = m[1] + (r.randint(1, m[3]) if m[3] > 0 and r.random() < 0.4 else 0)
            ops.append((5, node))
            if r.random() < 0.9:
                ops.append((6, node))
        elif k < 0.75 and made:
            ops.append((7, r.choice(sorted(made))))
        elif k < 0.8:
            ops.append(world())
        elif k < 0.9:
            m = r.choice(mechs)
            ops.append((9, m[2], int(r.random() < 0.3), *map(f2u, [r.uniform(-50, 50) for _ in range(3)])))
        else:
            m = r.choice(mechs)
            ops.append((10, m[1], r.choice([0, 1, 2])))
    return mechs, ops


def main():
    exe = Exe()
    rnd = random.Random(int(os.environ.get('SEED', '1')))
    n = int(os.environ.get('N', '500'))
    fails = 0
    for i in range(n):
        mechs, ops = script(rnd)
        a = exe.run(mechs, ops)
        b = port(mechs, exe.done)
        if a != b:
            fails += 1
            for name, x, y in (('state', a[0], b[0]), ('log', a[1], b[1])):
                xl, yl = x.splitlines(), y.splitlines()
                for k in range(max(len(xl), len(yl))):
                    p_, q_ = (xl[k] if k < len(xl) else '<end>'), (yl[k] if k < len(yl) else '<end>')
                    if p_ != q_:
                        print(f'MISMATCH script {i}: {name} line {k}:\n    exe:  {p_}\n    port: {q_}')
                        break
            if fails > 5:
                break
    missing = sorted(v for k, v in COVER.items() if k not in exe.reached)
    if missing:
        print('not reached:', ', '.join(missing))
    print('FAIL' if fails or missing else 'PASS', f'({n} scripts, {fails} mismatches)')
    return 1 if fails or missing else 0


if __name__ == '__main__':
    code = main()
    sys.stdout.flush()
    os._exit(code)

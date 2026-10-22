"""The objectives' checks and completion against the original x86
(docs/objectives.md, pieces 3 and 4): the types' checks (destroy_entity
0x45f3b0, node_change 0x45f180, reach_area 0x45e890, valve_change
0x45f1c0), a group's check 0x45e540 (and 0x45f150), the
completion 0x45e110 with 0x45ec40 / 0x45ec60, the cockpit flash's test
0x45c180, Instant Action's and multiplayer's "destroy the enemies"
0x45c4a0, the engine item's texts (0x4738b0, 0x4739d0, 0x432f00).

    cmake --build build && .venv/bin/python tests/differential/test_objective_check.py

Each scenario reads a made-up block (test_objectives.py's, both sides
identical there), may add the enemies' group over a made-up world, then runs
frames of the mission's checks: every group's 0x45e540(1), then 0x45c180.
The world changes between frames from a script both sides share: entities
destroyed (+0x08), nodes switched (+0x24 bit 2), valve values, the player's
position, nodes' positions, nav points. The player's
vehicle, the view's and its screen come and go between scenarios. Both
sides' objects, the mission's salvage store and the log of calls must match.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_objectives as T  # noqa: E402
from test_objectives import CAP, PILOT, POINT, NAVS, WORK, archive, h32, made_up, probe  # noqa: E402

probe.probe_objective_check_run.argtypes = [
    ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int,
    ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_int,
    ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
SALVAGE, VIEW, ENTS, STRTOK = 0x609990, 0x5f4c8c, 0x73655c, 0x5bb608
COVER = {0x45e5ad: 'announce voice', 0x45e608: 'simultaneous reset', 0x45e14a: 'objective voice',
         0x45e1ac: 'second desc line', 0x45e1f5: 'salvage line', 0x45e25e: 'node salvage line',
         0x45ec89: 'node salvage added', 0x45e2ad: 'salvage added', 0x45e3c8: 'set_valve',
         0x45e3fc: 'objectives box', 0x45e57a: 'in order: stop', 0x45c193: 'visible primary',
         0x45c4f8: 'ia mode 1', 0x45c52d: 'ia mode 2', 0x45c643: 'enemy target', 0x45c71d: 'multiplayer group',
         0x45c725: 'new primary', 0x45f3c8: 'destroy refind', 0x45f3d6: 'destroy flag',
         0x45e8c4: 'reach nav', 0x45e962: 'reach node', 0x45e93a: 'reach nav met', 0x45f1da: 'valve value',
         0x45f193: 'node_change flag'}


def mix(seed, k):
    h = (seed * 2654435761 + k * 40503) & 0xffffffff
    h ^= h >> 13
    h = (h * 0x5bd1e995) & 0xffffffff
    h ^= h >> 15
    return h


class CheckRun(T.Run):
    def __init__(self):
        super().__init__()
        cpu = self.cpu
        stubs = {
            0x48f6c0: (self.voice, 8),
            0x40fe70: (self.hud, 0),
            0x40fe20: (self.hud_line, 0),
            0x430aa0: (self.screen_of, 0),
            0x446ad0: (self.box, 0),
            0x479460: (lambda c: self.ia, 0),
        }
        stubs[0x5a85e0] = (self.position, 0)
        stubs[0x4ed530] = (self.node_position, 0)
        for addr, (fn, pop) in stubs.items():
            cpu.stub_function(addr, fn, pop)
        cpu.stub_import(STRTOK, self.strtok)
        self.tok = 0
        self.obj_vt = self.calloc(0x100)
        for off, fn in ((0x28, self.team), (0x48, self.x48)):
            r = cpu.code(b'\xc3')
            cpu.stub_function(r, fn, 0)
            cpu.u32(self.obj_vt + off, r)
        cpu.u32(self.obj_vt + 0xa4, self.u32(self.entity_vt + 0xa4))
        self.nav_vt, self.inner_vt = self.calloc(0x100), self.calloc(0x100)
        for vt, off, fn in ((self.nav_vt, 0x18, self.nav_position), (self.inner_vt, 0x4, self.valve_value)):
            r = cpu.code(b'\xc3')
            cpu.stub_function(r, fn, 0)
            cpu.u32(vt + off, r)
        self.ent_vt = self.calloc(0x100)
        r = cpu.code(b'\xc3')
        cpu.stub_function(r, lambda c: self.ent_obj[c.ecx], 0)
        cpu.u32(self.ent_vt + 0x2c, r)
        self.heap0 = self.next
        self.cover(COVER)

    # --- the fakes (probe_objectives.cpp's) ----------------------------------
    def node_mem(self, ident):
        if ident == 0:
            return 0
        if ident not in self.nodes:
            o = self.calloc(0x40)
            self.cpu.u32(o + 0x24, ((ident >> 5) & 1) << 2)
            self.nodes[ident] = o
            self.addr_of[o] = ident
        return self.nodes[ident]

    def nav_find(self, c):
        ident = super().nav_find(c)
        if not ident:
            return 0
        if ident not in self.navs:
            o = self.calloc(0x10)
            self.cpu.u32(o, self.nav_vt)
            self.navs[ident] = o
            self.addr_of[o] = ident
        return self.navs[ident]

    def valve_find(self, c):
        ident = super().valve_find(c)
        if not ident:
            return 0
        if ident not in self.valves:
            v, inner = self.calloc(0x10), self.calloc(0x10)
            self.cpu.u32(v + 4, inner)
            self.cpu.u32(inner, self.inner_vt)
            self.inner_of[inner] = ident
            self.valves[ident] = v
            self.addr_of[v] = ident
        return self.valves[ident]

    def valve_value(self, c):
        v = self.inner_of[c.ecx]
        self.ev(f'vvalue {h32(v)}')
        return mix(self.seed ^ v, self.frame + 1) % 3

    def coord(self, h):
        return float(h % 61 - 30)

    def put3(self, buf, xyz):
        self.cpu.write(buf, struct.pack('<3f', *xyz))
        return buf

    def position(self, c):
        self.ev(f'pos {h32(self.addr_of[c.ecx])}')
        m = mix(self.seed, 0x1000 + self.frame)
        return self.put3(self.buf3, (self.coord(m), self.coord(m >> 8) / 4.0, self.coord(m >> 16)))

    def node_position(self, c):
        n = self.addr_of[c.ecx]
        self.ev(f'npos {h32(n)}')
        m = mix(self.seed ^ n, self.frame)
        self.cpu.write(c.edx, struct.pack('<3f', self.coord(m), 1.0, self.coord(m >> 16)))
        return 0

    def nav_position(self, c):
        nav = self.addr_of[c.ecx]
        self.ev(f'navpos {h32(nav)}')
        m = mix(nav, 1)
        return self.put3(self.buf3 + 0x10, (self.coord(m), 0.5, self.coord(m >> 16)))

    def flag(self, e, base):
        if self.frame < 0:
            return base
        return 0 if self.frame >= mix(self.seed, e) % 6 else base

    def set_world(self):
        for ident, o in self.entities.items():
            self.cpu.write(o + 8, bytes([self.flag(ident, int(ident & 0xf0 != 0))]))
        for ident, (active, _, _) in self.objs.items():
            self.cpu.write(self.obj_addr[ident] + 8, bytes([self.flag(ident, active)]))
        for ident, o in self.nodes.items():
            bit = (mix(self.seed ^ ident, self.frame) >> 3) & 1
            self.cpu.u32(o + 0x24, bit << 2)

    def node_find(self, c):
        return self.node_mem(super().node_find(c))

    def subnode_find(self, c):
        name = self.cstr(c.edx)
        node = self.addr_of[c.ecx]
        self.ev(f'sub {h32(node)} {name.decode("latin-1")}')
        h = T.fnv(name) ^ node
        return self.node_mem(0 if h % 6 == 0 else 0x9000000 | (h & 0xffffff))

    def message_with(self, c):
        buf, size, mid = c.arg(0), c.arg(1), c.arg(2)
        if size == 0x40:
            return super().message_with(c)
        assert size == 0x100
        if mid in (0x261, 0xdb1):
            n = c.arg(3)
            self.ev(f'msgn {h32(mid)} {h32(n)}')
            text = f'N{h32(mid)}:{n - (1 << 32) if n >> 31 else n}'
        else:
            self.ev(f'msgn {h32(mid)}')
            text = f'N{h32(mid)}'
        self.cpu.write(buf, text.encode() + b'\0')
        return 1

    def strtok(self, c):
        p, delim = c.arg(0), self.cstr(c.arg(1))
        p = p or self.tok
        if not p:
            return 0
        while self.u8(p) and self.u8(p) in delim:
            p += 1
        if not self.u8(p):
            self.tok = 0
            return 0
        start = p
        while self.u8(p) and self.u8(p) not in delim:
            p += 1
        if self.u8(p):
            self.cpu.write(p, b'\0')
            self.tok = p + 1
        else:
            self.tok = 0
        return start

    def voice(self, c):
        assert c.ecx == 0x6098d0
        return self.ev(f'voice {h32(c.arg(0))} {h32(c.arg(1))}')

    def hud(self, c):
        return self.ev(f'hud {h32(c.arg(0))} {h32(c.arg(1))} {self.text(c.arg(2))}')

    def hud_line(self, c):
        return self.ev(f'line {h32(c.arg(0))} {self.text(c.arg(1))}')

    def screen_of(self, c):
        assert c.ecx == self.u32(self.vehicle_mem + 0x684)
        self.ev(f'screen {h32(self.vehicle_id)}')
        return self.screen_mem

    def box(self, c):
        return self.ev(f'box {h32(self.addr_of[c.ecx - 0x17964])}')

    def team(self, c):
        o = self.addr_of[c.ecx]
        self.ev(f'team {h32(o)}')
        return self.teams[o]

    def x48(self, c):
        o = self.addr_of[c.ecx]
        self.ev(f'x48 {h32(o)}')
        return self.objs[o][2]

    # --- a scenario -------------------------------------------------------------
    def scenario(self, files, block, gtype, point, seed, frames, vehicle, view, screen, ia, number, objs):
        cpu = self.cpu
        self.next = self.heap0
        self.lines, self.entities, self.addr_of, self.type = [], {}, {}, gtype
        self.nodes, self.order, self.seed = {}, {}, seed
        self.navs, self.valves, self.inner_of, self.obj_addr, self.frame = {}, {}, {}, {}, -1
        self.buf3 = self.calloc(0x20)
        self.trees = {k.lower(): [self.build(v)] for k, v in files.items()}
        cpu.write(POINT, struct.pack('<3f', *point))
        cpu.u32(NAVS, 0x1000)
        cpu.u32(PILOT, self.pilot)
        # The player's vehicle, the view's (its +0x684 the mech's screen data).
        self.vehicle_id, self.vehicle_mem, self.screen_mem = vehicle, 0, 0
        if vehicle:
            self.vehicle_mem = self.calloc(0x700)
            cpu.u32(self.vehicle_mem + 0x684, 0x1234)
            cpu.u32(self.vehicle_mem + 0xc8, 1)
            self.addr_of[self.vehicle_mem] = vehicle
        if screen:
            self.screen_mem = self.calloc(0x10)
            self.addr_of[self.screen_mem] = screen
        cpu.u32(self.pilot + 0x14, self.vehicle_mem)
        holder = self.calloc(0x100)
        cpu.u32(VIEW, holder)
        cpu.u32(holder + 0x64, self.vehicle_mem if view == vehicle and vehicle else self.calloc(0x10))
        store = self.calloc(0x14)
        cpu.call(0x470ce0, ecx=store, stack_args=[1])
        cpu.u32(SALVAGE, store)
        # Instant Action: its game, zEntity's list of objects.
        self.ia = self.calloc(0x100)
        cpu.u32(self.ia + 0x44, ia & 0xffffffff)
        cpu.u32(self.ia + 0x6c, number)
        self.ent_obj, self.objs, self.teams = {}, {}, {}
        arr = self.calloc(4 * max(1, len(objs)))
        for i, (ident, active, hostile, x48) in enumerate(objs):
            ent, obj, team = self.calloc(0x10), self.calloc(0x20), self.calloc(0x100)
            cpu.u32(ent, self.ent_vt)
            cpu.u32(obj, self.obj_vt)
            cpu.write(obj + 8, bytes([active]))
            cpu.u32(team + 0xc0, hostile)
            self.ent_obj[ent], self.addr_of[obj], self.objs[ident] = obj, ident, (active, hostile, x48)
            self.obj_addr[ident] = obj
            self.teams[ident] = team
            cpu.u32(arr + 4 * i, ent)
        cpu.u32(ENTS, arr)
        cpu.u32(ENTS + 4, arr + 4 * len(objs))
        out = self.calloc(0x14)
        fname, bname = self.new_string(b'objectives.zrd'), self.new_string(block.encode())
        cpu.call(0x45c2d0, ecx=fname, edx=bname, stack_args=[out + 4 * i for i in range(5)])
        if ia >= 0:
            cpu.call(0x45c4a0, ecx=out + 4, edx=out + 8, stack_args=[out + 12, out + 16])
        for f in range(frames):
            self.frame = f
            self.set_world()
            self.ev(f'frame {h32(f)}')
            for i, slot in enumerate((8, 4, 12, 16)):
                g = self.u32(out + slot)
                if g:
                    cpu.call(0x45e540, ecx=g, stack_args=[1])
                    self.ev(f'check {h32(i)} {h32(self.eax() & 0xff)}')
            cpu.call(0x45c180, ecx=out + 4, stack_args=[out + 12, out + 16])
            self.ev(f'visible {h32(self.eax() & 0xff)}')
        text = 'desc ' + self.opt(self.u32(out)) + '\n'
        for i, name in enumerate(('primary', 'secondary', 'final', 'fail')):
            text += name + '\n'
            p = self.u32(out + 4 + 4 * i)
            if p:
                text += self.obj(p, 1)
        text += 'salvage' + self.store(store) + '\n'
        return text, '\n'.join(self.lines) + ('\n' if self.lines else '')


def port(files, block, gtype, point, seed, frames, vehicle, view, screen, ia, number, objs):
    WORK.mkdir(parents=True, exist_ok=True)
    path = WORK / 'objective_check.zbd'
    path.write_bytes(archive(files))
    out, log = ctypes.create_string_buffer(CAP), ctypes.create_string_buffer(CAP)
    pt = (ctypes.c_float * 3)(*point)
    flat = [v & 0xffffffff for o in objs for v in o]
    arr = (ctypes.c_uint32 * max(1, len(flat)))(*flat)
    r = probe.probe_objective_check_run(str(path).encode(), block.encode(), gtype, pt, seed, frames, vehicle,
                                        view, screen, ia, number, arr, len(objs), out, CAP, log, CAP)
    assert r == 0, r
    return out.value.decode('latin-1'), log.value.decode('latin-1')


def report(tag, a, b):
    for name, x, y in (('objects', a[0], b[0]), ('log', a[1], b[1])):
        xl, yl = x.splitlines(), y.splitlines()
        for i in range(max(len(xl), len(yl))):
            p, q = (xl[i] if i < len(xl) else '<end>'), (yl[i] if i < len(yl) else '<end>')
            if p != q:
                print(f'MISMATCH {tag}: {name} line {i}:\n    exe:  {p}\n    port: {q}')
                if os.environ.get('CTX'):
                    print('    before:', ' | '.join(xl[max(0, i - 12):i]))
                break


def main():
    emu = CheckRun()
    emu.unported_seen = set()
    r = random.Random(int(os.environ.get('SEED', '1')))
    n = int(os.environ.get('N', '300'))
    fails = 0
    for i in range(n):
        data = made_up(r)
        gtype = r.choice([1, 1, 3, 0])
        point = (r.uniform(-50, 50), 0.0, r.uniform(-5, 5))
        seed = r.getrandbits(32)
        frames = r.randint(1, 8)
        vehicle = r.choice([0xb0001, 0xb0001, 0xb0001, 0])
        view = r.choice([vehicle, vehicle, 0xb0009])
        screen = r.choice([0xc0001, 0xc0001, 0])
        ia = r.choice([-1, -1, 0, 1, 2, 3, 4])
        number = r.randint(0, 20)
        objs = [(0xd00000 + 16 * k + r.randint(0, 15), r.random() < 0.8, r.random() < 0.7, r.choice([0, 0, 0, 1]))
                for k in range(r.randint(0, 5))]
        objs = [(a, int(b), int(c), d) for a, b, c, d in objs]
        if not vehicle and b'reach_area' in data:
            vehicle = 0xb0001  # the exe reads a null vehicle's position (the port's FIX (bug))
        if ia >= 0 and objs and not vehicle:
            vehicle = 0xb0001  # the builder reads the player's vehicle's team unchecked
        args = ({'objectives.zrd': data}, 'm1', gtype, point, seed, frames, vehicle, view, screen, ia, number, objs)
        a = emu.scenario(*args)
        b = port(*args)
        if a != b:
            report(f'scenario {i}', a, b)
            fails += 1
            if fails > 5:
                break
    missing = sorted(v for k, v in COVER.items() if k not in emu.reached)
    if missing:
        print('not reached:', ', '.join(missing))
    print('FAIL' if fails or missing else 'PASS', f'({n} scenarios, {fails} mismatches)')
    return 1 if fails or missing else 0


if __name__ == '__main__':
    code = main()
    sys.stdout.flush()
    os._exit(code)

"""The pocket commander's frame against PM's exe: 0x46b980 with 0x46b8d0 (the marker put on the ground: the world's
ground query 10 above the spot, 0x4ff8d0, and the height 0x4af770 picks, plus 1) real, on made-up controls (PM's
fields: MW3's +0x20) and a made-up panel; everything else it calls stood in from the case and logged. The port is
mech3/pocket_panel's pocket_frame under zgame's pocket_frame_pm on a real, empty panel (tests/probe_pm_pocket.cpp).
Compared: the calls in order (the panel's routing and re-pick, the map's and the reticle's picks, the node's and the
contact's entities, the target picked, the marker moved and where, the ground asked, the marker never shown or
hidden, the view and the camera), then the mode's fields, the cursor, the target line's entity and text, the aim.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_pocket_frame.py [cases]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_X87'] = '53'
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

FRAME = 0x46b980
VIEW, NET, WORLD = 0x61874c, 0x62daf0, 0x62ca90
CURSOR, MARKER, MAP1, MAP2 = 0x629a30, 0x629a2c, 0x629a24, 0x629a34
NAMES = ['map_up', 'map', 'map_w', 'map_h', 'mouse_x', 'mouse_y', 'trigger', 'map_hit', 'map_node', 'map_place0',
         'map_place1', 'map_place2', 'record', 'record_node', 'ret0', 'ret1', 'ret2', 'node_entity', 'marker',
         'marker_node', 'target', 'target_entity', 'target_owner', 'aim_entity', 'aim_owner', 'contact_entity', 'net',
         'net_element', 'net_prev', 'net_entity', 'net_place0', 'net_place1', 'net_place2', 'exiting', 'state', 'count',
         'ground_ret', 'ground_hits', 'ground_height', 'named']
K = {n: i for i, n in enumerate(NAMES)}
FLOATS = {'map_place0', 'map_place1', 'map_place2', 'ret0', 'ret1', 'ret2', 'net_place0', 'net_place1', 'net_place2',
          'ground_height'}
PLACES = 3  # the port's empty panel's +0x74


def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def case(r):
    ents = [0x9000 + 0x100 * i for i in range(4)]
    marker = r.choice([0xa000, 0xa000, 0])
    pick = r.choice(ents + [0, marker])
    v = {'map_up': r.choice([0, 0, 1]), 'map': 0xb000, 'map_w': r.choice([100, 400]), 'map_h': r.choice([100, 300]),
         'mouse_x': r.choice([5, 99, 100, 350, 450]), 'mouse_y': r.choice([5, 99, 100, 250, 350]),
         'trigger': r.choice([0, 0, 4, 4, 5, 1]), 'map_hit': r.choice([0, 1, 1]), 'map_node': r.choice([0, 0x7700, 0x7701]),
         'record': r.choice([0, 1, 1]), 'record_node': r.choice([0, 0x7702, 0x7703]), 'node_entity': pick,
         'marker': marker, 'marker_node': r.choice([0x7799, 0x7798]), 'target': r.choice([0, 1, 1]),
         'target_entity': r.choice(ents), 'target_owner': r.choice([0x8000, 0x8100]),
         'contact_entity': r.choice(ents + [marker]), 'net': r.choice([0, 0, 1]), 'net_element': r.choice([0xc000, 0xc100]),
         'net_entity': r.choice(ents + [0, 0]), 'exiting': r.choice([0, 0, 0, 1]), 'state': r.choice([0, 0, 1, 2, 3, 4]),
         'count': r.choice([1, 2, 3]), 'ground_ret': r.choice([0, 0, 1]), 'ground_hits': r.choice([0, 1, 1, 2]),
         'named': 1}
    v['net_prev'] = r.choice([0, v['net_element'], 0xc200])
    # The exe reads the marker's or the entity's +0x04 through null where the port doesn't (the mission's start always
    # makes a marker; a target's contact always has an entity): those cases aren't played.
    v['contact_entity'] = r.choice(ents)
    if not marker:
        v['node_entity'] = v['node_entity'] or ents[0]
        v['net_entity'] = v['net_entity'] or ents[1]
    same = r.random() < 0.4
    v['aim_entity'] = v['target_entity'] if same else r.choice(ents)
    v['aim_owner'] = v['target_owner'] if same else r.choice([0x8000, 0x8100])
    for n in FLOATS:
        v[n] = f32(r.uniform(-500, 500))
    return v


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.w = lambda a, v: self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))
        self.arena = z.alloc(0x200000)
        self.top = self.arena + 0x200000
        # 0x4af770 stood in once (fld [height]; ret): code patched each run would keep its first translation.
        self.height = z.alloc(16)
        self.uc.mem_write(0x4af770, b'\xd9\x05' + struct.pack('<I', self.height) + b'\xc3')
        self.reset()

    def reset(self):
        self.at = self.arena

    def zalloc(self, n):
        n = (n + 15) & ~15
        p = self.at
        self.at += n
        self.uc.mem_write(p, bytes(n))
        return p

    def ret(self, fn, pop=0):
        t = self.zalloc(4)
        self.uc.mem_write(t, b'\xc3')
        self.z.native_stub(t, fn, pop)
        return t

    def cstr(self, a):
        return bytes(self.uc.mem_read(a, 64)).split(b'\0')[0].decode('latin1')

    def string(self, s):
        p = self.zalloc(len(s) + 1)
        self.uc.mem_write(p, s.encode('latin1') + b'\0')
        return p

    def fl(self, a):
        return struct.unpack('<f', bytes(self.uc.mem_read(a, 4)))[0]

    def run(self, v):
        z, w, r32 = self.z, self.w, self.z.r32
        self.reset()
        log = []
        L = log.append
        # The entities (+0x04 the node, vt+0xb0 the name, vt+0x18 the marker's move) by handle: each made at that
        # address's twin in the arena, the handle mapped both ways.
        objs, back = {}, {}

        def obj(h):
            if not h:
                return 0
            if h in objs:
                return objs[h]
            o, vt = self.zalloc(0x40), self.zalloc(0x200)
            w(o, vt)
            w(o + 4, v['marker_node'] if h == v['marker'] else h + 0x10)
            name = self.string(f'E{h:x}')
            w(vt + 0xb0, self.ret(lambda c, n=name: n if v['named'] & 1 else 0))
            place = self.zalloc(12)
            self.uc.mem_write(place, struct.pack('<3f', float(h), float(h + 1), float(h + 2)))
            w(vt + 0x18, self.ret(lambda c, h=h: self.marker_move(L, h, c.arg(0)), 4))
            objs[h], back[o] = o, h
            self.places = getattr(self, 'places', {})
            self.places[o] = place
            return o
        ents = {h: obj(h) for h in {v['node_entity'], v['contact_entity'], v['net_entity'], v['target_entity'],
                                     v['marker']} if h}
        unknown = self.string('Unknown')
        c = self.zalloc(0x200)
        vbt = self.zalloc(0x10)
        w(c + 4, vbt)
        mech, mvt = self.zalloc(0x800), self.zalloc(0x200)
        w(mech, mvt)
        w(mvt + 0x58, self.ret(lambda cc: cc.ecx))
        w(c + 0x20, mech)
        target = self.zalloc(0x30)
        if v['target']:
            w(target + 0, v['target_owner'])
            w(target + 8, v['target_entity'])
            w(mech + 0x6c, target)
        w(c + 0x1a8, v['aim_owner'])
        w(c + 0x1b0, v['aim_entity'])
        w(c + 0x1dc, v['net_prev'])
        w(c + 0x1a4, v['exiting'])
        w(c + 0x168, v['state'])
        w(c + 0x16c, v['count'])
        w(c + 0x170, 3)
        view = self.zalloc(0x100)
        w(VIEW, view)
        w(view + 0x64, mech)
        hud = self.zalloc(0x2000)
        self.uc.mem_write(hud + 0x70, bytes([v['map_up']]))
        self.uc.mem_write(hud + 0x1d68, struct.pack('<3f', v['ret0'], v['ret1'], v['ret2']))
        mp, size = self.zalloc(0x100), self.zalloc(0x20)
        w(mp + 0xd8, size)
        w(size + 8, v['map_w'])
        w(size + 0xc, v['map_h'])
        panel, lvt = self.zalloc(0x600), self.zalloc(0x100)
        w(c + 0x1a0, panel)
        w(panel + 0x74, PLACES)
        w(panel + 0x7c, lvt)  # the target line, in the panel at +0x7c
        self.text = ''

        def set_text(cc):
            self.text = self.cstr(cc.arg(1)).replace('%s', self.cstr(cc.arg(2)))
            return 0
        w(lvt + 0x7c, self.ret(set_text))
        # The net: [0x62daf0] (+0x20), its element (0x5acad0): +0x18's vt+0x2c the entity, vt+0x18 the place.
        net, elem, sub, svt, evt = (self.zalloc(0x40) for _ in range(5))
        w(NET, net if v['net'] else 0)
        w(elem, evt)
        w(elem + 0x18, sub)
        w(sub, svt)
        w(svt + 0x2c, self.ret(lambda cc: ents.get(v['net_entity'], 0)))
        nplace = self.zalloc(12)
        self.uc.mem_write(nplace, struct.pack('<3f', v['net_place0'], v['net_place1'], v['net_place2']))
        w(evt + 0x18, self.ret(lambda cc: nplace))
        elems = {v['net_element']: elem}
        w(c + 0x1dc, elems.get(v['net_prev'], v['net_prev']))  # the element last seen, as the exe holds it
        w(MARKER, ents.get(v['marker'], 0))
        w(CURSOR, 0)
        world = self.zalloc(0x10)
        w(WORLD, world)
        w(world + 4, 0x5555)
        record = self.zalloc(0x40)
        w(record + 0x24, v['record_node'])
        self.uc.mem_write(self.height, struct.pack('<f', v['ground_height']))

        def ground(cc):
            L('ground %08x %08x %08x' % (cc.arg(1), cc.arg(2), cc.arg(3)))
            self.uc.mem_write(cc.arg(4), bytes([v['ground_hits']]))
            return v['ground_ret']

        def map_pick(cc):
            L('map pick %08x %08x' % (cc.arg(0), cc.arg(1)))
            if not v['map_hit']:
                return 0
            self.uc.mem_write(cc.arg(2), struct.pack('<3f', v['map_place0'], v['map_place1'], v['map_place2']))
            w(cc.arg(3) + 0x24, v['map_node'])
            return 1

        def map_mouse(cc):
            L('map mouse')
            w(cc.arg(0), v['mouse_x'])
            w(cc.arg(1), v['mouse_y'])
            return 0

        def reticle(cc):
            L('reticle pick')
            return record if v['record'] else 0

        def node_entity(cc):
            L('node entity %x' % r32(cc.ecx))
            return ents.get(v['node_entity'], 0)

        def contact(cc):
            L('contact entity')
            return ents.get(v['contact_entity'], 0)
        for addr, fn, pop in ((0x430e30, lambda cc: hud, 0), (0x46a3b0, lambda cc: L('route') or 0, 4),
                              (0x5acad0, lambda cc: elems.get(v['net_element'], 0), 4),
                              (0x495070, lambda cc: mp, 0), (0x468ff0, lambda cc: v['trigger'], 0),
                              (0x446060, map_mouse, 8), (0x459940, map_pick, 16), (0x445c40, reticle, 4),
                              (0x40f700, node_entity, 0), (0x596e50, lambda cc: unknown, 0),
                              (0x4ab090, lambda cc: L('pick target %x' % back.get(cc.arg(0), cc.arg(0))) or 0, 4),
                              (0x4f8d70, contact, 0), (0x5cb260, lambda cc: self.places[cc.ecx], 0),
                              (0x46a370, lambda cc: L('picked %d %d' % (cc.arg(0), cc.arg(1))) or 0, 8),
                              (0x4ff8d0, ground, 0), (0x4b96e0, lambda cc: L('view pocket %d' % (cc.arg(0) & 0xff)) or 0, 4),
                              (0x408a70, lambda cc: L('camera %d' % cc.arg(0)) or 0, 4)):
            z.native_stub(addr, fn, pop)
        z.call(FRAME, c, [0])
        place_ptr = r32(c + 0x188)
        pos = 1 if place_ptr == c + 0x18c else 2 if place_ptr == nplace else 3 if place_ptr else 0
        node = r32(c + 0x184)
        pl = [r32(c + 0x18c + 4 * i) for i in range(3)]
        e0, e1 = bytes(self.uc.mem_read(c + 0x1e0, 2))
        L('fields node %x place %08x %08x %08x pos %d e0 %d e1 %d net %x map %x state %d count %d x154 %x wrapped %d'
          % (node, pl[0], pl[1], pl[2], pos, e0, e1, {elem: v['net_element']}.get(r32(c + 0x1dc), r32(c + 0x1dc)),
             r32(c + 0x1d8) and v['map'], r32(c + 0x168),
             r32(c + 0x16c), r32(c + 0x174), r32(c + 0x38)))
        m = r32(MAP1)
        L('globals cursor %x map %x %x target %x text [%s]' % (back.get(r32(CURSOR), r32(CURSOR)), m and v['map'],
                                                               r32(MAP2) and v['map'], back.get(r32(panel + 0x53c), r32(panel + 0x53c)), self.text))
        L('aim %x %x' % (r32(c + 0x1b0), r32(c + 0x1a8)))
        return log

    def marker_move(self, L, h, p):
        q = struct.unpack('<3I', bytes(self.uc.mem_read(p, 12)))
        L('marker move %x %08x %08x %08x' % (h, *q))
        return 0


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_pocket_frame.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.c_char_p, ctypes.c_int]
    exe = Exe()
    r = random.Random(0x46b980)
    buf = ctypes.create_string_buffer(1 << 16)
    seen = {k: 0 for k in ('marker moved', 'ground', 'picked', 'pick target', 'net', 'target moved', 'map pick')}
    for n in range(cases):
        v = case(r)
        words = (ctypes.c_uint32 * len(NAMES))(*[fb(v[k]) if k in FLOATS else v[k] for k in NAMES])
        theirs = exe.run(v)
        k = p.probe_pm_pocket_frame(words, buf, len(buf))
        ours = buf.value.decode('latin1').splitlines() if k >= 0 else ['probe failed']
        if theirs != ours:
            print(f'case {n}: {v}')
            for a, b in zip(theirs + [''] * 40, ours + [''] * 40):
                print(('   ' if a == b else ' X ') + f'exe  {a}\n    port {b}' if a != b else f'    {a}')
            raise SystemExit('differ')
        text = '\n'.join(theirs)
        seen['marker moved'] += 'marker move' in text
        seen['ground'] += 'ground ' in text
        seen['picked'] += 'picked' in text
        seen['pick target'] += 'pick target' in text
        seen['net'] += 'e0 1' in text
        seen['target moved'] += 'contact entity' in text
        seen['map pick'] += 'map pick' in text
    floors = {k.replace(' ', '_'): (n, max(5, cases // 200)) for k, n in seen.items()}
    pm_floors.floors(cases=(cases, 3000), **floors)
    print(f'ok: {cases} frames identical; reached {seen}')


if __name__ == '__main__':
    main()

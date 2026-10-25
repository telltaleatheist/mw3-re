"""The entity classes' answers the HUD's radar and TARGET box ask for, against
the original x86 (docs/ai.md section 15, Radar contacts; mech3/entity_classes).

    cmake --build build && .venv/bin/python tests/differential/test_entity_classes.py [cases]

Each case lays out objects carrying the real vtables of every class (two
mechs, the network mech, the three vehicle vtables, the field base, a
turret, a target, a trap, the command marker, a plain AiTarget) with random
fields (team +0xc0, height +0x68, a target's flags +0xe8, name +0xec and
centroid node +0x14 whose data may want a refresh), the four blip tables
([0x605618], [0x60b120], [0x609a38], [0x60b4f0], some entries 0) and the
pilot ([0x60861c]: its vehicle one of the mechs or not, its name +0x18).
The original runs each object's own slots: vt+0x3c for every kind, vt+0x28
(+0xc0), vt+0x50, vt+0xa8, vt+0xa4, vt+0xbc, and 0x4e6fe0 / 0x4e7070 on a
contact seeing it (or nothing, for the size). zEntity3d's place 0x5a85e0,
the pilots' names 0x4aee70 and the network's 0x4709c0 are stubbed and
answered from one script; the refresh 0x4ed280 is recorded. Compared: every
answer (names as text, places as floats) and the calls in order.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, call_abs, compare, load_probe  # noqa: E402

U32 = ctypes.c_uint32
I32 = ctypes.c_int32
PILOT = 0x60861c
TABLES = {1: 0x605618, 2: 0x60b120, 3: 0x609a38, 4: 0x60b4f0}
# vtable -> the port's EntityClass
CLASSES = [(0x5bcef8, 1, 'mech'), (0x5bcef8, 1, 'mech2'), (0x5c3508, 2, 'net_mech'), (0x5c23d8, 3, 'vehicle'),
           (0x5c8460, 3, 'stealth'), (0x5c88e8, 3, 'basic'), (0x5c8d48, 4, 'field_base'), (0x5c8770, 5, 'turret'),
           (0x5ccb90, 6, 'target'), (0x5ccc80, 7, 'trap'), (0x5cced8, 8, 'marker'), (0x5ccfe0, 9, 'plain')]
SLOTS = (0x28, 0x3c, 0x50, 0xa4, 0xa8, 0xbc)
COUNT = {'objects': 0, 'answers': 0, 'calls': 0, 'refreshes': 0}


def die():
    sys.stdout.flush()
    os._exit(1)


def f2u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def u2f(u):
    return struct.unpack('<f', struct.pack('<I', u))[0]


def setup_probe(p):
    p.probe_ec_poke.argtypes = [U32, U32]
    p.probe_ec_poke8.argtypes = [U32, U32]
    p.probe_ec_string.argtypes = [U32, ctypes.c_char_p]
    p.probe_ec_class.argtypes = [U32, I32]
    for f in ('blip', 'team', 'mech', 'contact_vehicle'):
        getattr(p, 'probe_ec_' + f).restype = U32
    p.probe_ec_blip.argtypes = [U32, I32]
    p.probe_ec_team.argtypes = [U32]
    p.probe_ec_mech.argtypes = [U32]
    p.probe_ec_hidden.argtypes = [U32]
    p.probe_ec_name.argtypes = [U32, ctypes.c_char_p, I32]
    p.probe_ec_centre.argtypes = [U32, ctypes.POINTER(ctypes.c_float)]
    p.probe_ec_contact_vehicle.argtypes = [U32]
    p.probe_ec_contact_size.argtypes = [U32]
    p.probe_ec_contact_size.restype = ctypes.c_float


class Case:
    def __init__(self, seed):
        self.r = r = random.Random(seed)
        self.t = Trace()
        e = self.e = Emu(self.t)
        self.pokes, self.pokes8, self.strings = [], [], {}
        self.objects = []
        for vt, cls, name in CLASSES:
            o = e.world_alloc(0x400)
            e.w32(o, vt)
            self.put(o + 0xc0, 1 << r.randrange(0, 6))
            self.put(o + 0x68, f2u(r.choice([0.0, 2.5, 10.0, r.uniform(1, 40)])))
            self.put8(o + 0xe8, r.randrange(0, 0x40))
            self.put(o + 0xec, self.string(f'{name}{seed}') if r.random() < 0.8 else 0)
            node, data = e.world_alloc(0x40), e.world_alloc(0x100)
            self.put(o + 0x14, node)
            self.put(node + 0x38, data)
            self.put8(data, 0x20 if r.random() < 0.3 else 0)
            for i in range(3):
                self.put(data + 0x84 + 4 * i, f2u(r.uniform(-900, 900)))
                self.put(o + 0x200 + 4 * i, f2u(r.uniform(-900, 900)))  # zEntity3d's place, 0x5a85e0's answer
            self.objects.append((o, cls, name))
        for t, base in TABLES.items():
            for k in range(3):
                self.put(base + 4 * k, e.world_alloc(0x10) if r.random() < 0.85 else 0)
        pilot = e.world_alloc(0x40)
        self.put(PILOT, pilot)
        mechs = [o for o, cls, _ in self.objects if cls == 1]
        self.put(pilot + 0x14, r.choice(mechs + [0]))
        pname = f'pilot{seed}' if r.random() < 0.9 else ''
        e.cpu.write(pilot + 0x18, pname.encode() + b'\0')
        self.strings[pilot + 0x18] = pname
        names = {o: (self.string(f'ai{seed}_{n}') if r.random() < 0.7 else 0) for o, _, n in self.objects}
        self.t.script['0x004aee70'] = lambda n, a: names.get(a[0], 0)
        self.t.script['0x004709c0'] = lambda n, a: names.get(a[0], 0)
        self.t.script['0x005a85e0'] = lambda n, a: a[0] + 0x200
        e.stub(0x4aee70, '0x004aee70', 'c')
        e.stub(0x4709c0, '0x004709c0', 'c')
        e.stub(0x5a85e0, '0x005a85e0', 'c')
        e.stub(0x4ed280, '0x004ed280', 'c,d')
        e.stub(0x51e9d0, '@push', 'c')
        e.stub(0x51fde0, '@ident', '')
        e.stub(0x51ea00, '@pop', '')
        self.out = e.world_alloc(0x10)
        # 0x4e7070 returns on the x87 stack: a thunk stores it.
        th = e.world_alloc(0x20)
        e.cpu.write(th, call_abs(th, 0x4e7070) + b'\xd9\x1d' + struct.pack('<I', self.out) + b'\xc3')
        self.size_thunk = th
        self.contact = e.world_alloc(0x40)

    def put(self, a, v):
        self.e.w32(a, v & 0xffffffff)
        self.pokes.append((a, v & 0xffffffff))

    def put8(self, a, v):
        self.e.w8(a, v)
        self.pokes8.append((a, v))

    def string(self, s):
        a = self.e.world_string(s)
        self.strings[a] = s
        return a

    def slot(self, o, off):
        return self.e.r32(self.e.r32(o) + off)

    def original(self, o):
        e = self.e
        out = {}
        for k in range(3):
            out[f'blip{k}'] = e.call(self.slot(o, 0x3c), ecx=o, stack=(k,))
        out['team'] = e.r32(e.call(self.slot(o, 0x28), ecx=o) + 0xc0)
        out['mech'] = e.call(self.slot(o, 0x50), ecx=o)
        out['hidden'] = e.call(self.slot(o, 0xa8), ecx=o) & 0xff
        p = e.call(self.slot(o, 0xa4), ecx=o)
        out['name'] = e.cstr(p) if p else None
        p = e.call(self.slot(o, 0xbc), ecx=o)
        out['centre'] = tuple(e.r32(p + 4 * i) for i in range(3))
        e.w32(self.contact + 4, o)
        out['vehicle'] = e.call(0x4e6fe0, ecx=self.contact)
        e.call(self.size_thunk, ecx=self.contact)
        out['size'] = e.r32(self.out)
        return out

    def rebuild(self, probe, o):
        out = {}
        for k in range(3):
            out[f'blip{k}'] = probe.probe_ec_blip(o, k)
        out['team'] = probe.probe_ec_team(o)
        out['mech'] = probe.probe_ec_mech(o)
        out['hidden'] = probe.probe_ec_hidden(o)
        buf = ctypes.create_string_buffer(256)
        out['name'] = buf.value.decode('latin-1') if probe.probe_ec_name(o, buf, 256) == 0 else None
        c = (ctypes.c_float * 3)()
        probe.probe_ec_centre(o, c)
        out['centre'] = tuple(f2u(x) for x in c)
        out['vehicle'] = probe.probe_ec_contact_vehicle(o)
        out['size'] = f2u(probe.probe_ec_contact_size(o))
        return out


def run_case(probe, seed):
    case = Case(seed)
    t = Trace(dict(case.t.script))
    probe.probe_app_set_callback(t.callback())
    probe.probe_ec_reset()
    for a, v in case.pokes:
        probe.probe_ec_poke(a, v)
    for a, v in case.pokes8:
        probe.probe_ec_poke8(a, v)
    for a, s in case.strings.items():
        probe.probe_ec_string(a, s.encode('latin-1'))
    for o, cls, _ in case.objects:
        probe.probe_ec_class(o, cls)
    for o, cls, name in case.objects:
        case.t.reset()
        t.reset()
        a = case.original(o)
        b = case.rebuild(probe, o)
        label = f'seed {seed} {name}'
        if not compare(label, case.t.log, t.log):
            die()
        if a != b:
            bad = [k for k in a if a[k] != b[k]]
            print(f'{label}: {", ".join(f"{k} original {a[k]!r} rebuild {b[k]!r}" for k in bad)}')
            die()
        COUNT['objects'] += 1
        COUNT['answers'] += len(a)
        COUNT['calls'] += len(case.t.log)
        COUNT['refreshes'] += sum(1 for k, _ in case.t.log if k == '0x004ed280')
    # The size with no object seen: 5 ([0x5cd5f4]).
    case.e.w32(case.contact + 4, 0)
    case.e.call(case.size_thunk, ecx=case.contact)
    if case.e.r32(case.out) != f2u(probe.probe_ec_contact_size(0)):
        print(f'seed {seed}: the size with nothing seen differs')
        die()


def main():
    probe = load_probe()
    setup_probe(probe)
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    for seed in range(count):
        run_case(probe, seed)
    print(f'ok: {count} cases, {len(CLASSES)} vtables ({", ".join(f"{v} {k}" for k, v in COUNT.items())}), '
          'every slot as the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()

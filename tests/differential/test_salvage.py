"""A won mission's salvage (docs/campaign.md section 9) against the original x86.

    cmake --build build && .venv/bin/python -P tests/differential/test_salvage.py [runs] [first] [--verbose]

The original's store (test_pilot.py's emulator: its operator new, the store
at [0x00609990]) is filled by a plan through its own adds (0x00471c10 a mech, 0x00472630 a weapon's copy,
0x00471840 a weapon, 0x00471940 ammo, 0x00471a20 equipment); then 0x0047ae10
(CL 0: won, DL the whole team or not) runs over a fake zEntity list: entities
without a mech, the player's mech, allies and others by team, each with its
parts (torso, head, legs, arms, hip, hands, calves, feet; destroyed or not,
some missing), its weapon manager (weapons of every type and tech, beams,
shared and empty bins, infinite ones, missing slots and items), its loadout
(equipment on standing or destroyed parts, heat sinks, weapons and ammo) and
its sinks; 0x0041df10 (the definition's tech) is stubbed, rand answers from
the plan. The port (tests/probe_salvage.cpp: the ported store adds,
`lance_salvage`) runs the same. Compared: the store's items, the bins left,
the rand calls used. The player's mech goes into the campaign's +0x64
store (a real store at the fake game's +0x64), its allies' into [0x00609990].
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from twod_world import IAT  # noqa: E402
from test_pilot import FS, KIND_SIZE, KIND_VT, SALVAGE, Emulated, i32  # noqa: E402

probe = load_probe()
probe.probe_salvage_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]

RESULTS, MECH_TECH = 0x47ae10, 0x41df10
ADD_MECH, ADD_WEAPON, ADD_AMMO, ADD_EQUIP = 0x471c10, 0x471840, 0x471940, 0x471a20
CURRENT, LIST = 0x60861c, 0x73655c
RAND_IAT = 0x5bb668


class Salvage(Emulated):
    def __init__(self, seed):
        Emulated.__init__(self, FS())
        self.rands = []
        self.used = 0

        def rand(c):
            self.used += 1
            return self.rands.pop(0) if self.rands else 0
        self.cpu.stub_import(RAND_IAT, rand, 0)
        self.cpu.stub_function(MECH_TECH, lambda c: self.r32(c.ecx + 0x6f0))
        self.code = {}

    def fn(self, hexcode):
        if hexcode not in self.code:
            self.code[hexcode] = self.cpu.code(bytes.fromhex(hexcode))
        return self.code[hexcode]

    def part(self, gone, extra=0):
        p = self.alloc(0x80)
        self.cpu.write(p + 0x48, bytes([gone]))
        return p

    def mech(self, e):
        """A fake mech from the entity's description (the same goes to the probe)."""
        cpu = self.cpu
        m = self.alloc(0x700)
        vt = self.alloc(0x100)
        cpu.u32(m, vt)
        me = self.fn('89c8c3')  # mov eax, ecx; ret
        for off in (0x28, 0x2c):
            cpu.u32(vt + off, me)
        cpu.u32(vt + 0x50, me if e['mech'] else self.fn('31c0c3'))
        cpu.u32(m + 0xc0, e['team'])
        if e['has']:
            d = self.alloc(0x20)
            cpu.u32(d + 0xc, e['type'] & 0xffffffff)
            cpu.u32(m + 0x684, d)
        cpu.u32(m + 0x6f0, e['tech'])
        # parts by id: 1 torso, 2/3 arms, 4/5 legs, 6 head, 15 hip; 9..14 hands, calves, feet
        st = e['parts']
        torso, a0, a1, l0, l1, head, hip = (self.part(st[i] == 2) for i in (1, 2, 3, 4, 5, 6, 15))
        for off, p in ((0x39c, torso), (0x394, a0), (0x398, a1), (0x38c, l0), (0x390, l1), (0x3a4, head),
                       (0x3a0, hip)):
            cpu.u32(m + off, p)
        for pid, owner, off in ((9, a0, 0x74), (10, a1, 0x74), (11, l0, 0x74), (12, l1, 0x74), (13, l0, 0x78),
                                (14, l1, 0x78)):
            if st[pid]:
                cpu.u32(owner + off, self.part(st[pid] == 2))
        if e['manager']:
            wm = self.alloc(0x94)
            cpu.u32(wm + 0x24, len(e['weapons']))
            bins = []
            for b in e['bins']:
                a = self.alloc(0x18)
                cpu.u32(a, 1)
                if b == -1:
                    cpu.write(a + 0xc, b'\1')
                else:
                    cpu.u32(a + 4, b)
                    cpu.u32(a + 8, b)
                bins.append(a)
            e['bin_addrs'] = bins
            for i, w in enumerate(e['weapons']):
                if not w['present']:
                    continue
                o = self.alloc(0x98)
                wvt = self.alloc(0x40)
                cpu.u32(o, wvt)
                cpu.u32(wvt + 8, self.fn('b001c3' if w['beam'] else '30c0c3'))
                d = self.alloc(0x1f4)
                t = self.alloc(0xc8)
                cpu.u32(t, w['type'] & 0xffffffff)
                cpu.u32(d + 0x1f0, t)
                cpu.u32(o + 0x2c, d)
                if w['item']:
                    it = self.alloc(0x1c)
                    cpu.u32(it + 0x18, w['tech'])
                    cpu.u32(o + 0x90, it)
                if w['bin'] >= 0:
                    cpu.u32(o + 0x58, bins[w['bin']])
                cpu.u32(wm + 0x40 + 4 * i, o)
            cpu.u32(m + 0xd4, wm)
        if e['config'] is not None:
            c = self.alloc(0xd8)
            head_n = self.alloc(0xc)
            cpu.u32(head_n, head_n)
            cpu.u32(head_n + 4, head_n)
            cpu.u32(c + 8, head_n)
            for k, n, part, ident, tech in e['config']:
                it = self.item(k, ident & 0xffffffff, tech)
                cpu.u32(it + 4, n)
                cpu.u32(it + 0xc, part & 0xffffffff)
                node, last = self.alloc(0xc), self.r32(head_n + 4)
                cpu.u32(node, head_n)
                cpu.u32(node + 4, last)
                cpu.u32(node + 8, it)
                cpu.u32(last, node)
                cpu.u32(head_n + 4, node)
            cpu.u32(c + 0xc, len(e['config']))
            cpu.u32(m + 0x3ec, c)
        cpu.u32(m + 0x658, e['sink_tech'])
        cpu.u32(m + 0x65c, e['sinks'] & 0xffffffff)
        cpu.write(m + 0x660, bytes([e['doubles']]))
        return m

    def run(self, cfg):
        cpu, s = self.cpu, self.r32(SALVAGE)
        for op in cfg['fill']:
            if op[0] == 'mech':
                self.call(ADD_MECH, ecx=s, stack=(op[1] & 0xffffffff,))
            elif op[0] == 'weapon':
                self.call(ADD_WEAPON, ecx=s, stack=(op[1], op[2]))
            elif op[0] == 'copyw':
                it = self.item(1, op[1], op[2])
                self.call(0x472630, ecx=s, stack=(it, op[3]))
            elif op[0] == 'ammo':
                self.call(ADD_AMMO, ecx=s, stack=(op[1], op[2] & 0xffffffff))
            else:
                self.call(ADD_EQUIP, ecx=s, stack=(op[1], op[2], op[3] & 0xffffffff))
        ents = []
        player = None
        for e in cfg['ents']:
            m = self.mech(e)
            ents.append(m)
            if e['player']:
                player = m
        if player is None:
            player = self.alloc(0x700)
        cpu.u32(player + 0xc4, cfg['allies'])
        pilot, game, gvt = self.alloc(0x84), self.alloc(0xd0), self.alloc(0x40)
        cpu.u32(game, gvt)
        self.call(0x470ce0, ecx=game + 0x64, stack=(0,))  # the campaign's +0x64 store
        cpu.u32(gvt + 0x2c, self.fn('b8%02x000000c3' % cfg['kind']))
        cpu.u32(pilot + 0x6c, game)
        cpu.u32(pilot + 0x14, player)
        cpu.u32(CURRENT, pilot)
        arr = self.alloc(4 * max(1, len(ents)))
        for i, m in enumerate(ents):
            cpu.u32(arr + 4 * i, m)
        cpu.u32(LIST, arr)
        cpu.u32(LIST + 4, arr + 4 * len(ents))
        self.rands = list(cfg['rands'])
        self.used = 0
        self.call(RESULTS, ecx=0, edx=cfg['team'])
        out = self.store('store', s)
        own = self.store('own', game + 0x64)
        bins = []
        for e in cfg['ents']:
            for a in e.get('bin_addrs', []):
                bins.append(-1 if self.r8(a + 0xc) else i32(self.r32(a + 4)))
        return out, own, bins, self.used


def store_text(text):
    """test_pilot's store dump, as the probe's: the count, then the items."""
    body = text[text.index('{') + 1:text.rindex('}')]
    parts = body.split(' ', 3)  # b.. f.. n.. items
    return f'store{{{parts[2]} {parts[3] if len(parts) > 3 else ""}}}'


def make_cfg(seed):
    r = random.Random(f'salvage:{seed}')
    fill = []
    for _ in range(r.randrange(0, 12)):
        k = r.choice(['mech', 'weapon', 'ammo', 'equip', 'copyw'])
        if k == 'mech':
            fill.append(('mech', r.choice([0, 3, 7, 13, 18])))
        elif k == 'weapon':
            fill.append(('weapon', r.choice([r.randrange(37), 2, 9, 15, 35]), r.choice([1, 2, 3])))
        elif k == 'ammo':
            fill.append(('ammo', r.randrange(37), r.choice([1, 20, -5])))
        elif k == 'equip':
            fill.append(('equip', r.choice([0, 1, 2, 9, 10, 11]), r.choice([1, 2, 3]), r.choice([1, 2, -1, -3])))
        else:  # a weapon through add_copy: its place 0 (add_weapon clears it)
            fill.append(('copyw', r.choice([2, 9, 15, 35]), r.choice([1, 2, 3]), r.choice([1, 2])))
    ents = []
    player_at = r.choice([0, 0, 1, 2, None])
    for i in range(r.choice([0, 1, 2, 3, 4, 5])):
        nb = r.choice([0, 1, 2, 3])
        bins = [r.choice([0, 5, 20, -1, 1]) for _ in range(nb)]
        weapons = []
        for _ in range(r.choice([0, 1, 2, 4, 6])):
            weapons.append({'present': r.random() < 0.9, 'type': r.choice([r.randrange(37), 0x20, 9, 4, 0, 35, 15, 2]),
                            'item': r.random() < 0.5, 'tech': r.choice([1, 2, 3]), 'beam': r.random() < 0.25,
                            'bin': r.choice([-1] + list(range(nb)))})
        parts = [0] * 16
        for pid in (1, 2, 3, 4, 5, 6, 15):
            parts[pid] = r.choice([1, 1, 1, 2])
        for pid in range(9, 15):
            parts[pid] = r.choice([0, 1, 1, 2])
        config = None
        if r.random() < 0.8:
            config = []
            for _ in range(r.choice([0, 1, 3, 5])):
                k = r.choice([3, 3, 3, 1, 2])
                config.append((k, r.choice([1, 2, 4]), r.choice([-1, 1, 2, 4, 6, 9, 11, 13, 15, 0, 20]),
                               r.choice([0, 1, 2, 5, 9, 10, 11]) if k == 3 else r.randrange(37), r.choice([1, 2, 3])))
        ents.append({'mech': r.random() < 0.9, 'player': i == player_at, 'team': r.choice([0, 1, 2, 3, 0x10]),
                     'has': r.random() < 0.95, 'type': r.choice([0, 3, 7, 8, 13, 18, -1]), 'tech': r.choice([1, 2, 3]),
                     'parts': parts, 'manager': r.random() < 0.9, 'weapons': weapons, 'bins': bins,
                     'config': config, 'sink_tech': r.choice([1, 2, 3]), 'sinks': r.choice([8, 10, 10, 11, 14, 20]),
                     'doubles': r.choice([0, 1])})
    return {'fill': fill, 'ents': ents, 'allies': r.choice([0, 1, 3, 0x13]), 'team': r.choice([0, 1]),
            'kind': r.choice([1, 1, 1, 2]), 'rands': [r.randrange(32768) for _ in range(8)]}


def probe_setup(cfg):
    out = []
    for op in cfg['fill']:
        out.append(' '.join(str(x) for x in op))
    for v in cfg['rands']:
        out.append(f'rand {v}')
    for e in cfg['ents']:
        st = e['parts']
        torso, head, l0, l1 = (int(st[i] == 2) for i in (1, 6, 4, 5))
        f = [1 if e['mech'] else 0, 1 if e['player'] else 0, e['team'], 1 if e['has'] else 0,
             e['type'] if e['has'] else -1, e['tech'], torso, head, l0, l1, e['sink_tech'], e['sinks'],
             e['doubles'], 16] + st
        f += [1 if e['manager'] else 0, len(e['weapons']) if e['manager'] else 0]
        if e['manager']:
            for w in e['weapons']:
                f += [1 if w['present'] else 0, w['type'], 1 if w['item'] else 0, w['tech'], 1 if w['beam'] else 0,
                      w['bin']]
        f += [len(e['bins']) if e['manager'] else 0] + (e['bins'] if e['manager'] else [])
        if e['config'] is None:
            f += [-1]
        else:
            f += [len(e['config'])]
            for it in e['config']:
                f += list(it)
        out.append('ent ' + ' '.join(str(x) for x in f))
    out.append(f"run {cfg['allies']} {cfg['team']} {1 if cfg['kind'] == 1 else 0}")
    return '\n'.join(out)


def run_case(seed, verbose):
    cfg = make_cfg(seed)
    em = Salvage(seed)
    store, own, bins, used = em.run(cfg)
    want = f"{store_text(store)}\n{store_text(own).replace('store', 'own', 1)}\nbins{''.join(f' {b}' for b in bins)}\nrand {used}\n"
    buf = ctypes.create_string_buffer(1 << 16)
    n = probe.probe_salvage_run(probe_setup(cfg).encode(), buf, len(buf))
    assert n >= 0, 'probe failed'
    got = buf.value.decode()
    if got.replace(' }', '}') != want.replace(' }', '}'):
        print(f'salvage seed {seed}: differ\n  original: {want}\n  rebuild:  {got}')
        return False, cfg
    if verbose:
        print(f'salvage seed {seed}: {store_text(store)[:200]}')
    return True, cfg


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    runs = int(args[0]) if args else 200
    first = int(args[1]) if len(args) > 1 else 0
    verbose = '--verbose' in sys.argv
    passed = mechs = 0
    for seed in range(first, first + runs):
        ok, cfg = run_case(seed, verbose)
        passed += ok
        mechs += len(cfg['ents'])
    print(f'salvage: {passed}/{runs} runs identical ({mechs} entities)')
    return 0 if passed == runs else 1


if __name__ == '__main__':
    code = 1
    try:
        code = main()
    except BaseException:
        import traceback
        traceback.print_exc()
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(code)

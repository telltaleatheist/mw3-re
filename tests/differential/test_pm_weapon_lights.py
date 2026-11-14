"""Pirate's Moon's effect block reader 0x5a6700 against the port's zwep::read_effect (tests/probe_pm_weapon_lights.cpp),
on PM's exe: a FIRE block's EFFECT read only with the world's switch on ([0xb0e434], off at object detail 2; else its
MODEL), the ANIMATION entry's names into two slots (a third ends the entry) and each number into the flag, the rest as
MW3's (MODEL, the four animations, RANDOM_ROTATE, SOUND, BOUNCE_SOUND, the attach node cleared).

Random blocks of every entry, for FIRE and other keys, with the switch on and off. The exe reads a tree written into its
memory, the port the same tree from the reader's binary form; PM's own find (0x578950) runs, and the lookups (an
animation 0x524b50, a sound 0x590a70, an effect 0x5271c0, a node 0x50b920) answer a hash of the name on both sides.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_weapon_lights.py [cases]
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zreader  # noqa: E402
from original import ROOT  # noqa: E402

READ, SWITCH = 0x5a6700, 0xb0e434
INT, FLOAT, STRING, LIST = 1, 2, 3, 4

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_read_effect.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p]
OUT = (ctypes.c_uint32 * 64)()


def fnv(s, salt):
    h = 2166136261 ^ salt
    for c in s.encode():
        h = ((h ^ c) * 16777619) & 0xffffffff
    return h | 1


def serial(n):
    kind, v = n
    if kind == INT:
        return struct.pack('<Ii', INT, v)
    if kind == STRING:
        b = v.encode()
        return struct.pack('<II', STRING, len(b)) + b
    return struct.pack('<II', LIST, len(v) + 1) + b''.join(serial(x) for x in v)


class Exe:
    def __init__(self):
        e = self.e = test_pm_zreader.Emulated()
        self.uc = e.cpu._uc
        for addr, salt in ((0x524b50, 1), (0x590a70, 2), (0x5271c0, 3)):
            e.native_stub(addr, lambda c, s=salt: fnv(self.cstr(c.arg(0)), s), 0)
        e.native_stub(0x50b920, lambda c: fnv(self.cstr(c.arg(1)), 4 + c.arg(0)), 0)

    def cstr(self, a):
        out = bytearray()
        while True:
            b = self.uc.mem_read(a + len(out), 1)[0]
            if not b:
                return out.decode()
            out.append(b)

    def node(self, at, n):
        kind, v = n
        if kind == INT:
            self.uc.mem_write(at, struct.pack('<Ii', INT, v))
        elif kind == STRING:
            s = self.e.alloc(len(v) + 1)
            self.uc.mem_write(s, v.encode() + b'\0')
            self.uc.mem_write(at, struct.pack('<II', STRING, s))
        else:
            arr = self.e.alloc(8 * (len(v) + 1))
            self.uc.mem_write(arr, struct.pack('<Ii', INT, len(v) + 1))
            for i, x in enumerate(v):
                self.node(arr + 8 * (i + 1), x)
            self.uc.mem_write(at, struct.pack('<II', LIST, arr))

    def run(self, tree, key, switch):
        root = self.e.alloc(8)
        self.node(root, tree)
        k = self.e.alloc(len(key) + 1)
        self.uc.mem_write(k, key.encode() + b'\0')
        eff = self.e.alloc(0x70)
        self.uc.mem_write(eff, bytes(0x70))
        self.uc.mem_write(eff + 0x5c, struct.pack('<I', 0x5a5a5a5a))
        self.uc.mem_write(SWITCH, struct.pack('<I', switch))
        self.e.call(READ, args=(root, eff, k))
        w = struct.unpack('<28I', bytes(self.uc.mem_read(eff, 0x70)))
        # flags, the two animations, attached, model animation, surface, min range, effect, model, sounds, bounces, attach
        return [w[0], w[1], w[2], w[5], w[6], w[7], w[8], w[9], w[10], w[11], *w[12:16], w[16], *w[17:21], w[23]]


NAMES = ['flash.flt', 'lite.flt', 'smoke.flt', 'muz_red.flt', 'beam_glow.flt']


def entry(r, name):
    if name in ('ANIMATION',):
        items = []
        for _ in range(r.randrange(0, 5)):
            items.append((STRING, r.choice(NAMES)) if r.random() < 0.65 else (INT, r.choice([0, 1, 2, 3])))
        return items
    if name == 'RANDOM_ROTATE':
        return [(INT, r.choice([0, 1, 3]))]
    if name in ('SOUND', 'BOUNCE_SOUND'):
        return [(STRING, r.choice(['shot.wav', 'zap.wav', 'boom.wav'])) for _ in range(r.randrange(0, 5))]
    return [] if r.random() < 0.15 else [(STRING, r.choice(NAMES))]


def case(r):
    key = r.choice(['FIRE', 'FIRE', 'FLYOUT', 'default', 'water'])
    names = ['EFFECT', 'MODEL', 'ANIMATION', 'ANIMATION_ATTACHED', 'ANIMATION_MIN_RANGE', 'MODEL_ANIMATION',
             'SURFACE_ANIMATION', 'RANDOM_ROTATE', 'SOUND', 'BOUNCE_SOUND']
    body = []
    for n in names:
        if r.random() < 0.55:
            body += [(STRING, n), (LIST, entry(r, n))]
    blocks = [(STRING, key), (LIST, body)] if r.random() < 0.95 else [(STRING, 'OTHER'), (LIST, body)]
    if r.random() < 0.3:
        blocks = [(STRING, 'NAME'), (LIST, [(STRING, 'x')])] + blocks
    return (LIST, blocks), key, r.choice([0, 1])


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    exe = Exe()
    r = random.Random(5670)
    bad, two, fire_fx, fire_no = 0, 0, 0, 0
    for n in range(cases):
        tree, key, switch = case(r)
        e = exe.run(tree, key, switch)
        blob = serial(tree)
        k = probe.probe_pm_read_effect(blob, len(blob), key.encode(), switch, OUT)
        p = list(OUT[:k])
        two += bool(e[1] and e[2])
        fire_fx += key == 'FIRE' and switch == 1 and e[7] != 0
        fire_no += key == 'FIRE' and switch == 0 and e[8] != 0
        if e != p:
            bad += 1
            if bad <= 5:
                print(f'FAIL case {n} key {key} switch {switch}: {tree}\n  exe  {e}\n  port {p}')
    print(f'{cases} effect blocks ({two} with two animations, {fire_fx} FIRE effects read, {fire_no} FIRE models in '
          f'their place), {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()

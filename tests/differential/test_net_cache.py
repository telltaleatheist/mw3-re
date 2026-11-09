"""The AI net file's record compare (0x4c6680, NetNodeRec::same in zainet_node.cpp; docs/editor.md, The record
cache) against the original x86: the test the record cache (0x4dc2b0) uses to keep an edit or drop it as the
file's own. Random records in the exe's layout (0xb4 bytes; the names, links vector, valve union and
assignment as the exe holds them) and as the port's, each pair one field apart, or none, or a name equal in
another buffer.

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_net_cache.py [runs]

The answers must match, but where the port's fix departs: the exe leaves wait_time (+0x28, +0x2c) and the
valve (+0x30..+0x3c) out, so a record differing only there is "the same" to it and not to the port (counted;
docs/fixes.md). Breaks, by hand in zainet_node.cpp, each must fail: the place compared for kind 1 too; the
auxname left out; the links compared by count only.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402

SAME = 0x4c6680
OURS = {0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c}  # wait_time and the valve: the port compares them, the exe not

probe = load_probe()
probe.probe_net_rec_same.argtypes = [ctypes.c_char_p, ctypes.c_char_p]

INTS = [0x18, 0x20, 0x28, 0x30, 0x3c, 0x40, 0x44, 0x48, 0x50, 0x54, 0x58, 0x5c, 0x60, 0x64]
FLOATS = [0x0c, 0x10, 0x14, 0x1c, 0x24, 0x2c, 0x4c, 0x84, 0x8c]
BYTES = [0x38, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x80, 0x88, 0x89, 0x98, 0x99]
PTRS = [0x34, 0x78, 0x7c, 0x90, 0x94]  # valves and the sound: compared as pointers
VALVES = [0, 0x1000, 0x2000, 0x3000]
NAMES = [None, 'a', 'node', 'Gate_2', 'x']


def record(rng):
    b = bytearray(0xb4)
    struct.pack_into('<i', b, 4, rng.choice([0, 1]))
    struct.pack_into('<i', b, 8, rng.randrange(0, 40))
    for off in INTS:
        struct.pack_into('<i', b, off, rng.choice([0, 1, rng.randrange(-5, 50)]))
    for off in FLOATS:
        struct.pack_into('<f', b, off, rng.choice([0.0, 1.0, rng.uniform(-500, 500)]))
    for off in BYTES:
        b[off] = rng.choice([0, 1, rng.randrange(256)])
    struct.pack_into('<h', b, 0x74, rng.randrange(-30, 30))
    for off in PTRS:
        struct.pack_into('<I', b, off, rng.choice(VALVES))
    return dict(b=b, name=rng.choice(NAMES), aux=rng.choice(NAMES), gate=rng.choice(NAMES),
                links=[rng.randrange(-1, 20) for _ in range(rng.randrange(0, 4))],
                union=None if rng.random() < 0.6 else [(rng.choice(VALVES[1:]), rng.randrange(1, 4)) for _ in range(rng.randrange(0, 3))],
                assign=None if rng.random() < 0.6 else (rng.choice(VALVES[1:]), rng.randrange(0, 9)))


def mutate(rng, r):
    o = dict(r, b=bytearray(r['b']), links=list(r['links']), union=None if r['union'] is None else list(r['union']))
    what = rng.randrange(12)
    field = None
    if what == 0:
        return o, 'none'
    if what == 1:
        return o, 'names in other buffers'  # the port's strdup always is; the exe's buffers are written apart too
    if what == 2:
        field = rng.choice(INTS + [4, 8])
        struct.pack_into('<i', o['b'], field, struct.unpack_from('<i', o['b'], field)[0] + rng.choice([1, -1, 7]))
    elif what == 3:
        field = rng.choice(FLOATS)
        struct.pack_into('<f', o['b'], field, struct.unpack_from('<f', o['b'], field)[0] + rng.choice([0.5, -1.0, 100.0]))
    elif what == 4:
        field = rng.choice(BYTES)
        o['b'][field] ^= rng.choice([1, 0x80])
    elif what == 5:
        field = 0x74
        o['b'][0x74] ^= 1
    elif what == 6:
        field = rng.choice(PTRS)
        struct.pack_into('<I', o['b'], field, rng.choice([v for v in VALVES if v != struct.unpack_from('<I', o['b'], field)[0]]))
    elif what == 7:
        k = rng.choice(['name', 'aux', 'gate'])
        o[k] = rng.choice([n for n in NAMES if n != o[k]])
        return o, k
    elif what == 8:
        if o['links'] and rng.random() < 0.5:
            o['links'][rng.randrange(len(o['links']))] += 1
        else:
            o['links'].append(3)
        return o, 'links'
    elif what == 9:
        o['union'] = None if o['union'] is not None and rng.random() < 0.3 else (o['union'] or []) + [(0x1000, 1)]
        return o, 'union'
    elif what == 10:
        o['assign'] = None if o['assign'] is not None and rng.random() < 0.3 else (0x2000, (o['assign'] or (0, 0))[1] + 1)
        return o, 'assign'
    else:
        o['b'][4] = 1 - o['b'][4]  # the other kind: the place counts only for kind 0
        field = 4
    return o, '+%#x' % field


def line(r):
    words = ' '.join(str(w) for w in struct.unpack('<45I', bytes(r['b'])))
    t = lambda s: s if s else '-'  # noqa: E731
    links = ','.join(str(x) for x in r['links']) if r['links'] else '-'
    uni = '-' if not r['union'] else ','.join('%d:%d' % p for p in r['union'])  # an empty union is made none below
    assign = '-' if r['assign'] is None else '%d:%d' % r['assign']
    return '%s %s %s %s %s %s %s' % (words, t(r['name']), t(r['aux']), t(r['gate']), links, uni, assign)


class Exe:
    def __init__(self):
        self.o = Original()

    def put(self, r):
        o = self.o
        a = o.alloc(0xb4)
        o.write(a, bytes(r['b']))

        def text(s):
            if s is None:
                return 0
            p = o.alloc(len(s) + 1)
            o.write(p, s.encode() + b'\0')
            return p
        o.u32(a + 0x9c, text(r['name']))
        o.u32(a + 0xa0, text(r['aux']))
        o.u32(a + 0xa8, text(r['gate']))
        v = o.alloc(0x10)  # std::vector<int>: +4 begin, +8 end
        data = o.alloc(4 * max(1, len(r['links'])))
        o.write(data, struct.pack('<%di' % len(r['links']), *r['links']))
        o.u32(v + 4, data if r['links'] else 0)
        o.u32(v + 8, data + 4 * len(r['links']) if r['links'] else 0)
        o.u32(a + 0xa4, v)
        if r['union'] is None:
            o.u32(a + 0xac, 0)
        else:
            u = o.alloc(0x10)
            d = o.alloc(8 * max(1, len(r['union'])))
            for i, (val, n) in enumerate(r['union']):
                o.u32(d + 8 * i, val)
                o.u32(d + 8 * i + 4, n)
            o.u32(u + 4, d if r['union'] else 0)
            o.u32(u + 8, d + 8 * len(r['union']) if r['union'] else 0)
            o.u32(a + 0xac, u)
        if r['assign'] is None:
            o.u32(a + 0xb0, 0)
        else:
            s = o.alloc(8)
            o.u32(s, r['assign'][0])
            o.u32(s + 4, r['assign'][1])
            o.u32(a + 0xb0, s)
        return a

    def same(self, a, b):
        from unicorn.x86_const import UC_X86_REG_EAX
        self.o.call(SAME, ecx=self.put(a), stack_args=(self.put(b),))
        return self.o.uc.reg_read(UC_X86_REG_EAX) & 0xff


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    rng = random.Random(int(os.environ.get('SEED', '4')))
    exe = None
    bad = fixed = 0
    kinds = {}
    for r in range(runs):
        if r % 500 == 0:
            exe = Exe()  # a fresh heap now and then
        a = record(rng)
        if a['union'] == []:
            a['union'] = None
        b, what = mutate(rng, a)
        if b['union'] == []:
            b['union'] = None
        e = exe.same(a, b)
        p = probe.probe_net_rec_same(line(a).encode(), line(b).encode())
        kinds.setdefault(what, [0, 0])[e] += 1
        if p == e:
            continue
        off = int(what[1:], 16) if what.startswith('+') else None
        if off in OURS and e == 1 and p == 0:
            fixed += 1
            continue
        bad += 1
        if bad <= 5:
            print('run %d (%s): exe %d, port %d' % (r, what, e, p))
    seen = ', '.join('%s %d/%d' % (k, v[1], v[0]) for k, v in sorted(kinds.items()))
    print('%d pairs, %d differ, %d where the fix departs (wait_time, the valve)' % (runs, bad, fixed))
    print('  the exe\'s answers, same/differs: ' + seen)
    return 1 if bad else 0


if __name__ == '__main__':
    rc = main()
    sys.stdout.flush()
    os._exit(rc)

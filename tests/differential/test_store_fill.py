"""The "everything" store (0x00470e30, Store::fill_all; docs/pilot.md) against the original x86.

    cmake --build build && .venv/bin/python -P tests/differential/test_store_fill.py

The mech lab's, Instant Action's, multiplayer's and training's store: a
store (0x00470ce0) given 0..20 mechs first (0x00471c10), then filled; every
item compared in order (kind by vtable, id, tech, count, the +0x10 byte,
group, part), with the store's full flag and count. The port also runs with
its FIX (bug) on: the plain PPC (weapon 37, Inner Sphere, no ammo) is listed.
"""
import ctypes
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import Original  # noqa: E402
from calltrace import load_probe  # noqa: E402
from test_pilot import NEW, DELETE, STORE_CTOR, i32  # noqa: E402

FILL, ADD_MECH = 0x470e30, 0x471c10
VT_KIND = {0x5bc160: 0, 0x5c0fc0: 1, 0x5bd150: 2, 0x5bd180: 3}
probe = load_probe()
probe.probe_store_fill.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]


def original(before):
    cpu = Original()
    heap = [0x30000000]
    cpu.uc.mem_map(heap[0], 0x1000000)

    def new(c):
        p = heap[0]
        heap[0] += (c.arg(0) + 15) & ~15
        return p
    cpu.stub_function(NEW, new)
    cpu.stub_function(DELETE, lambda c: 0)
    st = new(type('A', (), {'arg': lambda self, n: 0x14})())
    cpu.call(STORE_CTOR, ecx=st, stack_args=(0,))
    for i in range(before):
        cpu.call(ADD_MECH, ecx=st, stack_args=(i % 19,))
    cpu.call(FILL, ecx=st)
    r = lambda a: int.from_bytes(cpu.read(a, 4), 'little')
    out = [f'{cpu.read(st + 0x10, 1)[0]} {i32(r(st + 0xc))}']
    head = r(st + 8)
    node = r(head)
    while node != head:
        it = r(node + 8)
        kind = VT_KIND[r(it)]
        ident = r(it + (0x18 if kind == 3 else 0x14))
        tech = r(it + 0x18) if kind == 1 else r(it + 0x1c) if kind == 3 else 0
        out.append(f'{kind} {i32(ident)} {tech} {i32(r(it + 4))} {cpu.read(it + 0x10, 1)[0]} {i32(r(it + 8))} '
                   f'{i32(r(it + 0xc))}')
        node = r(node)
    return '\n'.join(out) + '\n'


def port(before, bugs):
    buf = ctypes.create_string_buffer(1 << 16)
    assert probe.probe_store_fill(before, bugs, buf, len(buf)) >= 0
    return buf.value.decode()


def main():
    ok = 0
    cases = [0, 1, 5, 20]
    for before in cases:
        o, p = original(before), port(before, 0)
        if o == p:
            ok += 1
        else:
            for a, b in zip(o.split('\n'), p.split('\n')):
                if a != b:
                    print(f'store fill ({before} before): original {a!r}, rebuild {b!r}')
                    break
            else:
                print(f'store fill ({before} before): lengths differ')
    o = original(0)
    fixed = port(0, 1)
    extra = [x for x in fixed.split('\n')[1:] if x.startswith(('1 37 ', '2 37 '))]
    good = extra == ['1 37 2 1 1 0 -1'] and fixed.split('\n')[0] == f'1 {int(o.split()[1]) + 1}'
    print(f'store fill: {ok}/{len(cases)} identical ({o.split()[1]} items: '
          f'{sum(1 for x in o.split(chr(10))[1:] if x.startswith("0 "))} mechs, '
          f'{sum(1 for x in o.split(chr(10))[1:] if x.startswith("1 "))} weapons, '
          f'{sum(1 for x in o.split(chr(10))[1:] if x.startswith("2 "))} ammo, '
          f'{sum(1 for x in o.split(chr(10))[1:] if x.startswith("3 "))} equipment); '
          f'with the plain PPC fix {"+1 (the PPC, Inner Sphere; no ammo)" if good else "WRONG: " + repr(extra)}')
    return 0 if ok == len(cases) and good else 1


if __name__ == '__main__':
    code = 1
    try:
        code = main()
    except BaseException:
        import traceback
        traceback.print_exc()
    sys.stdout.flush()
    os._exit(code)

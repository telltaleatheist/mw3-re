"""Pirate's Moon's saved-world animation states (pm-tests item 30): PM's activation saver 0x524e10 run on made
objects, its "Activation%04d" entry against zeff::anim_state_pack (the port's save_node) for the same states.
PM's object state is 0x3c bytes (MW3 1.2's 0x44): the live index, a word of bits (active, keeps a matrix, the
model's flag 8), the matrix or translation, rotation and scale, the model's +0x24. Our builds before this wrote
MW3's layout, which PM read out of step and died on (Freako's campaign saves on the original).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_anim_save.py

Compared: the entry's size, the record head, the status and count, and in each state the fields the exe sets
(the bits it leaves keep the stack's leftovers). BREAK=1 packs the matrix bit as MW3's word: it must fail.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402

SAVER, COUNT, RECORDS = 0x524e10, 0x63a478, 0x63a470
FIND_DEF, NODE_AT, BIND, LIVE_INDEX = 0x524b50, 0x515f70, 0x522ea0, 0x515f20
GET_T, GET_R, GET_S, MATRIX, WRITE, REPORT = 0x510270, 0x5100f0, 0x50ff80, 0x510630, 0x5cb930, 0x406cb0


def main():
    brk = int(os.environ.get('BREAK', '0'))
    lib = test_pm_zar.load_pm_probe()
    lib.probe_pm_anim_state_pack.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.c_int, ctypes.c_char_p]
    z = test_pm_zar.Emulated()
    uc = z.cpu._uc
    w = lambda a, v: uc.mem_write(a, struct.pack('<I', v & 0xffffffff))  # noqa: E731
    rng = random.Random(30)
    nodes = {}  # node address -> its state
    got = []
    z.native_stub(REPORT, lambda c: 0)

    def sprintf(c):  # only "Activation%04d" here
        out = z.cstr(c.arg(1)).decode('latin1').replace('%04d', f'{c.arg(2):04d}').encode('latin1') + b'\0'
        uc.mem_write(c.arg(0), out)
        return len(out) - 1
    z.cpu.stub_import(0x5bb658, sprintf)  # MW3's slot, translated to PM's 0x5de658
    z.native_stub(BIND, lambda c: 0)
    z.native_stub(LIVE_INDEX, lambda c: nodes[c.arg(0)]['index'])

    def floats3(base):
        def f(c):
            s = nodes[c.arg(0)]
            for k in range(3):
                uc.mem_write(c.arg(1 + k), struct.pack('<I', s['m'][base + k]))
            return 0
        return f
    z.native_stub(GET_T, floats3(0))
    z.native_stub(GET_R, floats3(3))
    z.native_stub(GET_S, floats3(6))
    mat = z.alloc(0x30)

    def matrix(c):
        uc.mem_write(mat, struct.pack('<12I', *nodes[c.arg(0)]['m']))
        return mat
    z.native_stub(MATRIX, matrix)
    z.native_stub(WRITE, lambda c: got.append((z.cstr(c.arg(1)), bytes(uc.mem_read(c.arg(2), c.arg(3))))) or 0)

    record = z.alloc(0x50)
    w(RECORDS, record), w(COUNT, 1)
    defn = z.alloc(0x100)
    z.native_stub(FIND_DEF, lambda c: defn)
    z.native_stub(NODE_AT, lambda c: 0)
    objects = z.alloc(0x5c * 32)
    w(defn + 0xe4, objects)
    bad = cases = 0
    for case in range(300):
        nodes.clear()
        got.clear()
        n = rng.randint(1, 20)
        w(record, rng.choice((1, 2, 3, 4)))
        uc.mem_write(defn + 0xa0, bytes([rng.randint(0, 6)]))
        uc.mem_write(defn + 0xd9, bytes([n]))
        states = []
        for k in range(n):
            if rng.random() < 0.1:
                w(objects + 0x5c * k + 0x24, 0)
                states.append(None)
                continue
            node = z.alloc(0x40)
            o3d = z.alloc(4)
            s = dict(index=rng.randint(0, 5000), active=rng.randint(0, 1), matrix=rng.randint(0, 1),
                     m=[struct.unpack('<I', struct.pack('<f', rng.uniform(-3000, 3000)))[0] for _ in range(12)],
                     model=rng.random() < 0.7, mflags=rng.getrandbits(32), m24=rng.getrandbits(32),
                     cls=5 if rng.random() < 0.95 else 4)
            nodes[node] = s
            w(node + 0x24, rng.getrandbits(32) & ~4 | (s['active'] << 2))
            w(node + 0x34, s['cls'])
            w(node + 0x38, o3d)
            w(o3d, rng.getrandbits(32) & ~0x10 | (s['matrix'] << 4))
            if s['model']:
                model = z.alloc(0x28)
                w(model + 8, s['mflags']), w(model + 0x24, s['m24'])
                w(node + 0x3c, model)
            else:
                w(node + 0x3c, 0)
            w(objects + 0x5c * k + 0x24, node)
            states.append(s)
        z.call(SAVER, args=(0,))
        if len(got) != 1:
            print(f'case {case}: {len(got)} entries written')
            bad += 1
            continue
        name, data = got[0]
        if name != b'Activation0000' or len(data) != 0x58 + 0x3c * n or data[0x55] != n:
            print(f'case {case}: entry {name} {len(data)} bytes, count {data[0x55]} (want {0x58 + 0x3c * n}, {n})')
            bad += 1
            continue
        for k, s in enumerate(states):
            cases += 1
            exe = data[0x58 + 0x3c * k:0x58 + 0x3c * (k + 1)]
            if s is None or s['cls'] != 5:
                if struct.unpack_from('<i', exe, 0)[0] != -1:
                    print(f'case {case} object {k}: no node, exe index {exe[:4].hex()}')
                    bad += 1
                continue
            ins = [s['index'], s['active'], s['matrix']] + s['m'] + [int(s['model']), s['mflags'], s['m24']]
            out = ctypes.create_string_buffer(0x3c)
            lib.probe_pm_anim_state_pack((ctypes.c_uint32 * 18)(*ins), 1, out)
            port = bytearray(out.raw)
            if brk == 1:
                struct.pack_into('<I', port, 4, struct.unpack_from('<I', port, 4)[0] & ~2 | (s['matrix'] << 8))
            fields = [(0, 4)] + [(8, 0x30 if s['matrix'] else 0x24)] + ([(0x38, 4)] if s['model'] else [])
            mask = 7
            if (struct.unpack_from('<I', exe, 4)[0] ^ struct.unpack_from('<I', port, 4)[0]) & mask or \
                    any(exe[a:a + n_] != port[a:a + n_] for a, n_ in fields):
                print(f'case {case} object {k}: {s}\n  exe  {exe.hex()}\n  port {bytes(port).hex()}')
                bad += 1
                if bad > 5:
                    break
        if bad > 5:
            break
    print(f'anim save: 300 entries, {cases} object states; {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()

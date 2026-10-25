"""The AI's reset list 0x4d8610 against the original x86 (docs/ai.md section 19).

    cmake --build build && .venv/bin/python tests/differential/test_ai_lists.py [cases]

The list object (its sentinel node +0x08, count +0x0c) and 0..8 entries in
the emulator's memory, each entry an object whose vt+0x04 follows a plan
both sides share: nothing, an entry added at the back (a higher id: new,
waiting or already told), or another entry taken off (there or not). operator delete
counted. Compared: the order the entries are told, the count left, every
node freed, the list empty after.
"""
import ctypes
import os
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import Emu, Trace, compare, load_probe  # noqa: E402

I32 = ctypes.c_int32
RESET, DELETE = 0x4d8610, 0x5abd1a
COUNT = {'told': 0, 'added': 0, 'taken': 0}


def die():
    sys.stdout.flush()
    os._exit(1)


def make_plan(r):
    n = r.randrange(0, 9)
    first = list(range(n))
    ids = 24
    kind, arg = [0] * ids, [0] * ids
    for i in range(ids):
        k = r.random()
        if k < 0.25 and i < 16:
            kind[i], arg[i] = 1, r.randrange(i + 1, ids)
        elif k < 0.45:
            kind[i], arg[i] = 2, r.randrange(0, ids)
    # An add names a higher id (one there already, told before or new), so
    # every chain of adds ends.
    return first, kind, arg


def run_original(first, kind, arg):
    t = Trace()
    e = Emu(t)
    lst = e.world_alloc(0x10)
    sent = e.world_alloc(0xc)
    e.w32(sent, sent), e.w32(sent + 4, sent)
    e.w32(lst + 8, sent), e.w32(lst + 0xc, 0)
    frees = [0]
    ids = {}
    entries = {}

    def push(obj):
        n = e.world_alloc(0xc)
        prev = e.r32(sent + 4)
        e.w32(n, sent), e.w32(n + 4, prev), e.w32(n + 8, obj)
        e.w32(prev, n), e.w32(sent + 4, n)
        e.w32(lst + 0xc, e.r32(lst + 0xc) + 1)

    def told(em, key, a, pop):
        this = em.args('c')[0]
        i = ids[this]
        t.handle('entry.vt+0x4', [i])
        if kind[i] == 1:
            push(entry(arg[i]))
        elif kind[i] == 2:
            n = e.r32(sent)
            while n != sent:
                if e.r32(n + 8) in ids and ids[e.r32(n + 8)] == arg[i]:
                    nx, pv = e.r32(n), e.r32(n + 4)
                    e.w32(pv, nx), e.w32(nx + 4, pv)
                    e.w32(lst + 0xc, e.r32(lst + 0xc) - 1)
                    break
                n = e.r32(n)
        em._ret(0, 0)

    def entry(i):
        if i not in entries:
            o = e.object('entry', slots={4: ('c', 0, told)})
            entries[i] = o
            ids[o] = i
        return entries[i]
    for i in first:
        push(entry(i))
    e.stub(DELETE, 'delete', 'c', 0, lambda em, k, a, p: (frees.__setitem__(0, frees[0] + 1), em._ret(0, 0)))
    t.silent.add('delete')
    e.call(RESET, ecx=lst)
    left = e.r32(lst + 0xc)
    empty = e.r32(sent) == sent and e.r32(sent + 4) == sent
    return t.log, left, empty, frees[0]


def main():
    probe = load_probe()
    probe.probe_al_run.argtypes = [ctypes.POINTER(I32), I32, ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    for seed in range(count):
        r = random.Random(seed)
        first, kind, arg = make_plan(r)
        log_a, left_a, empty_a, frees = run_original(first, kind, arg)
        t = Trace()
        probe.probe_app_set_callback(t.callback())
        left_b = probe.probe_al_run((I32 * max(len(first), 1))(*first), len(first), (I32 * len(kind))(*kind),
                                    (I32 * len(arg))(*arg), len(kind))
        if not compare(f'seed {seed}', log_a, t.log):
            die()
        if left_a != left_b or not empty_a or left_a != 0 or frees != len(log_a):
            print(f'seed {seed}: count left {left_a} / {left_b}, empty {empty_a}, freed {frees} of {len(log_a)}')
            die()
        COUNT['told'] += len(log_a)
        COUNT['added'] += sum(1 for k, (i,) in log_a if kind[i] == 1)
        COUNT['taken'] += sum(1 for k, (i,) in log_a if kind[i] == 2)
    print(f'ok: {count} cases ({", ".join(f"{v} {k}" for k, v in COUNT.items())}), as the original')
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()

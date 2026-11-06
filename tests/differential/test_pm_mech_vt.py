"""Mech rows the address map doesn't pair, against PM's exe: the aim 0x41c900 (vt+0x164; MW3's
0x41bc20 cleared the aim on a point behind the torso's front, outside the cockpit view; PM always
gives the torso the direction), and the block 0x419c40 fills for predict when a mech is placed.
The torso's aim 0x467770, the weapons' 0x4b4a20 and the placement's other calls stand in, on both
sides. The placement's wiring in reset_at is read only.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_mech_vt.py
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402

probe = test_pm_zar.load_pm_probe()
Fl = ctypes.POINTER(ctypes.c_float)
probe.probe_pm_aim.argtypes = [Fl, ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, Fl]
probe.probe_pm_pending.argtypes = [Fl, Fl, Fl]
AIM, TORSO_AIM, SET_AIM, PLACE = 0x41c900, 0x467770, 0x4b4a20, 0x419c40


def floats(e, a, n):
    return list(struct.unpack(f'<{n}f', bytes(e.cpu._uc.mem_read(a, 4 * n))))


def main():
    e = test_pm_zar.Emulated()
    w = lambda a, v: e.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))  # noqa: E731
    wf = lambda a, *v: e.cpu._uc.mem_write(a, struct.pack(f'<{len(v)}f', *v))  # noqa: E731
    state = {}

    def torso(c):
        state['asked'] = 1
        d = c.arg(0)
        state['had'] = int(d != 0)
        state['dir'] = floats(e, d, 3) if d else [0.0, 0.0, 0.0]
        return state['answer']

    def set_aim(c):
        a = c.arg(0)
        state['aim'] = floats(e, a, 3) if a else None
        return 0
    e.native_stub(TORSO_AIM, torso, 12)
    e.native_stub(SET_AIM, set_aim, 4)
    for a, pop in ((0x4234d0, 4), (0x477a70, 0), (0x5cb240, 0)):
        e.native_stub(a, lambda c: 0, pop)
    mech, tor, node, data, at = e.alloc(0x800), e.alloc(0x100), e.alloc(0xd4), e.alloc(0x90), e.alloc(16)
    w(mech + 0x3ac, tor), w(tor + 8, node), w(node + 0x34, 5), w(node + 0x24, 0x80000), w(node + 0x38, data)
    wm = e.alloc(0x98)
    w(mech + 0xd4, wm)
    rng = random.Random(0x41c900)
    out = (ctypes.c_float * 10)()
    bad = n = behind = 0
    for _ in range(3000):
        pos = [rng.uniform(-500, 500) for _ in range(3)]
        p = [rng.uniform(-500, 500) for _ in range(3)]
        has, check, answer = rng.random() > 0.1, rng.randrange(2), rng.choice((0, 1, 1, 2))
        wf(data + 0x84, *pos), wf(at, *p)
        state.update(asked=0, had=0, dir=[0.0, 0.0, 0.0], aim=[-1.0, -1.0, -1.0], answer=answer)  # a stale aim
        got = e.call(AIM, ecx=mech, args=[at if has else 0, 0, check]) & 0xff
        theirs = [got, state['asked'], state['had'], *state['dir'], int(state['aim'] is not None), *(state['aim'] or [0.0, 0.0, 0.0])]
        probe.probe_pm_aim((ctypes.c_float * 6)(*p, *pos), int(has), check, answer, out)
        ours = list(out)
        n += 1
        behind += has and check
        if any(abs(a - b) > 1e-4 for a, b in zip(theirs, ours)):
            bad += 1
            if bad <= 5:
                print('aim', has, check, answer, p, pos, '\n  exe', theirs, '\n  port', ours)
    # The placement's block.
    vt, ctl, cvt, tr = e.alloc(0x200), e.alloc(0x10), e.alloc(0x40), e.alloc(48)
    plain, one = e.cpu.code(b'\xc3'), e.cpu.code(b'\xc2\x04\x00')
    e.native_stub(plain, lambda c: 0)
    e.native_stub(one, lambda c: 0, 4)
    w(mech, vt), w(vt + 0x18, one), w(vt + 0x14c, plain), w(mech + 0x38c, ctl), w(ctl, cvt), w(cvt + 0x18, plain), w(mech + 0xc, tr)
    pout = (ctypes.c_float * 19)()
    arg = e.alloc(16)
    for _ in range(200):
        q = [rng.uniform(-1, 1) for _ in range(4)]
        m = [rng.uniform(-9, 9) for _ in range(12)]
        wf(mech + 0x48, *q), wf(tr, *m), wf(mech + 0x30, 1.0, 2.0, 3.0)
        e.call(PLACE, ecx=mech, args=[arg])
        theirs = floats(e, mech + 0x6dc, 19)
        probe.probe_pm_pending((ctypes.c_float * 4)(*q), (ctypes.c_float * 12)(*m), pout)
        n += 1
        if any(abs(a - b) > 1e-6 for a, b in zip(theirs, list(pout))):
            bad += 1
            if bad <= 5:
                print('place\n  exe', theirs, '\n  port', list(pout))
    pm_floors.floors(cases=(n, 3000), checked=(behind, 1000))
    print(f'{n} cases: {bad} differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as ex:
        if isinstance(ex.code, str):
            print(ex.code)
        code = 0 if ex.code in (None, 0) else 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    import os
    os._exit(code)

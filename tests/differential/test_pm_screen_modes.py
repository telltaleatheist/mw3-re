"""Pirate's Moon's screen modes against the port's mech3::Engine with PM's table (tests/probe_pm_screen_modes.cpp):
the range 0x4af210 (software 3..5; a card from 5 to the first of 10, 9, 7, 6, 5 on its mode list, 0x598c90,
else 5) and Shift+V 0x4af360 (the step 0x4af2d0: 3, 5, 6 .. 10, then the range's first, a mode off the list
kept, past the top the first; a mode the switch 0x4aefe0 refuses is skipped until the round comes back; only a
mode taken is written to the in-game mode [0x6187d0]). The device (0x40a950), the current mode (0x40b8e0), the
list and the switch are stubbed from the case and logged on both sides. zfix's any_resolution is off in the
probe library (the original's path).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_screen_modes.py

Compared: the range, or the mode after and the in-game mode, then every list ask and switch in order.
Then the rounds left out, with the port's FIX (bug) on (zfix fixed, fixes.md): the exe still switching after
1000 tries (stopped there), the port ending after one round with the mode and the in-game mode untouched,
each mode of the exe's first round tried once, in its order.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
from original import ROOT  # noqa: E402

RANGE, TOGGLE, DEVICE, CURRENT, LISTED, SWITCH, IN_GAME = 0x4af210, 0x4af360, 0x40a950, 0x40b8e0, 0x598c90, \
    0x4aefe0, 0x6187d0

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_screen_modes_fixed.argtypes = [ctypes.c_int]
probe.probe_pm_screen_modes.argtypes = [ctypes.c_int, ctypes.c_int32, ctypes.c_int32, ctypes.c_uint32,
                                        ctypes.c_uint32, ctypes.c_void_p]
OUT = (ctypes.c_uint32 * 256)()


def cases():
    full = sum(1 << m for m in range(5, 11))
    for dev in (-1, 0, 1):
        for listed in (0, full, full & ~(1 << 10), (1 << 7) | (1 << 5), 1 << 8, (1 << 6), (1 << 9) | (1 << 8)):
            for cur in (3, 4, 5, 6, 7, 8, 9, 10, 11):
                for refused in (0, 1 << 6, (1 << 6) | (1 << 7), full, 1 << 5):
                    yield dev, cur, listed, refused
    rng = random.Random(3)
    for _ in range(500):
        yield rng.choice((-1, 0, 2)), rng.randint(2, 11), rng.getrandbits(12) & ~0x1f, rng.getrandbits(12)


def endless(dev, cur, listed, refused):
    # The round never comes back to a current mode outside it (3 on a card, or past the top) when the switch
    # refuses every mode in it: the exe spins there for good, so the case is left out (pm-tests.md).
    if dev < 0:
        lo, hi = 3, 5
    else:
        lo, hi = 5, next((m for m in (10, 9, 7, 6, 5) if (listed >> m) & 1), 5)

    def step(m):
        n = 5 if m == 3 else m + 1 if 5 <= m <= 9 else lo if m == 10 else m
        return lo if hi < n else n
    m, seen = step(cur), set()
    while m != cur:
        if not (refused >> m) & 1:
            return False
        if m in seen:
            return True
        seen.add(m)
        m = step(m)
    return False


def main():
    z = test_pm_zar.Emulated()
    u = z.cpu._uc
    st = {}
    cells = z.alloc(8)
    u.mem_write(IN_GAME, struct.pack('<I', cells))

    def listed(c):
        m = c.arg(1)
        st['log'] += [1, m]
        return (st['listed'] >> m) & 1

    def switch(c):
        m = c.arg(0)
        st['log'] += [2, m]
        st['switches'] = st.get('switches', 0) + 1
        if st.get('cap') and st['switches'] >= st['cap']:
            u.emu_stop()
        if not (st['refused'] >> m) & 1:
            st['cur'] = m
        return st['cur']

    z.native_stub(DEVICE, lambda c: st['dev'] & 0xffffffff, 0)
    z.native_stub(CURRENT, lambda c: st['cur'], 0)
    z.native_stub(LISTED, listed, 0)
    z.native_stub(SWITCH, switch, 0)
    lohi = z.alloc(8)
    bad = runs = skipped = 0
    seen = set()
    for dev, cur, lst, refused in cases():
        for which in (0, 1):
            key = (which, dev, cur if which else 0, lst, refused if which else 0)
            if key in seen or (which and endless(dev, cur, lst, refused)):
                skipped += which and key not in seen
                seen.add(key)
                continue
            seen.add(key)
            st.update(dev=dev, cur=cur, listed=lst, refused=refused, log=[])
            if which == 0:
                u.mem_write(lohi, struct.pack('<2i', -1, -1))
                z.call(RANGE, args=(dev & 0xffffffff, lohi, lohi + 4))
                e = list(struct.unpack('<2I', bytes(u.mem_read(lohi, 8)))) + st['log']
            else:
                u.mem_write(cells, struct.pack('<i', -1))
                z.call(TOGGLE, args=())
                e = [st['cur'], struct.unpack('<I', bytes(u.mem_read(cells, 4)))[0]] + st['log']
            k = probe.probe_pm_screen_modes(which, dev, cur, lst, refused, OUT)
            p = list(OUT[:k])
            runs += 1
            if e != p:
                bad += 1
                if bad <= 5:
                    print(f'FAIL {"toggle" if which else "range"} dev {dev} cur {cur} listed {lst:#x} '
                          f'refused {refused:#x}\n  exe  {e}\n  port {p}')
    print(f'{runs} cases ({skipped} endless left out), {bad} differ')
    # The endless rounds: the exe never gives up; the fixed port stops after one round.
    probe.probe_pm_screen_modes_fixed(1)
    hangs = ends = 0
    for dev, cur, lst, refused in sorted({(d, c, l, r) for d, c, l, r in cases() if endless(d, c, l, r)}):
        st.update(dev=dev, cur=cur, listed=lst, refused=refused, log=[], switches=0, cap=1000)
        u.mem_write(cells, struct.pack('<i', -1))
        try:
            z.call(TOGGLE, args=())
        except Exception:  # noqa: BLE001 - stopped inside the stub
            pass
        exe_hung = st['switches'] >= 1000
        exe_tries = [st['log'][i + 1] for i in range(0, len(st['log']), 2) if st['log'][i] == 2]
        first_round = exe_tries[:next((i for i, m in enumerate(exe_tries) if m in exe_tries[:i]), len(exe_tries))]
        st['cap'] = 0
        k = probe.probe_pm_screen_modes(1, dev, cur, lst, refused, OUT)
        p = list(OUT[:k])
        tries = [p[i + 1] for i in range(2, len(p), 2) if p[i] == 2]
        ended = p[0] == cur and p[1] == 0xffffffff and tries == first_round
        hangs += exe_hung
        ends += bool(ended)
        if not exe_hung or not ended:
            bad += 1
            if bad <= 5:
                print(f'FAIL endless dev {dev} cur {cur} listed {lst:#x} refused {refused:#x}: exe '
                      f'{st["switches"]} switches, port {p}')
    probe.probe_pm_screen_modes_fixed(0)
    print(f'endless rounds: the exe still switching after 1000 in {hangs}, the port ending in {ends}; {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()

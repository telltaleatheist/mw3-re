"""The Load State's resume back from the Mech Lab on Pirate's Moon (0x407fb0, MW3's 0x4092e0),
against PM's exe: for Instant Action (the game's kind 2) the line-up's mechs go back into the
Action screen's setup (the game's +0x40..): the player's at +0x40, the slots on the player's
side after it from +0x48 (+0x44 is the Mobile Field Base's), the rest of the five -1. The game
record is PM's own code (its vtable 0x5df058: the slot 0x40c990, the side count 0x40ca80);
the briefing's refresh and show and the state's resume are stand-ins. Line-ups of 1..11 slots,
allies anywhere (PM keeps them first; the count stops at the first enemy), mechs -1..24, the
eleven entries from +0x40 random before. Compared: the eleven after, and the other kinds left
alone.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_load_resume.py [runs]
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402

probe = test_pm_zar.load_pm_probe()
I32 = ctypes.POINTER(ctypes.c_int32)
probe.probe_pm_load_resume.argtypes = [ctypes.c_int32, I32, ctypes.c_char_p, I32, I32]
RESUME, PILOT, SCREEN, VTABLE = 0x407fb0, 0x62c40c, 0x618744, 0x5df058
COUNT, SLOTS, SIDES, SETUP = 0xd8, 0xc4, 0x1cb, 0x40


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 500
    rnd = random.Random(11)
    e = test_pm_zar.Emulated()
    u = e.cpu._uc
    w = lambda a, v: u.mem_write(a, struct.pack('<i', v))  # noqa: E731
    e.native_stub(0x407960, lambda c: 0)        # the briefing's loadout refreshed
    e.native_stub(0x404650, lambda c: 0, 4)     # and shown
    e.native_stub(0x48d0b0, lambda c: 0, 4)     # ShellState::resume
    state, pilot, game = e.alloc(0x40), e.alloc(0x100), e.alloc(0x300)
    kind_vt = e.alloc(0x100)                    # another kind of game: vt+0x28 says which
    kind = [2]
    kind_code = e.cpu.code(b'\xc3')
    e.native_stub(kind_code, lambda c: kind[0])
    w(kind_vt + 0x28, kind_code)
    w(PILOT, pilot), w(SCREEN, e.alloc(0x10))
    bad = 0
    for r in range(runs):
        n = rnd.randint(1, 11)
        mechs = [rnd.randint(-1, 24) for _ in range(n)]
        sides = [1] + [rnd.choice((1, 1, 0)) for _ in range(n - 1)]
        if rnd.random() < 0.5:  # allies first, as the screen makes them
            sides = [1] + sorted(sides[1:], reverse=True)
        before = [rnd.randint(-1, 30) for _ in range(11)]
        kind[0] = 2 if r % 8 else rnd.choice((1, 3, 4))
        if kind[0] == 2:
            w(game, VTABLE)
        else:
            w(game, kind_vt)
        w(pilot + 0x6c, game)
        w(game + COUNT, n - 1)
        w(game + 0x20 + 4, mechs[0])
        for i in range(1, 11):
            w(game + SLOTS + 0x18 * i + 4, mechs[i] if i < n else 77)
            u.mem_write(game + SIDES + i, bytes([sides[i] if i < n else 0]))
        for i in range(11):
            w(game + SETUP + 4 * i, before[i])
        e.call(RESUME, ecx=state, args=[4])
        theirs = [struct.unpack('<i', bytes(u.mem_read(game + SETUP + 4 * i, 4)))[0] for i in range(11)]
        out = (ctypes.c_int32 * 11)()
        if kind[0] == 2:
            probe.probe_pm_load_resume(n, (ctypes.c_int32 * 11)(*mechs), bytes(sides), (ctypes.c_int32 * 11)(*before),
                                       out)
            ours = list(out)
        else:
            ours = before  # the port's resume asks the pilot's Instant Action game only
        if theirs != ours:
            bad += 1
            if bad <= 5:
                print(f'run {r}: kind {kind[0]} mechs {mechs} sides {sides}\n  before {before}\n'
                      f'  exe  {theirs}\n  port {ours}')
    print(f'{runs} runs: {bad} differ')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as ex:
        code = 0 if ex.code in (None, 0) else 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    import os
    os._exit(code)

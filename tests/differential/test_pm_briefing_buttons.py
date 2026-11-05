"""The briefing's buttons by game, against PM's exe (0x407cc0, the address map has no pair for
it): which it enables and hides, and whether it launches by itself, for game types 0..5, loaded
or not. MW3's 0x408ef0 locks MECHLAB and SALVAGE at Op 1 Mission 1; PM's has no such test. The
buttons are stand-ins; the briefing around them isn't run.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_briefing_buttons.py
"""
import ctypes
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402

probe = test_pm_zar.load_pm_probe()
probe.probe_pm_briefing_buttons.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(ctypes.c_int32)]
RULE, LANCE, LAUNCH, MISSION = 0x407cc0, 0x407c20, 0x404a20, 0x62c40c
ABORT, GO, LAB, SALVAGE, REPLAY, LOADING = 0x11a24, 0x11ba0, 0x11d1c, 0x12014, 0x11e98, 0x12190


def main():
    e = test_pm_zar.Emulated()
    w = lambda a, v: e.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))  # noqa: E731
    hidden, launched = [], []
    e.native_stub(LANCE, lambda c: 0)
    e.native_stub(LAUNCH, lambda c: launched.append(1) or 0)
    kind = [0]
    mission, game, gvt = e.alloc(0x80), e.alloc(0x40), e.alloc(0x60)
    w(gvt + 0x28, e.cpu.code(b'\xc3'))
    e.native_stub(e.r32(gvt + 0x28), lambda c: kind[0])
    w(game, gvt), w(mission + 0x6c, game), w(MISSION, mission)
    hide = e.cpu.code(b'\xc3')
    e.native_stub(hide, lambda c: hidden.append(c.ecx) or 0)
    other = e.cpu.code(b'\xc2\x04\x00')
    e.native_stub(other, lambda c: 0, 4)
    plain = e.cpu.code(b'\xc3')
    e.native_stub(plain, lambda c: 0)
    vt = e.alloc(0x100)
    w(vt + 0x64, hide), w(vt + 0x60, other), w(vt + 0x80, plain)
    bad = n = 0
    out = (ctypes.c_int32 * 10)()
    for t in range(6):
        for on in (1, 0):
            s = e.alloc(0x13000)
            for off in (ABORT, GO, LAB, SALVAGE, REPLAY, LOADING, 0x58):
                w(s + off, vt)
            e.cpu._uc.mem_write(s + 0x113b4, bytes([0 if on else 1]))
            kind[0] = t
            hidden.clear(), launched.clear()
            e.call(RULE, ecx=s, args=[on])
            is_hidden = lambda off: int(s + off in hidden)  # noqa: E731
            theirs = [e.r32(s + ABORT + 0xc4), e.r32(s + GO + 0xc4), e.r32(s + LAB + 0xc4), e.r32(s + SALVAGE + 0xc4),
                      e.r32(s + REPLAY + 0xc4), is_hidden(ABORT), is_hidden(LAB), is_hidden(SALVAGE), is_hidden(REPLAY),
                      int(bool(launched))]
            probe.probe_pm_briefing_buttons(t, on, out)
            ours = list(out)
            n += 1
            if theirs != ours:
                bad += 1
                print(f'type {t} loaded {on}: exe {theirs}\n  port {ours}')
    print(f'{n} cases: {bad} differ')
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

"""PM's frame window on a mech: vt+0x34 0x41b900 and vt+0x3c 0x41b930 against mech3/app_weapons (docs/pm-map.md, The
frame's box window). Pilot's vehicle or not, with a headlight or not: the calls, the frame time +0x6d4, the clear of
+0x688. The box's save and restore 0x4b2d70, 0x4b2da0 are the caller's in the port: here the last call of each slot.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_window.py
"""
import ctypes
import os
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '53')
sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402

probe = test_pm_zar.load_pm_probe()
P = ctypes.c_void_p
probe.probe_pm_window.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_float, P, P, P]
BEFORE, AFTER = 0x41b900, 0x41b930
GAME = 0x62c40c
FRAME_DT, ATTACKER = 0x6d4, 0x688
SAVE, RESTORE = 0x4b2d70, 0x4b2da0


class Emulated(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        a = self.alloc
        self.mech, self.game, self.ammo, self.head = a(0x800), a(0x40), a(0x40), a(0x40)
        self.has_head, self.log = 0, []
        self.poke(GAME, self.game)
        self.poke(self.mech + 0xd4, self.ammo)
        self.native_stub(0x41f930, lambda c: self.head if self.has_head else 0, 0)
        self.native_stub(0x41e4d0, lambda c: self.note(1, c.ecx == self.mech), 0)
        self.native_stub(0x4b4c70, lambda c: self.note(2, c.ecx == self.ammo), 4)
        self.native_stub(0x479c30, lambda c: self.note(3, c.ecx == self.head), 4)
        self.native_stub(0x479c60, lambda c: self.note(4, c.ecx == self.head), 4)
        self.native_stub(SAVE, lambda c: self.note(5, c.ecx == self.mech), 4)
        self.native_stub(RESTORE, lambda c: self.note(6, c.ecx == self.mech), 4)

    def poke(self, a, v):  # the exe's own address, past the address map
        self.cpu._uc.mem_write(a, struct.pack('<I', v))

    def note(self, code, right):
        assert right, code
        self.log.append(code)
        return 0

    def run(self, slot, pilot, head, dt):
        self.log, self.has_head = [], head
        self.poke(self.game + 0x14, self.mech if pilot else self.mech + 4)
        self.poke(self.mech + FRAME_DT, struct.unpack('<I', struct.pack('<f', -1.0))[0])
        self.poke(self.mech + ATTACKER, 0x1234)
        self.call(AFTER if slot else BEFORE, ecx=self.mech, args=(struct.unpack('<I', struct.pack('<f', dt))[0],))
        return (self.log, struct.unpack('<f', struct.pack('<I', self.r32(self.mech + FRAME_DT)))[0], self.r32(self.mech + ATTACKER) != 0)


def port(slot, pilot, head, dt):
    log = (ctypes.c_uint32 * 8)()
    frame, kept = ctypes.c_float(), ctypes.c_int()
    n = probe.probe_pm_window(slot, pilot, head, dt, log, ctypes.byref(frame), ctypes.byref(kept))
    return list(log[:n]) + [6 if slot else 5], frame.value, kept.value != 0


emu = Emulated()
cases = bad = 0
for slot in (0, 1):
    for pilot in (0, 1):
        for head in (0, 1):
            want, got = emu.run(slot, pilot, head, 0.25), port(slot, pilot, head, 0.25)
            cases += 1
            if want != got:
                bad += 1
                print(f'slot {slot} pilot {pilot} head {head}\n  exe  {want}\n  port {got}')
print(f'{cases} cases: {bad} differ')
sys.exit(1 if bad else 0)

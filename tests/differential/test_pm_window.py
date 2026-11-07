"""PM's frame window on a mech: vt+0x34 0x41b900 and vt+0x3c 0x41b930 against mech3/app_weapons (docs/pm-map.md, The
frame's box window). Pilot's vehicle or not, with a headlight or not: the calls, the frame time +0x6d4, the clear of
+0x688. The box's save and restore 0x4b2d70, 0x4b2da0 are the caller's in the port: here the last call of each slot.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_window.py
"""
import ctypes
import os
import random
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


def cloud(emu):
    """The death band: 0x4b2b40 on a mech and 0x4b2c00 on another class, at the band's edges."""
    poke = emu.poke
    obj, node, vt = emu.alloc(0x100), emu.alloc(0x60), emu.alloc(0x200)
    m = emu.alloc(0x800)
    poke(m, vt)
    poke(m + 0x14, node)
    poke(node + 0x38, obj)
    log = []
    kill = emu.cpu.code(b'\xc3')
    poke(vt + 0x78, kill)
    emu.native_stub(kill, lambda c: log.extend((3,)) or 0, 0)
    emu.native_stub(0x408a70, lambda c: log.extend((1, c.arg(0))) or 0, 4)
    emu.native_stub(0x41de60, lambda c: log.extend((2,)) or 0, 0)
    view = emu.alloc(0x80)
    poke(0x61874c, view)
    bad = cases = kills = 0
    probe.probe_pm_cloud.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_int, ctypes.c_int, ctypes.c_int, P, P]
    probe.probe_pm_cloud_band.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_float]
    mem = emu.cpu._uc.mem_write
    for on in (0, 1):
        for death in (0, 1):
            for y in (-10001.0, -10000.0, -9999.0, -30.5, -30.0, -29.5, 5.0):
                for active in (0, 1):
                    for state in (1, 10):
                        for viewed in (0, 1):
                            mem(0x62da58, bytes([on]))
                            mem(0x62da7c, bytes([death]))
                            mem(0x62da80, struct.pack('<f', -10000.0))
                            mem(0x62da84, struct.pack('<f', -30.0))
                            mem(obj + 0x84 + 4, struct.pack('<f', y))
                            mem(m + 8, bytes([active]))
                            poke(m + 0x59c, state)
                            poke(m + 0x658, 0)
                            poke(view + 0x64, m if viewed else m + 4)
                            log.clear()
                            emu.call(0x4b2b40, ecx=m)
                            want = (list(log), emu.r32(m + 0x658) & 1)
                            out, flag = (ctypes.c_uint32 * 16)(), ctypes.c_int()
                            n = probe.probe_pm_cloud(on, death, -10000.0, -30.0, y, active, state, viewed, out, ctypes.byref(flag))
                            got = list(out[:n])
                            cases += 1
                            kills += 3 in log
                            if want != (got, flag.value):
                                bad += 1
                                print(f'mech on {on} death {death} y {y} active {active} state {state} viewed {viewed}: exe {want} port {got, flag.value}')
                            # the other classes: active, no fall test, no view
                            log.clear()
                            emu.call(0x4b2c00, ecx=m)
                            if bool(log) != (bool(active) and bool(probe.probe_pm_cloud_band(on, death, -10000.0, -30.0, y))):
                                bad += 1
                                print(f'vehicle on {on} death {death} y {y} active {active}: exe {log}')
                            cases += 1
    print(f'{cases} band cases, {kills} kills: {bad} differ')
    return bad


def cloud_tint(emu):
    """The screen tint 0x4096f0: the camera's height to an alpha, against cloud_fill."""
    view, node = emu.alloc(0x80), emu.alloc(0x60)
    emu.poke(view + 4, node)
    mem = emu.cpu._uc.mem_write
    height, seen = [0.0], []

    def where(c):
        mem(c.arg(1), struct.pack('<3f', 0.0, height[0], 0.0))
        return 0

    def tint(c):
        seen.append((c.arg(0) & 0xffff, struct.unpack('<d', struct.pack('<II', c.arg(1) & 0xffffffff, c.arg(2) & 0xffffffff))[0]))
        return 0
    emu.native_stub(0x4ff250, where, 0)
    emu.native_stub(0x5c6c70, tint, 0)
    fill = probe.probe_pm_cloud_fill
    fill.argtypes = [ctypes.c_float] * 5 + [ctypes.POINTER(ctypes.c_double)]
    r = random.Random(5)
    bad = cases = inside = ramps = 0
    clouds = [(-5.0, -65.0, 10.0), (-5.0, -65.0, 0.0), (10.0, -40.0, 22.5), (0.0, -1.0, 0.45), (-5.0, -65.0, 27.0)]
    for top, bottom, depth in clouds:
        middle = (top - bottom) * 0.5 + bottom
        low, high = middle - depth, middle + depth
        mem(0x62da6c, struct.pack('<2f', top, bottom))
        mem(0x62da88, struct.pack('<2f', low, high))
        mem(0x62da68, struct.pack('<H', 0x1234))
        edges = [top, bottom, low, high]
        ys = edges + [e + d for e in edges for d in (-0.01, 0.01, 1e-5, -1e-5)]
        ys += [r.uniform(bottom - 5, top + 5) for _ in range(300)]
        for y in ys:
            seen.clear()
            height[0] = y
            ret = emu.call(0x4096f0, ecx=view) & 0xff
            a = ctypes.c_double(-1.0)
            got = fill(top, bottom, low, high, y, ctypes.byref(a))
            cases += 1
            inside += bool(ret)
            ramps += 1 if seen and 0 < seen[0][1] < 1 else 0
            if bool(ret) != bool(got) or (ret and (len(seen) != 1 or seen[0] != (0x1234, a.value))):
                bad += 1
                print(f'cloud {top} {bottom} {low} {high} y {y}: exe {ret} {seen}, port {got} {a.value}')
    print(f'{cases} tint cases, {inside} in the cloud, {ramps} on a ramp: {bad} differ')
    return bad


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
bad += cloud(emu)
bad += cloud_tint(emu)
sys.exit(1 if bad else 0)

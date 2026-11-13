"""PM's lance orders against the exe: the order wrappers the dispatcher 0x46b4d0 picks (0x4fb420..0x4fb7f0), each with
the brain's slot it calls (PM's brain vtable 0x5eef04, its real slots 0x4fba80, 0x4fbaf0, 0x4fbb70, 0x4fbb00,
0x4fba20, 0x4fbbc0, 0x4fb9b0, 0x5afca0) and the radio 0x4fb3f0, against the port's lance_order_pm
(mech3/zai_brain_pm, tests/probe_pm_lance_orders.cpp). Everything they call stands in, answering from one script of
bits: the contact's vehicle 0x4f8d70, the order's leave test 0x4fa320, the vehicle's net (vt+0xe4), the follow
behaviour 0x4c7920, the order given 0x4fa150, the first order 0x4fa1d0 and the pop to it 0x4fa1b0, the net given
0x4fa130, the refit slot 0x4fb850 (ported and tested apart), the team (vt+0x28's +0xc0) and the radio's sound
0x4f8ae0. The answer, the kept target (the brain's +0x124's +0x8c), the bits used and the calls in order must match.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_lance_orders.py [cases]
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

WRAPPER = {0: 0x4fb420, 1: 0x4fb4a0, 3: 0x4fb4f0, 4: 0x4fb570, 5: 0x4fb5d0, 6: 0x4fb630, 7: 0x4fb690, 8: 0x4fb7f0,
           9: 0x4fb6e0, 10: 0x4fb730, 11: 0x4fb790, 12: 0x4fb690}
BRAIN_VT, FORMATION = 0x5eef04, 0x627c18
NET, BEH, FIRST, KEPT = 0x7a10, 0x7b20, 0x7c30, 0x4444
TEAM_ORDERS = (3, 4, 10, 11)  # their slots read the contact's vehicle unchecked: the exe crashes on none (the port's FIX)

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libpmprobe.*'))))
probe.probe_pm_lance_order.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_void_p]
OUT = (ctypes.c_uint32 * 4096)()


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        a = iter(range(z.alloc(0x8000), 1 << 31, 0x400))
        self.brain, self.ctl, self.own, self.veh, self.vvt, self.tgt, self.rec, self.radio = (next(a) for _ in range(8))
        self.contact = next(a)
        w = self.w
        w(self.brain, BRAIN_VT), w(self.brain + 0x100, self.ctl), w(self.ctl + 4, 0), w(self.brain + 0x124, self.rec)
        [w(self.radio + 4 * i, 0x1000 + i) for i in range(16)]
        w(self.veh, self.vvt), w(self.own, self.vvt)
        w(self.vvt + 0x28, self.ret(self.team)), w(self.vvt + 0xe4, self.ret(self.vnet))
        for addr, fn, pop in ((0x4f8d70, self.cv, 0), (0x4fa320, lambda u: self.ans(2), 0),
                              (0x4c7920, lambda u: (self.put(4, u.ecx), BEH)[1], 0),
                              (0x4fa150, lambda u: (self.put(5, u.arg(0)), self.put(5, u.arg(1)), 0)[2], 8),
                              (0x4fa1d0, lambda u: FIRST if self.ans(6) else 0, 0),
                              (0x4fa1b0, lambda u: self.put(7, u.arg(0)), 4),
                              (0x4fa130, lambda u: (self.put(8, u.arg(0)), 1)[1], 4),
                              (0x4fb850, lambda u: self.ans(11), 4),
                              (0x4f8ae0, lambda u: self.put(12, u.arg(0) - 0x1000), 0)):
            z.native_stub(addr, fn, pop)

    def w(self, addr, v):
        self.uc.mem_write(addr, struct.pack('<I', v & 0xffffffff))

    def ret(self, fn, pop=0):
        t = self.z.alloc(4)
        self.uc.mem_write(t, b'\xc3')
        self.z.native_stub(t, fn, pop)
        return t

    def put(self, k, v=0):
        self.log += [k, v & 0xffffffff]
        return 0

    def bit(self):
        b = self.bits[self.n]
        self.n += 1
        return b

    def ans(self, k):
        self.put(k)
        return self.bit()

    def cv(self, u):
        self.put(1)
        return self.cv_answer if self.bit() else 0

    def vnet(self, u):
        self.put(3, u.ecx)
        return NET if self.bit() else 0

    def team(self, u):
        self.put(10, u.ecx)
        return self.tgt

    def run(self, order, own, same, team, allies, enemies, radio, bits):
        w, b = self.w, self.brain
        self.log, self.bits, self.n = [], bits, 0
        own_addr = self.own if own else 0
        self.cv_answer = own_addr if same else self.veh
        w(b + 0x11c, own_addr), w(b + 0x2c8, self.radio if radio else 0), w(self.rec + 0x8c, KEPT)
        w(self.own + 0xc4, allies), w(self.own + 0xc8, enemies), w(self.tgt + 0xc0, team)
        r = self.z.call(WRAPPER[order], ecx=b, args=(self.contact,))
        return [r, self.z.r32(self.rec + 0x8c), self.n] + self.log


def port(exe, order, own, same, team, allies, enemies, radio, bits):
    own_addr = exe.own if own else 0
    inp = (ctypes.c_uint32 * 10)(own_addr, own_addr if same else exe.veh, NET, BEH, FIRST, FORMATION, team, allies,
                                 enemies, 1 if radio else 0)
    n = probe.probe_pm_lance_order(order, inp, bytes(bits), KEPT, OUT)
    return list(OUT[:n])


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 6000
    exe = Exe()
    rng = random.Random(3025)
    seen, bad = set(), 0
    for i in range(cases):
        order = rng.choice(sorted(WRAPPER))
        own, same = rng.random() < 0.9, rng.random() < 0.1
        masks = [rng.choice([0, 1, 2, 4, 6, 0x40000000, 0xfffffffe, 0xfffffff8]) for _ in range(3)]
        radio = rng.random() < 0.8
        bits = [1 if rng.random() < 0.7 else 0 for _ in range(32)]
        if order in TEAM_ORDERS:
            bits[0] = bits[1] = 1  # the wrapper's and the slot's contact vehicle: never none (see TEAM_ORDERS)
        args = (order, own, same, *masks, radio, bits)
        try:
            e = exe.run(*args)
        except Exception as x:
            print("EXE FAULT", args[:7], hex(exe.uc.reg_read(__import__("unicorn").x86_const.UC_X86_REG_EIP)), x)
            raise
        p = port(exe, *args)
        seen.add((order, tuple(e[3::2])))
        if e != p:
            bad += 1
            if bad <= 5:
                print('FAIL', args[:7], 'exe', e, 'port', p)
    print(f'{cases} orders, {len(seen)} distinct call paths, {bad} differ')
    sys.stdout.flush()
    os._exit(1 if bad else 0)


if __name__ == '__main__':
    main()

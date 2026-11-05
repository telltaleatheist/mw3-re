"""PM's jump jets 0x417f50 and AddJumpJet 0x417ea0 against mech3/mech_move (docs/movement-jump.md, docs/pm-mech-step.md), on MW3's
scenarios from test_jets.py with PM's field offsets. The jump pose is skipped by a jump and logged, as test_jets does. Move and
the landing come with their own pieces.

    cmake --build build --target pmprobe
    .venv/bin/python -P tests/differential/test_pm_jets.py [trials]
"""
import os
import random
import struct
import sys
from pathlib import Path

os.environ['MW3_PROBE_LIB'] = 'libpmprobe'  # test_jets' own, not built on this branch
sys.path.insert(0, str(Path(__file__).parent))
import test_jets as tj  # noqa: E402
import test_pm_zar  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP  # noqa: E402

JETS, ADD, MECH_VT, SIZE = 0x417f50, 0x417ea0, 0x5dfaac, 0x800
POSE_AT, POSE_TO = 0x4182ba, 0x4184d9
FIELDS = {0x3a8: 0x3b8, 0x3d8: 0x3e8, 0x58c: 0x5b0, 0x6c4: 0x734, 0x6c8: 0x738, 0x488: 0x498, 0x48c: 0x49c, 0x490: 0x4a0,
          0x518: 0x528, 0x53c: 0x550, 0x540: 0x554, 0x544: 0x558, 0x548: 0x55c, 0x560: 0x584, 0x564: 0x588, 0x568: 0x58c,
          0x578: 0x59c, 0x57c: 0x5a0, 0x580: 0x5a4, 0x584: 0x5a8, 0x594: 0x5b8, 0x598: 0x5bc, 0x59c: 0x5c0, 0x5c4: 0x5e8,
          0x620: 0x644, 0x688: 0x6ac}


def to_pm(mw3):
    out = bytearray(SIZE)
    for off, kind in tj.MECH:
        n = 1 if kind == 'b' else 4
        p = FIELDS.get(off, off)
        out[p:p + n] = mw3[off:off + n]
    return bytes(out)


def to_mw3(pm):
    out = bytearray(tj.MECH_SIZE)
    for off, kind in tj.MECH:
        n = 1 if kind == 'b' else 4
        p = FIELDS.get(off, off)
        out[off:off + n] = pm[p:p + n]
    return bytes(out)


class Emulated(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        a = self.alloc
        self.mech, self.mvt, self.ctl, self.vbt = a(SIZE), a(0x300), a(tj.CTL), a(8)
        self.pilot, self.config, self.skis, self.pos = a(0x300), a(0x40), a(0x300), a(12)
        self.player_obj, self.game, self.gvt, self.ff, self.opt = a(0x80), a(0x20), a(0x80), a(0x40), a(4)
        self.torso, self.hip, self.legs = a(0x1b4), a(0x100), [a(0xd4), a(0xd4)]
        self.posea, self.avt = a(0xdc), a(0x50)
        f = self.cpu.code(b'\xc2\x04\x00')
        self.w32(self.avt + 0x44, f)
        self.native_stub(f, lambda c: self.log(7, c.arg(0)), 4)
        self.w32(self.mvt, 0)
        self.cpu._uc.mem_write(self.mvt, bytes(self.cpu._uc.mem_read(MECH_VT, 0x300)))
        self.w32(self.gvt + 0x44, 0x435070)
        self.w32(self.game, self.gvt)
        self.ctl_vt = self.cpu.tr(tj.CTL_VT, 4)
        self.native_stub(0x58fe70, lambda c: self.log(1, c.arg(0)), 0)
        self.native_stub(0x51f830, lambda c: self.log(2, c.arg(0)), 0)
        self.native_stub(0x432d10, lambda c: self.handle(3, c.arg(0), c.arg(1), c.arg(4)), 0x14)
        self.native_stub(0x592a50, lambda c: self.log(4, c.arg(0)) and 0, 0)
        self.native_stub(0x432bb0, lambda c: self.handle(5, c.arg(0), c.arg(1)), 0x10)
        self.native_stub(0x434340, lambda c: self.log(6, c.arg(0) & 0xff), 4)
        self.native_stub(0x5cb260, lambda c: self.pos, 0)
        self.cpu._uc.hook_add(UC_HOOK_CODE, self.pose, begin=POSE_AT, end=POSE_AT)
        self.w32(0x62c40c, self.player_obj)
        self.w32(self.player_obj + 0x6c, self.game)
        self.w32(0x6187c4, self.opt)
        self.cpu._uc.mem_write(0x629314, struct.pack('<f', 38.0))

    def w32(self, a, v):
        self.cpu._uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def pose(self, uc, address, size, data):
        self.log(8)
        uc.reg_write(UC_X86_REG_EIP, POSE_TO)

    def log(self, *e):
        self.events.append(len(e))
        self.events.extend(v & 0xffffffff for v in e)
        return 0

    def handle(self, *e):
        self.log(*e)
        h = self.next
        self.next += 1
        return h

    def run(self, op, s):
        w, m = self.w32, self.mech
        self.events, self.next = [], 0x9000
        self.cpu._uc.mem_write(m, to_pm(s['mech']))
        w(m, self.mvt)
        w(self.mvt + 0x64, 0x402250 if s['flags'] & 8 else 0x406ce0)
        w(m + 0x38c, self.ctl), w(m + 0x6a8, self.pilot), w(self.pilot + 0xc, s['roster'])
        w(m + 0x3fc, self.config), w(self.config + 0x14, s['roster'])
        w(m + 0x42c, self.skis)
        self.cpu._uc.mem_write(self.skis + 0x13c, struct.pack('<f', s['ground6']))
        self.cpu._uc.mem_write(self.pos, struct.pack('<3f', 0.0, s['pos_y'], 0.0))
        w(m + 0x3ac, self.torso), w(m + 0x3b0, self.hip), w(m + 0x39c, self.legs[0]), w(m + 0x3a0, self.legs[1])
        w(m + 0x464, self.posea if s['flags'] & 16 else 0), w(self.posea, self.avt)
        self.cpu._uc.mem_write(self.ctl, s['ctl'])
        w(self.ctl + 4, self.vbt), w(self.vbt + 4, 0x5c), w(self.ctl + 0x5c, 0), w(self.ctl + 0x60, self.ctl_vt), w(self.ctl + 0x70, m)
        w(self.player_obj + 0x14, m if s['flags'] & 1 else 0)
        w(0x62c410, self.ff if s['flags'] & 2 else 0)
        w(self.opt, 0x80 if s['flags'] & 4 else 0)
        args = {0: [struct.unpack('<I', struct.pack('<f', s['dt']))[0]], 2: [s['arg']]}[op]
        self.call({0: JETS, 2: ADD}[op], ecx=m, args=args)
        r = self.cpu._uc.reg_read(UC_X86_REG_EAX) if op == 0 else 0
        return (tj.pick(to_mw3(bytes(self.cpu._uc.mem_read(m, SIZE))), tj.MECH),
                tj.pick(bytes(self.cpu._uc.mem_read(self.ctl, tj.CTL)), tj.CTL_FIELDS), r, self.events)


def main():
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
    emu = Emulated()
    rng = random.Random(0x417f50)
    seen = set()
    for n in range(trials):
        op = rng.choice([0, 0, 0, 2])
        s = tj.scenario(rng)
        s['roster'] = rng.randrange(25)
        want, got = emu.run(op, s), tj.port(op if op == 0 else 2, s)
        for k, name in enumerate(('mech', 'controls', 'result', 'calls')):
            if want[k] != got[k]:
                if k < 2:
                    diff = [hex(o) for o in range(0, len(want[k]), 4) if want[k][o:o + 4] != got[k][o:o + 4]]
                    print(f'trial {n} op {op}: {name} differ at {diff}')
                    for o in diff[:4]:
                        o = int(o, 16)
                        print(f'  {o:#x}: exe {want[k][o:o+4].hex()} port {got[k][o:o+4].hex()}')
                else:
                    print(f'trial {n} op {op}: {name} differ\n  exe  {want[k]}\n  port {got[k]}')
                sys.exit(1)
        log, i = want[3], 0
        while i < len(log):
            seen.add((op, log[i + 1]))
            i += log[i] + 1
    print(f'ok: {trials} jets and AddJumpJet calls identical; {sorted(seen)}')


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
    os._exit(code)

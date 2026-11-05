"""PM's Mech::Update 0x41ae00 against mech3/mech_update (docs/movement.md, The frame; docs/pm-mech-step.md), on test_update.py's
scenarios with PM's field offsets: the fuel, the parts before the collision, the commit of what predict made, the push and the
throttle's fall, the sounds, the knock. Everything else is a logging stub on both sides. Per-second fixes are off in the probe.

    cmake --build build --target pmprobe
    .venv/bin/python -P tests/differential/test_pm_update.py [trials]
"""
import os
import random
import re
import struct
import sys
from pathlib import Path

os.environ['MW3_PROBE_LIB'] = 'libpmprobe'  # test_update's own, not built on this branch
sys.path.insert(0, str(Path(__file__).parent))
import test_pm_zar  # noqa: E402
import test_update as tu  # noqa: E402
from original import FTOL, IAT_FTOL, ROOT  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402

UPDATE, MECH_VT, SIZE = 0x41ae00, 0x5dfaac, 0x800
EXT = 0x900  # the port's image, its PM fields at PM + 0x100
BUDGET, UPDATING, TIME, FLOOR, DEAD = 0x8d3ac0, 0x8d3ae4, 0x62da90, 0x6293b4, 0x62db48
FIELDS = {0xdc: 0xe0, 0xdd: 0xe1, 0xde: 0xe2, 0xe0: 0xe4, 0xe4: 0xe8, 0x414: 0x424, 0x528: 0x538, 0x53c: 0x550, 0x540: 0x554,
          0x544: 0x558, 0x548: 0x55c, 0x54c: 0x560, 0x570: 0x594, 0x578: 0x59c, 0x57c: 0x5a0, 0x580: 0x5a4, 0x584: 0x5a8,
          0x590: 0x5b4, 0x5ac: 0x5d0, 0x5b0: 0x5d4, 0x688: 0x6ac, 0x68c: 0x6b0, 0x6b0: 0x6d4, 0x7d0: 0x584, 0x7d4: 0x588}
for k in range(4):
    FIELDS[0x374 + k] = 0x384 + k
for o in range(0x594, 0x5ac, 4):
    FIELDS[o] = o + 0x24
for k in range(25):
    FIELDS[0x5b8 + 4 * k] = 0x5dc + 4 * k
for o in range(0x7dc, 0x828, 4):
    FIELDS[o] = o - 0x100
MECH = tu.MECH + [(0x7d0, 'f'), (0x7d4, 'f')] + [(o, 'f') for o in range(0x7dc, 0x828, 4)]
ASM = Path(__file__).parents[2] / 'analysis-output' / 'pm.asm'


def rets():
    out, cur = {}, None
    for l in ASM.read_text(errors='replace').split('\n'):
        g = re.match(r'\s+([0-9a-f]+):\s*[0-9a-f ]*\t(.*)', l)
        if not g:
            continue
        a, t = int(g.group(1), 16), ' '.join(g.group(2).split())
        if 0x401000 <= a < 0x5d0000:
            if cur is None:
                cur = a
            m = re.match(r'ret\s*(0x[0-9a-f]+)?', t)
            if m:
                out[cur] = int(m.group(1), 16) if m.group(1) else 0
                cur = None
    return out


class Emulated(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        self.cpu.import_code(IAT_FTOL, FTOL)
        a = self.alloc
        self.mech, self.mvt, self.ctl, self.vbt, self.pvt, self.ivt = a(SIZE), a(0x300), a(tu.CTL), a(8), a(0x10), a(0x80)
        self.pilot, self.skis, self.torso, self.transform, self.pos = a(0x40), a(0x300), a(tu.TORSO), a(48), a(12)
        self.parts = [a(0x100) for _ in range(5)]
        self.feet = [a(0x100), a(0x100)]
        self.pvts = [a(0x20) for _ in range(6)]
        self.dfn, self.view, self.viewp, self.player_obj = a(0x1b0), a(0x80), a(8), a(0x20)
        self._w(self.mvt, self.raw(MECH_VT, 0x300))
        self.vstub(self.mvt + 0x170, lambda c: self.log(38, c.arg(0)), 4)
        self._w(self.ivt, self.raw(self.cpu.tr(tu.CTL_VT, 4), 0x60))
        self.vstub(self.ivt + 0xc, lambda c: self.log(11, c.arg(0)) or self.go, 4)
        self.vstub(self.pvt + 4, lambda c: self.log(36) or self.ctl, 0)
        for k, code in enumerate((23, 24, 25, 26, 27, 28)):
            self.vstub(self.pvts[k] + 0x10, lambda c, code=code: self.log(code, c.arg(0)), 4)
        table = {0x41d920: lambda c: self.log(6), 0x58fe70: lambda c: self.log(1, c.arg(0) if self.first_arg(0x58fe70) else c.ecx),
                 0x41f340: lambda c: self.log(7, c.arg(0)), 0x4331c0: lambda c: self.log(8),
                 0x536c50: lambda c: self.log(9, c.arg(0)) or self.key, 0x4283f0: lambda c: self.log(10, c.arg(0)),
                 0x41dcd0: lambda c: self.log(12, c.arg(0)), 0x417350: lambda c: self.log(13, c.arg(0)),
                 0x418fc0: lambda c: self.log(14, c.arg(0)), 0x41c570: lambda c: self.log(15),
                 0x419de0: lambda c: self.log(16, c.arg(0)), 0x423730: self.collide, 0x41f740: lambda c: self.dfn,
                 0x5cb240: lambda c: self.log(19), 0x413df0: lambda c: self.log(20, self.r32(c.arg(0))),
                 0x4b6c90: lambda c: self.log(21) or self.weapons, 0x430e30: lambda c: self.log(40) or self.screen,
                 0x447da0: lambda c: self.log(22, self.screen), 0x4b4a80: lambda c: self.log(30, c.arg(0) & 0xff),
                 0x4b4a90: lambda c: self.log(31, c.arg(0)), 0x41a020: lambda c: self.log(32, c.arg(0)),
                 0x41a100: lambda c: self.log(33, c.arg(0)), 0x5907b0: lambda c: self.log(34, c.arg(0), c.arg(1)),
                 0x592c70: lambda c: self.log(35, c.arg(0)), 0x432d10: lambda c: self.handle(3, c.arg(0), c.arg(1), c.arg(4)),
                 0x592a50: lambda c: self.log(4, c.arg(0)), 0x4f8270: lambda c: self.log(37, c.ecx, c.arg(0)),
                 0x433500: lambda c: self.log(39, c.arg(0)), 0x5cb260: lambda c: self.pos, 0x40f760: lambda c: self.r32(c.ecx), 0x4e2e30: lambda c: self.log(41, c.ecx), 0x41df20: lambda c: self.log(5)}
        pops = rets()
        for addr, fn in table.items():
            self.native_stub(addr, fn, pops.get(addr, 0))
        self.w32(0x61874c, self.view), self.w32(0x62c40c, self.player_obj)
        self._w(self.pos, struct.pack('<3f', 1.5, 2.5, 3.5))

    def first_arg(self, a):
        return True

    def raw(self, a, n):
        return bytes(self.cpu._uc.mem_read(a, n))

    def _w(self, a, b):
        self.cpu._uc.mem_write(a, bytes(b))

    def w32(self, a, v):
        self._w(a, struct.pack('<I', v & 0xffffffff))

    def torso_out(self):
        got, out = self.raw(self.torso, 0x1b4), bytearray(tu.TORSO)
        for off, _ in tu.TORSO_FIELDS:
            out[off:off + 4] = got[off + 8:off + 12]
        return bytes(out)

    def vstub(self, slot, fn, pop):
        f = self.cpu.code(b'\xc3')
        self.w32(slot, f)
        self.native_stub(f, fn, pop)

    def collide(self, c):
        # The real one clears the flags and leaves at once for a mech that does not collide; the port asks first.
        if not self.raw(self.mech + 0x5d4, 1)[0]:
            self._w(self.mech + 0x384, bytes(4))
            return 0
        self.log(17)
        self._w(self.mech + 0x384, bytes(self.blocked))
        return self.held

    def log(self, *e):
        self.events.append(len(e))
        self.events.extend(v & 0xffffffff for v in e)
        return 0

    def handle(self, *e):
        self.log(*e)
        h = self.next
        self.next += 1
        return h

    def run(self, s):
        w, m = self.w32, self.mech
        self.events, self.next = [], 0x9000
        self.key, self.go, self.held, self.blocked = s['key'], s['go'], s['held'], s['blocked']
        self.weapons, self.screen = s['flags'] >> 4 & 1, 0x7000 if s['flags'] & 32 else 0
        img = bytearray(SIZE)
        for off, kind in MECH:
            n = 1 if kind == 'b' else 4
            p = FIELDS.get(off, off)
            img[p:p + n] = s['mech'][off:off + n]
        self._w(m, img)
        w(m, self.mvt)
        w(self.mvt + 0x64, 0x402250 if s['flags'] & 4 else 0x406ce0)
        w(m + 0xc, self.transform)
        self._w(self.transform, s['transform'])
        w(m + 0x38c, self.ctl), w(m + 0x6a8, self.pilot), w(self.pilot + 0xc, s['roster']), w(m + 0x42c, self.skis)
        self._w(self.skis + 5, bytes([s['slid']]))
        for k, rec in enumerate((0x1c0, 0x244, 0x29c)):
            self._w(self.skis + rec + 0x24, s['mech'][0x7c0 + 4 * k:0x7c4 + 4 * k])
        self._w(self.feet[0] + 0x94, s['mech'][0x7b0:0x7b4]), self._w(self.feet[1] + 0x94, s['mech'][0x7b4:0x7b8])
        self._w(self.skis + 0x160, struct.pack('<f', s['surf1']))
        self._w(self.skis + 0x16c, struct.pack('<f', s['surf4']))
        w(m + 0x494, s['actions'])
        w(m + 0x3ac, self.torso)
        t = bytearray(0x1b4)
        for off, _ in tu.TORSO_FIELDS:
            t[off + 8:off + 12] = s['torso'][off:off + 4]  # PM's torso is 8 further on here
        self._w(self.torso, t)
        w(self.torso, self.pvts[0])
        w(m + 0x3b0, self.parts[0]), w(self.parts[0], self.pvts[1])
        for k in range(4):
            w(m + 0x39c + 4 * k, self.parts[1 + k])
            w(self.parts[1 + k], self.pvts[2 + k])
        w(self.parts[1] + 0x78, self.feet[0]), w(self.parts[2] + 0x78, self.feet[1])
        self._w(self.ctl, s['ctl'])
        w(self.ctl, self.pvt), w(self.ctl + 4, self.vbt), w(self.vbt + 4, 0x5c), w(self.ctl + 0x5c, 0)
        w(self.ctl + 0x60, self.ivt), w(self.ctl + 0x70, m)
        self._w(self.dfn + 0x154, struct.pack('<2f', s['lean_angle'], s['lean_factor']))
        w(self.view + 0x64, m if s['flags'] & 1 else 0), w(self.view + 0x58, self.viewp), w(self.viewp + 4, 1 if s['flags'] & 8 else 0)
        w(self.player_obj + 0x14, m if s['flags'] & 2 else 0)
        self._w(TIME, struct.pack('<f', s['time'])), self._w(FLOOR, struct.pack('<f', s['floor'])), w(DEAD, s['dead'])
        b = s['budget']
        w(UPDATING, b[0])
        for k in range(4):
            w(BUDGET + 0x10 + 4 * k, b[1 + k])
        try:
            self.call(UPDATE, ecx=m, args=[tu.f2u(s['dt'])])
        except Exception as ex:
            uc = self.cpu._uc
            esp = uc.reg_read(UC_X86_REG_ESP)
            raise SystemExit(f'eip {uc.reg_read(UC_X86_REG_EIP):#x} ret {self.r32(esp):#x} log {self.events[-12:]}: {ex}')
        out = bytearray(EXT)
        got = self.raw(m, SIZE)
        for off, kind in MECH:
            n = 1 if kind == 'b' else 4
            p = FIELDS.get(off, off)
            out[off:off + n] = got[p:p + n]
        budget = [self.r32(UPDATING)] + [self.r32(BUDGET + 0x10 + 4 * k) for k in range(4)]
        return (tu.pick(bytes(out), MECH), tu.pick(self.raw(self.ctl, tu.CTL), tu.CTL_FIELDS),
                tu.pick(self.torso_out(), tu.TORSO_FIELDS), self.raw(self.transform, 48),
                budget + [self.r32(DEAD)], self.events)


def scenario(rng):
    s = tu.scenario(rng)
    m = bytearray(EXT)
    m[:len(s['mech'])] = s['mech']
    struct.pack_into('<f', m, 0x7d0, tu.f32(rng.choice([0.0, 0.3, 0.6])))
    struct.pack_into('<f', m, 0x7d4, tu.f32(rng.choice([0.0, 0.5, 0.99, 1.0])))
    q = tu.unit_quat(rng)
    for k in range(4):
        struct.pack_into('<f', m, 0x7dc + 4 * k, q[k])
    for o in range(0x7ec, 0x7f8, 4):
        struct.pack_into('<f', m, o, tu.f32(rng.uniform(-20, 20)))
    for o in range(0x7f8, 0x828, 4):
        struct.pack_into('<f', m, o, tu.f32(rng.uniform(-1, 1)))
    struct.pack_into('<ii', m, 0x7b0, rng.choice((0, 1, 3)), rng.choice((0, 1, 3)))
    struct.pack_into('<3i', m, 0x7c0, *[rng.choice((0, 0, 0x5500 + k)) for k in range(3)])
    struct.pack_into('<I', m, 0x68c, rng.choice((0, 0, 0x6000)))
    s['mech'] = bytes(m)
    s['roster'] = rng.randrange(25)
    return s


def main():
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
    tu.MECH_SIZE = EXT
    tu.MECH = MECH
    emu = Emulated()
    rng = random.Random(0x41ae00)
    seen = set()
    names = ('mech', 'controls', 'torso', 'matrix', 'budget', 'calls')
    for n in range(trials):
        s = scenario(rng)
        want, got = emu.run(s), tu.port(s, emu.torso)
        for k, name in enumerate(names):
            if want[k] != got[k]:
                if k < 4:
                    diff = [o for o in range(0, len(want[k]), 4) if want[k][o:o + 4] != got[k][o:o + 4]]
                    print(f'trial {n}: {name} differ at {[hex(o) for o in diff]}')
                    for o in diff[:5]:
                        print(f'  {o:#x}: exe {struct.unpack("<f", want[k][o:o+4])[0]} port {struct.unpack("<f", got[k][o:o+4])[0]}')
                else:
                    print(f'trial {n}: {name} differ\n  exe  {want[k][:60]}\n  port {got[k][:60]}')
                sys.exit(1)
        log, i = want[5], 0
        while i < len(log):
            seen.add(log[i + 1])
            i += log[i] + 1
    print(f'ok: {trials} Mech::Update calls identical; calls reached {sorted(seen)}')


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
    os._exit(code)

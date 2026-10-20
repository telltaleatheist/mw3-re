"""The weapon planner's three HUD getters (docs/ai.md section 17.7) against
the original x86: the selected group's longest range 0x4b1bd0, its lock-on
0x4b1c70 and its ammo 0x4b1cd0.

    cmake --build build && .venv/bin/python tests/differential/test_weapon_planner.py

They run on the managers test_weapons_run.py builds and fights with (the
real weapons.zrd, loadouts, groups, fire modes, ammo used up, beams in and
out of water, weapons destroyed, part loss): after every operation, once as
the manager stands and once with a random mode (0..2) and group (-1..4) set
on both sides for the call and put back, so empty groups in group fire come
up too. The exe answers the ammo of an empty group in group fire, or of a
group without a current weapon otherwise, with the top byte of the
manager's address (its saved ECX); the port says no (FIX (bug),
docs/fixes.md): counted and checked apart.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_weapons_run as wr  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX  # noqa: E402

probe = wr.probe
probe.probe_wm_planner.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_void_p]

MAX_RANGE, LOCKS_ON, HAS_AMMO = 0x4b1bd0, 0x4b1c70, 0x4b1cd0


class Run(wr.Run):
    stats = {'checks': 0, 'none': 0, 'group_fire': 0, 'empty_fire': 0, 'current': 0, 'no_current': 0,
             'range': 0, 'locks': 0, 'ammo': 0, 'no_ammo': 0}

    def getters(self, mode, group):
        emu, cpu = self.emu, self.emu.cpu
        m = emu.mgr
        saved = emu.cpu.read(m + 0x1c, 4), emu.cpu.read(m + 0x28, 4)
        cpu.u32(m + 0x1c, mode)
        cpu.u32(m + 0x28, group)
        cpu.call(MAX_RANGE, ecx=m)
        cpu.call(emu.fstp)
        rng_bits = emu.u32(emu.fslot)
        cpu.call(LOCKS_ON, ecx=m)
        locks = cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff
        cpu.call(HAS_AMMO, ecx=m)
        ammo = cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff
        g = 0 if group == 0xffffffff else emu.u32(m + 0x2c + 4 * group)
        count = emu.u32(g + 8) if g else 0
        current = emu.u32(g + 0x10) if g else 0
        cpu.write(m + 0x1c, saved[0])
        cpu.write(m + 0x28, saved[1])
        out = (ctypes.c_int32 * 3)()
        probe.probe_wm_planner(wr.s32(mode), wr.s32(group), out)
        port = (out[0] & 0xffffffff, out[1] & 0xff, out[2] & 0xff)
        orig = (rng_bits, locks, ammo)
        s = self.stats
        s['checks'] += 1
        empty_fire = g and (count == 0 if mode == 2 else current == 0)  # the exe's garbage exit 0x4b1d6e
        if not g:
            s['none'] += 1
        elif mode == 2:
            s['empty_fire' if count == 0 else 'group_fire'] += 1
        else:
            s['current' if current else 'no_current'] += 1
        if rng_bits:
            s['range'] += 1
        s['locks'] += locks != 0
        if empty_fire:
            if ammo != (m >> 24) & 0xff or port[2] != 0:  # FIX (bug): the exe's saved ECX byte, the port's no
                raise SystemExit(f'empty group or no current weapon: exe ammo {ammo}, port {port[2]}')
            orig = orig[:2] + (0,)
        else:
            s['ammo' if ammo else 'no_ammo'] += 1
        if orig != port:
            raise SystemExit(f'mode {mode} group {wr.s32(group)} (count {count}, current {current:#x}): '
                             f'exe range {struct.unpack("<f", struct.pack("<I", orig[0]))[0]} locks {orig[1]} '
                             f'ammo {orig[2]}, port {struct.unpack("<f", struct.pack("<I", port[0]))[0]} '
                             f'{port[1]} {port[2]}')

    def op(self, op, args):
        calls = super().op(op, args)
        emu = self.emu
        self.getters(emu.u32(emu.mgr + 0x1c), emu.u32(emu.mgr + 0x28))
        self.getters(self.rng.randrange(3), self.rng.choice([0xffffffff, 0, 1, 2, 3, 4]))
        return calls


def main():
    emu = wr.Emulated()
    rng = random.Random(4096)
    run = Run(emu, rng)
    managers = int(os.environ.get('WEAPON_MANAGERS', '200'))
    ops = 0
    for k in range(managers):
        run.new_manager()
        for _ in range(rng.randrange(1, 9)):
            if run.world.v['mech']:
                run.op(0, (1, rng.randrange(0, 5), rng.randrange(len(run.world.fps)), rng.choice(wr.MECH_WEAPONS), 1))
                ops += 1
        if rng.random() < 0.9:
            run.op(3, (rng.randrange(5),))
        for step in range(rng.randrange(20, 120)):
            if rng.random() < 0.3:
                run.world.script(rng)
            op, args = run.random_op()
            if op == 0 and not run.world.v['mech']:
                continue
            try:
                run.op(op, args)
            except SystemExit as e:
                raise SystemExit(f'manager {k} step {step}: {e}')
            ops += 1
    s = Run.stats
    need = ('none', 'group_fire', 'empty_fire', 'current', 'no_current', 'range', 'locks', 'ammo', 'no_ammo')
    missing = [n for n in need if not s[n]]
    if missing:
        raise SystemExit(f'cases never reached: {missing} ({s})')
    print(f'ok: {managers} managers, {ops} operations, {s["checks"]} checks of the three agree '
          f'({", ".join(f"{k} {v}" for k, v in s.items() if k != "checks")})')


if __name__ == '__main__':
    main()
    sys.stdout.flush()
    os._exit(0)

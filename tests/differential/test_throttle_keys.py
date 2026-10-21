"""The player's controls' command, 0x4274d0 (the throttle keys: Backspace,
1..0; docs/controls.md, docs/keybindings.md), against the port.

A PlayerControls image in emulator memory: the vbptr at +4 placing IControls
at +0x14c (the player's vtable 0x5bd650, vtordisp 0 at +0x148), the owner at
+0x15c a vehicle whose vt+0x50 gives a mech (or nothing) and the mech's
vt+0xa0 whether it is powered down. Random throttles, reverse flags and
commands 0x19..0x25; the image must match bit for bit.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402

probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libmw3probe.*'))))
probe.probe_player_command.argtypes = [ctypes.c_char_p, ctypes.c_int32, ctypes.c_int32]

COMMAND, SIZE = 0x4274d0, 0x160
FLOATS = [0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x44, 0x48, 0x4c]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def main():
    cpu = Original()
    ctl, vbt, veh, mech = cpu.alloc(SIZE), cpu.alloc(8), cpu.alloc(16), cpu.alloc(16)
    veh_vt, mech_vt = cpu.alloc(0x100), cpu.alloc(0x100)
    flag = cpu.alloc(4)
    mech_ret = cpu.alloc(4)
    # vt+0x50: mov eax, [mech_ret]; ret.  vt+0xa0: mov eax, [flag]; ret.
    cpu.u32(veh_vt + 0x50, cpu.code(b'\xa1' + struct.pack('<I', mech_ret) + b'\xc3'))
    cpu.u32(mech_vt + 0xa0, cpu.code(b'\xa1' + struct.pack('<I', flag) + b'\xc3'))
    cpu.u32(veh, veh_vt)
    cpu.u32(mech, mech_vt)
    rng = random.Random(0x4274d0)
    runs, kinds, moved = 4000, set(), 0
    for run in range(runs):
        b = bytearray(rng.randbytes(SIZE))
        for off in FLOATS:
            b[off:off + 4] = struct.pack('<f', f32(rng.choice([0.0, 0.0, 1.0, -1.0, 0.5, -0.3,
                                                                rng.uniform(-1.2, 1.2)])))
        for off in (0x3c, 0x3e, 0x50):
            b[off] = rng.randrange(2)
        b[4:8] = struct.pack('<I', vbt)
        b[0x148:0x14c] = bytes(4)                       # the thunks' vtordisp
        b[0x14c:0x150] = struct.pack('<I', 0x5bd650)    # PlayerControls' IControls vtable
        owner = rng.choice([0, 1, 1, 1, 2, 3])          # 3: a vehicle that is no mech
        b[0x15c:0x160] = struct.pack('<I', veh if owner else 0)
        cpu.u32(mech_ret, mech if owner in (1, 2) else 0)
        cpu.u32(flag, 1 if owner == 2 else 0)
        cpu.u32(vbt + 4, 0x148)                         # IControls at +4 + 0x148
        cmd = rng.randrange(0x19, 0x26)
        cpu.write(ctl, bytes(b))
        cpu.call(COMMAND, ecx=ctl, stack_args=(cmd,))
        want = bytearray(cpu.read(ctl, SIZE))
        port = ctypes.create_string_buffer(bytes(b), SIZE)
        probe.probe_player_command(port, {0: 0, 1: 1, 2: 2, 3: 0}[owner], cmd)
        got = bytearray(port.raw)
        # Compare what the port models: the floats, the flags +0x3c, +0x3e, +0x50.
        for off in FLOATS:
            if want[off:off + 4] != got[off:off + 4]:
                raise SystemExit(f'run {run}: cmd {cmd:#x} owner {owner}: +{off:#x} differs')
        for off in (0x3c, 0x3e, 0x50):
            if (want[off] != 0) != (got[off] != 0):
                raise SystemExit(f'run {run}: cmd {cmd:#x} owner {owner}: +{off:#x} differs')
        if want != b:
            moved += 1
            kinds.add(cmd)
    if moved < 1000 or len(kinds) < 11:
        raise SystemExit(f'coverage: {moved} changed, commands {sorted(kinds)}')
    print(f'ok: {runs} commands identical ({moved} changed the controls; commands {len(kinds)} kinds acting)')
    sys.stdout.flush()
    os._exit(0)


main()

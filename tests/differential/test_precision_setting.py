"""MW3_PRECISION=53 reaches both sides, and the check at exit catches a side
that is not there (original.py). Run with no arguments; it starts itself.

    cmake --build build --target mw3probe && .venv/bin/python tests/differential/test_precision_setting.py
"""
import ctypes
import os
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))


def child(mode):
    os.environ.setdefault('MW3_X87', '24')  # as a test's own default
    import original
    from original import ROOT, Original
    probe = ctypes.CDLL(str(next((ROOT / 'build').glob('libmw3probe.*'))))
    cpu = Original()
    cpu.fpu_control(0x007f)  # as 66 tests do in their own code
    # 1 / 3 stored as a double through the x87: 24 bits keep a float's digits.
    slot, one, three = cpu.alloc(8), cpu.alloc(4), cpu.alloc(4)
    cpu.u32(one, 0x3f800000)
    cpu.u32(three, 0x40400000)
    code = b'\xd9\x05' + struct.pack('<I', one) + b'\xd8\x35' + struct.pack('<I', three) + \
        b'\xdd\x1d' + struct.pack('<I', slot) + b'\xc3'
    cpu.call(cpu.code(code))
    third = struct.unpack('<d', bytes(cpu.uc.mem_read(slot, 8)))[0]
    print('third', third.hex(), 'cw', hex(cpu.uc.reg_read(original.UC_X86_REG_FPCW)), flush=True)
    if mode == 'port-off':
        probe.probe_set_fpu53(0)
    if mode == 'emu-off':
        original.PRECISION = None
        cpu.fpu_control(0x007f)


def run(mode, **env):
    e = {k: v for k, v in os.environ.items() if k not in ('MW3_X87', 'MW3_FPU53', 'MW3_PRECISION')}
    return subprocess.run([sys.executable, __file__, mode], env=dict(e, **env), capture_output=True, text=True)


def main():
    if len(sys.argv) > 1:
        return child(sys.argv[1])
    third53 = (1.0 / 3.0).hex()
    good = run('plain', MW3_PRECISION='53')
    if good.returncode != 0 or third53 not in good.stdout or 'port fpu53 1' not in good.stdout:
        raise SystemExit(f'not at 53: {good.stdout}{good.stderr}')
    plain = run('plain')
    if third53 in plain.stdout or 'precision:' in plain.stdout:
        raise SystemExit(f'no MW3_PRECISION changed something: {plain.stdout}')
    for mode in ('port-off', 'emu-off'):
        bad = run(mode, MW3_PRECISION='53')
        if bad.returncode != 3 or 'NOT at 53 bits' not in bad.stdout:
            raise SystemExit(f'{mode} was not caught: {bad.returncode} {bad.stdout}')
    print('ok: both sides at 53 on request, as they were without it, and either side off fails the run')


if __name__ == '__main__':
    main()

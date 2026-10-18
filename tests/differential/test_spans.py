"""The ported span routines against the original x86 code, on random inputs.

    cmake --build build && .venv/bin/python tests/differential/test_spans.py
"""
import ctypes
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
u16p = ctypes.POINTER(ctypes.c_uint16)

DEST, TEXTURE, DU, DV, VMASK = 0x0072c278, 0x0072c268, 0x0072c270, 0x0072c274, 0x005efb98
PAD = 8  # pixels of guard on each side, to catch writes outside the span


def rebuilt_fill(color, count):
    buf = (ctypes.c_uint16 * (count + 2 * PAD))(*([0xaaaa] * (count + 2 * PAD)))
    probe.probe_span_fill16(ctypes.cast(ctypes.byref(buf, PAD * 2), u16p), color, count)
    return bytes(buf)


def original_fill(cpu, color, count):
    buf = cpu.alloc((count + 2 * PAD) * 2)
    cpu.write(buf, b'\xaa' * ((count + 2 * PAD) * 2))
    cpu.u32(DEST, buf + PAD * 2)
    cpu.call(0x00564080, ecx=color, edx=count)
    return cpu.read(buf, (count + 2 * PAD) * 2)


def rebuilt_textured(tex, du, dv, mask, u, v, count):
    buf = (ctypes.c_uint16 * (count + 2 * PAD))(*([0xaaaa] * (count + 2 * PAD)))
    t = (ctypes.c_uint16 * len(tex))(*tex)
    probe.probe_span_textured16(ctypes.cast(ctypes.byref(buf, PAD * 2), u16p), t, du, dv, mask, u, v, count)
    return bytes(buf)


def original_textured(cpu, tex_addr, du, dv, mask, u, v, count):
    buf = cpu.alloc((count + 2 * PAD) * 2)
    cpu.write(buf, b'\xaa' * ((count + 2 * PAD) * 2))
    cpu.u32(DEST, buf + PAD * 2)
    cpu.u32(TEXTURE, tex_addr)
    cpu.u32(DU, du)
    cpu.u32(DV, dv)
    cpu.u32(VMASK, mask)
    cpu.call(0x0056cb40, ecx=u, edx=v, stack_args=(count,))
    return cpu.read(buf, (count + 2 * PAD) * 2)


def main():
    rng = random.Random(1999)
    cases = 0
    for _ in range(2000):
        cpu = Original() if cases % 250 == 0 else cpu  # fresh heap now and then
        color = rng.choice([rng.getrandbits(16), rng.getrandbits(32)])
        count = rng.randint(1, 640)
        a, b = original_fill(cpu, color, count), rebuilt_fill(color, count)
        if a != b:
            raise SystemExit(f'span_fill16 differs: color={color:#x} count={count}')
        cases += 1
    # The texture holds 1024 columns and up to 64 rows; v masks keep rows in range.
    tex = [rng.getrandbits(16) for _ in range(1024 * 64)]
    cpu = Original()
    tex_addr = cpu.alloc(len(tex) * 2)
    cpu.write(tex_addr, b''.join(t.to_bytes(2, 'little') for t in tex))
    for _ in range(2000):
        mask = (rng.getrandbits(6) << 16) & 0x3f0000  # row bits only, rows 0..63
        du, dv = rng.getrandbits(32), rng.getrandbits(32)
        u, v = rng.getrandbits(32), rng.getrandbits(32)
        count = rng.randint(1, 640)
        a = original_textured(cpu, tex_addr, du, dv, mask, u, v, count)
        b = rebuilt_textured(tex, du, dv, mask, u, v, count)
        if a != b:
            raise SystemExit(f'span_textured16 differs: du={du:#x} dv={dv:#x} mask={mask:#x} u={u:#x} v={v:#x} count={count}')
        cases += 1
        if cpu.heap_next > 0x10f00000:
            cpu = Original()
            tex_addr = cpu.alloc(len(tex) * 2)
            cpu.write(tex_addr, b''.join(t.to_bytes(2, 'little') for t in tex))
    print(f'{cases} cases: the rebuilt spans write exactly what the original x86 writes')


if __name__ == '__main__':
    main()

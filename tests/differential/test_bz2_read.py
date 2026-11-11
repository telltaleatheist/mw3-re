"""Our bzip2 reader (src/upgrades/mods/bz2_read.cpp, written from the format) against Python's bz2 module.

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/run_capped.py -- \
        .venv/bin/python -P tests/differential/test_bz2_read.py [cases]

Python makes each stream; our reader must give the input back byte for byte: empty, one byte, runs on both sides
of the run-length stages' limits (4, 255 + 4, 256 ...), every level 1 to 9, inputs larger than a block at level 1
(several blocks in one stream), all 256 byte values, text, bytes already compressed, and random sizes and
contents. Then hostile streams: every truncation and every single-bit flip of a few small streams, a cap smaller
than the output, and made-up headers: each refused, or (a bit the format does not use) the same bytes; never
other bytes, never more than the cap. Two streams one after the other: the first is read and `used` says where
it ended (an installer's entry holds one stream; the reader never reads past it).
"""
import bz2
import ctypes
import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
probe.probe_bz2.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_uint32,
                            ctypes.POINTER(ctypes.c_uint32)]
probe.probe_bz2.restype = ctypes.c_int32


def ours(stream, max_out):
    out = ctypes.create_string_buffer(max_out + 16)
    used = ctypes.c_uint32(0)
    n = probe.probe_bz2(stream, len(stream), max_out, out, max_out + 16, ctypes.byref(used))
    if n == -2:
        raise SystemExit('the reader wrote more than its cap')
    return (out.raw[:n], used.value) if n >= 0 else (None, 0)


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    rng = random.Random(1999)
    inputs = [b'', b'a', b'ab', b'aaaa', b'aaaaa', bytes(range(256)), bytes(range(256)) * 50,
              b'the quick brown fox jumps over the lazy dog. ' * 400, bz2.compress(rng.randbytes(40000)),
              rng.randbytes(300000), b'\x00' * 250000, bytes([7]) * 100000 + rng.randbytes(1000)]
    for n in list(range(1, 12)) + [250, 251, 252, 253, 254, 255, 256, 257, 258, 259, 260, 261, 510, 511, 512, 513, 514, 515, 516,
                                   65535, 65536, 99999, 100000, 100001]:
        inputs.append(b'r' * n)
        inputs.append(b'r' * n + b'x' + b'r' * n)
    while len(inputs) < cases:
        kind = rng.randrange(6)
        size = rng.choice([rng.randrange(0, 64), rng.randrange(0, 5000), rng.randrange(0, 200000)])
        if kind == 0:
            inputs.append(rng.randbytes(size))
        elif kind == 1:
            inputs.append(bytes(rng.choice(b'abc') for _ in range(min(size, 20000))))
        elif kind == 2:  # runs of random lengths
            b = bytearray()
            while len(b) < size:
                b += bytes([rng.randrange(256)]) * rng.choice([1, 2, 3, 4, 5, 6, 200, 255, 256, 259, 260, 1000])
            inputs.append(bytes(b[:size]))
        elif kind == 3:
            inputs.append((b'MechWarrior %d ' % rng.randrange(9)) * (size // 14))
        elif kind == 4:
            inputs.append(bytes(rng.randrange(4) for _ in range(min(size, 30000))))
        else:
            inputs.append(bytes([rng.randrange(256)]) * size)
    blocks = 0
    for i, data in enumerate(inputs):
        for level in ([1, 9] if i >= 80 else range(1, 10)):
            z = bz2.compress(data, level)
            got, used = ours(z, len(data))
            if got != data:
                raise SystemExit(f'case {i} level {level}, {len(data)} bytes: ' + ('refused' if got is None else f'{len(got)} bytes, differ'))
            if used != len(z):
                raise SystemExit(f'case {i} level {level}: used {used} of {len(z)}')
            blocks += len(data) > level * 100000
    # Two streams one after the other: the first alone, and where it ended.
    a, b = bz2.compress(b'first stream'), bz2.compress(b'second')
    got, used = ours(a + b, 100)
    if got != b'first stream' or used != len(a):
        raise SystemExit('two streams: the first was not read alone')
    # Hostile: truncations, bit flips, a small cap, made-up headers.
    refused = same = 0
    for data in (b'hello hello hello world', bytes(range(256)), b'a' * 300 + b'b', rng.randbytes(200), b'the quick brown fox ' * 40,
                 b'z' * 260, b''):
        z = bz2.compress(data, 1)
        for cut in range(len(z)):
            got, _ = ours(z[:cut], len(data))
            if got is not None:
                raise SystemExit(f'a stream cut at {cut} of {len(z)} was taken')
            refused += 1
        for bit in range(len(z) * 8):
            m = bytearray(z)
            m[bit // 8] ^= 1 << (bit % 8)
            got, _ = ours(bytes(m), len(data))
            if got is None:
                refused += 1
            elif got == data:
                same += 1
            else:
                raise SystemExit(f'bit {bit} flipped gave other bytes ({len(got)})')
        for cap in range(0, len(data), max(1, len(data) // 9)):
            got, _ = ours(z, cap)
            if got is not None:
                raise SystemExit(f'a cap of {cap} for {len(data)} bytes was ignored')
            refused += 1
    z = bytearray(bz2.compress(b'made-up headers ' * 20, 1))
    for patch in ((3, ord('0')), (3, ord(':')), (0, ord('A')), (4, 0x00), (10, z[10] ^ 0xff), (14, z[14] | 0x80),
                  (14, 0x7f), (18, 0x00), (19, 0xff)):
        m = bytearray(z)
        m[patch[0]] = patch[1]
        got, _ = ours(bytes(m), 4000)
        if got is not None and got != b'made-up headers ' * 20:
            raise SystemExit(f'a made-up header {patch} gave other bytes')
        refused += got is None
    print(f'ok: {len(inputs)} inputs unpacked as Python packed them ({blocks} streams of several blocks); '
          f'{refused} hostile streams refused, {same} flips of unused bits the same bytes, none other')


if __name__ == '__main__':
    main()

"""Ctrl+I in the lab, the real program in a hidden window: a loadout in import\\ lands in the variants of the shown mech,
with a number, its name being taken, and the file becomes .m3c.done. A .done file named back to .m3c is read again.

    cmake --build build --target mw3 && .venv/bin/python tests/differential/test_lab_import.py
"""
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def bmp(path):
    d = Path(path).read_bytes()
    off, w, h, bpp = struct.unpack('<I', d[10:14])[0], *struct.unpack('<ii', d[18:26]), struct.unpack('<H', d[28:30])[0]
    stride = ((w * bpp + 31) // 32) * 4
    tall = abs(h)

    def px(x, y):
        at = off + (tall - 1 - y if h > 0 else y) * stride + x * bpp // 8
        return d[at + 2], d[at + 1], d[at]
    return px


def run(wd, name, taps):
    shot = wd.parent / f'{name}.bmp'
    subprocess.run([ROOT / 'build' / 'mw3', ROOT / 'game' / 'mw3-1.2', '--lab-mech', '2', '--hidden', '--no-launcher',
                    '--nosound', '--write-dir', wd, '--frame-ms', '33', '--seed', '1', '--shot-any', '--shot', shot,
                    '--shot-frame', '2900', *taps], check=True, timeout=120, capture_output=True)
    return bmp(shot)


def second_variant_lit(px):
    """Bright letters on the second line of VARIANTS, 640 x 480 at twice the size."""
    return sum(px(x, y)[1] > 200 for x in range(905, 1200) for y in range(600, 630))


def main():
    with tempfile.TemporaryDirectory() as t:
        wd = Path(t) / 'wd'
        wd.mkdir()
        CTRL_X = ['--tap', '0x1d@2700:2720', '--tap', '0x2d@2705:2708']
        CTRL_I = ['--tap', '0x1d@2700:2720', '--tap', '0x17@2705:2708']
        run(wd, 'export', CTRL_X)
        files = list((wd / 'export').glob('*.m3c'))
        assert len(files) == 1, 'Ctrl+X wrote no file'
        assert second_variant_lit(run(wd, 'before', [])) == 0, 'a second variant before the import'
        (wd / 'import').mkdir()
        shutil.copy(files[0], wd / 'import' / 'mine.m3c')
        first = second_variant_lit(run(wd, 'import', CTRL_I))
        done = wd / 'import' / 'mine.m3c.done'
        print(f'the second variant line after Ctrl+I: {first} lit; .done: {done.exists()}')
        assert first > 100, 'the imported loadout is not in the variants'
        assert done.exists()

        done.rename(wd / 'import' / 'mine.m3c')  # what a player does to read a .done file again
        again = second_variant_lit(run(wd, 'again', CTRL_I))
        print(f'named back and read again: {again} lit')
        assert again > 100


if __name__ == '__main__':
    main()

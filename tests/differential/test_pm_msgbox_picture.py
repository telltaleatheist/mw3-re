"""PM's quit box as a picture, the real program in a hidden window: what test_twod_msgbox can't see. The box's buttons
are alpha images over the panel; around each the panel must stay the panel. They had squares of the screen behind them
(twod_dialog.cpp, the static grab), on every box. The port's own frames; the exe's are Owen's shot (2026-11-06).

    cmake --build build --target mw3pm && .venv/bin/python tests/differential/test_pm_msgbox_picture.py
"""
import struct
import subprocess
import sys
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
    return w, tall, px


def main():
    with tempfile.TemporaryDirectory() as t:
        shot = Path(t) / 'q.bmp'
        subprocess.run([ROOT / 'build' / 'mw3pm', '--install', ROOT / 'game' / 'pm', '--hidden', '--nosound', '--write-dir', t,
                        '--shot', shot, '--shot-frame', '190', '--frame-ms', '33', '--seed', '1', '--tap', '0x01@150'],
                       check=True, timeout=120, capture_output=True)
        w, h, px = bmp(shot)
    assert (w, h) == (1280, 960), (w, h)
    # The panel, 212, 158 in 640 x 480 at twice the size: the corners of the two buttons' 42 x 42 squares, and
    # the panel by them. Close to it, not the screen behind.
    panel = px(640, 540)
    worst = 0
    for bx, by in ((452, 540), (744, 540)):
        for x, y in ((bx + 1, by + 1), (bx + 82, by + 1), (bx + 1, by + 82), (bx + 82, by + 82)):
            worst = max(worst, max(abs(a - b) for a, b in zip(px(x, y), panel)))
    print(f'panel {panel}, the buttons\' corners within {worst} of it')
    if worst > 12:
        sys.exit('a button has a square of something else behind it')


if __name__ == '__main__':
    main()

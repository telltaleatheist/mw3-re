"""PM's zoom window as a picture, the real program twice in a hidden window: with Z held and without. Outside the window
the two must match: strips skip the software side clip for Direct3D's viewport, and without the renderer's clip the
zoomed world ran over the main view (sdl_render3d, set_clip; the exe's picture is Owen's shot).

    cmake --build build --target mw3pm && .venv/bin/python tests/differential/test_pm_zoom_picture.py
"""
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_pm_msgbox_picture import ROOT, bmp  # noqa: E402

# The square window at 1280 x 960, and the label under it.
WINDOW = (540, 380, 740, 610)


def shot(tmp, name, *extra):
    d = Path(tmp) / name
    d.mkdir()
    path = d / 's.bmp'
    subprocess.run([ROOT / 'build' / 'mw3pm', '--install', ROOT / 'game' / 'pm', '--mission', 'c1m1', '--hidden',
                    '--nosound', '--write-dir', d, '--frame-ms', '33', '--seed', '1', '--shot', path, '--shot-frame',
                    '340', *extra], check=True, timeout=120, capture_output=True)
    return bmp(path)


def main():
    with tempfile.TemporaryDirectory() as t:
        w, h, plain = shot(t, 'plain')
        _, _, zoom = shot(t, 'zoom', '--tap-ingame', '0x2c@280:345')
        changed = 0
        for y in range(120, 700, 2):  # below the top bar, above the dash
            for x in range(120, 1160, 2):
                if WINDOW[0] <= x < WINDOW[2] and WINDOW[1] <= y < WINDOW[3]:
                    continue
                if max(abs(a - b) for a, b in zip(plain(x, y), zoom(x, y))) > 40:
                    changed += 1
    print(f'{w} x {h}: {changed} of the sampled pixels outside the window changed with the zoom on')
    if changed > 200:
        sys.exit('the zoom draws outside its window')


if __name__ == '__main__':
    main()

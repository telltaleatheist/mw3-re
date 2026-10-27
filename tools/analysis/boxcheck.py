"""The message box over the screen in the program (docs/twod.md, the message
box): build/mw3 with --nosound --hidden on scratch write dirs.

    .venv/bin/python tools/analysis/boxcheck.py [--keep DIR]

1. The main menu's Escape: "Are you sure you want to quit?" must stand over
   the main menu (the screen outside the box as the menu alone shows it).
2. A mission won while its next game's save exists: "Replace existing
   game?" after the end screen's fade to black must stand on black, and the
   OK / NO buttons' corners (their images' see-through pixels) must show the
   dialog's green, not the backdrop.

The game pixels are the 640 x 480 screen's; the shots are the 1280 x 960
window's.
"""
import argparse
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MW3 = ROOT / 'build' / 'mw3'
sys.path.insert(0, str(ROOT / 'tools' / 'analysis'))
from savecheck import ACCEPT, LAUNCH, OP1M1_ROW, PILOT_CLICKS, run as play  # noqa: E402


def shot(write_dir, args, frame, out):
    cmd = [str(MW3), '--nosound', '--hidden', '--write-dir', str(write_dir), '--shot', str(out),
           '--shot-frame', str(frame)]
    for a in args:
        cmd += a if isinstance(a, list) else ['--click-at', a]
    subprocess.run(cmd, capture_output=True, timeout=900)
    d = out.read_bytes()
    off = struct.unpack_from('<I', d, 10)[0]
    w, h = struct.unpack_from('<ii', d, 18)
    bpp = struct.unpack_from('<H', d, 28)[0] // 8
    row = (w * bpp + 3) // 4 * 4

    def px(x, y):  # game pixels
        x, y = x * w // 640, y * abs(h) // 480
        i = off + ((abs(h) - 1 - y) if h > 0 else y) * row + x * bpp
        return d[i + 2], d[i + 1], d[i]
    return px


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--keep', type=Path)
    a = ap.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        root = a.keep or Path(tmp)
        root.mkdir(parents=True, exist_ok=True)
        fails = checks(root)
    sys.exit(1 if fails else 0)


def checks(root):
    fails = []

    def check(ok, what):
        print(('ok   ' if ok else 'FAIL ') + what)
        if not ok:
            fails.append(what)

    wd = root / 'menu'
    wd.mkdir(exist_ok=True)
    # The menu as the box found it (the Escape at 100, the box from 101; the
    # menu's backdrop moves, so near, not equal).
    menu = shot(wd, [], 101, root / 'menu.bmp')
    quit_ = shot(wd, [['--tap', '1@100:102']], 160, root / 'quit.bmp')
    outside = [(x, y) for x in range(10, 640, 60) for y in (10, 60, 120, 360, 420, 470)]
    diff = sum(abs(a - b) for x, y in outside for a, b in zip(quit_(x, y), menu(x, y))) / (3 * len(outside))
    magenta = sum(1 for x, y in outside if quit_(x, y) == (255, 0, 255))
    check(magenta == 0 and diff < 20, f'the quit box stands over the main menu (mean difference {diff:.1f}, key {magenta})')
    # The Replace box: a saved Op 1 Mission 2, then Op 1 Mission 1 won again.
    saved = root / 'saved'
    shutil.rmtree(saved, ignore_errors=True)
    play(saved, [['--briefing', 'c1m1'], LAUNCH.format(1500), ['--win', '60']], 6000)
    replace = root / 'replace'
    shutil.rmtree(replace, ignore_errors=True)
    shutil.copytree(saved, replace)
    box = shot(replace, PILOT_CLICKS + [OP1M1_ROW, ACCEPT.format(160), LAUNCH.format(1560), ['--win', '60']], 5550,
               root / 'replace.bmp')
    back = [box(x, y) for x, y in outside]
    check(all(max(p) <= 8 for p in back), f'the Replace box stands on black (the fade\'s end): {back[:2]}')
    # The buttons' squares (OK about 226..266 x 270..310, NO about 372..412):
    # their corners see-through, so the dialog's dark green shows.
    corners = [(230, 276), (266, 276), (377, 276), (413, 276)]
    cs = [box(x, y) for x, y in corners]
    check(all(p[1] > p[0] and p[1] > p[2] and p[1] >= 8 for p in cs),
          f'the buttons\' corners show the dialog: {cs}')
    print(f'{len(fails)} failed')
    return fails


if __name__ == '__main__':
    main()

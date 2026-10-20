"""The mission-start fade in the program (uses the GPU; docs/render-frame.md):
shoots frames during the fade with `mw3 --shot-any` and checks each shows a
picture (not flat, not black), brighter as the fade goes on.

    .venv/bin/python tools/analysis/fadecheck.py [mission] [frame ...]
"""
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def stats(path):
    d = path.read_bytes()
    off = struct.unpack_from('<I', d, 10)[0]
    bpp = struct.unpack_from('<H', d, 28)[0] // 8
    px = d[off:]
    vals, colours = [], set()
    for i in range(0, len(px) - bpp + 1, bpp * 97):
        b, g, r = px[i], px[i + 1], px[i + 2]
        vals.append((r * 3 + g * 6 + b) / 10)
        colours.add((r >> 3, g >> 3, b >> 3))
    m = sum(vals) / len(vals)
    sd = (sum(v * v for v in vals) / len(vals) - m * m) ** 0.5
    return m, sd, len(colours)


def main():
    mission = sys.argv[1] if len(sys.argv) > 1 else 'c1m1'
    frames = [int(f) for f in sys.argv[2:]] or [60, 200, 400]
    ok, last = True, -1.0
    with tempfile.TemporaryDirectory() as tmp:
        for f in frames:
            shot = Path(tmp) / f'fade{f}.bmp'
            subprocess.run([str(ROOT / 'build' / 'mw3'), '--mission', mission, '--nosound', '--shot', str(shot),
                            '--shot-frame', str(f), '--shot-any'], cwd=ROOT, capture_output=True)
            if not shot.exists():
                print(f'frame {f}: no shot')
                ok = False
                continue
            m, sd, n = stats(shot)
            good = m > 1.0 and sd > 0.5 and m > last
            print(f'frame {f}: mean {m:.1f}, sd {sd:.1f}, {n} colours: {"ok" if good else "FLAT, BLACK OR NOT BRIGHTER"}')
            ok &= good
            last = m
    print('the fade shows the picture' if ok else 'the fade does not show the picture')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()

"""PM's loadout file box, the real program in a hidden window: Ctrl+X and Ctrl+I in the lab open LOADSAVE_DIALOG,
Enter accepts, Escape cancels. The saved file carries the default name in configs; a loaded one lands in the
variants as the exe's 0x0044a010 puts it, with a number, the name being taken.

    cmake --build build --target mw3pm && .venv/bin/python tests/differential/test_pm_lab_filebox.py
"""
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).parent))
from test_pm_msgbox_picture import bmp  # noqa: E402

SAVE = ['--tap', '0x1d@2700:2720', '--tap', '0x2d@2705:2708']
LOAD = ['--tap', '0x1d@2900:2920', '--tap', '0x17@2905:2908']
ENTER, ESCAPE = '0x1c', '0x01'


def run(t, name, taps):
    wd = Path(t) / name
    shot = wd / 'shot.bmp'
    wd.mkdir()
    subprocess.run([ROOT / 'build' / 'mw3pm', '--install', ROOT / 'game' / 'pm', '--lab-mech', '2', '--hidden',
                    '--nosound', '--write-dir', wd, '--frame-ms', '33', '--seed', '1', '--shot-any', '--shot', shot,
                    '--shot-frame', '3100', *taps], check=True, timeout=120, capture_output=True)
    return wd, bmp(shot)


def second_variant_lit(px):
    """Bright letters on the second line of VARIANTS, 640 x 480 at twice the size."""
    return sum(px(x, y)[1] > 200 for x in range(905, 1200) for y in range(600, 630))


def main():
    with tempfile.TemporaryDirectory() as t:
        wd, _ = run(t, 'cancelled', SAVE + ['--tap', ESCAPE + '@2800:2803'])
        saved = wd / 'configs' / 'centurion_Primary.m3c'
        print(f'Escape on the save box: {"a file" if saved.exists() else "no file"}')
        assert not saved.exists()

        wd, (_, _, px) = run(t, 'load_cancelled', SAVE + ['--tap', ENTER + '@2800:2803'] + LOAD +
                             ['--tap', ESCAPE + '@3000:3003'])
        saved = wd / 'configs' / 'centurion_Primary.m3c'
        assert saved.stat().st_size > 100, 'Enter on the save box wrote nothing'
        kept = second_variant_lit(px)

        wd, (_, _, px) = run(t, 'loaded', SAVE + ['--tap', ENTER + '@2800:2803'] + LOAD +
                             ['--tap', ENTER + '@3000:3003'])
        added = second_variant_lit(px)
        print(f'saved {saved.stat().st_size} bytes; the second variant line: {kept} lit cancelled, {added} loaded')
        assert kept == 0 and added > 100, 'the loaded loadout is not in the variants'


if __name__ == '__main__':
    main()

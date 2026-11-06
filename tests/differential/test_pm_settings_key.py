"""mw3pm's settings key: PM's own, HKCU\\SOFTWARE\\MicroProse\\MechWarrior 3 EP1\\1.0 as the exe's
0x4828d2. A settings file from before that change, under the empty product, must not lose anything:
its values are read, and a save keeps them and every section the game doesn't own. Runs the
program, hidden and silent, on scratch write dirs only.

    cmake --build build --target mw3pm && .venv/bin/python -P tests/differential/test_pm_settings_key.py
"""
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OLD = 'HKEY_CURRENT_USER\\SOFTWARE\\MicroProse\\\\1.0'
NEW = 'HKEY_CURRENT_USER\\SOFTWARE\\MicroProse\\MechWarrior 3 EP1\\1.0'
OTHER = 'HKEY_CURRENT_USER\\SOFTWARE\\Somebody\\Else'


def run(exe, wd):
    env = dict(os.environ, SDL_AUDIODRIVER='dummy')
    subprocess.run([str(exe), '--hidden', '--nosound', '--write-dir', str(wd), '--frame-ms', '33', '--seed', '1',
                    '--install', str(ROOT / 'game' / 'pm'), '--frames', '60'], env=env, cwd=ROOT, capture_output=True, timeout=120)
    return (wd / 'settings.reg').read_text(errors='replace')


def section(text, name):
    out = {}
    on = False
    for line in text.splitlines():
        if line.startswith('['):
            on = line.strip('[]\r') == name
        elif on and line.startswith('"'):
            k, _, v = line.partition('=')
            out[k] = v.strip()
    return out


def main():
    tmp = Path(tempfile.mkdtemp(prefix='pm_settings_', dir=os.environ.get('TMPDIR')))
    exe = tmp / 'mw3pm_key'
    shutil.copy(ROOT / 'build' / 'mw3pm', exe)
    bad = []
    try:
        fresh = tmp / 'fresh'
        fresh.mkdir()
        text = run(exe, fresh)
        if not section(text, NEW):
            bad.append('a new file has no values under the new key')
        if section(text, OLD):
            bad.append('a new file wrote under the old key')
        old = tmp / 'old'
        old.mkdir()
        (old / 'settings.reg').write_text(f'Windows Registry Editor Version 5.00\n\n[{OLD}]\n"SoundVolume"=hex:00,00,40,3f\n"KeptMarker"=dword:00001234\n\n[{OTHER}]\n"Theirs"=dword:00000007\n')
        text = run(exe, old)
        new = section(text, NEW)
        if new.get('"SoundVolume"') != 'hex:00,00,40,3f':
            bad.append(f'the old volume was not read: {new.get(chr(34) + "SoundVolume" + chr(34))}')
        if new.get('"KeptMarker"') != 'dword:00001234':
            bad.append('a value the game does not know was lost in the move')
        if section(text, OTHER).get('"Theirs"') != 'dword:00000007':
            bad.append("another key's section was lost")
        if section(text, OLD).get('"KeptMarker"') != 'dword:00001234':
            bad.append('the old section was lost')
        # Both there: the new key wins, the old is left alone.
        both = tmp / 'both'
        both.mkdir()
        (both / 'settings.reg').write_text(f'[{NEW}]\n"SoundVolume"=hex:00,00,80,3e\n\n[{OLD}]\n"SoundVolume"=hex:00,00,40,3f\n')
        text = run(exe, both)
        if section(text, NEW).get('"SoundVolume"') != 'hex:00,00,80,3e':
            bad.append('the old key beat the new')
        if section(text, OLD).get('"SoundVolume"') != 'hex:00,00,40,3f':
            bad.append('the old section changed')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    for b in bad:
        print(b)
    print(f'{len(bad)} bad')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()

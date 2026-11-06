"""install.cmd and restore.cmd on real Windows cmd, in a scratch folder with dummy game files. Needs Windows and an unzipped
release, as make_zip.py makes it. No game file is touched, and the stand-in is never loaded.

    python test_install_cmd.py <unzipped release> <scratch folder>
"""
import shutil
import subprocess
import sys
from pathlib import Path

ZIP, SCRATCH = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
OURS = (ZIP / 'install-files' / 'IFORCE2-standin.dll').read_bytes()
SHIPPED_ROWS = (ZIP / 'install-files' / 'weapon-rows.txt').read_bytes()
ORIGINAL = b'MZ' + bytes(61438)  # the game's is 61,440 bytes; this is a dummy
failures = []


def run(script, *args, folder=ZIP):
    """As Explorer runs a dropped folder: cmd /c ""script" "arg"". Stdin is empty, so pause returns at once."""
    words = ' '.join(f'"{a}"' for a in (folder / script, *args))
    r = subprocess.run(f'cmd /d /s /c "{words}"', stdin=subprocess.DEVNULL, capture_output=True, text=True, errors='replace')
    return r.returncode, r.stdout + r.stderr


def snapshot(game):
    return {p.name: p.read_bytes() for p in game.iterdir() if p.is_file()}


def fresh(name='game (x86) & co', exe='Mech3.exe'):
    game = SCRATCH / name
    shutil.rmtree(game, ignore_errors=True)
    game.mkdir(parents=True)
    if exe:
        (game / exe).write_bytes(b'dummy exe')
    (game / 'IFORCE2.dll').write_bytes(ORIGINAL)
    return game


def case(what, got, want_code, text, game=None, before=None, after=None):
    code, out = got
    problems = []
    if code != want_code:
        problems.append(f'exit {code}, want {want_code}')
    if text not in out:
        problems.append(f'no {text!r} in the output')
    if 'Press any key' not in out:
        problems.append('no pause, so a double-click would lose this text')
    if game is not None:
        state = snapshot(game)
        if before is not None and state != before:
            problems.append(f'files changed: {sorted(state)}')
        for name, data in (after or {}).items():
            if state.get(name) != data:
                problems.append(f'{name} is wrong, or missing')
    print(f'{"FAIL" if problems else "ok  "} {what}' + (f': {problems}\n{out}' if problems else ''))
    failures.extend(problems)


for name in ('install.cmd', 'restore.cmd', 'INSTALL.txt'):  # cmd can lose its labels in a file with bare LFs
    data = (ZIP / name).read_bytes()
    if data.count(b'\n') != data.count(b'\r\n'):
        failures.append(f'{name} is not all CRLF')
        print(f'FAIL {name} is not all CRLF')

game = fresh()
case('a fresh install', run('install.cmd', game), 0, 'Installed in', game,
     after={'IFORCE2o.dll': ORIGINAL, 'IFORCE2.dll': OURS, 'weapon-rows.txt': SHIPPED_ROWS, 'Mech3.exe': b'dummy exe'})
installed = snapshot(game)
case('install twice', run('install.cmd', game), 1, 'already', game, before=installed)
case('a restore', run('restore.cmd', game), 0, 'Restored in', game,
     after={'IFORCE2.dll': ORIGINAL, 'weapon-rows.txt': SHIPPED_ROWS})
if (game / 'IFORCE2o.dll').exists():
    failures.append('IFORCE2o.dll left after the restore')
restored = snapshot(game)
case('a restore with nothing installed', run('restore.cmd', game), 1, 'nothing to put back', game, before=restored)

(game / 'weapon-rows.txt').write_bytes(b'# my own rows\r\n')
case('an install over edited rows keeps them', run('install.cmd', game), 0, 'kept', game,
     after={'weapon-rows.txt': b'# my own rows\r\n', 'IFORCE2.dll': OURS})
run('restore.cmd', game)

game = fresh()
(game / 'IFORCE2.dll').write_bytes(OURS)  # a paste, as the first tester's
before = snapshot(game)
case('the original already overwritten', run('install.cmd', game), 1, 'overwritten', game, before=before)
case('and restore there', run('restore.cmd', game), 1, 'nothing to put back', game, before=before)

game = fresh()
(game / 'IFORCE2.dll').unlink()
before = snapshot(game)
case('no IFORCE2.dll', run('install.cmd', game), 1, 'no IFORCE2.dll', game, before=before)
game = fresh(exe=None)
before = snapshot(game)
case('neither exe', run('install.cmd', game), 1, 'no Mech3.exe or Mech3fixup.exe', game, before=before)
game = fresh(exe='Mech3fixup.exe')
case('only Mech3fixup.exe', run('install.cmd', game), 0, 'Installed in', game,
     after={'IFORCE2o.dll': ORIGINAL, 'IFORCE2.dll': OURS, 'Mech3fixup.exe': b'dummy exe'})
case('only Mech3fixup.exe, restore', run('restore.cmd', game), 0, 'Restored in', game, after={'IFORCE2.dll': ORIGINAL})
game = fresh()
(game / 'IFORCE2o.dll').write_bytes(b'foreign')
before = snapshot(game)
case('a foreign IFORCE2o.dll', run('install.cmd', game), 1, 'already', game, before=before)
case('restore with a foreign IFORCE2.dll', run('restore.cmd', game), 1, "isn't this patch's file", game, before=before)
(game / 'IFORCE2.dll').unlink()
before = snapshot(game)
case('restore with IFORCE2.dll missing', run('restore.cmd', game), 1, 'by hand', game, before=before)

game = fresh()
before = snapshot(game)
with open(game / 'IFORCE2.dll', 'rb'):  # held open, as a running game holds it: the rename fails
    case('the DLL in use', run('install.cmd', game), 1, "couldn't rename", game, before=before)

game = fresh('a plain game folder')
case('a trailing backslash', run('install.cmd', str(game) + '\\'), 0, 'Installed in', game, after={'IFORCE2.dll': OURS})
case('a trailing backslash, restore', run('restore.cmd', str(game) + '\\'), 0, 'Restored in', game, after={'IFORCE2.dll': ORIGINAL})

case('no folder given', run('install.cmd'), 1, 'Drop your game folder')
case('no folder given, restore', run('restore.cmd'), 1, 'Drop your game folder')
case('not a folder', run('install.cmd', SCRATCH / 'no such folder'), 1, 'no Mech3.exe')

game = fresh('unzipped into the game')
shutil.copytree(ZIP / 'install-files', game / 'install-files')
for name in ('install.cmd', 'restore.cmd'):
    shutil.copy(ZIP / name, game / name)
case('pasted in the game folder, double-clicked', run('install.cmd', folder=game), 0, f'Installed in "{game}":', after=None)
if (game / 'IFORCE2.dll').read_bytes() != OURS or (game / 'IFORCE2o.dll').read_bytes() != ORIGINAL:
    failures.append('the pasted install did the wrong thing')
case('pasted in the game folder, restore', run('restore.cmd', folder=game), 0, 'Restored in', game, after={'IFORCE2.dll': ORIGINAL})

for exe, code, text in (('Mech3fixup.exe', 0, 'Installed in'), (None, 1, 'Drop your game folder')):
    game = fresh('pasted, exe %s' % exe, exe=exe)
    shutil.copytree(ZIP / 'install-files', game / 'install-files')
    for name in ('install.cmd', 'restore.cmd'):
        shutil.copy(ZIP / name, game / name)
    case(f'pasted, double-clicked, with {exe}', run('install.cmd', folder=game), code, text)
    if code == 0:
        case(f'pasted, restore, with {exe}', run('restore.cmd', folder=game), 0, 'Restored in', game, after={'IFORCE2.dll': ORIGINAL})
    else:
        case(f'pasted, restore, with {exe}', run('restore.cmd', folder=game), 1, 'Drop your game folder')

alone = SCRATCH / 'scripts without their files'
shutil.rmtree(alone, ignore_errors=True)
alone.mkdir()
shutil.copy(ZIP / 'install.cmd', alone / 'install.cmd')
shutil.copy(ZIP / 'restore.cmd', alone / 'restore.cmd')
case('run from inside the zip, files missing', run('install.cmd', game, folder=alone), 1, 'Unzip the whole zip')
case('and restore', run('restore.cmd', game, folder=alone), 1, 'Unzip the whole zip')

print('FAILED' if failures else 'all good')
sys.exit(1 if failures else 0)

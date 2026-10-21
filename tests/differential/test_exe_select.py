"""original.py's exe choice (CLAUDE.md, Pirate's Moon): MW3 1.2 by default,
Pirate's Moon by name, by path or through MW3_EXE, anything else refused."""
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import original  # noqa: E402

failed = 0


def check(cond, what):
    global failed
    if not cond:
        failed += 1
        print('FAIL', what)


env_choice = os.environ.get('MW3_EXE')
check(original.select()[0] == (env_choice and original.select(env_choice)[0] or 'mw3'), 'the default')
check(original.select('mw3')[1] == original.EXES['mw3'][0], 'mw3 by name')
check(original.select(str(original.EXES['mw3'][0]))[0] == 'mw3', 'mw3 by path')
have_pm = original.EXES['pm'][0].exists()
if have_pm:
    check(original.select('pm')[0] == 'pm', 'pm by name')
    check(original.select(str(original.EXES['pm'][0]))[0] == 'pm', 'pm by path')
    check(original.Original(exe='pm').game == 'pm', 'an emulator on pm')
    out = subprocess.run([sys.executable, '-c', 'import original; print(original.GAME, original.Original().game)'],
                         cwd=Path(__file__).parent, env=dict(os.environ, MW3_EXE='pm'),
                         capture_output=True, text=True).stdout.split()
    check(out == ['pm', 'pm'], 'MW3_EXE=pm')
try:
    original.select(str(original.ROOT / 'game' / 'mw3-1.2' / 'Mech3Msg.dll'))
    check(False, 'another file refused')
except SystemExit:
    pass
check(original.Original().game == original.GAME, 'the default emulator')

print('ok: exe choice' + ('' if have_pm else ' (no game/pm: Pirate\'s Moon not checked)') if not failed
      else f'{failed} failed')
sys.stdout.flush()
os._exit(1 if failed else 0)

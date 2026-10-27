"""An operation's end and the campaign's in the program (docs/campaign.md
section 1): build/mw3 with --nosound --hidden --verbose on scratch write
dirs.

    .venv/bin/python tools/analysis/opcheck.py [--keep DIR]

1. `--briefing c1m4`, LAUNCH, `--win 60`, the debriefing's checkmark: the
   Win State saves Op 2 Mission 1's game and the Load state briefs it (the
   new operation's field bases at 3).
2. `--briefing c4bm6` (Op 4 Mission 6, the campaign's last) the same way:
   no next game, and the checkmark goes through the Win Movie State (no
   movie without zFMV) to the main menu, which opens the credits.

The headless test (`campaign_operation_end_and_campaign_end_headless`)
checks the same with the field bases lowered first, and the movies asked
for.
"""
import argparse
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from savecheck import CHECKMARK, LAUNCH, run  # noqa: E402

SALVAGE_OK = '246,290@1530'  # LAUNCH's "salvage left" question, when it asks


def states(out):
    return [l[len('mw3: state '):] for l in out.splitlines() if l.startswith('mw3: state ')]


def checks(root):
    fails = []

    def check(ok, what):
        print(('ok   ' if ok else 'FAIL ') + what)
        if not ok:
            fails.append(what)

    for name, mission in (('op', 'c1m4'), ('campaign', 'c4bm6')):
        d = root / name
        shutil.rmtree(d, ignore_errors=True)
        out, briefs, saves = run(d, [['--briefing', mission], LAUNCH.format(1500), SALVAGE_OK, ['--win', '60'],
                                     CHECKMARK.format(5000)], 9000)
        seq = states(out)
        after = seq[seq.index('Win State') + 1:] if 'Win State' in seq else []
        if name == 'op':
            check(saves and saves[0].startswith('Op 2 Mission 1'), f'c1m4 won saves Op 2 Mission 1: {saves[:1]}')
            check(after[:1] == ['Load State'] and len(briefs) == 2 and briefs[1].startswith('Op 2 Mission 1'),
                  f'then c2m1\'s briefing: {after[:2]}')
            check(len(briefs) == 2 and '; field bases 3;' in briefs[1], 'the new operation\'s field bases at 3')
        else:
            check(saves and saves[0].startswith('Op 4 Mission 6'), f'c4bm6 won makes no next game: {saves[:1]}')
            check(after[:1] == ['Credits State'] and 'Load State' not in after,
                  f'then the Win Movie State, the main menu and its credits: {after[:2]}')
    print(f'{len(fails)} failed')
    return fails


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--keep', type=Path)
    a = ap.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        root = a.keep or Path(tmp)
        root.mkdir(parents=True, exist_ok=True)
        fails = checks(root)
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()

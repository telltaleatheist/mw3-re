#!/usr/bin/env python3
"""Runs the emulator tests one at a time at a precision and sorts how each ended
(docs/testing-precision.md). Niced, capped, and it can be stopped and started:
a test with a result in OUT is not run again.

    tests/differential/sweep_precision.py OUT [--secs 1800] [--mem-gb 6] [--only a,b] [--every N]
"""
import os
import re
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).parent
# Not the exe's arithmetic against ours, or not runnable headless (they start the game or a build).
SKIP = {'test_precision_setting': 'tests the setting itself', 'test_exe_select': 'starts programs',
        'test_lab_import': 'starts programs'}


def emulator_tests():
    for f in sorted(HERE.glob('test_*.py')):
        if re.search(r'\bOriginal\b|from original|import original|unicorn', f.read_text(errors='replace')):
            yield f


def arg(name, default):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    secs, mem = arg('--secs', '1800'), arg('--mem-gb', '6')
    only = arg('--only', '')
    tests = [f for f in emulator_tests() if not only or f.stem in only.split(',')]
    tests = tests[::int(arg('--every', '1'))]
    table = out / 'results.tsv'
    done = {line.split('\t')[0] for line in table.read_text().splitlines()} if table.exists() else set()
    for f in tests:
        if f.stem in done:
            continue
        log = out / (f.stem + '.out')
        if f.stem in SKIP:
            status, took, last = 'NOT RUN', 0, SKIP[f.stem]
        else:
            t0 = time.time()
            env = dict(os.environ, MW3_PRECISION='53')
            with open(log, 'wb') as o:
                r = subprocess.run(['nice', '-n', '10', sys.executable, str(HERE / 'run_capped.py'), '--secs', secs,
                                    '--mem-gb', mem, '--', sys.executable, str(f)],
                                   env=env, stdout=o, stderr=subprocess.STDOUT, cwd=HERE.parents[1])
            took = time.time() - t0
            text = log.read_text(errors='replace')
            lines = [x for x in text.splitlines() if x and not x.startswith(('run_capped', 'precision:'))]
            last = lines[-1][:150] if lines else ''
            good = 'port fpu53 1' in text and "emulator ['0x200']" in text
            code = re.findall(r'run_capped: exit (\d+)', text)
            code = int(code[-1]) if code else r.returncode
            if code == 124:
                status = 'TIMEOUT'
            elif code == 125:
                status = 'OVER MEMORY'
            elif 'NOT at 53 bits' in text or (code == 0 and not good):
                status = 'NOT AT 53'
            elif code == 0:
                status = 'PASS'
            elif good:  # 139: a test that stopped on a difference can crash in its teardown
                status = 'DIFFERS' if code == 1 else f'DIFFERS, then crash {code}'
            else:
                status = f'CRASH {code}'
        with open(table, 'a') as t:
            t.write(f'{f.stem}\t{status}\t{took:.0f}\t{last}\n')
        print(f'{f.stem}: {status} {took:.0f} s', flush=True)


if __name__ == '__main__':
    main()

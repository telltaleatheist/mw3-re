#!/usr/bin/env python3
"""Run a queue of breaks (deliberate mistakes in the port) against the differential tests, to see which the
tests catch. Each worker owns a git worktree with a configured `build/` (Ninja, the `pmprobe` target by default).

    python3 tools/analysis/breakrun.py QUEUE.json OUT.jsonl WORKTREE [WORKTREE...] [--python PY] [--tmp DIR]
                                       [--target pmprobe]

QUEUE.json is a list of {"file", "fn", "old", "new", "tests": [names], "secs", "args"}: in `file` the text `old`
becomes `new`, the target is rebuilt, and each of `tests` (tests/differential/<name>.py, under run_capped.py
with `secs`) runs until one fails. The file is put back after each break. Results go one line each to OUT.jsonl;
finished breaks are skipped on a restart.

A break counts as caught only when a test that passes unbroken fails with it in place:
- each worktree first runs every test of the queue unbroken (the baseline); a test whose baseline fails
  marks its breaks "invalid" without running them;
- a failure whose output shows a death at start-up (dyld, a library or module not found, a Python traceback,
  the file not opened) is "invalid", not caught;
- tests run with exit_on_fault (tests/differential/exit_on_fault.c, built into --tmp when missing), so a
  crash is a plain failure; the run stops if the shim cannot be made.
Found 2026-11-09: a missing shim killed every test at start-up and an older runner counted all 409 breaks as
caught. Results: caught, MISSED, nobuild, NOFIND, TIMEOUT, invalid (with why).
"""
import argparse
import json
import os
import queue
import subprocess
import sys
import tempfile
import threading
import time

DEATHS = ('dyld[', 'Library not loaded', 'image not found', 'Traceback (most recent call last)',
          'ModuleNotFoundError', 'ImportError', "can't open file", 'No such file or directory: ')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('queue')
    ap.add_argument('out')
    ap.add_argument('worktrees', nargs='+')
    ap.add_argument('--python', default=sys.executable, help='the Python with unicorn for the tests')
    ap.add_argument('--tmp', default=tempfile.gettempdir(), help="the tests' TMPDIR and the shim's home")
    ap.add_argument('--target', default='pmprobe')
    a = ap.parse_args()

    shim = os.path.join(a.tmp, 'exit_on_fault.dylib')
    if not os.path.exists(shim):
        src = os.path.join(a.worktrees[0], 'tests/differential/exit_on_fault.c')
        if subprocess.run(['cc', '-dynamiclib', '-o', shim, src]).returncode or not os.path.exists(shim):
            sys.exit('no exit_on_fault shim at ' + shim)

    breaks = json.load(open(a.queue))
    done = set()
    if os.path.exists(a.out):
        for line in open(a.out):
            d = json.loads(line)
            done.add((d['file'], d['old'], d['new']))
    q = queue.Queue()
    for d in breaks:
        if (d['file'], d['old'], d['new']) not in done:
            q.put(d)
    print(len(breaks), 'queued', q.qsize(), 'to run', flush=True)

    env = dict(os.environ, SDL_AUDIODRIVER='dummy', TMPDIR=a.tmp, DYLD_INSERT_LIBRARIES=shim)
    env.pop('MW3_X87', None)
    lock = threading.Lock()
    baseline = {}

    def build(w):
        return subprocess.run(['ninja', '-C', 'build', a.target], cwd=w, capture_output=True, text=True)

    def run_test(w, t, d):
        o = subprocess.run(['python3', 'tests/differential/run_capped.py', '--secs', str(d.get('secs', 400)), '--',
                            a.python, '-P', f'tests/differential/{t}.py'] + d.get('args', []),
                           cwd=w, env=env, capture_output=True, text=True)
        text = o.stdout + o.stderr
        return o.returncode, text, [x[:300] for x in text.strip().splitlines()[-3:]]

    def base_fails(w, t, d):
        k = (w, t, json.dumps(d.get('args', [])))
        if k not in baseline:
            if build(w).returncode:
                baseline[k] = 'baseline nobuild'
            else:
                rc, _, tail = run_test(w, t, d)
                baseline[k] = None if rc == 0 else f'baseline fails rc {rc}: ' + ' '.join(tail)[:200]
            print(time.strftime('%H:%M'), 'baseline', os.path.basename(w), t, baseline[k] or 'ok', flush=True)
        return baseline[k]

    def worker(w):
        while True:
            try:
                d = q.get_nowait()
            except queue.Empty:
                return
            p = os.path.join(w, d['file'])
            s = open(p).read()
            rec = dict(d)
            rec.pop('tail', None)
            rec['was'] = d.get('result')
            rec['ran'] = []
            bad = next((why for t in d['tests'] for why in [base_fails(w, t, d)] if why), None)
            if bad:
                rec['result'], rec['why'] = 'invalid', bad
            elif d['old'] not in s:
                rec['result'] = 'NOFIND'
            else:
                open(p, 'w').write(s.replace(d['old'], d['new'], 1))
                try:
                    b = build(w)
                    if b.returncode:
                        rec['result'] = 'nobuild'
                        rec['tail'] = [x for x in b.stdout.splitlines() if 'error:' in x][:2]
                    else:
                        rec['result'] = 'MISSED'
                        for t in d['tests']:
                            rc, text, tail = run_test(w, t, d)
                            rec['ran'].append(t)
                            rec['tail'] = tail
                            if rc:
                                why = next((m for m in DEATHS if m in text), None)
                                if rc == 124:
                                    rec['result'] = 'TIMEOUT'
                                elif why:
                                    rec['result'], rec['why'] = 'invalid', 'died at start-up: ' + why
                                else:
                                    rec['result'] = 'caught'
                                rec['by'], rec['rc'] = t, rc
                                break
                finally:
                    open(p, 'w').write(s)
            with lock:
                open(a.out, 'a').write(json.dumps(rec) + '\n')
                print(time.strftime('%H:%M'), rec['result'], d['file'], d['fn'], rec.get('by', ''), rec.get('why', ''),
                      flush=True)

    threads = [threading.Thread(target=worker, args=(w,)) for w in a.worktrees]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    for w in a.worktrees:
        build(w)
    print('finished', flush=True)


if __name__ == '__main__':
    main()

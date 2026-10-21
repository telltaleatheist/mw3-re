#!/usr/bin/env python3
"""Run a test with a memory cap and a wall-clock limit.

    python3 tests/differential/run_capped.py [--mem-gb 4] [--secs 1800] [--log FILE] -- cmd args...

macOS does not enforce `ulimit -v`, so this polls the child's resident size
(ps) twice a second and kills its process group past either limit. With
--log, the size is written every 10 s, to see where a run grows. Exit code:
the child's, or 124 on timeout, 125 over the memory cap.
"""
import os
import signal
import subprocess
import sys
import time


def rss_kb(pid):
    out = subprocess.run(['ps', '-o', 'rss=', '-g', str(os.getpgid(pid))], capture_output=True, text=True).stdout
    return sum(int(x) for x in out.split() if x.isdigit())


def main():
    argv = sys.argv[1:]
    mem_gb, secs, log = 4.0, 1800.0, None
    while argv and argv[0] != '--':
        flag, val = argv[0], argv[1]
        argv = argv[2:]
        if flag == '--mem-gb':
            mem_gb = float(val)
        elif flag == '--secs':
            secs = float(val)
        elif flag == '--log':
            log = open(val, 'a')
    cmd = argv[1:]
    child = subprocess.Popen(cmd, start_new_session=True)
    t0 = last_log = time.time()
    peak = 0
    code = None
    while code is None:
        time.sleep(0.5)
        code = child.poll()
        if code is not None:
            break
        try:
            kb = rss_kb(child.pid)
        except (ProcessLookupError, OSError):
            continue
        peak = max(peak, kb)
        now = time.time()
        if log and now - last_log >= 10:
            print(f'{now - t0:7.0f}s rss {kb / 1048576:.2f} GB', file=log, flush=True)
            last_log = now
        why = 125 if kb > mem_gb * 1048576 else 124 if now - t0 > secs else None
        if why:
            print(f'run_capped: killed ({"memory" if why == 125 else "time"}: {kb / 1048576:.2f} GB, '
                  f'{now - t0:.0f} s)', file=sys.stderr, flush=True)
            os.killpg(child.pid, signal.SIGKILL)
            child.wait()
            code = why
    print(f'run_capped: exit {code}, {time.time() - t0:.0f} s, peak {peak / 1048576:.2f} GB', file=sys.stderr)
    return code


if __name__ == '__main__':
    sys.exit(main())

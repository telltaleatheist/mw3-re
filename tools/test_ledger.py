#!/usr/bin/env python3
"""The test ledger: a test that passed is not run again on the same code.

    tools/test_ledger.py run <test binary> [--tests a,b,...] [--programs bin,...] [--data path,...] [--force]
                             [--mem-gb N] [--secs N]
    tools/test_ledger.py status <test binary> [--tests a,b,...] [--programs bin,...] [--data path,...]
    tools/test_ledger.py selftest

It builds nothing. The code a test ran on is its fingerprint: the content of
every source, header and library the test binary (and each program it starts,
--programs) is made from, as the Ninja build lists them (`ninja -t inputs` and
the compilers' dependency lists, `ninja -t deps`), plus the build commands and
the compiler's version. The build folder's generated files and the binaries
themselves are left out (the version define and `git describe` change them every
commit; the define is taken out of the commands too). Paths inside the source
tree are taken relative to it, so every worktree and checkout of the same code
shares a fingerprint. The data the tests read while running is part of it too:
every file under the source tree's assets/ (when there is one) and under each
--data path. The player's install (game/) is not: it does not change.

`run` skips each requested test (all of the binary's, without --tests) whose
last result on this fingerprint is a pass, runs the rest in one process through
MW3_TEST under tests/differential/run_capped.py, and adds one line to the ledger
for each `ok` or `FAIL` the binary prints. A test that failed, or never ran on
this code, runs. --force runs them all and marks its lines "forced".

The ledger is one JSON line a result: test, repo, fingerprint, result, time,
commit, binary, and "forced" when forced. It is $MW3_TEST_LEDGER, else this
machine's shared one (KEEP_LEDGER below) when that folder exists, else
test-ledger.jsonl in the build folder. docs/building.md, The test ledger.
"""
import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

try:
    import fcntl  # the lock between worktrees writing at once; Windows has none
except ImportError:
    fcntl = None

KEEP_LEDGER = '/Volumes/Callisto/claude-files/keep/mw3-rebuild/test-ledger.jsonl'
TOOL_ROOT = Path(__file__).resolve().parent.parent
# The compiler's own headers and the SDK: covered by the compiler's version line.
SYSTEM_PREFIXES = ('/Applications/Xcode.app/', '/Library/Developer/', '/usr/include/', '/usr/lib/',
                   '/System/Library/')
VERSION_DEFINE = re.compile(r'-DMW3_VERSION=(\\"[^ ]*\\"|"[^"]*"|\S+)')
RESULT_LINE = re.compile(r'^(ok|FAIL)\s+(\S+)\s*$')
TEST_MACRO = re.compile(r'^TEST\((\w+)\)', re.M)


def die(msg):
    print('test_ledger: ' + msg, file=sys.stderr)
    sys.exit(2)


def ledger_path(build):
    if os.environ.get('MW3_TEST_LEDGER'):
        return Path(os.environ['MW3_TEST_LEDGER'])
    if Path(KEEP_LEDGER).parent.is_dir():
        return Path(KEEP_LEDGER)
    return build / 'test-ledger.jsonl'


def build_dir_of(binary):
    for d in [binary.parent, *binary.parent.parents]:
        if (d / 'build.ninja').is_file():
            return d
    die(f'no build.ninja above {binary}: the ledger reads Ninja builds')


def source_dir_of(build):
    cache = build / 'CMakeCache.txt'
    if cache.is_file():
        for line in cache.read_text(errors='replace').splitlines():
            if line.startswith('CMAKE_HOME_DIRECTORY:'):
                return Path(line.split('=', 1)[1]).resolve()
    die(f'no CMAKE_HOME_DIRECTORY in {cache}')


def ninja(build, *args):
    r = subprocess.run(['ninja', '-C', str(build), *args], capture_output=True, text=True)
    if r.returncode != 0:
        die(f'ninja {" ".join(args)} failed: {r.stderr.strip() or r.stdout.strip()}')
    return r.stdout


def target_files(build, target):
    """Every file `target` is made from, by absolute path, the build folder's own left out."""
    inputs = [l.strip() for l in ninja(build, '-t', 'inputs', target).splitlines() if l.strip() and l.strip() != '.']
    if not inputs:
        die(f'ninja knows no inputs of {target}')
    objects = [i for i in inputs if i.endswith('.o') or i.endswith('.obj')]
    files = set()
    for i in inputs:
        if i in objects:
            continue
        files.add(os.path.normpath(i if os.path.isabs(i) else str(build / i)))
    for chunk in range(0, len(objects), 400):
        cur = None
        for line in ninja(build, '-t', 'deps', *objects[chunk:chunk + 400]).splitlines():
            if not line.strip():
                continue
            if not line.startswith(' '):
                cur = line
                if '(VALID)' not in line:
                    die(f'{line.split(":")[0]} has stale dependency lists: build {target} first')
                continue
            p = line.strip()
            files.add(os.path.normpath(p if os.path.isabs(p) else str(build / p)))
    b = str(build.resolve()) + os.sep
    return sorted(f for f in files if not f.startswith(b) and not f.startswith(SYSTEM_PREFIXES))


def target_commands(build, target, source, binary_dir):
    lines = []
    for c in ninja(build, '-t', 'commands', target).splitlines():
        c = VERSION_DEFINE.sub('-DMW3_VERSION=<version>', c)
        c = c.replace(str(binary_dir), '<build>').replace(str(source), '<source>')
        lines.append(c)
    return sorted(set(lines))


def compiler_version(build):
    cache = (build / 'CMakeCache.txt').read_text(errors='replace')
    m = re.search(r'^CMAKE_CXX_COMPILER:\w+=(.*)$', cache, re.M)
    if not m:
        return ''
    r = subprocess.run([m.group(1), '--version'], capture_output=True, text=True)
    return r.stdout.splitlines()[0] if r.stdout else ''


def data_files(source, paths):
    """Every file under the data paths (each relative to the source tree, or absolute), dotfiles left out."""
    files = set()
    for p in paths:
        root = Path(p) if os.path.isabs(p) else source / p
        if root.is_file():
            files.add(str(root.resolve()))
        elif root.is_dir():
            for d, dirs, names in os.walk(root):
                dirs[:] = [x for x in dirs if not x.startswith('.')]
                files.update(str(Path(d, n).resolve()) for n in names if not n.startswith('.'))
        else:
            die(f'no data path {root}')
    return sorted(files)


def fingerprint(build, source, targets, data=()):
    """(fingerprint, test names found in the test binary's own sources)."""
    h = hashlib.sha256()
    names = []
    h.update(compiler_version(build).encode() + b'\n')
    for f in data_files(source, data):
        rel = os.path.relpath(f, source) if f.startswith(str(source) + os.sep) else f
        h.update(f'data {rel}\t{hashlib.sha256(Path(f).read_bytes()).hexdigest()}\n'.encode())
    for t in targets:
        h.update(b'target ' + t.encode() + b'\n')
        for f in target_files(build, t):
            rel = os.path.relpath(f, source) if f.startswith(str(source) + os.sep) else f
            try:
                data = Path(f).read_bytes()
            except OSError:
                data = b'<missing>'
            h.update(f'{rel}\t{hashlib.sha256(data).hexdigest()}\n'.encode())
            if t == targets[0] and f.endswith(('.cpp', '.cc', '.c', '.mm')) and b'TEST(' in data:
                names += TEST_MACRO.findall(data.decode('utf-8', 'replace'))
        for c in target_commands(build, t, source, build.resolve()):
            h.update(c.encode() + b'\n')
    return h.hexdigest()[:20], sorted(set(names))


def git(source, *args):
    r = subprocess.run(['git', '-C', str(source), *args], capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else ''


def repo_name(source, given):
    """--repo, else the remote whose main HEAD's code is fewest commits past (mech3-re, mech3-pm-re, ...)."""
    if given:
        return given
    best = None
    for ref in git(source, 'for-each-ref', '--format=%(refname:short)', 'refs/remotes/*/main').split():
        base = git(source, 'merge-base', ref, 'HEAD')
        if not base:
            continue
        n = int(git(source, 'rev-list', '--count', f'{base}..HEAD') or 1 << 30)
        if best is None or n < best[0]:
            best = (n, ref.split('/')[0])
    return best[1] if best else source.name


def read_ledger(path):
    out = []
    if path.is_file():
        with open(path, encoding='utf-8') as f:
            for line in f:
                try:
                    out.append(json.loads(line))
                except ValueError:
                    pass
    return out


def append_ledger(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, 'a', encoding='utf-8') as f:
        if fcntl:
            fcntl.flock(f, fcntl.LOCK_EX)
        for r in rows:
            f.write(json.dumps(r, sort_keys=True) + '\n')
        f.flush()
        if fcntl:
            fcntl.flock(f, fcntl.LOCK_UN)


def last_results(rows, fp):
    """test -> its last row on this fingerprint."""
    last = {}
    for r in rows:
        if r.get('fingerprint') == fp:
            last[r['test']] = r
    return last


def prepare(args):
    binary = Path(args.binary).resolve()
    if not binary.is_file():
        die(f'no test binary {binary}')
    build = build_dir_of(binary)
    source = source_dir_of(build)
    programs = [Path(p).name for p in (args.programs.split(',') if args.programs else []) if p]
    targets = [binary.name] + [p for p in programs if p != binary.name]
    data = [d for d in (args.data.split(',') if args.data else []) if d]
    if (source / 'assets').is_dir() and 'assets' not in data:
        data.insert(0, 'assets')  # always: the launcher's and the game's tests read it
    fp, names = fingerprint(build, source, targets, data)
    # Only the tests built in: a TEST() inside an #if the build leaves out (MW3_PM_SUITE) has no name string in it.
    if names:
        built = set(re.findall(rb'[A-Za-z0-9_]{2,}(?=\x00)', binary.read_bytes()))
        names = [n for n in names if n.encode() in built]
    requested = [t for t in (args.tests.split(',') if args.tests else names) if t]
    unknown = [t for t in requested if t not in names]
    if unknown:
        die(f'not a test of {binary.name}: {", ".join(unknown)} (the whole name, as TEST() gives it)')
    return binary, build, source, targets, fp, names, requested


def cmd_status(args):
    binary, build, source, targets, fp, names, requested = prepare(args)
    last = last_results(read_ledger(ledger_path(build)), fp)
    print(f'{binary.name} on fingerprint {fp} ({", ".join(targets)}):')
    for t in requested:
        r = last.get(t)
        if r and r['result'] == 'ok':
            print(f'  {t}: passed on this code {r["time"]}')
        elif r:
            print(f'  {t}: FAILED on this code {r["time"]}')
        else:
            print(f'  {t}: not run on this code')
    return 0


def cmd_run(args):
    binary, build, source, targets, fp, names, requested = prepare(args)
    path = ledger_path(build)
    last = last_results(read_ledger(path), fp)
    to_run = []
    for t in requested:
        r = last.get(t)
        if r and r['result'] == 'ok' and not args.force:
            print(f'{t}: passed on this code {r["time"]}, not run')
        else:
            to_run.append(t)
    if not to_run:
        print(f'test_ledger: all {len(requested)} passed on this code (fingerprint {fp}); nothing run')
        return 0
    env = dict(os.environ)
    env.pop('MW3_TEST', None)
    env.pop('MW3_SKIP', None)
    if set(to_run) != set(names):
        env['MW3_TEST'] = ','.join(to_run)
        # MW3_TEST matches by substring: the binary's other tests holding a requested name are skipped, unless
        # one of them is itself inside a requested name (MW3_SKIP is by substring too).
        extra = [n for n in names if n not in to_run and any(t in n for t in to_run)]
        skip = [n for n in extra if not any(n in t for t in to_run)]
        if skip:
            env['MW3_SKIP'] = ','.join(skip)
    if args.force:
        print(f'test_ledger: --force: {len(to_run)} run whatever the ledger says; logged as forced')
    capped = source / 'tests' / 'differential' / 'run_capped.py'
    if not capped.is_file():
        capped = TOOL_ROOT / 'tests' / 'differential' / 'run_capped.py'
    cmd = [sys.executable, str(capped), '--mem-gb', str(args.mem_gb), '--secs', str(args.secs), '--', str(binary)]
    proc = subprocess.Popen(cmd, env=env, cwd=str(source), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors='replace', bufsize=1)
    seen = {}
    for line in proc.stdout:
        sys.stdout.write(line)
        m = RESULT_LINE.match(line.rstrip('\n'))
        if m:
            seen[m.group(2)] = m.group(1)
    code = proc.wait()
    now = datetime.datetime.now().astimezone().isoformat(timespec='seconds')
    commit = git(source, 'rev-parse', '--short=12', 'HEAD')
    repo = repo_name(source, args.repo)
    rows = []
    for t, result in seen.items():
        row = {'test': t, 'repo': repo, 'fingerprint': fp, 'result': result, 'time': now, 'commit': commit,
               'binary': binary.name}
        if args.force:
            row['forced'] = True
        rows.append(row)
    append_ledger(path, rows)
    missing = [t for t in to_run if t not in seen]
    failed = [t for t, r in seen.items() if r != 'ok']
    print(f'test_ledger: {len(seen)} recorded in {path} (fingerprint {fp}); {len(failed)} failed'
          + (f'; not reported (left out, or the run stopped): {", ".join(missing)}' if missing else '')
          + (f'; run_capped exit {code}' if code else ''))
    return 1 if failed or missing or code else 0


# The self-test: a small Ninja project with two tests on the runner's MW3_TEST rules.
SELF_MAIN = r'''#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "value.h"
static bool any_in(const char* list, const char* name) {
  for (const char* p = list; *p;) {
    const char* e = std::strchr(p, ',');
    char part[128] = {};
    std::strncpy(part, p, e ? size_t(e - p) : std::strlen(p));
    if (*part && std::strstr(name, part)) return true;
    if (!e) break;
    p = e + 1;
  }
  return false;
}
static bool wanted(const char* n) {
  const char* only = std::getenv("MW3_TEST");
  const char* skip = std::getenv("MW3_SKIP");
  if (skip && any_in(skip, n)) return false;
  return !only || any_in(only, n);
}
#define TEST(name) static bool name()
TEST(alpha) { return kValue == 1; }
TEST(alpha_long) { return true; }
TEST(beta) { return true; }
int main() {
  struct { const char* n; bool (*f)(); } all[] = {{"alpha", alpha}, {"alpha_long", alpha_long}, {"beta", beta}};
  for (auto& t : all)
    if (wanted(t.n)) std::printf("%s %s\n", t.f() ? "ok  " : "FAIL", t.n);
  return 0;
}
'''


def selftest():
    tmp = Path(tempfile.mkdtemp(prefix='test-ledger-selftest-'))
    try:
        src, build, ledger = tmp / 'src', tmp / 'src' / 'build', tmp / 'ledger.jsonl'
        src.mkdir()
        (src / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(ledgerself CXX)\n'
                                            'add_executable(selftests main.cpp)\n'
                                            'target_compile_definitions(selftests PRIVATE MW3_VERSION="v0-1-gabc")\n')
        (src / 'main.cpp').write_text(SELF_MAIN)
        (src / 'value.h').write_text('constexpr int kValue = 1;  // a\n')
        (src / 'assets' / 'ui').mkdir(parents=True)
        (src / 'assets' / 'ui' / 'menu.rml').write_text('<rml>a</rml>\n')
        (src / 'data').mkdir()
        (src / 'data' / 'table.json').write_text('{"a": 1}\n')
        quiet = dict(capture_output=True, text=True)
        if subprocess.run(['cmake', '-S', str(src), '-B', str(build), '-G', 'Ninja'], **quiet).returncode:
            die('selftest: cmake failed')
        bin_ = build / 'selftests'
        env = dict(os.environ, MW3_TEST_LEDGER=str(ledger))
        me = [sys.executable, str(Path(__file__).resolve())]

        def build_it():
            r = subprocess.run(['cmake', '--build', str(build)], **quiet)
            if r.returncode:
                die('selftest: build failed: ' + r.stdout[-400:])

        def run(*extra):
            r = subprocess.run(me + ['run', str(bin_), *extra], env=env, **quiet)
            return r.returncode, r.stdout + r.stderr

        def lines():
            return read_ledger(ledger)

        checks = []

        def check(name, cond, out=''):
            checks.append(cond)
            print(('ok   ' if cond else 'FAIL ') + name)
            if not cond and out:
                print(out)

        build_it()
        code, out = run()
        check('a first run passes and records all three', code == 0 and
              sorted(r['test'] for r in lines()) == ['alpha', 'alpha_long', 'beta'] and
              all(r['result'] == 'ok' for r in lines()), out)
        code, out = run()
        check('a second run skips all three', code == 0 and len(lines()) == 3 and
              out.count(', not run\n') == 3 and 'nothing run' in out, out)
        fp1 = lines()[0]['fingerprint']
        (src / 'CMakeLists.txt').write_text((src / 'CMakeLists.txt').read_text().replace('v0-1-gabc', 'v0-2-gdef'))
        build_it()
        code, out = run()
        check('a new version define alone runs nothing', code == 0 and len(lines()) == 3, out)
        (src / 'value.h').write_text('constexpr int kValue = 1;  // b\n')
        build_it()
        code, out = run()
        check('a one-character header change reruns all three', code == 0 and len(lines()) == 6 and
              lines()[-1]['fingerprint'] != fp1, out)
        code, out = run('--tests', 'alpha')
        check('--tests alpha after that: passed, not run', code == 0 and len(lines()) == 6 and
              'alpha: passed on this code' in out, out)
        (src / 'value.h').write_text('constexpr int kValue = 2;  // b\n')
        build_it()
        code, out = run('--tests', 'alpha')
        new = lines()[6:]
        check('--tests alpha on new code runs alpha alone (alpha_long skipped), its FAIL recorded',
              code == 1 and [(r['test'], r['result']) for r in new] == [('alpha', 'FAIL')], out)
        code, out = run('--tests', 'alpha')
        check('a failed test runs again', code == 1 and len(lines()) == 8, out)
        (src / 'value.h').write_text('constexpr int kValue = 1;  // b\n')
        build_it()
        code, out = run('--tests', 'beta', '--force')
        check('--force runs a passed test and marks it forced', code == 0 and len(lines()) == 9 and
              lines()[-1].get('forced') is True, out)
        n = len(lines())
        (src / 'assets' / 'ui' / 'menu.rml').write_text('<rml>b</rml>\n')
        code, out = run()
        check('a one-character change in assets/ reruns all three (assets/ is always in)', code == 0 and
              len(lines()) == n + 3, out)
        code, out = run('--data', 'data')
        check('--data adds a folder: all three run on the new fingerprint', code == 0 and len(lines()) == n + 6, out)
        code, out = run('--data', 'data')
        check('the same --data again runs nothing', code == 0 and len(lines()) == n + 6, out)
        (src / 'data' / 'table.json').write_text('{"a": 2}\n')
        code, out = run('--data', 'data')
        check('a change in a --data file reruns them', code == 0 and len(lines()) == n + 9, out)
        r = subprocess.run(me + ['status', str(bin_)], env=env, **quiet)
        check('status names what passed on this code', 'alpha: passed on this code' in r.stdout and
              'beta: passed on this code' in r.stdout, r.stdout + r.stderr)
        r = subprocess.run(me + ['run', str(bin_), '--tests', 'gamma'], env=env, **quiet)
        check('an unknown test is refused', r.returncode == 2 and 'not a test' in r.stderr, r.stderr)
        print(f'selftest: {sum(checks)} of {len(checks)} passed')
        return 0 if all(checks) else 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    for name in ('run', 'status'):
        p = sub.add_parser(name)
        p.add_argument('binary')
        p.add_argument('--tests', help='whole test names, comma-separated (default: all of the binary\'s)')
        p.add_argument('--programs', help='programs the tests start, by path or target name, comma-separated')
        p.add_argument('--data', help='files or folders the tests read, comma-separated (assets/ is always in)')
        p.add_argument('--repo', help='the repo name for the ledger (default: the remote main HEAD is nearest)')
        if name == 'run':
            p.add_argument('--force', action='store_true', help='run passed tests too (logged as forced)')
            p.add_argument('--mem-gb', type=float, default=8)
            p.add_argument('--secs', type=float, default=1800)
    sub.add_parser('selftest')
    args = ap.parse_args()
    if args.cmd == 'selftest':
        return selftest()
    return cmd_run(args) if args.cmd == 'run' else cmd_status(args)


if __name__ == '__main__':
    sys.exit(main())

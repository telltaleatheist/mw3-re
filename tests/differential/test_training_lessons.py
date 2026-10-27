"""Training's lessons against the original x86 (docs/objectives.md, Training's
lessons): the training game's vt+0x40 0x00437140 (a block as one lesson, or
its `_set` lists, or the numbered blocks), the list's add 0x0045ed40, select
0x0045efa0, get 0x0045efd0, and the step 0x004373b0 / 0x0045f080.

    cmake --build build && .venv/bin/python tests/differential/test_training_lessons.py

t1's objectives.zrd for m1..m4 (and m0, m5: none), then made-up files: a
block read straight, `_set` lists (some entries not lists, some that read
nothing), numbered blocks with gaps, both. Each run steps past the last
lesson. Both sides read through test_objectives.py's fake engine; the
lessons' objects, where the mission's objectives point after each step and
the log of calls must match.
"""
import ctypes
import os
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_objectives as T  # noqa: E402
from test_objectives import CAP, WORK, archive, h32, probe, ser, condition  # noqa: E402

probe.probe_training_run.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int,
                                     ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int]
LOAD, STEP = 0x437140, 0x4373b0


class Lessons(T.Run):
    def run_lessons(self, files, block, steps):
        cpu = self.cpu
        self.next = self.heap0
        self.lines, self.entities, self.addr_of, self.type = [], {}, {}, 4
        self.trees = {k.lower(): [self.build(v)] for k, v in files.items()}
        cpu.u32(T.PILOT, self.pilot)
        cpu.u32(T.NAVS, 0x1000)
        game = self.calloc(0x8c)
        cpu.u32(game + 0x44 + 0x44, 0xffffffff)
        out = self.calloc(0x14)
        fname, bname = self.new_string(b'objectives.zrd'), self.new_string(block.encode())
        cpu.call(LOAD, ecx=game, stack_args=[fname, bname] + [out + 4 * i for i in range(5)])
        ok = self.eax() & 0xff
        seq = game + 0x44
        first, last = self.u32(seq + 0x38), self.u32(seq + 0x3c)
        lessons = [self.u32(first + 4 * i) for i in range((last - first) // 4)] if first else []
        text = f'ok {h32(ok)} lessons {h32(len(lessons))}\n'
        for i, l in enumerate(lessons):
            text += f'lesson {h32(i)} desc {self.opt(self.u32(l))}\n'
            for k in range(4):
                g = self.u32(l + 4 + 4 * k)
                text += self.obj(g, 1) if g else '  -\n'

        def mission(tag):
            o = f'{tag} cur {h32(self.u32(seq + 0x44))} desc {self.opt(self.u32(out))}'
            for k in range(4):
                g = self.u32(out + 4 + 4 * k)
                at = next((i for i, l in enumerate(lessons) for j in range(4) if g and self.u32(l + 4 + 4 * j) == g), -1)
                o += ' ' + ((f'L{h32(at)}' if at >= 0 else 'own') if g else '-')
            return o + '\n'
        text += mission('start')
        for _ in range(steps):
            cpu.call(STEP, ecx=game, stack_args=[out + 4 * i for i in range(5)])
            text += f'next {h32(self.u32(seq + 0x44))}\n' + mission('after')
        return text, '\n'.join(self.lines) + ('\n' if self.lines else '')


def port(files, block, steps):
    WORK.mkdir(parents=True, exist_ok=True)
    path = WORK / 'training.zbd'
    path.write_bytes(archive(files))
    out, log = ctypes.create_string_buffer(CAP), ctypes.create_string_buffer(CAP)
    r = probe.probe_training_run(str(path).encode(), b'objectives.zrd', block.encode(), steps, out, CAP, log, CAP)
    assert r == 0, r
    return out.value.decode('latin-1'), log.value.decode('latin-1')


def lesson(r):
    b = []
    if r.random() < 0.6:
        b += ['desc', [f'LDESC{r.randint(0, 5)}']]
    for key in ('primary', 'secondary', 'final', 'fail'):
        if r.random() < 0.7:
            g = []
            for _ in range(r.randint(0, 2)):
                g += condition(r)
            b += [key, g]
    return b


def made_up(r):
    top = []
    base = r.choice(['m1', 'm2'])
    if r.random() < 0.25:
        top += [base, lesson(r)]
    if r.random() < 0.4:
        entries = []
        for _ in range(r.randint(0, 4)):
            entries.append(lesson(r) if r.random() < 0.85 else r.choice(['x', 3]))
        top += [base + '_set', entries]
    n = r.randint(0, 5)
    for i in range(1, n + 1):
        if r.random() < 0.1:
            continue  # a gap: the numbering stops there
        top += [f'{base}{i}', lesson(r)]
    return ser([top]), base


def main():
    emu = Lessons()
    fails = runs = 0
    for mission, body, blocks in T.shipped():
        if mission != 't1':
            continue
        for b in ('m0', 'm1', 'm2', 'm3', 'm4', 'm5'):
            files = {'objectives.zrd': body}
            exe, prt = emu.run_lessons(files, b, 30), port(files, b, 30)
            runs += 1
            if exe != prt:
                fails += 1
                T_report(f't1 {b}', exe, prt)
    r = random.Random(int(os.environ.get('SEED', '1')))
    for i in range(int(os.environ.get('N', '300'))):
        data, base = made_up(r)
        block = base if r.random() < 0.9 else 'm9'
        files = {'objectives.zrd': data}
        steps = r.randint(0, 8)
        exe, prt = emu.run_lessons(files, block, steps), port(files, block, steps)
        runs += 1
        if exe != prt:
            fails += 1
            T_report(f'made-up {i}', exe, prt)
        if fails > 5:
            break
    print('FAIL' if fails else 'PASS', f'({runs} runs, {fails} mismatches)')
    return 1 if fails else 0


def T_report(tag, a, b):
    for name, x, y in (('objects', a[0], b[0]), ('log', a[1], b[1])):
        xl, yl = x.splitlines(), y.splitlines()
        for i in range(max(len(xl), len(yl))):
            p, q = (xl[i] if i < len(xl) else '<end>'), (yl[i] if i < len(yl) else '<end>')
            if p != q:
                print(f'MISMATCH {tag}: {name} line {i}:\n    exe:  {p}\n    port: {q}')
                break


if __name__ == '__main__':
    code = main()
    sys.stdout.flush()
    os._exit(code)

"""PM's repair state against the exe (pm-tests 35): the approach 0x4e03c0, the run 0x4e0790 frame by frame and the
end 0x4e0570, the steering updated before each frame as the tick does (0x4dcb30), on test_pm_ai_walk.py's world (the brain's PM steering at +0x210 real, the boundary and the controls
stubbed alike). Each run: a dock point and a place, then frames with the world moving the pilot along the steering
and turning the boundary, a via, the dock's flags, the power and the pilot's own-fights switch on and off, now and
then the state set to a phase outright. After each frame every field of the state, the controls sent, the power
calls, the switch, the brain's +0x2c4, the pops, the vias queued, the brain's +0x2b8 and the steering compared.

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_repair_state.py [runs]

Breaks (in the port, by hand): the last close-in at 6 m, the down time at 20 s, the re-aim's kept `along` dropped.
"""
import ctypes
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_ai_walk as walk  # noqa: E402  (sets the x87 to 53 bits)
import test_pm_zar  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402

CTOR, RUN, END, UPDATE = 0x4e03c0, 0x4e0790, 0x4e0570, 0x4dcb30
DRIVE, CONTACT, POWER = 0x62fbb2, 0x62fbb1, 0x41dab0


def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


def hx(v):
    return f'{struct.unpack("<I", struct.pack("<f", v))[0]:x}'


class Exe(walk.Exe):
    def __init__(self):
        super().__init__()
        self.want_pilot = True  # vt+0x58: the mech the power goes to
        self.powers, self.power_last, self.pops = 0, -1, 0
        self.down = False
        self.w32(self.g(self.vehicle) + 0xac, self.ret_stub(lambda c: 1 if self.down else 0))

        def power(c):
            self.powers += 1
            self.power_last = self.g(c._uc.reg_read(UC_X86_REG_ESP) + 4)
            return 0
        self.native_stub(POWER, power, pop=4)
        ovt = self.alloc(0x20)
        self.w32(self.order, ovt)
        pop = self.cpu.code(b'\xc3')

        def popped(c):
            self.pops += 1
            return 0
        self.native_stub(pop, popped)
        self.w32(ovt + 0xc, pop)
        for nop in (0x4d0e40, 0x4d3a60):  # the end's base destructors: the behaviour copy and the state
            self.native_stub(nop, lambda c: 0)
        # floor (msvcrt, [0x5de61c]): the x87 rounded down.
        floor = bytes.fromhex('83ec04 d93c24 668b0424 6625fff3 660d0004 6689442402 d96c2402 dd442408 d9fc d92c24 83c404 c3')
        self.w32(0x5de61c, self.cpu.code(floor))
        self.beh = self.alloc(0x10)
        self.dock = self.alloc(12)
        self.rs = None

    def start(self, dock, place):
        self.fs(self.dock, dock)
        self.fs(self.vehicle + 0x80, place)
        self.rs = self.alloc(0x50)
        self.call(CTOR, ecx=self.rs, args=(self.beh, self.order, self.dock))

    def set_phase(self, phase, own, slow, inv, until):
        self.w32(self.rs + 0x24, phase)
        self.cpu._uc.mem_write(self.rs + 0x14, bytes([own]))
        self.fs(self.rs + 0x18, [slow, inv, until])

    def world(self, w):
        self.fs(self.vehicle + 0x80, w['place'])
        self.fs(self.vehicle + 0x24, [w['fw'][0]])
        self.fs(self.vehicle + 0x2c, [w['fw'][1]])
        self.fs(self.goals, [w['goals'][0], w['goals'][1]])
        self.fs(self.brain + 0x10c, [w['most']])
        self.fs(walk.NOW, [w['now']])
        self.fs(self.k, [w['k'][0], w['k'][1]])
        self.blocked, self.on_from, self.steers = w['blocked'], w['on_from'], w['steers']
        self.cpu._uc.mem_write(self.rec + 0x50, bytes([1 if w['via'] else 0]))
        self.cpu._uc.mem_write(DRIVE, bytes([1 if w['drive'] else 0]))
        self.cpu._uc.mem_write(CONTACT, bytes([1 if w['contact'] else 0]))
        self.down = w['down']
        self.w32(self.brain + 0x80, w['hold'])

    def dump(self):
        r, b, st = self.rs, self.brain, self.brain + 0x210
        v = [self.g(r + 0x24), self.cpu._uc.mem_read(r + 0x14, 1)[0]] + [self.g(r + o) for o in (0x18, 0x1c, 0x20)]
        v += [self.g(r + 0x28 + 4 * i) for i in range(3)] + [self.g(r + 0x38 + 4 * i) for i in range(3)]
        v += [self.sent[0], self.sent[1], self.sends, self.powers, self.power_last & 0xffffffff, self.g(b + 0x80),
              self.cpu._uc.mem_read(b + 0x2c4, 1)[0], self.pops, self.g(self.brain + 0x15c + 8)]
        v += [self.g(b + 0x2b8 + 4 * i) for i in range(3)]
        v += [self.g(st + o) for o in (4, 8, 0xc, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x34, 0x38, 0x40)]
        return [x & 0xffffffff for x in v]


def world_line(w):
    b = lambda x: '1' if x else '0'  # noqa: E731
    return ' '.join(['W'] + [hx(x) for x in (*w['place'], *w['fw'], *w['goals'], w['most'], w['now'], *w['k'])] +
                    [b(w[k]) for k in ('blocked', 'on_from', 'via', 'steers', 'drive', 'contact', 'down')] + [str(w['hold'])])


def plan(rng):
    dock = [f32(rng.uniform(-3000, 3000)), f32(rng.uniform(-50, 50)), f32(rng.uniform(-3000, 3000))]
    a, d = rng.uniform(0, 6.283), rng.choice([20.0, 60.0, 150.0, 400.0])
    place = [f32(dock[0] + d * float(__import__('math').sin(a))), dock[1], f32(dock[2] + d * float(__import__('math').cos(a)))]
    steps = [('S', dock, place)]
    w = {'place': place, 'fw': [0.0, 1.0], 'goals': [0.0, 0.0], 'most': f32(rng.choice([0.5, 1.0])), 'now': 100.0,
         'k': [f32(rng.choice([5.0, 12.5])), f32(rng.choice([0.4, 1.5, 3.0, 6.0, 9.0]))], 'blocked': False, 'on_from': False,
         'via': False, 'steers': True, 'drive': rng.random() < 0.7, 'contact': False, 'down': False,
         'hold': rng.choice([0, 1, 1])}
    for i in range(rng.choice([30, 80, 160])):
        w = dict(w)
        w['now'] = f32(w['now'] + 0.05)
        ang = rng.uniform(0, 6.283)
        w['fw'] = [f32(float(__import__('math').sin(ang))), f32(float(__import__('math').cos(ang)))]
        w['goals'] = [f32(rng.uniform(-1, 1)), f32(rng.uniform(-1, 1))]
        r = rng.random()
        if r < 0.5:  # toward the dock, some way of it
            t = rng.choice([0.05, 0.2, 0.6, 0.95, 1.0])
            w['place'] = [f32(w['place'][0] + (dock[0] - w['place'][0]) * t + rng.uniform(-2, 2)), w['place'][1],
                          f32(w['place'][2] + (dock[2] - w['place'][2]) * t + rng.uniform(-2, 2))]
        elif r < 0.6:
            w['place'] = [f32(dock[0] + rng.uniform(-3, 3)), dock[1], f32(dock[2] + rng.uniform(-3, 3))]
        elif r < 0.75:  # round the last close-in's 6.5 m
            b = rng.uniform(0, 6.283)
            rr = rng.uniform(5.5, 7.5)
            w['place'] = [f32(dock[0] + rr * float(__import__('math').sin(b))), dock[1],
                          f32(dock[2] + rr * float(__import__('math').cos(b)))]
        w['blocked'] = rng.random() < 0.15
        w['on_from'] = rng.random() < 0.3
        w['via'] = w['blocked'] and rng.random() < 0.3
        w['steers'] = rng.random() < 0.9
        if rng.random() < 0.1:
            w['drive'] = not w['drive']
        if rng.random() < 0.15:
            w['contact'] = not w['contact']
        if rng.random() < 0.15:
            w['down'] = not w['down']
        if rng.random() < 0.05:
            w['now'] = f32(w['now'] + rng.choice([10.0, 31.0]))
        steps.append(('W', w))
        if rng.random() < 0.06:
            steps.append(('P', rng.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]), rng.randrange(2),
                          f32(rng.uniform(0.5, 40)), f32(rng.uniform(0.02, 2)), f32(w['now'] + rng.uniform(-5, 40))))
        steps.append(('R',))
    if rng.random() < 0.5:
        steps.append(('E',))
    return steps


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    lib = test_pm_zar.load_pm_probe()
    lib.probe_pm_repair.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    rng = random.Random(0x4e0790)
    frames, phases, ends, closed_in = 0, set(), 0, 0
    buf = ctypes.create_string_buffer(1 << 22)
    for run in range(runs):
        steps = plan(rng)
        x = Exe()
        want, text = [], []
        for st in steps:
            if st[0] == 'S':
                x.start(st[1], st[2])
                text.append('S ' + ' '.join(hx(v) for v in (*st[1], *st[2])))
            elif st[0] == 'W':
                x.world(st[1])
                text.append(world_line(st[1]))
            elif st[0] == 'P':
                x.set_phase(*st[1:])
                text.append(f'P {st[1]} {st[2]} {hx(st[3])} {hx(st[4])} {hx(st[5])}')
            elif st[0] == 'R':
                if x.g(x.rs + 0x24) in (0,) or x.pops:  # popped: the order's gone, and with it the state
                    break
                was = x.g(x.rs + 0x24)
                phases.add(was)  # the phase the frame starts in
                x.call(UPDATE, ecx=x.brain + 0x210, args=(struct.unpack('<I', struct.pack('<f', 0.05))[0], x.vehicle))
                x.call(RUN, ecx=x.rs, args=(0,))
                want.append(x.dump())
                text.append('R')
                closed_in += was in (4, 5) and want[-1][0] == 8  # the last close-in begun by itself
            else:
                x.call(END, ecx=x.rs)
                want.append(x.dump())
                text.append('E')
                ends += 1
        n = lib.probe_pm_repair('\n'.join(text).encode(), buf, len(buf))
        assert n >= 0
        got = [[int(v) for v in ln.split()] for ln in buf.value.decode().splitlines()]
        for k, (a, b) in enumerate(zip(want, got)):
            if a != b:
                names = ['phase', 'own', 'slow', 'inv', 'until', 'pulled.x', 'pulled.y', 'pulled.z', 'appr.x', 'appr.y',
                         'appr.z', 'turn', 'throttle', 'sends', 'powers', 'power', 'hold', 'runs', 'pops', 'queued',
                         'from.x', 'from.y', 'from.z', 's.start.x', 's.start.y', 's.start.z', 's.dir.x', 's.dir.z',
                         's.along_min', 's.length', 's.along_at', 's.across', 's.along', 's.side', 's.x40']
                diff = [names[i] for i in range(len(a)) if a[i] != b[i]]
                raise SystemExit(f'run {run} frame {k}: differ in {diff}\n  exe  {a}\n  port {b}')
        if len(want) != len(got):
            raise SystemExit(f'run {run}: {len(want)} frames in the exe, {len(got)} in the port')
        frames += len(want)
    if not {1, 2, 3, 4, 5, 6, 7, 8, 9, 10} <= phases or not closed_in:
        raise SystemExit(f'coverage: phases {sorted(phases)}, close-ins begun {closed_in}')
    print(f'ok: {runs} runs, {frames} frames of the repair state identical (phases {sorted(phases)}, {closed_in} close-ins begun, {ends} ends)')


if __name__ == '__main__':
    main()

"""The mech's command handler (0x41be80, docs/controls.md, The mech's
commands) and the heat keys' 0x419190 / 0x419e60 against the original x86.

    cmake --build build && .venv/bin/python tests/differential/test_command.py [trials]

The handler runs on a mech in emulator memory with a copy of the real vtable;
everything it calls (the mech's and the weapon manager's own functions, the
HUD, the camera, sounds, lance orders, the torso, the controls' view) is a
logging stub answering from the scenario, and the port answers the same
through CommandCalls::own and its hooks (tests/probe_command.cpp). Each trial
is one command; the call log and the fields it writes (+0xbc, +0x78, the
AMS, the fire mode, [[0x5f4d44]], +0x37c, +0x614) must match. The heat keys
run for real on both sides: the state bits, heat, coolant and sounds.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

os.environ.setdefault('MW3_X87', '24')
sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT, Original  # noqa: E402

LIB = next((ROOT / 'build').glob('libmw3probe.*'))
probe = ctypes.CDLL(str(LIB))
P, I, U = ctypes.c_void_p, ctypes.c_int32, ctypes.c_uint32
probe.probe_command.argtypes = [P, P, P, P, I]
probe.probe_command.restype = I
probe.probe_heat_key.argtypes = [I, I, I, I, P, P, U, P, I]
probe.probe_heat_key.restype = I

COMMAND, OVERRIDE, FLUSH, MECH_VT = 0x41be80, 0x419190, 0x419e60, 0x5bcef8
VIEW, OPTION, LANCE, PILOT = 0x5f4c8c, 0x5f4d44, 0x609980, 0x60861c
HUD, PLAY, MECH_SOUND, CAMERA, OTHER = 0x1000, 0x1001, 0x1002, 0x1003, 0x1004

# The scenario's fields, in probe_command.cpp's In order.
FIELDS = ['cmd', 'active', 'viewed', 'screen', 'powered', 'crouched', 'ams', 'xbc', 'range', 'eq6', 'eq12', 'masc',
          'target', 'saved', 'view', 'mode', 'group', 'current', 'select', 'jettison', 'hud_weapon',
          'hud_weapon_group', 'lance_busy', 'stations']
# The commands the player's keys send (0x426ce0) and the second switch's.
COMMANDS = [0x24, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x1a, 0x25, 0x26, 0x27, 0x2a, 0x2b,
            0x28, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x39, 0x06, 0x07, 0x3b, 0x3a,
            0x6e, 0x6b, 0x68, 0x74, 0x75, 0xa5, 0x69, 0x47, 0x4e, 0x4d, 0x46, 0x4c, 0x4b, 0x48, 0x14, 0x15,
            0x49, 0x4a, 0x29, 0x99, 0x41, 0x16, 0x17, 0x76, 0x77, 0x92, 0x13, 0x3e, 0x3f, 0x40, 0x43, 0xa6,
            0x42, 0x44, 0x36, 0x37, 0x38, 0x52, 0x51, 0x4f, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x62, 0x63,
            0x66, 0x67, 0x64, 0x65, 0x45, 0xa1, 0x73, 0x59, 0x5a, 0x5d, 0x5e, 0x5f, 0x60, 0x5c, 0x5b, 0x50, 0xa7]


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Emulated:
    def __init__(self, heat_keys=False):
        cpu = self.cpu = Original()
        a = cpu.alloc
        self.mech, self.mvt, self.wm, self.weapon = a(0x700), a(0x200), a(0x94), a(0x100)
        self.groups = [a(0x40) for _ in range(5)]
        self.screen, self.current, self.ctl, self.cvt = a(0x18000), a(0x100), a(0x80), a(0x10)
        self.torso, self.pilot, self.ams, self.view, self.cam, self.camvt = a(0x100), a(0x100), a(0x10), a(0x80), \
            a(0x10), a(0x20)
        self.option, self.lance, self.lancevt, self.screenvt, self.zoom, self.zoomvt = a(4), a(0x10), a(0x80), \
            a(0x10), a(0x10), a(0x80)
        self.sound, self.new_ctl, self.other_view = 0x7700, a(0x80), a(0x10)

        def vstub(slot_addr, fn, pop):
            f = cpu.code(b'\xc3')
            cpu.u32(slot_addr, f)
            cpu.stub_function(f, fn, pop)
        cpu.write(self.mvt, cpu.read(MECH_VT, 0x200))
        vstub(self.mvt + 0xa0, lambda c: self.log(0x41cc10, 0, 0) or self.s['powered'], 0)
        vstub(self.mvt + 0xb8, lambda c: self.log(OTHER, self.s['cmd'], 0), 0)
        vstub(self.mvt + 0xc0, lambda c: self.log(0x41d890, c.arg(0), c.arg(1)), 8)
        vstub(self.cvt, lambda c: 0, 4)                         # the controls' +0: nothing
        vstub(self.camvt + 0x10, lambda c: self.log(CAMERA, c.arg(0), 0), 4)
        for off, pop in ((0x60, 0), (0x64, 0), (0x68, 4)):
            vstub(self.lancevt + off, lambda c: self.log(OTHER, self.s['cmd'], 0), pop)
        vstub(self.screenvt + 4, lambda c: self.log(HUD, c.ecx, 0x6b), 4)
        vstub(self.zoomvt + 0x60, lambda c: self.log(HUD, c.ecx - 0x17964, 0x43), 4)
        vstub(self.zoomvt + 0x64, lambda c: self.log(HUD, c.ecx - 0x17964, 0x43), 0)
        wm = lambda addr, pop, arg=False: (lambda c: self.log(addr, c.ecx, c.arg(0) if arg else 0), pop)  # noqa: E731
        stubs = {
            0x430aa0: (lambda c: self.screen if self.s['screen'] else 0, 0),
            0x442e00: (lambda c: self.log(HUD, c.ecx - 0x1508, 0x15), 0),
            0x444330: (lambda c: self.log(HUD, c.ecx - 0x144c, 0x48 if self.s['cmd'] == 0x48 else 0x14), 4),
            0x43da40: (lambda c: self.log(HUD, c.ecx - 0x14, 0x68), 0),
            0x43d240: (lambda c: self.log(HUD, c.ecx, 0x74), 0),
            0x43d250: (lambda c: self.log(HUD, c.ecx, 0x75), 0),
            0x43dc10: (lambda c: self.log(HUD, c.ecx - 0x14, 0xa5), 0),
            0x432990: (lambda c: self.log(PLAY, c.arg(0), 0) or self.sound, 0x14),
            0x41e770: (lambda c: self.log(MECH_SOUND, c.ecx, 0), 8),
            0x465a00: (lambda c: self.log(0x465a00, c.arg(0), 0), 4),
            0x41d810: (lambda c: self.log(0x41d810, c.arg(0), 0), 4),
            0x426780: (lambda c: self.log(0x426780, c.arg(0), 0), 4),
            0x479480: (lambda c: self.s['lance_busy'], 0),
            0x469930: (lambda c: self.log(0x469930, c.edx, 0) or self.new_ctl, 0),
            0x5a85e0: (lambda c: 0, 0),
            0x47a910: (lambda c: self.log(OTHER, self.s['cmd'], 0), 0),
            0x41d070: (lambda c: self.log(OTHER, self.s['cmd'], 0), 0),
            0x41e4e0: (lambda c: self.log(OTHER, self.s['cmd'], 0), 4),
            0x40f070: (lambda c: self.log(OTHER, self.s['cmd'], 0), 0),
            0x41bda0: (lambda c: self.log(0x41bda0, c.arg(0) & 0xff, 0), 4),
            0x41cc30: (lambda c: self.log(0x41cc30, c.arg(0), 0), 4),
            0x41df50: (lambda c: self.log(0x41df50, c.arg(0) & 0xff, 0), 4),
            0x419190: (lambda c: self.log(0x419190, c.arg(0), 0), 4),
            0x419e60: (lambda c: self.log(0x419e60, 0, 0), 0),
            0x41b840: (lambda c: self.log(0x41b840, 0, 0), 0),
            0x4d6d20: (lambda c: self.log(0x4d6d20, c.arg(0), 0), 4),
            0x4b0730: (lambda c: self.log(0x4b0730, c.ecx, c.arg(0)) or self.s['select'], 4),
            0x4b1de0: wm(0x4b1de0, 4, True),
            0x4b1e10: wm(0x4b1e10, 4, True),
            0x4b0a70: wm(0x4b0a70, 4, True),
            0x4b04c0: (lambda c: self.log(0x4b04c0, c.ecx, 0) or (self.current if self.s['current'] else 0), 0),
            0x4b0ae0: (lambda c: self.log(0x4b0ae0, c.ecx, 0) or self.s['jettison'], 0),
        }
        for addr in (0x4b0790, 0x4b07e0, 0x4b04f0, 0x4b0610, 0x4b0830):
            stubs[addr] = wm(addr, 0)
        for addr in (0x4d5ee0, 0x4d6260, 0x4d64b0, 0x4d6970, 0x4d6710, 0x4d6a80):
            stubs[addr] = (lambda c, addr=addr: self.log(addr, c.arg(0), 0), 4)
        if heat_keys:  # run for real
            del stubs[OVERRIDE], stubs[FLUSH]
        for addr, (fn, pop) in stubs.items():
            cpu.stub_function(addr, fn, pop)
        cpu.u32(VIEW, self.view)
        cpu.u32(self.view + 0x58, self.cam)
        cpu.u32(self.cam, self.camvt)
        cpu.u32(OPTION, self.option)
        cpu.u32(LANCE, self.lance)
        cpu.u32(self.lance, self.lancevt)
        cpu.u32(PILOT, self.pilot)
        cpu.u32(self.screen, self.screenvt)
        cpu.u32(self.screen + 0x17964, self.zoomvt)

    def log(self, code, a, b):
        self.events.extend([3, code, a & 0xffffffff, b & 0xffffffff])
        return 0

    def tokens(self):
        return [self.wm, self.weapon] + self.groups + [self.screen, self.current, self.mech, self.sound, self.new_ctl]

    def run(self, s):
        cpu, m = self.cpu, self.mech
        self.s, self.events = s, []
        cpu.write(m, bytes(0x700))
        cpu.u32(LANCE, self.lance if s['stations'] else 0)
        cpu.u32(m, self.mvt)
        cpu.write(m + 8, bytes([s['active']]))
        cpu.u32(m + 0x6c, s['target'])
        cpu.u32(m + 0x78, s['saved'])
        cpu.u32(m + 0x9c, s['range'])
        cpu.write(m + 0xbc, bytes([s['xbc']]))
        cpu.u32(m + 0xd4, self.wm)
        cpu.u32(m + 0x37c, self.ctl)
        cpu.u32(m + 0x39c, self.torso)
        cpu.u32(m + 0x3c4, s['eq6'])
        cpu.u32(m + 0x3dc, s['eq12'])
        cpu.u32(m + 0x3f0, self.ams if s['ams'] else 0)
        cpu.write(self.ams, bytes([s['ams'] == 2]))
        cpu.u32(m + 0x574, s['masc'])
        cpu.write(m + 0x586, bytes([s['crouched']]))
        cpu.u32(m + 0x684, self.pilot)
        cpu.u32(self.ctl, self.cvt)
        cpu.u32(self.new_ctl, self.cvt)
        cpu.u32(self.ctl + 0xc, s['view'])
        cpu.u32(self.wm + 0x1c, s['mode'])
        cpu.u32(self.wm + 0x28, s['group'])
        for k, g in enumerate(self.groups):
            cpu.u32(self.wm + 0x2c + 4 * k, g)
        cpu.u32(self.weapon + 0x60, s['hud_weapon_group'])
        cpu.u32(self.screen + 0x1795c, self.weapon if s['hud_weapon'] else 0)
        cpu.u32(self.screen + 0x17970, s['zoom_flags'])
        cpu.u32(self.screen + 4, s['screen4'])
        cpu.u32(self.view + 0x64, m if s['viewed'] else self.other_view)
        cpu.u32(self.option, 0xffffffff)
        cpu.call(COMMAND, ecx=m, stack_args=(s['cmd'],))
        u = lambda addr: struct.unpack('<I', cpu.read(addr, 4))[0]  # noqa: E731
        ctl = u(m + 0x37c)
        out = [cpu.read(m + 0xbc, 1)[0], u(m + 0x78), cpu.read(self.ams, 1)[0] if s['ams'] else 0,
               u(self.wm + 0x1c), u(self.option), 0 if ctl == self.ctl else ctl, u(m + 0x614)]
        return out, self.events

    def heat_key(self, which, on, viewed, pilot, state, heat, coolant):
        cpu, m = self.cpu, self.mech
        self.events = []
        cpu.write(m, bytes(0x700))
        cpu.u32(m + 0x684, self.pilot)
        cpu.u32(m + 0x69c, state)
        cpu.write(m + 0x554, struct.pack('<f', heat))
        cpu.write(m + 0x56c, struct.pack('<f', coolant))
        cpu.u32(self.view + 0x64, m if viewed else self.other_view)
        cpu.u32(self.pilot + 0x14, m if pilot else self.other_view)
        if which == 0:
            cpu.call(OVERRIDE, ecx=m, stack_args=(on,))
        else:
            cpu.call(FLUSH, ecx=m)
        u = lambda addr: struct.unpack('<I', cpu.read(addr, 4))[0]  # noqa: E731
        self.events.extend([3, 0x610, u(m + 0x610), 0])
        return [u(m + 0x69c), u(m + 0x554), u(m + 0x56c)], self.events


def port(s, tokens):
    i = (ctypes.c_int32 * len(FIELDS))(*[ctypes.c_int32(s[k] if s[k] < 2**31 else s[k] - 2**32).value
                                          for k in FIELDS])
    t = (ctypes.c_uint32 * len(tokens))(*tokens)
    out, log = (ctypes.c_int32 * 7)(), (ctypes.c_int32 * 4096)()
    n = probe.probe_command(i, t, out, log, 4096)
    return [v & 0xffffffff for v in out], [v & 0xffffffff for v in log[:n]]


def port_heat(which, on, viewed, pilot, state, heat, coolant, sound):
    st = ctypes.c_uint32(state)
    io = (ctypes.c_float * 2)(heat, coolant)
    log = (ctypes.c_int32 * 256)()
    n = probe.probe_heat_key(which, on, viewed, pilot, ctypes.byref(st), io, sound, log, 256)
    return [st.value, f2u(io[0]), f2u(io[1])], [v & 0xffffffff for v in log[:n]]


def pick(rng, *values):
    return rng.choice(values)


def scenario(rng):
    r = rng.random
    return {
        'cmd': rng.choice(COMMANDS), 'active': int(r() < 0.85), 'viewed': int(r() < 0.8), 'screen': int(r() < 0.8),
        'powered': int(r() < 0.3), 'crouched': int(r() < 0.3), 'ams': rng.randrange(3), 'xbc': rng.randrange(256),
        'range': f2u(pick(rng, 250.0, 500.0, 999.0, 1000.0, 1500.0, 2000.0, 4000.0, rng.uniform(0, 3000))),
        'eq6': pick(rng, 0, 0, 1, 2, -1), 'eq12': pick(rng, 0, 1, 1, -1),
        'masc': f2u(pick(rng, 0.0, 0.0, 1.5, -0.0)), 'target': pick(rng, 0, 0x6100, 0x6200),
        'saved': pick(rng, 0, 0x6300), 'view': rng.randrange(5), 'mode': rng.randrange(3),
        'group': rng.randrange(-1, 5), 'current': int(r() < 0.7), 'select': int(r() < 0.6),
        'jettison': int(r() < 0.5), 'hud_weapon': int(r() < 0.7), 'hud_weapon_group': rng.randrange(-1, 5),
        'lance_busy': int(r() < 0.3), 'zoom_flags': rng.choice((0, 0x10, 0x30)), 'screen4': rng.randrange(2),
    }


def with_stations(s):
    # [0x00609980] (multiplayer's stations) null one time in four, from the
    # scenario's own numbers so the random stream stays as it was.
    s['stations'] = int(s['xbc'] % 4 != 0)
    return s


def main():
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 6000
    rng = random.Random(41)
    e = Emulated()
    bad = 0
    seen = set()
    for n in range(trials):
        s = with_stations(scenario(rng))
        if n < len(COMMANDS):
            s['cmd'], s['active'], s['viewed'], s['screen'] = COMMANDS[n], 1, 1, 1
        want, got = e.run(s), port(s, e.tokens())
        seen.add(s['cmd'])
        if want != got:
            bad += 1
            if bad <= 5:
                print(f'mismatch {n}: cmd {s["cmd"]:#x} {s}\n  original {want}\n  port     {got}')
    # The radar at its edges: every range step, with and without the sensors.
    for r in (250.0, 500.0, 999.0, 1000.0, 1001.0, 1999.0, 2000.0, 4000.0):
        for eq6 in (0, 1, -1):
            for xbc in (0, 4, 0xfb, 0xff):
                s = with_stations(scenario(rng))
                s.update(cmd=0x69, active=1, range=f2u(r), eq6=eq6, xbc=xbc)
                trials += 1
                if e.run(s) != port(s, e.tokens()):
                    bad += 1
                    if bad <= 5:
                        print(f'radar mismatch: range {r} eq6 {eq6} xbc {xbc:#x}')
    hbad = 0
    e = Emulated(heat_keys=True)
    for n in range(3000):
        which, on, viewed, pilot = rng.randrange(2), rng.randrange(2), int(rng.random() < 0.8), int(rng.random() < 0.8)
        state = rng.choice((0, 1, 2, 3, 0x1000000, 0x1000001, 0x1000003, 0x2000002)) | rng.randrange(8) << 3
        heat = pick(rng, 0.0, 0.3, 0.9, 1.4, 2.0, rng.uniform(0, 2))
        coolant = pick(rng, 0.0, 1.0, 0.5, 0.125, rng.uniform(0, 1))
        want = e.heat_key(which, on, viewed, pilot, state, heat, coolant)
        got = port_heat(which, on, viewed, pilot, state, heat, coolant, e.sound)
        if want != got:
            hbad += 1
            if hbad <= 5:
                print(f'heat mismatch {n}: which {which} on {on} viewed {viewed} pilot {pilot} state {state:#x} '
                      f'heat {heat} coolant {coolant}\n  original {want}\n  port     {got}')
    print(f'{trials - bad}/{trials} commands identical ({len(seen)} kinds), '
          f'{3000 - hbad}/3000 heat keys identical')
    sys.exit(1 if bad or hbad else 0)


if __name__ == '__main__':
    # Leave without Unicorn's teardown (it can hang at exit): the status
    # SystemExit carries, 1 for any other fault.
    code = 0
    try:
        main()
    except SystemExit as ex:
        if ex.code not in (None, 0):
            print(ex.code if isinstance(ex.code, str) else '', flush=True)
        code = 0 if ex.code in (None, 0) else 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(code)

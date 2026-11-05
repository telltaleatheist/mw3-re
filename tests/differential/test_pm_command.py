"""PM's mech command handler 0x41cad0 with eject and the lancemate keys (docs/controls.md, docs/pm-map.md), against
mech3/mech_command on a made-up mech: what it calls and what it writes; then the keys 0x4e6ca0, 0x4e6fb0, 0x4e6710 and the
pilot's exit 0x41de60 run for real against the port.

    cmake --build build --target pmprobe
    .venv/bin/python -P tests/differential/test_pm_command.py [trials]

Everything the handler calls is a logging stub, under MW3's addresses as labels (what the port's CommandCalls::own says). The
HUD's stubs log the command, not which of its parts was called; the heat keys are not run here.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_zar  # noqa: E402
import unicorn.x86_const  # noqa: E402

COMMAND, PREP, NEXT_L, FIRST_L, PREV_L, MECH_VT = 0x41cad0, 0x41de60, 0x4e6ca0, 0x4e6fb0, 0x4e6710, 0x5dfaac
VIEW, OPTION_P, LANCE, GAME_P, MODE_LOW = 0x61874c, 0x618804, 0x62daf0, 0x62c40c, 0x6293a8
HUD, PLAY, MECH_SOUND, CAMERA, OTHER = 0x1000, 0x1001, 0x1002, 0x1003, 0x1004
TEAM, REFUSES, DOWN = 0x2001, 0x2002, 0x2003
ELSE = 0x7f00
POP_SOUND = 0  # cdecl here
FIELDS = ['cmd', 'active', 'viewed', 'screen', 'powered', 'crouched', 'ams', 'xbc', 'range', 'eq6', 'eq12', 'masc',
          'target', 'saved', 'view', 'mode', 'group', 'current', 'select', 'jettison', 'hud_weapon',
          'hud_weapon_group', 'lance_busy', 'allies', 'aim', 'entity', 'team', 'refuse', 'ncontacts',
          'seen0', 'seen1', 'seen2', 'seen3', 'stations', 'limp', 'down']
COMMANDS = [0x24, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x1a, 0x25, 0x26, 0x27, 0x2a, 0x2b,
            0x28, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x39, 0x06, 0x07, 0x3b, 0x3c,
            0x3a, 0x76, 0x73, 0x70, 0x7c, 0x7d, 0xad, 0x71, 0x47, 0x48, 0x49, 0x4d, 0x4e, 0x4f, 0x4a, 0x4b,
            0x4c, 0x50, 0x14, 0x15, 0x51, 0x52, 0x29, 0xa1, 0x42, 0x16, 0x17, 0x7e, 0x7f, 0x9a, 0x13, 0x3f,
            0x40, 0x41, 0x44, 0xae, 0x43, 0x45, 0x36, 0x37, 0x38, 0x56, 0x55, 0x53, 0x57, 0x58, 0x59, 0x5a,
            0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x6a, 0x6b, 0x6e, 0x6f, 0x6c, 0x6d, 0x46, 0xa9, 0x7b, 0x61,
            0x62, 0x65, 0x66, 0x67, 0x68, 0x64, 0x63]


def f2u(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


class Emulated(test_pm_zar.Emulated):
    def __init__(self):
        super().__init__()
        a = self.alloc
        self.mech, self.mvt, self.wm, self.weapon = a(0x800), a(0x200), a(0x94), a(0x100)
        self.groups = [a(0x40) for _ in range(5)]
        self.screen, self.current, self.ctl, self.cvt = a(0x18000), a(0x100), a(0x80), a(0x10)
        self.torso, self.head, self.headvt, self.ams = a(0x100), a(0x20), a(0x40), a(0x10)
        self.view, self.cam, self.camvt, self.option = a(0x80), a(0x10), a(0x20), a(4)
        self.lance, self.lancevt, self.screenvt, self.zoomvt = a(0x10), a(0x80), a(0x10), a(0x80)
        self.sound, self.new_ctl, self.other_view, self.game, self.gamevt = 0x7700, a(0x80), a(0x10), a(0x10), a(0x40)
        self.rec, self.node, self.carr = a(0x40), a(0x40), a(0x10)
        self.contacts = [a(0x30) for _ in range(4)]
        self.ents, self.teams = [a(0x10) for _ in range(3)], [a(0x100) for _ in range(3)]
        self.pilot = a(0x80)
        self._w(self.mvt, bytes(self.cpu._uc.mem_read(MECH_VT, 0x200)))
        self.vstub(self.mvt + 0xac, lambda c: self.log(0x41cc10, 0, 0) or self.s['powered'], 0)
        self.vstub(self.mvt + 0xc4, lambda c: self.log(OTHER, self.s['cmd'], 0), 0)
        self.vstub(self.mvt + 0xcc, lambda c: self.log(0x41d890, c.arg(0), c.arg(1)), 8)
        self.vstub(self.cvt, lambda c: 0, 4)
        self.vstub(self.camvt + 0x10, lambda c: self.log(CAMERA, c.arg(0), 0), 4)
        self.vstub(self.headvt + 0x20, lambda c: self.log(0x4671e0, c.arg(0), c.arg(1)), 8)
        self.vstub(self.gamevt + 0x28, lambda c: 1, 0)
        for off, pop in ((0x58, 0), (0x5c, 0), (0x60, 4)):
            self.vstub(self.lancevt + off, lambda c: self.log(OTHER, self.s['cmd'], 0), pop)
        self.vstub(self.screenvt + 8, lambda c: self.log(HUD, self.screen, 0x73), 4)
        self.vstub(self.zoomvt + 0x60, lambda c: self.log(HUD, self.screen, 0x44), 4)
        self.vstub(self.zoomvt + 0x64, lambda c: self.log(HUD, self.screen, 0x44), 0)
        for ent, team in zip(self.ents, self.teams):
            evt = a(0x100)
            self.w32(ent, evt)
            self.vstub(evt + 0x28, lambda c, team=team: self.log(TEAM, c.ecx, 0) or team, 0)
            self.vstub(evt + 0xb8, lambda c: self.log(REFUSES, c.ecx, 1) or self.s['refuse'] & 1, 0)
            self.vstub(evt + 0xbc, lambda c: self.log(REFUSES, c.ecx, 0) or self.s['refuse'] >> 1 & 1, 0)
            self.vstub(evt + 0xac, lambda c: self.log(DOWN, c.ecx, 0) or self.s['down'], 0)
        wm = lambda label, pop, arg=False: (lambda c: self.log(label, c.ecx, c.arg(0) if arg else 0), pop)  # noqa: E731
        hud = lambda cmd: (lambda c: self.log(HUD, self.screen, cmd), 0)  # noqa: E731
        for addr, (fn, pop) in {
            0x430e30: (lambda c: self.screen if self.s['screen'] else 0, 0),
            0x444650: (lambda c: self.log(HUD, self.screen if self.s['viewed'] and self.s['screen'] else 0, 0x15), 0),  # off the view, the exe's register is stale
            0x445c40: (lambda c: self.log(HUD, self.screen, 0x50 if self.s['cmd'] == 0x50 else 0x14) or self.rec, 4),
            0x40f700: (lambda c: self.aimed(self.r32(c.ecx)), 0),
            0x43eec0: hud(0x70), 0x43e570: hud(0x7c), 0x43e580: hud(0x7d), 0x43f090: hud(0xad),
            0x432d10: (lambda c: self.log(PLAY, c.arg(0), 0) or self.sound, 0x14),
            0x41f310: (lambda c: self.log(MECH_SOUND, c.arg(0), 0), POP_SOUND),
            0x4672d0: (lambda c: self.log(0x465a00, c.arg(0), 0), 4),
            0x41e760: (lambda c: self.log(0x41d810, c.arg(0), 0), 4),
            0x4283f0: (lambda c: self.log(0x426780, c.arg(0), 0), 4),
            0x47b710: (lambda c: self.s['lance_busy'], 4),
            0x46b4b0: (lambda c: self.log(0x469930, c.arg(1), 0) or self.new_ctl, 0),
            0x5cb260: (lambda c: 0, 0),
            0x47cbe0: (lambda c: self.log(OTHER, self.s['cmd'], 0), 0),
            PREP: (lambda c: self.log(0x41d070, 0, 0), 0),
            0x41f000: (lambda c: self.log(OTHER, self.s['cmd'], 0), 4),
            0x40f940: (lambda c: self.log(OTHER, self.s['cmd'], 0), 0),
            0x41c9f0: (lambda c: self.log(0x41bda0, c.arg(0) & 0xff, 0), 4),
            0x41dab0: (lambda c: self.log(0x41cc30, c.arg(0), 0), 4),
            0x41eec0: (lambda c: self.log(0x41df50, c.arg(0) & 0xff, 0), 4),
            0x419fb0: (lambda c: self.log(0x419190, c.arg(0), 0), 4),
            0x41ad50: (lambda c: self.log(0x419e60, 0, 0), 0),
            0x41c570: (lambda c: self.log(0x41b840, 0, 0), 0),
            0x41f900: (lambda c: self.log(0x41f900, 0, 0), 0),
            0x4e7170: (lambda c: self.log(0x4d6d20, c.arg(0), 0), 4),
            0x4b4560: (lambda c: self.log(0x4b0730, c.ecx, c.arg(0)) or self.s['select'], 4),
            0x4b5cb0: wm(0x4b1de0, 4, True), 0x4b5ce0: wm(0x4b1e10, 4, True), 0x4b48c0: wm(0x4b0a70, 4, True),
            0x4b4370: (lambda c: self.log(0x4b04c0, c.ecx, 0) or (self.current if self.s['current'] else 0), 0),
            0x4b4940: (lambda c: self.log(0x4b0ae0, c.ecx, 0) or self.s['jettison'], 0),
            0x4b45f0: wm(0x4b0790, 0), 0x4b4640: wm(0x4b07e0, 0), 0x4b43a0: wm(0x4b04f0, 0), 0x4b4480: wm(0x4b0610, 0),
            0x4e63e0: (lambda c: self.log(0x4d5ee0, c.arg(0), 0), 4), 0x4e65d0: (lambda c: self.log(0x4d6260, c.arg(0), 0), 4),
            0x4e6840: (lambda c: self.log(0x4d64b0, c.arg(0), 0), 4), 0x4e6df0: (lambda c: self.log(0x4d6970, c.arg(0), 0), 4),
            0x4e6ab0: (lambda c: self.log(0x4d6710, c.arg(0), 0), 4), 0x4e6ed0: (lambda c: self.log(0x4d6a80, c.arg(0), 0), 4),
            NEXT_L: (lambda c: self.log(NEXT_L, 0, 0), 0), FIRST_L: (lambda c: self.log(FIRST_L, 0, 0), 0),
            PREV_L: (lambda c: self.log(PREV_L, 0, 0), 0),
            0x4e7680: (lambda c: self.log(0x4d7220, 0, 0), 0), 0x4e76d0: (lambda c: self.log(0x4d7230, 0, 0), 0),
        }.items():
            self.native_stub(addr, fn, pop)
        self.w32(VIEW, self.view)
        self.w32(self.view + 0x58, self.cam)
        self.w32(self.cam, self.camvt)
        self.w32(OPTION_P, self.option)
        self.w32(self.lance, self.lancevt)
        self.w32(GAME_P, self.game)
        self.w32(self.game + 0x6c, self.game + 8)
        self.w32(self.game + 8, self.gamevt)
        self.w32(self.screen, self.screenvt)
        self.w32(self.screen + 0x17b50, self.zoomvt)

    def _w(self, a, data):
        self.cpu._uc.mem_write(a, bytes(data))

    def w32(self, a, v):
        self._w(a, struct.pack('<I', v & 0xffffffff))

    def vstub(self, slot, fn, pop):
        f = self.cpu.code(b'\xc3')
        self.w32(slot, f)
        self.native_stub(f, fn, pop)

    def log(self, code, a, b):
        self.events.extend([3, code, a & 0xffffffff, b & 0xffffffff])
        return 0

    def aimed(self, node):
        self.log(0x40eee0, node, 0)
        return self.ents[self.s['entity'] - 1] if node and self.s['entity'] else 0

    def seen(self, k):
        v = self.s[f'seen{k}']
        return self.ents[v - 1] if v else ELSE

    def tokens(self):
        return [self.wm, self.weapon] + self.groups + [self.screen, self.current, self.mech, self.sound,
                                                        self.new_ctl, self.node] + self.contacts + self.ents

    def run(self, s):
        m, w = self.mech, self.w32
        self.s, self.events = s, []
        self._w(m, bytes(0x800))
        w(LANCE, self.lance if s['stations'] else 0)
        w(m, self.mvt)
        self._w(m + 8, bytes([s['active']]))
        w(m + 0x6c, self.contacts[s['target']] if s['target'] >= 0 else 0)
        w(m + 0xc4, s['allies'])
        for k, ct in enumerate(self.contacts):
            w(ct + 4, self.seen(k))
            w(self.carr + 4 * k, ct)
        w(m + 0xac, self.carr), w(m + 0xb0, self.carr + 4 * s['ncontacts'])
        w(self.rec + 0x24, self.node if s['aim'] else 0)
        for team in self.teams:
            w(team + 0xc0, s['team'])
        w(m + 0x78, s['saved']), w(m + 0x9c, s['range'])
        self._w(m + 0xbc, bytes([s['xbc']]))
        w(m + 0xd4, self.wm), w(m + 0x38c, self.ctl), w(m + 0x3ac, self.torso), w(m + 0x3b4, self.head)
        w(self.head, self.headvt)
        w(m + 0x3d4, s['eq6']), w(m + 0x3ec, s['eq12']), w(m + 0x400, self.ams if s['ams'] else 0)
        self._w(self.ams, bytes([s['ams'] == 2]))
        w(m + 0x598, s['masc']), w(m + 0x5d0, s['limp'])
        self._w(m + 0x5ab, bytes([s['crouched']]))
        w(m + 0x6a8, self.pilot)
        w(self.ctl, self.cvt), w(self.new_ctl, self.cvt)
        w(self.ctl + 0xc, s['view'])
        w(self.wm + 0x1c, s['mode']), w(self.wm + 0x2c, s['group'])
        for k, g in enumerate(self.groups):
            w(self.wm + 0x30 + 4 * k, g)
        w(self.weapon + 0x64, s['hud_weapon_group'])
        w(self.screen + 0x17b48, self.weapon if s['hud_weapon'] else 0)
        w(self.screen + 0x17b5c, s['zoom_flags'])
        w(self.screen + 4, s['screen4'])
        w(self.view + 0x64, m if s['viewed'] else self.other_view)
        w(self.option, 0xffffffff)
        w(self.wm + 0x20, 0), w(0x8c1b50, f2u(2.5))
        try:
            self.call(COMMAND, ecx=m, args=[s['cmd']])
        except Exception as ex:
            raise SystemExit(f'cmd {s["cmd"]:#x} eip {self.cpu._uc.reg_read(unicorn.x86_const.UC_X86_REG_EIP):#x}: {ex}\n{s}')
        ctl = self.r32(m + 0x38c)
        out = [self.r32(m + 0xbc) & 0xff, self.r32(m + 0x78), self.r32(self.ams) & 0xff if s['ams'] else 0,
               self.r32(self.wm + 0x1c), self.r32(self.option), 0 if ctl == self.ctl else ctl, self.r32(m + 0x638),
               1 if self.r32(m + 0x688) == m else 0, self.r32(self.wm + 0x20)]
        return out, self.events


def port(p, s, tokens):
    i = (ctypes.c_int32 * len(FIELDS))(*[ctypes.c_int32(s[k] if s[k] < 2**31 else s[k] - 2**32).value for k in FIELDS])
    t = (ctypes.c_uint32 * len(tokens))(*tokens)
    out, log = (ctypes.c_int32 * 9)(), (ctypes.c_int32 * 4096)()
    n = p.probe_command(i, t, out, log, 4096)
    return [v & 0xffffffff for v in out], [v & 0xffffffff for v in log[:n]]


def scenario(rng):
    r = rng.random
    c = rng.choice
    return {
        'cmd': c(COMMANDS), 'active': int(r() < 0.85), 'viewed': int(r() < 0.8), 'screen': int(r() < 0.8),
        'powered': int(r() < 0.3), 'crouched': int(r() < 0.3), 'ams': rng.randrange(3), 'xbc': rng.randrange(256),
        'range': f2u(c((250.0, 500.0, 999.0, 1000.0, 1500.0, 2000.0, 4000.0, rng.uniform(0, 3000)))),
        'eq6': c((0, 0, 1, 2, -1)), 'eq12': c((0, 1, 1, -1)),
        'masc': f2u(c((0.0, 0.0, 1.5, -0.0))), 'target': c((-1, -1, 0, 1, 2, 3)),
        'saved': c((0, 0x6300)), 'view': rng.randrange(5), 'mode': rng.randrange(3),
        'group': rng.randrange(-1, 5), 'current': int(r() < 0.7), 'select': int(r() < 0.6),
        'jettison': int(r() < 0.5), 'hud_weapon': int(r() < 0.7), 'hud_weapon_group': rng.randrange(-1, 5),
        'lance_busy': int(r() < 0.3), 'zoom_flags': c((0, 0x10, 0x30)), 'screen4': rng.randrange(2),
        'allies': c((0, 2, 6, 0x1e)), 'aim': int(r() < 0.85), 'entity': rng.randrange(4),
        'team': c((1, 2, 4, 8)), 'refuse': c((0, 0, 0, 1, 2, 3)), 'ncontacts': rng.randrange(5),
        **{f'seen{k}': rng.randrange(4) for k in range(4)},
        'stations': int(r() < 0.75), 'limp': c((-1, -1, 0, 1, 2)), 'down': int(r() < 0.2),
    }


def keys(emu, p, rng, trials):
    """0x4e6ca0, 0x4e6fb0, 0x4e6710 for real: each contact's seen object answers itself as its vehicle."""
    m, player, w = emu.alloc(0x100), emu.alloc(0x100), emu.w32
    vt, pvt = emu.alloc(0x100), emu.alloc(0x100)
    w(m, vt), w(player, pvt)
    picked = []
    emu.vstub(vt + 0xcc, lambda c: picked.append(c.arg(0)), 8)
    emu.vstub(pvt + 0x28, lambda c: c.ecx, 0)
    veh, vvt = [emu.alloc(0x100) for _ in range(4)], emu.alloc(0x40)
    emu.vstub(vvt + 0x2c, lambda c: c.ecx, 0)
    for v in veh:
        w(v, vvt)
    cs, arr = [emu.alloc(0x30) for _ in range(6)], emu.alloc(0x20)
    stubbed = {a: emu.cpu.stubs.pop(a) for a in (NEXT_L, FIRST_L, PREV_L, 0x4e7680, 0x4e76d0)}
    bad, seen = 0, {0: 0, 1: 0, 2: 0, 3: 0, 4: 0}
    for n in range(trials):
        kind = n % 5
        count = rng.randrange(7)
        t, ally, enemy = (rng.randrange(-1, 6) for _ in range(3))
        side = rng.randrange(2)
        rows, flags = [], [rng.choice((0, 0x10, 0x10, 0x13)) for _ in range(4)]
        for i in range(6):
            rows += [rng.randrange(4), rng.randrange(1, 4), rng.randrange(2)]
        for i, c in enumerate(cs):
            w(c + 4, veh[rows[3 * i]]), w(c + 8, rows[3 * i + 1]), w(c, 0), emu._w(c + 0x2c, bytes([rows[3 * i + 2]]))
            w(arr + 4 * i, c)
        for v, f in zip(veh, flags):
            emu._w(v + 0xbc, bytes([f]))
        w(m + 0x6c, cs[t] if t >= 0 else 0), w(m + 0x70, cs[ally] if ally >= 0 else 0), w(m + 0x74, cs[enemy] if enemy >= 0 else 0)
        w(m + 0xac, arr), w(m + 0xb0, arr + 4 * count), w(m + 0xc4, 4)
        w(player + 0xc0, 4 if side else 2)
        w(emu.game + 0x14, player)
        picked.clear()
        saved, xbc = rng.randrange(-1, 6), rng.randrange(256)
        w(m + 0x78, cs[saved] if saved >= 0 else 0), emu._w(m + 0xbc, bytes([xbc]))
        emu.call({0: NEXT_L, 1: FIRST_L, 2: PREV_L, 3: 0x4e7680, 4: 0x4e76d0}[kind], ecx=m)
        want = cs.index(picked[0]) if picked else -1
        if kind >= 3:  # what is saved and +0xbc after
            now = emu.r32(m + 0x78)
            want = (cs.index(now) + 1 if now else 0) << 8 | emu.r32(m + 0xbc) & 0xff
        got = p.probe_pm_lance_keys(kind, (ctypes.c_int32 * 29)(t, ally, enemy, side, count, *rows, *flags, saved, xbc))
        seen[kind] += want >= 0 if kind < 3 else want >> 8 != 0
        if want != got:
            bad += 1
            if bad <= 5:
                print(f'keys {n}: kind {kind} t {t} ally {ally} enemy {enemy} side {side} n {count} {rows} {flags}: exe {want} port {got}')
    emu.cpu.stubs.update(stubbed)
    return bad, seen


def prep(emu, p):
    """0x41de60 for real, every state, table, viewer."""
    m, w = emu.alloc(0x800), emu.w32
    hip = emu.alloc(0x20)
    emu.cpu.stubs.pop(PREP)
    log = {}
    emu.native_stub(0x4ff250, lambda c: 0, 0)
    emu.native_stub(0x432bb0, lambda c: log.__setitem__('canned', c.arg(0)) or log['ret'], 0x10)
    emu.native_stub(0x4638f0, lambda c: log.__setitem__('duck', c.arg(0)), 4)
    emu.cpu.stubs[0x432d10] = (lambda c: log.__setitem__('sound', c.arg(0)) or 0x7700, 0x14)  # hooked already
    emu.native_stub(0x492710, lambda c: log.__setitem__('died', 1), 0)
    w(m + 0x3b0, hip)
    bad = cases = 0
    out = (ctypes.c_uint32 * 5)()
    for state in (0, 1, 10, 12, 13):
        for low in (0, 1):
            for player in (0, 1):
                for detail in (0, 1, 0x1c):
                    for canned in (0, 0x9000):
                        log.clear()
                        log['ret'] = canned
                        w(m + 0x59c, state), w(m + 0x6ac, detail), w(m + 0x604, 0)
                        w(MODE_LOW, low)
                        w(emu.game + 0x14, m if player else emu.other_view)
                        emu.call(PREP, ecx=m)
                        want = [log.get('canned', 0xffffffff), log.get('duck', 0), log.get('sound', 0xffffffff),
                                log.get('died', 0), emu.r32(m + 0x604)]
                        p.probe_pm_death_prep(state, low, player, detail, canned, 0x7700, out)
                        cases += 1
                        if want != list(out):
                            bad += 1
                            if bad <= 5:
                                print(f'prep state {state} low {low} player {player} detail {detail} canned {canned:#x}: exe {want} port {list(out)}')
    w(MODE_LOW, 0)
    return bad, cases


def main():
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 6000
    p = test_pm_zar.load_pm_probe()
    p.probe_command.restype = ctypes.c_int32
    P = ctypes.c_void_p
    p.probe_command.argtypes = [P, P, P, P, ctypes.c_int32]
    p.probe_pm_lance_keys.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_int32)]
    p.probe_pm_death_prep.argtypes = [ctypes.c_int] * 3 + [ctypes.c_uint32] * 3 + [ctypes.POINTER(ctypes.c_uint32)]
    rng = random.Random(41)
    e = Emulated()
    bad = 0
    seen = set()
    for n in range(trials):
        s = scenario(rng)
        if n < len(COMMANDS):
            s['cmd'], s['active'], s['viewed'], s['screen'] = COMMANDS[n], 1, 1, 1
        want, got = e.run(s), port(p, s, e.tokens())
        seen.add(s['cmd'])
        if want != got:
            bad += 1
            if bad <= 5:
                print(f'mismatch {n}: cmd {s["cmd"]:#x} {s}\n  exe  {want}\n  port {got}')
    kbad, kseen = keys(e, p, rng, trials)
    pbad, pcases = prep(e, p)
    print(f'{trials - bad}/{trials} commands ({len(seen)} kinds), {trials - kbad}/{trials} lancemate picks (hits {kseen}), '
          f'{pcases - pbad}/{pcases} pilot exits identical')
    if bad or kbad or pbad:
        sys.exit(1)
    pm_floors.floors(commands=(trials, 6000), kinds=(len(seen), 103), pilot_exits=(pcases, 120))
    print('ok')


if __name__ == '__main__':
    code = 0
    try:
        main()
    except SystemExit as ex:
        if isinstance(ex.code, str):
            print(ex.code)
        code = 0 if ex.code in (None, 0) else 1
    except BaseException:
        import traceback
        traceback.print_exc()
        code = 1
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(code)

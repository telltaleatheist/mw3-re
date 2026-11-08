"""Pirate's Moon's state message (0xf) flags word against the port (docs/pm-map.md, the in-game types; mech_net's
state_bits). PM's motion state is 4 bits where MW3's is 5, so every field from the first action up sits one bit
lower, and bit 31 is the headlight.

Part 1, the builder: PM packs the word inline in its tick (0x471040), so the test runs only that stretch,
0x471522..0x471746 (the struck flag through the headlight), on a PM mech's fields (+0x10c struck, +0x08 active,
+0x59c motion state, +0x490/+0x494 the actions, +0x5d0 limp, vt+0xac powered down, +0xbc passive, +0x400 the AMS,
+0x598 MASC, +0x594 NARC, the controls' jump (+0x38c, its base's vt+0x38), +0x5ab crouched, 0x41f920 the
headlight), the shots and parts counts in their registers; the port's word is the 0xf message's from
probe_mech_net_send on the same mech (test_mech_net_send's cases).

Part 2, the apply 0x4705e0 on an inactive state (bit 1: the motion's half skipped) of the fixed 0x3c bytes: what
the flags change on the mech (power, AMS, MASC, NARC, crouch, the headlight, the jump, the motion state's turns)
against probe_mech_net_apply's calls.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_net_state.py [cases]
"""
import ctypes
import math
import os
import random
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_EXE'] = 'pm'
import test_pm_zar  # noqa: E402

I32 = ctypes.c_int32
PACK_FROM, PACK_END = 0x471522, 0x471746
HEADLIGHT_ON = 0x41f920

# The send probe's input (test_mech_net_send.py's, copied: that file loads MW3's probe).
NAMES = ['kind', 'arg', ('vel', 3), 'qw', 'qy', ('turn_q', 4), 'throttle', 'turn', 'jump', ('torso_at', 3), ('aim', 3),
         'clear', 'active', 'anim_state', 'action', 'limp', 'xbc', 'power', 'masc', 'ams', 'narc', 'crouched',
         'target', 'damaged', 'shots', ('shot', 15), 'torso_internal', 'torso_armor', ('sec_armor', 5),
         ('side_internal', 2), ('parts', 10), ('pos', 3), 'remotes', ('owners', 4), ('echo', 4), ('heard', 4),
         'ping_turn', 'ping_counter', 'acked', 'now', 'host', 'players', 'clock_next', 'end_time', 'end_state',
         'game_time', 'clock_left', 'clock_on', 'next_state', 'next_ping', 'resends', 'rate_setting', 'warned',
         'last_second', 'has_game', 'weapon_locked', 'weapon_slot', 'weapon_target', 'state']
K = {}
_i = 0
for n in NAMES:
    name, size = (n, 1) if isinstance(n, str) else n
    K[name] = _i
    _i += size
COUNT = _i


def bits(f):
    return struct.unpack('<i', struct.pack('<f', f))[0]


def frac(rng):
    return rng.choice([0.0, 1.0, rng.random(), rng.random(), 0.5, 1.0 / 255])


def case(rng):
    v = [0] * COUNT
    v[K['kind']] = rng.choice([0xf, 0xf, 0xf, 0x10, 0x15, 0x16, 100, 100, 0x11, 0x12, 0x13, 0x17, 0x21])
    v[K['arg']] = rng.randrange(-1, 12)
    if v[K['kind']] == 0x15:
        v[K['arg']] = rng.randrange(0, 12)
    for i in range(3):
        v[K['vel'] + i] = bits(rng.choice([rng.uniform(-30, 30), rng.uniform(-300, 300), 0.0]))
    yaw = rng.uniform(-math.pi, math.pi)
    v[K['qw']], v[K['qy']] = bits(math.cos(yaw / 2)), bits(math.sin(yaw / 2))
    q = [rng.uniform(-1, 1) for _ in range(4)]
    n = math.sqrt(sum(x * x for x in q))
    for i in range(4):
        v[K['turn_q'] + i] = bits(q[i] / n)
    for k in ('throttle', 'turn', 'jump'):
        v[K[k]] = bits(rng.choice([0.0, rng.uniform(-1, 1), rng.uniform(-2, 2), 1.0]))
    for i in range(3):
        v[K['torso_at'] + i] = bits(rng.uniform(-500, 500))
        v[K['aim'] + i] = bits(rng.uniform(-500, 500))
        v[K['pos'] + i] = bits(rng.uniform(-900, 900))
    if rng.random() < 0.1:
        for i in range(3):
            v[K['aim'] + i] = v[K['torso_at'] + i]  # aiming at itself: no direction
    v[K['clear']] = rng.randrange(2)
    v[K['active']] = int(rng.random() < 0.9)
    v[K['anim_state']] = rng.randrange(0, 40)
    v[K['action']] = rng.choice([-1, -1, rng.randrange(0, 20)])
    v[K['limp']] = rng.choice([-1, 0, 1, 2])
    v[K['xbc']] = rng.randrange(32)
    v[K['power']] = rng.randrange(4)
    v[K['masc']] = bits(rng.choice([0.0, 2.5]))
    v[K['ams']] = rng.randrange(3)
    v[K['narc']] = bits(rng.choice([0.0, 0.0, 3.0, -1.0]))
    v[K['crouched']] = rng.randrange(2)
    v[K['target']] = rng.choice([0, 0, rng.randrange(1, 9)])
    v[K['damaged']] = rng.choice([0, 0, rng.getrandbits(10), 0x1fe, 1 << rng.randrange(10)])
    v[K['shots']] = rng.choice([0, 0, rng.randrange(1, 6)])
    for k in range(5):
        v[K['shot'] + 3 * k] = rng.randrange(0, 20)
        v[K['shot'] + 3 * k + 1] = rng.randrange(0, 5)
        v[K['shot'] + 3 * k + 2] = rng.choice([0, rng.randrange(1, 9)])
    v[K['torso_internal']], v[K['torso_armor']] = bits(frac(rng)), bits(frac(rng))
    for i in range(5):
        v[K['sec_armor'] + i] = bits(frac(rng))
    v[K['side_internal']], v[K['side_internal'] + 1] = bits(frac(rng)), bits(frac(rng))
    for i in range(10):
        v[K['parts'] + i] = bits(frac(rng))
    v[K['remotes']] = rng.randrange(0, 5)
    owners = rng.sample(range(1, 9), 4)
    for k in range(4):
        v[K['owners'] + k] = owners[k]
        v[K['echo'] + k] = rng.randrange(0, 100)
        v[K['heard'] + k] = rng.randrange(0, 50000)
    v[K['ping_turn']] = rng.choice([0, owners[rng.randrange(4)]])
    v[K['ping_counter']] = rng.randrange(1, 40)
    v[K['acked']] = rng.randrange(0, 50000)
    v[K['now']] = rng.randrange(50000, 90000)
    v[K['host']] = rng.randrange(2)
    v[K['players']] = rng.randrange(0, 10)
    t = rng.uniform(0, 600)
    v[K['game_time']] = bits(t)
    v[K['clock_next']] = bits(t + rng.uniform(-3, 3))
    v[K['end_time']] = bits(t + rng.uniform(-3, 3))
    v[K['end_state']] = rng.choice([0, 0, 1, 2, 3])
    v[K['clock_left']] = bits(rng.choice([rng.uniform(0, 600), rng.uniform(0, 70), rng.uniform(0, 4), 1.0, 60.0]))
    v[K['clock_on']] = rng.randrange(2)
    v[K['next_state']] = bits(t + rng.uniform(-0.5, 0.5))
    v[K['next_ping']] = bits(t + rng.uniform(-3, 3))
    v[K['resends']] = rng.choice([0, 5, 24, 25, 26])
    v[K['rate_setting']] = rng.choice([0, 1, 2, 3])
    v[K['warned']], v[K['last_second']] = rng.randrange(2), rng.randrange(2)
    v[K['has_game']] = int(rng.random() < 0.9)
    if v[K['kind']] in (0x10, 0x11):
        v[K['has_game']] = 1  # the host's ping reads the game unchecked; it is only sent from the timer, which has one
    v[K['weapon_locked']] = rng.randrange(2)
    v[K['weapon_slot']] = rng.randrange(0, 20)
    v[K['weapon_target']] = rng.randrange(0, 6)
    v[K['state']] = rng.randrange(0, 4)
    if v[K['kind']] == 0x21:  # a network mech's part: the exe reads its mech's +0x6fc unchecked
        v[K['remotes']] = max(v[K['remotes']], 1)
        v[K['weapon_target']] = rng.randrange(1, v[K['remotes']] + 1)
    return v


FLOAT_WORDS = (6, 7, 8, 10, 11, 12)  # a state's +0x18 heading, +0x1c/+0x20 torso, +0x28/+0x2c aim turn, +0x30 range


def f2u(f):
    return struct.unpack('<I', struct.pack('<f', f))[0]


def u2f(u):
    return struct.unpack('<f', struct.pack('<I', u & 0xffffffff))[0]


class Pack:
    """PM's packing stretch on one mech's fields."""

    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.mech, self.buf = z.alloc(0x800), z.alloc(0x40)
        self.vt, self.ctl, self.tbl = z.alloc(0x200), z.alloc(0x20), z.alloc(0x100)
        self.head, self.node, self.ams, self.jump = z.alloc(0x10), z.alloc(0x10), z.alloc(4), z.alloc(4)
        w = self.w
        w(self.mech, self.vt)
        self.power, power_fn = z.alloc(4), z.alloc(8)  # vt+0xac: mov eax, [power]; ret (code once: Unicorn keeps it)
        self.uc.mem_write(power_fn, b'\xa1' + struct.pack('<I', self.power) + b'\xc3')
        w(self.vt + 0xac, power_fn)
        w(self.mech + 0x38c, self.ctl), w(self.ctl + 4, self.tbl), w(self.tbl + 4, 0)  # its base at +4, offset 0
        jump_fn = z.alloc(8)
        self.uc.mem_write(jump_fn, b'\xd9\x05' + struct.pack('<I', self.jump) + b'\xc3')  # fld dword [jump]; ret
        w(self.tbl + 0x38, jump_fn)
        w(self.mech + 0x490, self.head), w(self.head, self.node)
        self.light = 0
        z.native_stub(HEADLIGHT_ON, lambda u: self.light)
        self.uc.mem_write(PACK_END, b'\x83\xc4\x60\xc3')  # add esp, 0x60; ret: the stretch's end

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def run(self, v, shots, parts, light):
        w, m = self.w, self.mech
        self.uc.mem_write(self.buf, b'\0' * 0x40)
        w(m + 0x10c, v[K['clear']])
        self.uc.mem_write(m + 8, bytes([v[K['active']] & 0xff]))
        w(m + 0x59c, v[K['anim_state']])
        act = v[K['action']]
        w(m + 0x494, 0 if act < 0 else 1), w(self.node + 8, act if act >= 0 else 0)
        w(m + 0x5d0, v[K['limp']])
        w(self.power, 1 if v[K['power']] else 0)
        self.uc.mem_write(m + 0xbc, bytes([v[K['xbc']] & 0xff]))
        self.uc.mem_write(self.ams, bytes([1 if v[K['ams']] == 2 else 0]))
        w(m + 0x400, self.ams if v[K['ams']] else 0)
        w(m + 0x598, v[K['masc']]), w(m + 0x594, v[K['narc']]), w(self.jump, v[K['jump']])
        self.uc.mem_write(m + 0x5ab, bytes([1 if v[K['crouched']] else 0]))
        self.light = light
        # mov ebp, m; mov esi, buf; xor edi, edi; mov ebx, parts; sub esp, 0x60; mov [esp+0x4c], shots; xor eax, eax;
        # jmp PACK_FROM
        code = (b'\xbd' + struct.pack('<I', m) + b'\xbe' + struct.pack('<I', self.buf) + b'\x31\xff' +
                b'\xbb' + struct.pack('<I', parts) + b'\x83\xec\x60' + b'\xc7\x44\x24\x4c' + struct.pack('<I', shots) +
                b'\x31\xc0' + b'\xb8' + struct.pack('<I', PACK_FROM) + b'\x50\x31\xc0\xc3')  # push PACK_FROM; ret
        thunk = self.z.alloc(64)  # new each time: Unicorn keeps the code it translated at an address
        self.uc.mem_write(thunk, code)
        self.z.cpu.call(thunk, count=1_000_000)
        return struct.unpack('<I', bytes(self.uc.mem_read(self.buf + 0x38, 4)))[0]


def port_flags(probe, v, light):
    probe.probe_mech_net_light(light)
    out = (I32 * 8192)()
    n = probe.probe_mech_net_send((I32 * COUNT)(*v), out, 8192)
    log = list(out[:n])
    i = 0
    while i < len(log):  # each send: 1, to, guaranteed, size, then its words
        if log[i] == 1 and i + 4 < len(log) and (log[i + 4] & 0xffff) == 0xf:
            return log[i + 4 + 14] & 0xffffffff
        i += 1
    return None


APPLY = 0x4705e0
# The apply's callees and the port's calls they stand for (mech_net's mech_do by MW3's address, or our log codes).
POWER, MASC, NARC, CROUCH, LIGHT_ON, LIGHT_TURN = 0x41dab0, 0x41eec0, 0x41b9d0, 0x41c9f0, 0x41f920, 0x41f8d0
STAND, LIMP, ANIM, RESPAWN = 0x4164c0, 0x41e3b0, 0x417290, 0x470d40
RX = dict(mode=0, len=1, msg=2, flags=66, power=94, masc=95, narc=96, crouched=97, anim=98, ams=99, count=112)


class Apply:
    """PM's apply on an inactive state, its callees logged as the port's calls."""

    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        self.mech, self.msg = z.alloc(0x900), z.alloc(0x40)
        self.vt, self.ctl, self.tbl, self.ams = z.alloc(0x200), z.alloc(0x20), z.alloc(0x100), z.alloc(4)
        self.power = z.alloc(4)
        w = self.w
        w(self.mech, self.vt)
        power_fn = z.alloc(8)
        self.uc.mem_write(power_fn, b'\xa1' + struct.pack('<I', self.power) + b'\xc3')  # mov eax, [power]; ret
        w(self.vt + 0xac, power_fn)
        w(self.mech + 0x38c, self.ctl), w(self.ctl + 4, self.tbl), w(self.tbl + 4, 0)
        jump_fn = z.alloc(4)
        z.native_stub(jump_fn, lambda u: self.log.append(('jump', u.arg(0))), 4)
        w(self.tbl + 0x34, jump_fn)
        self.log, self.light = [], 0
        z.native_stub(POWER, lambda u: self.log.append((10, 0x41cc30, u.arg(0))), 4)
        z.native_stub(MASC, lambda u: self.log.append((10, 0x41df50, u.arg(0) & 1)), 4)
        z.native_stub(NARC, lambda u: self.log.append((10, 0x41ac10, u.arg(0) & 1)), 4)
        z.native_stub(CROUCH, lambda u: self.log.append((10, 0x41bda0, u.arg(0) & 1)), 4)
        z.native_stub(LIGHT_ON, lambda u: self.light)
        z.native_stub(LIGHT_TURN, lambda u: self.log.append((17, u.arg(0) & 1)), 4)
        z.native_stub(STAND, lambda u: self.log.append((10, 0x415a90, 0)))
        z.native_stub(LIMP, lambda u: self.log.append((10, 0x41d5f0, u.arg(0) & 1)), 8)
        z.native_stub(ANIM, lambda u: self.log.append((10, 0x416780, u.arg(0))), 8)
        z.native_stub(RESPAWN, lambda u: self.log.append((16,)), 4)

    def w(self, a, v):
        self.uc.mem_write(a, struct.pack('<I', v & 0xffffffff))

    def run(self, flags, power, masc, narc, crouched, anim, ams, light):
        w, m = self.w, self.mech
        self.log, self.light = [], light
        msg = bytearray(0x3c)
        struct.pack_into('<HH', msg, 0, 0xf, 0x3c)
        struct.pack_into('<I', msg, 0x38, flags)
        self.uc.mem_write(self.msg, bytes(msg))
        w(self.power, 1 if power else 0)
        w(m + 0x400, self.ams if ams else 0)
        self.uc.mem_write(self.ams, bytes([1 if ams == 2 else 0]))
        w(m + 0x598, masc), w(m + 0x594, narc)
        self.uc.mem_write(m + 0x5ab, bytes([1 if crouched else 0]))
        w(m + 0x59c, anim)
        self.z.call(APPLY, ecx=m, args=(self.msg,))
        ams_after = self.uc.mem_read(self.ams, 1)[0] if ams else -1
        jump = [e[1] for e in self.log if e[0] == 'jump']
        calls = [e for e in self.log if e[0] != 'jump']
        return calls, ams_after, (jump[-1] if jump else None)


def port_apply(probe, flags, power, masc, narc, crouched, anim, ams, light):
    probe.probe_mech_net_light(light)
    v = [0] * RX['count']
    v[RX['mode']], v[RX['len']] = 0, 0x3c
    msg = bytearray(0x3c + 4 * 64 - 0x3c)
    struct.pack_into('<HH', msg, 0, 0xf, 0x3c)
    struct.pack_into('<I', msg, 0x38, flags)
    for k in range(64):
        v[RX['msg'] + k] = struct.unpack_from('<i', msg, 4 * k)[0] if 4 * k + 4 <= len(msg) else 0
    v[RX['flags']] = 0
    v[RX['power']], v[RX['masc']], v[RX['narc']] = power, masc, narc
    v[RX['crouched']], v[RX['anim']], v[RX['ams']] = crouched, anim, ams
    out = (I32 * 4096)()
    n = probe.probe_mech_net_apply((I32 * RX['count'])(*v), out, 4096)
    log = list(out[:n])
    calls, i = [], 0
    while i < len(log) and log[i] != 90:
        if log[i] == 10:
            calls.append((10, log[i + 1], log[i + 2]))
            i += 3
        elif log[i] == 17:
            calls.append((17, log[i + 1]))
            i += 2
        elif log[i] == 16:
            calls.append((16,))
            i += 4
        else:
            i += 1
    j = log.index(91)
    return calls, log[j + 3], log[j + 6] & 0xffffffff


def run_applies(probe, count, rng):
    a = Apply()
    seen = {}
    for i in range(count):
        flags = rng.getrandbits(32) | 2  # inactive: the motion's half skipped
        anim = rng.choice([0, 0, 1, 5, 9, 10, 12, 13, rng.randrange(16)])
        state = dict(power=rng.randrange(4), masc=f2u(rng.choice([0.0, 2.5])), narc=f2u(rng.choice([0.0, 3.0, -1.0])),
                     crouched=rng.randrange(2), anim=anim, ams=rng.randrange(3), light=rng.randrange(2))
        theirs = a.run(flags, **state)
        mine = port_apply(probe, flags, **state)
        if theirs != mine:
            print(f'apply case {i}: flags {flags:#010x} {state}\n  exe  {theirs}\n  port {mine}')
            sys.stdout.flush()
            os._exit(1)
        for e in theirs[0]:
            seen[e[:2]] = seen.get(e[:2], 0) + 1
    print(f'apply: {count} states identical; calls seen {sorted(seen)}')


def main():
    probe = test_pm_zar.load_pm_probe()
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    rng = random.Random(1)
    pack = Pack()
    done = 0
    seen = [0] * 32
    for i in range(count):
        v = case(rng)
        v[K['kind']] = 0xf
        v[K['anim_state']] = rng.randrange(0, 32)  # PM keeps 4 bits of it
        light = rng.randrange(2)
        mine = port_flags(probe, v, light)
        if mine is None:
            continue
        shots, parts = mine >> 11 & 0x7f, mine >> 18 & 0xf
        theirs = pack.run(v, shots, parts, light)
        if mine != theirs:
            print(f'builder case {i}: exe {theirs:#010x} port {mine:#010x} (differ {theirs ^ mine:#010x})')
            sys.stdout.flush()
            os._exit(1)
        for b in range(32):
            seen[b] += theirs >> b & 1
        done += 1
    never = [b for b in range(32) if not seen[b] and b not in (14, 15, 16, 17, 27, 28)]
    print(f'builder: {done} words identical; bits never set: {never or "none"} (27, 28 unused; 14..17 the shot '
          'counts over 7, which the send probe\'s five shots do not reach: the field shows in 11..13)')
    if never:
        os._exit(1)
    run_applies(probe, count, rng)
    sys.stdout.flush()
    os._exit(0)


if __name__ == '__main__':
    main()

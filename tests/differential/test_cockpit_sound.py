"""The cockpit sounds against the port (docs/cockpit-sounds.md): the table's
load 0x004141a0 (30 names through zSound's find 0x0056f6f0), the play
0x0041e770, the stop 0x0041e7a0 (with and without the fade), the fade
0x0041e7e0 and the voice 0x0041e800. zSound's calls are stubbed alike on
both sides and logged; random sequences, with some names missing, compare
the table, the voices and the log after every call.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402

SOUNDS, VOICES = 0x605598, 0x605520
NAMES = ['mech_missilelock', 'damaged', 'destroyed', 'mech_chit_main', 'mech_chit_head', 'mech_chit_ltorso',
         'mech_chit_torso', 'mech_chit_rtorso', 'mech_chit_larm', 'mech_chit_rarm', 'mech_chit_lleg',
         'mech_chit_rleg', 'mech_acquirelock', 'mech_low_ammo', 'mech_no_ammo', 'ammo_explosion', 'ammo_jettison',
         'mech_flush_coolant', 'coolant_depleted', 'ecm_engaged', 'single_fire_engaged', 'group_fire_engaged',
         'linked_fire_engaged', 'mech_friendly_fire', 'snd_gyro_overload', 'snd_gyro_ok', 'mech_heat_critical',
         'masc_engaged', 'tc_online', 'mech_traitor']

probe = load_probe()
probe.cs_open.argtypes = [ctypes.c_void_p, ctypes.c_int32]
probe.cs_play.argtypes = [ctypes.c_int32, ctypes.c_float]
probe.cs_stop.argtypes = [ctypes.c_int32, ctypes.c_int32]
probe.cs_fade.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_float]
probe.cs_voice.argtypes = [ctypes.c_int32]
probe.cs_state.argtypes = [ctypes.c_void_p]


def bits(f):
    return struct.unpack('<i', struct.pack('<f', f))[0]


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Emu:
    def __init__(self):
        o = self.o = Original()
        self.found, self.log, self.next_voice = [], [], 0x1000

        def cstr(a):
            b = o.read(a, 64)
            return b[:b.index(b'\0')].decode()

        def find(c):
            n = cstr(c.ecx)
            return 0x100 + self.found.index(n) if n in self.found else 0

        def play(c):
            self.log += [1, s32(c.ecx), s32(c.arg(0)), 0]
            assert c.edx == 0 and c.arg(1) == 0 and c.arg(2) == 0xbf800000
            v = self.next_voice
            self.next_voice += 1
            return v

        def stop(c):
            self.log += [2, s32(c.ecx), 0, 0]
            return 0

        def fade(c):
            self.log += [3, s32(c.ecx), s32(c.edx), s32(c.arg(0))]
            return 0 if c.edx else c.ecx
        o.stub_function(0x56f6f0, find)
        o.stub_function(0x4e6d80, play, 0xc)
        o.stub_function(0x56e9f0, stop)
        o.stub_function(0x571d20, fade, 4)

    def open(self, found):
        self.found, self.log, self.next_voice = found, [], 0x1000
        self.o.write(SOUNDS, bytes(120))
        self.o.write(VOICES, bytes(120))

    def u(self, a):
        return struct.unpack('<I', self.o.read(a, 4))[0]

    def load(self):
        self.o.call(0x4141a0)

    def play(self, i, vol):
        self.o.call(0x41e770, ecx=i, stack_args=(bits(vol) & 0xffffffff, 0xbf800000))

    def stop(self, i, fade):
        self.o.call(0x41e7a0, ecx=i, edx=fade)

    def fade(self, i, stop, level):
        self.o.call(0x41e7e0, ecx=i, edx=stop, stack_args=(bits(level) & 0xffffffff,))

    def voice(self, i):
        self.o.call(0x41e800, ecx=i)
        from unicorn.x86_const import UC_X86_REG_EAX
        return s32(self.o.uc.reg_read(UC_X86_REG_EAX))

    def state(self):
        return ([s32(self.u(SOUNDS + 4 * i)) for i in range(30)] + [s32(self.u(VOICES + 4 * i)) for i in range(30)]
                + self.log)


class Port:
    def __init__(self):
        self.buf = (ctypes.c_int32 * 65536)()

    def open(self, found):
        self.keep = [n.encode() for n in found]
        arr = (ctypes.c_char_p * max(len(found), 1))(*self.keep)
        self.arr = arr
        probe.cs_open(arr, len(found))

    def load(self):
        probe.cs_load()

    def play(self, i, vol):
        probe.cs_play(i, vol)

    def stop(self, i, fade):
        probe.cs_stop(i, fade)

    def fade(self, i, stop, level):
        probe.cs_fade(i, stop, level)

    def voice(self, i):
        return probe.cs_voice(i)

    def state(self):
        k = probe.cs_state(self.buf)
        return list(self.buf[:k])


def main():
    n = int(os.environ.get('COCKPIT_SOUND_CASES', '200'))
    rng = random.Random(0x41e770)
    emu, port = Emu(), Port()
    failed = 0
    counts = {'play': 0, 'stop': 0, 'stop_fade': 0, 'fade': 0, 'voice': 0, 'missing': 0}
    for case in range(n):
        found = [x for x in NAMES if rng.random() < 0.85] + ['not_a_cockpit_sound']
        rng.shuffle(found)
        counts['missing'] += 31 - len(found)
        for s in (emu, port):
            s.open(found)
            s.load()
        ops = []
        for _ in range(rng.randrange(1, 30)):
            i = rng.randrange(30)
            k = rng.random()
            if k < 0.45:
                op = ('play', i, rng.choice((1.0, 0.5, 0.0)))
            elif k < 0.7:
                op = ('stop', i, rng.choice((0, 0, 1)))
                counts['stop_fade' if op[2] else 'stop'] += 1
            elif k < 0.85:
                op = ('fade', i, rng.choice((0, 1)), rng.choice((0.0, 0.5)))
            else:
                op = ('voice', i)
            counts[op[0]] += op[0] != 'stop'
            want = getattr(emu, op[0])(*op[1:])
            got = getattr(port, op[0])(*op[1:])
            ops.append(op)
            ws, gs = emu.state(), port.state()
            if want != got or ws != gs:
                failed += 1
                if failed <= 5:
                    print(f'FAIL case {case} after {op}: {want} vs {got}\n  want {ws}\n  got  {gs}\n  ops {ops}')
                break
    probe.cs_close()
    print(f'ok: {n} sequences identical (the table loaded with {counts["missing"]} names missing; '
          f'{counts["play"]} plays, {counts["stop"]} stops, {counts["stop_fade"]} faded stops, {counts["fade"]} fades, '
          f'{counts["voice"]} voices asked)' if not failed else f'{failed} of {n} differ')
    sys.stdout.flush()
    os._exit(1 if failed else 0)


main()

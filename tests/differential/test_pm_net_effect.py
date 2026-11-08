"""Pirate's Moon's effect animations over the network (0x1a) against the port (docs/pm-map.md, the in-game types):
the sender 0x471f50 (MW3's 0x46feb0) and the handler 0x471fd0 (MW3's 0x46ff50), through probe_mech_net_world's
kinds 2 and 3 as test_mech_net_world does MW3's. The handler is MW3's: the log's lookup (0x524d70, MW3's 0x50b680)
first, a replay (0x5261e0) under the echo flag [0x629a89] only if none like it is logged. The sender drops MW3's
check for the field base stations: PM sends an "mfb_station_01" record as any other. Stood in for on both sides:
the lookup, the replay; the record's size (0x526170) runs.

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_net_effect.py
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_EXE'] = 'pm'
import test_pm_zar  # noqa: E402

I32 = ctypes.c_int32
SENT, HEARD, LOOKUP, REPLAY, NET_SEND, ECHO = 0x471f50, 0x471fd0, 0x524d70, 0x5261e0, 0x5a97d0, 0x629a89
MALLOC, FREE = 0x5de664, 0x5de65c

# probe_mech_net_world's input (test_mech_net_world.py's, copied: that file loads MW3's probe).
NAMES = ['kind', 'host', 'local', 'len', ('msg', 24), ('order', 10), ('defaults', 10), 'pilot_shot', 'made', 'has',
         'has_game', 'end_state', 'end_time', 'clock']
K, _i = {}, 0
for n in NAMES:
    name, size = (n, 1) if isinstance(n, str) else n
    K[name], _i = _i, _i + size
COUNT = _i


def s32(v):
    return struct.unpack('<i', struct.pack('<I', v & 0xffffffff))[0]


class Exe:
    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        for slot, fn in ((MALLOC, lambda c: z.alloc(c.arg(0))), (FREE, lambda c: 0)):  # PM's own IAT slots
            f = z.cpu.code(b'\xc3')
            self.uc.mem_write(slot, struct.pack('<I', f))
            z.native_stub(f, fn)
        z.native_stub(NET_SEND, self.send)
        z.native_stub(LOOKUP, self.has)
        z.native_stub(REPLAY, self.replay)

    def send(self, c):
        buf, n, guaranteed, to = (c.arg(i) for i in range(4))
        data = bytes(self.uc.mem_read(buf, n)) + bytes(4)
        self.log += [1, s32(to), 1 if guaranteed else 0, n]
        self.log += [struct.unpack('<i', data[k:k + 4])[0] for k in range(0, n, 4)]
        return 1

    def has(self, c):
        self.log.append(44)
        return self.v[K['has']]

    def replay(self, c):
        self.log += [43, self.uc.mem_read(ECHO, 1)[0]] + list(struct.unpack('<20i', self.uc.mem_read(c.arg(0), 0x50)))
        return 0

    def run(self, v):
        self.v, self.log = v, []
        self.uc.mem_write(ECHO, bytes([v[K['made']] == 2 and v[K['kind']] == 2]))  # the probe's echo on, for a send
        msg = self.z.alloc(0x100)
        self.uc.mem_write(msg, struct.pack('<24i', *v[K['msg']:K['msg'] + 24])[:v[K['len']]])
        self.z.call(SENT if v[K['kind']] == 2 else HEARD, args=(msg,))
        self.log += [90, v[K['end_state']], v[K['end_time']], self.uc.mem_read(ECHO, 1)[0]]
        return self.log


def case(rng):
    v = [0] * COUNT
    kind = v[K['kind']] = rng.choice([2, 3])
    v[K['local']] = rng.randrange(1, 9)
    v[K['has']] = rng.randrange(2)
    v[K['made']] = rng.choice([0, 0, 2])  # 2: a send while one heard replays (the echo flag up)
    v[K['has_game']] = 1
    rtype = rng.choice([1, 2, 3, 4, 5])
    name = rng.choice([b'boom_small', b'mfb_station_01_door', b'fire', b'x_mfb_station_01', b'mfb_station_01'])
    # the node and the connector's -1, so the port's FIX (A) liveness check passes on both
    rec = struct.pack('<iI32si9i', rtype, rng.getrandbits(32), name.ljust(32, b'\0'), -1,
                      *([-1, rng.getrandbits(31), rng.getrandbits(31), rng.getrandbits(31), -1] +
                        [rng.getrandbits(31) for _ in range(4)]))
    msg = rec if kind == 2 else struct.pack('<HH', 0x1a, 0x54) + rec
    v[K['len']] = len(msg)
    words = struct.unpack(f'<{(len(msg) + 3) // 4}i', msg + bytes(-len(msg) % 4))
    v[K['msg']:K['msg'] + len(words)] = words
    return v


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_mech_net_world.argtypes = [ctypes.POINTER(I32), ctypes.POINTER(I32), I32]
    assert COUNT == probe.probe_mech_net_wd_count(), (COUNT, probe.probe_mech_net_wd_count())
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    exe = Exe()
    rng = random.Random(7)
    seen = {}
    for i in range(count):
        v = case(rng)
        theirs = exe.run(v)
        out = (I32 * 4096)()
        n = probe.probe_mech_net_world((I32 * COUNT)(*v), out, 4096)
        mine = list(out[:n])
        if mine != theirs:
            print(f'case {i} kind {v[K["kind"]]}:\n  exe  {theirs}\n  port {mine}', flush=True)
            os._exit(1)
        name = struct.pack('<8i', *v[K['msg'] + (2 if v[K['kind']] == 2 else 3):][:8]).split(b'\0')[0]
        key = (v[K['kind']], b'mfb_station_01' in name, v[K['made']] == 2, theirs[0])
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} effects identical; (kind, a station\'s, echo up, first call): {dict(sorted(seen.items()))}', flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()

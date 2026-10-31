"""The mission map's pick 0x004587b0 (MissionMap::pick) against the
original x86: the lance command panel's click on the map (0x00469d00) turns
the cursor's pixel into a world spot, x and z back from the map's offset by
its metres a pixel, and asks the ground under it from 1000 m up (0x004edab0,
stubbed on both sides alike); a hit gives the spot's height and the hit's
record, whose node (+0x24) the panel orders at. Random maps, pixels, a world
or none ([0x00608c84]) and the ground's answers; compared: the answer, the
spot as written (from a sentinel), the node and the ground's arguments.

    cmake --build build && .venv/bin/python -P tests/differential/test_map_pick.py

BREAK=1 swaps x and z in the port's spot (its probe), which must fail.
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_weiler import Emulated, probe  # noqa: E402

F3 = ctypes.c_float * 3
probe.probe_map_pick.argtypes = [ctypes.c_float, F3, ctypes.c_float, ctypes.c_float, ctypes.c_int, ctypes.c_int,
                                 ctypes.c_int, ctypes.c_float, ctypes.c_uint32, F3, ctypes.POINTER(ctypes.c_uint32), F3]
PICK, GROUND, WORLD = 0x4587b0, 0x4edab0, 0x608c84
SENTINEL = 12345.5


def fbits(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


def bitsf(u):
    return struct.unpack('<f', struct.pack('<I', u & 0xffffffff))[0]


def main():
    brk = os.environ.get('BREAK') == '1'
    emu = Emulated()
    cpu = emu.cpu
    rng = random.Random(1999)
    case = {}

    def ground(c):  # 0x004edab0: ECX the world node, EDX the hits; x, y, z on the stack
        case['asked'] = [bitsf(c.arg(i)) for i in range(3)]
        case['calls'] = case.get('calls', 0) + 1
        if case['ret'] == 0:
            hits = c.edx
            cpu.write(hits, bytes([case['count']]))
            cpu.write(hits + 4 + 0x10, struct.pack('<f', case['height']))
            cpu.u32(hits + 4 + 0x24, case['node'])
        return case['ret']
    cpu.stub_function(GROUND, ground, 12)
    world_obj = emu.alloc(0x20)
    cpu.u32(world_obj + 4, 0x2c000000)  # the world node, passed on in ECX
    fails = runs = hits = 0
    for n in range(3000):
        emu.begin()
        mpp = rng.choice([0.5, 1.0, 2.5, 3.125, 7.8, rng.uniform(0.1, 12.0)])
        off = [rng.uniform(-3000, 3000) for _ in range(3)]
        px, py = rng.uniform(-10, 650), rng.uniform(-10, 650)
        world = rng.random() < 0.85
        case.clear()
        case.update(ret=rng.choice([0, 0, 0, 1]), count=rng.choice([0, 1, 1, 3]), height=rng.uniform(-200, 300),
                    node=rng.randrange(1, 1 << 31))
        # The original: the map (ECX) with mpp at +0xd0, the offset at +0xc4.
        m = emu.alloc(0xdc)
        cpu.write(m + 0xd0, struct.pack('<f', mpp))
        cpu.write(m + 0xc4, struct.pack('<3f', *off))
        place, rec = emu.alloc(12), emu.alloc(0x28)
        cpu.write(place, struct.pack('<3f', SENTINEL, SENTINEL, SENTINEL))
        cpu.u32(WORLD, world_obj if world else 0)
        theirs = cpu_ret = None
        cpu.call(PICK, ecx=m, stack_args=(fbits(px), fbits(py), place, rec))
        cpu_ret = cpu.uc.reg_read(19) & 0xff  # UC_X86_REG_EAX (al)
        t_place = list(struct.unpack('<3f', cpu.read(place, 12)))
        t_node = struct.unpack('<I', cpu.read(rec + 0x24, 4))[0] if cpu_ret else 0
        t_asked, t_calls = case.get('asked', [0.0, 0.0, 0.0]), case.get('calls', 0)
        # The port.
        p_place, p_node, p_asked = F3(SENTINEL, SENTINEL, SENTINEL), ctypes.c_uint32(0), F3()
        r = probe.probe_map_pick(mpp, F3(*off), px, py, int(world), case['ret'], case['count'], case['height'],
                                 case['node'], p_place, ctypes.byref(p_node), p_asked)
        mine_place = list(p_place)
        if brk:
            mine_place[0], mine_place[2] = mine_place[2], mine_place[0]
        p_ret, p_calls = r & 1, r >> 1
        ok = (p_ret == cpu_ret and p_calls == t_calls and [fbits(v) for v in mine_place] == [fbits(v) for v in t_place]
              and p_node.value == t_node and (not t_calls or [fbits(v) for v in p_asked] == [fbits(v) for v in t_asked]))
        runs += 1
        hits += cpu_ret
        if not ok:
            fails += 1
            if fails <= 5:
                print(f'FAIL case {n}: mpp {mpp} off {off} pixel {px:.2f},{py:.2f} world {world} ground {case}')
                print(f'  original: {cpu_ret} {t_place} node {t_node:#x} asked {t_asked} ({t_calls})')
                print(f'  port:     {p_ret} {mine_place} node {p_node.value:#x} asked {list(p_asked)} ({p_calls})')
    if fails:
        print(f'{fails} of {runs} differ')
        sys.stdout.flush(); os._exit(1)
    print(f'ok: {runs} picks identical bit for bit ({hits} hits, the rest no world, no ground or no polygon)')
    sys.stdout.flush(); os._exit(0)


if __name__ == '__main__':
    main()

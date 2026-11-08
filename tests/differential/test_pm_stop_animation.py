"""PM's STOP_ANIMATION anim event (pm/gamez/zeffect/zeff_anim_events, 0x51f900) against PM's exe: the names of every
shipped event-25 record in PM's anim.zbd files, each looked up in a calling def's anim refs (by either of a ref's
two names; the ref's started def stopped) and else in the defs (index 0 never stopped), with the two cached indices
unset, set to a ref or set to a def. Compared: which def the stop reached (0x51f830 stubbed) and the event's bytes.

    cmake --build build --target pmprobe
    .venv/bin/python -P tests/differential/test_pm_stop_animation.py [cases]
"""
import collections
import ctypes
import glob
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import pm_floors  # noqa: E402
import test_pm_gamez as g  # noqa: E402
import test_pm_zar  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
EVENT, STOP, DEFS, COUNT, DEF_SIZE = 0x51f900, 0x51f830, 0x8d3f0c, 0x8d3f0a, 0x10c


def shipped_names():
    out = []
    for f in sorted(glob.glob(str(ROOT / 'game' / 'pm' / 'zbd' / '*' / 'anim.zbd'))):
        d = open(f, 'rb').read()
        for m in re.finditer(rb'\x19[\x01\x02\x03]\x00\x00\x30\x00\x00\x00', d):
            name = d[m.start() + 12:m.start() + 44].split(b'\0')[0]
            if name and all(32 < c < 127 for c in name):
                out.append(name)
    return out


def shown(recs):
    z = bytes(1)
    return [(r[:0x20].split(z)[0], r[0x20:0x40].split(z)[0]) for r in recs]


def main():
    cases = int(sys.argv[1]) if len(sys.argv) > 1 else 6000
    p = test_pm_zar.load_pm_probe()
    p.probe_pm_stop_animation.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p,
                                          ctypes.POINTER(ctypes.c_int32), ctypes.c_int]
    names = shipped_names()
    emu = g.Emulated()
    seen = []
    emu.native_stub(STOP, lambda c: seen.append(c.arg(0)) or 0)
    defs, caller, refs, ev = emu.alloc(DEF_SIZE * 24), emu.alloc(0x140), emu.alloc(0x48 * 12), emu.alloc(0x30)
    emu.w32(DEFS, defs), emu.w32(caller + 0x104, refs)
    rng = random.Random(0x51f900)
    kinds = collections.Counter()
    for n in range(cases):
        want_name = rng.choice(names)
        pool = list({rng.choice(names) for _ in range(30)} - {want_name})
        ndefs = rng.randrange(1, 24)
        dnames = [rng.choice(pool) for _ in range(ndefs)]
        if rng.random() < 0.7:
            for _ in range(rng.choice((1, 1, 2))):
                dnames[rng.randrange(ndefs)] = want_name
        nrefs = rng.randrange(0, 12)
        recs, started = [], []
        for i in range(nrefs):
            a = rng.choice(pool) if rng.random() < 0.7 else want_name
            b = rng.choice(pool + [b''] * 10) if rng.random() < 0.8 else want_name
            r = bytearray(0x48)
            r[0:len(a)] = a[:0x1f]
            r[0x20:0x20 + len(b)] = b[:0x1f]
            recs.append(bytes(r))
            started.append(rng.choice((-1, rng.randrange(ndefs))))
        e = bytearray(0x30)
        struct.pack_into('<BBHIf', e, 0, 25, 1, 0, 0x30, 0.0)
        e[0xc:0xc + len(want_name)] = want_name
        cached = rng.choice(('none', 'none', 'none', 'ref', 'def', 'def0'))
        c2c, c2e = -1, -1
        if cached == 'ref' and nrefs:
            c2c = rng.randrange(nrefs)
        elif cached == 'def':
            c2e = rng.randrange(ndefs)
        elif cached == 'def0':
            c2e = 0
        struct.pack_into('<hh', e, 0x2c, c2c, c2e)
        # the exe
        for i, nm in enumerate(dnames):
            emu.cpu._uc.mem_write(defs + i * DEF_SIZE, nm.ljust(0x20, b'\0'))
        emu.cpu._uc.mem_write(COUNT, struct.pack('<h', ndefs))
        emu.cpu._uc.mem_write(caller + 0xe2, bytes([nrefs]))
        for i, r in enumerate(recs):
            emu.cpu._uc.mem_write(refs + i * 0x48, r)
            emu.w32(refs + i * 0x48 + 0x44, defs + started[i] * DEF_SIZE if started[i] >= 0 else 0)
        emu.cpu._uc.mem_write(ev, bytes(e))
        seen.clear()
        emu.call(EVENT, args=[caller, 0, ev])
        want = -1
        if seen and seen[0]:
            want = (seen[0] - defs) // DEF_SIZE
        want_e = emu.read(ev, 0x30)
        # the port
        got_e = ctypes.create_string_buffer(bytes(e), 0x30)
        got = p.probe_pm_stop_animation(got_e, b''.join(nm.ljust(0x20, b'\0')[:0x20] for nm in dnames), ndefs, b''.join(recs) or b'\0',
                                        (ctypes.c_int32 * max(nrefs, 1))(*(started or [0])), nrefs)
        where = f'case {n}: {want_name!r}, {ndefs} defs, {nrefs} refs, cached {cached}'
        if got != want:
            raise SystemExit(f'{where}: stopped {got}, the exe {want} (calls {[hex(x) for x in seen]}); event exe {want_e[0x2c:0x30].hex()} port {got_e.raw[0x2c:0x30].hex()}; started {started}; refs {shown(recs)}')
        if got_e.raw != want_e:
            raise SystemExit(f'{where}: the event {got_e.raw[0x2c:0x30].hex()}, the exe {want_e[0x2c:0x30].hex()}')
        c2c_after = struct.unpack_from('<h', want_e, 0x2c)[0]
        kinds['by a ref'] += c2c_after >= 0 and want >= 0
        kinds['by a def'] += c2c_after < 0 and want > 0
        kinds['def 0 kept'] += c2c_after < 0 and struct.unpack_from('<h', want_e, 0x2e)[0] == 0
        kinds['ref with nothing started'] += c2c_after >= 0 and want < 0
        kinds['none'] += c2c_after < 0 and struct.unpack_from('<h', want_e, 0x2e)[0] < 0
        kinds['second name'] += c2c_after >= 0 and cached == 'none' and recs[c2c_after][:len(want_name) + 1] != want_name + b'\0'
    pm_floors.floors(cases=(cases, 6000), names=(len(set(names)), 50), **{k.replace(' ', '_'): (v, 100) for k, v in kinds.items()})
    print(f'ok: {cases} stops identical over {len(names)} shipped records ({len(set(names))} names); {dict(kinds)}')


if __name__ == '__main__':
    main()

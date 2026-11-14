"""Where Pirate's Moon's Direct3D triangle stripper 0x570b70 could act on shipped data (docs/pm-map.md, the models'
form), measured in the emulator on PM's exe.

    .venv/bin/python -P tools/analysis/pm_strip_reach.py [mechlib|reread]

mechlib: every mechlib.zbd tree loaded (test_pm_mechlib's bench), then 0x570b70 run as the post-pass 0x510ab0 runs it
(models without 0x80, model, 0x7fffffff, 0x7fffffff) and each model compared before and after.
reread: each world's type-0 models with polygons (what the crater reset's re-read 0x558440 hands it, with 0x7fffffff,
4 and no 0x80 test) counted, and 0x570b70 run on them; the type-0 path needs more of the renderer than this bench
has (it faults after its image load 0x5329e0, stubbed), so crashes are counted, not compared.
"""
import collections
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tests' / 'differential'))
import test_pm_gamez as g  # noqa: E402
import test_pm_mechlib as lib  # noqa: E402
import test_pm_strips as strips  # noqa: E402


def mechlib():
    emu = lib.Emulated()
    data = lib.MECHLIB.read_bytes()
    names = [n for n, _, _ in lib.entries(data) if n not in ('version', 'format', 'materials')]
    emu.native_stub(0x5329e0, lambda c: 0)
    emu.run(None, lib.MECHLIB, names)
    used = [s for s in strips.model_slots(emu) if emu.i32(s + 0x10) > 0]
    marked = sum(1 for s in used if emu.read(s + 8, 1)[0] & 0x80)
    print(f'{len(names)} trees: {len(used)} models with polygons, {marked} with 0x80; types '
          f'{dict(collections.Counter(emu.i32(s) for s in used))}')
    given = [s for s in used if not emu.read(s + 8, 1)[0] & 0x80]
    emu.cpu.stubs.pop(g.STRIPS, None)
    changed = 0
    for s in given:
        before = strips.slot_dump(emu, s)
        emu.call(g.STRIPS, args=[s, 0x7fffffff, 0x7fffffff])
        changed += before != strips.slot_dump(emu, s)
    print(f'the post-pass gives it {len(given)}; {changed} changed')


def reread():
    emu = g.Emulated()
    for w in g.WORLDS:
        emu.stack([g.ZBD / w / 'gamez.zbd'], 5, 6, 5, models=True)
        emu.cpu.stubs.pop(g.STRIPS, None)
        emu.native_stub(0x5329e0, lambda c: 0)
        typed0 = [s for s in strips.model_slots(emu) if emu.i32(s + 0x10) > 0 and emu.i32(s) == 0]
        marked = sum(1 for s in typed0 if emu.read(s + 8, 1)[0] & 0x80)
        changed = crashed = 0
        for s in typed0:
            before = strips.slot_dump(emu, s)
            try:
                emu.call(g.STRIPS, args=[s, 0x7fffffff, 4])
            except Exception:  # noqa: BLE001 - the bench's limit, counted
                crashed += 1
                continue
            changed += before != strips.slot_dump(emu, s)
        print(f'{w}: {len(typed0)} type-0 models with polygons ({marked} with 0x80); {changed} changed, '
              f'{crashed} faulted on the bench')


if __name__ == '__main__':
    which = sys.argv[1] if len(sys.argv) > 1 else 'mechlib'
    {'mechlib': mechlib, 'reread': reread}[which]()

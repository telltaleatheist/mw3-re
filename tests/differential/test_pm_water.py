"""Pirate's Moon's WATER switch (docs/fixes.md, The Upgrades switches; Pirate's Moon 0x418fc0's wading).

    cmake --build build --target pmprobe && .venv/bin/python -P tests/differential/test_pm_water.py

The port alone, through test_pm_gait's probe: a mech's speed target at chosen water depths over its target on dry
ground. Off (the exe's path, which test_pm_gait pins against the exe at depths either side of a tenth of the height
and of full depth): full speed to a tenth of the height, then a flat 60%. On: full speed at a tenth, falling evenly
to Pirate's Moon's own 60% at full depth, 60% deeper; no step where the wading starts.
"""
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import test_pm_gait as G  # noqa: E402

TARGET = 0x53c  # Mech::speed_target on Pirate's Moon (the factor is +0x538)
# depth as a share of the height: (off, on)
WANT = [(0.0, 1.0, 1.0), (0.05, 1.0, 1.0), (0.1, 1.0, 1.0), (0.1001, 0.6, 0.99996), (0.325, 0.6, 0.9), (0.55, 0.6, 0.8),
        (0.775, 0.6, 0.7), (1.0, 0.6, 0.6), (1.4, 0.6, 0.6), (2.0, 0.6, 0.6)]


def target(s, share):
    mech = s['mech']
    height = struct.unpack('<f', mech[0x68:0x6c])[0]
    feet = bytearray(s['feet'])
    for i in range(2):
        struct.pack_into('<f', feet, G.FOOT * i + 0xdc, G.f32(height * share))
    t = dict(s, feet=bytes(feet))
    dt = struct.unpack('<f', mech[0x6d4:0x6d8])[0]
    got, _ = G.port(0, t, dt)
    return struct.unpack('<f', got['mech'][TARGET:TARGET + 4])[0]  # pick() keeps the image's offsets


def main():
    defs = G.mechs()
    rng = random.Random(1999)
    used = 0
    for n in range(4000):
        s = G.scenario(rng, defs)
        before = struct.unpack('<f', s['mech'][TARGET:TARGET + 4])[0]
        G.probe.probe_zfix_water(0)
        dry = target(s, 0.0)
        if dry == before or dry == 0.0:  # in the air (the target is left alone), or nothing to scale
            continue
        used += 1
        for share, off, on in WANT:
            for switch, want in ((0, off), (1, on)):
                G.probe.probe_zfix_water(switch)
                got = target(s, share) / dry
                if abs(got - want) > 2e-4:
                    G.probe.probe_zfix_water(0)
                    raise SystemExit(f'scenario {n}: depth {share} of the height, WATER {"on" if switch else "off"}: '
                                     f'{got:.5f} of the dry speed, {want} wanted')
    G.probe.probe_zfix_water(0)
    if used < 2000:
        raise SystemExit(f'coverage: only {used} scenarios on the ground')
    print(f'ok: {used} mechs on the ground at {len(WANT)} depths: off a flat 60% past a tenth of the height, '
          f'on from full speed at a tenth to 60% at full depth')


if __name__ == '__main__':
    main()

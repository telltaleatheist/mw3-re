# Exploration (render-shadows, 2026-10-20): every light-state event (type 4,
# mech3ax LightStateMwC) in each mission's anim.zbd: how many set SUBDIVIDE
# (event flag 0x200, which 0x503589 hands to the setter 0x4fac70 that sets
# light flag 0x40, the projected lights of 0x547740), and their types.
import collections, struct, sys
sys.path.insert(0, __import__('os').path.dirname(__file__))
import anim_walk as aw

totals = collections.Counter()
cur = {}


def events(blob, st):
    o = 0
    while o + 12 <= len(blob):
        t, start, pad, size, time = struct.unpack_from('<BBHIf', blob, o)
        if size < 12:
            return
        if t == 4:
            d = blob[o + 12:o + size]
            name = d[:32].split(b'\0')[0].decode('latin-1')
            flags, active, ty, directional, saturated, subdivide = struct.unpack_from('<6I', d, 36)
            totals['events'] += 1
            if flags & 0x200:
                totals['subdivide flag'] += 1
                totals[f'subdivide={subdivide}'] += 1
                if subdivide:
                    print(f"  {cur['m']}: {name!r} t={time:.2f} flags {flags:#x} type {ty} range "
                          f"{struct.unpack_from('<2f', d, 92)}")
            totals[f'type {ty}'] += 1
        o += size


aw.events = events


class Stats(dict):
    """The counters anim_walk.walk() expects."""
    def __missing__(self, k):
        self[k] = collections.Counter() if k in ('ev_type', 'status', 'activation', 'prereq_type',
                                                 'info_+0x3c', 'info_gravity') else 0
        return self[k]


for m in aw.MISSIONS:
    cur['m'] = m
    aw.walk(m, Stats())
print(dict(totals))

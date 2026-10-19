# Exploration script (render map, 2026-10-19): prints every fog-state event
# (type 28, mech3ax FogStateC) in each mission's anim.zbd, for docs/drafts/render.md.
import collections, struct, sys
sys.path.insert(0, __import__('os').path.dirname(__file__))
import anim_walk as aw

cur = {}
def events(blob, st):
    o = 0
    while o + 12 <= len(blob):
        t, start, pad, size, time = struct.unpack_from('<BBHIf', blob, o)
        if size < 12: return
        if t == 28:
            d = blob[o + 12:o + size]
            flags, ty = struct.unpack_from('<II', d, 32)
            col = struct.unpack_from('<3f', d, 40); alt = struct.unpack_from('<2f', d, 52)
            rng = struct.unpack_from('<2f', d, 60)
            print(f"  {cur['m']} anim {cur['name']!r} t={time:.2f} flags {flags:#x} type {ty} "
                  f"colour {tuple(round(c, 3) for c in col)} altitude {alt} range {rng}")
        o += size
aw.events = events
class Stats(dict):
    """The counters anim_walk.walk() expects."""
    def __missing__(self, k):
        self[k] = collections.Counter() if k in ('ev_type', 'status', 'activation', 'prereq_type',
                                                 'info_+0x3c', 'info_gravity') else 0
        return self[k]

for m in aw.MISSIONS:
    cur['m'], cur['name'] = m, '?'  # the def name is not passed to events()
    aw.walk(m, Stats())

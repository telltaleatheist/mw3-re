# Exploration script (puffer port, 2026-10-20): every PufferState event (type
# 42, 0x5035b0) in every zbd/*/anim.zbd, with the fields the exe reads
# (docs/puffers.md), and every puffer ref. `--dump` prints each event.
import collections, struct, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from anim_walk import G, MISSIONS, LISTS, U, cstr  # noqa: E402

FLAGS = {0x1: 'translate', 0x2: 'at_node', 0x4: 'active', 0x8: 'local_vel', 0x10: 'world_vel',
         0x20: 'min_rand_vel', 0x40: 'max_rand_vel', 0x80: 'interval_type', 0x100: 'interval_value',
         0x200: 'size', 0x400: 'lifetime', 0x800: 'deviation', 0x1000: 'fade', 0x2000: 'growth',
         0x4000: 'textures', 0x8000: 'start_age', 0x10000: 'accel', 0x20000: 'friction',
         0x40000: 'colors', 0x80000: 'near_fade', 0x100000: 'wind_factor', 0x200000: 'number',
         0x400000: 'priority (never read)'}


def puffer_events(b):
    """Yield (mission def name, puffer refs, event data) for every type-42 event."""
    sig, ver, lcount = struct.unpack_from('<III', b, 0)
    o = 12 + lcount * 0x54
    info = b[o:o + 0x44]; o += 0x44
    dcount = U(info, 0x0a, '<h')
    for d in range(dcount):
        h = b[o:o + 0x13c]; o += 0x13c
        lists = {}
        for cnt_off, name, size in LISTS:
            n = h[cnt_off]
            lists[name] = [b[o + i * size:o + (i + 1) * size] for i in range(n)]
            o += n * size
        reset = b[o:o + 0x40]; o += 0x40
        blobs = []
        rsize = U(reset, 0x3c)
        if rsize > 0:
            blobs.append(b[o:o + rsize]); o += rsize
        for s in range(h[0x108]):
            sd = b[o:o + 0x40]; o += 0x40
            size = U(sd, 0x3c)
            if size > 0:
                blobs.append(b[o:o + size]); o += size
        for blob in blobs:
            e = 0
            while e < len(blob):
                t, _, _, size, _ = struct.unpack_from('<BBHIf', blob, e)
                if t == 42:
                    yield cstr(h[0:0x20]), lists, blob[e:e + size]
                e += size


def main():
    st = collections.Counter()
    vals = collections.defaultdict(collections.Counter)
    for m in MISSIONS:
        b = (G / m / 'anim.zbd').read_bytes()
        for name, lists, ev in puffer_events(b):
            st['events'] += 1
            st[f'size {len(ev)}'] += 1
            f = U(ev, 0x30, '<I')
            for bit, n in FLAGS.items():
                if f & bit:
                    st['flag ' + n] += 1
            vals['active'][U(ev, 0x38)] += 1
            vals['index'][U(ev, 0x2c)] += 1
            if f & 2: vals['node index'][U(ev, 0x34)] += 1
            if f & 0x200000: vals['number dword'][hex(U(ev, 0xc0, '<I'))] += 1
            if f & 0x180: vals['interval'][(U(ev, 0x84), round(U(ev, 0x88, '<f'), 3), f & 0x180)] += 1
            if f & 0x80000: vals['near fade'][(U(ev, 0xa8, '<f'), U(ev, 0xac, '<f'))] += 1
            if f & 0x1000: vals['fade'][(U(ev, 0xb0, '<f'), U(ev, 0xb4, '<f'))] += 1
            if f & 0x20000: vals['friction'][round(U(ev, 0xb8, '<f'), 3)] += 1
            if f & 0x100000: vals['wind factor'][round(U(ev, 0xbc, '<f'), 3)] += 1
            if f & 0x400: vals['lifetime'][(round(U(ev, 0x94, '<f'), 2), round(U(ev, 0x98, '<f'), 2))] += 1
            if f & 0x4000:
                ntex = sum(1 for i in range(6) if ev[0xcc + i * 0x24] != 0)
                vals['textures (timed, count)'][(U(ev, 0xc4), ntex)] += 1
            if f & 0x40000: vals['colors count'][U(ev, 0x1a0)] += 1
            if f & 0x2000: vals['growth count'][U(ev, 0x21c)] += 1
            if '--dump' in sys.argv:
                print(m, name, cstr(ev[0xc:0x2c]), hex(f))
        for cnt_off, lname, size in LISTS:
            pass
    for k, v in sorted(st.items()):
        print(f'{k}: {v}')
    for k, v in vals.items():
        print(f'{k}: {dict(v.most_common(12))}')


if __name__ == '__main__':
    main()

# Exploration script (subagent, 2026-10-19): what the post-load pass 0x50ae60
# (docs/drafts/anim-postload.md) would see in each shipped anim.zbd, from the
# file alone: flag bits, activation, the defs that reach each call.
import collections, io, contextlib, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from anim_walk import walk, MISSIONS, U, cstr  # noqa: E402

tot = collections.Counter()
for m in MISSIONS:
    st = collections.defaultdict(collections.Counter)
    with contextlib.redirect_stdout(io.StringIO()):
        _, _, _, defs = walk(m, collections.defaultdict(lambda: collections.Counter()) if False else
                             collections.defaultdict(int, {k: collections.Counter() for k in
                                                           ('ev_type', 'info_+0x3c', 'info_gravity', 'status',
                                                            'activation', 'prereq_type')}))
    c = collections.Counter()
    roots = collections.defaultdict(list)
    for i, r in enumerate(defs[1:], 1):
        h = r['h']
        f, status, act = U(h, 0x94, '<I'), h[0x98], h[0x99]
        if status in (4, 5):
            c['skipped_status_4_5'] += 1
            continue
        c['defs'] += 1
        for b in range(32):
            if f >> b & 1:
                c[f'flag_{1 << b:#x}'] += 1
        c[f'activation_{act}'] += 1
        c[f'priority_{h[0x9a]}'] += 1
        if f & 0x20 and status in (0, 1):
            c['reset_call_508310'] += 1
            rsize = U(h, 0x104)
            if rsize > 0:
                c['reset_call_with_events'] += 1
                c[f'reset_call_events_reset_state_{h[0xe8]}'] += 1
        if act in (0, 1, 2):
            roots[cstr(h[0x44:0x64])].append((i, act))
            if act in (0, 2) and f & 0x100000:
                c['weapon_cb_sets_node_flag_0x40'] += 1
            mh, ch, thr = (U(h, o, '<f') for o in (0xac, 0xb0, 0xb4))
            c['health_max_eq_cur' if mh == ch else 'health_max_ne_cur'] += 1
            if thr != 0:
                c['threshold_b4_nonzero'] += 1
            if mh == 0:
                c['damage_def_with_zero_health'] += 1
        if status != 2 and act == 4:
            c['start_call_508ae0'] += 1
        c['save_objects_507fc0_objrefs'] += h[0x109]
    shared = {k: v for k, v in roots.items() if len(v) > 1}
    c['damage_root_names_shared'] = len(shared)
    c['damage_defs_on_shared_root_names'] = sum(len(v) for v in shared.values())
    for v in shared.values():
        slots = collections.Counter()
        for _, a in v:
            if a in (1, 2): slots[1] += 1
            if a in (0, 2): slots[0] += 1
        c['same_name_same_slot_extra'] += sum(n - 1 for n in slots.values() if n > 1)
    print(m, dict(sorted(c.items())))
    tot.update(c)
print('total', dict(sorted(tot.items())))


def reset_event_types():
    """Event types in the reset sequences 0x508310 runs at load (flag 0x20)."""
    from anim_walk import G, LISTS
    types = collections.Counter()
    for m in MISSIONS:
        b = (G / m / 'anim.zbd').read_bytes()
        o = 12 + U(b, 8) * 0x54
        n = U(b[o:o + 0x44], 0x0a, '<h'); o += 0x44
        for d in range(n):
            h = b[o:o + 0x13c]; o += 0x13c
            for cnt, _, size in LISTS:
                o += h[cnt] * size
            o += 0x40
            rsize = U(h, 0x104)
            blob = b[o:o + max(rsize, 0)]; o += max(rsize, 0)
            if d and h[0x98] not in (4, 5) and U(h, 0x94, '<I') & 0x20:
                p = 0
                while p < len(blob):
                    types[blob[p]] += 1; p += U(blob, p + 4)
            for s in range(h[0x108]):
                size = U(b, o + 0x3c); o += 0x40 + max(size, 0)
    print('reset events by type:', dict(sorted(types.items())))


reset_event_types()

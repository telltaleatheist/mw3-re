# Exploration script (subagent, 2026-10-19): walks every zbd/*/anim.zbd in the
# order 0x50a0e0 reads it (docs/drafts/anim.md) and checks each ends exactly at
# the file size. `--emu` also runs the original 0x50a0e0 in Unicorn with the
# CRT and the node lookups stubbed, and checks it returns 0 having read every
# byte. A starting point for the anim port's tests.
import collections, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
G = ROOT / 'game' / 'mw3-1.2' / 'zbd'
MISSIONS = ['c1', 'c2', 'c3', 'c4', 'c4b', 't1']
U = lambda b, o, f='<i': struct.unpack_from(f, b, o)[0]
cstr = lambda b: b.split(b'\0')[0].decode('latin-1')

# def+0x109.. count byte -> (name, record size); the exe's read order
LISTS = [(0x109, 'objects', 0x60), (0x10a, 'nodes', 0x28), (0x10b, 'lights', 0x2c),
         (0x10c, 'puffers', 0x2c), (0x10d, 'dyn_sounds', 0x2c), (0x10e, 'static_sounds', 0x24),
         (0x10f, 'effects', 0x24), (0x110, 'prereqs', 0x30), (0x112, 'anim_refs', 0x48)]


def events(blob, st):
    """Walk the 12-byte event headers inside one sequence blob (runtime layout,
    0x507c9d: next = cur + [cur+4]); the loader itself never looks inside."""
    o = 0
    while o < len(blob):
        t, start, pad, size, time = struct.unpack_from('<BBHIf', blob, o)
        st['ev_type'][t] += 1
        if size < 12 or o + size > len(blob):
            st['ev_bad'] += 1
            return
        o += size
    if o != len(blob):
        st['ev_bad'] += 1


def walk(m, st):
    b = (G / m / 'anim.zbd').read_bytes()
    sig, ver, lcount = struct.unpack_from('<III', b, 0)
    assert sig == 0x08170616 and ver == 39, (m, hex(sig), ver)
    o = 12
    names = [cstr(b[o + i * 0x54:o + i * 0x54 + 0x50]) for i in range(lcount)]
    o += lcount * 0x54
    info = b[o:o + 0x44]; o += 0x44
    dcount = U(info, 0x0a, '<h'); mcount = U(info, 0x10)
    st['info_nonzero_04_08'] += (U(info, 4) != 0) + (U(info, 8, '<H') != 0)
    st['info_+0x3c'][U(info, 0x3c)] += 1
    st['info_gravity'][round(U(info, 0x1c, '<f'), 3)] += 1
    defs = []
    for d in range(dcount):
        h = b[o:o + 0x13c]; o += 0x13c
        rec = {'h': h, 'lists': {}}
        for cnt_off, name, size in LISTS:
            n = h[cnt_off]
            rec['lists'][name] = [b[o + i * size:o + (i + 1) * size] for i in range(n)]
            o += n * size
            st['n_' + name] += n
        reset = b[o:o + 0x40]; o += 0x40
        if reset != h[0xc8:0x108]:
            st['reset_copy_differs'] += 1
        rsize = U(reset, 0x3c)
        if rsize > 0:
            events(b[o:o + rsize], st); o += rsize
        for s in range(h[0x108]):
            sd = b[o:o + 0x40]; o += 0x40
            size = U(sd, 0x3c)
            if size > 0:
                events(b[o:o + size], st); o += size
            else:
                st['seq_empty'] += 1
        defs.append(rec)
    o += mcount * 0x24
    st['messages'] += mcount
    end_ok = o == len(b)

    # what the fixup pass would see
    for i, r in enumerate(defs):
        h = r['h']
        st['status'][h[0x98]] += 1
        st['activation'][h[0x99]] += 1
        if U(h, 0x94, '<I') & 0x80000: st['flag_local_only'] += 1
        if i and cstr(h[0x20:0x40]) == cstr(defs[i - 1]['h'][0x20:0x40]):
            st['same_node_name_as_previous_def'] += 1
        for p in r['lists']['prereqs']:
            st['prereq_type'][p[4]] += 1
            if p[4] == 1:
                target = cstr(p[8:0x28])
                found = [k for k in range(dcount - 1) if cstr(defs[k]['h'][0:0x20]) == target]
                if not found:
                    last = cstr(defs[-1]['h'][0:0x20]) == target
                    st['prereq_anim_not_found' + ('_is_last_def' if last else '')] += 1
    print(f'{m}: {len(b)} bytes, list {lcount}, defs {dcount}, messages {mcount}, '
          f'walk ends at {o} {"OK" if end_ok else "MISMATCH"}')
    return end_ok, len(b), names, defs


def emulate(m, size):
    """Run the original 0x50a0e0 on the file; return (eax, bytes read)."""
    sys.path.insert(0, str(ROOT / 'tests' / 'differential'))
    from original import Original
    data = (G / m / 'anim.zbd').read_bytes()
    cpu = Original()
    pos = [0]
    FILE = 0x2e800000
    heap = [0x31000000]
    cpu.uc.mem_map(heap[0], 0x4000000)
    fake = cpu.alloc(0x1000)          # a stand-in node for every lookup

    def malloc(c):
        a = heap[0]; heap[0] += (c.arg(0) + 0xf) & ~0xf
        return a

    def fread(c):
        dst, sz, n = c.arg(0), c.arg(1), c.arg(2)
        want = sz * n
        chunk = data[pos[0]:pos[0] + want]
        pos[0] += len(chunk)
        c.write(dst, chunk)
        return len(chunk) // sz if sz else 0
    cpu.stub_import(0x5bb628, lambda c: FILE)       # fopen
    cpu.stub_import(0x5bb594, fread)
    cpu.stub_import(0x5bb630, lambda c: 0)          # fclose
    cpu.stub_import(0x5bb664, malloc)
    cpu.stub_import(0x5bb65c, lambda c: 0)          # free
    for addr, ret in [(0x4f8bd0, fake), (0x4ed8b0, 0), (0x509640, fake), (0x4fa720, fake),
                      (0x4eb300, 0), (0x54a710, fake), (0x4fd280, fake), (0x4fd480, 0),
                      (0x56f6f0, 1), (0x50d9e0, 0), (0x4f9d60, fake), (0x575f90, 0),
                      (0x49bfd0, 0)]:
        cpu.stub_function(addr, lambda c, r=ret: r)
    cpu.u32(0x5ebcbc, 1)                            # loading enabled (0x509150)
    cpu.write(0x60c570, b'anim.zbd\0')              # path (0x5090f0)
    cpu.call(0x50a0e0)
    from unicorn.x86_const import UC_X86_REG_EAX
    return cpu.uc.reg_read(UC_X86_REG_EAX), pos[0]


def main():
    st = collections.defaultdict(int)
    for k in ('ev_type', 'status', 'activation', 'prereq_type', 'info_+0x3c', 'info_gravity'):
        st[k] = collections.Counter()
    ok = True
    for m in MISSIONS:
        end_ok, size, _, _ = walk(m, st)
        ok &= end_ok
        if '--emu' in sys.argv:
            eax, read = emulate(m, size)
            good = eax == 0 and read == size
            ok &= good
            print(f'  original 0x50a0e0: eax={eax:#x}, read {read} of {size} '
                  f'{"OK" if good else "MISMATCH"}')
    for k in sorted(st):
        v = st[k]
        print(f'  {k}: {dict(sorted(v.items())) if isinstance(v, collections.Counter) else v}')
    print('ALL OK' if ok else 'FAILED')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())

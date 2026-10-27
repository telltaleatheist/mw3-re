"""zSave's three anim registrants (zeff_anim_save: AnimActivation 0x50b710 /
0x50b970, RunningAnim 0x50c190 / 0x50c250 with 0x50be30, Anim 0x50c640 /
0x50c890; docs/save.md section 3) against the original x86 in Unicorn, on
the missions' real animation state.

    cmake --build build && .venv/bin/python -P tests/differential/test_anim_save.py [frames, 60] [after, 20] [missions...]

For each mission both sides read the world and run test_anim_run's frames of
random calls (starts, resets, stops, hits, ...), then the mission's end
(0x50ace0(1), as CoreState::leave does before the win's save) and the three
saves in zSave's order. The original's entries are caught at zSave's calls
(0x5a8a50 write, the temporary files 0x5a8ab0 / 0x5a8ad0 / 0x5a8af0 /
0x5a8b10, fwrite and fread on them); the port writes a real saved game
through zsave::save. Compared: every entry's name and bytes, and the state
after the saves (the activations' save binds each def to its node).

Then the same world is read again on both sides (a new random table, as the
next mission of the operation reads it) and the save loaded: the original's
entries given to its loads in the file's order, the port's file through
zsave::load. The state is compared, then the port's world is read once more
and loaded from a file made of the original's entries (the cross-load: an
original save into the port), compared again, and frames of random calls run
on both to show the restored runs go on alike.

The exe builds each record on its stack and leaves what it does not set as
stack leftovers; the savers' frames are zeroed on entry here, as the port
writes zeros (the loads never read those bytes). A running sequence's
current event is written as cur - events and its events' address: the
address is zeroed and an offset outside the events (a sequence never
started) is -1, as the port writes them.
"""
import ctypes
import os
import random
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from test_anim_run import (COUNT, DEFS, LOG_COUNT, Compare, Emu, Port, f2u, f32, prepare, play,  # noqa: E402
                           probe, random_table)
from test_gamez import WORLDS, ZBD  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
from unicorn.x86_const import UC_X86_REG_ESP  # noqa: E402

probe.ar_stop_all.argtypes = [ctypes.c_int]
probe.ar_save_game.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
probe.ar_perturb.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_float]

IAT_FWRITE, IAT_FREAD = 0x5bb5a0, 0x5bb594
WRITE, TEMP, WRITE_TEMP, TEMP_FROM, TEMP_DONE = 0x5a8a50, 0x5a8ab0, 0x5a8ad0, 0x5a8af0, 0x5a8b10
STOP_ALL = 0x50ace0
SAVERS = [('AnimActivation', 0x50b710, 0x50b970), ('RunningAnim', 0x50c190, 0x50c250), ('Anim', 0x50c640, 0x50c890)]
# Where each saver's (or loader's) stack frame is complete, and its size.
FRAMES = {0x50b71a: 0x4484, 0x50c64a: 0x4480, 0x50be36: 0xcc, 0x50c256: 0xb0}
TEMP_HANDLE = 0x7e000000
HEADER = b'zSaveHeader'


class SaveEmu(Emu):
    """The original with zSave's calls caught: entries kept as (name, bytes)."""

    def __init__(self):
        super().__init__()
        cpu = self.cpu
        self.entries, self.owner = [], None
        self.temp, self.tpos = bytearray(), 0
        self.buffer, self.capacity = 0, 0
        cpu.stub_function(WRITE, self.write_entry, 8)
        cpu.stub_function(TEMP, self.tmpfile)
        cpu.stub_function(WRITE_TEMP, self.write_temp, 4)
        cpu.stub_function(TEMP_FROM, self.temp_from)
        cpu.stub_function(TEMP_DONE, lambda c: 0)
        cpu.stub_import(IAT_FWRITE, self.fwrite)

        def zero(uc, addr, size, _):
            esp = uc.reg_read(UC_X86_REG_ESP)
            uc.mem_write(esp, bytes(FRAMES[addr]))
        for a in FRAMES:
            cpu.uc.hook_add(UC_HOOK_CODE, zero, begin=a, end=a)
        self.ctx = self.alloc_fixed(16)

    def sprintf(self, c):
        fmt = self.cstr(c.arg(1))
        for pre in (b'Activation', b'Running', b'Anim'):
            if fmt.startswith(pre) and fmt.endswith(b'd'):
                n = int(fmt[len(pre) + 1:-1])
                s = pre + str(struct.unpack('<i', struct.pack('<I', c.arg(2)))[0]).zfill(n).encode()
                c.write(c.arg(0), s + b'\0')
                return len(s)
        return super().sprintf(c)

    def write_entry(self, c):
        self.entries.append((f'{self.owner}/{self.cstr(c.edx).decode("latin-1")}', c.read(c.arg(0), c.arg(1))))
        return 0

    def tmpfile(self, c):
        self.temp, self.tpos = bytearray(), 0
        return TEMP_HANDLE

    def fwrite(self, c):
        buf, size, count, f = c.arg(0), c.arg(1), c.arg(2), c.arg(3)
        assert f == TEMP_HANDLE
        self.temp += c.read(buf, size * count) if size * count else b''
        return count

    def fread(self, c):
        if c.arg(3) != TEMP_HANDLE:
            return super().fread(c)
        buf, size, count = c.arg(0), c.arg(1), c.arg(2)
        chunk = bytes(self.temp[self.tpos:self.tpos + size * count])
        self.tpos += len(chunk)
        if chunk:
            c.write(buf, chunk)
        return len(chunk) // size if size else 0

    def write_temp(self, c):
        self.entries.append((f'{self.owner}/{self.cstr(c.arg(0)).decode("latin-1")}', bytes(self.temp)))
        return 0

    def temp_from(self, c):
        self.temp, self.tpos = bytearray(c.read(c.ecx, c.edx)), 0
        return TEMP_HANDLE

    def stop_all(self, keep):
        self.guard(lambda: self.cpu.call(STOP_ALL, ecx=keep, count=2_000_000_000))
        self.build_ids()

    def save(self):
        self.entries = []
        for owner, save, _ in SAVERS:
            self.owner = owner
            self.guard(lambda: self.cpu.call(save, ecx=self.ctx, count=2_000_000_000))
        self.build_ids()
        return self.entries

    def load(self, entries=None):
        if entries is None:
            return super().load()
        loads = {o: ld for o, _, ld in SAVERS}
        for name, data in entries:
            owner, _, sub = name.partition('/')
            sub = sub.split(' ')[0]
            if owner not in loads or not sub:
                continue
            if len(data) > self.capacity:  # zSave's buffer, grown and never shrunk
                self.buffer, self.capacity = self.alloc_fixed(len(data)), len(data)
            self.cpu.write(self.buffer, data)
            p = self.alloc_fixed(len(sub) + 1)
            self.cpu.write(p, sub.encode('latin-1') + b'\0')
            self.guard(lambda: self.cpu.call(loads[owner], ecx=self.ctx, edx=p,
                                             stack_args=(self.buffer, len(data), 0), count=2_000_000_000))
        self.build_ids()


def normalized(entries):
    """The original's running sequences' event pointers as the port writes them."""
    out = []
    for name, data in entries:
        if name.startswith('RunningAnim/') and len(data) >= 0x68:
            b = bytearray(data)
            pos, last = 0x68, 0
            for _ in range(b[0x64]):
                off, ptr, size = struct.unpack_from('<iIi', b, pos + 0x34)
                if not (0 <= off <= max(size, 0)) or ptr == 0:
                    off = -1
                struct.pack_into('<iI', b, pos + 0x34, off, 0)
                pos, last = pos + 0x40 + max(size, 0), ptr
            # A light or sound without its node leaves the record's tail as the
            # buffer had it: after the sequences, the last one's address at +0x38.
            pos += 0x38 * b[0x65]
            for _ in range(b[0x66]):
                if last and struct.unpack_from('<I', b, pos + 0x38)[0] == last:
                    struct.pack_into('<I', b, pos + 0x38, 0)
                pos += 0x3c
            data = bytes(b)
        out.append((name, data))
    return out


def read_zar(path):
    f = Path(path).read_bytes()
    version, count = struct.unpack_from('<II', f, len(f) - 8)
    assert version == 1
    table = len(f) - 8 - count * 0x94
    out = []
    for i in range(count):
        e = table + 0x94 * i
        start, length = struct.unpack_from('<II', f, e)
        name = f[e + 8:e + 0x48].split(b'\0')[0].decode('latin-1')
        out.append((name, f[start:start + length]))
    return out


def write_zar(path, entries):
    body, table = bytearray(), bytearray()
    for name, data in [(HEADER.decode(), struct.pack('<ii', 8, 2))] + entries:
        e = bytearray(0x94)
        struct.pack_into('<II', e, 0, len(body), len(data))
        e[8:8 + len(name)] = name.encode('latin-1')
        table += e
        body += data
    Path(path).write_bytes(bytes(body + table + struct.pack('<II', 1, len(entries) + 1)))


def words(b):
    b = b + bytes(-len(b) % 4)
    return list(struct.unpack(f'<{len(b) // 4}I', b))


def check_entries(what, want, got, cmp):
    """Names exactly, the bytes as words (floats within the tolerance: the
    rotations come from the matrices through the x87)."""
    names_w, names_g = [n for n, _ in want], [n for n, _ in got]
    if names_w != names_g:
        raise SystemExit(f'{what}: entry names differ\n  original {names_w[:20]}\n  port     {names_g[:20]}')
    cmp.marks = []
    for (n, a), (_, b) in zip(want, got):
        if len(a) != len(b):
            raise SystemExit(f'{what}: entry {n} sizes {len(a)} / {len(b)}')
        cmp(f'{what} entry {n}', words(a), words(b))


def save_both(emu, mission, label, cmp, stats, folder):
    want = normalized(emu.save())
    if os.environ.get('MW3_KEEP'):
        write_zar(folder / f'{mission}-{label}-exe.mw3', want)
    name = f'{mission}-{label}.mw3'
    assert probe.ar_save_game(str(folder).encode(), name.encode(), 0) == 1
    got = [e for e in read_zar(folder / name) if e[0] != HEADER.decode()]
    check_entries(f'{mission} {label}', want, got, cmp)
    for n, _ in want:
        stats[n.split('/')[0]] += 1
    return want, name


def run_mission(mission, frames, after, seed, cmp, stats, folder):
    emu, port, rng, nrng = prepare(mission, seed, cmp, stats, SaveEmu)
    play(emu, port, mission, frames, rng, nrng, cmp, stats)
    count, base = struct.unpack('<h', emu.cpu.read(COUNT, 2))[0], emu.u(DEFS)

    def runs():
        return sum(1 for i in range(1, count) if emu.cpu.read(base + 0x13c * i + 0x98, 1)[0] in (2, 6))
    # A save in the middle of the mission (MW3 makes none, but the code is the
    # same): the runs as they are.
    stats['running_mid'] += runs()
    saves = [('mid',) + save_both(emu, mission, 'mid', cmp, stats, folder)]
    cmp(f'{mission} saved mid', emu.dump(True), port.dump(True))
    # The win's: the mission's end first, as CoreState::leave.
    emu.stop_all(1)
    probe.ar_stop_all(1)
    cmp(f'{mission} stopped', emu.dump(True), port.dump(True))
    stats['running_win'] += runs()
    stats['log_saved'] += struct.unpack('<i', emu.cpu.read(LOG_COUNT, 4))[0]
    saves.append(('win',) + save_both(emu, mission, 'win', cmp, stats, folder))
    cmp(f'{mission} saved win', emu.dump(True), port.dump(True))
    del emu
    # The next mission of the operation: the world read again, each save loaded.
    for k, (label, want, name) in enumerate(saves):
        seed2 = seed ^ 0x5a8a90 ^ k
        emu2, port2, rng2, nrng2 = prepare(mission, seed2, cmp, stats, SaveEmu)
        emu2.load(want)
        assert probe.ar_save_game(str(folder).encode(), name.encode(), 1) == 1
        restored = emu2.dump(True)
        cmp(f'{mission} {label} loaded', restored, port2.dump(True))
        # The original's entries loaded by the port.
        cross = f'{mission}-{label}-exe.mw3'
        write_zar(folder / cross, want)
        port3 = port_world(mission, seed2, emu2)
        assert probe.ar_save_game(str(folder).encode(), cross.encode(), 1) == 1
        cmp(f'{mission} {label} cross-loaded', restored, port3.dump(True))
        stats['cross'] += 1
        play(emu2, port3, mission, after, rng2, nrng2, cmp, stats, f'after {label} load')
        if label == 'mid':
            damaged(mission, want, seed2, emu2, folder, stats)
        del emu2
    # The mid save loaded into a world already played (not a case MW3 makes:
    # it loads at the mission's start), so the restores' node states, statuses
    # and instances are seen against a world that differs from the save's.
    label, want, name = saves[0]
    seed3 = seed ^ 0x50b970
    emu3, port4, rng3, nrng3 = prepare(mission, seed3, cmp, stats, SaveEmu)
    play(emu3, port4, mission, after, rng3, nrng3, cmp, stats, 'before played load')
    perturb(emu3, random.Random(seed3), stats)
    cmp(f'{mission} perturbed', emu3.dump(True), port4.dump(True))
    emu3.load(want)
    assert probe.ar_save_game(str(folder).encode(), name.encode(), 1) == 1
    cmp(f'{mission} played loaded', emu3.dump(True), port4.dump(True))
    play(emu3, port4, mission, after, rng3, nrng3, cmp, stats, 'after played load')
    stats['played'] += 1


def perturb(emu, rng, stats):
    """Every def's objects (class 5) hidden or shown and moved, through
    0x4eb170, 0x4fc020 and 0x4fc0b0 on the original and the same calls in the port:
    the restore must put back what the save holds."""
    count, base = struct.unpack('<h', emu.cpu.read(COUNT, 2))[0], emu.u(DEFS)
    done = set()
    for i in range(1, count):
        d = base + 0x13c * i
        objs = emu.u(d + 0x114)
        for k in range(emu.cpu.read(d + 0x109, 1)[0] if objs else 0):
            n = emu.u(objs + 0x60 * k + 0x24)
            if not n or n in done or emu.u(n + 0x34) != 5:
                continue
            done.add(n)
            active = rng.randrange(2)
            dv = [f32(rng.uniform(-2, 2)) for _ in range(3)]
            emu.cpu.call(0x4eb170, ecx=n, edx=active)
            v = emu.alloc_fixed(12)
            emu.cpu.call(0x4fc020, ecx=n, edx=v, stack_args=(v + 4, v + 8))
            x, y, z = struct.unpack('<3f', emu.cpu.read(v, 12))
            emu.cpu.call(0x4fc0b0, ecx=n, stack_args=tuple(f2u(f32(a + b)) for a, b in zip((x, y, z), dv)))
            probe.ar_perturb(emu.index(n), active, *dv)
    emu.build_ids()
    stats['perturbed'] += len(done)


def damaged(mission, entries, seed, emu, folder, stats):
    """The port's FIX (bug)s: entries cut short or given an index, count or
    sequence count that does not fit, loaded by the port (the exe reads past
    them; save.md section 4). The load must come through."""
    rng = random.Random(seed)
    for k in range(12):
        bad = []
        for name, data in entries:
            b = bytearray(data)
            r = rng.random()
            if r < 0.15:
                b = b[:rng.randrange(len(b) + 1)]
            elif r < 0.25 and name.startswith('Anim/') and len(b) >= 0x54:
                struct.pack_into('<i', b, 0x50, rng.choice((-1, 0x7fff, 100000)))
            elif r < 0.35 and len(b) >= 0x56 and not name.startswith('RunningAnim/'):
                b[0x55] = 0xff
            elif r < 0.45 and name.startswith('RunningAnim/') and len(b) >= 0x68:
                struct.pack_into('<i', b, 0, rng.choice((-5, 0x7fff)))
            elif r < 0.55 and name.startswith('RunningAnim/') and len(b) >= 0x68:
                b[0x64] = 0xff
            bad.append((name, bytes(b)))
        path = f'{mission}-bad{k}.mw3'
        write_zar(folder / path, bad)
        port_world(mission, seed, emu)
        assert probe.ar_save_game(str(folder).encode(), path.encode(), 1) == 1
        stats['damaged'] += 1


def port_world(mission, seed, emu):
    """prepare()'s port side alone (the same table and cockpit reset), the
    original's ids taken from `emu`, a world of the same mission."""
    port = Port()
    port.open(ZBD / mission / 'gamez.zbd', ZBD / mission / 'anim.zbd')
    port.set_random(random_table(random.Random(seed)))
    port.load()
    count, base = struct.unpack('<h', emu.cpu.read(COUNT, 2))[0], emu.u(DEFS)
    names = {emu.cpu.read(base + 0x13c * i, 0x20).split(b'\0')[0]: i for i in range(1, count)}
    if b'cpit_effects_explosion' in names:
        port.restart(names[b'cpit_effects_explosion'], emu.find_node(b'dummy_hud.flt'))
    return port


def main():
    args = sys.argv[1:]
    frames = int(args.pop(0)) if args and args[0].isdigit() else 60
    after = int(args.pop(0)) if args and args[0].isdigit() else 20
    missions = args or WORLDS
    cmp = Compare()
    stats = {k: 0 for k in ('frames', 'start', 'start_at', 'start_node', 'reset', 'reset_node', 'reset_cockpit',
                            'stop', 'invalidate', 'ram', 'hit', 'player', 'zone', 'running_mid', 'running_win', 'log_saved',
                            'AnimActivation', 'RunningAnim', 'Anim', 'cross', 'damaged', 'played', 'perturbed')}
    keep = os.environ.get('MW3_KEEP')  # a folder to keep the saves in
    with tempfile.TemporaryDirectory() as d:
        if keep:
            d = keep
            Path(d).mkdir(parents=True, exist_ok=True)
        for i, m in enumerate(missions):
            run_mission(m, frames, after, 2000 + i, cmp, stats, Path(d))
            print(f'{m}: ok ({stats["frames"]} frames so far)', flush=True)
    print(f'ok: {len(missions)} missions saved, loaded and cross-loaded identically; {cmp.words} words compared, '
          f'{cmp.approx} within tolerance (worst {cmp.worst:.2e}); '
          f'{", ".join(f"{k} {v}" for k, v in stats.items() if k != "frames")}')


if __name__ == '__main__':
    main()

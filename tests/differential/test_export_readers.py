"""EXPORT of a new world read by the original's own readers (playtest Z5; docs/modding.md, EXPORT of a new world).

mw3tests' mod_export_new_world_and_its_editor_copy_unpack_as_mounted makes the world (c1 copied as ironcoast,
three missions, added by IMPORT) and a copy of its campaign with an editor save, exports both, lays both zips
over a copy of the install and checks every archive there against what the mount gives the game; run with
MW3_EXPORT_KEEP it leaves that folder. Here every archive the zips laid under zbd/ironcoast is read by the
original x86 in Unicorn and by the port on the same file, as the format's own differential does on the
install's: gamez.zbd (0x4ff520's texture directory, materials, models, nodes), anim.zbd against it (0x50a0e0),
each texture library (open 0x5187c0, every texture loaded 0x518db0) and each record archive (every entry's
node read by zRdrReadNode 0x557180, its tree). Each must agree byte for byte.
"""
import ctypes
import os
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from original import ROOT  # noqa: E402
import test_anim as anim_t  # noqa: E402
import test_gamez as gamez_t  # noqa: E402
import test_textures as tex_t  # noqa: E402
import test_zreader as zr_t  # noqa: E402
from test_anim_run import short_link  # noqa: E402

KEEP = ROOT / 'build' / 'test-tmp' / 'export-readers'
TEST = 'mod_export_new_world_and_its_editor_copy_unpack_as_mounted'


def unpacked():
    """The install copy with both zips laid over it, made by the C++ test (which also checks it as mounted)."""
    env = dict(os.environ, MW3_TEST=TEST, MW3_EXPORT_KEEP=str(KEEP))
    r = subprocess.run([str(ROOT / 'build' / 'mw3tests')], env=env, capture_output=True, text=True)
    if r.returncode or '1 tests, 0 failed checks' not in r.stdout:
        raise SystemExit(f'{TEST} did not pass:\n{r.stdout[-3000:]}')
    for line in r.stdout.splitlines():
        if line.startswith('  unpacked over') or line.startswith('  world:') or line.startswith('  copy:'):
            print(line.strip())
    return KEEP


def records(path, buf):
    data = path.read_bytes()
    emu = zr_t.Emulated(data)
    n = 0
    for name, start, length in zr_t.entries(data):
        got = emu.load(start)
        mine = zr_t.probe.probe_zrdr_load(str(path).encode(), start)
        if got != mine or got != length:
            raise SystemExit(f'{path.name} {name}: bytes read original {got} port {mine} entry {length}')
        out, paths, past_end, strings = [], {}, set(), set()
        emu.walk(emu.root, out, paths, past_end, '', strings)
        k = zr_t.probe.probe_zrdr_dump(buf, len(buf))
        if k < 0 or ''.join(out) != buf.value.decode('latin-1'):
            raise SystemExit(f'{path.name} {name}: the trees differ')
        n += 1
    return n


def world(path, emu, buf):
    data = path.read_bytes()
    p = str(path).encode()
    count, offset = struct.unpack_from('<ii', data, 8)
    want = emu.texture_directory(data, offset, count, 3)
    n = gamez_t.probe.probe_texture_directory(p, offset, count, 3, buf, len(buf))
    if buf.raw[:n] != want:
        raise SystemExit(f'{path}: the texture directory differs')
    moff, = struct.unpack_from('<i', data, 0x10)
    for fmt in ((5, 6, 5), (5, 5, 5)):
        want = emu.materials(data, moff, *fmt)
        n = gamez_t.probe.probe_materials(p, moff, *fmt, buf, len(buf))
        if n < 0 or buf.raw[:n] != want:
            raise SystemExit(f'{path}: materials {fmt} differ')
    mdl_off, = struct.unpack_from('<i', data, 0x14)
    want = emu.models(data, moff, mdl_off)
    n = gamez_t.probe.probe_models(p, moff, mdl_off, buf, len(buf))
    if n < 0 or buf.raw[:n] != want:
        raise SystemExit(f'{path}: models differ')
    size, free_head, node_off = struct.unpack_from('<iii', data, 0x18)
    want = emu.nodes(data, moff, mdl_off, node_off, size, free_head)
    n = gamez_t.probe.probe_nodes(p, moff, mdl_off, node_off, size, free_head, buf, len(buf))
    if n < 0 or buf.raw[:n] != want:
        raise SystemExit(f'{path}: nodes differ')
    return count, struct.unpack_from('<i', data, moff + 4)[0], struct.unpack_from('<i', data, mdl_off + 4)[0], struct.unpack_from('<i', want, 8)[0]


def animations(gamez, anim, buf):
    w, a = short_link(gamez, 'ironcoast-gamez.zbd'), short_link(anim, 'ironcoast-anim.zbd')
    emu = anim_t.Emulated()
    want = emu.run(w, a, full=True)
    anim_t.compare('ironcoast anim.zbd', want, anim_t.probe.probe_anim(str(w).encode(), str(a).encode(), -1, 1, buf, len(buf)), buf)
    return struct.unpack_from('<i', want, 4)[0]


def textures(path, emu, buf):
    folder = short_link(path.parent, 'ironcoast-zbd')
    data = path.read_bytes()
    got = emu.open(path.name, data, 6)
    mine = tex_t.probe.probe_tex_open((str(folder) + '/').encode(), path.name.encode(), 6)
    if got != mine:
        raise SystemExit(f'{path.name}: open original {got} port {mine}')
    for name in tex_t.names(data):
        a = emu.load(name)
        n = tex_t.probe.probe_tex_load(name, buf, len(buf))
        if a != (buf.raw[:n] if n >= 0 else None):
            raise SystemExit(f'{path.name} {name!r}: original {len(a or b"")} bytes, port {n}')
    return got


def main():
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else unpacked()
    folder = root / 'zbd' / 'ironcoast'
    files = sorted(folder.glob('*.zbd'))
    buf = ctypes.create_string_buffer(64 << 20)
    done, entries, texs = [], 0, 0
    tex_emu = tex_t.Emulated()
    for f in files:
        n = f.name.lower()
        if n == 'gamez.zbd':
            t, m, mdl, live = world(f, gamez_t.Emulated(), buf)
            print(f'gamez.zbd: {t} textures named, {m} materials, {mdl} models, {live} live nodes identical')
        elif n == 'anim.zbd':
            calls = animations(folder / 'gamez.zbd', f, buf)
            print(f'anim.zbd: read against its gamez.zbd identical ({calls} unported calls)')
        elif n.startswith('texture') or n.startswith('rtexture'):
            texs += textures(f, tex_emu, buf)
        elif n.startswith('reader'):
            entries += records(f, buf)
        else:
            raise SystemExit(f'{f.name}: an archive this check does not know')
        done.append(n)
    if len(done) != 17:
        raise SystemExit(f'{len(done)} archives under {folder}, 17 expected')
    print(f'ok: {len(done)} archives of the exported world read by the original and the port identically '
          f'({entries} record entries, {texs} textures)')


if __name__ == '__main__':
    main()

"""The AI net file's text writers against the original x86 (docs/editor.md, The AI net editor, the map): the
header 0x4da1d0 -> 0x4dad10 and a node 0x4db8d0 (the file object's slot +0x14, 0x4da7e0), which nothing in the
release calls. Random records and headers, in the exe's layout in emulator memory and as the port's, written by
both; the text must match byte for byte. The exe's fprintf and sprintf (msvcrt imports) are this file's own
formatter, so the C runtime's rounding is the host's on both sides.

    cmake --build build --target mw3probe && .venv/bin/python -P tests/differential/test_ai_netwrite.py [runs]

Covered: every property line, each one at and off its default (0x5e8af4), versions 105 (three links padded with
-1, the report past three), 106, 107 and 108, both node kinds and a third (no node line), empty valveunion slots.
BREAK=1 writes the port's scan_hit before scan_time (the exe writes aim, time, hit); BREAK=2 writes floats at
%6.2f. Each must fail.
"""
import ctypes
import os
import random
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from calltrace import load_probe  # noqa: E402
from original import Original  # noqa: E402

FPRINTF, SPRINTF, REPORT = 0x5bb62c, 0x5bb658, 0x49bfd0
STRRCHR, MALLOC, STRNCPY = 0x5bb5a8, 0x5bb664, 0x5bb648  # the base name's (0x4da060)
HEADER, NODE = 0x4da1d0, 0x4db8d0
DEFAULT = 0x5e8af4
NAMES = ['alpha', 'Gate_2', 'v', 'ValveLongerName_7', 'x y', 'mcb']

probe = load_probe()
probe.probe_netwrite_record.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p,
                                        ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                        ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
probe.probe_netwrite_header.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p,
                                        ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


class Exe:
    def __init__(self):
        cpu = self.cpu = Original()
        self.out, self.reports = bytearray(), []
        cpu.stub_import(FPRINTF, lambda c: self.printf(c, 1, None), 0)
        cpu.stub_import(SPRINTF, lambda c: self.printf(c, 1, c.arg(0)), 0)
        cpu.stub_function(REPORT, lambda c: self.reports.append(self.cstr(c.arg(3))), 0)
        cpu.stub_import(STRRCHR, self.strrchr, 0)
        cpu.stub_import(MALLOC, lambda c: c.alloc(c.arg(0) + 1), 0)
        cpu.stub_import(STRNCPY, self.strncpy, 0)
        self.file = cpu.alloc(0x118)

    def strrchr(self, c):
        s, ch = c.arg(0), c.arg(1) & 0xff
        text = self.cstr(s)
        at = text.rfind(bytes([ch]))
        return s + at if at >= 0 else 0

    def strncpy(self, c):
        d, s, n = c.arg(0), c.arg(1), c.arg(2)
        text = self.cstr(s)[:n]
        c.write(d, text + bytes(n - len(text)))
        return d

    def cstr(self, a):
        b = bytearray()
        while True:
            ch = self.cpu.read(a, 1)[0]
            if not ch:
                return bytes(b)
            b.append(ch)
            a += 1

    def printf(self, c, fmt_arg, dest):
        """fprintf (FILE*, fmt, ...) and sprintf (buf, fmt, ...): %d %s %f with flags, width and precision."""
        fmt = self.cstr(c.arg(fmt_arg)).decode('latin-1')
        at = 4 + 4 * (fmt_arg + 1)
        esp = c.uc.reg_read(__import__('unicorn').x86_const.UC_X86_REG_ESP)

        def take(n):
            nonlocal at
            v = c.read(esp + at, n)
            at += n
            return v

        def conv(m):
            spec = m.group(0)
            if spec == '%%':
                return '%'
            kind = spec[-1]
            if kind == 'f':
                return spec % struct.unpack('<d', take(8))[0]
            if kind == 'd':
                return spec % struct.unpack('<i', take(4))[0]
            return spec % self.cstr(struct.unpack('<I', take(4))[0]).decode('latin-1')
        text = re.sub(r'%%|%[-+ 0#]*\d*(?:\.\d+)?[dsf]', conv, fmt).encode('latin-1')
        if dest is None:
            self.out += text
        else:
            c.write(dest, text + b'\0')
        return len(text)

    def string(self, s):
        if s is None:
            return 0
        a = self.cpu.alloc(len(s) + 1)
        self.cpu.write(a, s.encode('latin-1') + b'\0')
        return a

    def valve(self, name):
        if name is None:
            return 0
        v = self.cpu.alloc(0xc)
        self.cpu.write(v, bytes(0xc))
        self.cpu.u32(v, self.string(name))
        return v

    def record(self, r, version, net, key):
        cpu = self.cpu
        rec = cpu.alloc(0xb4)
        img = bytearray(r['img'])
        put = lambda off, v: struct.pack_into('<I', img, off, v)  # noqa: E731
        put(0x34, self.valve(r['strs'][3]))
        put(0x78, self.valve(r['strs'][4]))
        put(0x7c, self.valve(r['strs'][5]))
        put(0x90, self.valve(r['strs'][6]))
        put(0x9c, self.string(r['strs'][0]))
        put(0xa0, self.string(r['strs'][1]))
        put(0xa8, self.string(r['strs'][2]))
        links = cpu.alloc(0x10)
        data = cpu.alloc(4 * len(r['links']) + 4)
        cpu.write(data, struct.pack(f'<{len(r["links"])}i', *r['links']))
        cpu.write(links, struct.pack('<IIII', 0, data if r['links'] else 0, data + 4 * len(r['links']) if r['links']
                                     else 0, 0))
        put(0xa4, links)
        if r['union'] is not None:
            u = cpu.alloc(0x10)
            items = cpu.alloc(8 * len(r['union']) + 8)
            cpu.write(items, b''.join(struct.pack('<Ii', self.valve(n), k) for n, k in r['union']))
            cpu.write(u, struct.pack('<IIII', 0, items, items + 8 * len(r['union']), 0))
            put(0xac, u)
        if r['strs'][7] is not None:
            va = cpu.alloc(8)
            cpu.write(va, struct.pack('<Ii', self.valve(r['strs'][7]), r['assign']))
            put(0xb0, va)
        cpu.write(rec, img)
        cpu.u32(self.file + 0xf4, net)
        cpu.u32(self.file + 0xf8, version)
        self.out.clear()
        self.reports.clear()
        cpu.call(NODE, ecx=rec, stack_args=(0x1234, self.string(key), self.file))
        return bytes(self.out), list(self.reports)

    def header(self, h):
        cpu = self.cpu
        cpu.write(self.file, bytes(0x118))
        cpu.u32(self.file + 4, self.string(h['file']))
        cpu.write(self.file + 0xf4, struct.pack('<iii3f', h['net'], h['version'], h['has_pos'], *h['pos']))
        cpu.u32(self.file + 0x10c, self.string(h['name']))
        self.out.clear()
        cpu.call(HEADER, ecx=self.file, stack_args=(0x1234,))
        return bytes(self.out)


def port_record(r, version, net, key, brk):
    strs = (ctypes.c_char_p * 8)(*[s.encode('latin-1') if s is not None else None for s in r['strs']])
    links = (ctypes.c_int32 * max(1, len(r['links'])))(*r['links'])
    un = r['union'] or []
    names = (ctypes.c_char_p * max(1, len(un)))(*[n.encode('latin-1') if n else None for n, _ in un])
    counts = (ctypes.c_int32 * max(1, len(un)))(*[k for _, k in un])
    out = ctypes.create_string_buffer(1 << 14)
    n = probe.probe_netwrite_record(bytes(r['img']), links, len(r['links']), strs, names, counts,
                                    -1 if r['union'] is None else len(un), r['assign'], version, net, key.encode(),
                                    out, len(out))
    text = out.raw[:n]
    if brk == 1:  # scan_hit before scan_time
        m_t, m_h = re.search(rb'\n +scan_time \([^)]*\)', text), re.search(rb'\n +scan_hit \([^)]*\)', text)
        if m_t and m_h:
            text = text[:m_t.start()] + m_h.group(0) + m_t.group(0) + text[m_h.end():]
    if brk == 2:  # %6.2f
        text = re.sub(rb'(-?\d+\.\d{3})\b', lambda m: b'%6.2f' % float(m.group(1)), text)
    return text


def port_header(h):
    pos = (ctypes.c_float * 3)(*h['pos'])
    out = ctypes.create_string_buffer(4096)
    n = probe.probe_netwrite_header(h['net'], h['version'], h['has_pos'], pos,
                                    h['name'].encode() if h['name'] is not None else None, h['file'].encode(), out,
                                    len(out))
    return out.raw[:n]


def fresh(rng, default):
    img = bytearray(default[:0x9c]) + bytes(0xb4 - 0x9c)  # the defaults are the plain fields; the owned ones none
    put = lambda off, fmt, v: struct.pack_into(fmt, img, off, v)  # noqa: E731
    moved = lambda: rng.random() < 0.35  # noqa: E731
    put(0x04, '<i', rng.choice([0, 0, 0, 1, 1, 2]))
    put(0x08, '<i', rng.randrange(-1, 120))
    for i in range(3):
        put(0x0c + 4 * i, '<f', f32(rng.choice([rng.uniform(-3000, 3000), round(rng.uniform(-500, 500), 1), 0.05,
                                                -0.05, 0.25, 12.25])))
    for off in (0x18, 0x20, 0x28, 0x48):
        if moved():
            put(off, '<i', rng.choice([0, 1, 2, -1]))
        if moved():
            put(off + 4, '<f', f32(rng.choice([rng.uniform(-100, 100), 0.0005, 1.0, 10.0])))
    if moved():
        put(0x30, '<i', rng.choice([0, 1, 3]))
    if moved():
        img[0x38] = rng.randrange(2)
    if moved():
        put(0x3c, '<i', rng.randrange(-5, 50))
    for off in (0x40, 0x44, 0x50, 0x54, 0x58, 0x5c, 0x60, 0x64):
        if moved():
            put(off, '<i', rng.randrange(-3, 30))
    for off in range(0x68, 0x73):
        if moved():
            img[off] = rng.choice([0, 1, 2, 255])
    if moved():
        put(0x74, '<h', rng.randrange(-300, 300))
    if moved():
        img[0x80] = rng.randrange(2)
        put(0x84, '<f', f32(rng.uniform(0, 50)))
    if moved():
        img[0x88], img[0x89] = rng.randrange(2), rng.randrange(256)
    if moved():
        put(0x8c, '<f', f32(rng.uniform(0, 30)))
    if moved():
        img[0x98] = rng.randrange(2)
    if moved():
        img[0x99] = rng.randrange(2)
    pick = lambda p: rng.choice(NAMES) if rng.random() < p else None  # noqa: E731
    strs = [pick(0.5), pick(0.3), pick(0.25), pick(0.5), None, None, None, pick(0.2)]
    if img[0x70:0x73] != default[0x70:0x73] or img[0x74:0x76] != default[0x74:0x76] or moved():
        strs[4], strs[5] = rng.choice(NAMES), rng.choice(NAMES)  # deploy's valves (the exe needs both)
    if img[0x88] != default[0x88] or img[0x89] != default[0x89] or img[0x8c:0x90] != default[0x8c:0x90] or moved():
        strs[6] = rng.choice(NAMES)
    union = None
    if rng.random() < 0.25:
        union = [(rng.choice(NAMES + [None]), rng.randrange(1, 9)) for _ in range(rng.randrange(0, 4))]
    return {'img': bytes(img), 'strs': strs, 'links': [rng.randrange(-1, 200) for _ in range(rng.choice([0, 1, 2, 3,
                                                                                                         3, 4, 6]))],
            'union': union, 'assign': rng.randrange(-5, 99)}


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    brk = int(os.environ.get('BREAK', '0'))
    exe = Exe()
    default = exe.cpu.read(DEFAULT, 0xb4)
    # The port's defaults are the exe's (the writer compares against them).
    rng = random.Random(0x4db8d0)
    lines, reports, versions = set(), 0, set()
    for run in range(runs):
        version = rng.choice([0x69, 0x6a, 0x6a, 0x6a, 0x6b, 0x6c])
        net, key = rng.randrange(0, 40), f'node_{rng.randrange(100):02d}'
        r = fresh(rng, default)
        want, rep = exe.record(r, version, net, key)
        got = port_record(r, version, net, key, brk)
        if want != got:
            at = next((i for i in range(min(len(want), len(got))) if want[i] != got[i]), min(len(want), len(got)))
            raise SystemExit(f'run {run} (version {version}): node text differs at byte {at}\n'
                             f'  exe  {want[max(0, at - 60):at + 60]!r}\n  port {got[max(0, at - 60):at + 60]!r}')
        reports += len(rep)
        versions.add(version)
        lines.update(re.findall(rb'\n +([a-z_]+) \(', want))
    for run in range(runs // 5):
        h = {'net': rng.randrange(0, 60), 'version': rng.choice([0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x64]),
             'has_pos': rng.randrange(2), 'pos': [f32(rng.uniform(-4000, 4000)) for _ in range(3)],
             'name': rng.choice([None, None, 'c1_main', 'net 7']), 'file': rng.choice(['net_04.zrd', 'reg_a.zrd',
                                                                                         'waynet'])}
        want, got = exe.header(h), port_header(h)
        if want != got:
            raise SystemExit(f'header run {run} {h}: differs\n  exe  {want!r}\n  port {got!r}')
    need = {b'name', b'speed', b'wait_interval', b'wait_time', b'valve', b'canleave', b'jump', b'scan_aim',
            b'scan_time', b'scan_hit', b'hidden', b'fixed', b'autonext', b'visible', b'deploy', b'mcbwait',
            b'arrangenodes', b'backup', b'gate', b'shunt', b'valveunion', b'valve_assign', b'auxname'}
    if not need <= lines or not reports or len(versions) < 4:
        raise SystemExit(f'coverage: missing {sorted(need - lines)}, reports {reports}, versions {sorted(versions)}')
    print(f'ok: {runs} nodes and {runs // 5} headers written as the exe writes them ({len(lines)} kinds of line, '
          f'{reports} BS-style reports past three links)')


if __name__ == '__main__':
    main()

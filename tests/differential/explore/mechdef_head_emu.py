"""Scratch checks for docs/drafts/mechdef-head.md (not a test):

1. The head of the mech reader, 0x46ba95..0x46be07, run in Unicorn after the
   real MechDfn ctor on all 19 dfn_*.zrd, against a Python model of the doc.
2. The whole reader 0x46ba40 with `version` patched to 3 (returns 0), then the
   dtor 0x46b9b0 on that dfn (+0x20 still 0): expected to fault in 0x424f60.
3. Saved loadouts: every built-in config (0x46ba95..0x46cb3c) saved with the
   original Config::Save 0x424480, read back through 0x4242a0 (ctor + Load
   0x424600 + Validate 0x4254d0), fields compared; then one byte tampered.

    .venv/bin/python tests/differential/explore/mechdef_head_emu.py
"""
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from original import STACK_TOP, FTOL, IAT_FTOL  # noqa: E402
from test_zreader import READER, entries, HANDLE  # noqa: E402
from test_mechdef import Emulated, FLOOR, IAT_FLOOR, LIST_INIT, DAMAGE_MODE, MSG_HOOK  # noqa: E402
from mechdef_head_survey import find  # noqa: E402
import zrd  # noqa: E402  (tools/analysis, put on the path by the survey module)
from unicorn import UcError  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP  # noqa: E402

CTOR, HEAD, HEAD_END, CFG_END, READ, DTOR = 0x46b890, 0x46ba95, 0x46be07, 0x46cb3c, 0x46ba40, 0x46b9b0
SAVE, LOAD_ONE = 0x424480, 0x4242a0
IAT_STRNCMP, IAT_FREE, IAT_CEIL, IAT_WRITEFILE, IAT_MKDIR = 0x5bb654, 0x5bb65c, 0x5bb61c, 0x5bb17c, 0x5bb618
CEIL = FLOOR.replace(bytes.fromhex('660d0004'), bytes.fromhex('660d0008'))
MECHS = ['annihilator', 'avatar', 'blackhawk', 'bushwacker', 'cauldron', 'champion', 'daishi', 'elemental',
         'firefly', 'madcat', 'orion', 'owens', 'puma', 'shadowcat', 'strider', 'sunder', 'supernova', 'thor',
         'vulture']
K = 0.01745329251994


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


class Emu(Emulated):
    def __init__(self, data):
        super().__init__(data)
        cpu = self.cpu
        cpu.import_code(IAT_FTOL, FTOL)
        cpu.import_code(IAT_FLOOR, FLOOR)
        cpu.import_code(IAT_CEIL, CEIL)
        cpu.stub_import(IAT_STRNCMP, self.strncmp)
        cpu.stub_import(IAT_FREE, lambda c: 0)
        cpu.stub_import(IAT_WRITEFILE, self.write_file, pop_bytes=20)
        cpu.stub_import(IAT_MKDIR, lambda c: 0)
        cpu.stub_function(0x576080, lambda c: 1)  # ".\configs" exists
        self.out = None

    def strncmp(self, c):
        a, b, n = self.cstr(c.arg(0))[:c.arg(2)], self.cstr(c.arg(1))[:c.arg(2)], c.arg(2)
        return (a > b) - (a < b)

    def write_file(self, c):
        self.out += self.cpu.read(c.arg(1), c.arg(2))
        self.cpu.u32(c.arg(3), c.arg(2))
        return 1

    def f(self, a):
        return struct.unpack('<f', self.cpu.read(a, 4))[0]

    def i(self, a):
        return struct.unpack('<i', self.cpu.read(a, 4))[0]

    def run(self, start, end, dfn):
        cpu = self.cpu
        esp = STACK_TOP - 0x2000
        cpu.write(esp, bytes(0xc0))
        cpu.u32(esp + 0x3c, dfn)
        cpu.uc.reg_write(UC_X86_REG_EBP, dfn)
        cpu.uc.reg_write(UC_X86_REG_ESP, esp)
        cpu.uc.emu_start(start, end, count=100_000_000)
        return esp

    def new_dfn(self, offset, index):
        cpu = self.cpu
        self.load(offset)
        dfn = cpu.alloc(0x1a8)
        cpu.write(dfn, bytes(0x1a8))
        cpu.call(CTOR, ecx=dfn, stack_args=(index, self.root))
        return dfn


def model(tree, ctor):
    """Expected MechDfn fields after the head, from the doc."""
    exp = dict(ctor)

    def get(k, node=tree):
        r = find(node, k)
        if r is None:
            return None
        v = r[0]
        if isinstance(v, list):
            v = v[0] if v else None
        return v

    def num(k, node=tree):
        v = get(k, node)
        return f32(float(v)) if isinstance(v, (int, float)) and not isinstance(v, bool) else None

    exp[0xc] = get('version')
    ch = get('chicken')
    if isinstance(ch, int):
        exp[0x18] = int(ch == 1)
    elif isinstance(ch, str) and ch.lower() in ('true', 'on', 'false', 'off'):
        exp[0x18] = int(ch.lower() in ('true', 'on'))
    s = {0x44: f32(1.2566368579864502), 0x58: 0.5, 0x5c: f32(1.3), 0x60: 1.5}
    for off, k in [(0x44, 'max_turn_rate'), (0x58, 'accel_rate'), (0x5c, 'decel_rate'), (0x60, 'turn_accel_factor')]:
        if num(k) is not None:
            s[off] = num(k)
    s[0x44] = f32(s[0x44] * 0.5)
    for off, k in [(0x148, 'crouch_height'), (0x158, 'lean_factor'), (0x15c, 'lean_speed'), (0x184, 'recoil_slide'),
                   (0x18c, 'aim_slerp_freq'), (0x1a0, 'aim_arm_pitch_factor')]:
        if num(k) is not None:
            exp[off] = num(k)
    for off, k in [(0x154, 'lean_angle'), (0x188, 'recoil_twist'), (0x190, 'aim_min_pitch'),
                   (0x194, 'aim_max_pitch'), (0x198, 'aim_min_yaw'), (0x19c, 'aim_max_yaw')]:
        if num(k) is not None:
            exp[off] = f32(num(k) * K)
    if num('recoil_bend') is not None:
        exp[0x180] = f32(math.sin(num('recoil_bend') * K))
    em = find(tree, 'engine_minmax')
    if em and isinstance(em[0], list) and len(em[0]) >= 2:
        exp[0x10], exp[0x14] = int(em[0][0]), int(em[0][1])
    exp[0x14] = 400
    tl = find(tree, 'torso_limits')[0]
    exp[0x174], exp[0x17c], exp[0x178] = (f32(f32(float(x)) * K) for x in tl[:3])
    sh = find(tree, 'torso_shake')[0]
    for off, k in [(0x160, 'pitch_amp'), (0x164, 'yaw_amp'), (0x168, 'roll_amp')]:
        if num(k, sh) is not None:
            exp[off] = f32(num(k, sh) * K)
    for off, k in [(0x16c, 'freq'), (0x170, 'damp_freq')]:
        if num(k, sh) is not None:
            exp[off] = num(k, sh)
    if isinstance(get('aim_ignore_arms'), int):
        exp[0x1a4] = int(get('aim_ignore_arms') == 1)
    return exp, s


INTS = {0xc, 0x10, 0x14}
BYTES = {0x18, 0x1a4}
FLOATS = [0x148] + list(range(0x154, 0x1a4, 4))


def read_fields(emu, dfn):
    out = {}
    for o in INTS:
        out[o] = emu.i(dfn + o)
    for o in BYTES:
        out[o] = emu.cpu.read(dfn + o, 1)[0]
    for o in FLOATS:
        out[o] = emu.f(dfn + o)
    return out


def close(a, b):
    if isinstance(a, int) or isinstance(b, int):
        return a == b
    return a == b or abs(a - b) <= 2e-7 * max(1.0, abs(a))


def config_bytes(emu, c):
    r = emu.cpu.read
    fields = {'mech': emu.i(c + 0x14), 'name': emu.cstr(emu.u32(c + 0x18)),
              'b1c_1f': bytes(r(c + 0x1d, 1)) + bytes(r(c + 0x1f, 2)), 'eng': r(c + 0x24, 5),
              'armor': r(c + 0x2c, 9) + r(c + 0x38, 0x2c), 'sinks': r(c + 0x64, 9), 'move': r(c + 0x70, 0x18)}
    items, node, sent = [], emu.u32(emu.u32(c + 8)), emu.u32(c + 8)
    while node != sent:
        it = emu.u32(node + 8)
        items.append((emu.u32(it), r(it + 4, 12), r(it + 0x10, 1), r(it + 0x14, 0xc)))
        node = emu.u32(node)
    fields['items'] = items
    fields['crc'] = emu.u32(c + 0x88)
    fields['count'] = emu.i(c + 0xc)
    return fields


DIAG = '--diag' in sys.argv


def fcall(emu, fn, ecx, args=()):
    """Calls a function returning on the x87 stack; returns that value as float."""
    cpu = emu.cpu
    out = cpu.alloc(4)
    code = b'\xb9' + struct.pack('<I', ecx)
    for a in reversed(args):
        code += b'\x68' + struct.pack('<I', a)
    code += b'\xb8' + struct.pack('<I', fn) + b'\xff\xd0' + b'\xd9\x1d' + struct.pack('<I', out) + b'\xc3'
    t = cpu.code(code)
    cpu.call(t)
    return emu.f(out)


def diag(emu, name_ptr):
    """Config ctor + Load without Validate, then each of Validate's checks."""
    cpu = emu.cpu
    c = cpu.alloc(0xd8)
    cpu.call(0x423d00, ecx=c, stack_args=(0xffffffff, name_ptr))
    # Load = 0x424600 minus its last call: run up to 0x424744
    esp = STACK_TOP - 0x3000
    cpu.u32(esp, 0)  # return address (never reached)
    cpu.u32(esp + 4, HANDLE)
    cpu.uc.reg_write(UC_X86_REG_ESP, esp)
    from unicorn.x86_const import UC_X86_REG_ECX
    cpu.uc.reg_write(UC_X86_REG_ECX, c)
    cpu.uc.emu_start(0x424600, 0x424744, count=10_000_000)
    eax = lambda: cpu.uc.reg_read(UC_X86_REG_EAX)
    cpu.call(0x4255b0, ecx=c)
    print(f'     crc {eax():#x} stored {emu.u32(c + 0x88):#x}')
    print('     weight', fcall(emu, 0x425300, c, (0xffffffff,)))
    print('     max mass', fcall(emu, 0x4846e0, emu.u32(c + 0x14)), 'mech', emu.i(c + 0x14))
    cpu.call(0x424220, ecx=c)
    print('     0x424220', eax() & 0xff)
    cpu.call(0x425ff0, ecx=c); a = eax()
    cpu.call(0x425fb0, ecx=c)
    print('     jjets', a, 'limit', eax())
    from unicorn import UC_HOOK_CODE
    from unicorn.x86_const import UC_X86_REG_ECX as RC
    seen = []
    h = cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, addr, size, _: seen.append((addr, uc.reg_read(RC) & 0xff)),
                        begin=0x4254d0, end=0x425591)
    cpu.call(0x4254d0, ecx=c, stack_args=(1,))
    cpu.uc.hook_del(h)
    print('     validate ->', eax() & 0xff, [(hex(a), v) for a, v in seen if a in
          (0x4254fe, 0x425532, 0x425544, 0x42555f, 0x42558d, 0x425588)])


def item_key(x):
    vt, body, _, tail = x  # tail = +0x14..+0x1f
    if vt == 0x5c0fc0:
        return vt, body, tail[:8]      # weapon index, tech
    if vt == 0x5bd180:
        return vt, body, tail[4:12]    # equipment kind, tech
    return vt, body, tail[:4]          # +0x14


def main():
    data = READER.read_bytes()
    emu = Emu(data)
    cpu = emu.cpu
    dfns = [(n, s) for n, s, _ in entries(data) if n.lower().startswith('dfn_')]
    # 1. the head
    bad = 0
    for name, start in dfns:
        idx = MECHS.index(name[4:-4])
        tree, _ = zrd.node(data[start:start + dict((n, l) for n, _, l in entries(data))[name]], 0)
        dfn = emu.new_dfn(start, idx)
        ctor = read_fields(emu, dfn)
        esp = emu.run(HEAD, HEAD_END, dfn)
        got = read_fields(emu, dfn)
        exp, s = model(tree, ctor)
        for o in sorted(got):
            if not close(exp[o], got[o]):
                bad += 1
                print(f'{name} +{o:#x}: model {exp[o]!r}, original {got[o]!r}')
        for o, v in s.items():
            g = emu.f(esp + o)
            if not close(v, g):
                bad += 1
                print(f'{name} stack +{o:#x}: model {v!r}, original {g!r}')
        tech = emu.i(esp + 0x54)
        cpu.call(0x484720, ecx=idx)
        if tech != cpu.uc.reg_read(UC_X86_REG_EAX):
            bad += 1
            print(name, 'stack +0x54 tech', tech)
        if name == 'dfn_strider.zrd':
            print('strider recoil_slide +0x184 =', got[0x184], '(ctor', ctor[0x184], ')')
        if name == 'dfn_elemental.zrd':
            print('elemental crouch', got[0x148], 'engine', got[0x10], got[0x14])
    print(f'1. head: {len(dfns)} files, {bad} mismatches')

    # 2. bad version: reader returns 0; the dtor then dereferences +0x20 == 0
    name, start = next(d for d in dfns if d[0] == 'dfn_madcat.zrd')
    dfn = emu.new_dfn(start, 9)
    root = emu.u32(dfn + 8)
    # find the int node after "version": top list -> first item list -> items
    top = emu.u32(root + 4)
    inner = emu.u32(top + 8 + 4)
    assert emu.cstr(emu.u32(inner + 8 + 4)) == b'version'
    vnode = inner + 16
    if emu.u32(vnode) == 4:  # (2) is a one-item list
        vnode = emu.u32(vnode + 4) + 8
    cpu.u32(vnode + 4, 3)
    cpu.call(READ, ecx=dfn)
    print('2. version 3: reader returns', cpu.uc.reg_read(UC_X86_REG_EAX) & 0xff, ', +0x20 =', emu.u32(dfn + 0x20),
          ', loaded =', cpu.read(dfn + 4, 1)[0])
    try:
        cpu.call(DTOR, ecx=dfn)
        print('   dtor returned normally')
    except UcError as e:
        print('   dtor faults:', e, 'at', hex(cpu.uc.reg_read(UC_X86_REG_EIP)))

    # 3. saved loadouts round trip
    cpu.write(DAMAGE_MODE, b'\0')
    cpu.u32(MSG_HOOK, 0)
    total = rejected = mism = tampered = 0
    shorts = {}
    for name, start in dfns:
        idx = MECHS.index(name[4:-4])
        emu.load(start)
        cpu.call(LIST_INIT)
        dfn = cpu.alloc(0x1a8)
        cpu.write(dfn, bytes(0x1a8))
        cpu.call(CTOR, ecx=dfn, stack_args=(idx, emu.root))
        emu.run(HEAD, CFG_END, dfn)
        lst = emu.u32(dfn + 0x20)
        first, last = emu.u32(lst + 0xc), emu.u32(lst + 0x10)
        for k in range((last - first) // 4):
            c = emu.u32(first + 4 * k)
            emu.out = bytearray()
            # only configs with +0x1c set are ever saved (0x424f30), and the checksum covers it
            cpu.write(c + 0x1c, b'\x01')
            cpu.call(SAVE, ecx=c, stack_args=(HANDLE,))
            n = cpu.uc.reg_read(UC_X86_REG_EAX)
            blob = bytes(emu.out)
            shorts.setdefault(len(blob) - n, 0)
            shorts[len(blob) - n] += 1
            if name == 'dfn_madcat.zrd' and k == 0:
                print(f'   madcat primary: Save returns {n}, wrote {len(blob)} bytes; head {blob[:48].hex(" ", 4)}')
            want = config_bytes(emu, c)  # after Save: +0x88 now holds the checksum
            for tamper in (False, True):
                b = bytearray(blob)
                if tamper:
                    b[len(b) - 9 - 8 - 4] ^= 1  # inside the armor points
                saved_data, saved_pos = emu.data, emu.pos
                emu.data, emu.pos = bytes(b), 0
                p = cpu.alloc(len(want['name']) + 1)
                cpu.write(p, want['name'] + b'\0')
                if DIAG and not tamper and k == 0:
                    diag(emu, p)
                    emu.pos = 0
                if DIAG and not tamper and k == 0:
                    from unicorn import UC_HOOK_CODE
                    hh = cpu.uc.hook_add(UC_HOOK_CODE, lambda uc, a, sz, _: print('     at', hex(a), 'eax', hex(uc.reg_read(UC_X86_REG_EAX)), 'pos', emu.pos),
                                         begin=0x4254d0, end=0x425591)
                cpu.call(LOAD_ONE, ecx=p, edx=HANDLE)
                if DIAG and not tamper and k == 0:
                    cpu.uc.hook_del(hh)
                got_c = cpu.uc.reg_read(UC_X86_REG_EAX)
                used = emu.pos
                emu.data, emu.pos = saved_data, saved_pos
                if tamper:
                    if bool(got_c) != (idx == 7):
                        print(f'   {name} {want["name"]}: tampered file accepted={bool(got_c)}')
                    tampered += 1
                    continue
                total += 1
                if used != len(blob):
                    print(f'   {name}: Load read {used} of {len(blob)} bytes')
                if not got_c:
                    rejected += 1
                    print(f'   {name} {want["name"].decode()}: rejected by Validate')
                    continue
                g = config_bytes(emu, got_c)
                for key in want:
                    a, b = want[key], g[key]
                    if key == 'items':
                        a, b = [item_key(x) for x in a], [item_key(x) for x in b]
                    if a != b:
                        mism += 1
                        print(f'   {name} {want["name"]} {key}: differs\n     {a!r}\n     {b!r}')
                if cpu.read(got_c + 0x1c, 1)[0] != 1:
                    print('   +0x1c not set after load')
        if name == 'dfn_madcat.zrd':
            print('   madcat: last config saved as', len(blob), 'bytes')
    print('   Save return short by {bytes: configs}:', shorts)
    print(f'3. saved loadouts: {total} configs round-tripped, {rejected} rejected, {mism} field mismatches; {tampered} tampered copies, only the elemental ones accepted')


if __name__ == '__main__':
    main()

# Exploration script (subagent, 2026-10-19): runs the original post-load pass
# 0x50ae60 (docs/drafts/anim-postload.md) in Unicorn after test_anim's load of
# each mission's gamez.zbd + anim.zbd, and records what it does: the calls it
# makes per def, the damage-callback registrations and their results, the
# functions reached, the status/flag changes. 0x549410 and 0x50a0e0 are
# stubbed in the second call (the data is already loaded).
#   .venv/bin/python tests/differential/explore/anim_postload_emu.py [mission ...]
import collections, json, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from test_anim import Emulated, ZBD, WORLDS, DEFS  # noqa: E402
from original import ROOT  # noqa: E402
from unicorn import UC_HOOK_BLOCK, UC_HOOK_CODE, UcError  # noqa: E402
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP  # noqa: E402

FUNCS = {int(p.stem, 16) for p in (ROOT / 'analysis-output' / 'decomp').glob('0x*.c')}
CALLS = {0x4f9e00: 'facade_walk', 0x509750: 'copy_tree_8000', 0x5832f0: 'damage_cb_register',
         0x508310: 'anim_reset', 0x507fc0: 'save_object_states', 0x508ae0: 'anim_start',
         0x5079b0: 'run_events', 0x508170: 'reset_finish', 0x4eb500: 'set_action_cb',
         0x4eb790: 'node_flag_40', 0x508760: 'start_inner'}
RETS_5832F0 = (0x583339, 0x58336d, 0x5833a5)


class Emu(Emulated):
    def sprintf(self, c):
        fmt = self.cstr(c.arg(1))
        assert fmt in (b'%s', b'_%s'), fmt
        s = fmt.replace(b'%s', self.cstr(c.arg(2)))
        c.write(c.arg(0), s + b'\0')
        return len(s)


def run(m):
    emu = Emu()
    cpu = emu.cpu
    world, anim = ZBD / m / 'gamez.zbd', ZBD / m / 'anim.zbd'
    head = emu.run(world, anim)
    assert struct.unpack_from('<i', head)[0] == 0, m
    count = struct.unpack('<h', cpu.read(0x73f72a, 2))[0]
    defs = emu.u(DEFS)
    before = [cpu.read(defs + 0x13c * i, 0x13c) for i in range(count)]
    cpu.uc.mem_map(0x70000000, 0x200000)  # the fake puffer/sound ids test_anim hands out
    head13 = emu.u(emu.u(0x5ea6d0 + 4 * 13))   # list 13 (worlds): first item
    world_node = emu.u(head13) if head13 else 0
    cpu.u32(0x73f738, world_node)            # 0x5090e0's node; the world is assumed
    st_world = (emu.index(world_node), emu.i32(0x5ea6d0))
    cpu.stub_function(0x549410, lambda c: 0)
    cpu.stub_function(0x50a0e0, lambda c: 0)
    st = collections.Counter()
    reached = collections.Counter()
    regs = []

    def ret_addr():
        return emu.u(cpu.uc.reg_read(UC_X86_REG_ESP))

    def on_block(uc, addr, size, _):
        if addr in FUNCS:
            reached[addr] += 1

    def on_code(uc, addr, size, _):
        if addr in CALLS:
            st[f'{CALLS[addr]}_from_{ret_addr():#x}'] += 1
            if addr == 0x5832f0:
                d = cpu.ecx
                regs.append([(d - defs) // 0x13c, emu.index(cpu.edx), cpu.arg(0), cpu.arg(1),
                             emu.u(cpu.edx + 0xbc) if cpu.edx else None])
        elif addr == 0x583370:   # node's record is inherited from a parent: a new one
            old = emu.u(cpu.edx + 0xbc) if False else None
            st['register_new_record_over_inherited'] += 1
        elif addr == 0x58333c:   # the node already has a record
            rec, slot = emu.u(cpu.edx + 0xbc) if False else 0, 0
            st['register_node_has_record'] += 1
        elif addr in RETS_5832F0:
            regs[-1].append(cpu.uc.reg_read(UC_X86_REG_EAX))

    cpu.uc.hook_add(UC_HOOK_BLOCK, on_block)
    for a in list(CALLS) + list(RETS_5832F0) + [0x583370, 0x58333c]:
        cpu.uc.hook_add(UC_HOOK_CODE, on_code, begin=a, end=a)
    emu.reports = []
    p = emu.alloc(16)
    cpu.write(p, b'anim.zrd\0')
    cpu.u32(0x73f720, 0)
    try:
        cpu.call(0x50ae60, ecx=p, count=2_000_000_000)
        result = 'returned'
    except (UcError, RuntimeError) as e:
        result = f'stopped: {e} at eip {cpu.uc.reg_read(UC_X86_REG_EIP):#x}'
    after = [cpu.read(defs + 0x13c * i, 0x13c) for i in range(count)]
    for i in range(1, count):
        b, a = before[i], after[i]
        if b[0x98] != a[0x98]:
            st[f'status_{b[0x98]}_to_{a[0x98]}'] += 1
        fb, fa = struct.unpack_from('<I', b, 0x94)[0], struct.unpack_from('<I', a, 0x94)[0]
        for bit in range(32):
            m_ = 1 << bit
            if (fb ^ fa) & m_:
                st[f'flag_{m_:#x}_{"set" if fa & m_ else "cleared"}'] += 1
        if (fb ^ fa) & 0x100:
            st['flag_0x100_defs'] = st.get('flag_0x100_defs', '') + a[:0x20].split(b'\0')[0].decode() + ' '
        diff = [o for o in range(0x13c) if b[o] != a[o]]
        st['changed_bytes_' + ','.join(sorted({f'{o & ~3:#x}' for o in diff}))] += 1
    ok = collections.Counter((r[3], r[-1] if len(r) > 5 else None) for r in regs)
    st.update({f'register_slot{s}_result{r}': n for (s, r), n in ok.items()})
    st['reports'] = len(emu.reports)
    st['report_lines'] = sorted({l for _, l in emu.reports})
    print(m, result, 'world node slot', st_world[0])
    for k, v in sorted(st.items(), key=lambda kv: str(kv[0])):
        print(f'  {k}: {v}')
    top = sorted(reached)
    print('  functions reached:', ' '.join(f'{a:#x}' for a in top))
    return regs


if __name__ == '__main__':
    for m in sys.argv[1:] or WORLDS:
        run(m)

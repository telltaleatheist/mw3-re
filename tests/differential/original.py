"""Run functions from the original Mech3.exe 1.2 in the Unicorn x86 emulator.

Only self-contained code works here: anything that calls Windows or DirectX has
no emulated system to call into. That covers the logic the rebuild ports.
"""
import hashlib
import os
import struct
from pathlib import Path

from unicorn import UC_ARCH_X86, UC_HOOK_CODE, UC_MODE_32, Uc
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EIP,
                                UC_X86_REG_ESP, UC_X86_REG_FS, UC_X86_REG_GDTR,
                                UC_X86_REG_SS, UC_X86_REG_DS, UC_X86_REG_ES)

ROOT = Path(__file__).resolve().parents[2]
EXE = ROOT / 'game' / 'mw3-1.2' / 'Mech3.exe'
SHA256 = '95bc2c114c9b2e5c5ada8e40ca78cc79470f862e45313ffad0d1e8e6d4d916bf'
IMAGE_BASE = 0x400000
STACK_TOP = 0x30000000
RETURN = 0x2fff0000   # a mapped address nothing executes; emulation stops there
HEAP = 0x10000000
STUBS = 0x2f000000  # one 16-byte slot per imported function we stand in for
CODE = 0x2f001000   # x86 written here for imports that are easier as machine code
TEB = 0x2e000000    # a thread block for fs:[0], the SEH chain MSVC code links into

# MSVCRT's _ftol, instruction for instruction: FISTP with the rounding mode set
# to chop, the 64-bit result in EDX:EAX.
FTOL = bytes.fromhex('55 8bec 83ec0c d97dfe 668b45fe 80cc0c 668945fc d96dfc df7df4 d96dfe'
                     '8b45f4 8b55f8 c9 c3')
IAT_FTOL = 0x5bb644


class Original:
    def __init__(self, exe=EXE):
        data = Path(exe).read_bytes()
        if hashlib.sha256(data).hexdigest() != SHA256:
            raise SystemExit(f'{exe} is not Mech3.exe 1.2')
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        count = struct.unpack_from('<H', data, pe + 6)[0]
        optsize = struct.unpack_from('<H', data, pe + 20)[0]
        end = 0
        sections = []
        for i in range(count):
            o = pe + 24 + optsize + i * 40
            vsize, va, rsize, raw = struct.unpack_from('<IIII', data, o + 8)
            sections.append((va, data[raw:raw + rsize]))
            end = max(end, va + max(vsize, rsize))
        size = (end + 0xfff) & ~0xfff
        self.uc.mem_map(IMAGE_BASE, size)
        self.uc.mem_write(IMAGE_BASE, data[:0x400])
        for va, body in sections:
            self.uc.mem_write(IMAGE_BASE + va, body)
        self.uc.mem_map(STACK_TOP - 0x100000, 0x100000)
        self.uc.mem_map(HEAP, 0x1000000)
        self.heap_next = HEAP
        self.uc.mem_map(STUBS, 0x1000)
        self.uc.mem_write(STUBS, b'\xc3' * 0x1000)
        self.stubs = {}
        self.uc.hook_add(UC_HOOK_CODE, self._stub_hit, begin=STUBS, end=STUBS + 0xfff)
        self.uc.mem_map(CODE, 0x1000)
        self.uc.mem_map(TEB, 0x2000)
        self.u32(TEB, 0xffffffff)
        # A GDT with flat 32-bit data (1) and an FS segment based at TEB (2).
        gdt = TEB + 0x1000

        def seg(base, limit, access):
            return (limit & 0xffff) | ((base & 0xffffff) << 16) | (access << 40) | \
                (((limit >> 16) & 0xf) << 48) | (0xc << 52) | ((base >> 24) << 56)
        self.write(gdt + 8, struct.pack('<Q', seg(0, 0xfffff, 0x92)))
        self.write(gdt + 16, struct.pack('<Q', seg(TEB, 0x1, 0x92)))
        self.uc.reg_write(UC_X86_REG_GDTR, (0, gdt, 0x18 - 1, 0))
        for reg in (UC_X86_REG_SS, UC_X86_REG_DS, UC_X86_REG_ES):
            self.uc.reg_write(reg, 1 << 3)
        self.uc.reg_write(UC_X86_REG_FS, 2 << 3)
        self.code_next = CODE
        # In play the x87 runs at 24-bit precision (Direct3D 6, no
        # DDSCL_FPUPRESERVE; CLAUDE.md). MW3_X87=24 starts every run there;
        # the default is the power-on 64-bit (0x037f).
        if os.environ.get('MW3_X87') == '24':
            self.fpu_control(0x007f)

    def fpu_control(self, cw):
        """Loads the x87 control word (0x037f power-on, 0x007f single precision)."""
        slot = self.alloc(4)
        self.u32(slot, cw)
        self.call(self.code(b'\xd9\x2d' + struct.pack('<I', slot) + b'\xc3'))

    def alloc(self, size):
        addr = self.heap_next
        self.heap_next += (size + 0xf) & ~0xf
        return addr

    def write(self, addr, data):
        self.uc.mem_write(addr, bytes(data))

    def read(self, addr, size):
        return bytes(self.uc.mem_read(addr, size))

    def u32(self, addr, value):
        self.uc.mem_write(addr, struct.pack('<I', value & 0xffffffff))

    def call(self, addr, ecx=0, edx=0, stack_args=(), count=50_000_000):
        """Call a __fastcall/__stdcall function; returns when it returns."""
        esp = STACK_TOP - 0x1000
        for arg in reversed(stack_args):
            esp -= 4
            self.u32(esp, arg)
        esp -= 4
        self.u32(esp, RETURN)
        self.uc.reg_write(UC_X86_REG_ESP, esp)
        self.uc.reg_write(UC_X86_REG_ECX, ecx & 0xffffffff)
        self.uc.reg_write(UC_X86_REG_EDX, edx & 0xffffffff)
        self.uc.emu_start(addr, RETURN, count=count)
        if self.uc.reg_read(UC_X86_REG_EIP) != RETURN:
            raise RuntimeError(f'{addr:#x} did not return within {count} instructions')

    def arg(self, n):
        """Stack argument n (0-based) of the function being entered."""
        esp = self.uc.reg_read(UC_X86_REG_ESP)
        return struct.unpack('<I', self.read(esp + 4 + 4 * n, 4))[0]

    @property
    def ecx(self):
        return self.uc.reg_read(UC_X86_REG_ECX)

    @property
    def edx(self):
        return self.uc.reg_read(UC_X86_REG_EDX)

    def stub_import(self, iat_slot, fn, pop_bytes=0):
        """Stand in for an imported Windows or C runtime function.

        `iat_slot` is the import table address the exe calls through, such as
        0x5bb0c4 for ReadFile. `fn(self)` returns EAX; `pop_bytes` is what the
        callee pops (stdcall), 0 for cdecl.
        """
        addr = STUBS + 16 * len(self.stubs)
        self.stubs[addr] = (fn, pop_bytes)
        self.u32(iat_slot, addr)

    def stub_function(self, addr, fn, pop_bytes=0):
        """Stand in for one of the exe's own functions: runs `fn(self)` on entry
        and returns EAX from it, popping `pop_bytes` of arguments."""
        self.stubs[addr] = (fn, pop_bytes)
        self.uc.hook_add(UC_HOOK_CODE, self._stub_hit, begin=addr, end=addr)

    def code(self, code):
        """Write a little x86 routine; returns its address."""
        addr = self.code_next
        self.write(addr, code)
        self.code_next += (len(code) + 0xf) & ~0xf
        return addr

    def import_code(self, iat_slot, code):
        """Stand in for an import with x86 machine code, such as FTOL."""
        self.write(self.code_next, code)
        self.u32(iat_slot, self.code_next)
        self.code_next += (len(code) + 0xf) & ~0xf

    def _stub_hit(self, uc, address, size, user_data):
        fn, pop_bytes = self.stubs[address]
        eax = fn(self)
        esp = uc.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', self.read(esp, 4))[0]
        uc.reg_write(UC_X86_REG_EAX, (eax or 0) & 0xffffffff)
        uc.reg_write(UC_X86_REG_ESP, esp + 4 + pop_bytes)
        uc.reg_write(UC_X86_REG_EIP, ret)

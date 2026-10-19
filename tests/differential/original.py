"""Run functions from the original Mech3.exe 1.2 in the Unicorn x86 emulator.

Only self-contained code works here: anything that calls Windows or DirectX has
no emulated system to call into. That covers the logic the rebuild ports.
"""
import hashlib
import struct
from pathlib import Path

from unicorn import UC_ARCH_X86, UC_HOOK_CODE, UC_MODE_32, Uc
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EIP,
                                UC_X86_REG_ESP)

ROOT = Path(__file__).resolve().parents[2]
EXE = ROOT / 'game' / 'mw3-1.2' / 'Mech3.exe'
SHA256 = '95bc2c114c9b2e5c5ada8e40ca78cc79470f862e45313ffad0d1e8e6d4d916bf'
IMAGE_BASE = 0x400000
STACK_TOP = 0x30000000
RETURN = 0x2fff0000   # a mapped address nothing executes; emulation stops there
HEAP = 0x10000000
STUBS = 0x2f000000  # one 16-byte slot per imported function we stand in for


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

    def call(self, addr, ecx=0, edx=0, stack_args=()):
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
        self.uc.emu_start(addr, RETURN, count=50_000_000)

    def arg(self, n):
        """Stack argument n (0-based) of the function being entered."""
        esp = self.uc.reg_read(UC_X86_REG_ESP)
        return struct.unpack('<I', self.read(esp + 4 + 4 * n, 4))[0]

    def stub_import(self, iat_slot, fn, pop_bytes=0):
        """Stand in for an imported Windows or C runtime function.

        `iat_slot` is the import table address the exe calls through, such as
        0x5bb0c4 for ReadFile. `fn(self)` returns EAX; `pop_bytes` is what the
        callee pops (stdcall), 0 for cdecl.
        """
        addr = STUBS + 16 * len(self.stubs)
        self.stubs[addr] = (fn, pop_bytes)
        self.u32(iat_slot, addr)

    def _stub_hit(self, uc, address, size, user_data):
        fn, pop_bytes = self.stubs[address]
        eax = fn(self)
        esp = uc.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', self.read(esp, 4))[0]
        uc.reg_write(UC_X86_REG_EAX, (eax or 0) & 0xffffffff)
        uc.reg_write(UC_X86_REG_ESP, esp + 4 + pop_bytes)
        uc.reg_write(UC_X86_REG_EIP, ret)

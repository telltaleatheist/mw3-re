"""Run functions from the original Mech3.exe 1.2 in the Unicorn x86 emulator.

Only self-contained code works here: anything that calls Windows or DirectX has
no emulated system to call into. That covers the logic the rebuild ports.
"""
import hashlib
import struct
from pathlib import Path

from unicorn import UC_ARCH_X86, UC_MODE_32, Uc
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP

ROOT = Path(__file__).resolve().parents[2]
EXE = ROOT / 'game' / 'mw3-1.2' / 'Mech3.exe'
SHA256 = '95bc2c114c9b2e5c5ada8e40ca78cc79470f862e45313ffad0d1e8e6d4d916bf'
IMAGE_BASE = 0x400000
STACK_TOP = 0x30000000
RETURN = 0x2fff0000   # a mapped address nothing executes; emulation stops there
HEAP = 0x10000000


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

"""Pirate's Moon's chat line (0x1f) against the port (docs/pm-map.md, the in-game types): the handler 0x4725a0
(MW3's 0x470490, the same code and strings, only its arguments passed differently) against the port's net_chat_in
through probe_mech_net_chat: "<" the sender's name, " ", the message table's 0x899 (to the team, the message's +4
bit 0) or 0x89a (to all), "> " and the text, on the HUD for 5 s (0x410750). Stood in for on both sides: the
players' names (0x5a9960: "P<id>" for 1..8, none else) and the message table (0x596e50: "M<id>"); MFC's CString
calls modelled. Texts are printable: the port turns control characters into "?" (FIX (A), a peer's text).

    cmake --build build --target pmprobe && MW3_EXE=pm .venv/bin/python tests/differential/test_pm_net_chat.py
"""
import ctypes
import os
import random
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
os.environ['MW3_EXE'] = 'pm'
import test_pm_zar  # noqa: E402

CHAT_IN, PLAYER_NAME, MESSAGE, HUD_LINE = 0x4725a0, 0x5a9960, 0x596e50, 0x410750
CS_FROM, CS_ADD, CS_PLUS_SC, CS_PLUS_CS, CS_ASSIGN, CS_FREE = 0x5ceabe, 0x5ceb30, 0x5ceac4, 0x5ceb24, 0x5ceaac, 0x5ce8d8


class Exe:
    """A CString is a pointer to its text; each change makes a new one."""

    def __init__(self):
        z = self.z = test_pm_zar.Emulated()
        self.uc = z.cpu._uc
        s = self.s
        stubs = {CS_FROM: (lambda c: self.set(c.ecx, s(c.arg(0))), 4),
                 CS_ADD: (lambda c: self.set(c.ecx, s(self.r32(c.ecx)) + s(c.arg(0))), 4),
                 CS_PLUS_SC: (lambda c: self.set(c.arg(0), s(c.arg(1)) + s(self.r32(c.arg(2)))), 12),
                 CS_PLUS_CS: (lambda c: self.set(c.arg(0), s(self.r32(c.arg(1))) + s(c.arg(2))), 12),
                 CS_ASSIGN: (lambda c: self.set(c.ecx, s(self.r32(c.arg(0)))), 4),
                 CS_FREE: (lambda c: 0, 0),
                 PLAYER_NAME: (lambda c: self.text(b'P%d' % c.arg(0)) if 1 <= c.arg(0) <= 8 else 0, 0),
                 MESSAGE: (lambda c: self.text(b'M%d' % c.arg(0)), 0),
                 HUD_LINE: (self.hud, 0)}
        for a, (fn, pop) in stubs.items():
            z.native_stub(a, fn, pop)

    def r32(self, a):
        return struct.unpack('<I', bytes(self.uc.mem_read(a, 4)))[0]

    def s(self, a):
        return bytes(self.uc.mem_read(a, 0x200)).split(b'\0')[0] if a else b''

    def text(self, b):
        a = self.z.alloc(len(b) + 1)
        self.uc.mem_write(a, b + b'\0')
        return a

    def set(self, obj, b):
        self.uc.mem_write(obj, struct.pack('<I', self.text(b)))
        return obj

    def hud(self, c):
        self.line = b'%d|' % struct.unpack('<i', struct.pack('<I', c.arg(0)))[0] + self.s(c.arg(1))
        return 0

    def run(self, frm, msg):
        self.line = b'(none)'
        m = self.text(msg)
        self.z.call(CHAT_IN, args=(frm, m))
        return self.line


def main():
    probe = test_pm_zar.load_pm_probe()
    probe.probe_mech_net_chat.argtypes = [ctypes.c_uint32, ctypes.c_char_p, ctypes.c_int32, ctypes.c_char_p,
                                          ctypes.c_int32]
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 1000
    exe = Exe()
    rng = random.Random(31)
    seen = {}
    for i in range(count):
        frm = rng.randrange(0, 11)
        flags = rng.getrandbits(32)
        text = bytes(rng.randrange(0x20, 0x7f) for _ in range(rng.randrange(0, 60)))
        msg = struct.pack('<HHI', 0x1f, 8 + len(text) + 1, flags) + text + b'\0'
        theirs = exe.run(frm, msg)
        out = ctypes.create_string_buffer(512)
        n = probe.probe_mech_net_chat(frm, msg, len(msg), out, 512)
        mine = out.raw[:n]
        if mine != theirs:
            print(f'case {i} (from {frm}, flags {flags:#x}):\n  exe  {theirs!r}\n  port {mine!r}', flush=True)
            os._exit(1)
        key = (flags & 1, 1 <= frm <= 8)
        seen[key] = seen.get(key, 0) + 1
    print(f'{count} chat lines identical, e.g. {theirs!r}; (to the team, a named sender): {dict(sorted(seen.items()))}',
          flush=True)
    os._exit(0)


if __name__ == '__main__':
    main()

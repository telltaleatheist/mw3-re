# The stack-pointer blit trick

Found with REA + Ghidra on `Mech3.exe` 1.2 (sha256 `95bc2c11…16bf`), 2026-10-18.

## Where it is

MW3's 16-bit software renderer draws scanlines by pointing ESP at the
destination and using `PUSH`, which writes 2 or 4 bytes and steps backwards in
one instruction. The renderer lives at roughly `0x55c000`–`0x56e000`; every
routine there that touches the scanline pointer `0x72c278` belongs to it.

How it was found: a byte search for instructions that load ESP
(`8B E0+r`, `89 /r` with ESP as destination, `8B 25 addr`) after dropping the
normal `mov esp, ebp` epilogues. About 30 functions remain, all in that range,
all loading ESP from `0x72c278` and restoring it from `0x74a338`. Everything
else was a false match inside a longer instruction.

## Globals

| Address | Meaning |
|---|---|
| `0x72c278` | Destination scanline pointer (16-bit pixels) |
| `0x72c268` | Texture base (16-bit texels) |
| `0x72c270` | u step per pixel (fixed point, integer part in bits 20+) |
| `0x72c274` | v step per pixel |
| `0x5efb98` | v mask (selects the texture row bits) |
| `0x74a338` | Saved real ESP while the span runs |

## Two examples

`0x564080`, solid span fill. Color in ECX (replicated to 32 bits), count in EDX.
ESP = scanline + 2·count, then `PUSH AX` / `PUSH EAX` pairs fill backwards. It
restores ESP from EBP, so it never touches `0x74a338`.

`0x56cb40`, textured span (`__fastcall`: u in ECX, v in EDX, count on stack):

```asm
PUSH EBP
MOV  [0x74a338], ESP        ; save the real stack
MOV  ESP, [0x72c278]        ; ESP = scanline
ADD  ESP, count*2           ; start at the right end
loop:
  index = ((u >> 20) & 0x3ff) + ((v & mask) >> 10)
  u += du;  v += dv
  PUSH word [texture + index*2]   ; write one pixel, step left
  JNZ loop
MOV  ESP, [0x74a338]        ; put the real stack back
```

Ghidra decompiles this one correctly despite the trick, with the warning
`Unable to track spacebase fully for stack`. Its pseudocode, cleaned:

```c
// Mech3.exe 1.2 @ 0x0056cb40: textured span, 16-bit
void span_textured(uint32_t u, uint32_t v, int count) {
    uint16_t *dst = scanline + count;
    while (count--) {
        uint32_t index = ((u >> 20) & 0x3ff) + ((v & v_mask) >> 10);
        u += du; v += dv;
        *--dst = texture[index];
    }
}
```

## What the rebuild does

The span routines become ordinary pointer writes like the one above: same
arithmetic, same order, same output, no ESP. None of them has a direct caller;
they are reached through a function-pointer table (the span variant is chosen
at run time), which is the next thing to map.

This path is the software renderer only. The Direct3D path does not use it.

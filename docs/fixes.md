# Changes from the original

The rebuild stays as close to the original code as possible and changes it only
to fix what (A) cannot run on macOS or modern Windows, or (B) breaks on modern
Windows without helpers such as DDrawCompat. Each change is also marked `FIX`
in the code next to the original address.

| Where | Original | Why it changes | Rebuild |
|---|---|---|---|
| Span routines, 0x564080, 0x56cb40 | ESP pointed at the pixels, written with PUSH | (A) x86-only; fragile anywhere, since anything using the stack meanwhile writes into the picture | Same writes in the same order through a pointer; emulator-verified identical |
| Frame clock, 0x575a00 | Float seconds from `GetTickCount()`, ms since boot | (B) after ~10 days of uptime a float can only step 64 ms, so frames quantise and the game stutters | Ticks counted from game start; the float arithmetic is unchanged and emulator-verified identical |
| CD music, zsnd_cd.cpp | MCI `cdaudio` playing CD tracks | (A) no MCI, and machines have no CD drive | Same API and track numbers; `music/trackNN.(ogg\|flac\|wav\|mp3)` stands in for each track |
| zReader lists, zreader.cpp | `find` can return the slot past a list's end, and the getters read item 1 of an empty list | (A) reads past a heap block: whatever the CRT heap holds in the original, out of bounds in C++ | Each list gets one spare zeroed slot, read as "no value"; everything else emulator-verified identical on all of `reader.zbd` |
| Archives, zutl_zar.cpp | `CreateFileA`, `ReadFile`, `SetFilePointer` on Windows paths | (A) Win32 only, `\\` separators | C stdio, same calls in the same order; `\\` becomes `/` |
| Pools: models, nodes, list items (`gmod_const`, `cls_node`, `list`) | Fixed-size slots holding 32-bit pointers, in blocks reached through a `realloc`ed pointer array | (A) pointers are 64-bit on modern systems, so the file's slot layout cannot hold them | Each slot keeps its bytes as read; the pointers the exe writes over them live in typed fields; blocks are owned arrays. Emulator-verified identical, pointers compared as indices |
| Engine abort, 0x4eada0 | Leaves the DirectDraw mode, `MessageBoxA`, prints, `ExitProcess(0)` | (A) Win32 | `platform::Alert` shows the message, then the same prints and `exit(0)` |
| `rand()` | MSVCRT's | (A) every C library's `rand()` differs | `crt::msvc_rand`, MSVCRT's exact sequence |

Notes on the fixes:

- Frame clock: in the original the product `ticks * 0.001f` stays on the x87
  stack at 80 bits while it is stored to `now_s` and subtracted from `prev_s`,
  so `raw_dt` is rounded once, from the unrounded product. The port computes it
  in double, which gives the same result while ticks stay under 2^29 (six days).
- zReader: the original's `malloc(count * 8)` for a list stored with count 0
  also wrote slot 0 past the block; the spare slot covers that too.

Noted but not yet ported:

- A mech file with a bad `version` crashes the original in the definition's
  dtor (0x46b9b0 saves loadouts that were never made). The port's `load_dfn`
  returns no definition instead, since the dtor is not ported yet; when it is,
  decide whether to keep the crash or guard it as a FIX (B).

- The app constructor (0x47f7a0) quits when the DirectX version reads below 6.
  Whether modern Windows reports this correctly is unchecked; the rebuild has
  no DirectX to check.

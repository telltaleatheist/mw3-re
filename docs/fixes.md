# Changes from the original

The rebuild stays as close to the original code as possible and changes it only
to fix what (A) cannot run on macOS or modern Windows, or (B) breaks on modern
Windows without helpers such as DDrawCompat, plus real bugs that break play
(Owen, 2026-10-19), marked `FIX (bug)`, and the copy-protection and
anti-tamper traps (Owen, 2026-10-20, `protection.md`), marked `FIX (trap)`.
Each change is also marked `FIX` in the code next to the original address.

| Where | Original | Why it changes | Rebuild |
|---|---|---|---|
| Span routines, 0x564080, 0x56cb40 | ESP pointed at the pixels, written with PUSH | (A) x86-only; fragile anywhere, since anything using the stack meanwhile writes into the picture | Same writes in the same order through a pointer; emulator-verified identical |
| Frame clock, 0x575a00 | Float seconds from `GetTickCount()`, ms since boot | (B) after ~10 days of uptime a float can only step 64 ms, so frames quantise and the game stutters | Ticks counted from game start; the float arithmetic is unchanged and emulator-verified identical |
| CD music, zsnd_cd.cpp | MCI `cdaudio` playing CD tracks | (A) no MCI, and machines have no CD drive | Same API and track numbers; `music/trackNN.(ogg\|flac\|wav\|mp3)` stands in for each track |
| zReader lists, zreader.cpp | `find` can return the slot past a list's end, and the getters read item 1 of an empty list | (A) reads past a heap block: whatever the CRT heap holds in the original, out of bounds in C++ | Each list gets one spare zeroed slot, read as "no value"; everything else emulator-verified identical on all of `reader.zbd` |
| Archives, zutl_zar.cpp; every file open | `CreateFileA`, `ReadFile`, `SetFilePointer` on Windows paths relative to the install, found along search paths (0x5762b0) | (A) Win32 only, `\\` separators | C stdio, same calls in the same order; files are found through ordered mounts and a write dir (`zutl_path`, `assets-layer.md`), so replacement files can sit in front of the install or stand in for it |
| Pools: models, nodes, list items (`gmod_const`, `cls_node`, `list`) | Fixed-size slots holding 32-bit pointers, in blocks reached through a `realloc`ed pointer array | (A) pointers are 64-bit on modern systems, so the file's slot layout cannot hold them | Each slot keeps its bytes as read; the pointers the exe writes over them live in typed fields; blocks are owned arrays. Emulator-verified identical, pointers compared as indices |
| Engine abort, 0x4eada0 | Leaves the DirectDraw mode, `MessageBoxA`, prints, `ExitProcess(0)` | (A) Win32 | `platform::Alert` shows the message, then the same prints and `exit(0)` |
| Mech definition dtor, 0x46b9b0 | Saves the player's loadouts (0x424f60) with no check that any exist | (bug) a mech file with a bad or missing `version` stops the read before the loadouts are made, and the save then crashes the game (emulator: 0x424f9b) | The save is skipped when there are no loadouts; the file simply fails to load. Decided with Owen 2026-10-20 |
| Animation prerequisites, 0x50aaa3 | The search for a prerequisite animation stops one short of the list | (bug) the mission's last animation can never be waited for | The whole list is searched. No shipped mission names the last one, so all six still match the original exactly |
| Saved loadouts, item list 0x471540 | Counts a damaged item count down past the file's end, reading nothing | (bug) a corrupted `configs\<mech>.mw3` can hang the game at load for up to 4 billion reads | Stops at the end of the file; the same items result (emulator-checked by making the original stop there too) |
| Length-prefixed strings, 0x576460 | `malloc(length + 1)` for whatever length the file holds | (bug) a damaged length makes it allocate up to 4 GB and crash when that fails | The buffer stops at the file's end, which holds the same string |
| Saved loadouts, item load 0x472b20 | An id cut off by the file's end is whatever ECX held: the item's own address | (A) a heap address | INT32_MAX, which, like the address, no weapon, equipment or engine table knows (the test pins the original's to the same value) |
| Engine table, 0x432f40 / 0x432fa0 | An index past the 79 entries reads whatever follows the table (a corrupted save, or the kind-4 item's clamp to 79) | (A) memory past the table | Weight and rating 0 (the test zeroes the original's out-of-range entry the same way) |
| Saved loadouts, Load 0x424600 | Validate(true): a loadout whose CRC does not match is silently dropped | (trap) anti-tamper, `protection.md` | Validate(false): edited loadouts load if they follow the rules (weight, slots, jump jets, armor); the CRC is still computed and written, so files stay loadable by the original game |
| Disc check, 0x482f90 / 0x575d40 | Startup loop until `video\c1.avi` is on a CD-ROM drive | (trap) and (A): no CD drives | Left out of the main window (0x482d10, `mech3/app`); the importer copies the movies |
| Exe self-check, 0x4854c0 | CRC of the mech roster and structure tables, else exit after the intro | (trap) | Left out of InitInstance and 0x47fed0 (`mech3/app`); the Exit-after-intro flag 0x608f70 is never set |
| Main loop, 0x58fd70 | `PeekMessageA`, `PumpMessage`, `MsgWaitForMultipleObjects`, `PostQuitMessage`, `SetThreadPriority(HIGHEST)` | (A) Win32 | The same loop, as written, over `platform::MessagePump`; emulator-verified identical (`test_app_loop.py`, game-loop.md) |
| App start and window, 0x47f7a0 / 0x480000 / 0x480570 / 0x482d10 | Mech3Msg.dll, the DirectX 6 check, RunDir, the window class, `CreateEx` clamped to the work area, the debug menu, `UpdateWindow`, `MessageBoxA` + `ExitProcess` on failures | (A) Win32 / DirectX | The same calls in the same order through `mech3::Engine` hooks for the front end (`test_app_states.py`) |
| Missions, 0x480c70 / 0x481fc0 | `SystemParametersInfoA(SPI_SETSCREENSAVERRUNNING)` around a network mission (Windows 9x: no Alt-Tab, Ctrl-Alt-Del) | (A) | A hook (`lock_system_keys`) |
| Multiplayer rules check, 0x46fc08 | Host ends the game when its locked rules block changes | (trap) | To be left out when the network code is ported |
| Matrix multiply mode 2, 0x51fee0 | Mode 2 (3x3 only, reached through 0x4ed280 from 0x5047f5 and 0x504979) copies leftover stack bytes into the translation | (bug) undefined values in a node's matrix | Translation 0 (`zmth::multiply`, render-matrix.md) |
| Model draw, 0x53f2e0 | Polygons are copied into 64-vertex work buffers unchecked | (bug) a longer polygon overruns the buffers; only the inout areas (up to 159) have them, and those are deleted at mission load | Polygons over 64 vertices are skipped (render-modeldraw.md) |
| Heat-sink critical, 0x460d50 | The callout waits for 0x45fbc0 to report success, which it never does | (bug) losing a sink is silent (`heat.md`, `damage.md`) | Success is a sink actually lost: the "heat sink" callout and the critical sound play. The damage test runs the original with 0x45fbc0 reporting it the same way |
| Equipment critical, 0x461211 | When `rand()` lands above the summed weights (r = 1.0 and rounding), the item pointer is the random float's bits | (bug) a crash | The last candidate is taken |
| `rand()` | MSVCRT's | (A) every C library's `rand()` differs | `crt::msvc_rand`, MSVCRT's exact sequence |
| Direct3D draw table, zvid_ddd3d.c (0x57bf80, 0x57c700, 0x57ce20, 0x57d160, 0x57d690, flushes 0x57e110 / 0x57e4c0, frame calls 0x57a740 / 0x57a7e0 / 0x5777f0 / 0x57a850, FOGCOLOR 0x57bcf0, texture records 0x57b130) | IDirect3DDevice3 SetRenderState / SetTextureStageState / SetTexture / DrawPrimitive, BeginScene / EndScene, DirectDraw Blt fills, Flip, two surfaces and a Load per texture | (A) Direct3D 6 does not exist on macOS and is legacy on Windows | The same calls under the same caches set a `platform::DrawState` and go to `platform::Render3D` (draw, clear, present, create_texture as RGBA8 decoded from the 16-bit surface the original filled); HRESULT reports and lost-surface restores go. Emulator-verified identical as DrawState and vertex bytes (`test_drawtable.py`, drafts/drawtable.md) |
| Script files, zinterp.cpp (0x595990, 0x596620, 0x596810, 0x5763b0) | `fopen(name, "r")` text mode, the archive on the `.;zbd` search path, `_stat` relative to the working directory, `_mkdir`, `DeleteFileA` | (A) Windows paths and MSVCRT text mode | Through the mounts (`zutl_path`); loose files read binary with MSVCRT's text-mode rules (CR LF to LF, Ctrl-Z ends the file) done by hand. Emulator-verified with loose files, CR, Ctrl-Z and NUL (`test_interp.py`) |
| Script lines, zinterp.cpp (0x5954a0 / 0x5955e0, 0x5956e0, 0x595850) | A 1 KB line buffer, a 64-byte macro name and a 1 KB expansion buffer, all unbounded; 16 argv slots with no bound (the 17th token overwrites the macro table pointer) | (bug) a long line, name or expansion, or too many tokens, corrupts memory | The line buffer grows, the name has no limit, the expansion stops at 4 KB, tokens past 16 are dropped; the shared expansion buffer itself is kept (its aliasing is original behaviour) |
| Archive lines, 0x5955e0 / 0x596620 | A short read leaves the line size as the FILE pointer; the size and argc are trusted; a huge script count wraps the directory's size | (trap) a damaged interp.zbd crashes or overruns | A short read or a size over 1 MB ends the script; argc is capped at 16 and tokens stop at the text's end; a count outside 0..1M rejects the archive |
| Freed line, exec_line 0x595510 / `source` 0x590e80 | After a `source` whose loose file ends in an unterminated failing line, the error hook (0x410770) reads argv[0] of that line, already freed | (bug) use after free | Lines are freed when the outermost line ends; the hook sees the text the freed block still held in the emulator |
| Script commands, 0x591300 | Crashes on: `FreeNode` of a missing node, `ModelNew` without a second word, `var NAME` without an operator, `LODSetRange` without a node or data, `Object3DRegisterTexturesToWorld` / `Object3DSetMorphVertex` on a node without a model; `LightSet{Saturated,Static,Subdivide}` log a `%s` with no argument; a polygon of more than 35 vertices or 38 UVs writes past the 0x1d8-byte build block (from 11 on they already run into the next fields, as kept) | (bug) none reached by the shipped scripts | The command does nothing (or the word counts as no match); argv[0] is passed for the `%s`; writes past the block are dropped (the model pointer at +0x1d4 is a typed field, so a UV landing there leaves it). The scroll-off null out pointer (0x596370) is passed on unchanged until node_get_model is ported |
| String variables, 0x596020 | Prints the first character where the format wants the string | (bug) none is registered | The string is printed |
| Weapon effect blocks, 0x584bb0 | `SOUND` and `BOUNCE_SOUND` lists are copied into four slots each with no bound | (bug) a fifth sound writes over the next fields | At most four are kept (no shipped weapon has more); the test keeps its variants within four |
| Damage animations, 0x58459c | `DAMAGE_ANIM_ON_HEALTH` pairs are copied into room for four with no bound | (bug) more overwrite the destroy and kill animations | At most four are kept (no shipped weapon has any) |
| Projectile pool size, 0x58468e | A def's shots in the air are counted from a local only `RANGE` sets | (bug) a def without RANGE uses the previous def's range, the first one stack garbage | The def's own range (500 by default); every shipped weapon has RANGE, so the pool is the same 164. The test patches the original the same way |
| LOCK_ON_LEAD, 0x583fb9 | The second value is read when the first value's bits exceed 3, not when the list has two values | (bug) a typo: `[eax+0xc]` for `[eax+4]` | The count decides; no shipped weapon has LOCK_ON_LEAD. The test patches the original the same way |
| Weapon manager with no group, 0x4b0c30 / 0x4b0790 / 0x4b07e0 / 0x4b08c0 | Trigger, next and previous weapon and the network release call through the selected group without checking it | (bug) group -1 is a null call (crash); the manager starts at group 0, so only a weapon moved to group -1 gets there | Nothing happens without a group. The test never reaches it on either side |
| Gravity shots that accelerate, 0x585d61 | The velocity is rebuilt with ECX = 1 as the base vector | (bug) a crash; no shipped weapon has gravity | The shot's base velocity, as in the branch without gravity |
| Air burst record, 0x582320 | The record built for a burst in the air leaves the surface normal unset (stack bytes) | (bug) the surface animation's orientation is whatever the stack held | A zero normal. Only LANDMINE has a surface animation in its default impact; the test leaves it out of the air bursts |

Notes on the fixes:

- Frame clock at 24-bit: in play the x87 runs at single precision (CLAUDE.md),
  so the original rounds `ticks * 0.001` to a float before the subtraction:
  frame times jitter by about 0.06 ms once the game has run some minutes, and
  a zero-millisecond frame comes out as about 1.6e-6 s. The port keeps double
  (as the 64-bit reading below), part of the frame clock fix; ztimer's test
  passes at 64 bits and shows the difference at 24 (`MW3_X87=24`).
- Frame clock: in the original the product `ticks * 0.001f` stays on the x87
  stack at 80 bits while it is stored to `now_s` and subtracted from `prev_s`,
  so `raw_dt` is rounded once, from the unrounded product. The port computes it
  in double, which gives the same result while ticks stay under 2^29 (six days).
- zReader: the original's `malloc(count * 8)` for a list stored with count 0
  also wrote slot 0 past the block; the spare slot covers that too.

Noted but not yet ported:

- The app constructor (0x47f7a0) quits when the DirectX version reads below 6.
  Whether modern Windows reports this correctly is unchecked; the rebuild asks
  its `directx_version` hook and ignores the answer.

## Original bugs still to fix

Bugs whose intent is clear, fixed as the port reaches the code:

- Data typos, fixed by the importer (the readers stay as written, the patched
  data goes through them):
  - Firefly skin 5 is labelled `MSG_SKIN_VARIANTD`; should be `VARIANTE`
    (SLATE).
  - Sunder Alternate B: `extra (120 120)` sits beside the LRM20's `ammo`
    instead of inside it, losing 240 missiles.
  - Strider `recoil_slide ("1.5.0")` is text; should be `1.5`, as on the
    Cauldron, Bushwacker and Champion.
  - Mission c1's script calls `Object3DSetScrllAlways` on `ocean52`; should be
    `Object3DSetScrollAlways`, so the water scrolls.

- CPU speed, 0x5988c0 (Intel's `cpuspeed`): divides by zero on PCs whose
  performance counter runs at CPU speed, a startup crash; the 1.2 NoCD exe
  NOPs the divides (`protection.md`). FIX (B): do not port the measurement.
- Software 16-bit line, 0x563b80: no clipping; the NoCD exe clamps to
  1024x768. FIX (B): clip to the real surface.

- Point-lit textured vertices, 0x57d160 / 0x57d690: the colour rounding adds
  and subtracts 1.5 x 2^52 (a double trick), which yields 0 at the 24-bit
  precision the game runs at, so textured vertices lit by a coloured point
  light come out black (emulator; render-drawtable.md). Check in play
  (explosions or muzzle flashes near textured surfaces); then likely a
  FIX (bug): round properly.
- Translucent untextured polygons on lit or fogged models, 0x57c700: coloured,
  then neither queued nor drawn, so fake shadows vanish there (emulator; check
  in play).

- Cloud shading, 0x543ac0: the table index uses the same 1.5 x 2^52 rounding,
  so at 24-bit it is always 0 and the moving cloud pattern on the sun is a
  constant (c1: a flat 70%) on Direct3D (emulator, render-lighting.md). If
  confirmed in play, the hardware renderer never showed moving cloud
  shadows; restoring them would be a FIX (bug).
- Point lights just inside their near range, 0x545720: below near squared the
  squared distance is compared as a distance, so a vertex can go dark
  (near 32 / far 64: 0.875 at 6, 0 from 10 to 31, 1 at 32). How often shipped
  lights show it is not checked.

## Candidates for Owen

These look like bugs, but fixing them is a guess at intent that changes balance:

- Lava heat, 0.05 per frame (`heat.md`): frame-rate dependent, so faster
  machines cook faster. Options: a fixed per-second rate matching some
  reference frame rate (1.5/s at 30 fps, 3/s at 60 fps), or keep per frame.
- Wading (`heat.md`): the depth ratio is clamped with max where min was likely
  meant, so any depth gives the full x8 cooling. Fixing it makes shallow
  water cool less.
- Water cooling checks only the left foot.
- Mad Cat damage list: two extra zeros shift every damage mask by two, so two
  textures get none and two get a mask of the wrong size (dropped). Removing
  the zeros restores the damage look; the Supernova's one extra item likewise.
- Elemental armor: the file hands out 11 points for a ton worth 16.
- Missing animation object, 0x50a0e0: one prerequisite naming an object not
  in the level clears all of the animation's prerequisites, so it fires
  without waiting. Dropping only the broken one, or never firing, are both
  plausible intents. Whether any shipped mission hits it is not yet checked.
- ZAPS_HUD, 0x4af71b: the second value reads the first value's slot again, so
  the ERPPC's `(0.5 2.0)` HUD static is (0.5, 0.5). Reading the second value
  is the evident intent, but what the second number does on the HUD
  (0x43be70) is not traced; the port keeps the original.
- Pock marks: `weapons.zrd` gives the autocannons, Gauss, missiles and MGUN a
  `SURFACE_ANIMATION` on `dirt`, `mud` and `snow`, kinds the impact table
  (0x5edbc0: default, water, seafloor, quicksand, lava, fire) does not have,
  so they never show. Mapping those names to the soils that exist would
  restore them; which soil each should be is a guess.
- Missiles that miss vanish at RANGE without exploding (every shipped missile
  has EXPIRES, 0x5860fb); keep, or let them burst.
- The plain PPC is defined but missing from the mech lab's table; every mech
  maximum engine is forced to 400; only a weapon's first group counts. These
  look deliberate and stay unless Owen says otherwise.


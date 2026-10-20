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
| Missing animation object, 0x50a0e0 | One node prerequisite naming an object not in the level sets the def's prerequisite count to 0 | (bug) the animation then fires without waiting for any of its conditions | Only the broken entry is dropped; the count to meet comes down only if fewer counting entries are left. Decided with Owen 2026-10-20. No shipped mission reaches it (all six checked); `anim().fix_prerequisites = false` keeps the exe's path, which test_anim.py compares exactly, plus a doctored c2 file showing both |
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
| Projected shadows and lights, zvid_ddd3d.c (0x57eeb0, 0x57f2a0, 0x57f860, 0x57ea20, 0x57fe20) | DrawPrimitive with FVF `0xc4 \| (n + 1) << 8`: one interleaved buffer with a UV pair per shadow, drawn again per shadow with SRCBLEND ZERO / DESTBLEND SRCCOLOR, COLOROP SELECTARG1 and TEXCOORDINDEX k | (A) Direct3D 6 | `Render3D::draw_multi` (default: the selected UV set as a TLVertex, white diffuse under `color_select`); `DrawState` gains `color_select` and `texcoord`, `multiply` follows the blend factors. Emulator-verified identical (`test_shadows.py`, render-shadows.md) |
| Projected-light subdivision, 0x547160 (from 0x547740) | Splits into fixed arrays of 2,048 triangles and 6,144 vertices with no check | (bug) a large polygon in a light's falloff band overruns them into the light count and the vertex records | A split that would not fit is not made (the triangle stays whole); the test applies the same to the original at that point. The unbounded search for the shared vertex across the edge stops at 3 (it always finds it) |
| Shadowed draws, 0x57eeb0.. | Read the base texture's address modes through the pointer, null or not | (bug) a crash on a textured polygon whose material lost its texture | The address modes are skipped for a null texture |
| Shadow image size, 0x542d30 | Resizing an image clears the caster's image pointer, then frees through it (reads address 0x10) | (bug) a crash; unreached in 1.2 (only a new caster gets a size) | The old image is freed (render-shadows.md) |
| Polygon filler, 0x55d010 | A level edge just at or above a row centre is listed without x or slope (stack garbage, used for that row); with no vertices the second edge walk never ends | (bug) uninitialised memory; a hang | The edge's end x and no slope (the test makes the original do the same); a polygon without vertices is not filled. Its 16.16 rounding runs at 53 bits by default: in play on Windows 11 + DDrawCompat the shadows are visible (Owen, 2026-10-20), so the FPU is not truncated there; `shadow_state().precision = k24` keeps the blank 24-bit images (whether Windows 98's DirectX 6 hid them is unconfirmed) |
| Shadow textures, 0x57baa0 / 0x57bb00 ([0x72f374] / [0x72f378]) | The image copied into the system surface (0x57b6c0) and Loaded into the video texture; the surfaces and texture Released | (A) Direct3D 6 | `Render3D::update_texture` (new) and `release_texture`; the RGBA as 0x57b130's (test_shadow_image.py) |
| Device caps, 0x579b70 | D3D6 triangle and address caps (blend ZERO / SRCCOLOR, CLAMP) | (A) Direct3D 6 | `zvid::Display` holds them, all set for a modern GPU |
| Direct3D draw table, zvid_ddd3d.c (0x57bf80, 0x57c700, 0x57ce20, 0x57caa0, 0x57d160, 0x57d690, flushes 0x57e110 / 0x57e4c0, frame calls 0x57a740 / 0x57a7e0 / 0x5777f0 / 0x57a850, FOGCOLOR 0x57bcf0, texture records 0x57b130) | IDirect3DDevice3 SetRenderState / SetTextureStageState / SetTexture / DrawPrimitive, BeginScene / EndScene, DirectDraw Blt fills, Flip, two surfaces and a Load per texture | (A) Direct3D 6 does not exist on macOS and is legacy on Windows | The same calls under the same caches set a `platform::DrawState` and go to `platform::Render3D` (draw, clear, present, create_texture as RGBA8 decoded from the 16-bit surface the original filled); HRESULT reports and lost-surface restores go. Emulator-verified identical as DrawState and vertex bytes (`test_drawtable.py`, drafts/drawtable.md) |
| 2D blit, 0x55b1f0 (zTwoD's every draw) | DirectDraw `Blt` (0x576f90) when the image has a surface and the target is the locked buffer, else the software blit | (A) DirectDraw | Images carry no surface: always the software blit `zrndr::blit16` (0x55b3c0) into the 16-bit buffer the platform presents (twod.md section 5) |
| 2D blit, 0x55b3c0 | The bottom clip keeps a last row equal to the height (`jbe`) | (bug) an image whose bottom lands exactly one row past the buffer writes that row beyond it | Clipped at the last row; everything else pixel-identical on 3,000 random blits (`test_twod_blit.py`) |
| 2D blit, 0x55b3c0, palettised paths | The row loops count the width up from minus itself to 0 | (bug) a zero-width source rectangle on a palettised image counts from 0 past 2^32 and faults | Nothing drawn |
| Text box, 0x5a2320 / 0x5a1e80 / 0x5a2280 | Shrinking the line count leaves the current line one past the last | (bug) the next write reads the flags of a line that is not there, and an append (0x5a22f0) writes through it | A line past the end counts as hidden (the write scrolls onto the last line); an append goes to the last line |
| Menu text, twod_wintext.cpp (0x5a00c0..0x5a0790, 0x5a0f00) | GDI: `CreateFontA`, `GetStockObject`, `CreateCompatibleDC`, `SelectObject`, `DrawTextA`, `GetTextExtentPoint32A`, `GetTextMetricsA`, colours and modes, a DC on the text image ([0x72f394]); a mutex around the render | (A) GDI is Win32 only | The same calls in the same order through `ztwod::Host` for a TrueType rasteriser over the shipped fonts; the mutex (only for the movie thread) is left out. Emulator-verified call for call (`test_twod.py`) |
| Script files, zinterp.cpp (0x595990, 0x596620, 0x596810, 0x5763b0) | `fopen(name, "r")` text mode, the archive on the `.;zbd` search path, `_stat` relative to the working directory, `_mkdir`, `DeleteFileA` | (A) Windows paths and MSVCRT text mode | Through the mounts (`zutl_path`); loose files read binary with MSVCRT's text-mode rules (CR LF to LF, Ctrl-Z ends the file) done by hand. Emulator-verified with loose files, CR, Ctrl-Z and NUL (`test_interp.py`) |
| Script lines, zinterp.cpp (0x5954a0 / 0x5955e0, 0x5956e0, 0x595850) | A 1 KB line buffer, a 64-byte macro name and a 1 KB expansion buffer, all unbounded; 16 argv slots with no bound (the 17th token overwrites the macro table pointer) | (bug) a long line, name or expansion, or too many tokens, corrupts memory | The line buffer grows, the name has no limit, the expansion stops at 4 KB, tokens past 16 are dropped; the shared expansion buffer itself is kept (its aliasing is original behaviour) |
| Archive lines, 0x5955e0 / 0x596620 | A short read leaves the line size as the FILE pointer; the size and argc are trusted; a huge script count wraps the directory's size | (trap) a damaged interp.zbd crashes or overruns | A short read or a size over 1 MB ends the script; argc is capped at 16 and tokens stop at the text's end; a count outside 0..1M rejects the archive |
| Freed line, exec_line 0x595510 / `source` 0x590e80 | After a `source` whose loose file ends in an unterminated failing line, the error hook (0x410770) reads argv[0] of that line, already freed | (bug) use after free | Lines are freed when the outermost line ends; the hook sees the text the freed block still held in the emulator |
| Script commands, 0x591300 | Crashes on: `FreeNode` of a missing node, `ModelNew` without a second word, `var NAME` without an operator, `LODSetRange` without a node or data, `Object3DRegisterTexturesToWorld` / `Object3DSetMorphVertex` on a node without a model; `LightSet{Saturated,Static,Subdivide}` log a `%s` with no argument; a polygon of more than 35 vertices or 38 UVs writes past the 0x1d8-byte build block (from 11 on they already run into the next fields, as kept) | (bug) none reached by the shipped scripts | The command does nothing (or the word counts as no match); argv[0] is passed for the `%s`; writes past the block are dropped (the model pointer at +0x1d4 is a typed field, so a UV landing there leaves it). The scroll-off null out pointer (0x596370) is passed on unchanged until node_get_model is ported |
| String variables, 0x596020 | Prints the first character where the format wants the string | (bug) none is registered | The string is printed |
| Particle fade-in, 0x5489a0 | The fade-in ramp subtracts the fade-out start (+0x3c) where the fade-in start (+0x34) belongs | (bug) a typo: inside a puffer's fade-in band the alpha is negative and the particle is not drawn, instead of fading in | The fade-in start. No shipped puffer sets a fade-in, so play is unchanged; the test runs the original with the same one-byte change (`puffers.md`) |
| Puffer event key counts, 0x5035b0 | Reads as many colour and growth keys as the event's counts say, past the six slots | (bug) a damaged anim.zbd reads the following data as keys | At most six; shipped counts are 1..5, so identical |
| Puffer sprite, 0x567720 | The texture entry's record is passed on unchecked | (A) the default texture's record is made at startup by 0x5184e0, not ported | No record, no sprite (the front end sets `zimg::default_entry()->surface`) |
| Sound, zSound (zsnd_*.cpp, sound.md) | DirectSound: DirectSoundCreate, IDirectSound, IDirectSoundBuffer (and Aureal A3D, never selected) | (A) DirectSound does not exist on macOS and is legacy on Windows | `platform::SoundOutput`, one method per DirectSound call with its HRESULTs and units; the SDL3 front end mixes in software (`sdl_sound`). A3D is not ported. Emulator-verified call for call (`test_sound.py`) |
| Sound output format, 0x5702a0 | The primary buffer set to the PLAYBACK_FORMAT, with 1 channel whatever the data says | (A) a modern mixer has no primary buffer | The request is made as the original's (and tested), but the SDL output mixes stereo, as DirectSound does since Vista (discoveries: mono on Windows 9x) |
| Sound duplicates, 0x56e2d0 | A new duplicate voice is `malloc`ed and only two fields set | (bug) its position and has-position flag are whatever the heap held, which can place a later volume change in 3D | Zeroed (the test's emulator heap hands out zeroed memory, so both sides agree) |
| Sound cue points, 0x56dfb0 | After the last cue the index steps past the array and is compared with the heap beyond it | (A) out of bounds in C++ | Read as 0, what the exe's heap most likely holds there (the next array's start): one more call with index = count, then the walk ends |
| Weapon effect blocks, 0x584bb0 | `SOUND` and `BOUNCE_SOUND` lists are copied into four slots each with no bound | (bug) a fifth sound writes over the next fields | At most four are kept (no shipped weapon has more); the test keeps its variants within four |
| Damage animations, 0x58459c | `DAMAGE_ANIM_ON_HEALTH` pairs are copied into room for four with no bound | (bug) more overwrite the destroy and kill animations | At most four are kept (no shipped weapon has any) |
| Projectile pool size, 0x58468e | A def's shots in the air are counted from a local only `RANGE` sets | (bug) a def without RANGE uses the previous def's range, the first one stack garbage | The def's own range (500 by default); every shipped weapon has RANGE, so the pool is the same 164. The test patches the original the same way |
| LOCK_ON_LEAD, 0x583fb9 | The second value is read when the first value's bits exceed 3, not when the list has two values | (bug) a typo: `[eax+0xc]` for `[eax+4]` | The count decides; no shipped weapon has LOCK_ON_LEAD. The test patches the original the same way |
| Weapon manager with no group, 0x4b0c30 / 0x4b0790 / 0x4b07e0 / 0x4b08c0 | Trigger, next and previous weapon and the network release call through the selected group without checking it | (bug) group -1 is a null call (crash); the manager starts at group 0, so only a weapon moved to group -1 gets there | Nothing happens without a group. The test never reaches it on either side |
| Gravity shots that accelerate, 0x585d61 | The velocity is rebuilt with ECX = 1 as the base vector | (bug) a crash; no shipped weapon has gravity | The shot's base velocity, as in the branch without gravity |
| Air burst record, 0x582320 | The record built for a burst in the air leaves the surface normal unset (stack bytes) | (bug) the surface animation's orientation is whatever the stack held | A zero normal. Only LANDMINE has a surface animation in its default impact; the test leaves it out of the air bursts |
| Animation phase cache, 0x5abae0 / 0x432a80 (and every sampler inlining it) | The phase is cached by time alone; SetLoopTime 0x5aa3b0 resets the cached time to 0 but not the keys, so after the 60-frame resample (0x5aa360) a first sample at t = 0 reuses the last resample's keys | (bug) for motions longer than 60 frames those keys are past the new arrays: the exe reads the heap beyond them for that sample | Keys past the last are recomputed; in-range stale keys are kept as the exe has them. `test_mechanim.py` samples elsewhere first where the original would read out of bounds (motion.md, Playback) |
| Alan's voice, aiv.zrd 0x4df730 | A record without a `sound_tag` gets Alan's radio voice only when named exactly `Alan_Mattila`; the c1 Instant Action map 1 and c3 mission 6 spell him `Alan_Mattilla` and `Alan_Matilla` | (bug) misplaced data: his voice lines likely do not play there | Both misspellings also give `alan`; the record names are kept, since scripts may look him up by them (Owen 2026-10-20) |
| Loose orders, aiv.zrd 0x4df730 | `Enemy_12` (c4 readeria1) and `Enemy_06` (c4 readeria2) list their orders loose in the record, without `"vlist" ( ... )` | (bug) misplaced data: the reader skips them, so those enemies follow no scripted orders | A record with no `vlist` takes order-shaped lists (17 items or more, not after a key) as its orders (Owen 2026-10-20) |
| Foot ground probes, mech skis +0x2f4 (0x475de0, 0x475ea0) | The width that picks a foot's middle probe for its ground contact and its water is never written: whatever the heap held | (bug) a foot's ground and water come from the front or back probe, or from any probe by chance | The ski's half length, so the nearest probe wins (Owen 2026-10-20); `zfix bugs`, off in the probes (the test pins the original's to 0) |
| Gait blend-in, 0x417a80 (0x417d85) | Over the first 20 frames of walking the left foot's rotation is blended twice, the second time from the right foot's last rotation, and the right foot's never | (bug) the right foot snaps to the walk's angle and the left lurches toward the right's old one for the first 2/3 s of every walk | Each foot blends from its own last rotation (Owen 2026-10-20); `zfix bugs`, off in the probes (`movement-pose.md`) |
| Toe poses, 0x417fde.. / 0x4178a8.. / 0x417177.. | Both feet loop over the left foot's toe count; the right foot's index is checked with `ja`, so it reaches its own count | (bug) a right foot with fewer toes than the left reads and writes one quaternion and one toe node past its arrays | Stopped at the count. Shipped mechs likely have equal counts (not checked); `zfix bugs` off in the probes keeps the exe's check (`movement-pose.md`) |
| Missing stand or step motion, 0x431350 | Frees the hip velocity keys of the loaded animation without checking it loaded | (bug) a crash; never with the shipped motions | Checked |
| Lava heat, 0x4192f0 | Adds 0.05 heat each frame either foot is on lava | (B) frame-rate dependent: about 1.5/s at 30 fps, 7/s at 144 fps | 0.05 per 1/30 s (1.5 heat a second at any frame rate), the 30 fps reference chosen by Owen 2026-10-20; `zfix::options().per_second`, off in the probes |
| Movement steps per frame: a fall's carried momentum (x0.6) 0x416860, the ground probes' step-kick, slope-clamp and ledge pushes and the far mechs' push x3 0x4765b0, the jump jets' climb cap (6) 0x4173b0, the gait's 20-frame blend-in 0x417a80, the 20 damage to touched buildings 0x423c90 | Each applied once a frame | (B) at 144 fps a fall's momentum dies in a fifth of the time, slopes and ledges shove nearly five times as hard, the walk blends in in 0.14 s, a building takes 2,880 a second instead of 600 | Each per 1/30 s (Owen 2026-10-20): 0.6^(30 dt), x3^(30 dt), pushes and the cap times 30 dt, the blend counted in 1/30 s steps, 600 dt damage; equal to the exe's step at dt = 1/30. `zfix::frame_steps`, off in the probes; `tests/test_fixes.cpp` |
| Deep water, gait 0x4181c0 | Past a tenth of the mech's height the speed factor is 0.7 (H - d) / H, and 0.7 once deeper than H | (bug, game-breaking per Owen) the factor falls from 0.63 to a standstill as the water nears the mech's height, then jumps back to 0.7 | A straight line from 1 at 0.1 H to 0.7 at H, then 0.7 (movement.md, Speed and gait); `zfix::options().deep_water`, off in the probes, so `test_gait.py` checks the exe's curve |
| Throttle keys, 0x427d60 | CMD_THROTTLE_UP / DOWN move the throttle target 0.025 each frame they are held | (B) at 144 fps the throttle opens almost five times as fast (full in 0.28 s instead of 1.3 s) | 0.025 per 1/30 s (`zfix::frame_steps`), equal to the exe's at 30 fps; off in the probes; `tests/test_fixes.cpp` |
| Smoke drag and wind, 0x5490e0 / 0x523f70 | Drag from a table of e^-(i/51) with the step truncated; the wind's speed steps by up to `random_accel` a frame | (B) at 60 fps and above muzzle smoke and trails get no drag at all, and gusts change faster the faster the game runs | e^-(dt * friction) computed, and the speed step scaled as per 1/30 s (Owen 2026-10-20); `zfix` switch as above. At 30 fps the drag is now a little stronger than the table's (it rounded 0.033 down to 0.0196) |
| Sound fades, 0x571940 | A fade ends only when a 25 dB/s step lands exactly on the target, which happens only at full volume or silence | (bug) fades to a partial volume never end: the voice wobbles a step around the target and the fade stays in the list for good | A step that reaches or passes the target ends the fade on it (Owen 2026-10-20); `zfix::options().bugs` |
| Unpause, 0x56f130 | Every voice of each sound kept at pause plays again, finished ones from the start, once per record (a sound with two copies playing replays both twice) | (bug) sounds that had ended start over and doubles pile up after a pause | Only the voices that were playing resume, each once, from where they stopped (Owen 2026-10-20) |
| Wingmen's ammo calls, group reader 0x573560 | `dom/alan/epona_low/out_of_ammo` list `"PLAY_COUNT" 2` at group level; the reader takes any unknown word as the group's name, so all six are named "PLAY_COUNT" | (bug) the low-ammo and out-of-ammo lines never play | PLAY_COUNT at group level applies to each line of the group (each plays at most twice); the groups keep their names (Owen 2026-10-20: a cue to repair at the MFB) |
| Stereo, the PLAYBACK_FORMAT reader (zsnd_init `read_playback`) | The channel count in `sounds.zrd` (HIGH: 2) is not read; 1 is stored | (bug) on Windows 9x the mix was mono, so the 3D sounds' left/right pan was lost | The data's channel count is read (Owen 2026-10-20). The SDL output mixes stereo either way |
| Medium sound format, 0x570ccf and the per-sound formats | Looks up `"MED"` where `sounds.zrd` says `MEDIUM` | (bug) a typo: the medium playback format is never read | `MEDIUM` |
| Mad Cat and Supernova damage textures, skin reader (`mech3::read_skins`) | `damage` lists two (Mad Cat) and one (Supernova) slots longer than the primary skin, so every damage mask lands on the wrong texture; the size check then drops some | (bug) misplaced data: those mechs show the wrong or no battle damage | Leading empty slots are dropped until the damage list is as long as the primary (Owen 2026-10-20); `tests/test_fixes.cpp` checks both on the real files |
| Plain PPC, weapon table 0x5e64f0 | 37 weapons; `weapons.zrd` defines a `PPC` and Mech3Msg.dll names it (0x2fd, after the last weapon's 0x2fc; the old slot 0x2ee between NARC and the pulse lasers is empty) but the table has no row | (bug) the mech lab cannot offer a weapon the data defines | A 38th row, `PPC` (message 0x2fd, family 3 as ERPPC, Inner Sphere tech 2 as the plain lasers, no ammo); `mech3::weapon_count()` (Owen 2026-10-20) |
| The program, `mw3` (program.md) | The exe on Win32: `WinMain`, MFC's `CWinApp`, the `Mech3Class` frame | (A) Win32 / MFC | The app constructor, InitInstance, the engine start, `zGameApp::Run` and the states run as ported, from `mw3_main.cpp` on SDL3; `mech3::GameEngine` wires their hooks to the ports and to `src/frontend/sdl` |
| Main window, 0x482d10 (`CWnd::CreateEx`, `SetWindowTextA`, `UpdateWindow`, `GetFocus`) | A 640x480 frame clamped to the work area, the debug menu (MENU 0x93) | (A) Win32 | An SDL window (1280x960 by default, the game's 640x480 scaled over it), titled from message 7; no debug menu |
| Message queue, 0x58fd70 / OnActivate 0x590c00 / MM_MCINOTIFY 0x590740 | `PeekMessageA`, `PumpMessage`, `MsgWaitForMultipleObjects`, `WM_ACTIVATE`, the MCI notify message | (A) Win32 | `SdlPump`: SDL's event queue; focus gained / lost is the frame's OnActivate; a music file's end is posted as the notify; the window's close posts the quit |
| Mech3Msg.dll, 0x575f30 / ZLocGetID / 0x576040 / 0x575fd0 | `LoadLibraryA`, the DLL's sorted `{name, id}` table, `FormatMessageA` on its RT_MESSAGETABLE | (A) a Windows DLL cannot be loaded on macOS | `zutl_msgdll` reads the table and the message resource from the file (found through the mounts) and answers the same lookups; `%1` inserts done as FormatMessage does |
| Message boxes, `MessageBoxA` and the game's box 0x4ac150 | Win32 boxes; the game's own dlgMessage.zrd box | (A) Win32; the game's box is the 2D front end's, not ported yet | SDL's message box with the same texts and buttons (OK, OK/Cancel) |
| Clock, `GetTickCount` / `Sleep`, and the clock start 0x5759a0 | Win32 | (A) | `SdlClock` (milliseconds from the start, `SDL_Delay`); 0x5759a0 ported as `ztimer::init`, emulator-checked (`test_ztimer.py`) |
| Input, DirectInput (0x51b0a0: `DirectInputCreateA`, the keyboard buffered by `GetDeviceData`, the mouse and joystick by `GetDeviceState`) | DIK scan codes, DIMOUSESTATE, DIJOYSTATE2 | (A) DirectInput does not exist on macOS | `platform::Input` with DirectInput's structures and HRESULTs; `SdlInput` maps SDL scancodes to the same DIK codes key for key, sums mouse motion into the relative counts and reads the first joystick in the 0..65535 range. zInput itself (0x51a170.., what it does with them) is not ported |
| Video, 0x579c60 / 0x576de0 / 0x576ea0 / [0x72f38c] | DirectDraw's mode, locked back buffer and flip; the 3D card chosen from the registry's InstallOptions and HWCardDev; a 4 MB check | (A) DirectDraw | The GPU is the 3D card (InstallOptions answers 0x101, HWCardDev picks device 0, its memory reported as 256 MB); the menus' 16-bit buffer is presented through `SdlScreen2D` (a texture drawn by Render3D) |
| Settings store, 0x5977d0 / 0x597a60 / 0x597fc0 (settings.md) | RegOpenKeyExA / RegQueryValueExA (read), RegCreateKeyExA / RegSetValueExA (write) on `HKCU` and `HKLM\SOFTWARE\MicroProse\MechWarrior 3\1.0`; both keys must open for anything to be read | (A) the registry is Windows' | `platform::Settings` call for call; `platform::RegFile` keeps both keys in the write dir's `settings.reg` in regedit's text (a Windows export reads as is; unknown values kept). The file stands for both keys, so no installer's HKLM key is needed |
| Machine probe, 0x597fc0 (CPUID, a timing loop for the MHz, GlobalMemoryStatus, DirectDraw's caps) | What detail.zrd's conditions compare | (A) x86 and DirectDraw only | `GameEngine::settings_init` describes the machine: class 6, 2000 MHz, MMX, a 3D card, the real memory (capped at 4 GB as GlobalMemoryStatus caps it), 256 MB of video memory; InstallOptions 0x101 |
| ActiveCtlCfg, the loader 0x40c420 to 0x4120a0 | The saved 0x104-byte name is passed as a C string | (bug) a saved value without its 0 runs past the buffer | The name stops at the buffer's end (`test_settings.py` truncates the original's read the same way) |
| Mission load thread, 0x490880 / 0x490900 (mission-load.md) | `CreateThread` at idle priority runs the load beside the briefing; `CreateEvent`/`SetEvent`, `WaitForSingleObject` polled by 0x490930 | (A) the world's textures go to the GPU, which wants the main thread | The load runs inside 0x490880; the briefing finds it done |
| Menu text, GDI (twod_wintext.cpp, through `ztwod::Host`) | `CreateFontA` on the installed Arial / Impact / Lucida Console, `DrawTextA` into a 16-bit DIB | (A) GDI | `SdlTwodHost` on SDL3_ttf with the game's own arial.ttf, impact.ttf, lucon.ttf, unantialiased so the key-colour blit keeps clean edges; GDI's height rules (negative: character height, positive: cell height), line breaks, `DT_WORDBREAK`, `&` prefixes, opaque and transparent modes; fonts.zrd's WINDOWS_FONTS read by name (the zImage font table 0x517280.. is not ported) |
| AI reader files, zrdraiv.cpp / zrdrainet.cpp (0x4decf0, 0x4ded20, 0x4da0d0, 0x4db850, 0x4db1b0, 0x4df730, 0x4e08c0) | Every value is read as the shape the code expects: a list's items, a string pointer, `key + 5` for node_XX | (bug) a malformed file (a number where a list or a name belongs, a short key) crashes the game or reads past a string; emulator: a non-list file root crashes 0x4da0d0 | The value is passed over; the shipped files never take these paths, so all 773 still match exactly (`ai.md` section 5) |
| aiv.zrd old tags, 0x4df730 | The scratch the p3/p4/p5/p7 tags share is uninitialised stack when the tag that sets it is missing, and p4's order is built on uninitialised stack | (bug) the result depends on what was on the stack | Zeros and the order defaults; no shipped file has these tags (the test pins the exe's stack the same way) |
| AI values of the wrong type | A string or list where a number belongs is copied as its pointer bits | (A) a 32-bit heap address | 0 |
| Input, zInput (0x51a170..0x51ddc0, input.md) | DirectInput 5 (DirectInputCreateA, IDirectInputDevice2: keyboard buffered, mouse and joystick states, cooperative levels, axis ranges and dead zones) and Win32 (ClientToScreen, ScreenToClient, SetCursorPos, GetClientRect, MapVirtualKeyA, GetKeyboardState, ToAscii) | (A) DirectInput does not exist on macOS | `platform::Input` with DirectInput's structures and results, `zin::Host` for the Win32 calls; on SDL3 the exclusive mouse is relative mouse mode while acquired, motion scaled to the 640-pixel frame, the joystick's ranges and dead zones applied to SDL's axes, characters from SDL's keymap |
| Keyboard read, 0x51a5d0 / 0x51aa00 / 0x51a300 | A key code (DIK code with Alt 0x100, Ctrl 0x200, Shift 0x400) indexes the 0x7de key states unchecked | (bug) a DIK code from 0xde up (Power, Sleep, Wake, the web and media keys) with all three modifiers held writes past the table into the text callback's pointer (a crash at the next key) and the joystick's globals | Codes past the table are skipped |
| Mouse start, 0x51b5f0 | CreateDevice's failure ignored; QueryInterface called through the pointer it did not get | (bug) a crash at start on a machine without a mouse | The mouse counts as present and reads nothing |
| Key names, 0x51c480 | A code's low byte indexes the 0xde key names unchecked | (bug) 0xde..0xff read the joystick's names, then numbers, as names | No name |
| zReader text files, 0x556240 / 0x5563a0 (zreader.md, Text files) | `_access` on the path, then the search path; `CreateFileA`, `ReadFile` a byte at a time, `SetFilePointer` | (A) Win32 | zutl::open (the write dir, then the mounts) and stdio; emulator-verified identical trees (`test_zreader_text.py`) |
| Text tokens, 0x556510 | An unquoted token of 256 characters writes its terminator over the low byte of the return address | (bug) the reader returns into the middle of an instruction; its "too long" message (after 257) is never reached cleanly | The token is dropped and the list ends, as the message intends (`mw3tests`) |
| Text strings and directives, 0x556510 / 0x557550 | A quoted part is read with no length check and on past the file's end; an `#ifdef` or `#else` without its `#endif` scans for `#` past the end | (bug) a stack overrun, or a hang at load | Both stop at the end of the file; a quoted part keeps 255 characters (`mw3tests`) |
| Controls file axes, 0x4120a0 (controls.md, Setup) | An AXES entry named other than Throttle, Twist, Pitch, LR is stored as a fifth table entry | (bug) it overwrites 0x604110..0x60411f: the table's flags and the groups' vector, so the game crashes at exit or in the controls screen | Skipped (`test_ctlcfg.py` puts the original's bytes back after its read and checks the rest) |
| Controls file entries, 0x4120a0 | Every KEYS / AXES item is read as a list whose first item is a string (and KEYS, AXES as lists) | (bug) a hand-edited file with a number or an empty list there crashes the game | Such items are skipped (`mw3tests`) |
| Controls name map, 0x410fe0 | `std::map<const char*, int>` keeps the pointer it is given; a name from a controls file is added and the file freed | (bug) a dangling key, read by later lookups and the writer | The map keeps its own copy of the name |
| Controls file location, 0x42d0b0 / 0x411e60 / 0x4120a0 | `keys\<name>.zrd` relative to the game folder, `_mkdir("keys")` | (A) the install is read-only data in the rebuild | Written to and read from the write dir through zutl::open, which makes the folder; text mode (LF on macOS), which the reader takes either way |

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
  FIX (bug): round properly. Settled (Owen, 2026-10-20: mechs cast shadows in play on Windows 11, so
  the FPU is not at 24 bits there): `zfix::options().fpu53`, on by default,
  gives the 53-bit result (x rounded to the nearest integer); off, the
  24-bit one, which the probes keep. Checked against the original run at
  64 bits: within one colour step (the original's arithmetic is wider).
- Projected-light triangles, 0x57ea20 / 0x57fe20: the same rounding, so the
  ground the weapon lights subdivide is drawn black at 24-bit (emulator,
  render-shadows.md). Same check in play and the same likely FIX (bug); the
  port keeps the 24-bit result for now, as for the point-lit vertices. Settled (Owen, 2026-10-20: mechs cast shadows in play on Windows 11, so
  the FPU is not at 24 bits there): `zfix::options().fpu53`, on by default,
  gives the 53-bit result (x rounded to the nearest integer); off, the
  24-bit one, which the probes keep. Checked against the original run at
  64 bits: within one colour step (the original's arithmetic is wider).
- Mech shadow images, 0x55d010 (from 0x542e70): every edge goes to 16.16
  fixed point with the same trick, so at 24-bit nothing is filled and the
  shadow texture is plain white: shadows invisible on hardware (emulator,
  render-shadows.md). In play on Windows 11 + DDrawCompat the shadows are
  visible (Owen, 2026-10-20): the reference runtime leaves the FPU at 53
  bits at least there (whether 1999 DirectX 6 on Windows 98 hid them stays
  unconfirmed). Ported with a switch, `zmod::shadow_state().precision`:
  `k53`, the default, runs the filler at double precision as seen in play
  (the original's own images, bit for bit: test_shadow_image.py); `k24`
  keeps the blank images of a 24-bit FPU. Not a FIX: the original's
  behaviour on the reference runtime.
- Translucent untextured polygons on lit or fogged models, 0x57c700: coloured,
  then neither queued nor drawn, so fake shadows vanish there (emulator; check
  in play).

- Cloud shading, 0x543ac0: the table index uses the same 1.5 x 2^52 rounding,
  so at 24-bit it is always 0 and the moving cloud pattern on the sun is a
  constant (c1: a flat 70%) on Direct3D (emulator, render-lighting.md). If
  confirmed in play, the hardware renderer never showed moving cloud
  shadows; restoring them would be a FIX (bug). Settled (Owen, 2026-10-20: mechs cast shadows in play on Windows 11, so
  the FPU is not at 24 bits there): `zfix::options().fpu53`, on by default,
  gives the 53-bit result (x rounded to the nearest integer); off, the
  24-bit one, which the probes keep. Checked against the original run at 64
  bits: identical.
- Point lights just inside their near range, 0x545720: below near squared the
  squared distance is compared as a distance, so a vertex can go dark
  (near 32 / far 64: 0.875 at 6, 0 from 10 to 31, 1 at 32). How often shipped
  lights show it is not checked.

## Decided to keep (Owen, 2026-10-20)

- Wading: the depth ratio is clamped with max where min was likely meant, so
  any depth gives the full x8 cooling. Players plan around it; kept.
- Water cooling checks only the left foot; kept.
- Elemental armor: 1 point in every location (11), where the 1 ton the file
  gives is worth 16. The only effect of the mismatch is a log line at load,
  and one point a location is evidently the design (an Elemental dies to a
  single hit); kept.
- Missiles that miss vanish at RANGE without exploding; kept.
- Radio chatter (QUEUE) plays newest first; kept as design (chatter stays
  current).
- Every mech's maximum engine forced to 400 and only a weapon's first group
  counting look deliberate; kept.

## Candidates for Owen

These look like bugs, but fixing them is a guess at intent that changes balance:

- Resolution and aspect (Owen, 2026-10-20: wanted, as a setting): any
  window size up to 4K, 1920x1080 and 16:9 included, defaulting to the
  original 640x480 4:3. The game's projection and clipping work from the
  video mode's width and height; the mode list (640x480, 800x600,
  1024x768, plus 1152x864, 1280x1024 and 1600x1200 reachable only from the
  registry) and its checks would accept any size; 16:9 widens the field of
  view (more to the sides) rather than stretching; the 640x480 menus and
  HUD art are scaled, the menus pillarboxed. Players' crashes at high
  resolutions (Escape, the pause menu's Options above ~1000 lines;
  `community-issues.md`) are to be found in the 2D code then. A FIX (B),
  after the settings loader and the menus.

- ZAPS_HUD, 0x4af71b: the second value reads the first value's slot again, so
  the ERPPC's `(0.5 2.0)` HUD static is (0.5, 0.5). Reading the second value
  is the evident intent, but what the second number does on the HUD
  (0x43be70) is not traced; the port keeps the original.
- Pock marks: `weapons.zrd` gives the autocannons, Gauss, missiles and MGUN a
  `SURFACE_ANIMATION` on `dirt`, `mud` and `snow`, kinds the impact table
  (0x5edbc0: default, water, seafloor, quicksand, lava, fire) does not have,
  so they never show. Mapping those names to the soils that exist would
  restore them; which soil each should be is a guess.

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
| World materials, 0x5363d0 / 0x536ad0 / 0x537830 (`gmod_matl`) | Blocks of 1000 0x2c-byte slots in a `realloc`ed pointer array, grown when mechlib's and each mech's materials are added | (A) 32-bit pointers in the slots; the polygons hold the materials' addresses | Owned blocks: a material keeps its address as the table grows; the slots, free and used lists, growth and compare are emulator-verified identical (`test_tree_copy.py`) |
| mechlib materials, 0x4fc520 (`cls_mechlib`) | The raw 0x28 file bytes get the texture's pointer at +0x10, then the object's +0 becomes the world material | (A) 32-bit pointer fields in a file layout | The texture and the world material are fields of their own; the raw bytes stay as read |
| Mech data part effects, 0x4320b0 (`Pilot`, mech.h) | +0x1a4 + 0xc x part id; ids 0 and 1 (never used) overlap the canned effects' last slot | (A) one C++ array per kind | `part_effects[6][3]` and `canned[5]` apart; the test lays both over the original's block |
| Mech data refresh, 0x4319f0 | A type whose definition failed to load stays cached, and the next mission's load reads its reader through null (0x431a20) | (bug) a crash at the second mission's load after a broken mech file | No reader: no canned effects; the sounds and soils load as usual. No shipped mech fails |
| Shadow image size, 0x542d30 | Resizing an image clears the caster's image pointer, then frees through it (reads address 0x10) | (bug) a crash; unreached in 1.2 (only a new caster gets a size) | The old image is freed (render-shadows.md) |
| Polygon filler, 0x55d010 | A level edge just at or above a row centre is listed without x or slope (stack garbage, used for that row); with no vertices the second edge walk never ends | (bug) uninitialised memory; a hang | The edge's end x and no slope (the test makes the original do the same); a polygon without vertices is not filled. Its 16.16 rounding runs at 53 bits by default: in play on Windows 11 + DDrawCompat the shadows are visible (Owen, 2026-10-20), so the FPU is not truncated there; `shadow_state().precision = k24` keeps the blank 24-bit images (whether Windows 98's DirectX 6 hid them is unconfirmed) |
| The fade's stills, 0x579500 / 0x55b1f0 / 0x517cc0 (the fade state 0x48f0b0) | A DirectDraw surface copied to an image; the image blitted back; the image freed | (A) DirectDraw | `GameEngine` (app_engine.cpp): a copy of the 16-bit buffer, copied back whole (game-loop.md, The fade state); the colour is the ported tint (render-frame.md) |
| The HUD's timer, 0x43c390 | Seconds are the time less the minutes alone (`floor(t - 60 * minutes)`), the hours never taken off | (bug) past an hour the seconds run into the thousands: 3700 s shows 01:01:3640 | The hours taken off too, behind `zfix` bugs (hud.md section 14) |
| The game options' invert box, ACCEPT 0x00438100 | MouseAccelY set to -1.3 checked, 1.3 not | (bug) the player's mouse sensitivity (the registry's MouseAccelY, any size) is lost each time the options are accepted | The size kept, only the sign set (a zero one takes 1.3); `mouse_y_accel` in options_menu, behind `zfix` bugs; `test_menus.py` checks the exe's, `test_fixes.cpp` the kept size (Owen 2026-10-20, Decided) |
| TARGET's distance past 1 km, 0x00441850 | `"%.2fkm"` appended with no space or bracket ("Owens: Standard1.23km"), where under 1 km it reads " (500m)" | (bug) a cosmetic text slip | " (1.23km)", behind `zfix` bugs (Owen's cosmetic-slips rule, decided through the lead 2026-10-20); `test_hud.py` checks the exe's |
| The coolant gauge's easing, 0x004456f0 | A tenth of the way to the value each call, that is each frame | (B) it settles faster the higher the frame rate | A tenth per 1/30 s (0.9 to the power of the frame's 30 fps steps), behind `zfix` per_second (Owen's frame-rate rule); `test_hud.py` checks the exe's |
| The message panel's slide, 0x0040f8d0 | Moves ftol(60 x the frame's seconds) pixels a frame | (B) above 60 fps that is 0: the panel never comes up, the messages are never seen (1 from 30 to 60 fps: 30 to 60 pixels a second) | The fraction carried to the next frame, so 60 pixels a second (its speed field) at any rate, behind `zfix` per_second; `test_hud.py` checks the exe's (hud.md section 18) |
| The briefing's loading bar, 0x00406440 | Creeps 0.005 a frame towards the load's fraction | (B) it fills faster the higher the frame rate (a per-frame step, Owen's rule; the lead: no candidate) | 0.15 a second (0.005 per 1/30 s), behind `zfix` per_second; `test_briefing.py` checks the exe's, `test_mainmenu.cpp` both |
| The credits' "Other Voices" line, read by 0x00429200 | `"offset" (20)`, one number, so the line keeps the name above's (30 0) | (bug) a typo in dlgCredits.zrd: the heading sits indented like a name with no gap | The one-number offset gets the headings' (20 20) (x 20 is the headings'; all 46 are (20 20)); `credit_line_offset`, behind `zfix` bugs; `test_menus.py` checks the exe's, `test_fixes.cpp` the fixed one |
| The lead point, 0x41d940 (TARGET's lead circle) | Reads the selected group's current weapon (+0x10) through the group pointer, which is null with no group selected (+0x28 = -1) | (bug) a crash: a mech with a targeting computer whose groups are all gone, with a target on screen | No group, no lead point (hud.md section 14) |
| A world point on the screen, 0x53cf10 (TARGET's tracking) | Behind the near plane the outcode is 8 ORed with the low bits of the input point's address (with the identity on the stack, an unset local) | (bug) the caller (TARGET's tracking 0x440b00) tests those bits, so which way the arrow goes for a target behind the zoom camera depends on where a buffer sits | 8 alone (hud.md section 14) |
| The back buffer's lock in Direct3D, 0x577590 / 0x577600 under 0x576d00 / 0x576d60 | DirectDraw Lock of the back buffer: the 2D code (HUD, the fade's still) draws into the 3D picture | (A) DirectDraw; a GPU frame cannot be locked cheaply | The lock hands out the 16-bit layer filled with the key 0xf81f; the unlock lays the changed pixels over the frame (`Screen2D::overlay_16`); Render3D keeps one back-buffer texture a frame. 2D code that reads the screen sees the key (nothing ported does). Alpha blends (the HUD's images: compass, reticle) record each pixel's coverage and colour while the layer is locked (`zrndr::LayerCover`), and the overlay draws them with that alpha, instead of mixing them with the key (magenta fringes) (render-frame.md) |
| The screen grab in Direct3D, 0x579500 (the fade's still) | Locks the back buffer and copies the 3D picture into an image | (A) the GPU's frame is not in memory: the grab copied the empty 16-bit layer, and the mission-start fade came in from black (Owen) | `Render3D::read_back` downloads the back-buffer texture (SDL GPU), scaled to the game's 16-bit size; the menus' software grabs keep the layer. `tools/analysis/fadecheck.py` shoots frames in the fade (render-frame.md) |
| Cockpit camera update, 0x421e40 | The offset is left unset when the distance is not 0 (stack garbage) | (bug) uninitialised memory; unreached: every shipped `attach` block has `dist (0.0)` | The offset is 0 there (view.md) |
| Software tint rows 0x5582c0 / 0x558320 | One pixel pair more than count / 2 | (bug) on the buffer's last row the extra pair is written past its end | Only the pixels inside the buffer are read and written; identical elsewhere (test_d3d_frame.py) |
| Shadow textures, 0x57baa0 / 0x57bb00 ([0x72f374] / [0x72f378]) | The image copied into the system surface (0x57b6c0) and Loaded into the video texture; the surfaces and texture Released | (A) Direct3D 6 | `Render3D::update_texture` (new) and `release_texture`; the RGBA as 0x57b130's (test_shadow_image.py) |
| Device caps, 0x579b70 | D3D6 triangle and address caps (blend ZERO / SRCCOLOR, CLAMP) | (A) Direct3D 6 | `zvid::Display` holds them, all set for a modern GPU |
| Direct3D draw table, zvid_ddd3d.c (0x57bf80, 0x57c700, 0x57ce20, 0x57caa0, 0x57d160, 0x57d690, flushes 0x57e110 / 0x57e4c0, frame calls 0x57a740 / 0x57a7e0 / 0x5777f0 / 0x57a850, FOGCOLOR 0x57bcf0, texture records 0x57b130) | IDirect3DDevice3 SetRenderState / SetTextureStageState / SetTexture / DrawPrimitive, BeginScene / EndScene, DirectDraw Blt fills, Flip, two surfaces and a Load per texture | (A) Direct3D 6 does not exist on macOS and is legacy on Windows | The same calls under the same caches set a `platform::DrawState` and go to `platform::Render3D` (draw, clear, present, create_texture as RGBA8 decoded from the 16-bit surface the original filled); HRESULT reports and lost-surface restores go. Emulator-verified identical as DrawState and vertex bytes (`test_drawtable.py`, drafts/drawtable.md) |
| 2D blit, 0x55b1f0 (zTwoD's every draw) | DirectDraw `Blt` (0x576f90) when the image has a surface and the target is the locked buffer, else the software blit | (A) DirectDraw | Images carry no surface: always the software blit `zrndr::blit16` (0x55b3c0) into the 16-bit buffer the platform presents (twod.md section 5) |
| 2D blit, 0x55b3c0 | The bottom clip keeps a last row equal to the height (`jbe`) | (bug) an image whose bottom lands exactly one row past the buffer writes that row beyond it | Clipped at the last row; everything else pixel-identical on 3,000 random blits (`test_twod_blit.py`) |
| 2D blit, 0x55b3c0, palettised paths | The row loops count the width up from minus itself to 0 | (bug) a zero-width source rectangle on a palettised image counts from 0 past 2^32 and faults | Nothing drawn |
| 2D blit, 0x55b3c0, any source rectangle | The source rectangle is taken as given; only the screen clips | (B) the menu screens draw their still background (a mission's Options, FastMenu) with the screen's size as the rectangle of the 1280x960 image (menus.md, The background at other sizes): on a screen taller than 960 lines or wider than 1280 it reads past the image, the likely cause of the pause screen's Options crash above ~1000 lines (community-issues.md); the port, which has no 640x480 mode switch yet and will run up to 4K, would read past it at any larger window | The rectangle is cut at the image's edges (a negative left or top moves the draw instead); identical for every rectangle inside the image (`test_twod_blit.py`); `tests/test_menus.cpp` checks the cut and the menus' still background on a tall screen |
| Credits background, 0x004289a0 | Reads the main menu's background through [0x00609854] unchecked | (bug) a null read without a main menu (unreachable: the credits open only from it) | No image, still (`MenuScreen::borrow_background`) |
| Credits text, 0x00429200 | Each line's messages are `strcat`ed into a 256-byte stack buffer | (bug) a longer line overruns the stack (no shipped line comes near) | Cut at 255 characters |
| The briefing map's circles, 0x00407ea0 | Shrink max(20 x dt, 1) pixels a frame | (B) a pixel a frame above 20 fps: faster the higher the frame rate | 30 pixels a second (the 30 fps frame's pixel), the fraction carried, behind `zfix` per_second; `test_briefing.py` checks the exe's |
| The briefing's SOUND, inline in 0x00405230 | MARKERS read unchecked | (bug) a crash: a briefing's SOUND without MARKERS (none ships) | No map steps: the voice and its end only |
| The briefing's debug lines, 0x00405120 | A load line passed to the text box as its printf format | (bug) a `%` in a line reads arguments that are not there | The line printed as text (`"%s"`) |
| Video device names, 0x004394d0 | `sprintf(buf, name)`: the card's name is the format, into 64 bytes | (bug) a `%` in a driver's name reads the stack; a long one overruns | The name copied as text, cut at 63 |
| Cycle buttons, 0x0048b550 / 0x0048b6b0 | The mouse is read through the dialog's cursor unchecked | (bug) a null read in a dialog without a CURSOR (every shipped one has one) | (0, 0) |
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
| Beam recharge, 0x4b4230 | Adds ftol(rate x dt) (dt times the heat factor 0.5..1) to a beam's charge each frame it is not firing | (B) the fraction is lost every frame, so the faster the game runs the slower beams recharge: TAG (166.7 a second) refills in 67 s at 30 fps, 83 s at 60, 69 s at 144, and never above 166 fps, or at 144 fps once heat passes 0.58 (emulator, `test_tag.py`; weapons.md, "TAG, end to end"); plain lasers stop recharging above 420 fps | The 30 fps frame's whole units, ftol(rate x / (30 dt)), spread over the frame with the fraction carried (`mech3::beam_recharge`): the exe's amount at 30 fps, the same per second at any rate; `zfix::options().per_second`, off in the probes; `tests/test_fixes.cpp` |
| Smoke drag and wind, 0x5490e0 / 0x523f70 | Drag from a table of e^-(i/51) with the step truncated; the wind's speed steps by up to `random_accel` a frame | (B) at 60 fps and above muzzle smoke and trails get no drag at all, and gusts change faster the faster the game runs | e^-(dt * friction) computed, and the speed step scaled as per 1/30 s (Owen 2026-10-20); `zfix` switch as above. At 30 fps the drag is now a little stronger than the table's (it rounded 0.033 down to 0.0196) |
| Menu motion: the background's scroll 0x40a210, MECH3TEXT's slide 0x42fb30, the buttons' ripple fade 0x485650 | 1 pixel (right and down), 2 pixels and 0.3 of the ripple's 7 each frame | (B) the main menu crawled and the ripple died faster the faster the machine drew (at 144 fps a ripple lasts 0.13 s instead of 0.6 s) | Each per 1/30 s, whole pixels carried over (`zfix::frame_steps`, `mech3/menu_widgets`); off in the probes. The menus ran at the monitor's refresh in 1999, so 60 may suit them better than 30: a candidate for Owen |
| Sound fades, 0x571940 | A fade ends only when a 25 dB/s step lands exactly on the target, which happens only at full volume or silence | (bug) fades to a partial volume never end: the voice wobbles a step around the target and the fade stays in the list for good | A step that reaches or passes the target ends the fade on it (Owen 2026-10-20); `zfix::options().bugs` |
| Unpause, 0x56f130 | Every voice of each sound kept at pause plays again, finished ones from the start, once per record (a sound with two copies playing replays both twice) | (bug) sounds that had ended start over and doubles pile up after a pause | Only the voices that were playing resume, each once, from where they stopped (Owen 2026-10-20) |
| Wingmen's ammo calls, group reader 0x573560 | `dom/alan/epona_low/out_of_ammo` list `"PLAY_COUNT" 2` at group level; the reader takes any unknown word as the group's name, so all six are named "PLAY_COUNT" | (bug) the low-ammo and out-of-ammo lines never play | PLAY_COUNT at group level applies to each line of the group (each plays at most twice); the groups keep their names (Owen 2026-10-20: a cue to repair at the MFB) |
| Stereo, the PLAYBACK_FORMAT reader (zsnd_init `read_playback`) | The channel count in `sounds.zrd` (HIGH: 2) is not read; 1 is stored | (bug) on Windows 9x the mix was mono, so the 3D sounds' left/right pan was lost | The data's channel count is read (Owen 2026-10-20). The SDL output mixes stereo either way |
| Medium sound format, 0x570ccf and the per-sound formats | Looks up `"MED"` where `sounds.zrd` says `MEDIUM` | (bug) a typo: the medium playback format is never read | `MEDIUM` |
| Mad Cat and Supernova damage textures, skin reader (`mech3::read_skins`) | `damage` lists two (Mad Cat) and one (Supernova) slots longer than the primary skin, so every damage mask lands on the wrong texture; the size check then drops some | (bug) misplaced data: those mechs show the wrong or no battle damage | Leading empty slots are dropped until the damage list is as long as the primary (Owen 2026-10-20); `tests/test_fixes.cpp` checks both on the real files |
| Plain PPC, weapon table 0x5e64f0 | 37 weapons; `weapons.zrd` defines a `PPC` and Mech3Msg.dll names it (0x2fd, after the last weapon's 0x2fc; the old slot 0x2ee between NARC and the pulse lasers is empty) but the table has no row | (bug) the mech lab cannot offer a weapon the data defines | A 38th row, `PPC` (message 0x2fd, family 3 as ERPPC, Inner Sphere tech 2 as the plain lasers, no ammo); `mech3::weapon_count()` (Owen 2026-10-20) |
| ZAPS_HUD, WeaponType reader 0x4af71b | The second value is read from the first value's slot again | (bug) the HUD static (0x43be70 adds the first value to a channel's level and keeps the largest second value as a hold time; 0x43d07a counts the hold down before the level fades) held 0.5 s after an ERPPC hit, not the 2 s the data gives; only the ERPPC has ZAPS_HUD | The second value is read (Owen 2026-10-21: traced, the hold is its only use); `zfix::options().bugs`, off in the probes, so `test_weapons_zrd.py` checks the exe's (0.5, 0.5); `tests/test_fixes.cpp` the (0.5, 2.0) |
| zWeapon kill_all, 0x584fe0 | Runs every def's list through the projectile kill 0x584f40, BEAM defs too | (A) a firing beam (0x150 bytes) is taken for a projectile (0x5d8): the kill writes its speed past the beam's end and stops the beam's zone pointer and loop sound as animations; the exe gets away with heap slack, C++ would corrupt memory | BEAM defs are skipped (their beams are stopped by their weapons). Reached when the pool's scene nodes are freed with a beam still firing (`test_weapons_fly.py` never kills with one firing) |
| Mine bounce length, 0x586740 | A pass that does not bounce (a record at the mine's own position) reuses the bounce length left from the last pass, uninitialised on the first | (A) stack garbage | The last length, 0 on the first pass (unreachable in the shipped game: no weapon is a MINE) |
| Projectile pool and beams, zwep_init 0x583880, 0x584a70, 0x5832f0 | `calloc`'d pool of 0x5d8-byte slots; beams and node handler tables `calloc`'d | (A) 32-bit layouts holding pointers | Typed structs: a `std::vector` of slots zeroed as calloc's (same free-list order), `new` beams and tables |
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
| AI regions, 0x4c5b00 (from 0x4bf3f0) | A version-108 file without a `name` gives the region `strdup(NULL)` | (bug) a crash at mission load (emulator: the `no name` case) | The region has no name, so the lookup (0x4c5c00) drops it as the exe would have had it lived; no shipped file lacks one |
| AI nets, arrangenodes 0x4c7340, node links 0x4c71f0 | A link's node is read from the builder's slots unchecked, and a link index may reach the link count | (bug) a link past the file's nodes reads past the vector | Nothing to arrange, no node; the shipped files never link past their nodes |
| AI net compile, 0x4bde20 / 0x4bd790 | A node's properties are chained onto each of its links through a pointer into the node step's links, which have lost their trailing links to no positioned node | (bug) a write past the vector; a node whose links all go nowhere crashes the load (emulator: 0x4bd790..0x4bdd70) | That link's chain starts nowhere; its steps are still made, as the exe's are (`test_ai_nets.py` pins the exe's read past the links to -1) |
| AI net compile, 0x4bde20 | A link to a node number past the net is looked up in the slot map unchecked | (bug) heap garbage as the link's target; c2 mission 3's `net_13` does it | No target (-1), so no link (the test pins the exe's read to -1) |
| AI net compile, 0x4bd790 | A node linking to one node twice with an edge node on that link makes more steps than were counted | (bug) writes past the compiled net's vector | The extra steps are dropped: the net is the size the exe's is, the chain through them broken as in the exe |
| AI net steps, 0x4c9e60 | A property step whose chain ends at a slot with no step keeps the slot number as its pointer | (bug) followed later, a crash | The null step |
| AI way finder, valve unions 0x4d4490 | Each entry's valve is read unchecked | (bug) a union entry without a valve (no name) crashes the search | The entry does not open the way |
| Numbered objects, 0x58b060 | An index out of range asks element 0 of the object, which an empty object does not have | (bug) reads past an empty vector | None |
| Input, zInput (0x51a170..0x51ddc0, input.md) | DirectInput 5 (DirectInputCreateA, IDirectInputDevice2: keyboard buffered, mouse and joystick states, cooperative levels, axis ranges and dead zones) and Win32 (ClientToScreen, ScreenToClient, SetCursorPos, GetClientRect, MapVirtualKeyA, GetKeyboardState, ToAscii) | (A) DirectInput does not exist on macOS | `platform::Input` with DirectInput's structures and results, `zin::Host` for the Win32 calls; on SDL3 the exclusive mouse is relative mouse mode while acquired, motion scaled to the 640-pixel frame, the joystick's ranges and dead zones applied to SDL's axes, characters from SDL's keymap |
| Keyboard read, 0x51a5d0 / 0x51aa00 / 0x51a300 | A key code (DIK code with Alt 0x100, Ctrl 0x200, Shift 0x400) indexes the 0x7de key states unchecked | (bug) a DIK code from 0xde up (Power, Sleep, Wake, the web and media keys) with all three modifiers held writes past the table into the text callback's pointer (a crash at the next key) and the joystick's globals | Codes past the table are skipped |
| Mouse start, 0x51b5f0 | CreateDevice's failure ignored; QueryInterface called through the pointer it did not get | (bug) a crash at start on a machine without a mouse | The mouse counts as present and reads nothing |
| Key names, 0x51c480 | A code's low byte indexes the 0xde key names unchecked | (bug) 0xde..0xff read the joystick's names, then numbers, as names | No name |
| zReader text files, 0x556240 / 0x5563a0 (zreader.md, Text files) | `_access` on the path, then the search path; `CreateFileA`, `ReadFile` a byte at a time, `SetFilePointer` | (A) Win32 | zutl::open (the write dir, then the mounts) and stdio; emulator-verified identical trees (`test_zreader_text.py`) |
| Text tokens, 0x556510 | An unquoted token of 256 characters writes its terminator over the low byte of the return address | (bug) the reader returns into the middle of an instruction; its "too long" message (after 257) is never reached cleanly | The token is dropped and the list ends, as the message intends (`mw3tests`) |
| Text strings and directives, 0x556510 / 0x557550 | A quoted part is read with no length check and on past the file's end; an `#ifdef` or `#else` without its `#endif` scans for `#` past the end | (bug) a stack overrun, or a hang at load | Both stop at the end of the file; a quoted part keeps 255 characters (`mw3tests`) |
| Controls file axes, 0x4120a0 (controls.md, Setup) | An AXES entry named other than Throttle, Twist, Pitch, LR is stored as a fifth table entry | (bug) it overwrites 0x604110..0x60411f: the table's flags and the groups' vector, so the game crashes at exit or in the controls screen | Skipped (`test_ctlcfg.py` puts the original's bytes back after its read and checks the rest) |
| Controls file entries, 0x4120a0 | Every KEYS / AXES item is read as a list whose first item is a string (and KEYS, AXES as lists) | (bug) a hand-edited file with a number or an empty list there crashes the game | Such items are skipped (`mw3tests`) |
| Controls screen rows, 0x0042c4e0 / 0x0042cca0 / 0x0042ce90 (menus.md section 6) | The selected row's command is read from the commands list unchecked | (bug) a group with no listed command (no name, or empty; not in the shipped setup) reads through a null vector: a crash | Command 0 (`ControlsList::value_at`); unreachable with the shipped groups, so the test never meets it |
| Controls screen file path, 0x0042d0b0 | `sprintf(buf[0x104], "%s\\%s.zrd", "keys", name)`, the name from ActiveCtlCfg (registry, up to 0x104) or the popup | (bug) a long ActiveCtlCfg overruns the stack buffer | Cut at 0x104 bytes (`snprintf`) |
| Controls name map, 0x410fe0 | `std::map<const char*, int>` keeps the pointer it is given; a name from a controls file is added and the file freed | (bug) a dangling key, read by later lookups and the writer | The map keeps its own copy of the name |
| Mission start names, 0x49e810 (mission-start.md section 9) | A vehicle's aiv.zrd name is copied with `sprintf(buf, name)` into 28 bytes before the record being read, its type into 20 before that | (bug) a `%` in a name is read as a format; a name of 20+ characters loses its start, one of 28+ overwrites the record's vtable and settings | Names copied as text, kept whole (`zfix bugs`; off, the exe's frame is laid out and gives its result). No shipped name is over 19 |
| Vehicle controller, 0x49f800 | Without an AI net the vehicle is given the aiv record as its controller | (bug) its animation slot becomes a stack address; a failed build deletes the record as a controller (a crash). Only a `mech...` suffix without a net reaches it; none ship | No controller (`zfix bugs`; off, the record's handle, as the exe) |
| Lancemate slot, 0x49e810 | `new(0x18)` is written through unchecked | (bug) a crash if it failed | Skipped |
| AI radio names, 0x4e7dc0 (ai.md section 12) | `sprintf("%s_%s", voice, event)` into 32 bytes | (bug) a radio voice (aiv.zrd `sound_tag`) of 15 or more characters overruns the frame; the shipped ones are 3 to 5 | Cut to fit (`test_ai_brain.py` keeps voices under 15) |
| valves.zrd records, 0x4e2330 (ai.md section 13) | Every tag's value is read as a list and every name item as a string | (bug) a hand-edited file with a number or a string where a list or name belongs crashes the mission start | Such tags and names are skipped (`test_ai_valves.py`'s six broken files) |
| Valve scans, 0x4e3200 | A scan action keeps the vehicle's brain found at mission start and writes through it when it fires | (bug) a scan naming a vehicle that is missing or not AI-driven writes through null (a crash); no shipped mission has scans | Nothing is written |
| Valve teleport, 0x4e3ba0 | The pilot's current order is read from the back of its deque without a check | (bug) a pilot with no order (a net that is not compiled) reads before the deque: a crash | Skipped |
| Delayed updates, 0x4d3d30 | A lone timer whose update starts another delayed update becomes a list of both as it runs; the slot is then cleared | (bug) the new timers are lost: c3 mission 2's `ambush_powerup_down_a` can lose three radio lines | The list stays (`zfix bugs`; off, as the exe, which the test checks) |
| AI vehicle controls' destructor, 0x4ac800 | Calls its owner's vt+0xec with no check that it has one | (bug) a vehicle that fails to build deletes its controller before giving it an owner (mission-start.md section 5): a crash | The owner is asked only when there is one |
| Firepoints, 0x46cb80 (mech-build.md section 12) | A definition's firepoint takes the part of its model node with no check that the node exists or a part owns it | (bug) a crash (it reads through a null part) for a model without one of its definition's firepoints; none ship | The firepoint keeps part -1 |
| Launch, 0x491150 (mech-build.md section 12) | The definition's skins are counted without a null check; the HUD reset at the end gets 0x14 (no HUD + 0x14) when the view is not on the player's mech | (bug) crashes: a definition without skins (none ship), the view elsewhere (cannot happen: the launch attaches it first) | Checked; the HUD call skipped (`zfix bugs`; off, as the exe) |
| HUD constructor, 0x43b190 (hud.md section 3) | +0x22e3c, the resolution's hud.zrd block during the set-up, is not cleared; the set-up keeps it for a video mode without a block | (bug) the first set-up in such a mode reads hud.zrd through whatever `new` left there: a crash or garbage positions; the shipped modes all have blocks | Cleared in the constructor (`zfix bugs`; off, as the exe, in `test_hud.py`) |
| zRender's 2D primitives, 0x563c50, 0x5630c0, 0x563440, 0x562970 (twod.md) | Pixels written without a clip rect are not checked against the surface | (bug) a shape partly off screen writes into the next rows or past the surface | Off-target pixels skipped, as `line16` already did (`test_prim2d.py` keeps shapes on the target) |
| HUD line draw, 0x5a68b0 | Saves the render's clip volume only when the line has a rect, but always puts it back | (bug) a clipped line without a rect restores the box from its uninitialised stack | Saved either way; nothing in the HUD makes such a line |
| Foot, 0x462590 | The foot's node is read through unchecked | (bug) a crash for a model without the foot's node; none ship | Sole 0, no shadow or toes |
| Controls file location, 0x42d0b0 / 0x411e60 / 0x4120a0 | `keys\<name>.zrd` relative to the game folder, `_mkdir("keys")` | (A) the install is read-only data in the rebuild | Written to and read from the write dir through zutl::open, which makes the folder; text mode (LF on macOS), which the reader takes either way |
| Node bounds, 0x4ecc00 | `printf("haha")` for a node named `blackhawk_01` each time its bounds are rebuilt (a developer's leftover) | (A) the Windows game has no console; on macOS it would print to the terminal every frame the node moves | Dropped (`zclass/class.cpp`, `action-lists.md` section 7) |
| Sequence step, 0x4fdf50 | A sequence without entries reads the entry past its data (the calloc'd header, then memory before it once the index steps to -1) | (bug) undefined reads, possibly a hang | The time still advances; with no entries nothing steps. No shipped script makes a sequence (`zclass/seq.cpp`) |
| Controls file list, 0x42eff0 (menus.md section 6) | `FindFirstFileA` / `FindNextFileA` over `keys\*.zrd` in the game folder, folders skipped; the handle never closed | (A) Win32; the files are in the write dir | `Engine::list_files` -> `zutl::list`: the write dir's and every mount's `keys`, each name once, sorted as NTFS lists a folder (by the upper-cased name); the same names reach the list in the same order (`test_loadsave.py` hands both sides one listing) |
| zTwoD's 2D line, 0x563b80 (behind 0x562940; the edit bar's caret) | Every pixel written, unclipped | (bug) a point off the screen writes outside the buffer | Pixels off the target are skipped; the same pixels otherwise |
| Line editor resize, 0x5a4740 | `strncpy` of the old text into a shorter buffer leaves no terminator | (bug) a text longer than the new size runs past the buffer (the popup resizes its empty line, so nothing shipped reaches it) | Cut at the buffer's last byte, as set_text cuts |
| Edit bar text, 0x5a52a0 (from the file list's select, 0x42f1c0) | A null text goes to `strncpy` and `strlen` | (bug) a crash when the list's select is given -1 or an index past its items (nothing in the popup does) | Empty |
| File list layout, 0x48cda0 | With no row shown (no items, or `top` at the end) the rectangle's bottom is a stack slot never written; the shipped popup's list has no image, so this rectangle is its hit area | (bug) the empty list's hit area is garbage: a click anywhere below could count as on the list | The bottom is the top: an empty list has an empty area. `test_loadsave.py` writes the same into the original's slot (a code hook) |
| File list rows, 0x48cda0 | Each row's text is set with the item's name as the printf format | (bug) a `%` in a file name reads arguments that are not there (garbage, or a crash on `%s`) | The name shown as it is; identical for every other name |
| File list row font, 0x48d1c0 | The row is indexed by the item's index | (bug) an item below the shown rows indexes past the rows (no caller in the ported screens) | Only an existing row changes |
| Pilot files, 0x478ce0 / 0x478560 / 0x4781e0 / 0x477fe0 (pilot.md) | `CreateFileA` OPEN_EXISTING then OPEN_ALWAYS (never truncating), `ReadFile` / `WriteFile` / `SetFilePointer`, `DeleteFileA`, `MoveFileA`, `_access`, `_mkdir` on `pilots\` in the game folder; `FindFirstFileA` / `FindNextFileA` over `pilots\*.mw3`, closed with `CloseHandle` | (A) Win32; the files are in the write dir | C stdio in the write dir, the same reads and writes in the same order, never truncating; a pilot only in a read mount (an imported install) is copied to the write dir when first opened; `MoveFileA`'s refusal of a taken name kept (a change of case moves); the listing is `zutl::list` (`test_pilot.py` compares every file's bytes) |
| Game paths, `zutl::host` / `zutl::list` | Names in the ANSI code page (Windows-1252): what the edit bars type | (A) the host's names are UTF-8 | A relative game path is converted to UTF-8 for the host, a listed name back (a name with a character Windows-1252 lacks is not listed) |
| Pilot campaigns and lancemates, 0x4790d0 / 0x434040 | A damaged count reads on past the file's end, making a campaign or a slot each time | (bug) a corrupted pilot file hangs the game for up to 4 billion reads | Stops at the end of the file (`test_pilot.py` makes the original stop there too, 0x479366, 0x434244) |
| Pilot name, 0x477b20 / 0x478560 | `strcpy` of the name into 64 bytes | (bug) a pilots\ file name of 64 characters or more overruns the record into the games' vector: a crash at the pilot screen | Such files are not listed; a name is cut to 63 characters |
| Current pilot, 0x477f50 | Deleting a pilot leaves [0x0060861c] pointing at it when it is the current one | (bug) a dangling pointer (the pilot screen's DELETE on the pilot chosen before; a lobby launch then took it for a pilot) | Cleared (the test clears the original's the same way, 0x477f56) |
| Pilot names, 0x478be0 / 0x477fe0 (pilot.md) | Names are compared with case (strncmp) while the file names are not, and a rename never checks the new name is free | (bug) "BOB" made over "bob", or a rename onto another pilot's name, shares one file: both are listed and whichever is saved last decides what it holds | Names match without case (ASCII and the Windows-1252 letters), so "BOB" finds bob; a rename onto another pilot's name is refused (Owen 2026-10-20); `zfix::options().bugs`, off in the probes; `tests/test_pilot_names.cpp` |
| Loadout free slots, 0x425bd0 (mechdef-configs.md) | Each item's slots come off the table entry of its part, +0x90 + 4 * part, unchecked | (bug) an item placed in a part outside 0..8 (a damaged saved loadout) writes past the table into the loadout | Only parts 0..8 are counted per part; the total still drops |
| Loadout copy, 0x423e00 | The item added for each copied one is used without a check, and a new engine item is never made | (bug) copying a loadout with an engine item (a hand-made saved file) writes through null: a crash | The item is skipped |
| Pilot screen DELETE, 0x487e30 | With no pilot chosen the question formats the name at address 0x18 | (bug) a crash: DELETE pressed before any pilot exists | Nothing happens |
| Pilot list item, 0x488770 | The item's value is the pilot's address (read by nothing) | (A) a 64-bit pointer | 0 |
| Sensors' C3 merge, 0x4a9fd0 | The union of the master's and a member's contacts is written into the scratch vector [0x609a5c], sized to zEntity's list (at least 32) | (bug) targets.zrd objects count as contacts too, so a big enough union writes past the heap block | The union goes into a vector of its own that grows; the same contacts result. `mech3/sensors`, test_sensors.py (the original's path: unions within the scratch) |
| Sensors' contact, 0x4a9530 | The seen entity's vehicle (vt+0x2c) is written through without a check | (bug) a vehicle-less entity in sight would crash the game | Not written; every mech and target answers itself, so play never reaches it |
| Entity sort, 0x4d7650 | Two lancemates' pilot names (brain +0xd4) are compared with strcmp | (bug) a lancemate without a `pilot` in aiv.zrd crashes the mission start | It ranks after the named ones; every shipped lancemate has a name |
| Soil table, 0x47f400 (mission-start.md section 11) | A soil name zModel does not know gives index -1 and the game soil is written to the word before the table [0x608c00] | (bug) a write outside the table; no shipped soils.zrd lacks a name | Skipped, as is an index past the table's 64 |
| Shot queries' model test, 0x54cad0 from 0x4ef570 / 0x4ef980 / 0x4efaa0 / 0x4efba0 (movement-collision.md, The shot queries) | A crossing found for a record already holding 32 is written past the record (only the walk's entry checks) | (A) memory safety; a shot through more than 32 nodes | Stops at 32; differential test identical on the six worlds |
| Point query's model test, 0x54be40 from 0x4eddb0/0x4ee140/0x4ee220 (mission-start.md section 11) | A polygon found for a record already holding 32 is written past the record (only the walk's entry checks) | (A) memory safety; no shipped place has 32 nodes' polygons under one point | Stops at 32; differential test identical on the six worlds |
| Selected group's ammo, 0x4b1cd0 (the HUD's) | An empty group in group fire, or a group without a current weapon, returns a byte of the saved ECX: the top byte of the weapon manager's address | (bug) the answer is whatever the heap address holds, usually "has ammo" or not by chance | No ammo; `test_weapon_planner.py` checks the exe's byte and the port's no apart |
| AI order stack, 0x4e9100 | Searching the orders for a net, the loop steps past a full last block of the deque into the map (the push takes its next block only when it needs one) | (bug) reads past an 8-byte heap block: a crash or garbage when no order is on the net and the orders fill a block exactly (13, 39, ... of them) | The search stops at the last order: no order on the net; `test_ai_orders.py` counts the case apart |
| AI brain's attack, 0x4e9710 | With no target vehicle the distance is FLT_MAX, compared with the attack range; a range past it (a NaN or infinite params 3) takes the attack branch and calls the null vehicle's vt+0xd8 | (bug) a crash on broken aiv.zrd data; no shipped record has one | No attack without a vehicle; `test_ai_frame.py` keeps the ranges finite |
| Quicksand texture, declient.zrd read 0x501ab0 | One frame: adds the name past the list's end (frames[1]) and drops the texture; no `DEFAULT_TEXTURE`: reads the last mission's freed name list, or a null one | (bug) a crash or a random texture at mission load | One frame: that frame, set as the material's texture; no `DEFAULT_TEXTURE`: untextured ("Quick sand will NOT be textured"). No shipped file reaches either; `test_zdec_init.py` runs them on the port alone (craters.md) |
| Crater clip buffers, Weiler 0x512940 | The edge, contour and crossing buffers grow by realloc while edges and contours hold pointers into them | (bug) past 128 entries (a large clip) the clip reads and writes freed memory | Buffers that never move (a deque); same results where the original works (`test_weiler.py`, craters.md) |
| Crater clip side tables, Weiler 0x513750 | Each polygon's points against the other's edges go into two fixed 1,280-float tables | (bug) for large polygon pairs the first table runs into the second and the second past the Weiler's end | Tables sized to the polygons (craters.md) |
| Crater clip walk, Weiler 0x510da0 | A polygon corner lying on a region edge to within a float's error: each pass makes a new crossing a little off the last, and the divided pieces link into a cycle that never returns to the ring's first edge | (bug) the game freezes in an endless loop that keeps allocating, when a crater's clip meets that case | The walk gives up after 64 + 16 x the edge count steps for one region edge, through the walk's own error exit (the clip of that polygon fails); `test_weiler.py` confirms the original never returns on those inputs (craters.md) |
| Crater clip contour count, Weiler 0x513390 (and the output 0x5135a0) | After some clips where the polygon shares an edge with the region and touches it at a corner, a merged contour's links cycle without coming back to its first edge | (bug) the game freezes in an endless loop counting the contour | The count gives up after as many steps as there are edges and the clip of that polygon fails; the output stops after one lap, which is the whole contour (the exe writes on without end when the first edge's prev was left off the ring, a crash); `test_weiler.py` confirms the original never returns on those inputs (craters.md) |
| Crater ring triangles, convexify 0x515be0 (0x515aa0) | A point's usable edges are listed in 32 ints on the stack | (bug) a polygon corner seeing more than 30 points of a crater ring writes past the list on the stack | A vector; same triangles (`test_convexify.py`, craters.md) |
| Crater record, zdec 0x4ff940 / 0x4ffad0 | The record is freed when the crater is done, but the crater node (ZDEC_FEATURE) keeps pointing at it (+0x40) and the next crater in the cell tests its box against that | (bug) the 5 m overlap test reads freed memory: whatever the heap left there decides it | Each crater node keeps its type and box (zdec::craters().features); `test_crater.py` (the emulator's heap never reuses, so the original reads the old record as intended) |
| Untextured crater or quicksand, zdec 0x4fff80 / 0x4ff940 / 0x5026c0 | The floor centre's UV is written through the UV buffer that only textured craters allocate; the dust reads the texture entry they alone have | (bug) a crater ordered without its texture flags (0x1008) crashes the game; every shipped crater has them | Skipped without a buffer; `test_crater.py` confirms the original crashes |
| Polygon add, colinear points, zModel 0x53a530 / 0x53c6c0 | Colinear points are dropped from the points but not from the normals, UVs, colours and morphs | (bug) the texture and shading of such a polygon slide by one corner from the dropped point on; about 1 in 200 crater polygons (a cut piece's corner on a straight edge) | zfix bugs: the other arrays drop the same points (`test_gmod_add.py` checks the port against the original fed the cleaned arrays; the probes test the original path) |
| Strip expansion, zModel 0x553160 | The new triangles' flag bits beyond the count and normals come from the uninitialised realloc'd record | (bug) garbage flags; no shipped model has strips | Those bits 0 |
| Crater clip node list, zgeo 0x50ec10 | The list of changed nodes is sized by the cell's node count, but the world's own ground nodes may add to it | (bug) a heap overrun when they do | A growing list |
| Placing a mech, 0x4161c0 | The ground pick is given y + 10 as its step, so it reaches 2y + 10: below sea level nothing is in reach and the lowest surface is taken, high up a surface up to y + 10 above can be | (bug) a mech re-placed where it stands (a mech swap, 0x4783c0) can drop under a bridge or land on a roof; the other callers pass a constant | A step of 10 (Owen 2026-10-20, `zfix bugs`). Every shipped place checked both ways (`placements_on_every_campaign_mission_headless` with `MW3_PLACE_POINTS`): 195 campaign placements and 768 aiv.zrd records, none moves; `fix_place_steps_ten_above`; test_place.py checks the exe's path |
| TAG's hold, 0x5814b0 | The time on the target adds each frame's dt in float and must reach DESIGNATE's 1.0 | (bug) 60 x (1/60) and 144 x (1/144) fall just short, so the hold takes a frame more at those rates | Compared in whole milliseconds: one second at any rate (Owen 2026-10-20, `zfix bugs`); `fix_tag_hold_and_beam_in_milliseconds`; test_tag.py checks the exe's path |
| TAG's beam, 0x4b4230 | The charge drains ftol(10000 dt) a frame and the beam stops at the update after it empties | (bug) the beam lasts 32 frames at 30 fps (1.067 s), 62 at 60 (1.033), 146 at 144 (1.014): the slack over the one-second hold is 2, 1 and 1 frames | The beam lasts its 30 fps time, 1.067 s, at any rate (stopped at the first update at or past it, the charge drained evenly meanwhile): about 67 ms of slack everywhere (2, 4, 10 frames). It still fires only from a full charge, and letting go keeps what was left (Owen). `zfix bugs` |
| TAG's strike, 0x4afed0 | The four missiles lock on the point the beam touched (no entity, no velocity) | (bug) they arrive 7 s later where the target stood: a mech walking at 30 kph or more is not touched | When the marked thing is a mech, the lock is the mech's live position and velocity (`WeaponCalls::strike_target`, the hit node's damage handler); anything else keeps the point. `zfix bugs`. `TAG_UNIT=1 test_tag.py` flies it with the original's homing: four hits at 30, 60 and 144 fps, standing, walking, crossing and running at 20 m/s; `fix_tag_strike_homes_on_the_unit` |

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

- Saved game replaced before the question, 0x478f10 / 0x434e00 (campaign.md
  4): after a mission the next mission's saved game is written by zSave
  (0x5a8a70, CREATE_ALWAYS) before the pilot asks "Replace existing game?";
  No keeps the old entry but its file already holds the new world. Fix when
  zSave is wired: write the world after the answer, and not on No.

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
- Missiles that miss vanish at RANGE without exploding; kept (Owen
  2026-10-21: no change).
- Radio chatter (QUEUE) plays newest first; kept as design (chatter stays
  current).
- Every mech's maximum engine forced to 400 and only a weapon's first group
  counting look deliberate; kept.

Resolved (2026-10-21):

- ZAPS_HUD: traced and fixed, see the table (the second value is the HUD
  static's hold time).
- Pock marks: not a bug in the exe. The impact table is the engine's soil
  list (0x5edbc0, count 0x5edbb8) at the time weapons.zrd is read, and the
  mission script's `LoadSoils soils.zrd` (0x537cb0, run before the weapons at
  0x490173) has added dirt, mud, grass, concrete, snow, mech, silt and noslip
  by then: fourteen kinds, a material's +0x20 indexing them. So the dirt, mud
  and snow blocks are read and the marks show (28 weapons). The port had a
  fixed six-kind table; it now keeps the engine's soil list
  (`zmod::soil_names`, `load_soils`) and sizes the table by it, as the exe
  does (`test_weapons_zrd.py`: LoadSoils on 41 files and the fourteen-kind read
  against the original). No FIX.

## Decided (Owen, 2026-10-20 afternoon)

To do, in the specialists' queues:

- Placing a mech (0x4161c0): done (mac2/place-fix): a step of 10; no
  shipped place moves (mission-start.md section 11, "The step fix").
- Valves that update themselves round a loop: add the guard (a valve's
  reactions are not re-entered while they run). Owen may make his own
  missions later. FIX (bug).
- c4b mission 6's boundary: make the valve trip when the player leaves the
  area (the region names the player's orders give do not exist; find the
  pair the mission's geometry means). FIX (bug).
- TAG: done (mac2/tag-fix): the hold in whole milliseconds, the beam's
  time its 30 fps one, the strike on the marked unit (weapons.md, "TAG,
  end to end", The fix).
- Allied pilots fight the target they are told to, then pick their own
  targets after it (the attack 0x4e9710's "nearest enemy when none is
  selected", which never happens as written). `zfix bugs`.
- Inverted mouse Y: keep the player's sensitivity and flip only its sign
  (the options' ACCEPT wrote -1.3 or 1.3 over MouseAccelY). FIX (bug). Done
  (the table above).
- Cosmetic slips in the data (spelling, a credits heading's `"offset" (20)`
  missing its second number, so it borrows the line above's): fix them
  (Owen: "we can fix little cosmetic things like spelling errors").
  FIX (bug), each listed here and in CHANGES.md. Done for the credits'
  heading (the table above). The credits file's other offsets and
  fonts are consistent (headings (20 20), names (30 0)); a spelling pass
  over the message table (Mech3Msg.dll) is not done yet. The HUD's `hud_othererrow_*` images are misspelled
  but are names the code looks up, not text shown, so they stay; a credits
  name has a trailing space, which does not show. Also done: TARGET's
  distance past 1 km, " (1.23km)" (the table above).
- Resolution: 16:9 as the shape either way (today's monitors); limit the
  size to 1920x1080 if larger sizes are a problem.

Kept:

- The AI's health floor (only pilots already hurt at their first check are
  held back; pilots leave a fight as they always have).

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
  after the settings loader and the menus. The main menu (mainmenu.md,
  Resolution) places its version line for 640x480 and its dialog in
  640x480 pixels, while its background and title wrap at the display size.
  The option and credits screens (menus.md, Resolution) are 640x480 dialogs
  too; their background view is the display size when they are made (a
  mission's when the pause screen makes them), and the scrolling background
  is cut into four pieces, which cover at most twice the image (2560x1920):
  a 4K screen needs it tiled further. The graphics screen offers modes 5..7
  (and 3 for software) by a video-memory estimate (0x004ab860); its names
  for 8..10 (1152x864, 1280x1024, 1600x1200) are never used, and ACCEPT
  writes the chosen mode over a registry-set 8..10.

- Quicksand that does nothing (found 2026-10-21, craters.md section 3):
  a quicksand pit's lid is solid ground at the hit's height in zModel soil
  3, named "quicksand", but the game's soil table has no quicksand entry,
  so it reads as soil 0 and a mech walks over it as on flat ground. The
  name suggests a slowing or sinking effect that was cut or never wired.
  Options: keep it as a picture (the original as played), or give soil 3
  an effect (a guess at intent: how much slowing, whether it sinks).

# Pirate's Moon: a first look

Not a port. Pirate's Moon (PM, the expansion) is read side by side with
MW3 1.2 for what it tells us about MW3: the same studio, engine and code
base, built eight weeks later. Started 2026-10-20 (Owen: "see if it has any
hidden files that could give us some extra insight ... discrepancies between
mw3 and pm, which might connect dots").

## The copy

- Source: `MW3 pirates moon.zip` from Owen's Win98 backups, a Redump-style
  image: `Track 1.bin` (MODE1/2352, volume `MW3 PM`) and `Track 2.bin`
  (CD audio, about 9.4 minutes: the expansion's music).
- Extracted under `game/` (git-ignored): `game/pm-disc/` (the bins, `pm.iso`
  made from track 1, `iso/` its files) and `game/pm/`, the install
  assembled as for MW3 (every DATA1.CAB component merged, no collisions)
  with the disc's loose `MECH3.EXE` as `Mech3.exe`.
- `Mech3.exe` PM 1.0 (U.S.), sha256
  `57cd2ee74bafda3eeb9e9353c7c78a8b7c213a81f3040f1be7c2a164c505ddd9`,
  2,220,057 bytes, linked 1999-11-17 02:05 UTC (the code also carries the
  date string "Nov 14 1999"; the readme is dated 15 November 1999). MW3 1.2
  was linked 1999-09-22. Sections: `.text` 0x401000 (0x1dca18, 140 KB more
  than MW3's), `.rdata` 0x5de000, `.data` 0x602000, `.rsrc` 0xb0f000; entry
  0x5cefe8. Window class `Mech3Ep1Class`, title "MechWarrior 3 EP1".
- No official PM patch is in Owen's backups (only a third-party no-CD
  patcher, `MW3PM-NoCD.EXE`, in `MW3 patches and cracks.zip`), and none
  turned up in a web search. This is PM 1.0.
- Also on the disc: DirectX 7 (PM requires it; MW3 needed 6),
  `HSBR.EXE` (Hasbro Interactive's product registration) and
  `MECH3R~1.EXE` (registers "MechWarrior 3 EP1" with DirectX through
  dsetup's `DirectXRegisterApplicationA`), `EXTRAS/SYSINFO.EXE`.

## The source tree, from the assert paths

MW3 1.2 was built from `C:\zipper\Mech3\` and `C:\zipper\GameZMech3\`. PM
was built from a branch, `C:\zipper\branchep1\`, with the game in
`mech3ep1\` and the engine in plain `gamez\`: the engine was branched for
the expansion, not shared. The files with assert strings, compared:

- New in PM: `gamez\zClass\cls_clutter.cpp` (a clutter class: scattered
  props such as barrels, tents, rocks and wrecks, from `templates.zrd`;
  strings `ClutterLoadTemplates`, `dirt_clutter`, `sand_clutter`,
  `pirate_clutter`, `clutter_*.flt`), `mech3ep1\promote1.cpp`, and
  `mech3ep1\mech_firepoint.h` (`CZMechFirePoint::CFixup`: firepoints are
  moved to the torso when attached to a part that cannot carry them).
- Renamed: `zReader\zreader.cpp` became `zReader\zrdr_main.cpp`.
- Gone from PM: `zFMV\fmv_main.cpp` and `fmv_stream.cpp` (PM ships no
  movies; `fmv_script.cpp` stays) and `mech3\mech_salvage.cpp` (the salvage
  screen code; PM has `SALVAGE_AWARD` and `SALVAGE_BUTTON` strings, so
  salvage moved or was rewritten).

## The data

- The same InstallShield components as MW3 (hardware and software textures,
  mech textures, sound sets, database files, fonts, program files).
- `reader.zbd` holds 198 files to MW3's 178. New: six mechs (Atlas,
  Awesome, Centurion, Clint, Masakari, Ryoken: `dfn_`, `anim_`, `_prints`
  each), `cargotruck`, `mobile_hq`, `packrat`, `bubbles`, `clutter_ani`,
  `dlgLoadSave` (a load/save dialog), `ia_progression` (Instant Action
  progression), `mprounds` (multiplayer rounds). Gone: `mechs.zrd`,
  `player.zrd`, `equipment.zrd`, `camera.zrd`, `dlgMechLab.zrd` (the
  second mech lab dialog stays) and `mechLibDemo.zrd`.
- New mission readers per world: `readersa1..4.zbd` (`zbd\c%d\readersa%d.zbd`
  in the exe, with `sainfo.zrd`, `start_standalone_%s`, "Standalone State":
  standalone missions) and `readeria4.zbd`, plus new campaign missions.
- **Archive version 2.** PM's `.zbd` archives end with version 2, the count
  and one more dword, where MW3's end with version 1 and the count (MW3's
  reader 0x5767b0 accepts only 1). The dword is a CRC of every byte before
  the entry table, by MW3's own CRC (`zutl::crc32`, 0x576c10: MSB-first,
  polynomial 0x04c11db7, starting from 0): checked on PM's `reader.zbd`
  (0x8538af96). mech3ax also knows version 2. The entries' second name
  field holds the name again in PM; in MW3 it held the build machine's temp
  path (`C:\games\mech3\data\common\zrdr\bri2191.TMP`).
  `tools/analysis/zrd.py` reads both.

## The exe's new strings, by topic

From `strings` on both exes (only in PM):

- Weapons: `HeavyLaser_Small/Medium/Large`, `XPulseLaser_*`,
  `ERLaser_Micro`, `GaussRifleLt`, `double_sinks`, turret `tur_ppc02`.
- Units: `centurion`, `masakari`, `cargotruck`, `hulk_apc`, `Emobilehq`,
  `mechmover`, `the_dude01..16` (16 named pilots or infantry).
- Lighting: light maps (`LIGHT MAPS`, `LightMapTexture`,
  `LightSetLightMap`, `LightSetBicolored`, `LightSetAmbientColor`,
  `LightSetDiffuseColor`), per-node light overrides
  (`NodeSetOverrideAmbient/Diffuse`), `spotlight`, `headlight` and
  `ai_headlights` (mech headlights, `CMD_HEADLIGHT_TOGGLE`), `cloud_cover`.
- Controls: `CMD_EJECT`, `CMD_SNAPSHOT`, `CMD_TARGET_CLOSE_L` /
  `NEXT_L` / `PREV_L` (lancemate targeting), `MSG_KEY_LMENU`.
- Multiplayer: Attack/Defend with one or two bases, team and free-for-all
  deathmatch, rounds with overtime ("We are into Overtime, folks!"),
  pilot statistics (`Total Kills`, `Kills per 1k Shots`).
- Saves: `pilots//games//%s%02d.mw3`, `LOADSAVE_DIALOG`, `CZUnload::LoadMission()`.
- AI: `retreat_dist`, `retreat_health`, `retreat_time`, `scan_delay_*`,
  `scan_duration_*`, `approachRange`, `attack_radius`, `aimTolerance`.
- Debugging left in: `PrintChildren`, `PrintUsedTextures`, `ShowClip` /
  `ShowNoClip` / `ShowTStrips`, render and texture-stage state dumps, a
  `db_verbose` switch, `tweaks.zrd`.
- Windows: `QueryCancelAutoplay` and the Explorer `NoDriveTypeAutoRun`
  policy (PM switches CD autorun off while it runs), `AudioCD\Shell`,
  `DirectDrawCreateEx` (DirectX 7), `nv3dd32.dll` (an Nvidia RIVA 128
  driver name: a card-specific workaround).

## What this suggests for MW3

- PM's `CZMechFirePoint::CFixup` messages point at a class MW3 has inline;
  the "must be moved to the torso" rule may explain MW3 firepoint handling.
- Archive version 2 shows what the CRC in MW3's archive writer was for.
- PM drops `mechs.zrd`, `player.zrd` and `equipment.zrd`; where their data
  went (the exe? other files?) may say what MW3 read them for.
- Next, if worth it: a function-level diff of the two exes (Ghidra on PM,
  matching functions by bytes and call graph, as planned in CLAUDE.md), to
  see which MW3 functions PM changed: changed functions are often fixed
  bugs, which would confirm or question our FIX (bug) choices.

## Running the differential tests on PM's exe (2026-10-21)

`tests/differential/original.py` knows both exes (`EXES`: MW3 1.2 and PM
1.0, each by its SHA-256). MW3 is the default. `Original(exe='pm')` or
`MW3_EXE=pm` (or a path to either exe) runs PM's instead, and any other
file is refused. `calltrace.py` decodes a function's `ret` from the selected
exe when it is not MW3's, since `mech3.asm` is MW3's listing.
`twod_world.py` reads the selected exe's imports. The tests' addresses are
MW3's, so a test runs on PM only where the function sits at the same
address. The function-by-function diff that maps PM's addresses comes
later. `test_exe_select.py` checks the choice.

To check when the PM work starts (the lead, 2026-10-28): whether PM's
TextBase is live. In MW3 1.2 its vtable is kept by no object, so zFont's
bitmap fonts are loaded and never drawn (twod.md section 4); the same
check on PM's exe says whether its reader 0x00519e50 needs porting there.


## The function diff (2026-10-30)

PM's exe decompiled as MW3's was (no porting yet). Outputs, git-ignored in
`analysis-output/`, MW3's own left as they were:

- `pm-decomp/`: Ghidra 12.1.4's pseudocode of every PM function
  (`<addr>.c`, 11,344) and `functions.jsonl` (each function's ranges,
  callees, callers, strings, imports, constants). Made by
  `tools/ghidra/DumpFunctions.java` in the headless project
  `pm-ghidra-project` after Ghidra's analysis, with functions created at the
  data sections' code pointers as `vtscan.py` did for MW3 (1,012 in PM,
  957 in MW3). `pm-diff/mw3/functions.jsonl` is the same dump of MW3 1.2
  (no pseudocode), so both sides of the diff come from one analysis.
- How: `analyzeHeadless <main checkout>/analysis-output/pm-ghidra-project pm
  -import game/pm/Mech3.exe -scriptPath tools/ghidra -postScript
  DumpFunctions.java analysis-output/pm-decomp "Mech3.exe PM 1.0" decompile`
  (about 20 minutes, the MW3 dump without pseudocode 2; give the main checkout's paths, Ghidra refuses a
  `.claude` component), and the same with `mw3` and `pm-diff/mw3` for MW3.
- `pm.asm`: the objdump listing (`pm-diff/mw3/mech3-bytes.asm` is MW3's
  with instruction bytes, which `mech3.asm` leaves out).
- `pm-func2file.json`: each function's source file by link order, from the
  assert paths, as `modmap.py` made MW3's.
- `pm-diff/pairs.json` and `report.txt`: `tools/analysis/pmdiff.py`'s pairs
  and summary (about a minute).

**The compiler changed.** MW3 1.0 and 1.2 were linked by Visual C++ 5.0
(linker 5.10, compiler build 7303 in the Rich header); PM by Visual C++ 6.0
(linker 6.00, builds 8168 and 8447, VC6 SP3). PM was rebuilt from source
with the newer compiler, so nearly every function's instructions differ
(register choice, inlining, scheduling) even where its source did not
change, and the raw bytes match only where the code is simple. The matcher
therefore pairs on what the compiler does not change and measures a change
by it: the calls, strings, constants (immediates and `.rdata` values) and
imports a function adds or removes. VC6's different inlining still moves
calls (a function that loses calls and gains none, such as 0x5079b0's 41,
is most likely inlining), so strings and imports are the firmest evidence.

**Matching** (`pmdiff.py`), on the 8,030 MW3 and 8,567 PM functions left
after setting aside the compiler's exception-unwind funclets (2,864 and
2,777, Ghidra's `Unwind@...`):

1. unique masked bytes (the operands' image addresses and the calls' and
   jumps' displacements leaving the function masked; neither exe has a
   `.reloc`): 916 pairs;
2. a string found in one function on each side (assert paths by file
   name): 412;
3. repeated: each pair's calls aligned in order (1,642), the unpaired runs
   between pairs in link order aligned by score (3,709), mutual best within
   a source file or link-order range (99);
4. the rest of each group, mutual best at a lower threshold (97).

Of the pairs whose functions both hold strings, 7 of about 790 share none,
so the matching is right well over 99 times in 100 where it can be checked;
the loose stage's pairs are mostly two-to-four-instruction functions and
are the least certain.

**The counts:**

| | functions |
|---|---|
| paired | 6,875 (86% of MW3's) |
| identical (the same instructions once addresses are named by what they point at) | 1,488 |
| changed in calls, strings, constants or imports | 1,531 |
| changed in their instructions only (most likely the compiler) | 3,856 |
| new in PM (unpaired PM functions) | 1,692 |
| MW3 only (unpaired MW3 functions) | 1,155 |

"MW3 only" is an upper bound: it holds functions PM removed, small ones
VC6 inlined (`TurnLimits*::Within`, `inout_side`), and some whose rewrite
the matcher did not recognise (`FollowState::Run` and `Mech::Die` must
exist in PM). By module, paired with feature changes / new / unpaired:
Mech3's game code 876 / 1,074 / 766 (of 5,011 MW3 functions); GameZ
zClass 240 / 180 / 60, zModel 84 / 183 / 104, zTwoD 43 / 42 / 18, zSound
30 / 38 / 31, zVideo 36 / 40 / 13, zFMV 17 / 9 / 33 (PM has no movies),
zDecal 21 / 16 / 40; the rest of GameZ (input, network, geometry, reader,
render, weapons) changed little beyond the compiler.

**The biggest changes** (MW3 address, then PM's):

- 0x00591300 -> 0x005b2d10, the script interpreter's dispatcher
  (`zinterp_parse.cpp`): 97 calls and 23 strings added, PM's new script
  commands (`LightSetLightMap`, `NodeSetOverrideAmbient`, the clutter).
- 0x00497720 -> 0x0049a3c0, the multiplayer game types (`mpinfo.zrd`):
  64 calls added, the new types' names ("Deathmatch with Teams",
  "Attack/Defend - One Base" and "Two Bases").
- 0x004e44b0 -> 0x004f5f50 (`ZRdrValve.cpp`): the AI valves' new keys
  (`retreat_dist`, `retreat_health`, `retreat_time`, `scan_delay_min/max`,
  `scan_duration_min/max`, `approachRange`, `attack_radius`, `minRange`).
- 0x0048faf0 -> 0x00492830, the mission load (`mission_load_world`): 33
  calls and 20 strings added; 0x00490ab0 (`mission_setup`) and 0x00491150
  (`mission_launch`) changed with it.
- 0x00491de0 -> 0x00495250, the world parameters' reader (`mission.cpp`):
  new keys `ambient_color`, `diffuse_color`, `cloud_cover`, `ai_headlights`,
  `nearfar_clip`, `camera`, `death`, `depth`.
- 0x0046ba40 -> 0x0046d690, the mech definition reader (`mech_dfn_read`):
  14 calls and 9 strings added (PM's weapons, the firepoint fix-up).
- 0x00570af0 -> 0x00592170, the sound syntax reader: 21 calls and 11
  strings fewer, rewritten smaller; 0x00446ad0 `HudObjectives::Fill` lost
  18 calls and 6 imports the same way.
- 0x00580550 -> 0x005a1740, the DirectDraw/Direct3D error names: 58
  constants fewer (DirectX 7's table).
- The front end: `MultiplayerScreen1/2::Create`, `PilotScreen::FillGames`
  (PM's saved games, `%s%02d.mw3`), `ActionScreen`, the briefing dialog
  0x00405230 (`brief_%s%s`).
- The readers: `zcls_read_zbd` 0x004ff520 and `zcls_read_node` 0x004feaf0
  (gamez.zbd version 41), `zeff_anim_read_zbd` 0x0050a0e0 (anim.zbd
  version 50), `AivRecord::Parse` 0x004df730 and `valves_read` 0x004e2330.

**New in PM** (by link-order range; `report.txt` lists them all): the
AI's new behaviours beside `zainetmechinout.cpp` (177 functions, 12,000
instructions, "mcbdeploy"), the front end's new screens (load/save,
`BTN_ACCEPT`), the light maps and graphics flags (`LIGHT MAPS`,
`GfxFlags_HW`), the new AI movement modes ("Circle", "Direct", "Random
Move"), `cls_clutter.cpp` (32), `promote1.cpp` and the firepoint fix-up
(`CZMechFirePoint::CFixup`, 32), the saved mech configurations
(`%s\%s.m3c`, "Incomplete Configuration"), the spotlights and headlights,
the Nvidia RIVA 128 workaround (`nv3dd32.dll`, `texel_pixel_offset`), the
CD autorun switch (`QueryCancelAutoplay`, `NoDriveTypeAutoRun`), and the
renamed reader `zrdr_main.cpp` (its 122 new functions pair with the 61 of
MW3's `zreader.cpp` range that are left unpaired, a rewrite).

**MW3 only**, beyond the inlined and the unrecognised: the movie player
(`fmv_stream.cpp`, `FmvAvi`, `FmvMci`: PM has no movies), the salvage
range, MW3's text reader functions (`zrdr_parse_text`, the vector keys:
`zreader.cpp` was rewritten as `zrdr_main.cpp`), and the fog
(`zmod_fog_set_on`, `zmod_fog_apply`: PM's lighting replaced it).

TextBase (the open check above): in PM, too, TextBase's constructors
(0x005c3940, 0x005c39d0) are called only by FontText's constructor and
copy, as in MW3, so its bitmap-font drawing is most likely dead there as
well. (The fresh MW3 dump shows four callers of 0x005a3040, which twod.md
section 4 lists as having none: 0x005a1219, 0x005a12b9, 0x005a1354,
0x005a2f7d.)

## PM's data against MW3 1.2's (2026-10-30)

`tools/analysis/pmdata.py` compares `game/pm/zbd` with `game/mw3-1.2/zbd`
file by file (and a zArchive entry by entry): 91 files in PM, 122 in MW3;
none the same, 71 in both by name, 20 new, 51 only in MW3. PM's `c1`..`c4`
are its own worlds under MW3's folder names, so "changed" there means a
different world. PM adds `c1/readerm5`, `c2/readerm8`, `readeria4` and the
standalone missions `readersa1..4` per world; MW3's other mission readers
are gone. `reader.zbd`: 101 entries the same, 71 changed, 26 new, 6 gone;
`mechlib.zbd` 60 -> 79 entries; `motion.zbd` 258 -> 364;
`soundsh/l.zbd` 931 -> 768 sounds (339 new, 502 gone).

What the port's readers make of PM's files (`mw3 --install game/pm`, and
with a local build that also took archive version 2):

| Format | MW3 1.2 | PM 1.0 | The port |
|---|---|---|---|
| zArchive (`reader*.zbd`, `mechlib`, `motion`, `sounds*`) | version 1 | version 2 (a CRC after the count) | rejected (`zutl_zar` read_table takes 1 only): "Cannot read required data" at the start |
| `gamez.zbd` | version 27 | version 41 (and a time stamp, 1999-11-12) | rejected (`cls_zbd` 0x004ff520: "incompatible file version") |
| `anim.zbd` | version 39 | version 50 (and a time stamp) | rejected (`zeff_anim_init` 0x0050a0e0's check) |
| `mechlib.zbd`'s `version` | 27 | 41 | rejected (`cls_mechlib`'s check) |
| `interp.zbd` | 0x08971119, version 7 | the same | read |
| texture libraries (`rimage`, `rmechtex*`, `rtexture*`, `texture*`) | type 1 | type 1 | read (the world's `texture.zbd` has a new third field, 0x19) |
| `motion.zbd`'s motions | format 4 | format 4 | read |
| the mech definitions (`dfn_*.zrd`) | | new weapons | "Unrecognized Weapon" (`XPulseLaser_Large`, `HeavyLaser_Large`, `TBOLT5`), and the shadowcat and thor not built |

A side effect seen: `mw3 --fly` on a world that fails to load retries the
load every frame (ours, the debug start), so it never quits.

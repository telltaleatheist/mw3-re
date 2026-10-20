# Pirate's Moon: a first look

Not a port. Pirate's Moon (PM, the expansion) is read side by side with
MW3 1.2 for what it tells us about MW3: the same studio, engine and code
base, built eight weeks later. Started 2026-10-20 (Owen: "see if it has any
hidden files that could give us some extra insight ... discrepancies between
mw3 and pm, which might connect dots").

## The copy

- Source: `/Volumes/iO/backups/win98/MW3 pirates moon.zip`, a Redump-style
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

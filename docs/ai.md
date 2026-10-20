# The AI

Mech3.exe 1.2, 2026-10-20. The map of the AI code in the game half
(0x4b4880..0x4eada0), the data files it reads, how one AI tick runs, and a
porting order. The data side, the nets at run time, the AI controls
with the brain's data and the valves at run time are ported and
emulator-checked (sections 5, 8, 12, 13); the brain's decisions, the
behaviours and targeting are not.

Names are ours unless an assert string gives them (`CZAiNetEdit`,
`CFileDialogNet`, the five source paths). "Likely" marks a reading of what the
code does that no string confirms.

## 1. What is where

| Range | Funcs | File | What |
|---|---:|---|---|
| 0x4b4880-0x4b5580 | ~45 | weapon.cpp (tail) | weapon helpers the AI uses: the per-type fire timers 0x4b4bd0..0x4b4c90 (WeaponType +0xc4), the radar list 0x4b49c0; weapons.md |
| 0x4b5580-0x4b7d80 | ~80 | (unnamed, after weapon.cpp) | **the AI's weapon firing**: the per-group fire state machine 0x4b5b80 (states 1..7, game time 0x60991c, called from the brain 0x4e9fb0), its fire-plan objects (vtables 0x5c9458, 0x5c9460: 0x4b6390, 0x4b6790), aim and lead helpers 0x4b70b0..0x4b77e0 |
| 0x4b7d80-0x4bcb70 | ~95 | (unnamed) | **the AI tester dialog**, a developer tool: an MFC CDialog (0x4b80f0, 0x988 bytes, resource 0xa5, sub-dialogs 0xa6..0xb7) opened by 0x4b86e0 from the debug menu (0x404090, 0x40db60). Strings "Direct / Zig Zag / Circle / Jump / Random Move" (movement modes), "player / nearest enemy / ionet" (targets). Not to be ported (FIX (A)) |
| 0x4bcba0-0x4bf1b0 | 34 | ZAiNetEdit.cpp | `CZAiNetEdit` (0x4bcba0 ctor, 0x44 bytes, vtables 0x5ca420/0x5ca388): **a net at run time**, built from its file (section 3): one node object (0x4c6d30, 0xbc) per record; edits, save (0x4bef80 -> 0x4dc2b0), regions (0x4bf000, 0x4bf1b0 "unable to create region") |
| 0x4bf3f0-0x4c1160 | 60 | (unnamed) | `CFileDialogNet` (0x4bf750, the editor's file box, FIX (A)); net classes over the net base 0x4d5120 (vtable 0x5ca6d8: 0x4bf910, 0x4bfbb0, 0x4bfe40; 0x4c0090, 0x20 bytes, five behaviour vtables at once); small move classes (0x5ca738, 0x5ca748, 0x5cab50..0x5cac40) |
| 0x4c11f0-0x4c3ed0 | 67 | ZAiNetMechInOut.cpp | **inside/outside nets**: closed node loops (the reg_*.zrd regions) and which vehicles are inside them ("zero length segment in an inside/outside net" 0x4c11f0); the mission's set-up 0x4c3b80 (from the mission start 0x490ab0); "Unknown class type %d for node %s" 0x4c3ed0 |
| 0x4c3f70-0x4d2660 | 385 | (unnamed) | the bulk of the AI: the net record class (0x4c5fb0 family) and run-time node (0x4c6d30); the region registry (0x4c5c00 find-or-load by name); net classes 0x4c5640, 0x4c5b00; **the behaviour (order) classes** (section 2); the pilot brain object 0x4cf330 (0x2b0 bytes + IControls); "mcbdeploy" 0x4cfad0 (multiplayer deploy); the health_below list 0x4d1f60/0x4d1f80 |
| 0x4d26f0 | 1 | ZAiTarget.cpp | the target check (asserts on a target's collide and weapons callbacks) |
| 0x4d27d0-0x4da8f0 | 229 | (unnamed) | **targets and targeting**: `CZAiTarget` (0x4d2960, 0xf4; 0x4d3080 derived), made at mission start from targets.zrd (0x4d2e20, 0x4d31d0); **the valves** (0x4d3b20..0x4d3e50, registry 0x60b500); `AiTarget`, the vehicle's targetable base (0x4d56e0, vtable 0x5cced8, 0xd4: height +0x68, team masks +0xc0..+0xc8) and 0x4d7f80 (0xd4); the contact pick 0x4d6b90; the sides 0x4d5af0 / 0x4d5b10; the AI controls (0x4d8670, 0x4d8750 = 0x320 bytes); the AI settings block AiParams (0x4d8d30..0x4d9b70); region lookups for the map (0x4d92f0 reg_inside_, 0x4d9360 reg_outside_); the net file cache and its STL (0x4da060..0x4da840) |
| 0x4da900-0x4db8d0 | 11 | ZRdrAiNet.cpp | **net and region files**: header 0x4da900, node records 0x4db850..0x4db1b0, the editor's writers 0x4dad10 / 0x4db8d0 |
| 0x4dc150-0x4de980 | 45 | (unnamed) | the net file classes' methods (record lookup 0x4de730, 0x4de830) and their std::map code |
| 0x4de9c0-0x4e08c0 | 43 | ZRdrAiv.cpp | the reader-file base; **aiv.zrd** (0x4df730) and **targets.zrd** (0x4e08c0); traps.zrd (none ship) |
| 0x4e0c20-0x4e2330 | 25 | (unnamed) | the aiv record cache's std::map (editor only); **valves.zrd** file (0x4e1cb0) and its load at mission start (0x4e2120 from 0x490ab0) |
| 0x4e2330-0x4e44b0 | ~55 | ZRdrValve.cpp (+0x4e2330) | the valve file's reader 0x4e2330 (sound, sound_once, animation, scan_*, canleave, shutdown, mission complete/abort/reset, destroy, setteam, attack_strategy, update, delayupdate, any/all zero/nonzero, namelist, teleport) and the action classes (0x4e2ff0..0x4e4430, vtables 0x5cd458..0x5cd598) |
| 0x4e4d20-0x4e7090 | ~70 | (unnamed) | valve conditions (0x4e4d20.. all_nonzero etc.), valve file class 0x4e5290; a mech hook from the Mech ctor (0x4e61e0) and Mech::Update (0x4e6480); subtitles ("MINDELAY SUBTITLE" 0x4e6aa0) |
| 0x4e7090-0x4eaab0 | ~90 | (unnamed) | **the brain**: vehicle AI controls 0x4e7090/0x4e71c0; lancemate radio 0x4e7c40 ("call_help", "confirm_order", "engaging", "low_ammo", ... built as `<tag>_<event>` 0x4e7dc0); orders from the player 0x4ea000..0x4ea230; the AI update 0x4e9710 |

The zaitarget/zrdrainet unnamed range is big because several small files sit
in it; where one ends is not known.

## 2. Classes

Sizes from `operator new` at the call sites; vtable slot counts are where the
next vtable begins, so they may run long.

| Class (ours) | Ctor | Vtable | Size | Notes |
|---|---|---|---|---|
| RdrFile (CZRdrFile) | 0x4de9c0 | 0x5cd418 | 0x24 | slots: dtor, +4 returns 1, +8 unload 0x4deb80, +0xc parse (empty) |
| net file chain | 0x4da740, inline in 0x4da570 | 0x5cd400, 0x5cd3e0, 0x5cd3c0, 0x5cd3a0 | 0x118 | +0x2c the shared record, +0xe4 the editor's cache, +0xf4 the header; slots +0x14 write node, +0x18 read node, +0x1c write header |
| AivFile | 0x4df640 | 0x5cd428 | 0x28 | one, at 0x60b5e0 |
| RecordFile (targets, traps) | 0x4e0740 | 0x5cd440 | 0x24 | 0x60b608, 0x60b630 |
| valves file | 0x4e5290 | 0x5cd5a8/0x5cd5c8 | | valves.zrd |
| NetNodeRec | 0x4c5fb0 / 0x4c6ca0 | 0x5cae50 / 0x5cae54 | 0xb4 | defaults 0x5e8af8 |
| net node (run time) | 0x4c6d30, 0x4c6da0 | 0x5cae60, 0x5cae58, 0x5caef8 | 0xbc | a record plus +0xb4.. |
| net base | 0x4d5120 | 0x5ccda8 | 0x5c | over a network object (0x58aec0, vtable 0x5cefd0): nets are synced in multiplayer (likely) |
| CZAiNetEdit | 0x4bcba0 | 0x5ca420 (+0x5ca388) | 0x44 | |
| other nets | 0x4bf910 (0x1c), 0x4c5640 (0x1c), 0x4c5b00 | 0x5ca6d8, 0x5cac90, 0x5cae00 | | over 0x4d5120 |
| behaviour base | 0x4c8600 | 0x5cb180 (40 slots) | 0x0c | +4 bit 0, +8 0 |
| behaviours, 40-43 slots | 0x4c8750, 0x4ca370, 0x4caca0, 0x4cad70, 0x4cbe10, 0x4d08d0, 0x4d1c80 | 0x5cb278, 0x5cb7e8, 0x5cb948, 0x5cba20, 0x5cbe18, 0x5cc970, 0x5ccaa8 | | the orders a pilot runs (likely: move along a net, attack, follow, ...) |
| behaviour step base | 0x4c9d00 | 0x5cb3d0 (19) | | over 0x4c8600, +0xc = 0x603fa0 |
| behaviour steps | 0x4c9eb0, 0x4c9f80, 0x4ca0a0, 0x4ca1c0, 0x4cb2a0, 0x4cb950 (+0x4cbcf0, 0x4cbd40), 0x4cce30, 0x4cd4a0, 0x4cd6e0, 0x4cd770, 0x4cda50, 0x4cdc20 | 0x5cb488 ... 0x5cc440 | 0x10-0x20 | |
| behaviour factories | 0x4c8660 base; one per behaviour (0x4c88c0, 0x4c9ef0, 0x4c9fc0, ...) | 0x5cb238 (2 slots) + 2-slot / 7-slot vtables | | +4 the argument |
| AiParams | 0x4d98c0 | 0x5cd348 | 0xdc | section 4 |
| AivRecord | 0x4e0490 | 0x5cd438 | 0xe4 | holds a reference on aiv.zrd |
| pilot brain | 0x4cf330 | 0x5cc698 (+ IControls 0x5bd500 at +0x2b0) | 0x2b0 + | starts with an AiParams copy (0x4d9bf0, vtable 0x5cd360) |
| AI mech controls | 0x4d8670 / 0x4d8750 | 0x5cd1e0 / 0x5cd318 | 0x320 | MechControls + the brain at +0x5c (controls.md) |
| AI vehicle controls | 0x4e7090 / 0x4e71c0 | 0x5cd738 / 0x5cd880 | | the brain at +0xa04 |
| AiTarget | 0x4d56e0 | 0x5cced8 | 0xd4 | base of Vehicle (structs.md) |
| CZAiTarget | 0x4d2960 / 0x4d3080 | 0x5ccb90 / 0x5ccc80 | 0xf4 | a targets.zrd target |
| Valve | 0x4d3b20 | - | 0x0c | name, value holder (vtable 0x5cd558), reactions |
| valve actions | 0x4e2ff0..0x4e3f50 | 0x5cd458..0x5cd528 | 8-0x1c | one class per valves.zrd action |
| valve conditions | 0x4e4d20, 0x4e4df0, 0x4e4ec0, 0x4e4f90 | 0x5cd568..0x5cd598 | 0x14 | all/any zero/nonzero over 0x4e40e0 |
| AI tester dialog | 0x4b80f0 | 0x5c97b8 | 0x988 | MFC, developer tool |

## 3. Data files

Each mission's `zbd\<world>\reader<mission>.zbd` (opened with `reader.zbd` by
0x490630; the search path also names `..\data\<world>\zrdr\aipath` for loose
files, which do not ship) holds, across the 48 archives:

| File | Count | Read by | What |
|---|---:|---|---|
| net_NN.zrd | 308 | ZRdrAiNet (version 106) | AI paths: numbered nodes with links |
| reg_*.zrd | 393 | the same reader (version 108; 3 are 106) | regions: node loops; reg_inside_* / reg_outside_* are the mission's boundary on the map (0x43d920) |
| aiv.zrd | 48 | ZRdrAiv 0x4df730 | the vehicles: one record per entity name |
| valves.zrd | 46 | ZRdrValve 0x4e2330 | the mission's triggers |
| targets.zrd | 27 | 0x4e08c0 | scored objects |
| waynet*, mpinfo.zrd | 2 each | mech_waynet.cpp | multiplayer |

mech3ax does not read these files (its `reader` crate reads any .zrd tree,
nothing AI-specific); the exe is the only source.

### Net and region files

```
( ( "version" (106 ["mech3"])  "name" ("net_01")  ["netparams" ("pos" (x y z))]
    "node_00" (12 (x y z) (links...) props...)        a "12" node (kind 0)
    "node_01" ((links...) props...)                   a list-first node (kind 1)
    ... ) )
```

The header (0x4da900): version 105-108 accepted (105 with "mech3" reads as
106), others reported; 105 files must be named net_NN after their number; a
name "mcbwaynet" is refused. Records start at the first `node_` key. Links
are the list's non-negative values. Node properties (0x4db1b0, each a list
after its key, values copied as raw 32-bit words): speed (on rate), canleave,
wait_interval, wait_time, valve (on name [value]), name (localized) and
auxname (defaults to the name as written), jump, scan_aim, scan_time,
scan_hit, hidden, fixed, autonext, visible, deploy (valve byte valve byte
short), mcbwait (valve byte time), arrangenodes, sound, backup, gate, shunt,
valveunion ((on name [count]) ...), valve_assign (name value). The shipped
files use speed, canleave, wait_interval, wait_time, valve, name, auxname,
jump, scan_*, deploy, arrangenodes, gate, valveunion, valve_assign.

Records are parsed on demand into one shared NetNodeRec (0x4de730). A file
object per name lives for the game in the map at 0x60b5a8 (0x4da570, named
net_NN when asked by number); its tree is loaded on the first reference and
freed on the last.

### aiv.zrd

```
"name" (kind (x y z) yaw [team] tag value ...)
```

`team` is given or comes from the name (0x4d5b10: player 1, stealth 0,
others 2). Tags (0x4df730, all lists of raw words): params (9), p2 (8), p6
(4), p8 (17), p9 (5), p10 (2), p11 (1), p12 (mech name, 2 flags), p13 (1),
pilot (localized name, can_configure), sound_tag, skin, lancemate
(0/1/2 or on/off/none/default), vlist (orders), mech3. The older p3, p4
(an inline order), p5 and p7 are read too; no shipped file has them, and p8
with any of them is reported. Where each lands in AiParams:

| Offset | From | Notes |
|---|---|---|
| +0x04 | kind | the record's first item (0-26 in the shipped files) |
| +0x08..+0x10 | (x y z) | |
| +0x14 | yaw | degrees in the file, radians after finish |
| +0x18 | team | |
| +0x1c, +0x3c, +0x20..+0x38 | params 1, 2, 3..9 | +0x1c (params 1) is compared squared with a contact's distance: sight range (likely); +0x20 (params 3) the attack range (likely); +0x1c, +0x20, +0x24, +0x74 are kept squared |
| +0x40, +0xcc bits 0-2, +0x44..+0x50 | p2 | +0x4c x the GameIntensity value 0x608e54 sets a delay (0x4e9710) |
| +0x54..+0x60 | p6 | |
| +0x64..+0xa4 | p8 1-4, 6-8 at +0x74..+0x7c, 5 at +0x80, 9 at +0x84, 10-17 at +0x88..+0xa4 | +0x64/+0x68: a random interval's bounds (0x4e9710) |
| +0xa8..+0xb0, +0xcc bit 3, +0xb4 | p9 | |
| +0xb8, +0xbc / +0xc0 | p10 / p11 | |
| +0xc4, +0xcc bits 4-5 | p12 | the mech index (0x4845f0), -1 for none |
| +0xcc bit 6 / bit 7 | p13 / can_configure | |
| +0xc8 | skin | |
| +0xcd bits 0-1 | lancemate | |
| +0xd0 | vlist | a list of 0x20-byte orders: three floats, two squared ranges, a region by name (0x4c5c00), flags from the 0/1 items, a valve |
| +0xd4 / +0xd8 | pilot / sound_tag | sound_tag defaults to dom / alan / epona for exactly "Dominic_Paine", "Alan_Mattila", "Epona_Rhi" |

The defaults (0x5e8dc0) are stored squared; the reader takes them back to
plain units first (0x4d9b20) and squares again at the end (0x4d9b70).

### targets.zrd

`"name" (kind "health" (h) "target" ("node" [(offset)]) "centroid" (...)
"destroy_anim" ("anim") "name" ("MSG_...") "radar" / "display" /
"reticule_select" / "zoom_select" (bool) "height" (h) "health_below" ("valve"
n h ...) "translate_to_anim_root" ())`. health_below sets a valve when the
target's health falls below each value (likely: the mission scripts' "hit"
and "down" events).

## 4. One AI tick

The AI vehicle's controls are its IControls; Mech::Update calls its Update
(slot +0x0c). For an AI mech that is 0x4e96f0, which runs the brain at
controls +0x5c:

```
0x4e9710 brain update(dt):
  AI off (0x60b938 == 0): stop the controls (+0x14), run the order (vt +4), done
  powered = vehicle vt +0xa0
  0x4cec80: smooth two skill values, then the contacts (0x4ceb50)
  powered down and not +0x2a4: stop, run the order, done
  pick a target: the nearest contact of a side (0x4d6b90: 0 ally, 1 enemy, 2 neither, 3 any;
    the contacts' AiTarget +0xc0 against our masks +0xc4 / +0xc8), within the sight range (+0x1c)
  engaged:
    weapon.cpp's 0x4b4ed0 under 0.25 (+0x2a8 set): 0x4e6d50(1.0), not traced
    0x4e9550: weapons and targets; 0x4e9070: a clear line (likely); the fire timer +0x100
    the attack range (+0x20) and the delay +0x4c x GameIntensity
    a random wait between +0x64 and +0x68
    run the current order (+0x17c, vt +4) until it stops asking to run again (+0x104)
  the order's own update (vt +4); not powered down: the firing state machine (0x4e9fb0 -> 0x4b5b80)
```

Mission start (0x490ab0) builds what the AI uses, in this order: the AI nets
the pilots ask for (CZAiNetEdit through the cache), the inside/outside nets
(0x4c3b80), the vehicles from aiv.zrd by name (0x4e0680 from the vehicle and
mech builders 0x46da80, 0x49e810, 0x4a4de0), the targets (0x4d2e20), the
valves (0x4e2120).

## 5. Ported

All load-time code, checked in Unicorn against the original on every shipped
file (`tests/differential/test_ai_readers.py`, 1211 cases: 698 net and region
files by number and by name, 48 aiv.zrd, 27 targets.zrd, 438 made-up files
reaching 80 marked places in the original). Output streams (records with
valves as registry places, regions as their place in the region list,
strings as text) and the logs of zerr, localize and zSound calls match byte
for byte, at 53-bit and at 24-bit x87. The orders' regions are loaded by the
ported lookup (section 8) on both sides.

| Piece | Original | Port |
|---|---|---|
| reader-file base | 0x4de9c0, 0x4deb20, 0x4deb50, 0x4deba0, 0x4dec80, 0x4deb80, 0x4debd0, 0x4decf0, 0x4ded20, 0x4ded60 | `mech3/zrdraiv` RdrFile |
| net files | 0x4da570, 0x4da8e0, 0x4da840, 0x4da060, 0x4da0d0, 0x4da900, 0x4dac70, 0x4db850, 0x4dae40, 0x4db140, 0x4daee0, 0x4db1b0, 0x4de730, 0x4dc690, 0x4de830, 0x4de010, 0x4de090, 0x4d5280; header 0x4bcb50, 0x4bca20, 0x4bca40, 0x4bca90 | `mech3/zrdrainet` |
| net record | 0x4c5fb0, 0x4c6050, 0x4c63a0, 0x4c62d0, 0x4c6bd0, 0x4d44b0, 0x4d45c0 | `mech3/zainet_node` |
| valves registry | 0x4d3b20, 0x4d3b80, 0x4d3c40, 0x4d3e50, 0x4e4460 | `mech3/zai_valve` |
| valves at run time (section 13) | 0x4d3c00, 0x4d3bf0, 0x4d3d20, 0x4d3d30, 0x4d3ef0, 0x4d3fa0, 0x4d3fd0, 0x4d4080, 0x4d41a0, 0x4d41e0, 0x4d4310, 0x4d4320, 0x4d4480; valves.zrd 0x4e1cb0, 0x4e5290, 0x4e5e30, 0x4e2190, 0x4e1d70, 0x4e2330, 0x4e2120, 0x4e1d00, 0x4e20d0, 0x4e1f10, 0x4e2fb0, 0x4e44b0; the actions 0x4e2ff0, 0x4e3040, 0x4e3070, 0x4e30b0, 0x4e30f0, 0x4e3120, 0x4e3150, 0x4e3180, 0x4e3230, 0x4e31c0, 0x4e3200, 0x4e3350, 0x4e3310, 0x4e3330, 0x4e3500, 0x4e33c0, 0x4e34b0, 0x4e36a0, 0x4e3560, 0x4e3650, 0x4e3870, 0x4e3700, 0x4e37f0, 0x4e3a30, 0x4e38d0, 0x4e39c0, 0x4e3cc0, 0x4e3a90, 0x4e3ba0, 0x4e3da0, 0x4e3d20, 0x4e3e00, 0x4e3d70, 0x4e3d90, 0x4e4020, 0x4e3f50, 0x4e3fb0, 0x4e3e60, 0x4e3f10; the conditions 0x4e40e0, 0x4e4d20, 0x4e4df0, 0x4e4ec0, 0x4e4f90, 0x4e4360, 0x4e43b0, 0x4e4400, 0x4e4430, 0x4e4080, 0x4e5080; the brain's switches 0x4d9c20..0x4d9ca0 | `mech3/zai_valve`, `mech3/zrdrvalve` |
| AiParams | 0x4d98c0, 0x4d99e0, 0x4d9a50, 0x4d9ac0, 0x4d9b20, 0x4d9b70, 0x4d8d30, 0x4d8e20, 0x4d8e40, 0x4d5af0, 0x4d5b10 | `mech3/zai_params` |
| aiv.zrd | 0x4df620, 0x4ded70, 0x4df5f0, 0x4def10, 0x4e0490, 0x4e04f0, 0x4e0600, 0x4e0680, 0x4df730 | `mech3/zrdraiv` |
| targets / traps | 0x4e0720, 0x4e0820, 0x4e0740, 0x4e07d0, 0x4e07e0, 0x4e08c0, 0x4e0890 | `mech3/zrdraiv` |
| reader getters | 0x556fe0, 0x557030, 0x556e90 | `gamez/zreader` |
| mech by name | 0x4845f0 | `mech3/mech_names` mech_index |
| AI controls and the brain's data (section 12) | 0x49fe90, 0x49ff10 (player.cpp), 0x4d8750, 0x4d8670, 0x4265a0, 0x4e71c0, 0x4e7090, 0x4ac750, 0x4ac740, 0x4a4af0, 0x4a4a70, 0x4a4000, 0x4a3fc0, 0x4cf330, 0x4d9bf0, 0x4d9940, 0x4d8fe0, 0x4b6570, 0x4b6390, 0x4b7060, 0x4b6660, 0x4e82a0, 0x4e8180, 0x4e8070, 0x4e8030, 0x4e7f80, 0x4ce200, 0x4e8090, 0x4cd770, 0x4c9d00, 0x4ce620, 0x4e9380, 0x4e9d80, 0x4e9d90, 0x4c0070, 0x4c0010, 0x4c0520, 0x4c0540, 0x4bffe0, 0x4cbe30, 0x4cbe50, 0x4c8660, 0x4e7e20, 0x4e7c40, 0x4e7dc0; the destructors 0x4d8bf0, 0x4d8c00, 0x4d8c40, 0x4e7820, 0x4e7830, 0x4e7870, 0x4cf630, 0x4e7df0, 0x4e7ff0, 0x4b65f0 | `mech3/mech_control` AiControls, `mech3/zai_brain` |

Hooks (`RdrHooks`): the loose-file search (0x556240; none, FIX (A)),
localize, zSound's lookup. The region lookup 0x4c5c00 is the port's
(`region_find`, section 8).
Not ported on purpose: the editor's writers (0x4dad10, 0x4db8d0, 0x4dc2b0),
the record caches the editor fills (left empty), the AI tester dialog.

Quirks kept: values read as raw words whatever their type; `find` searching
nested lists (a record's key could match a string inside another record; no
shipped file does that); auxname lost when a record is assigned (0x4c63a0,
editor only); the p3 checks swapped as written. FIX (bug) guards where the
exe would crash on malformed data (a non-list where a list belongs, a
non-string where a name belongs, key+5 past a short key); FIX (bug) zeros for
the old tags' uninitialised stack (the test pins the exe's stack the same
way). A string or list where a number belongs gives the exe its pointer bits;
the port 0 (FIX (A)); the tests avoid it.

Breaks: section 7.

## 6. Porting order

Clean boundaries for later work, each a piece with its own test:

1. **Regions and the inside test**: done (section 8), the lookup 0x4c5c00,
   the builder's run-time side it needs, the edges and 0x4c57d0.
   `ZAiNetMechInOut.cpp` (0x4c11f0..0x4c3ed0) is not this: it is the
   mission boundary from the world's `inout` polygons (render.md 6a), read
   at mission load (0x4c3b80 -> 0x4c3930) and asked by the behaviours
   (0x4c3000, seven callers) to keep a destination inside; not ported
   (section 11).
2. **The run-time net**: done (section 9), the builder and the compiled
   net of steps. The mission start's call (0x49e810, player.cpp) is ported
   (mission-start.md). Left: the ground snap and the multiplayer waynet (NetHooks), the built-in nets
   (0x604008, the seven-behaviour net 0x4c0090) and the 105/107 versions.
3. **Net queries**: done (section 10), the nearest step and the way
   finder. What the steps do when a pilot reaches them is step 6.
4. **Valves** at run time: done (section 13), the reader, the conditions,
   every action, the timers. Left: the missions' update that ticks the
   timers (0x48f470) and the cleanup that ends them (0x491780), both in
   mission.cpp.
5. **Targeting**: AiTarget (0x4d56e0) masks and height, the contact list
   (0x4ceb50, +0xac vector), the pick 0x4d6b90, CZAiTarget 0x4d2960 from the
   ported TargetRec. Meets the vehicles through their AiTarget fields only.
6. **The behaviours** (0x4c8600 family, ~40 classes with factories): one
   class per sitting, each against the net queries and a vehicle hooks struct.
7. **The brain** 0x4e9710 and the AI controls' update 0x4e96f0, then the
   firing state machine 0x4b5b80 (meets weapon.cpp through the manager, which
   another agent owns), the lancemate radio's lines and orders 0x4ea000...
   The controls' constructors and the brain's data are done (section 12).

## 7. Tests and breaks

`tests/differential/test_ai_readers.py` (probe `tests/probe_ai.cpp`, emulator
side `tests/differential/ai_world.py`). Breaking the port one way at a time showed the test catches: a valve value
read from a 3-item list, negative links kept, the auxname taken from the
localized name, no "mech3" upgrade, the first node one slot late, the
canleave default, one record too many, valves matched in any case, the
valveunion count default, list-first nodes without properties, the reserved
name accepted, the last release one reference late, a deploy byte from the
wrong item, netparams keeping an old has_pos; params item 2 out of order, the
team from the name inverted, the yaw left in degrees, ranges not squared,
Alan's name misspelt, the lancemate bits swapped, p8 item 5 out of order, the
p3-alone copy, p5's jump case, can_configure off by default, mech names
matched by case, an order's region read from item 9, its item 1 dropped; the
targets' reticule default, radar setting the display bit, health_below read
in twos, an int height not converted, a target offset needing 3 items: 32 of
32. Three tried changes made no difference and were replaced: an order's item
17 read without its length check (past the end the port reads the zeroed
spare slot, the same as the default), the targets' header default (the reader
resets the flags itself), and radar on any non-zero value (get_bool_int only
gives 0 or 1).

`tests/differential/test_ai_regions.py` (probe `tests/probe_ai_net.cpp`,
`RegionsRun` in `ai_world.py`): every reg_*.zrd of every mission looked up
twice, every net file and a missing name as regions, then 93 made-up files
(versions, names, loops broken every way, holes before the start,
duplicates, a side of no length, arrangenodes round a loop); the lookups at
53 bits, then a few dozen points per region tested in turn at 24 bits. Streams
(each region's id, name, edges and where its test stopped; the registry's
ids and kinds; the region list) and zerr logs match: 140 of 140. Breaks
caught: the test's start kept after a pass, the plane counting as outside,
the normal flipped, the test always starting at the first edge, a side of
no length's normal, the start's budget not shrinking, the walk starting at
slot 0, names matched in any case, region ids from 10001, arrangenodes not
rounding to a tenth, its angle not snapped, a duplicate record replacing
the first, edges reversed, regions put at the front of the list, a loop
needing 3 nodes: 15 of 15. Three tried breaks were replaced: the dot
product's order (the normal's y is 0 but for sides of no length, whose x and
z are 0, so no order rounds differently), negative links followed (the
reader never keeps a negative link) and the walk following the last link
(the edges then follow another way and the port runs out of memory: caught,
but no use as a break).

`tests/differential/test_ai_nets.py` (`CompiledRun`): 0x402240 for 1..99
in every mission's archive (308 net files), then 137 made-up nets (every
property alone and on edge nodes, special nodes, duplicates, links to
missing and unpositioned nodes, nets built twice, ids out of range, 80
random); the registry and every compiled net's steps (kind, data, links as
slots) match: 174 of 174, and in 29 made-up nets the original crashes
where the port reads (section 9; the emulator pins the bytes just past a
heap block to 0xff, the -1 the port's FIX takes). Breaks caught: the
default valve value on the first link, speed after the waits, edge ends
counted per link, an edge node on its first end only, backup before
mcbwait, plain nodes unnamed, missing links kept, the null step kept by
attach, the deploy target by number, the canleave tests swapped, the scan
aim inverted, edge nodes not counted, the net under the builder's id,
built nets rebuilt: 14 of 14.

`tests/differential/test_ai_paths.py` (`PathsRun`, with a pilot made in the
emulator's memory: brain, vehicle, node name and position, mech and jump
jets): every mission's nets built, then the nearest step to 6 points round
each net's nodes, and in each net of at most 30 nodes 6 ways between random
nodes for pilots near the start, with and without jets, named for a gate or
not, under random valve settings, each way followed up to 7 steps (24
bits); plus a ladder net (a jump, a gate, a valve, a valve union) and a
ring net, 12 rounds each, and a fork whose short way a valve shuts once it
is found (so the finder must search again): 3912 queries, 64 of 64 cases
identical. Ways are not asked in the bigger nets: the walk is exhaustive
up to the best length found, which in them runs past four billion emulated
instructions (and the emulator, which never frees memory, runs out of it);
natively, ways sampled in the biggest shipped nets (246 nodes) take
milliseconds. Breaks caught: the nearest step the last of equals, the
pilot counted at the target within 20, an equal way replacing the best,
only the last leg measured, visited marks kept, the step reached dropped
with the way before it, no new search when the way shuts, valves open on
the other values, jumps for all, gates open to all, unions never open: 11
of 11. Two tried breaks change nothing a caller sees and were dropped:
keeping branches as long as the best (the cut only saves time: a way no
shorter is never kept), and not testing the target when checking a kept
way (targets are node steps, which let every pilot by).

## 8. The net objects

Ported 2026-10-20 (`gamez/znetwork/znet_object`, `mech3/zainet`,
`mech3/zainetedit`, `mech3/zainet_region`); test in section 7.

**Numbered objects.** Every AI net, region and waynet is a numbered object
(0x58aec0, 0x18 bytes: vtable 0x5cefd0, the id at +4, a vector of elements
at +0xc). While it lives it sits in one list (0x603fd8, made by 0x4c7b80),
where 0x58b160 finds the first of an id. 0x58b1c0 hands out a free id in a
range, from the one asked for upwards and wrapping to the range's start
(but a search that started above the start stops when it wraps, so a full
top end gives -1 even with free ids below it; an id asked for outside the
range is taken as it is when free). At mission end 0x58b290 deletes every
object under 10000 (0x5f2a48); the regions (10000 up) are freed by their
own list. The ids likely let a multiplayer session name nets across
machines (the code sits after `znet_dplay.cpp`).

**The builder** (`CZAiNetEdit`, 0x4bcba0, 0x44 bytes). In play it is only
ever made for a moment, with its `build` flag set (0x402240, 0x4797c0,
0x4c5c00); the editing side (flag clear, 0x4bd360 / 0x4bd3d0 from the
debug menu) writes edits back through the record cache and is not ported.
Made for net `n`: its own id comes from 400..499; a net already registered
under `n` is taken as it is (nothing is rebuilt; the builder's file is read
under that net's name); else the file (`net_%02d` of `n` or the name given)
is read into one node (0x4c6d30, 0xbc bytes: the record copied, the
builder at +0xb8) per record, at the slot of the record's number (the
vector grows to the highest number; slots without a record hold a
placeholder node, 0x4c6da0, whose setters do nothing; a second record with
a number is reported, "net %d: ignoring duplicate node %d", and dropped).
Then the link pass (0x4d53f0 -> 0x410590 / 0x4c7170 / 0x4c6fc0: each link
re-stored as the index of the node it names, which changes nothing for the
builder's own nodes) and `arrangenodes` (0x4c74a0, below). With `build`
and at least one record, the file's version picks what is built: 106 a
compiled net (0x4be670, section 9), 108 a region (0x4bf3f0); 105
(0x4bed90) and 107 (0x4be690, the multiplayer waynets) are not ported (no
single-player file has them). The builder's destructor leaves what it
built alone.

**arrangenodes ( r )** (0x4c74a0 -> 0x4c7300 / 0x4c7340): each node this
one links to is moved onto the circle of radius `r` round it, at the
nearest of `links` even angles (from +z, through atan2(dx, dz)), keeping
its height, each coordinate rounded to a tenth. Nodes are arranged in
slot order, so a node moved by one arrangement can be the centre of a later
one, and a node linking to itself moves its own centre mid-way. In the
shipped nets only `net_` files use it.

**Regions.** 0x4c5c00 is the lookup the vehicles' orders use (aiv.zrd
`vlist` item 10, 0x4dfb00; the old p4 order 0x4e0281). The map's mission
boundary is not looked up again: it is the last of the player's order
regions whose name starts `reg_inside_` / `reg_outside_` (0x4d92f0,
0x4d9360, `strncmp`). A region
already loaded is found by name (0x4c5e50, `strcmp`, so case counts);
otherwise a builder is made for the first free id from 10000 and the
name, and what it built is kept only if it is a region whose own `name`
field is the name asked for (else it is deleted and nothing is kept, so the
file is read again at every lookup). Kept regions go to the end of the list
at 0x60b3f0.

A region (0x4c5b00, 0x20 bytes over the edge net 0x4c5640) is built
(0x4bf3f0) when the builder's net id is 10000 or more, the file's version
is 108 and its nodes form one closed loop (0x4bf000): the loop starts at
the first slot holding a real node (the placeholders before it are skipped
with a budget that shrinks with every slot skipped, so the start must lie
in the first half of the slots) and follows each node's first link until
it comes back to the start; a link to no node is reported (0x800,
"region "%s" node %d points to nonexistant node %d") and ends the walk, as
does a node without links, a negative link or running out of slots. Each
step of the loop becomes an edge (0x4bf1b0 -> 0x4c5960, 0x1c bytes): its
start, and the normal of its vertical plane, (dz, 0, -dx) of the side
normalized (0, 1, 0 for a side of no length on the ground, which then
tests only height). The region keeps the file's `name`.

**Inside** (0x4c57d0 -> 0x4c5a30): a point is inside when it is on the
inner side (dot with the normal not negative) of every edge, so a region
is a convex area drawn with its inside on the right going round (seen from
above, x right and z up: the loop runs clockwise). Heights only count for a
side of no length. The test starts at the edge the last failing test
stopped at and goes round; a pass resets it to the first edge. The region
test of an order (0x4d8e70: vlist item 9 "when inside", item 17 "when
outside"; both set: always) needs the region, so an order whose region did
not load never fires on it.

In the shipped missions, 386 of the 390 region files load (each region in
4184 edges' worth, `test_ai_regions.py`); the four that do not:

| Mission | File | Why |
|---|---|---|
| c1 m1 | `reg_firefly_02_go` | version 106: a net, not a region |
| c1 m1 | `reg_friefly_02_go` | its own name is `reg_firefly_02_go` (the file name is misspelt) |
| c3 m2 | `reg_p2`, `reg_up_circle` | version 106 |

No order names any of them. One order names a region its mission does not
have: in c4b mission 6 the player's own `vlist` sets the valves
`inside_c4m7` and `outside_c4m7` from `reg_inside_c4m7` and
`reg_outside_c4m7`, but the archive's files are `reg_inside_c4bm6/7` and
`reg_outside_c4bm6/7`. The mission's fail objective is `outside_c4m7`
("left the mission boundaries", objectives.zrd `m6`), so in that mission
leaving the area never fails it (and the map has no boundary to draw,
likely). Which pair was meant is not clear
(fixes.md, candidates).

## 9. The compiled net

Ported 2026-10-20 (`mech3/zainetedit` Compiler, `mech3/zai_steps`); test
`tests/differential/test_ai_nets.py`.

**When.** As a mission starts the player's set-up (0x49e810, player.cpp)
asks for nets 1..99 (0x402170 -> 0x402240): a builder for each number reads
`net_NN.zrd` and, for a version-106 file, compiles it (0x4be670 ->
0x4be300). The compiled net (0x4bf910, 0x1c bytes, vtable 0x5ca6d8) is a
numbered object under the net's number, so asking again (or a later
builder) finds it and builds nothing. A pilot's brain (0x4cf330) takes
the net of its aiv.zrd record's first item (AiParams +0x04, "kind" in
section 3: it is the vehicle's net number), as compiled (cast +0x34), or
the built-in one at 0x604008 when there is none. In the port
`nets_build_all` / `net_build`, called by the ported mission start
(`mech3::player_setup`, mission-start.md), whose builders then find each
vehicle's net through znet (0x58b160 -> vt+0x28 -> vt+0x34).

**What.** The net becomes a flat array of behaviours (the order classes of
section 2), one slot per piece:

- each **positioned** node (`node_XX (12 (x y z) (links) ...)`) is a node
  step at the slot 0x4bd670 gave it: plain (0x4cf820, vtable 0x5cc6f0,
  named with the record's auxname), or with `deploy` a deploy node
  (0x4cff10: auxname, both valves and bytes, and the slot of the node its
  last item names), with `mcbwait` an mcbwait node (0x4ccc30: auxname,
  valve, byte, time), with `backup` a backup node (0x4cfa00, no name), in
  that order of precedence. Its position is the record's, put on the ground
  under it when a world is loaded (0x4edab0 / 0x4abd90 at [0x608c84];
  `NetHooks::ground`). With `arrangenodes` the node also joins the
  multiplayer "mcbwaynet" (0x4d5460.., mech_waynet.cpp; `NetHooks`, not
  ported, likely only used by the multiplayer deploy points).
- after it, for **each link** in turn, one step per property the node has
  (0x4bd790), in this order: speed (0x4c9eb0, the rate), wait_interval
  (0x4c9f80), wait_time (0x4ca0a0), valve (0x4ca1c0: the valve and its
  value; a valve given without a value gets 1 on the node's last link and 0
  on the others: the way finder lets a pilot by only while the valve holds
  it, section 10), canleave (0x4cbcf0 with test 0x4d9c80 for `canleave (1
  n)`, n != 0; 0x4cbd40 with 0x4d9ca0 for `(1 0)`), scans (one 0x4cb950
  with a test per scan_aim / scan_hit / scan_time present, each by whether
  its second item is non-zero), jump (0x4cb2a0), sound (0x4cce30), gate
  (0x4cd4a0, the name), shunt (0x4cd6e0), valveunion (0x4cda50, the list
  copied), valve_assign (0x4cdc20); then the same for every **edge node**
  (`node_XX ((from to to...) ...)`, no position) whose first link is this
  node, once for each of its other links equal to the link's target. The
  link's steps form a chain: the node step's link points to the first, each
  to the next (+0x0c), the last to the target's node step.
- slots no piece took hold the shared null step (0x603fa0).

So a path's properties sit on its links, not on its nodes: a node's own
properties apply to every link out of it, an edge node's to the links it
names. Then each step resolves its slot numbers into pointers (+0x6c:
0x4c9e60, 0x4c94a0 dropping links to nothing, 0x4cffd0 for the deploy
target). A link to a node without a position (an edge node) or to no node
is no link (dropped by the node step; a property step whose chain ends
there keeps the slot number as its pointer in the exe: the port's FIX
(bug) makes it the null step).

**Counts.** 0x4bd670 / 0x4bd4e0 / 0x4bd5b0 count the slots first: a node
step, then the node's property count times its links, plus each edge
node's properties times the number of its other ends this node links to.
The shipped nets: 308 files, 12944 nodes of which 3384 edge nodes; the
properties used, most first: valve 4416, canleave 2858, scan_time 2796,
speed 452, wait_interval 363, deploy 288, valve_assign 120, arrangenodes
96, valveunion 94, scan_hit 70, gate 46, wait_time 26, jump 14, scan_aim 3.

**Faults kept or fixed** (fixes.md):

- The trailing links that name no positioned node are cut from a node
  step, but its properties are still put on every link: for a cut link the
  chain starts past the node's links (a write past the vector; with every
  link cut, through a null pointer, a crash at load: 0x4bd790). The port
  starts that chain nowhere (FIX (bug)); the steps are still made.
- A node linking to a node number past the net reads the slot map past
  its end (0x4bde20): heap garbage. One shipped net does it: c2 mission 3's
  `net_13`, a single node linking to a node 1 that is not there. The port
  takes no node (FIX (bug); the test pins the exe's read to the same).
- A node that links to one node twice, with an edge node on that link,
  makes more steps than were counted: the exe writes them past the net's
  vector. The port drops them, so the net is what the exe's ends up as.
- arrangenodes past the nodes: section 8.

## 10. Finding the way

Ported 2026-10-20 (`mech3/zai_path`, the steps' tests in `zai_steps`);
test `tests/differential/test_ai_paths.py`. Per-frame code, at 24 bits.

**Nearest step** (0x4d51d0, a method of every net; eight callers: the
behaviours 0x4c8860, 0x4cae32, 0x4cbfb9, the waynet 0x479b58 / 0x47a8a5,
0x45a159 / 0x45a3a9, 0x4ae45c): the element with a position whose squared
distance to the point is least (the first of equals), else element 0.
Only node steps have positions.

**The way finder** (0x4c7d00, 0x38 bytes, kept by its user through
0x4c7c10 / 0x4c7cb0, made on the first search and dropped when a search
fails or the way is used up; its user in the exe is the brain's order
stack, 0x4c84b0). A search (0x4c7f70 -> 0x4c7e80) from a start step to a
target step:

- a pilot standing within 10 (squared distance under 100) of the target
  has the way [start] at once;
- else a depth-first walk (0x4c7fa0) over the steps' links, in link order,
  that measures the way from where the pilot stands through each step with
  a position (straight lines, 0x51e000), keeps the shortest way to reach
  the target (0x4c8140: only a strictly shorter one replaces it), and cuts
  a branch once it is no shorter than the best (so the first of equal ways
  wins). Steps already on the way are not entered again (each step's
  visited bit, +0x04; the net's +0x18 bit marks a search in progress, and
  a second search of the same net at once finds nothing).
- a step is entered only if it lets this pilot by from where it comes
  (+0xac): a valve step while its valve holds the step's value (0x4ca260),
  a jump step if the pilot's vehicle is a mech with jump jets (0x4cb2c0:
  vehicle slot +0x50, mech +0x3d8 > 0), a gate step if the gate's name is
  the vehicle's node's name (0x4cd5d0), a valve-union step while any of its
  valves holds its count (0x4d4600); every other step always.

The way is kept as a list of steps from the start (the target is not on
it). Asked for the step after one it has reached (0x4c7cb0), the finder
drops the way up to that step (0x4c8330), checks that every step left (and
the target) still lets the pilot by from the one before (0x4c8270), and
answers the next one (the target when the list runs out). If the way is
closed or the step is not on it, it searches again from that step and
answers from the new way; no way, or the target, and the finder is
dropped. So a pilot re-plans only when a valve, gate or the like has shut
its way, and the way is the shortest by straight lines between the nodes,
whatever the ground between them (jump steps aside).

## 11. Not ported: the mission boundary

`ZAiNetMechInOut.cpp` (0x4c11f0..0x4c3ed0, 67 functions): at mission load
(0x4c3b80 -> 0x4c36c0, 0x4c3930) the world's `inout` node is found, the
variant for the game mode picked by name (render.md 6a), its polygons
classed by their material colour (0x4c1ab0: four colours, inside or
outside, the four constants at 0x5e8a20..0x5e8a58) and turned into lines
with widths (0x4c11f0, "zero length segment in an inside/outside net"),
then the `inout` subtree is deleted. In play the behaviours ask 0x4c3000
(seven callers) to pull a destination back inside: 0x4c2b00 crosses the
segment from the vehicle to the destination with the lines (0x4c1410,
0x4c1770, 0x4c22c0) and moves the point to the nearest crossing or along
the boundary. It reads the world's nodes and models (zModel polygons with
their material colours, the node matrix 0x4c3520), so it belongs with the
world's port; the next piece to take for the AI's movement.

## 12. The AI controls and the brain's data

Ported 2026-10-20 (`mech3/mech_control` AiControls, `mech3/zai_brain`); test
`tests/differential/test_ai_brain.py`. Only what
the constructors set up: what the brain decides each frame (0x4e9710), the
behaviours and the firing are not ported.

**Who makes them.** player.cpp's AI mechs (0x49fa20 -> 0x49fe90:
`new(0x320)`, 0x4d8750(owner 0, net, record, 1)), the campaign's lancemates
at launch (0x49e5e0, the second caller 0x49e208) and the AI-driven vehicles
(0x49f800 -> 0x49ff10: 0x4ac860(0xcd8), malloc and zeroed, 0x4e71c0). The
net is the vehicle's compiled net (mission-start.md section 3).

**The mech's** (0x320 bytes): MechControls 0x4265a0 with most-derived 0
(view state 2, ThrottleConst off, the hat at 1, every axis 0; +0x40 and
+0x5b left as the heap had them), the brain on +0x5c (a base class, vtable
0x5cd2c8), the IControls virtual base at +0x30c (vtable 0x5cd210, owner
+0x31c, the owner the mech's constructor sets through vt+0x08). Its
vt+0x04 is the brain's set-net 0x4e9380, vt+0x0c the update 0x4e96f0,
vt+0x88 the settings' assignment 0x4d99e0 (the launch gives the controls
their record so), vt+0x90 the brain itself (player.cpp holds its +0x88 at
1 for the power-down's update).

**The vehicle's** (0xcd8): the basic vehicle controls 0x4ac750 (veh_basic.cpp:
its +0x764 block holds at +0x28 a pointer to turret limits at +0x2c,
0x4a4a70: the sines and cosines of -pi/2 and pi/2; the -pi and pi it is
also given are not kept), the brain on +0xa04, +0xcb0 a zero vector,
+0xcbc 0, +0x38 2, the IControls base at +0xcc4 (owner +0xcd4). Every other
byte stays 0 from the allocation.

**The brain** (0x4cf330, 0x2ac bytes; offsets the brain's):

| Off | What |
|---|---|
| +0x00 | the record's settings copied (AiParams, 0x4d9bf0 -> 0x4d9940: the 0xcc bytes, the orders' list copied, an empty one as none, the pilot and voice strdup'd), +0xdc FLT_MAX |
| +0xe0 | the virtual base pointer |
| +0xe4..+0x104 | 0, FLT_MAX, 1.0, 0, FLT_MAX, byte 0, 0, 0 (+0x100 the fire timer), byte 0 (+0x104 the order's run-again flag; section 4) |
| +0x108 | the firing state (0x4b6570, 0x58 bytes): 11 words from 0x5e8868 (8, then zeros), +0x2c the brain (0x4b6660, which also calls the brain's IControls vt+0x10(0): nothing then), +0x34 a fire plan (0x4b6390, vtable 0x5c9458), +0x54 the aim (`new(0x24)`, 0x4b7060: zero, (1, 0, 0), 0, 0, 1.0) |
| +0x160 | the order stack, a `std::deque` of 0x98-byte orders (blocks of 26, 0xf70 bytes); +0x190 its owner, the brain; the last order is the current one (+0x17c) |
| +0x194 | an order of the brain's own (0x4e8180, vtable 0x5cd8e8), its owner the brain |
| +0x22c..+0x23c | byte 2, then four words 0 (+0x238 and +0x23c objects the destructor deletes; nothing here sets them) |
| +0x240 | a step (0x4cd770 over 0x4c9d00: vtable 0x5cc290, +0x0c the null step 0x603fa0; the base clears only bit 0 of +0x04) |
| +0x250 | steering (0x4ce620, 0x48 bytes, likely: 0x4ce6a0 tests a distance against +0x34): zeros, +0x34 20.0, +0x38 [0x60b4a0] = cos(pi/4) (start-up 0x4ce5b0); [0x60b4b8] / [0x60b4bc], read into +0x0c, +0x10, +0x2c, +0x30, are never written (0) |
| +0x298..+0x2a8 | a zero vector, byte 0, the radio |

Then: without a net, znet's object of the record's first item (0x58b160,
vt+0x28, vt+0x34), else **the built-in net** at 0x604008 (made at start-up
by 0x4c0310 over the seven-behaviour net, not ported: the port has none,
so a pilot whose net is missing gets no order; player.cpp never asks for
one without a net). Then **the first order** (0x4e9380): the net as
compiled (vt+0x34; not compiled: nothing more, no order at all), every order
popped (none at first; each destroyed in place, a block freed as it empties,
the map with the last), and one pushed (0x4e9d90: an empty deque takes a
block and a map of two, `new(0xf70)` then `new(8)`, the order in the
block's middle; the order is 0x4e8070's default, whose template argument
is unused, then +0x14..+0x2f copied from the template at 0x60b898, or from
the last order when there is one, and the brain its owner). In it the net
builds its part at +0x30 (vt+0x5c: 0x4c0070 for a compiled net, vtable
0x5ca748, 0x4c0520 for the built-in one, 0x5cab50; the order and the net)
and the shared behaviour at 0x60b460 (class 0x4cbe10, made at start-up)
its state at +0x54 (vt+0x8c, 0x4cbe30: vtable 0x5cbed0, the order);
+0x0c and +0x10 point at them.

**The radio** (0x4e7e20, `new(0x40)`, 0x4e7c40): for a pilot with a voice
(aiv.zrd `sound_tag`, set by default for the three lancemates by name),
sixteen zSound lookups of `<voice>_<event>` (0x4e7dc0: call_help,
last_shot, confirm_order, confirm_refit, disregard_order, disengages_order,
ejecting, engaging, friendly_fire, low_ammo, out_of_ammo, nuke, follow,
stop, repairs, objective), kept as handles. Only when the player's vehicle
([0x60861c] +0x14) counts the pilot's teams (5 bits at a time, as the team
masks) among its allies, or when there is no player's vehicle. At mission
start there never is (the cleanup 0x491780 clears it, the launch sets it),
so every voiced pilot made then gets a radio, enemies too; the lancemates
made at launch (0x49e5e0) are checked against the player's mech.

**Destruction** (IControls slot 0 with 1: 0x4d8bf0 / 0x4e7820): the brain's
destructor 0x4cf630 (the objects at +0x23c and +0x238, the radio, the
orders first to last with the block and the map, the firing state's
0x4b65f0: +0x14 unless it is +0x30, +0x30, +0x50 and the aim, then the
settings), for the vehicle then the basic controls' 0x4ac800 (the owner's
vt+0xec and 0x402510 on its answer), then the block (`delete`, the
vehicle's 0x4ac890).

**Fixes** (fixes.md): the radio's names are built with `sprintf` into 32
bytes, so a voice of 15 or more characters overruns the frame (the shipped
are 3 to 5): cut to fit, FIX (bug). The vehicle controls' destructor reads
through its owner, which a vehicle that failed to build never set
(mission-start.md section 5: a crash): asked only when there is one, FIX
(bug).

**The test** (`test_ai_brain.py`, probe `tests/probe_ai_brain.cpp`): every
aiv.zrd record of the 48 archives (768, mech and vehicle controls in turn)
read by the ported reader, then 600 crafted (random words, first items,
teams packed and odd, voices of 0 to 14 characters or none, pilots, order
lists of 0 to 4 random entries or none); the vehicle's net compiled, not
compiled or none, znet answering a compiled net, the net base or nothing;
no pilot, a pilot without a vehicle, or a vehicle with assorted allies; an
owner or none (the vehicle without one is not deleted). The original runs
first (x87 at 53 bits, the start-up's cos(pi/4) and the shared behaviour
made by their own code, the built-in net's vtable put in place), every call
out stubbed; the port then on a Replay of its answers. Compared: the call
traces (`new` and `delete` with their blocks, znet, the sixteen sounds, the
owner's calls; the settings' list copies and strings apart), every block
the original allocated as the port's object laid out byte by byte (the
step's +0x04 as its bit), the settings' copies (the list walked, the
strings as text), nothing written outside the blocks: 1368 of 1368, 13,595
calls and 5478 blocks identical. Breaking the port showed the test catches
it, 20 of 20: +0xec 0, the firing defaults moved, the steering's 20 as 10,
a radio for every team, two radio events swapped, the map before the block,
the built-in net's part as a compiled one's, any net taking an order, znet
not asked, the turret limits swapped, view state 0, the vehicle's +0x38 0,
the map freed before the block, one team per value for the radio, the
brain's own order without its owner, the aim's (0, 1, 0), the vehicle's
destructor not asking its owner, the order without its owner, the
settings' copy without its orders, a radio without a voice.

**In the program** (`app_mission.cpp`): player.cpp's hooks make the port's
controllers over the vehicle's compiled net and record, and the AI mechs on
them through the ported constructor chain (mech-build.md); the placement
0x4161c0 is not ported (the mech stays where its constructor left it, the
exe's always succeeds), the power-down runs `Mech::power`, the brain's +0x88
is the brain's settings. The vehicles themselves are not ported, so their
controllers are made and deleted. `mw3tests`
`mission_start_reads_c1m1_vehicles_headless` checks c1 mission 1's AI mech:
owens_01, an Owens (11 parts: its model has no hands) on team 2, powered
down, its brain's settings, one order on net 4 with the net's part and the
shared behaviour's state, no radio (no voice).

## 13. Valves at run time

Ported 2026-10-20 (`mech3/zai_valve`, `mech3/zrdrvalve`); test
`tests/differential/test_ai_valves.py`.

**A valve** (0x4d3b20) is a name, a value holder and its reactions. The
holder (0x4e4460, vtable 0x5cd558) keeps a number: get vt+0x04, set vt+0x08,
vt+0x0c says valves.zrd may replace it. Setting a valve (0x4d3c00) stores
the value and fires its reactions (0x4d3bf0) only when it changes, and
gives back the old one. The reactions sit in one slot (+0x08): the first
alone, the second turns it into a list of both (0x4d3fd0, vtable 0x5ccd70),
later ones join the list (0x4d4320); firing a list fires each in order
(0x4d41a0). Every reaction's vtable: +0x00 the deleting dtor, +0x04 tick,
+0x08 fire, +0x0c the list cast.

**valves.zrd** (one file object at 0x60b668, 0x4e5290 over the reader-file
base, read at the mission setup by 0x4e2120: a reference held while every
record is built). A record is `"valve" (tag (args) tag (args) ...)`; a
valve may have several records (c1 mission 1's `bramp` has four), each
adding to it. The record (0x4e2330 into the 0xc8-byte block at the file's
+0x2c, 0x4e2190) reads, all lists of raw words, names strdup'd: `value`
(n), `sound` / `sound_once` (name n), `animation` / `animation_once`,
`scan_aim` / `scan_hit` / `scan_time` / `canleave` (vehicle on n),
`shutdown` (vehicle down n), `missionreset` / `missionabort` /
`missioncomplete` (n), `destroy` (entity n), `setteam` (entity team n),
`attack_strategy` (entity strategy n), `update` (valve), `delayupdate`
(valve seconds), `any_nonzero` / `any_zero` / `all_nonzero` / `all_zero`
(any value: the shipped files give the string `namelist`, so the next
`namelist` found is the list of names), `teleport` (entity (x y z) yaw n).
The tag lookup searches nested lists (as every reader file's), and the
second of a pair wins (`sound_once` after `sound`).

**The build** (0x4e44b0): the valve found or made (0x4d3c40); a condition
tag needs a non-empty namelist ("Expected non empty namelist"), a namelist
without one is reported ("Expected no namelist") and ignored; a valve that
already holds a condition cannot take another ("Conflicting definition":
the record is dropped). The condition replaces the holder; then `value`
sets the holder directly (no reactions fire), then the actions are added in
a fixed order: sound, animation, the four scans, shutdown, destroy,
setteam, attack_strategy, teleport, missionreset (only reported: "Failed to
create mission reset", nothing is made), missionabort, missioncomplete,
update, delayupdate. Reports go to 0x49bfd0, empty in the release build.

**The conditions** (0x14 bytes over 0x4e40e0): the namelist's valves found
or made (a name that is no string is dropped with a report), each given an
update reaction of the condition's valve (0x4e4080), so a change of any
member fires the condition's reactions. The value is computed on each get:
any nonzero, any zero, all nonzero, all zero (vtables 0x5cd568, 0x5cd578,
0x5cd588, 0x5cd598); a set is ignored, but the set still fires the
reactions when the computed value differs from the one asked.

**The actions** (each fires when its valve's value equals its own, but the
two updates):

| Tag | Made by | Fires |
|---|---|---|
| sound | 0x4e2ff0: zSound's lookup of the name (0x56f6f0) | 0x4e3040: plays it (0x56e7e0, volume 1); `sound_once` (0x4e30b0) only the first time |
| animation | 0x4e30f0: the mission's animation by name (0x50b450) | 0x4e3120: runs it (0x508c10); `_once` the first time |
| scan_aim, scan_hit, scan_time, canleave | 0x4e3230: the named vehicle's brain (0x4d7330, vt+0x40, vt+0x140, IControls vt+0x54) found as the mission starts | 0x4e3200: the brain's switch on or off by the record's flag: +0x98, +0x9c, +0xa0 (aiv.zrd p8's 14th..16th items), canleave +0x3c (params' second item) with +0xdc FLT_MAX (0x4d9c20..0x4d9ca0, the same functions the compiled nets' scan steps name) |
| shutdown | 0x4e3560 | 0x4e3650: a mech of that name powered down or up (0x41cc30, the record's second item) |
| destroy | 0x4e33c0 | 0x4e34b0: a mech destroyed (0x41cfa0(1)), anything else through its vt+0x70 |
| setteam | 0x4e3700 | 0x4e37f0: the entity's AiTarget masks from the team, as the mission start sets them |
| attack_strategy | 0x4e38d0 | 0x4e39c0: the vehicle's brain +0x40 (aiv.zrd p2's first item) |
| teleport | 0x4e3a90 | 0x4e3ba0: zEntity's vehicle (0x5a8210) placed at (x y z) and yaw (vt+0xdc), its controls given the brain's current order's net again (IControls vt+0x04, section 12), so it plans its way from there |
| missionabort | 0x4e3310(0x48f730) | the missions' abort (0x48f9e0) |
| missioncomplete | 0x4e3310(0x48f710) | 0x48f710 is empty (a report): nothing happens |
| update | 0x4e3d20 | 0x4e3d90: the named valve's reactions fire, whatever the value |
| delayupdate | 0x4e3f50 | 0x4e3fb0, on any change: a timer (0x4e3e60) that sets the named valve to this valve's value then, the delay after the game time [0x60991c] |

**Timers** (the slot 0x60b514, one or a list): the missions' update
(0x48f470) calls 0x4d3d30 every frame; a timer whose time is past (0x4e3f10:
time < now) sets its valve, deletes itself and leaves the list (0x4d41e0,
which visits a timer added meanwhile too; an emptied list deletes itself).
A **lone timer** whose valve's reactions start another delayed update is
turned, as it runs, into a list of itself and the new timer; 0x4d3d30 then
clears the slot, so the new timer (and every other one the same update
starts) is lost. FIX (bug), behind `zfix bugs`: the list stays. Two shipped
chains can reach it: c3 mission 2's `ambush_powerup_down_a` (1 s) starts
three 10 s timers for the radio lines `ig3214`, `ig3215`, `ig3205`, and c3
mission 3's `m3n2go`; whether the lines are lost depends on whether another
delayed update was waiting then. At mission end (0x4d3fa0, from the cleanup
0x491780) the timers are deleted and every valve with them (0x4d3ef0).

**In the shipped files** (46 of 48 missions have one, 1512 records): sound
537, delayupdate 294, attack_strategy 236, sound_once 229, shutdown 185,
any_nonzero 68 and all_nonzero 69 (all_zero 2), teleport 5, destroy 4. No
mission uses scans, canleave, animations, setteam, update or the mission
tags. c3 mission 3 keeps a dead record, `:m3n2go_delay` (a colon in its
name, `:` around a shutdown of avatar_02, a mech the mission does not
have: a second Avatar taken out, likely). Every shutdown in
the shipped files has the power flag 0: it **wakes** a mech (the AI mechs
that start powered down, mission-start.md section 8).

**Fixes** (fixes.md): the record's tags read as lists and names as strings
whatever they hold (a crash otherwise: FIX (bug), the six broken files of
the test); a scan whose vehicle has no brain writes through null when it
fires; a teleport of a pilot without an order reads before its deque; the
lone timer above. A valve that updates itself, directly or round a loop of
updates and conditions, recurses until the stack runs out, in the exe and
in the port (a candidate, not fixed; no shipped file does it).

**The test** (`test_ai_valves.py`, probe `tests/probe_ai_valve.cpp`, the
emulator side over `ai_world.py`'s): every mission's valves.zrd loaded,
then forty steps of sets (every valve to the values the records use),
ticks forward in game time and the mission's end; then 300 made-up files
(every tag, conditions over earlier valves, updates of later ones, scans of
a vehicle with a brain, entities found or not) and six broken ones the
original crashes on. The world is fake: names to objects whose vtable
slots answer from a table, brains with one order. Compared: one stream (the
registry, each valve's holder, value and reactions with every field, the
timers, after the load and after every step; the brains' and targets'
words written) and the log of every call out (zerr, the lookups, sounds,
animations, powers, destroys, the objects' calls, places, nets): 352 of
352, the original path (`zfix bugs` off; the lone timer's fix has its own
`mw3tests` case). Breaking the port showed the test catches it, 22 of 22:
sound_once replayed, any_zero as any_nonzero, all_nonzero as any_nonzero,
a set firing unchanged, conditions not firing on a set, a timer due at
its time (the port then loops forever: caught as a hang), delayed updates
only on one value, the lone timer kept, a list's tick stopping at the
first done, a scan's flag and value swapped, sound losing to sound_once's
order, setteam's allies keeping bit 0, shutdown's flag inverted, a
conflicting condition replaced, the initial value firing, a teleport
reusing its first controls, missionreset made, null names kept in a
namelist, the namelist from its second name, members without their
update, the attack strategy one word off, canleave without +0xdc.

**In the program** (`app_mission.cpp`): the mission setup's 0x4e2120 runs
`valves_load`; the actions find the AI mechs by their records' names (the
other vehicles are not ported, so their lookups give none), power them
through `Mech::power`, write their brains' settings, re-plan a teleported
pilot (its placement is logged: not ported), play sounds through zSound;
animations, destroys and the mission's abort are logged. Left: the
missions' per-frame update 0x48f470 that ticks the timers, and the cleanup
0x491780 that ends them (`valves_end`), both mission.cpp's.

`mw3tests` `mission_start_reads_c1m1_vehicles_headless` loads c1 mission
1's valves.zrd through the program: `bramp` holds its four reactions, and
setting it wakes the Owens (Mech::power) and starts its two timers.

## Open

- Whether mission loading runs at 24 or 53 bits: hardware mode has a
  Direct3D device by then. The readers give the same result either way.
- What the behaviours, the order items and most AiParams numbers mean (the
  brain is not mapped beyond section 4).
- Duplicate record names in a file: by number both are read, by name the
  first wins.

## The developer tools (decided with Owen, 2026-10-20)

The AI tester dialog and the in-game AI path editor (ZAiNetEdit's editing
side), reached in the exe through the debug menu, are to be ported later
behind a `--debug` switch, off by default, as hidden as the original's
menu. They are not needed to play.

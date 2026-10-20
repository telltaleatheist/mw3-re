# The mission start: the vehicles from aiv.zrd

Mech3.exe 1.2, `C:\zipper\Mech3\player.cpp` (0x0049dc80..0x0049fa20, and
the vehicle builders after it up to 0x0049ff80, whose reports name the same
file). Ported as `mech3/player`; checked against the exe by
`tests/differential/test_mission_start.py` (section 7). 2026-10-20.

## 1. Where it sits

The mission setup 0x00490ab0 (mission-load.md section 3) calls 0x0049e810
with CL = the app's +0x1b2 ([0x00608e3a]: 1, 0 after the debug menu's Mech
Lab), after the InOut AI data, `landmines.zrd` and the radar's object.
**It does not build the player's mech.** That happens at launch: the
briefing's Launch (0x0040500a) calls 0x00491150 (mission.cpp), which reads
the `player` record, makes the mech from the pilot's game slot (0x004140c0,
its skin through 0x00414390), the controls (0x00427280), the cockpit, and
then per game type the campaign's lancemates (0x0049e5e0), instant action's
whole line-up (0x0049dc80) or multiplayer's (0x0046da80). Section 8.

## 2. 0x0049e810

1. [0x005ea1d0] = 1 (zModel's zone mode, render-scenewalk.md; the ground
   probes clear it around their query).
2. With CL: **the AI nets** 1..99 compiled (0x00402170, ai.md section 9).
3. 0x004d52b0: every registered net's vt+0x28 -> vt+0x2c (the compiled
   nets and regions override it with an empty function); at the end
   0x004d5300 the same with vt+0x30 (the link pass, 0x004d53f0).
4. **The player's tick**: an object3d node [0x00609a00] with action
   priority 2 and action 0x004a1160 (section 6).
5. **Gravity** from `player.zrd` (zbd\reader.zbd: `nom_gravity` 40,
   `wat_gravity` 9, `qsd_gravity` 3) into [0x00609a14], [0x00609a0c],
   [0x00609a10]; missing keys give 60, a third of it and a sixth.
6. Without CL, or in multiplayer (3) or instant action (2), that is all:
   those place their own vehicles at launch.
7. `vehicle.zrd` (the vehicle types: stealth, ttruck, bulldog, harasser,
   striker, apc, mobilehq), an aiv.zrd record object on the stack, the
   `player` record read by name, the record count (aiv.zrd, ai.md section
   3), the campaign game (0x00479440: the pilot's game when type 1) and the
   training game (0x004794a0: type 4). The player's team is kept (+0x18).
8. **Training**: the player's mech definition (0x004846a0 on the record's
   +0xc4) gives its configurations (+0x20, vector +0x0c); the record's +0xbc
   (p11) picks one, which the training game's slot (vt+0x34(0)) takes
   (0x00433df0). So the training missions set the player's loadout.
9. **Every other record** (not `player`, not `stealth`): its name is split
   at the first `_` into a type and a suffix (`owens_01`: type `owens`);
   the mech index (+0xc4, p12) is clamped to -1..18.
   - **A campaign lancemate**: a campaign game, a mech, the player's team
     (non-zero) and `pilot (... can_configure true)` (+0xcc bit 7): a
     0x18-byte slot (0x00433cc0: mech, 0, 0; +0x14 the record's index) goes
     to the campaign's lancemate list (0x004351c0). A slot already there
     keeps its own mech and loadout and only takes the new record index; a
     new one is appended. The count goes to +0x9c (0x00435230) at the end.
     The lancemates' mechs are made at launch (0x0049e5e0).
   - **Anything else** goes to 0x0049fce0 with vehicle.zrd's entry for the
     type (0 when there is none).
10. vehicle.zrd freed, 0x004d5300, the record object dropped.

## 3. Which builder: 0x0049fce0

`player` and `stealth` make nothing. A mech index (+0xc4, else the type as
a mech name, 0x004845f0, any case) makes an **AI mech** (0x0049fa20).
Otherwise the record's first item (+0x04, its AI path) names a net: znet's
object of that number (0x0058b160), its vt+0x28 (as_net), vt+0x34
(as_compiled). With a compiled net, or a suffix starting `mech`, it is an
**AI-driven vehicle** (0x0049f800); without either, a **vehicle without a
driver** (0x0049f8a0 with no controller), and nothing at all when
vehicle.zrd has no such type.

## 4. An AI mech: 0x0049fa20

1. The net (the record's, when the caller gave none). None: "no mech net
   %d found for %s (%d)" and the mech is **not made** ("ignoring %s (%d)").
2. The controller (0x0049fe90): `new(800)` and the AI mech controls
   0x004d8750(0, net, record, 1), which make the pilot's brain
   (controls.md, ai.md). None: "failed to create controller for %s (%d)",
   ignored.
3. The mech: 0x00414150(mech index, controller, +0xbc, name): the mech data
   of the index (0x00430d10, 0x00432950, the default when +0xbc's lookup
   fails) built by 0x004140c0 (`new(0x6d8)`, the constructor 0x004149f0,
   structs.md). None: "failed to create mech for %s (%d)", the controller
   deleted through its IControls base, ignored.
4. The skin: +0xc8 (aiv.zrd `skin`) from 1 to 7 picks the definition's
   skin of that index (0x0041e9a0 +0x1c), given to 0x00414390 (which
   swaps the mech's texture set, 0x004757f0).
5. The team masks (section 5), then 0x004161c0 puts it on the ground at the
   record's (x y z) and yaw (section 11). Failing (it never does):
   controller and mech deleted, 0.
6. **p12's second flag** (+0xcc bit 5): the controller's vt+0x90 object
   gets +0x88 = 1 for one 0.1 s update of the mech (vt+0x34), then its own
   value back, and the mech **powers down** (0x0041cc30(1)).

## 5. A vehicle: 0x0049f800 and 0x0049f8a0

0x0049f800 takes the net again; with one the controller is the AI vehicle
controls (0x004ac860(0xcd8), 0x004e71c0(0, net, record, 1), 0x0049ff10).
Without one the exe passes **the record itself as the controller** (FIX
(bug), section 9). A failed build deletes the controller.

0x0049f8a0: type `mobilehq` (matched whole, 9 bytes with the NUL) is the
mobile HQ (0x004589d0), any other the basic vehicle (0x004ac320, veh_basic),
each given the controller, type and name; none: "failed to create basic
vehicle: type "%s", name "%s"". Then the entity's name (0x005a84c0), the
team masks, the controller defaulting to the vehicle's own (+0xe4), its
place (vt+0xdc: (x y z), yaw), its setup from vehicle.zrd's `common_mode`
(vt+0x158 with the name and the record's path number), 0x0049ec90 ->
0x0049ef40 (its `sounds` and `motion_anim` among others, with the mode
list, the def's item 4, and the controller's +0x214 or a scratch word),
0x0049f660, 0x005833b0(node, 1, 0x004af550) (zWeapon's touch handler on its
node) and 0x005a85c0.

## 6. The team masks and the tick

**Teams** (inline at 0x0049f924, 0x0049fbe1 and the launch's 0x00491262):
aiv.zrd's team is read as 5-bit team numbers, one per 5 bits; the mask
(AiTarget +0xc0) has bit n set for each, the allies (+0xc4) are the mask
without bit 0 and the enemies (+0xc8) everything else but bit 0. Team 1
(the player's side) is mask 2, allies 2, enemies 0xfffffffc; team 2 the
reverse; team 0 is nobody's ally and everybody's enemy. [0x0060b56c] is
cleared each time.

**The player's tick** 0x004a1160, the action of [0x00609a00], once a frame:
the frame time [0x0072c52c] into [0x005e8110] and its inverse into
[0x005e8114] (10000 when it is 0, or NaN: the x87 compare reads that as
equal), the pilot's vehicle updated (vt+0x34), the stealth entity too when
the view ([0x005f4c8c] +0x64) is on it, [0x006099c0] = 0, 0x004a1140
(0x005a8850 on 0x00736558 when [0x005e7284] is set), 0x0041e3f0, in instant action and
multiplayer 0x00537f20(10), the vehicle's 0x0041aba0 and the view's vt+8.

## 7. The port and its test

- `mech3::player_setup` (0x0049e810), `create_vehicle` (0x0049fce0),
  `create_ai_mech` (0x0049fa20), `create_controlled_vehicle` (0x0049f800),
  `create_basic_vehicle` (0x0049f8a0), `make_mech_controls` (0x0049fe90),
  `make_vehicle_controls` (0x0049ff10), `set_team_masks`, `player_tick`
  (0x004a1160); globals in `PlayerState`. Every call out of player.cpp is a
  `PlayerHooks` method named by address; the records are read through hooks
  too.
- The program (`app_mission.cpp`) wires the reader, aiv.zrd, the mech
  definitions, the nets (the compiled ones built by `nets_build_all`),
  zClass, the AI controllers (0x004d8750 and 0x004e71c0 with the brain's
  data, ai.md section 12) and the AI mechs (mech-build.md) to the ports.
  The placement 0x004161c0 is the port's (section 11), so the Owens stands
  on the terrain at its aiv.zrd place; the other vehicles are not ported (logged, none made: their controllers are deleted). The weapons load
  0x00583880 now runs the ported `zwep::init` with `make_weapon_type`, which
  makes the weapon_tick object (priority 3, action `zwep::tick`). Both run
  each Core frame from the ported action lists (0x004f7d80,
  `action-lists.md`); `mw3tests` `mission_c1m1_frames_run_the_ticks_in_order_headless`
  checks the order frame by frame.
- `mw3tests` `mission_start_reads_c1m1_vehicles_headless`: the debug launch
  of c1 mission 1 without a window: gravity 40 / 9 / 3, the tick object at
  priority 2, weapons.zrd's definitions and the weapon_tick object, the AI
  mech owens_01 built (an Owens on team 2, powered down, on its controller
  with its brain and one order over net 4), three mobile HQs and four basic
  vehicles asked for, every one over its net, their controllers made.
- `test_mission_start.py` (probe `tests/probe_player.cpp`): 400 cases, the
  setup and the tick each, in the emulator (x87 at 53 bits) and the port,
  every call out stubbed and answered from one script. The records are
  every shipped aiv.zrd (all worlds, campaign, instant action, multiplayer
  and training blocks) read by the ported reader, from case 42 on changed at
  random: mechs (-3..25), teams (packed ones too), the lancemate and
  power-down flags, skins (0..9, -1), configurations, paths, names (long
  ones, `mech` suffixes, no `_`, a made-up `mobilehqs` type), records
  dropped, the player missing or shuffled, reads failing, the count off by
  one. The engine's side: game types -1..5, campaign and training games or
  not, player.zrd keys missing, vehicle.zrd missing or short of types, nets
  present at random (none, half, all), every allocation, controller, mech,
  vehicle and placement failing at random, the definitions' vectors of 0..9
  (null entries, null vectors), the controller's held object or none; the
  tick at frame times 0, NaN, 1e-30 and plain ones, the stealth entity or
  none. Compared: the call traces (strings as text, zerr's reports
  formatted), the globals (0x005ea1d0, 0x00609a00, the three gravities,
  0x0060b56c, 0x005e8110, 0x005e8114) and every dword written outside the
  stack (the team masks, the lancemate slots' +0x14, the held flag,
  [0x006099c0]). 35,209 calls identical (92,334 over 1000 cases). Breaking the port showed the test
  catches it, 22 of 22: the allies keeping bit 0, the mech clamp at 19, the
  lancemate flag read from bit 6, skin 8 allowed, the water and quicksand
  defaults swapped, `mobilehq` matched as a prefix, no `mech` suffix rule,
  the animation slot from the vehicle's controls, the held flag not
  restored, the power-down only with the held object, the player's team
  read only in training, lancemate slots counted from 1, the tick's inverse
  1000 at 0, NaN not taken as 0, the stealth updated when not looked at,
  the exe's name buffers ignored, instant action not skipped, the type's
  mech looked up for index 0 too, the controller kept on a failed mech,
  the configuration index's signed check dropped, the record not passed as
  the controller, one team per value.
- What the test leaves out: a lancemate slot's allocation failing and a
  vehicle build failing with the record as its controller (both crash the
  exe, section 9), a name over 235 characters (it would overwrite the
  record's order list), names with `%`.

## 8. Findings

- **Two in three campaign AI mechs start the mission powered down**: their
  p12 lists end in 1 (`p12 ("shadowcat" 0 1)`), the flag above; 106 of the
  168 campaign and training AI mechs (lancemate slots aside). Most of
  operation 1's are, while whole groups later start up and running (c2
  mission 3, c4 mission 1, c4b mission 4). Waking them is the AI's (not
  ported yet).
- **Dominic starts c1 mission 4 powered down too**: his record there has
  the flag and `can_configure false`; he is a plain AI mech in that mission.
- **Lancemates join the lance after their first mission.** Dominic Paine
  in c1 mission 4, Epona Rhi in c2 mission 4 and Alan Mattila in c3
  mission 2 have `can_configure false`: they are made like any AI mech
  there, with the mission's own mech and loadout. From the next mission
  their records say `true` and they become slots of the campaign, kept
  from mission to mission (with the loadout the player gives them in the
  mech lab); a mission's record only tells the slot which record (place,
  orders) it is. c3 mission 3 gives Alan two `pilot` entries, `true` first;
  the reader takes the first.
- **A missing AI path costs a mech but not a vehicle.** An AI mech whose
  path (the record's first number) has no net is not made at all; a
  vehicle without one is made but has no driver. Every shipped campaign
  and training record has its net.
- **Team numbers can be combined**: aiv.zrd's team is read 5 bits at a
  time, so one value can put a vehicle on several teams (33 = teams 1 and
  1, 65 = teams 1 and 2). No shipped file uses more than one.
- **Training sets your loadout from the mission file**: the player record's
  p11 picks one of the mech's stock configurations.
- **Gravity is data**: `player.zrd` gives 40 on land, 9 in water and 3 in
  quicksand (the exe's own defaults would be 60, 20 and 10).

## 9. Quirks and fixes (fixes.md)

- The name is copied with `sprintf(buffer, name)`: a `%` in a vehicle's name
  would be read as a format. FIX (bug): copied as text.
- The type's buffer is 20 bytes and the name's 28, followed by the record
  being read: a name of 20 or more characters runs from the type into the
  name (the name then reads as its own tail, cut where the type is cut at
  its `_`), one of 28 or more on into the
  record's vtable and settings (the path number first). No shipped name is
  longer than 19. FIX (bug), behind `zfix::options().bugs`: whole names; off,
  the port lays out the exe's frame and gets the same.
- Without a net, 0x0049f800 passes the aiv record as the vehicle's
  controller: the vehicle's animation slot becomes the record's address +
  0x214 (stack), and a failed build deletes the record through its first
  field as if it were a controller (a crash). Reached only by a `mech...`
  suffix without a net; none ship. FIX (bug), behind `bugs`: no controller.
- A lancemate slot whose `new` fails is written through (a crash). FIX
  (bug): skipped (new does not fail).

## 10. Left

- The launch 0x00491150 and the mech's constructor chain (0x00414150,
  0x004140c0, 0x004149f0 and down) are ported: mech-build.md. The AI mechs
  of section 4 can use it (`app_mission.cpp` wires `mech_create` to it)
  once their controls are. Left of the launch: 0x0049e5e0 (the campaign's
  lancemates from their slots, with 0x0049e540), 0x0049dc80 (instant
  action's line-up from its records, 381 lines), 0x0046da80 (multiplayer).
- The builders' callees: the vehicles 0x004589d0 / 0x004ac320, their setup
  0x0049ef40 and 0x0049f660 (and their vt+0xdc place), the skin swap
  0x004757f0. The AI controls 0x004d8750 / 0x004e71c0 are ported up to the
  brain's decisions (ai.md section 12).
- The nets' passes 0x004d52b0 / 0x004d5300 (base AiNet vt+0x2c 0x004d5350
  and the link pass; the compiled nets and regions do nothing there).

## 11. The placement: 0x004161c0

`Mech::place(pos, yaw)` (mech.cpp; `mech3/mech_place.cpp`), from
player.cpp's AI mechs (section 4) and the pilot's spawn 0x004782f0 (the
player, mech-build.md section 8). Always 1.

1. zModel's zone mode [0x005ea1d0] is saved and set to 0 (every node
   passes), the ground under (x, y + 100, z) is asked of the world
   ([0x00608c84]+4) with the **point query 0x004edab0** (below), and the
   mode restored.
2. With polygons found, **0x004abd90** picks one with the mech's y and
   **y + 10 as its step** (water, soil 1, not taken as ground), and y
   becomes the pick minus -0.01: **the mech stands 0.01 above the ground.**
3. The mech's node joins the world: [0x00608c84] vtable +0 (0x0040bab0)
   is `node_add_child(world, node)`. The query ran before, so a mech never
   finds itself.
4. vtable +0xdc, **SetPlace 0x00418f30**: the controls zeroed (0x00426c80),
   the orientation the yaw's quaternion (0x005251a0) and the node's 3x3 from
   it (0x00525740), then **0x00418e90**: the controls zeroed again, +0x3c,
   +0x30, +0x58 zeroed, vtable +0x18 (zEntity3d's 0x005a8550: the node's
   translation 0x004fc0b0, then 0x005a85c0), 0x005a85c0 again (for a node
   with a parent, vtable +0x1c 0x005a85a0: the node's matrix set, the two
   marker nodes' world matrices, +0x18 and +0x24 from the matrix's z row),
   the collider's points through the matrix into this frame's and last
   frame's (0x00422500), and the skis reset (0x00475fb0: the six ski
   probes at (+-half track, 0, -half / 0 / +half), both skis level through
   0x004764a0).

**The pick 0x004abd90** (between turret.cpp and veh_basic.cpp; the skis'
0x00477820 is its twin with the soil table): the highest polygon at most
`step` above y; water (a material whose +0x20 is 1, read directly) gives
the surface and is ground only when the fourth argument is 0; in water when
the surface is within the step below y. None in reach: **the lowest
polygon**; an empty record: 0. Its other callers pass a constant step (4,
20, a mech's own); 0x004161c0 passes y + 10, so the reach is 2y + 10, not
y + 10 (below).

**The point query 0x004edab0** (cls_di.c, `zcls::ground_point`): one point
instead of 0x004ee2f0's up to 24. The stored cell of the point (moved into
it when the world wraps; off the grid without wrapping, no cell), its
nodes (each zone checked first), its fading nodes (their pose and zone set
first), then the world's own children at the point as given. The walk
0x004eddb0 tests a node's model with 0x0054be40 (the cached polygon first,
then the rest; a strip as its triangles) and **records one polygon per
node**: the first whose plane at the point is no higher than the point's y
(0x0054d640: inside within 0.0001 of every edge; a vertical polygon takes
its first vertex's height; facing down counts, unlike 0x004ee2f0's). Its
box test 0x004f12e0 uses the box through the matrices axis aligned again in
x and z (0x004ec430 with 0x00523040). Class 8 tests its own model before
its matrix goes on the stack. The record holds 32; the walk's entry stops
at 32 with "Database intersections array is full", the model tests do not
check (FIX (A) below). Returns 1 when nothing was found.

**The spawn 0x004782f0** (pilot.cpp's neighbourhood, `place_pilot_mech`):
0x004161c0 on the pilot's mech (+0x14), its Invincible flag (+0x630) from
the cheat byte [0x00608e40] or the option 0x0040cf20, one update of 1/30 s
(vtable +0x34) with detail bit 0 (the sounds) off, then +0xdc 0, +0xde 1,
+0x587 1: **the player's mech starts the mission powered down**, the HUD's
saved state 1; the weapons' fire mode (+0xd4 +0x1c) from the pilot's +0x80.
**The power-up** is Core's resume(7) (0x00481170, game-loop.md), when the
mission's fade in ends: zInput reset, then for the pilot's mech (unless the
pilot's +0x59) 0x0041ca30(0) (the HUD's fall flag +0x638: the player's mech
takes the value, any other 1) and **0x0041cc30(0), the power-up**; Core's
enter has meanwhile played the "mech_powerup" sound. So every mission
starts with the reactor coming on as the screen fades in; the power key
(the command at 0x0041c077) toggles it after.

**Findings**

- **The step quirk**: the pick's reach is 2y + 10 for a mech placed at y.
  For y = 0 that is the intended 10 above; for the Mad Cat's c1m1 place
  (y -35.9) it is -61.8, below the ground (-35.86), so nothing is "in
  reach" and the lowest polygon is taken, the same one here. Where
  polygons stack (a bridge over a valley, a roof over a floor) a mech
  placed below 0 takes the lowest surface, one placed high may take a
  surface up to y + 10 + y above it. `test_place.py` finds such places on
  the shipped worlds (237 of 3600 placements over stacked polygons).
  Kept: fixes.md, Candidates.
- c1m1: the Owens' place (-1767.7 3 -1229, yaw 30 degrees) is flat ground
  at 3, so it stands at 3.01; the Mad Cat starts at (-3200.9 -35.85
  -1225.1), 1433 m from it, beyond its 1000 m sensors, **on the seabed
  under 4.86 m of the ocean** (`ocean67` at -31 above the ground at
  -35.86; the pick leaves water out): the campaign opens with a wade
  ashore.

**FIX (A)**: a model test past the record's 32nd slot would write past it
in the exe (only the walk's entry checks); the port stops at 32. No shipped
place comes near (a point under more than 32 nodes' polygons).

**The port and its test.** `zcls::ground_point` (cls_di.cpp),
`zmod::model_ground_point` / `polygon_ground_point` (gmod_hit.cpp),
`zcls::node_box_extent_xz` (class.cpp), `zmth::transform_box_xz`,
`mech3::ground_pick` (veh_basic.cpp), `Mech::place` / `set_place` /
`reset_at` / `entity_moved` (mech_place.cpp), `Collider::place`,
`Skis::reset`, `place_pilot_mech` (mission_launch.cpp). Calls out:
`place_calls()` (the query, the attach), `collide_calls().set_position`
(vtable +0x18) and `update_calls().moved` (0x005a85c0) when set; unset,
the ports on `world_hooks().world`. The program wires player.cpp's
`mech_place` and the launch's `pilot_place` to them; `node_add_child`
now takes a world parent (0x004f2a40, already ported).

`tests/differential/test_place.py` (in run_all.sh), on the six worlds:
points over each world (on and off the grid, wrapping or not, zone mode 0
and 1, heights around the ground; the camera and the sun added as world
children for classes 1 and 9; half way, strips and boxed nodes as in
test_di), each record, result, cached polygon and node flag bit for bit;
the pick on each record and on crafted ones (water, no material, exactly
step above, the surface exactly step below); a placement at each point on a
mech image (its fields, matrix, controls, collider, skis and ski matrices,
and every call out with the zone mode the query saw); 3000 crafted polygons
for 0x0054d640 (walls, slivers, points on vertices and edges); 200 spawns.
7200 queries, 7200 picks, 3600 placements identical.

Break tests (`test_place.py 100` had to fail): 22 of 30 caught. Not
caught: the cell's zone check left to the walk, the xz box writing y, the
controls kept by SetPlace (all three equivalent); the wrapped point not
moved back and the world's children tested as only children (no world
child has a model or a box that matters on the shipped worlds); a lod at
exactly 5 skipped (none is); the combined box matrix's [8] summed in
0x004ec150's order (a rounding difference not reached). Caught: the edge
tolerance, a vertical polygon's height, a polygon exactly at y, the
cached polygon, strip winding, the zone, the box's max z (both paths), the
pick's boundary, its in-water boundary, its fallback, its water flag, the
0.01's sign, the step of 10 (the fix candidate), the zone mode, one
0x005a85c0 instead of two, +0x58 kept, the collider's last points, the ski
probes swapped, the right ski not levelled, the spawn's update with
sounds, the fire mode, the HUD state.

`mw3tests`: `mission_start_reads_c1m1_vehicles_headless` (the Owens on the
ground at 3.01, in the world, its yaw), `mission_c1m1_sensors_fill_the_contacts_headless`
(the Mad Cat placed and powered down by the spawn, 1433 m out and not
sensed, then moved 300 m short of the Owens with SetPlace).

Left: the classes 7 and 8 and fading nodes of the point walk are not on
the shipped worlds (they mirror 0x004ee2f0's, test_di); a mech's polygons
are found only once mechlib nodes carry their models as `model`
(Node::lib_model, cls_mechlib); the vehicles' vt+0xdc; zEntity's marker
nodes (0x005a8390: the program's stand-in uses the mech's own node).


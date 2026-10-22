# Building a mech: the constructor chain and the launch

Mech3.exe 1.2, `C:\zipper\Mech3\mech.cpp` (0x004140c0..0x00416200 and the
pieces it reaches in `mech_parts.cpp`, the skis, `mech_data.cpp` and
`mech_reader.cpp`), and `mission.cpp`'s launch 0x00491150. Ported as
`mech3/mech_build`, `mech3/mech_part_build` (the parts' constructors and
armor), `mech3/mech_skis` (the skis' constructor), `mech3/mech_control` (the
player's controls) and `mech3/mission_launch`; checked against the exe by
`tests/differential/test_mech_build.py` (section 10). 2026-10-20.

## 1. Who builds a mech

- **The player's**, at launch: the briefing's Launch (0x0040500a) calls
  0x00491150 on the missions object (section 8), which takes the pilot's
  game slot (game vt+0x34(0): +4 the roster index, +8 the configuration, +0x10
  the skin) and calls **0x004140c0** with the mech data, the player's controls
  and the slot's configuration, named `player`.
- **An AI mech's**, as the mission comes up (player.cpp 0x0049fa20,
  mission-start.md section 4): **0x00414150**(roster index, controller,
  configuration index, name): the mech data (0x00430d10(index, create 1)),
  the definition's configuration of that index (0x00432950, the first when
  that one is missing), then 0x004140c0.
- 0x004140c0(data, controls, configuration, name) refuses without any of the
  three, else `new(0x6d8)` and the constructor **0x004149f0**.

## 2. The constructor, 0x004149f0

1. **The bases** (with no node, no team): ZEntity 0x005a8310 (+8 and +9
   set, the nodes +4, +0x10, +0x14 cleared), ZEntity3d 0x005a7e10 (four zero
   vectors from +0x18, the orientation an identity quaternion, +0x58, +0x64),
   AiTarget 0x004d56e0(0, 0) (+0x68 the height 10, a vector of two nulls at
   +0x88 (`new(8)`), +0x9c and +0xa0 1000 and +0xa4 their square, the low
   nibble of +0xbc cleared, team 0's masks: mask 1, allies 0, enemies
   0xfffffffe, [0x0060b56c] cleared), Vehicle 0x004aeb00 (+0xd4 the weapons,
   +0xd8 0), each storing its vtable over the last (Mech 0x005bcef8).
2. **The collider** 0x004222e0 on +0xfc: its lists empty, restitution 0.1,
   the yaw kick 0.01, 11 probes, their positions zero, an identity matrix at
   +0x340.
3. **The mech's fields**: four empty `std::list`s (+0x380 the parts, +0x47c
   the actions, +0x674, +0x6cc; each `new(0xc)`, its allocator byte the
   low byte of whatever the register held), the configuration at +0x3ec,
   `damaged` (+0x410) **set**, speed factors 1, jump fuel 1, anim state and
   previous state 1, the screen state saved 1, the collides flag, the heat
   sinks (3, 0, 0, then the mech's tech 0x00484720, 10 sinks, singles), the
   data at +0x684, the level of detail -2 (0x0041e9e0), the limp -1, +0xf8
   -1, the last hit time -1.0, anim slot 2, every gait, action and sound
   slot empty, the dissipation 0.02, and the knockdown constants 0.47,
   0x3ec08313 (0.37600002, not 0.376), 15.04, 3.76, 11.28 (damage.md). The
   tonnage (0x00416770) is compared with 10 and asked again; nothing keeps it.
4. **The controls** (+0x37c) and their IControls vt+0x08: the owner.
5. **The weapons**: `new(0x94)` WeaponManager 0x004b00d0 with its five
   groups (`new(0x54)` each, 0x004b1d80; weapons.md).
6. **The model and parts** (section 3); when that fails, the message box
   0x004ac150(0, 0x1e0, 0xf25, name). Then +0x68c: `new(0x24)` and
   0x004e61e0(mech, &mech+0x380) (the AI's), the knock vector zero.

## 3. The model, 0x00415b00, and the parts, 0x00415e40

- **0x00430c60** (mech_data.cpp): a copy (zClass 0x004f9af0(node, 1, 1)) of
  the data's model's first child (+0x14: mechlib's `mech_<name>.flt`); with
  the definition's skins, their decal state (0x00475a80(skins, 0, node)) at
  +0x3e8. No copy or no configuration: "Failed to copy Mech Database Tree"
  (zerr 0x400, line 0x3d3) and no mech parts.
- +0x694 the copy's first child; +0x698 **0x00430c20**(1): a copy of the
  low-detail tree's grandchild (`mech_<name>_1.flt`, the data's +0x1c).
- [0x00605628] counts the mechs made; the entity's name is the caller's,
  else `<type><nn>` from that count (`madcat07`). 0x0041de80 gives zEntity
  the node and name (0x004d6c90: the node's matrix +0xc, a centroid +0x14
  and a target node +0x10 made under it, the node's entity) with the +0x68c
  object off its list meanwhile (null here), and keeps the target at +0xe8.
  The markers (0x005a8390, `zent::marker_nodes`, 2026-10-21): the nodes
  named "centroid" and "target" in the mech's tree (0x004eb0d0), else
  object3ds of those names made under its node; the build then puts them at
  half and 0.65 of the mech's height and refreshes their matrices (0x005a8500,
  `Mech::refresh_markers`). Before, the program's stand-in used the mech's
  node for both, so the build's two translations moved the node itself and
  everything that aims at a mech aimed at its feet. The AI targets' set-up
  (app_mission) uses the same function.
  0x005a7f50 puts the mech on zEntity's list (0x00736558). 0x00415ae0: the
  node's owner (+0x40) the mech and its id (+0x28) 0x1000.
- **The parts** from the definition's slots (MechDfn +0x28, by part id): hip
  (`new(0xb0)`, 0x004620a0), torso (0x1b4, 0x00464000), head (0x74,
  0x004671a0), left and right arm (0xd0, 0x00466280) each with its hand
  (0x90, 0x00466da0), left and right thigh (0xd4, 0x004635e0) each with its
  calf (0x74, 0x00466ee0) and foot (0xec, 0x00462590). The set-ups (vt+0x04)
  run torso, hip, arms, legs, head; movement.md has them.
- **A part's base** 0x0045fbe0: the slot's name (or the part's own, 0x00484cc0)
  looked up under the mech's node (+0xc) and under the low-detail copy
  (+0x10); 0x00460190 makes the first the part's node when it is an
  object3d (its matrix +0x14, +0x1c, +0x20, +0x24, translation +0x18, +0x28);
  0x00460050 marks both trees: every node from the part's down takes the
  part's id (+0x28) and the part (+0x40) and loses its action; then a node
  reference `new(0xc)` (0x004e6250) at +0x68. Structure and armor 0, the
  structure's scale 1, the pose an identity.
- **The torso**'s pose and aim at rest, facing -z; its look limits default
  to -12 / +20 / 100 degrees (0xbe567750, 0x3eb2b8c2, 0x3fdf66f3) until its
  set-up reads the definition's. **A foot** sinks its sole to the box's
  bottom (-min y, 0 when that is negative or NaN), hides its shadow marker
  `<c>shadow` (c the node name's first letter; placed at the sole, its
  model's priority 10) and drops it, and counts its toes: the children
  whose names start `<c>toe`, then finds `<c>toe01`.. (`calloc`s for their
  nodes and rotations); ground state 3, +0xbc -1.
- The **AMS** (`new(0x1c)`, 0x004b3ab0: range 150, period 0.6, 100 rounds;
  weapons.md) at +0x3f0; **the firepoints** (0x0046cb80, mech_reader.cpp):
  each of the definition's finds its node under the mech, takes the id of
  the part that owns it (0x0040ea10) and its translation (0x004fc020).
- zClass's pending nodes (0x004f7e50), the node's flag 8 off, the **height**
  (+0x68) from the node's box (0x004ebd50: max y - min y; 12 without a box),
  the centroid placed at half of it and the target node at 0.65 (each
  followed by zEntity's update 0x005a8500), the low-detail copy added under
  the node just while zWeapon's hit handler (0x005833b0 with 0x004161a0, the
  mech's vt+0x164) is set on it, the motions (section 5), the collider's
  set-up (0x00422360), the configuration (section 6), the jump jets' node
  `butt` (+0x3a8).
- Back in 0x00415b00: the 25 sound slots cleared again, and every node of
  the tree loses its action (0x00415c70).

## 4. The parts' armor, 0x0045fe00 and 0x004644b0

vt+0x08 with a configuration (the set-up's call passes none, which changes
nothing but the torso's side structure): structure and armor whole (1.0),
the structure's scale 1 / (points x 0.01) from the table at 0x005e6998 by
the mech's tonnage (0x00485400 centre torso, 0x00485430 side torso,
0x00485460 arm, 0x00485490 leg; the head 3; 1 when none), the armor's from
the configuration's slot for the part (100 when 0). The torso also takes
the side and rear sections 1-5 from slots 1-5, and its two side torsos'
structure (always, the set-up's call too). damage.md has the slots.

## 5. The motions and skis, 0x00415ca0

The data's gaits 0..6 and 12 (0x00431fb0) and one-shots 1..8 (0x00432030)
copied, four blends between neighbouring gaits at slots 7..10 (0x0041ff30:
stand-step, step-walk, walk-trot, trot-run), slot 11 empty, then **the skis**
(`new(0x300)`, 0x00476320) half the feet's x apart, 4 m long for the
Elemental and 12 for every other mech: under a `ski base` object3d added to
the mech's node, each ski (0x00476090: `left ski`, `right ski`) gets its
three probes (front, middle, back at -half, 0, +half), a debug model of two
quads (a 1 m wide flat one and an upright one along the ski, model priority
1), flag 8 off, and is levelled (0x004764a0); both are then hidden.

## 6. The configuration, 0x00414430

Refused (false, nothing changed) without a configuration, when it fails its
check (0x004254d0(0)) or belongs to another mech. Else: the configuration's
+0x70..+0x78 at +0x51c (the turn rate at +0x520), its throttle and turn
rates at +0x530..+0x538, the equipment counts cleared, the top speed
(0x004143c0: the engine's 0x00433030 by the configuration's tonnage), +0x514
0 (0x00424290 returns 0), the weapons and equipment (0x00414501..0x00414732,
`mech3::configure_weapons`, weapons.md: the manager remade, the weapons
mounted, the configuration kept at +0x3ec, ammo, jump jets, the AMS's
rounds, TAG), the heat sinks (+0x64..+0x6c, a dword copy), the equipment
items' +0x08 cleared (0x00425960), each part's armor (section 4) in the
order thighs, arms, head, torso, the model's and low-detail copy's
`firepoint00`..`firepoint99` markers removed (unlinked from their parent,
hidden, freed), and the first weapon group with weapons selected (0x004b0730;
group 0 when none has).

## 7. The player's controls, 0x00427280, and the control mode, 0x0041cf30

- 0x00427280(mode) makes the controls once ([0x006056ec]; the destructor
  clears it): the 95 commands whose keys reach the controls get the callback
  0x00426cb0 (0x00426ce0: zInput 0x0051d2f0), then `new(0x160)` and
  0x00427320(mode, most derived): the base 0x004265a0 (view state 2, every
  input and target 0, the throttle returning (ThrottleConst off), the hat at
  1, +0x58 0, +0x59 1), the AI's base at +0x5c (0x004d9840, not ported),
  the IControls base at +0x14c (owner +0x15c), the mode (0x00426650,
  controls.md), the mouse look 0. A later call only sets the mode.
- 0x00426cb0 hands a command to the controls (vt+0x1c, 0x004274d0) and to
  the vehicle the view looks at (vt+0xfc, the mech's 0x0041be80).
- 0x0041cf30(mode): mode 4 without a joystick (0x0051dc10) becomes 3; with
  one, the torso's input is absolute when TorsoAutoReturn is 1; mode 1
  makes it relative. The view's camera keeps its mode (0x0040ae70 with the
  camera's +4), then the controls' mode.

## 8. The launch, 0x00491150

The loader thread's handles closed (+0x68, +0x6c; the port has no thread,
mission-load.md), 0x00491d20, the `player` record of aiv.zrd (none: false):
its place, yaw (+0x14), team (+0x18). The controls (mode 6 when the
UseJoystick option is 1, else 3), the camera mode (0x0040ce70), the slot's
mech (section 1); none: the message box 0xf30 with the mech's name, false.
The slot's skin (+0x10) of the definition's (0x00414390, with the skins'
0x004757f0), the team masks (as player.cpp's), the control mode, the view
attached (vt+0x108, 0x0041ce30) and the camera set twice around the pilot's
vehicle and place (0x004782c0, 0x004782f0, ported: mission-start.md section
11; the second attach with zModel's
zone mode off), the interpreter's mech (0x004108a0), the HUD reset (screen
+0x17964 vt+0x64), the controls given the record (IControls vt+0x88), the
camera mode set again, the game type; the cockpit's `cpit_ania`..`cpit_anic`
nodes (the data's +0x228 block) each run `support\cpit_cycle<A..C>.gw` with
the node as the script's current node, and the textures load when any did.
The pilot's +0x59 is the record's p12 second flag outside multiplayer;
[0x0060b570] cleared. By type: 1 the campaign's lancemates (0x0049e5e0) and
0x00478f10(0); 2 instant action's line-up (0x0049dc80) and its objectives
(0x0045c4a0 on +0x7c..+0x88), 3 multiplayer's (0x0046da80), both then
0x00478ce0, 0x004794f0 (vt+4 when set) and the pilot's handle +0x10 closed;
4 the HUD's zoom -1 when the view is on the pilot's mech. Then 0x004d6d20
with the mech's +0x9c, the HUD's 0x0043d920, the list at +0xc0 freed (each
entry's object, then the entries), 0x0040f0a0, 0x00537520: true.

## 9. The port

- Every call out of the chain is a `MechBuildHooks` method named by address
  (`mech_build.h`), the launch's a `LaunchHooks` one (`mission_launch.h`,
  apart from `mission.h` because its objects are the mech's pointers). The
  ported code the chain reaches (the parts' set-ups, the collider's, the
  skis' levelling, the configuration's check, the weapons, the gaits'
  copies, zInput, the options) goes through hooks whose defaults run the
  ports; zClass's nodes default to the ported zcls nodes.
- The port's objects are C++ (`Mech`, the parts, `WeaponManager`, `Ams`,
  `Skis`, `PlayerControls`); each `new` of the original is an `on_new` hook
  (silent in the program) so the test can give the port's objects the
  original's addresses. The bases, the collider and the parts' constructors
  set exactly the fields the exe's do; fields the port did not have are
  added with their offsets (`mech.h`, `mech_parts.h`, `mech_skis.h`).
- **The program** (`app_mech.cpp`, our addition): the mech data is the
  port's (0x00430d10, section 14), its trees mechlib's through the ported
  library (mechlib.md); each mech's model and low-detail model are zClass
  tree copies (0x004f9af0(node, 1, 1): its own models and materials); the
  node removals, tree delete, model set and pool, subtree update and pending
  nodes the chain asks for are the ports; zEntity's node set-up keeps the
  node and its matrix. `mw3 --mission c1m1` launches after
  the load (standing in for the briefing's Launch): the pilot's slot is the
  Mad Cat in its first configuration (`GameEngine::Launch::mech`), and the
  log says what was built. player.cpp's `mech_create`, `mech_set_skin` and
  `mech_dfn_of` hooks call the port (`build_ai_mech` ...) on the AI's
  controllers (0x004d8750, ai.md section 12), so the mission's AI mechs are
  built as it comes up (c1 mission 1: owens_01, mission-start.md section 7).
- `mw3tests` `mech_build_launches_the_player_c1m1_headless`: c1 mission 1's
  launch without a window builds a Mad Cat with its 13 parts on their nodes,
  ten weapons, 17 heat sinks, six toes, 10.9 m tall, 22.5 m/s, its centre
  torso's 36 armor points, on team 1, with skis and AMS.

## 10. The test

`test_mech_build.py` (probe `tests/probe_mech_build.cpp`): 240 cases, three
in four the launch (one in five of those with the controls already made),
the rest 0x00414150, on all 19 mechs: their definitions read from
reader.zbd by the ported reader, every shipped configuration (the first 76
cases cycle them), crafted ones (engine, sinks, double sinks, sink tech,
armor slots, the rates, the mech, +0x70/+0x78), the real mechlib.zbd trees
of both models laid out as fake nodes; the cockpit block or none, the
low-detail model or none, markers, shadows, toes and `butt` missing, the
box query failing, gaits failing to copy, the configuration failing its
check or belonging to another mech, the record missing, no mech data, no
configuration, a definition without skins (0x00414150 only), game types -1
to 5, the options, the joystick, the view on the mech or not, the
missions' list of 0..3 entries, the weapon counts. The original runs first,
every call out of the chain stubbed (the weapons block of 0x00414430 jumped
over, test_weapons_run.py checks it), the x87 at 24 bits; the port then on
a Replay of its answers. Compared: the call traces (strings as text), every
block the original allocated (the port's object laid over it: each byte the
original wrote a field the port sets alike, each other byte the allocator's
0xcd), every byte written to the fake objects (the nodes' ids and owners,
the firepoints, the pilot, the interpreter, the list), the globals
([0x00605628], [0x0060b56c], [0x005ea1d0]); the view's attach logs the zone
mode it sees. Three in ten cases craft the feet (a box bottom above the
origin or NaN, children named nearly like toes). 148,784 calls and 10,240
blocks identical. Breaking the port showed the test catches it, 33 of 33:
the part base's structure scale 0, the torso facing +z, toes matched on
three letters, a sole kept below zero, the head with 4 structure points, no
armor scaled 1, the marks keeping the nodes' actions, the side torsos'
structure from the arm column, `damaged` clear, the knockdown constant
rounded to 0.376, the owner set after the weapon manager, the hip set up
before the torso, the target at 0.6 of the height, the blends one gait off,
unnamed mechs numbered without the width, the low-detail model's child
instead of its grandchild, another mech's configuration taken, the last
weapon group with weapons selected, the markers to 99, +0x70 copied from
+0x78, the Elemental's skis 12 m, the skis' flat quad 2 m wide, the skis in
the air, joystick mode for any UseJoystick answer, allies keeping bit 0,
the zone mode kept for the second attach, instant action's objectives
swapped, the pilot's flag in multiplayer, the pilot's handle closed at -1,
the controls in view state 0, ThrottleConst on, one command not registered,
the structure table one row off.

## 11. Findings

- **Every mech carries its low-detail model's parts**: each part finds its
  node in both mechlib trees and marks both, and the low-detail copy hangs
  under the mech's node while the hit handler is set, so hits on either
  model's nodes reach the mech.
- **Unnamed mechs are numbered**: `<type><nn>` from a count of every mech
  made since the program started.
- **A mech is "damaged" from birth** (+0x410 set by the constructor).
- **The knockdown threshold's constant is 0.37600002**, not 0.376.
- **The skis are 12 m long, 4 m for the Elemental**, between the feet; each
  carries a hidden debug model (a flat and an upright quad).
- **The torso's look limits start at -12, +20 and 100 degrees** until its
  set-up reads the definition's.
- **The head always has 3 structure points**, whatever the mech's tonnage.
- **A joystick only counts if the UseJoystick option is 1**: the launch
  picks control mode 6 then, else 3 (keyboard and mouse).
- 0x005a7f50, once read as moving zEntity's centroid and target onto the
  mech's node, only adds the mech to zEntity's list (0x00736558); the two
  nodes stay, at half and 0.65 of the mech's height.

## 12. Quirks and fixes (fixes.md)

- A definition's firepoint whose node is missing from the model, or whose
  node no part owns, reads through a null part in 0x0046cb80 (a crash).
  FIX (bug): its part stays -1. No shipped mech has one.
- The launch reads the definition's skins without checking for none, and a
  foot without its node is read through (0x00462620): crashes; the port
  checks. Every shipped definition has skins and every model its feet.
- With the view on another vehicle at the end of the launch, the exe passes
  0x14 (no HUD + 0x14) to 0x0043d920, which writes through it (a crash).
  FIX (bug), behind `bugs`: skipped; off, as the exe (the test's path). It
  does not happen in play: the launch attaches the view first.
- Allocations: the port's `new` does not fail; the exe's null checks after
  each `new` (a part, the manager, the AMS, the skis) are not reached.

## 13. Left

- The mech data's camera kinds 0x00430e70 (the 0x421530 objects), the HUD's
  model object 0x00430aa0 / 0x00430ad0, the canned effects' spawn 0x00432830
  (and so 0x00431800 has nothing to take down), the skins' state on the
  model 0x00475780.
- zEntity's record (0x004d6c90 / 0x005a8390 beyond the markers) and its list
  0x005a7f50; the hit handler 0x005833b0.
- The mech's destructor 0x00414f40 (its view attach 0x0041ce30 is ported:
  view.md), the AI side
  (0x004e61e0, 0x004d6d20), the HUD calls of the launch, the lancemates
  0x0049e5e0, instant action 0x0049dc80, multiplayer 0x0046da80.
- The AI mechs are built (ai.md section 12) and placed (0x004161c0,
  mission-start.md section 11); the AI side of the mech (0x004e61e0,
  0x004d6d20) is left.
- The mission's unload 0x00490480 (which clears the mech types, 0x004318e0,
  closes mechlib and resets zClass between two missions of one run) is a
  hook: the program's runs are one mission each.

## 14. The mech types' data (mech_data.cpp, ported 2026-10-20)

One 0x234-byte object per mech type (`MechData`, mech.h), in a cache
(0x00605704, a `std::list`, count 0x00605708):

- **0x00430d10 (ECX roster, EDX create)**: the cached type, else with
  `create` a new one (`new(0x234)`, the constructor 0x004303d0); +8, its
  count of holders, goes up either way. It returns 0 when the load failed,
  **but the failed type stays cached** and is handed out (as 0) without a
  second try. The exe also builds and destroys an empty one on the stack
  around the lookup (0x004303c0 / 0x00430580), which leaves no trace.
- **0x004303d0**: 0x00430490 (the defaults: roster -1, the gaits' throttle
  bounds stand 0..0.01, step 0..0.04, walk 0.02..0.45, trot 0.41..0.85, run
  0.75..1, the limps' stride speed 13, the five canned slots' offsets the
  zero vector), +0xc the roster, +0x10 its name (0x00484650), the loader,
  then onto the cache (`new(0xc)`).
- **0x004308a0** clears +0x20 and reads both mechlib trees (0x00484760,
  variants 0 and 1), setting the intersect flag on each (0x004eb6d0, also
  on a null one: a report); +0x14 the full one. Without it: the error box
  (0x004ac150, 0x1e0, 0xf2c, the name). Without a definition: "Unable to
  setup (%s): invalid data file" (0x800, line 0xe2). Else +0x20 = 1, the
  skins' state on the model (0x00475780, when the definition has skins),
  +0x1b0..+0x220 cleared (the part effects, death effects, splashes, stub
  nodes), the ranges (0x00430840: the definition's +0x14c and +0x150
  squared), then while +0x20 holds: the cameras (0x00430e70), the sounds
  (0x004306d0), the animations (0x00431a70, movement.md), the effects
  (0x004320b0); the soils (0x00432670) whatever happened; a failure shows
  the error box with no name (0xf2d).
- **The sounds** (0x004306d0), 25 slots at +0x28: engine, engine_water,
  limp, jumpjet, servo, heat_warning, heat_warning1, engine_up,
  engine_down, lockon_warning, eject, crouch, landing, collide,
  jumpjet_start, jumpjet_stop, radar_up, radar_down, falling, fall,
  shutdown_warning, shutdown, shutdown_off, zoom (each `mech_<x>`, the
  radar ones without), and slot 24 `mech_<name>`.
- **The effects** (0x004320b0), by name through the animations (0x0050b450):
  the four death effects `<name>_disable`, `_destroy`, `_fwd_disable`,
  `_back_disable`; `<name>_landing_effects`, `mech_splash`, `nuke_splash`;
  per thigh and arm `<name>_<part>_destroy`, `_detach`, `_stub`; the stub
  nodes `larm_stub01.flt` .. `rleg_stub01.flt` by name on list 6 (0x004f7fe0);
  `cpit_effects_explosion`. **In instant action and multiplayer** (the
  pilot's game type 2 or 3, 0x00479460 / 0x00479480) every one but the
  landing and splashes takes its "m" form (`_mdisable`, `_lthigh_mdestroy`,
  ...), except for the Elemental.
- **The soils** (0x00432670, `new(0xb0)`, 0x004326d0): for the 11 soils
  (default, water, grass, dirt, mud, concrete, snow, mech, lava, silt,
  noslip, 0x0047f380) the step sound and the footprint named `<name><Soil>`
  (`madcatDirt`), each missing one `soil<Soil>`; the footprint scale 1, grass
  0.9.
- **The canned effects** (0x00431750, from 0x00431a70 and the refresh): the
  definition's `animations` / `canned` blocks `eject`, `jumpjets`, `narc`
  into +0x14c + 0x14 i: the effect named first in the block, its `ofs`, its
  `node` found by name on list 6.
- **0x004319a0**, at each mission's load: on every cached type 0x004319f0
  makes the soils again and reads the canned effects and the sounds again
  (they belong to the mission's sound and effect sets).
- **0x004318e0**, at the mission's unload: each type's destructor
  (0x00430580: off the cache, the HUD object, the animations and actions,
  the cameras, the soils; the block at +0x228 is the HUD's) and delete,
  then 0x00431800 takes down the canned effects' node copies (made by
  0x00432830).

The program: `mech_data()` replaces app_mech.cpp's own assembly; mechlib
opens with the mission's load (0x00484270) and the types refresh with it
(0x004319a0); the zClass reset (0x004f8f40) and mechlib's close (0x004844b0)
run at the stop.

**Quirks.** A type whose definition fails to load stays cached; at the next
mission's refresh the exe reads its reader through null (0x00431a20): FIX
(bug), the port reads none. The trees and models it copies are mechlib's,
shared by every type's mechs (each mech copies them, section 3).

`test_mech_data_load.py` (probe `tests/probe_mech_data.cpp`): 152 sessions,
all 19 mechs, each a random run of lookups (create or not, the same type
again, others), refreshes and clears, with types missing their model, the
low-detail model, the definition or skins, the animations failing the step,
the game type, and sounds, effects, nodes and canned blocks missing at
random. The original runs first with every call out of mech_data.cpp
stubbed (CString emulated), the port on a Replay: 32,824 calls, every
result and the cache's order, and 550 live data and soils blocks laid over
field by field identical. Sessions that refresh keep every definition (the
exe crashes otherwise, above).

## 15. Taking a mech down (ported 2026-10-21)

The cleanup (0x00491780) lets the pilot drop its mech first (0x004782c0
with 0: the weapons' fire mode, +0xd4 +0x1c, kept at the pilot's +0x80,
+0x14 cleared; the port also clears the mech globals the launch set), then
deletes that mech (its vtable +0 with 1),
then 0x0049dc40 runs zEntity's 0x005a8280: each entity's vt+0x30 (a mech's
0x004b3d30 returns 1 and does nothing), then deletes the first entity
until the list is empty (each one's destructor takes it off, 0x005a7f70).

The Mech's destructor 0x004152d0 (`Mech::release`, then the delete in
`mech_delete`): for the pilot's mech the +0x378 object (0x0041e580); then
the body 0x00414f40: the flown parts' nodes off their parents and destroyed
(0x00413db0), the decals (+0x3e8, 0x00473f40), the cockpit off the torso
(0x0041ce10(0)), the view let go when it looks at this mech (0x0040adf0(0,
0)), the contacts (0x004a7be0), the 25 sounds stopped, the 30 cockpit
sounds (0x0041e7a0), the node's handlers removed with the other model
briefly under it (0x004eb9b0, 0x00583400, 0x004ebb40), the weapon manager
(0x004b01d0), the parts by vt+0 in the order hip, torso, legs, arms, then
the skis (+0x41c, 0x00476460), the head, the AMS (+0x3f0), +0x94, the 13
motions and 9 actions (0x005a9dc0), the type's data when no other mech holds
it (0x00430e30's count, 0x00430580), the parts list, +0x68c (vt+0), the
controls when their vt+0x18 says so, and with a node: out of the world (one
parent), the two models off it, renamed `deletedmech` (0x005a84c0), the
three trees destroyed (0x004f9050), vt+0x04(0, 0, 0, 0) and 0x005a7f70. A
mech without a node is never taken off zEntity's list by it.

The parts (`part_delete`: release, then the delete): the base 0x0045fd40
stops the trail and debris effects, frees the node reference +0x68
(0x004e66a0, 0x005ac5c0), renames the node `deletedpart`, clears its +0x2c
bits 5 and 6 and its ids and owners down the tree (0x004600b0); the torso
(0x00464250) stops its death effects first; the thigh (0x00463740) deletes
its calf and foot; the arm (0x00466380) its hand; the foot (0x004628c0)
stops and frees its wake and frees the toe arrays.

In the port the C++ destructors stay empty (the tests build parts on the
stack); `release` is the exe's body. The program wires the outside calls in
`app_mech.cpp` (FreeCalls) and the deletes in `app_mission.cpp`
(delete_object, fn_49dc40); the AI's controls go through the player hooks'
controls_delete. Checked: the parts' destructors in `test_deaths.py`
(release, 400 cases, BREAK=11); the whole teardown by
`missions_c1m1_c1m2_then_c2m1_in_one_process_headless`, which finds no mech
alive after each cleanup (`mechs_alive`), and by an ASan run of the suite.
The leftovers (2026-10-21): the cockpit's removal 0x0041ce10 is
`Torso::set_cockpit` (0x00464580: the type's cockpit node, MechData +0x228
[1], off the torso node with its ids, and with `on` back under it as part
1's), wired for the teardown and the view; it acts once the block
0x00430ad0 makes is ported (null till then). The +0x378 object is
multiplayer's: commands 0x76 / 0x77 (0x0041e4e0) make it (0x0041e5a0, a
zTwoD panel, vtable 0x005bd070) only when networked, so the teardown's
0x0041e580 stays unwired with the network. In 0x0049dc40, the player's tick
object [0x00609a00] loses its action and is released (0x004eb450,
0x004f3390 -> 0x004eb050); 0x00402560 frees the AI nets 0x00402170 made (the
AI's, logged). Checked: `test_deaths.py` cockpit (200, BREAK=16); the
three-mission test finds the tick object gone after each cleanup.

## The detail scale (0x00430440, ported 2026-10-21)

The options' object detail (0x0040d310) gives the mechs a scale (High 1,
Medium 0.75, Low 0) in [0x005e2928]; 0x00430440 stores it and redoes every
cached type's ranges (0x00430840: each range times the scale, squared, at
the game's 24-bit x87). `mech3::set_lod_scale`, checked in
test_mech_data_load.py (scale changes between the sessions' gets, at 24
bits; at 53 bits the unrounded product differs in the last bit).

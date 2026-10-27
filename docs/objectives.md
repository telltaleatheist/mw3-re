# The mission objectives (mech_objective.cpp): map and porting plan

Mech3.exe 1.2, scoped 2026-10-21 (mac 2) after the c1m1 census (playtest.md,
"Unported in c1m1"): nothing of it is ported, so a mission can be neither won
nor lost in the program.

## What is where

The code sits between mech_map.cpp and mech_parts.cpp, 0x0045b800..0x0045f9xx:
about 60 functions and 3,300 pseudocode lines. 23 of them (about 2,650 lines)
are reached by direct calls from the five entry points the mission uses; the
rest are the objective types' virtual methods (vtables 0x005c2660..
0x005c2730, two slots each: the destructor and the check).

| Entry (mission.h hook) | Address | Lines | What |
|---|---|---|---|
| `objectives_load` | 0x0045c2d0 | 23 | objectives.zrd's block for the mission into +0x7c..+0x88 |
| `objectives_start` | 0x0045c4a0 | 179 | at the start: each objective armed |
| `objective_check` | 0x0045e540 | 49 | each frame (mission-frame.md section 3) |
| (all done) | 0x0045c180 | 24 | the win's test |
| `objectives_free` | 0x0045c1d0 | 26 | at the end, and Instant Action's next wave |

Under them:
- **reading**: 0x0045c340 (98, a block's lists), 0x0045bf60 (101, one
  objective), 0x0045bc90 (156, its set-up; `mobilehq_0_d`), 0x0045b800 (31),
  0x0045b9e0 (59);
- **the condition parser** 0x0045c920 (1,376): one branch per type;
- **the check** 0x0045e110 (126: on completion the message panel 0x0040fe20 /
  0x0040fe70, the HUD's objectives box 0x00446ad0, salvage 0x00470bc0 /
  0x00470d20 / 0x00472cf0, sounds 0x0049ccb0 / 0x0049ce90, the valves
  0x004d3c00, the mission 0x0048f6c0), 0x0045eb70 / 0x0045ec40 / 0x0045ec60
  (95);
- **node_state** 0x0045e9f0 (125) with 0x0045c810 (41, "Failed to find
  node") and 0x004711a0 (131);
- **the types' checks**: 21 functions of 8 to 90 lines.

## The types and where the shipped data uses them

| Type | Uses (keyword count) |
|---|---|
| `destroy_entity` | c1, c2, c3, c4, c4b, t1 (137) |
| `valve_change` | c1, c2, c3 (+ mp3, mp4), c4 (159) |
| `node_change` | c1, c2, c3, c4, c4b (85) |
| `obj_list` | every mission (41) |
| `reach_area` | c2, c4b, t1 (8) |
| `weapon_fire`, `heat_warning`, `target_object`, `weapon_select`, `time_limit`, `face_object`, `reach_velocity`, `aim_at_object` | t1 (training) only (piece 5, below) |
| `view_object`, `key_press`, `mouse_press`, `joystick_press`, `weapon_group_select` | none shipped |

## Porting plan (pieces of a few hundred lines)

1. **The file and the objects** (~520): 0x0045c2d0, 0x0045c340, 0x0045bf60,
   0x0045bc90, 0x0045b800, 0x0045b9e0, the free 0x0045c1d0 / 0x0045c250 and
   the base methods. Test: every shipped mission's block read identically
   (the original against the port), and broken blocks.
2. **The parser for the campaign's types** (~400 of 0x0045c920):
   destroy_entity, valve_change, node_change, obj_list, reach_area and the
   fields every type shares; the rest kept as "type not ported". Test:
   every shipped condition parsed identically.
3. **Start and check** (~450): 0x0045c4a0, 0x0045e540, 0x0045e110,
   0x0045c180 and their helpers. The completion's effects are hooks: the
   message panel, the HUD box (mac 3), salvage, sounds, the valves, the
   mission. Test: random objective states through start and check.
4. **The campaign's checks** (~350): those five types' checks, with
   node_state 0x0045e9f0 / 0x0045c810. Then the wiring (the mission's hooks,
   +0x7c..+0x88), and a headless c1m1 that is won by destroying its
   targets and lost by dying. c1m1 can be won and lost after this piece.
5. **Training's types** (~550, after the campaign): the other parse
   branches and checks (t1's eight; the five unshipped ones last).

## The reading (pieces 1 and 2, ported 2026-10-21)

`src/mech3/mech_objective.{h,cpp}`; the engine calls go through
`ObjectiveHooks`. Checked by `tests/differential/test_objectives.py` (with
`tests/probe_objectives.cpp`): every block of every mission's
objectives.zrd as a campaign and as multiplayer (then freed), and 400 made-up
blocks, both sides on the same fake entities, nav points, nodes, valves,
sounds and messages; the objects and the order of the calls match.

### The file

The root holds one block per mission (`m1`, `m2`...; mpinfo.zrd's block is
`objectives`), found by the reader's depth-first search (0x0045c2d0). A
block: `desc` (the mission's text, localized into a calloc'd copy, the
missions object's +0x78), then `primary`, `secondary`, `final` and `fail`
(+0x7c..+0x88), each a list of `type (arguments)` pairs with its own
optional `desc` (default messages 0x16d, 0x16e, 0xf03, 0xf04), `sound`,
`visible` and `silent` (0x0045bf60). A list with no condition that reads
leaves its group unmade. The fail group's goal count is 1: one failed
condition fails the mission.

A campaign (game type 1) adds a fail condition the files do not have
(0x0045bc90): "the mobile field base destroyed", a group of the entities
`mobilehq_01..03` that exist and are active (entity +0x08), each a
destroy_entity labelled "MSG_OBJ_MFB_DESTROYED" (then renamed from the
entity's own name, message 0xde0). It goes into the fail group, or becomes
it.

### Every type's shared keys (0x0045c920)

`desc` (localized), `sound` (a zSound by name, +0x20), `visible` (+0x30,
default 1), `silent` (+0x31), `salvage` (a store, +0x10; 0x004711a0),
`cond_salvage` (`node_state` entries: a node, whether its `state` is
`active`, a store; +0x18; 0x0045e9f0), `persist` (a copy, +0x2c),
`set_valve` (`(name value)`: the valve and value, +0x24/+0x28) and `nav` (a
nav point by name, +0x04). All but salvage and set_valve are found by the
depth-first search, so a nested condition's key can answer for its parent.

| Type | Object | Reads |
|---|---|---|
| `destroy_entity` | 0x40, vtable 0x005c2668 (one name), or a group of them | the names after the keys; each entity by name (0x004d7330), relabelled "destroy <its name>" when it has one |
| `node_change` | 0x3c, 0x005c2730 | `node` (a world node, then subnodes by name: 0x0045c810), `state` == `active` |
| `reach_area` | 0x4c, 0x005c2728 | `radius` (25), then `node`, else `nav` (the nav point moved to the end of the nav list's +0x28 list, 0x0047a270), else `point`; none: no objective |
| `valve_change` | 0x3c, 0x005c26f8 | `valve` (multiplayer sets it to 0 as it reads), `value` (1) |
| `obj_list` | 0x4c group, 0x005c2678 | `list` of conditions, `goal_count` (+0x38), `simultaneous` (+0x35) |

The other fourteen types (training's and five no file uses) are piece 5's
(below, Training's types).

### Quirks (all kept)

- destroy_entity takes as entity names whatever follows its keys, counting
  two slots for each of desc, salvage, cond_salvage, visible, silent,
  set_valve and nav that it found. `sound` and `persist` are not counted,
  nor a `nav` that names no nav point: their slots would be read as names.
  No shipped destroy_entity has them.
- The salvage reader wants weapons as `(name tech)` lists and engines under
  `engines`. Three shipped rewards miss: c1m2's primary lists its 120 XL
  engine under `engine`, c2m2's secondary and c3m3's primary list their
  weapons as plain names; those parts of the reward are never given (their
  ammo, mechs and equipment are).
- An ammo count is not type-checked, an engine's XL flag is "the entry has
  no second item" (`(120)` is XL, `(120 "xl")` is not).
- The base's +0x14 is the low byte of the description's pointer, and a
  group's +0x40 a stray stack byte (the constructor reads its argument
  twice; the groups copy an unset local). Neither is read.
- The objects' destructors never free the persist copy.

### Changes (FIX (bug), `fixes.md`)

Only malformed files reach them: a condition whose type is not a string, an
entity name, salvage entry or tech that is not a string, an empty `node` or
engine list, an obj_list without `list`, a block `desc` that localizes to
nothing. The exe reads through the wrong type or a null; the port passes
the item over.

## The check and the completion (piece 3, ported 2026-10-21)

Checked by `tests/differential/test_objective_check.py` (probe entry
`probe_objective_check_run`): 300 scenarios of made-up blocks, Instant
Action's group over made-up worlds, 1..8 frames of every group's check and
the flash test; the types' own checks (piece 4) answer from a script both
sides share. Objects, the mission's salvage store and the calls match.

- **A group's check** (0x0045e540; as a child, vt+0x04 0x0045f150 with no
  announcement). Done stays done. It asks each condition in order and
  counts those met; with +0x34 clear it stops at the first not met (every
  group the reader makes sets +0x34, so in play all are asked; the test cannot reach the stop). A count
  above the best so far (+0x3c) is remembered and, from the mission's own
  check, plays the mission's objective voice (0x0048f6c0 with no sound)
  unless the group is silent. The group is complete when the count
  reaches goal_count (0: all). A simultaneous group that has some but not
  all clears its conditions again (0x0045e110(0) on each), so they must
  all be met in the same frame.
- **The completion** (0x0045e110): +0x08 = the result; when true and the
  pilot has a vehicle ([0x0060861c]+0x14: set to the player's mech by the
  launch 0x004913be and cleared only by the cleanup 0x00491872, so in play
  always, a dead mech included; in a campaign or Instant Action the
  player's death ends the mission's checks anyway, in multiplayer the
  game goes on with the same vehicle): the voice (+0x20 at priority 1, unless silent);
  when visible, the description's lines in the message panel for 5 s
  (the first as message 0x263 through 0x0040fe70, the rest through
  0x0040fe20) and one line per salvage item (0x264 with the item's label;
  cond_salvage's only where the node's active flag, +0x24 bit 2, matches
  its state); then the salvage into the mission's store [0x00609990]
  (0x00472cf0 per store) and the stores freed; set_valve's valve set; and,
  when visible and the player's vehicle is the view's, the HUD's
  objectives box reset (0x00446ad0 on the mech screen's +0x17964).
- **0x0045c180** (called the announce in mission.h): 1 unless one of
  primary, final or fail is done and not visible: whether the cockpit
  flashes its objectives box.
- **0x0045c4a0** is not a general start: it is Instant Action's and
  multiplayer's "destroy the enemies" group (after the block is read, and
  each new wave). Its description by the game's mode (Instant Action's
  +0x44: 1 message 0x261 with +0x6c, 2 0xdb1 with +0x6c, 4 0xdb9, else
  0x262); one destroy_entity for each object in zEntity's list that is
  active, hostile to the player's vehicle (+0xc8 against its team's +0xc0)
  and whose vt+0x48 is 0. It joins primary (secondary in multiplayer,
  mode 3), made empty if missing. FIX (bug): a mode outside 0..4 reads
  through a null in the exe; the port drops the group.
- The engine item's store texts (0x004739d0, 0x00432f00, 0x004738b0) are
  ported with it (`item_name` / `item_label`): "<maker> <rating>[ XL]".

## The types' checks and the wiring (piece 4, ported 2026-10-21)

Each type's vt+0x04 returns 1 once done, else completes itself
(0x0045e110) with this frame's answer:

| Type | Check | Met when |
|---|---|---|
| destroy_entity | 0x0045f3b0 | the entity's +0x08 equals +0x3c (0): gone. An entity not found is looked up again (vt+0x08, 0x0045f340) and, still missing, counts as destroyed |
| node_change | 0x0045f180 | the node's active flag (+0x24 bit 2) is the state asked |
| valve_change | 0x0045f1c0 | the valve's value (its holder's vt+0x04) equals the target |
| reach_area | 0x0045e890 | the player's vehicle (0x005a85e0) within the radius, in 3D: of the nav point (its vt+0x18), else of the point, which the node's world position (0x004ed530) replaces at each check |

`test_objective_check.py` now runs the real checks over a world that
changes between frames (entities destroyed, nodes switched, valve values,
the player's and the nodes' positions): 300 scenarios identical.

**Wiring** (`app_mission.cpp`, WiredObjectiveHooks): the mission's hooks
(objectives_load / _start / _check / _done, condition_met, the flash test,
the free) run the port on one `Objectives` whose groups' handles go into
the missions object's +0x7c..+0x88. Entities by name: the AI mechs by
their aiv.zrd names, then the targets by their nodes' names (0x004d7330's
zEntity list, then the targets'); a target's +0x08 is its active flag, a
mech's is "still in zEntity's list" (an approximation: zEntity is not
ported). Nodes, valves, positions, the message panel and the mission's
voice are the ported ones. Not behind them yet: the nav points (none is
found, so reach_area by nav and the nav keys do nothing), the HUD's
objectives box (0x00446ad0, mac 3's), Instant Action's game and enemies,
and the vehicles built from aiv.zrd other than mechs (c1m1's field bases,
so the campaign's field-base fail group is not made).

**c1m1 in the program** (`mission_c1m1_won_and_lost_by_its_objectives_headless`,
from the menus to the debriefing): primary = comcntr_down (the satdish
target's health_below sets it when destroyed) and the four SRM
emplacements; final = reach_mfb; fail = outside_c1m1. The emplacements
are aiTurrets.zrd's turrets (tur_srm01..04), found by name on zEntity's
list as 0x004d7330 does (the AI mechs by their records, the turrets and
vehicles by their nodes, then the targets; HUD plan item 6d: before it the
program's lookup knew only the mechs and targets, so every turret and
vehicle objective counted as destroyed at once). Won: the satdish and the
four emplacements destroyed, reach_mfb set (as aiv.zrd's reg_mfb region would: the regions
are the AI's), Exit Mission: the debriefing says won and shows the
mission's salvage list. Lost: outside_c1m1 set, or the player's mech
destroyed (the mission's own loss, Mech::die): the debriefing says
failed and hides the salvage. c1m1's objectives carry no salvage of their
own; the store path is in test_objective_check.

**Every mission's destroy objectives** (survey_destroy_objectives_every_mission,
MW3_SURVEY=1, item 6d): with the turrets and vehicles in the lookup, every
destroy objective of the campaign finds its entity at the start but two:
c2 m3's secondary objective names `firefly_01` and `firefly_02`, while the
mission's aiv.zrd makes `firefly_03` and `firefly_04` (reader.zbd's m3
block). Never found, those two count as destroyed from the start in the exe
too: a data typo. Fixed (Owen, 2026-10-25): `fix_objective_data` points
them at firefly_03 / 04 after the read (fixes.md, Data typos). Turrets made per world: c1 14, c2
12, c3 21, c4 19, c4b 13, t1 none (the same loader as the exe's,
test_turret.py, on the ported worlds).

## Training's types (piece 5, ported 2026-10-27, frontend-plan item 13a)

The fourteen other branches of 0x0045c920 and their checks (vt+0x04). None
takes `nav`; each also reads the shared keys above. "None" means no
objective.

| Type | Object | Reads | Check (met when) |
|---|---|---|---|
| `reach_velocity` | 0x3c, 0x005c2720 | `test` GT 0, GE 1 (default, and any other word), EQ 2, NE 3, LE 4, LT 5; `velocity` (a float; none without) | 0x0045f630: the player's screen's speed (+0x1ddc) against it, while the view is the player's |
| `aim_at_object` | 0x38, 0x005c2718 | `node` (0x0045c810; none without) | 0x0045f800: the node the reticle's aim hits (the screen's +0x144c, 0x00444330(0), its +0x24) is it, or a parent of it, climbing while a node has exactly one parent (+0x54 1, +0x58's first) |
| `target_object` | 0x40 over the named target, 0x005c2708 | `entity` (a name; none without), `part` (0x00484e40, -1 none) | 0x0045f8f0: the player's target (+0x6c, 0x004e6fe0) is the entity, and with a part the aimed part (+0xf8) is it |
| `time_limit` | 0x38, 0x005c2700 | the first number after the type (an int converted; none: 0 s) | 0x0045e6c0: the mission's clock ([0x0060991c]) past it |
| `view_object` | 0x3c over the named target, 0x005c26e8 | `entity` | 0x0045f400: the view camera's direction (its angles, +0x20, through 0x00522170) and the level direction to the entity within about 11 degrees (dot product of the two level, normalised directions over 0.98) |
| `face_object` | 0x3c, 0x005c26d8 | `entity` | 0x0045f520: the same with the player's mech's facing (+0x24..+0x2c, as it is) |
| `heat_warning` | 0x34, 0x005c26d0 | nothing | 0x0045f7b0: the player's heat warning on (+0x69c) |
| `User_Command` | 0x40, 0x005c26c8 | `cmd`: a CMD_ name (0x00410fe0), its key, joystick and mouse buttons (0x0051d1f0, 0x0051d220, 0x0051d230); none when it has no key above 0 and no buttons | 0x0045e790: the key down (0x0051a9b0), else the joystick button (0x0051dcf0), else the mouse button (0x0051b6e0) |
| `joystick_press` | 0x38, 0x005c26c0 | `button` (an int, not 0) | 0x0045e760: the button down |
| `key_press` | 0x38, 0x005c26b8 | `key`: names, each a key (0x0051aee0) or KEY_CTRL 0x200, KEY_ALT 0x100, KEY_SHIFT 0x400, OR'd (other KEY_ words count nothing); none without one | 0x0045e700: the key (with its modifiers) down |
| `mouse_press` | 0x38, 0x005c26b0 | `button` left 1, right 2, middle 3 (any case) | 0x0045e730: the button down |
| `weapon_fire` | 0x38, 0x005c26a8 | `weapon`: a weapon's name (0x004848a0), 0..36 | 0x0045f220: that weapon fired (0x004464d0 on the screen) |
| `weapon_select` | 0x38, 0x005c26a0 | `weapon` | 0x0045e800: the current weapon (+0xd4, 0x004b04c0) of that type (+0x28) |
| `weapon_group_select` | 0x38, 0x005c2698 | `group` 1..5 (kept as 0..4) | 0x0045e850: that group chosen (the manager's +0x28) |

Each check completes its condition with what it found (0x0045e110), so a
condition met counts as done for good: the checks stop at done. The input
checks keep only the answer's low bit. Quirks kept: `time_limit` takes any
number among its keys' values, the first; `weapon_*` refuse the plain PPC
(37: the exe's limit; the rebuild's own weapon, fixes.md). Fixed (fixes.md):
`view_object` reads the player vehicle's position without checking there is
one (`face_object` checks); `weapon_group_select` reads the vehicle's and
its weapon manager's group unchecked. Both give nothing without them.

The port: the classes `ReachVelocityObjective` .. `WeaponObjective`
(mech_objective.h), through `ObjectiveHooks`' training hooks (the engine's
answers are the program's, item 13b). Checked: `test_objectives.py` reads
every t1 block (as training and as a campaign) and made-up training
conditions; `test_objective_check.py` runs them over frames of a world
whose speed, aim and node parents, target and part, facing, heat, weapons,
clock, camera angles and buttons change from a shared script (600
scenarios). Breaking the port showed the tests catch it, 25 of 26: the
velocity test's default, its words' order, the velocity optional, the aim
node optional, the target's part dropped, time_limit's ints ignored or its
last number taken, User_Command's key 0 kept, KEY_SHIFT as Alt, the keys not
OR'd, mouse's right as 3 or its case, the group from 1; in the checks GT as
GE, LE as LT, no node parents, the part ignored, the clock's <=, the facing
at 0.9, the view not level or its yaw sign, the heat inverted, the input's
whole answer, the mouse before the joystick, the group for the weapon.
Missed, and unobservable on the original's path: the plain PPC accepted
(it exists only with the rebuild's fixes on).

## Training's lessons (frontend-plan item 13b)

A training mission's objectives are a list of lessons, each its own
objectives (description, primary, secondary, final, fail), held by the
training game (its +0x44 object: the list at +0x38..+0x40, 0x14 bytes each,
the current one at +0x44). The mission start (type 4) asks the game's vt+0x40
(0x00437140) for block `m<mission>` of objectives.zrd:

1. The block itself, read straight into the mission's objectives: one
   lesson, not in the list.
2. Else `<block>_set`: each list in it read as a lesson (entries that are
   not lists skipped; one that reads nothing not added).
3. Else the blocks `<block>1`, `<block>2`... while they are found and read
   (t1 has m11..m114, m21..m214, m31..m326, m41..m413).

With lessons, the first is selected (0x0045efa0) and given to the mission
(0x0045efd0: the mission's five slots point at the lesson's). The win
(0x0048f740) asks for the next (0x004373b0, 0x0045f080): there is one, the
mission's objectives become it and the mission goes on (no voice line);
past the last, the mission's are cleared and it ends. The cleanup frees the
lessons through the game's vt+0x44 (0x004379d0, 0x0045ef10), not the
mission's (0x0045c1d0 in training).

Quirk kept: lessons 2 and 3 read into the mission's slots, clearing only the
four groups between `_set` entries, so a lesson without its own `desc` has
the last read's (at first the block's own, read or not); without any lesson
the mission keeps the last read's description. The numbered blocks clear it
each time.

The end screen (mission-frame.md) fades training to the "TR Dummy State"
(0x006098c8, constructor 0x00491a70, vtable 0x005c60e0; campaign.md
section 12).

The port: `TrainingLessons` and `training_load` (mech_objective), the
training game's `lessons`; the program's mission hooks (`game_objectives`,
`training`, `training_next`, the objectives' `training_free`) and the
training types' answers (app_mission.cpp: the controls' commands and
zInput's keys and buttons, the clock, the view camera's angles; the player's
mech's target, aimed part, forward, heat state and weapon manager, its HUD's
speed, reticle hit and weapon list, through app_mech.cpp). Checked:
`test_training_lessons.py` (t1's m0..m5 and 300 made-up files, each stepped
past its last lesson: the lessons, where the mission points after each step,
the calls); `training_menus_to_lesson3_headless` (through the menus: m11's
1 s time_limit, m12's CMD_SHUTDOWN key pressed, m13 reached, then the TR
Dummy State).
Breaking the port showed the tests catch it, 17 of 18 (with the TR Dummy
State's and the program's): the step past the last lesson, the description
kept at the end, not copied, the block not read first, `_set` entries of any
type, the description not carried, the numbering past a gap or after a
`_set`, the first lesson not given; the TR Dummy State counting a loss, the
wrong mission, no save, no reopen; in the program the lessons not stepped,
no command key, the clock stopped, the fade lost. Missed, and unobservable:
the select out of range (only 0 is asked, with lessons).


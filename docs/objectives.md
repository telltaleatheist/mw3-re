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
| `weapon_fire`, `heat_warning`, `target_object`, `weapon_select`, `time_limit`, `face_object`, `reach_velocity`, `aim_at_object` | t1 (training) only |
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

The other fourteen types (training's and five no file uses) report
themselves as unported (`ObjectiveHooks::unported`, key `0x0045c920
<type>`) and give no objective; piece 5 ports them.

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
too: a data typo (fixes.md, candidates). Turrets made per world: c1 14, c2
12, c3 21, c4 19, c4b 13, t1 none (the same loader as the exe's,
test_turret.py, on the ported worlds).


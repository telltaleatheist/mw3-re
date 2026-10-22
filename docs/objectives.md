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

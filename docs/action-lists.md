# zClass's action lists and the frame update

What runs the game's per-frame ticks (the player's, the weapons', the
animations', the puffers', the scripts' scrolling textures) and what brings
the scene's nodes up to date between them. Ported in `gamez/zclass`
(`list.cpp`, `class.cpp`, `seq.cpp`, `cls_world.cpp`, `cls_util.cpp`) and
`gamez/zmodel/gmod_const.cpp`; emulator-checked by
`tests/differential/test_action_lists.py`. The lists themselves (storage,
the item pool, lazy removal and purging) are in `zclass-world.md` section 2.

## 1. Actions

A node gets an action (a callback at +0x48, called with ECX = the node) and
a priority (+0x44). Priorities 0..5 are the six action lists; any other
value is stored, and the action then sits on no list and never runs.
`node_alloc` (0x4eacd0) gives every node priority 1, so an action set
without a priority runs on list 1.

| Function | What it does |
|---|---|
| 0x4eb450 `node_set_action` | No action before and one now: pushed at the front of list +0x44; an action before and none now: removed (lazily) from it. +0x48 = the action. A priority outside 0..5 is reported ("ERROR setting action callback; priority = %d") and nothing changes. |
| 0x4eb500 `node_set_action_back` | The same, pushed at the back. |
| 0x4eb5b0 `node_set_priority` | With an action: off the old list (if 0..5), at the front of the new one (if 0..5). +0x44 = the priority. |
| 0x4eb170 `node_set_active` | Flag 4: only active nodes' actions run. |

What MW3 registers (all checked in the ported code paths): the scripts'
always-scrolling textures (the interpreter's `ScrollAlways` holder, priority
1 from `node_alloc`), the player's tick 0x4a1160 (2, `mission-start.md`
section 6), the radar clear (2, `weapon.cpp`), zWeapon's weapon_tick
0x5823c0 (3, `weapons.md`), each mission animation's tick (the priority from
anim.zbd's +0x9a, 4 in the shipped files; `anim-run.md`), the puffer tick
0x5490e0 (5, `puffers.md`).

## 2. The frame (0x4f7d80)

The Core state's frame calls 0x4f7d80 once (`game-loop.md`, Core frame step
3). For lists 0 to 5 in order (the head pointers at 0x5ea6b8): an empty list
is skipped, else 0x4f7d20 runs it and the frame update 0x4f7f70 follows. So a
node moved by a priority-2 tick has its matrix, bounds and world cell current
before priority 3 runs, and the scene is current again before the draw.

0x4f7d20 (ECX the head item): purging off ([0x5ea6b0] = 0), then for each
item in order: no node or no action marks the item removable; else an item
not removable whose node is active (flag 4) calls the action. The `next`
link is read after the call, so what the actions do to the list mid-run
shows at once:

- an item pushed at the back of the running list runs this pass; one pushed
  at the front does not (it is behind the walk);
- a removed item (`list_remove` clears its node and marks it) is skipped, so
  an action can stop a later one on its list this frame;
- a priority change onto a later list runs the action again this frame,
  onto an earlier one not until the next;
- nothing is unlinked during the walk (the purge is held off), so freeing a
  node's item mid-run is safe.

Purging back on, the lists are purged (0x4f7bd0).

## 3. The frame update (0x4f7f70)

Also called by the mission load (`mission.cpp`, after the view is made),
the camera's move and the scripts' WriteZBD. Three passes:

1. 0x4f7ec0: list 11, the sequences: each live item's node steps
   (0x4fdf50, section 6), purging held off, then a purge.
2. 0x4f7f10: list 12, the animates: each live item whose node is active
   steps (0x4fd950), the same way.
3. 0x4f7e50: list 7, the changed nodes. Take the first item still held
   (removable 0); its node gets flags 1 and 2 and is brought up to date by
   0x4f7e00; then rescan from the head, until no held item is left. (Each
   pass also marks the head item's node flag 1, held or not.)

0x4f7e00 (ECX node): with flag 2, 0x4f7db0 on it; then the same for each
parent that is not a world (so a changed node's ancestors come up to date
first and the world's cells see their final boxes); then, with purging on,
the purge. 0x4f7db0: the node's children with flag 2 first (depth first),
then gwNodeUpdate on the node, then its list-7 item released
(`list_remove(7)`; the flag-2 hold is gone by then).

The same 0x4f7e00 is what `node_to_world` (0x4ed280) calls for a changed node
on its chain, and 0x4f7e50 what the ground and segment queries call when list
7 is not empty (`cls_di.c`): queries always see the scene current.

## 4. gwNodeUpdate (0x4eca00)

ECX the node. The flags at +0x2c ask for its boxes: bit 2 its model's box
(0x4ed220), bit 1 its children's (0x4ecf70); either sets "bounds due". Both
bits cleared. Then by class:

| Class | Update |
|---|---|
| 1 camera | data +0x10 bit 2 (dirty): unless bit 1 (its own matrix), +0x80 = euler(+0x20 angles, +0x14 position, scale 1) (push, identity, 0x521670, pop); bounds now; bit 2 cleared |
| 2 world | 0x4f1df0: its fog to the renderer and its pending cells' area updates (below) |
| 5 object3d | data +0 bit 0 (dirty): unless bit 4 (USE_MATRIX), +0x30 = euler(+0x18, +0x54, scale +0x24); bounds now; bit 0 cleared |
| 6 LoD, 7 sequence | nothing |
| 8 animate | active, playing (+4 bit 2) and data +0 set: with bit 0, +0x08 = euler(+0x70, +0x7c, scale +0x88) and bounds now; +0 = 0 |
| 10 sound | 0x4fd680 (start, move or stop its voice; `sound.md`) |
| 3 window, 4 display, 9 light, others | reported "gwNodeUpdate(): Unrecognized node class type" (0x200), result 3 |

Bounds still due (0x4ecc00) run after; then flag 2 cleared and the list-7
item marked removable.

**The model box** 0x4ed220: a node with a model (+0x3c) gets +0x8c..+0xa0
from 0x53b4e0 and flag 0x200; without, flag 0x200 is cleared. 0x53b4e0
(gmod_const.c, ECX model, EDX box): type 0 the extent (0x53b680): from the
first vertex (else the first light's first point; neither: the box is kept),
grown by every light's points, the other vertices and the vertices fully
morphed (0x5220e0 into the scratch [0x74a288], factor 1). Type 1 (a facade)
takes each axis's larger end (|min| or max); mode 0 (+4) makes z as wide as
x, otherwise a cube of the largest. Then the centre (+0x48) and radius
(+0x54, the half-diagonal by the engine's square root guess: the float's
bits halved plus 0x1fc00000). The compares differ per axis (`!(v >= min)`
for x, `min > v` for y and z, and so on), which matters only for NaNs.

**The children's box** 0x4ecf70: not for worlds; flag 0x400 cleared; the
first child with a box (flag 0x100) through its own matrix (0x5231b0; an
object3d's +0x30 unless still the identity, a camera's +0x80, a playing
animate's +0x08; none: the box as is) into +0xa4..+0xb8, flag 0x400; every
later child's merged (min, max).

**The bounds** 0x4ecc00: worlds have none. +0x74 = the model box and the
children's merged, or either alone (flag 0x100); neither clears 0x100. +0x2c
bit 0 set. Then each parent: a world refiles the node: its box through its
matrix (once), the cell it fits (0x4f2110, `world_add_child`'s test; flag
0x80 keeps it out of the cells), and if that is its cell (+0x4c, +0x50) the
cell is flagged for its area update (0x4f2360), else it leaves its cell or
the world's direct children (0x4f2c30) and joins the new one (0x4f28c0). Any
other parent is marked: +0x2c bit 1 (its children's box is due), onto list
7 if not there, held (flag 2, item not removable).

**The area update** 0x4f18a0, run by 0x4f1df0 for each pending cell: flag
0x100 cleared; the cell's height range (+0x14 min y, +0x20 max y) from its
nodes with boxes, each through its matrix (0x4f2b70, y only), else from its
fade entries (none in the files); then, set, its sphere (0x4f9bc0: +0x28
centre, +0x34 radius). The scene walk and the shadows read these.

## 5. Sequences (0x4fdf50)

Class 7, made by the scripts' `NewSEQ` / `SEQNew` (0x4fdc70: data 0x28
bytes, step 1, on list 11; no shipped file has one, `interp.md`). Data:
+0x00 active, +0x04 repeat, +0x08 loop, +0x0c pause, +0x10 step, +0x14 index,
+0x18 time, +0x1c count, then {node, time} entries. Not paused and active:
time += the frame; while the current entry's time is below it, time -= that
entry's time and the index steps. Past either end it wraps (loop) or bounces
back (step negated); without repeat it stops (active 0) but the loop goes on
to settle the index.

## 6. Animates (0x4fd950)

Class 8, a node with its own keyed motion (no shipped file has one and
nothing in MW3 makes one). Active and playing (data +4 bit 2): the motion
record at +0x68 advances (0x4fda20: time += the frame; past the duration it
stops, state 2, or with a loop (+0xd2 != -1) goes back by the duration to
the restart time +0xc4; with a loop, past the loop end +0xc8 likewise); a
stopped one (2) clears its playing bit. Else the pose (0x4fdab0): key i =
the truncated (count - 1) * time / duration, the nine values (angles,
position, scale) interpolated to key i + 1 and times their multipliers
(+0x94..); data +0 bit 0 (matrix dirty), onto list 7, held.

## 7. Findings

- **A debug leftover.** 0x4ecc00 compares every node's name with
  "blackhawk_01" and `printf`s "haha" when it matches: a developer's
  breakpoint stand-in. A Windows program has no console, so nobody saw it;
  the rebuild drops it (FIX (A), `fixes.md`).
- **Priorities 6 and up are silent.** Any priority is stored, but only 0..5
  have lists: a node with an action moved to priority 6 or more (or below
  0) leaves its list and never runs again, unreported (`node_set_action`
  reports a bad priority only when the action is set after it).
- A sequence whose entries all have time 0, active and past them, loops for
  ever inside 0x4fdf50 (the time never drops). No shipped script makes a
  sequence. A sequence without entries reads past its data: guarded
  (FIX (bug)).
- The frame update runs after every non-empty list, six times a frame at
  most; nodes changed by the last list (5, the puffers) are current for the
  draw.
- The zClass reset (0x4f8f40 -> 0x4f8fb0, zclass-world.md "Taking nodes
  down") is ported and runs at the program's stop, so its tick objects go
  with the nodes; the program tests start as a new process does (no mission
  up, no mech type cached), and `mission_c1m1_frames` sees exactly one
  player tick object. The mission's own unload (0x490480, which runs the
  same reset between two missions of one run) is still a hook: a second
  mission in one program run would keep the first one's mech types (their
  trees freed by then), so it must come with the mission end.

## 8. The port and its test

- `zcls::run_action_lists` (0x4f7d80), `run_actions` (0x4f7d20),
  `frame_update` (0x4f7f70), `sequences_update` (0x4f7ec0), `animates_update`
  (0x4f7f10), `update_changed` (0x4f7e50), `update_node` (0x4f7e00),
  `update_subtree` (0x4f7db0); `node_update` (0x4eca00), `node_model_box`
  (0x4ed220), `node_children_box` (0x4ecf70), `node_bound_box` (0x4ecc00);
  `seq_update` (0x4fdf50), `animate_update` (0x4fd950, with 0x4fda20 and
  0x4fdab0); `world_touch_cell` (0x4f2360), `box_y_range` (0x4f2b70), the
  area update (0x4f18a0), `box_sphere` (0x4f9bc0); `zmod::model_box`
  (0x53b4e0, 0x53b680). `update_hooks()` now defaults to the port (a test can
  still stand in recorders).
- Wired: the Core state's `lists_update` (`GameEngine`), the mission start's
  frame update (`lists_4f7f70`), the interpreter's `set_action`,
  `set_action_priority` and WriteZBD's frame update and purge.
- `test_action_lists.py` (probe `tests/probe_action_lists.cpp`): every
  mission world read by both sides, then (1) the frame update on what the
  load left changed, on every facade, morph and lights-only model's box
  rebuilt, and over rounds of random moves (translation, rotation, scale,
  added translation, a given matrix, the camera; one node to a hundred),
  some with the model or children's box asked for; (2) the sequences and
  animates made by hand (random headers, keys, steps, stops and loops);
  (3) the action lists with twelve action nodes on priorities -1..7, front
  and back, active or not, running scripts that log themselves and set or
  clear actions (their own, others', front or back), change priorities
  (their own list's included), activity, remove items from lists, move world
  objects, ask a node's world position mid-run and log a node's flags; the
  scripts change every four frames, the sequences and animates go on. After
  each call: every node on list 6 (flags, +0x2c, class, priority, cell,
  action, parents, children, boxes +0x64..+0xbb, +0xc0/+0xc4, its list-7
  item), its class data (camera, object3d, sequence, animate, a world's
  header and every cell with its nodes), the model boxes, the 16 lists and
  their items, and the trace (actions run, flags logged, every gwNodeUpdate
  in order, zerr reports). All six worlds identical (`run_all.sh`); floats
  within 1e-5 where the x87's euler sines differ in the last bits.
- Break tests: section 9.

## 9. Break tests

Each changed the port the way a bug would; `test_action_lists.py 8 c1 t1`
had to fail. 19 of 22 were caught; one is equivalent to the original, two were not reached:

| Break | Caught at |
|---|---|
| the action run ignores the active flag | the action trace |
| the frame update once after all six lists | the trace (node updates, sequences) |
| an empty action list still runs the frame update | the trace (sequences step once more) |
| list 7 walked on from the item, not rescanned | the dump |
| a changed node updated before its changed children | the order of node updates |
| update_node climbs into worlds too | the dump (cells) |
| the children's box takes the first child without its matrix | the dump |
| the bounds take the larger minimum | the dump |
| a node staying in its cell does not flag the cell | the dump (cells) |
| USE_MATRIX ignored (object3d_set_matrix) | the dump |
| a facade always a cube | the model boxes |
| the model box ignores the lights | the model boxes |
| the morphed vertices left out of the model box | the model boxes |
| the area update does nothing | the cells |
| the area update keeps the lower maximum | the cells |
| a sequence wraps where it should bounce | the sequence data |
| a sequence keeps running at its end without repeat | the sequence data |
| the animate pose not interpolated | the animate data |
| a stopped animate keeps playing | the animate data |
| the class lists run removed items | equivalent: a removed item has no node, so nothing runs either way |
| a parent marked changed but not held on list 7 | not caught: equivalent in every case seen, since 0x4f7e00 brings each changed node's parents up to date by climbing to them anyway |
| purging not held off during the actions | not caught: it shows only when an action frees its own item and then, in the same call, makes a node update purge the lists (a world-position query on a changed node); the random scripts did not reach that |

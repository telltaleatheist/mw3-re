# Mission animations: the runtime (`zeff_anim_run.c`)

Mech3.exe 1.2, 2026-10-20. Follows `anim.md` (the file) and `anim-postload.md`
(the post-load pass). Port: `src/gamez/zeffect/zeff_anim_run.*` (runner,
ticks, reset, start, stop), `zeff_anim_events.*` (the handlers),
`zeff_anim_save.*` (the call log), `zeff_anim_init.cpp` (0x50ae60, the
instances, the on-demand light and sound refs), `zeff_anim_puffer.*` (event
42, earlier), `zweapon/zwep_damage.*` (the damage records), and in zClass the
action lists, the setters and the world's child/light/sound lists. Check:
`tests/differential/test_anim_run.py` (probe `tests/probe_anim_run.cpp`).

## How an animation runs

- **Start** (0x508ae0 plain, 0x5085e0 at a pose, 0x508c30 / 0x508dc0 as a
  connector; each goes through 0x508760). Refused for status 4, 5, 6; a
  running one with a reset time of -99 never restarts. A running def started
  at another node gets an *instance*: the next free copy on its chain
  (+0x138), else a new one (0x5097c0, its own object, lights, puffers and
  sounds, its lists looked up again under the node). With a node the def is
  rebound to it (0x509eb0). Prerequisites must hold (0x508950). RESET_TIME
  defs reset first. 0x5084f0 adds a free-hanging node to the world (flag
  0x100: it is then put at the origin, and turned to 0 unless it is a
  billboard), zeroes the clock (+0xa8), saves the object states (unless
  0x40000) and restarts every sequence. The def's own object (+0x68,
  `_<name>`) gets the tick 0x507de0 on its action list (priority +0x9a,
  always 4); status 2. The start vector (+0xb8) is stored (flag 0x80 when
  over 0.01). Each start is logged (0x50cbb0..0x50cf40, below).
- **Tick** (0x507de0, ECX the object): defs with EXECUTION_BY_RANGE (2),
  BY_ZONE (8) or flag 4 (the root was drawn: node flag 0x80000000, cleared
  here) test that, then skip 1..10 frames (+0x9b, a counter shared round
  robin) before testing again. Range: the squared distance from the root to
  the player position (0x502d40) within [+0x9c, +0xa0); zone: the zone list
  (0x502d70) against the root's zone byte (0x53cdc0). Then the clock
  advances by the frame time and every sequence in state 0 or 1 runs with
  the frame's time (block +0x40). An error sets status 5. When none is
  left running the anim stops (0x506ca0).
- **Events** (0x5079b0): a sequence runs events while they finish at once.
  An event waits for its start time (+0x08) on the anim clock (+0x01 = 1),
  the sequence clock (2) or the time since the last event (3). Handlers
  return 2 (done), 1 (still going: the time the event used is taken from the
  frame's time) or, for the loop event, 0 (restarted). After the last event
  a sequence whose reset state is 3 restarts and waits to be called (event
  22); others stop (state 2).
- **Stop** (0x506ca0): with a reset time (+0xa4) below 0 the def rests at
  once (0x508170; status 3, or 4 from 6); else its object's action becomes
  the wait 0x5085b0, which resets (0x508490: reset keeping the saved states,
  then logged) once the clock passes the reset time. The def's callback
  (+0x6c, set by 0x508f00) hears 0.
- **Reset** (0x508310): AUTO_RESET (0x10000) defs restore the object states
  (0x508090: active, matrix or translation/rotation/scale, and the model's
  opacity off); the reset sequence runs, finishing later from 0x508260 if it
  takes time; 0x508170 then takes the node out of the world, switches the
  lights, puffers and sounds off, stops the anims it called, clears the tick,
  status 1 (4 from 6) and refills the health.

## The events

Offsets from the event's start (mech3ax's structs start after the 12-byte
header). "Node" fields are node ref indices (+0x118 list; 0 is empty); -200
is the node the anim was started at (+0x74, with its offset +0x78), -100 the
def's own node.

| Type | Handler | What it does |
|---|---|---|
| 1 sound | 0x503100 | static sound +0x0c, at node +0x0e + offset +0x10 (play_at) or plain; volume 1; nothing with sounds off |
| 2 sound node | 0x503270 | dynamic sound by name (+0x2c caches the ref, made on demand 0x5092e0): active +0x34 (into or out of the world), position absolute (1) or from node +0x38 (2) |
| 3 effect | 0x5031b0 | not shipped; hooked |
| 4 light state | 0x503390 | light by name (made on demand 0x509400): active, type (+0x38), position (1), at node (2), orientation (4), range (8), colour (0x10), ambient (0x20), diffuse (0x40), the mode bits (0x80..0x400) |
| 5 light animation | 0x503910 | range and colour move at their rates until +0x6c; a near range below 0 becomes 1, a far one under the near one becomes near + 1 |
| 6 active | 0x5044d0 | node +0x10 on/off |
| 7 translate | 0x504380 | node +0x1c to +0x10 in node +0x1e's frame; flag 1 adds |
| 8 scale | 0x504350 | node +0x18 |
| 9 rotate | 0x504180 | node +0x1c: add (1), relative to node +0x1e's rotation (2) or world rotation (4), or set |
| 10 motion | 0x504520 | below |
| 11 motion from-to | 0x505200 | position, rotation, scale, opacity from start values at rates until +0x8c, then the end values (a camera's end rotation is its start one) |
| 12 SI script | 0x505b00 | key frames from +0x24, linear or cubic tracks; rotation as an axis-angle exponential times a base quaternion |
| 13 opacity | 0x505bf0 | the opacity flag and value |
| 14 opacity from-to | 0x505c60 | at a rate until +0x20 |
| 15 add child | 0x505d90 | unless already a child |
| 16 delete child | 0x505df0 | not shipped; hooked |
| 17 cycle texture | 0x505e20 | the model's texture cycle to its start, then frame +0x12 |
| 18 connector | 0x505e80 | a node stretched between two points (its z scale is the distance, a fast square root), growing over time |
| 19 call connector | 0x506290 | starts a def as a connector (0x508dc0) |
| 20, 21 camera | 0x503c10, 0x503d50 | not shipped; hooked |
| 22, 23 call / stop sequence | 0x5064e0, 0x506590 | a waiting sequence (3) starts; a sequence stops |
| 24 call animation | 0x506640 | starts a def plainly, at a node's pose (flag 1, with the node's world angles for 4), or as a connector (8), at node +0x2c; the anim ref +0x32 keeps it; with 0x10 the event waits while it runs |
| 25, 26, 27 | 0x506d60, 0x506e00, 0x506ea0 | stop, reset, invalidate a def by name |
| 28 fog | 0x503ba0 | the world's fog type, colour, altitude, range |
| 30 loop | 0x506f60 | a count (-1 for ever) or a total time; else the sequence restarts. The anim clock is held under a day (86400 s) |
| 31..34 if, else, elseif, endif | 0x507010, 0x5073b0, 0x5073e0 | random, hardware, block +0x3c, player distance, the ground under the player or a node (a ray, 0x507220), heights, the level (0x508fb0) |
| 35 callback | 0x507410 | the def's callback hears +0x0c |
| 36, 37 fbfx | 0x5074b0, 0x5076c0 | screen colour and wave (the 2D layer); hooked |
| 38 | 0x507970 | a message (the 2D layer); hooked |
| 39, 40 | 0x507960 | nothing |
| 41 detonate | 0x507430 | the weapon named +0x0c goes off at +0x18 in node +0x16's frame; hooked |
| 42 puffer | 0x5035b0 | `puffers.md` |

29 and anything over 42 are reported as invalid and end the run (-1).

### Motion (type 10)

Flags +0x0c: 1 gravity (+0x18 added to the acceleration's y, or in the
node's own frame with 0x2000), 2 the anim's start vector added in the
parent's frame (when the def has one: flag 0x80), 4 velocity +0x40 /
acceleration +0x4c, 8 a random direction (yaw +0x20..0x24 degrees, pitch
+0x28..0x2c as a fraction of 90, speeds +0x30.., +0x38..), 0x20 spin
(+0x88, its rate +0x94), 0x40 roll with the motion, 0x80 tumble, 0x100
scale (+0xac, rate +0xb8; never under 0.001), 0x200 fade (+0x1c), 0x400 a
set run time (+0x148), 0x800 a sequence per surface on landing, 0x1000 a
sound per surface (volume by speed), 0x4000 no ground test. With gravity the
node looks for ground under it (0x505160 over 0x4edab0's hits): crossing it
lands it (snapped, or bounced up by half the step when moving sideways);
landing slower than its acceleration ends the motion, faster keeps 20% of
the speed. Falling 15 s with nothing below ends it (unless 0x400).

The frame used for the start vector and the own-frame gravity is the
*transpose* of the node-to-world rotation (0x5047fa, 0x50497e), i.e. world
to the frame. Both take node_to_world in mode 2, which in the original
copies stack leftovers into the translation (0x51fee0; the port's FIX (bug)
in `fixes.md`): that matrix is also cached as the node's world matrix, so a
flying piece's next ground test in the original runs at an undefined place.
The test patches the original's temp to zero, as the port does.

## The call log (`zeff_anim_save.c`)

0x50bd50 hands out 0x50-byte records (1000, doubled when full, 0x60c708).
Each start writes one (type 1 at a pose, 2 plain, 3 connector, 4 connector
at, 5 reset: name, node index, the arguments) when the def's flags or the
defaults (0x5ebf0c save, 0x5ebf10 network, both 1) say so; a def added to
the world, a nameless def or a node outside the pool is never logged. The
connector logger (0x50cde0) ignores the defaults. A network record goes to
the hook [0x60c714] (null in single play); unsaved ones are dropped again.

## Not ported (hooks, `AnimRunHooks`)

zSound's play calls, the ground hits (0x4edab0, `cls_di.c`), the "if" ray
(0x507220, `cls_di.c`), events 3, 16, 20, 21, 36, 37, 38 and the weapon
lookup and detonation of 41; zClass's frame update (0x4f7f70) and node
update (0x4f7e00), still `UpdateHooks`. The tree copy 0x4f9b50 stays a hook
(the instance path without a node, never reached by shipped data, fails).

## Check

`test_anim_run.py` runs each mission's post-load pass and frames of the
action lists in both, with random starts, resets, stops, hits, rams, player
positions, zones and levels between frames, comparing every def, instance,
sequence, list, puffer, touched node (every node and event blob every 25th
frame), the action and class lists, the damage records, the log and the
hooked calls. The original runs with the port's mode-2 matrix FIX (bug)
patched in (above).

Results (2026-10-20): 300 frames on each of the six missions identical
(1.52 billion words compared, 48,406 floats within 1e-4 relative, worst
3.6e-6), with 862 plain starts, 249 at a pose, 123 at a node (instances),
244 resets, 206 stops, 52 invalidations, 248 rams and 260 weapon hits. Event
types run (default 60-frame run): 1, 2, 4..15, 17, 18, 22..25, 27, 30..35,
41, 42. Never reached by the runs: 19 call connector and 26 reset (12 and 3
shipped events), 28 fog (7). Breaks caught: the motion frame not transposed,
a from-to ending a step late, call-animation never waiting, the sequence
clock read as the event clock, the stagger over 1..9, status 6 resting as 1,
prerequisites' active state inverted, no x margin for world cells, a list
removal not flagged, the light animation's near-range clamp dropped (c2).
Not caught: the prerequisite count off by one (no run meets a def one short)
and a taken damage slot overwritten (shipped data never registers twice).

The FIX (bug) for a missing prerequisite node is checked in `test_anim.py`
(exe path exact on a doctored c2 file; the fix keeps the other entries).

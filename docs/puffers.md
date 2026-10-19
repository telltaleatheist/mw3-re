# Puffers: smoke, fire and dust sprites

Mech3.exe 1.2, 2026-10-20. `zModel\gmod_puff.cpp` (the file name comes from
0x549410's error report; 0x548840..0x54b750 are the same system). Port:
`src/gamez/zmodel/gmod_puff.{h,cpp}`, the sprite `src/gamez/zrender/zrndr_draw.*`
(0x567720), the animation event `src/gamez/zeffect/zeff_anim_puffer.*`, the
draw entry 0x57caa0 in `zvid_ddd3d`, `zmth::exp_neg` / `euler_to_quat` /
`quat_matrix`, `zcls::node_world_euler` / `_position`. Check:
`tests/differential/test_puffers.py` (probe `tests/probe_puffers.cpp`), and
0x57caa0 in `test_drawtable.py`.

A puffer is an emitter the animations create and steer. Each frame it drops
particles along the path it moved; each particle flies, slows toward the
wind, grows and fades, and is drawn as a square textured sprite facing the
screen. Every missile smoke trail, muzzle puff, jump-jet plume, burning
helicopter, steam vent and the smoke from a mech's torn-off arm is one.

## What is ported

| Exe | Port | |
|---|---|---|
| 0x54a710 | `puffer_create` | new 0xe4-byte puffer, defaults below, appended to the list 0x72be04 |
| 0x54ab40..0x54b460 | `puffer_set_*`, `_add_*`, `_clear_*` | the 29 setters (table below) |
| 0x54b630..0x54b670 | `wind_set_velocity`, `_max_speed`, `_random_accel`, `_random_angvel` | world.zrd `global_wind` |
| 0x549410 | `puff_tick_reset` | tick node once, the wind zeroed (not its angle and speed) |
| 0x5490e0 | `puff_tick` | the tick node's callback: wind, puffers, particles |
| 0x54a090 | `puffer_update` | one puffer's emission |
| 0x5495a0 | `puff_shutdown` | frees all (called from the anim shutdown 0x50b420) |
| 0x5496c0, 0x5489a0 | `puff_draw`, `particle_draw` | the world render's call after the scene walk |
| 0x567720 | `zrndr::draw_sprite` | the square, cut to the screen, through [0x72f3c0] / [0x72f3bc] |
| 0x57caa0 | `zvid::draw_textured_white` | draw table [0x72f3bc] (also weather, 0x5a5a03) |
| 0x523f70 | `zmth::exp_neg` | e^-x by a table (drag; also mech code 0x466xxx, 0x4a1xxx) |
| 0x525340, 0x525740 | `zmth::euler_to_quat`, `zmth::quat_matrix` | the second moved here from gmod_draw |
| 0x4ed5d0, 0x4ed530 | `zcls::node_world_euler`, `node_world_position` | zClass\class.c |
| 0x5035b0 | `zeff::anim_puffer_state` | anim event 42 |
| 0x509260, 0x509520 | `zeff::anim_find_puffer`, `anim_puffer_index` | zeff_anim_init.c |
| 0x518210 | `zimg::default_entry` | the "DEFAULT_TEXTURE" entry 0x5ecbf8 |

Not ported: the software sprite 0x566620 (the software renderer, render.md
section 9); the default entry's record is made by 0x5184e0 at startup (not
ported: the front end sets `default_entry()->surface`). Dead code, never
called: 0x548840 (the wind step with a dt argument) and 0x548f30 (the
particle step with one); 0x5490e0 has both inlined.

Hooks (`PuffHooks`): the tick node's zClass calls (0x4fb740 make, 0x4eb5b0
priority, 0x4eb450 callback, 0x4eaea0 destroy) and, for the tests, the node
world rotation and position (null = the zcls ports). The integration points
`mech3::world_hooks()` (wind, tick reset), `zcls::world_render_hooks().puffers`
and `zeff::anim_hooks().create_puffer` at these (the viewer does,
`tools/viewer/realframe.cpp`).

**Correction to render.md**: the draw table entries [0x72f3cc] = 0x57ea20 and
[0x72f3dc] = 0x57fe20, called at 0x5481eb / 0x548211, are not the puffers'.
They are in 0x547740, the model draw's projected-light pass (lights with
flags 0x10 and 0x40, `zmod::DrawHooks::projected_draw`). Puffers draw through
0x567720 into [0x72f3c0] (0x57ce20) or [0x72f3bc] (0x57caa0).

## The puffer (0xe4 bytes, 0x54a710)

| Off | Field | Default | Setter |
|---|---|---|---|
| +0x00 | active (byte) | 0 | 0x54ab40 (EDX); off also clears +0x80 |
| +0x04 | particles per emission | 1 | 0x54ada0 (EDX) |
| +0x08 | timed textures (byte) | 0 | set by the texture adders |
| +0x0c | textures: vector of {time, entry} (8) | empty | 0x54adb0 clear, 0x54adf0 add (time 0, untimed), 0x54afc0 add timed |
| +0x1c | colours: vector of {r, g, b, a, time} (0x14) | empty | 0x54b190 clear, 0x54b1d0 add (time, r, g, b, a) |
| +0x2c | growth: vector of {time, factor} (8) | empty | 0x54b420 clear, 0x54b460 add |
| +0x3c | interval by distance (byte) | 0 | 0x54ac30 |
| +0x40, +0x44 | interval, 1 / interval | 1, 1 | 0x54ac40 (0 ignored) |
| +0x48 | accumulated time or distance | 0 | |
| +0x4c, +0x50 | size min, max | 1, 1 | 0x54ac70, 0x54ac80 |
| +0x54, +0x58 | lifetime min, max | 1, 1 | 0x54ac90, 0x54aca0 |
| +0x5c, +0x60 | start age min, max | 0, 0 | 0x54acb0, 0x54acc0 |
| +0x64 | deviation | 0 | 0x54acd0 |
| +0x68 | friction | 0 | 0x54ad80 |
| +0x6c | wind factor | 1 | 0x54ad90 |
| +0x70 | node | null | 0x54ab60 (EDX) |
| +0x74, +0x80 | last position, valid (byte) | | |
| +0x84 | translate (vec3) | 0 | 0x54ab70 (EDX vec) |
| +0x90 | local velocity | 0 | 0x54ab90 |
| +0x9c | world velocity | 0 | 0x54abb0 |
| +0xa8, +0xb4 | random velocity min, max | 0 | 0x54abd0, 0x54abf0 |
| +0xc0 | acceleration | 0 | 0x54ac10 |
| +0xcc, +0xd0, +0xd4 | fade-in start, end, 1 / span | 0, 0, 0 | 0x54ace0 |
| +0xd8, +0xdc, +0xe0 | fade-out start, end, 1 / span | FLT_MAX, FLT_MAX, 0 | 0x54ad30 |

The span setters store the span itself when it is exactly 0. The tracks are
MSVC `std::vector`s (allocator byte, begin, end, capacity); the bytes at +0x0c,
+0x1c, +0x2c are the allocators' (garbage), not flags.

## The particle (0x90 bytes)

Position +0x00, velocity +0x0c, acceleration +0x18, size +0x24 (half the
sprite's width, world units), age +0x28, lifetime +0x2c, 1 / lifetime +0x30,
the fades +0x34..+0x48 (in, in end, out, out end, in inverse, out inverse),
friction +0x4c, wind factor +0x50, then the three tracks with an iterator
each: textures +0x58 (iterator +0x54), colours +0x6c (+0x68), growth +0x7c
(+0x8c). All particles of all puffers share one list (0x72be14, count
0x72be18), with no limit.

## Emission (0x54a090, per active puffer, every frame)

1. Position: the translate, or with a node: the node's world euler angles
   (0x4ed5d0) -> quaternion 0x525340 (called with (yaw, pitch, roll), i.e.
   angles 1, 0, 2) -> 3x3 0x525740, the node's world position (0x4ed530) as
   the translation; translate times that.
2. The first frame (or after switching on) only remembers the position.
3. Step: dt, or by distance the length moved, **0 when over 200** (a jump).
   `accum += step`, `n = ftol(accum / interval)`.
4. For i = 1..n at `frac = i * interval / accum` along the path, `number`
   particles each, drawing from rand (MSVCRT, same order): lifetime, start age
   `= range + dt * (1 - frac)` (so each starts as old as it would be had it
   left at its point on the path); **only if age < lifetime**: position
   `= (r - 0.5) * deviation + frac * moved + last position` per axis, size,
   velocity `= range + world velocity + local velocity (turned with the node)`
   per axis, then untimed textures pick one at random (`rand % count`; none:
   the default texture). A particle gets copies of the puffer's tracks.
5. `accum -= n * interval`; remember the position.

## The frame (0x5490e0)

The wind first: `angle += U(-1, 1) * dt * random_angvel`, wrapped to
0..2pi; `speed += U(-1, 1) * random_accel` (**per frame, not scaled by dt**);
a negative speed turns the angle round by pi (not wrapped again) and is
negated; clamped to `max_speed`. Wind = base velocity + speed * (cos, 0, sin)
of the angle (in the x-z plane). The angle and speed are never reset, so a
mission starts with the last one's wind.

Then every puffer's emission, then every particle: `age += dt`, `pos += dt *
vel` (the old velocity), `vel += accel * dt`, and with friction != 0:
`vel = (vel - wind * factor) * exp_neg(dt * friction) + wind * factor`. A
particle dies when `age >= lifetime`. **Without friction the wind does not
touch a particle.**

`exp_neg` (0x523f70) is a 256-entry table of e^-(i/51), x **truncated** to a
step: 1 below 1/51. So whenever `dt * friction < 1/51` the drag is off
entirely: friction 1.0 at 60 fps (dt 0.0167 -> index 0), friction 1.75 above
89 fps. The 1999 frame rates (30-60) mostly gave the drag; a modern 144 Hz
display switches it off for every shipped puffer (fixes.md, candidates).

## Drawing (0x5496c0, 0x5489a0, 0x567720)

0x5496c0 folds the projection into the view matrix (x by fx/near, y by
fy/near, z by 1/near), then per particle:

1. `t = age / lifetime` (0 before 0). Growth = the growth keys at t (the
   iterator moves on while the next key's time is reached, then it holds or
   blends linearly to the next key); 1 without keys.
2. Cull 0x541760 against the near and far planes only, radius growth * size.
3. Depth d = view z. Fade: 0 beyond the fade-out end, a ramp between its
   start and end, 0 at or before the fade-in start, 1 from the fade-in end,
   a ramp between. **Bug**: the fade-in ramp subtracts the fade-out start
   instead of the fade-in start, so it is negative and the sprite is skipped
   there; no shipped puffer has a fade-in (`unk_range` in mech3ax). The port
   fixes it (fixes.md); the test patches the original the same way.
4. No sprite when the fade is 0 or below. Screen x, y, 1/z; half size
   `fx/z * size * growth` pixels.
5. Alpha: with colour keys the key's alpha (blended like growth) times the
   fade, the colour into the four work records; without, `(1 - t) * fade`, a
   linear fade over the lifetime. Texture: the texture keys at t.
6. 0x567720: nothing under 1 pixel (half size < 1). The square is cut to the
   model renderer's screen rect (0x7402fc..) with its UVs (v = 1 at the top),
   z = 1/z on all four, then [0x72f3c0] 0x57ce20 (coloured) or [0x72f3bc]
   0x57caa0 (white), count 4, the current primitive (a fan: the model draw
   resets [0x72f418] to 6 at its end).

0x57caa0 is 0x57ce20 with white vertices: transparent (queued, depth sorted)
when the texture is ARGB4444 or alpha < 1, else drawn at once **flat
shaded**; unlike 0x57ce20 it writes the TL vertices only once a queue or
overlay slot is free.

## The animation format (anim.zbd event 42, 0x5035b0)

Animations hold puffer refs (def +0x120, count +0x10c, 0x2c each: name, +0x24
the puffer made at load, `zeff_anim_init`; ref 0 unused) and switch them with
PUFFER_STATE events, 0x250 bytes with the 12-byte event header. Offsets from
the header (mech3ax's are 12 less):

| Off | Field | Flag that applies it |
|---|---|---|
| +0x0c | puffer name (32) | |
| +0x2c | ref index, < 1: looked up by name (0x509520, making a puffer and a ref when missing) and **written back into the event** | |
| +0x30 | flags | |
| +0x34 | node ref (def +0x118); -200 = def +0x74 | 0x2 (applied last) |
| +0x38 | active | 0x4 |
| +0x3c, +0x48, +0x54 | translate, local velocity, world velocity | 0x1, 0x8, 0x10 |
| +0x60, +0x6c | random velocity min, max | 0x20, 0x40 |
| +0x78 | acceleration | 0x10000 |
| +0x84, +0x88 | by distance (int), interval | 0x80 **or** 0x100: either sets both |
| +0x8c, +0x94, +0x9c | size, lifetime, start age ranges | 0x200, 0x400, 0x8000 |
| +0xa4 | deviation | 0x800 |
| +0xa8 | fade-in range (mech3ax `unk_range`) | 0x80000 |
| +0xb0 | fade-out range | 0x1000 |
| +0xb8, +0xbc | friction, wind factor | 0x20000, 0x100000 |
| +0xc0 | particles per emission, an **int** (mech3ax: `priority`, a float) | 0x200000 |
| +0xc4 | textures timed (int), then 6 x {time, name[32]}; names looked up exactly (0x518220), unknown ones get the default texture; stop at an empty name | 0x4000 |
| +0x1a0 | colour count, then 6 x {time, r, g, b, a} | 0x40000 |
| +0x21c | growth count, then 6 x {time, factor} (the exe applies this block twice) | 0x2000 |

Flag 0x400000 (mech3ax PRIORITY) is never read. The counts are not bounded
by the exe (FIX (bug) in the port: at most the six slots).

### Shipped (explore/puffer_survey.py)

842 events in the six anim.zbd files (447 switch a puffer off, 395 set one
up), in 62 animations. 33 puffer names: `trailpuffer1..4` (smoke trails,
anims `black_smoke_trails`, `white_smoke_trails`, `miss_narc.flt`: mostly by
distance, a puff every 0.75-12 units moved), `muzpuffer` (every 0.2 s),
`jumppuffer1/2` (anim `jumpjets`), `helipuffer`, `bsmokepuffer`, `steam`,
`boiler1/2`, `sparkpuff`, `flamepuffer` (`flame_ball*`), `lbxpuffer`,
`ultrapuffer` and `puffer*`; the most used anims are `mech_larm_stub01` and
`mech_rarm_stub01` (72 events each: likely the smoking stump of a lost
arm). Textures: `fire_f01..05`, `bsmok1`, `wsmok1`,
`wtsmoke1/2`, `laser_impact_blue`, always untimed (one at random per
puff). Every set-up has growth keys (0, 1) -> (1, 0.15..3; mostly 2: puffs
double over their life); none has colours (so all draw white through
0x57caa0 with the linear age fade), a fade-in, a wind factor or a number.
Fade-out ranges 80-100 up to 400-600. Friction 1.0 (muzzle, trails), 1.5,
1.75 (black smoke, boilers), 0.3 (sparks), or none. `trailpuffer2` has
lifetimes with max < min (0.8, 0.1): the range runs backwards, still
0.1..0.8. World wind (world-params.md): base 0, max speed 13, random accel 2,
random angvel 10 degrees, in every mission.

## Checks

`test_puffers.py`, the FPU at 24 bits: `exp_neg` on every table step and
random inputs; `euler_to_quat`; 60 random runs (1-6 puffers through every
setter, the wind, 40-300 frames of random or fixed frame steps with moving
nodes and emitters, puffers switched on and off, a third with fixed 0.125 s
frames and power-of-two lifetimes and key times so ages land exactly on
them), every puffer, particle, track, iterator and the wind compared after
every call and draw; draws under random cameras and screen rects; animation
events against random defs (missing refs, cached indices, node refs, -200,
textures found and not, every flag). All bit-identical, with the original's
near-fade bug patched to the port's fix (`--unfixed` shows the difference).
Then on world c1: 0x4ed5d0 / 0x4ed530 on all 8,166 nodes (7,879
bit-identical, the rest within 1e-6: last bits of zcls::node_to_world's
chain, as in test_scenewalk.py) and runs with puffers at real object3d
nodes. 0x57caa0 in test_drawtable.py's random sequences.

Break tests, each caught: the wind's speed step scaled by dt, the turned-round
angle wrapped again, particles moved with the new velocity, `>` for death,
the emission fraction from i - 1, no start-age offset, no teleport guard, a
wrong random texture index, the drag without the wind factor, the euler
angles in file order, the velocity not turned with the node, `exp_neg`
rounding its step or its table in double, `>` in the key walk, the
uncoloured alpha without the fade, the fade-out ramp from its start, the cull
without growth, sub-pixel sprites drawn, v not flipped, a wrong clipped u, the
interval needing both flags, the number read as a float, colour keys with
the time last, the ref search including ref 0, the node euler from the
local matrix, 0x57caa0 Gouraud, and 0x57caa0 writing the TL before the queue
check.

## Open

- What sets def +0x74 (the node for node ref -200; no shipped event uses it).
- The emulator rounds fsin / fcos results to float before the multiplies in
  0x525340 and 0x5490e0 (as the port does); a real x87 keeps 64 bits there,
  which can change the last bit of a quaternion or wind component.

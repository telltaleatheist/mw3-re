# Ground following and the part updates

Mech3.exe 1.2, 2026-10-20, from the asm (part of `movement.md`). The mech never
casts a ray for its height: a probe object at mech +0x41c ("skis", source
`C:\zipper\Mech3\mech_skis.cpp`, the "No altitude" assert; code 0x475c80..
0x4779d1) holds seven ground probes and moves the body.

## The skis (0x300 bytes, ctor 0x476320 from mech setup 0x415e09)

Probes j: 0..2 the left ski (local z = -L, 0, +L), 3..5 the right ski, 6 the
centre. Forward is -z, so 0 and 3 are the front probes.

| Off | Meaning |
|---|---|
| +0x00 | the mech |
| +0x05 | slid or pushed this frame (Update reads it: no push decay, throttle stop) |
| +0x08 | the mech's world 3x4 matrix, copied each frame (translation +0x2c) |
| +0x38, +0x3c, +0x40 | ski root and the two ski nodes (object3d, hidden) |
| +0x44 / +0x4c + 4s | sin / cos of each ski's pitch |
| +0x54, +0x5c, +0x64 + 4s | each ski's height parabola a, b, c |
| +0x6c, +0x6d | left / right ski on the ground (ctor 1) |
| +0x70, +0x74 | left / right slope (sine of the pitch), smoothed |
| +0x78 | half track: (foot 1 x - foot 0 x) / 2 |
| +0x7c + 0xc j | world probe positions |
| +0xd0 + 0xc j | local probes in ski space; +0xe4, +0x108 (the mid probes) follow the feet's z |
| +0x124 + 4j | ground height under each probe |
| +0x140 + 4j | probe vertical velocity |
| +0x15c + 4j, +0x178 + 4j | water surface (-300 none), in water |
| +0x194 + 0x28 j | the ground contact (an engine hit, x and z set to the probe's) |
| +0x2ac + 4j | clearance (probe y - ground) |
| +0x2c8 + 4j | probe y after integration |
| +0x2e4 + j | landed hard (vy > 20 at the snap) |
| +0x2ec, +0x2f0 | ski length (12, Elemental 4), L = half of it |
| +0x2f4 | **never written** (read as the mid-probe width; heap garbage) |
| +0x2f8, +0x2fc | 1/L, 1/L^2 |

## Update (0x4765b0, from 0x4162c0, returns airborne)

```
simple = !(detail bit 4) || !mech active                 one probe (6) instead of seven
frozen = collider +0x374 || +0x375 || +0x377: ignore the local x/z velocity
skis matrix = mech matrix, translation += (v_local x R) * dt in x and z    (predicted)
mid probes' z = the feet's local z (0 when the foot part is gone)
probes to world through the ski nodes; ground query 0x4ee2f0 (below), top = y + 50
g = gravity [0x5e5d34] (30); air = jet impulse > 0 || (clearance 6 >= 4 && !both feet down)
if !air: g *= 4                                          hugs the ground
step = (ceiling hit && air) ? 0 : 0.7 * height
for j = 6 down to the first probe:
  if landed hard && !air && vy > 0: vy = 0
  vy += jet impulse (per frame)
  h = PickGround 0x477820(hits, y, step): highest hit within step above y, water never
      (the mech walks on the river bed; water sets the surface); none valid: the lowest
  integrate 0x477940: vy -= g dt; y' = y + vy dt; above h: y = y'; else
      vy = (h - y) / max(dt, 0.0333), y = h, landed hard = vy > 20
  c = y - h; !air && c < 3: snap y = h, vy -= c / dt
  !air: slope slide 0x477620: steeper than n.y 0.85 and not noslip: push += 30 dt n.xz
  vy < 0: vy = max(vy, vy 6)
simple: probes 0..5 copy probe 6; the push is tripled when sliding (per frame)
per ski: pick the regular segment; slope = -(dh/dz); s += (slope - s) * 2 dt, snap 0.01,
  clamp to sin 35 deg; offset = clamp(slope z1 + y1 - y6, -0.8, 1)
  blocked step (> 3 m up at the front or back, clearance <= 0): push +-1 along forward
  clamped slope: push along the ski by slope * 0.8 / 0.5736;   foot down = |clearance mid| < 0.2
one ski down and the other not, clearance differs by > 1.5: push 1 sideways toward it
V = mean vy (simple: vy 6), capped at (jet ceiling [0x5e5d44] - y) / 4; probes y += V dt
ski pitch asin(s) and offset to the ski nodes (0x4764a0), mech local vy (+0x40) = V
both down and !air: every vy = 0
zone 0x476550 (contact 6's zone set -> mech +0x64)
airborne = jet impulse > 0 || (|clearance 6| >= 0.2 && neither ski down)
```

The body never tilts (the orientation is yaw only); the legs follow slopes
because the animation's feet are evaluated in the pitched ski frames (anim
vt +0x1c 0x41fbe0) and solved by the leg IK (0x463e40 / 0x463c90, not mapped).

## The terrain query (0x4ee2f0, `zclass\cls_di.c`)

Ported (`zclass/cls_di`, the polygon test in `zmodel/gmod_hit`) and checked
on the six worlds in `test_di.py`; the skis use it by default.

Not a ray: per probe a flat x/z point-in-polygon test over the probe's grid
cell (`floor((x - W+0x34) * W+0x74)`, same for z; off-grid probes wrap through
0x4f1f10). Nodes with ACTIVE and ALTITUDE_SURFACE, then the cell's 0x44-byte
entries (empty in the shipped worlds), then the world's children; object3d
matrices are pushed. A polygon counts when n.y > 0 and its height at the
point (0x54c330) is at most `top`. Inside test per edge, backwards:
`(p.x - vj.x)(vi.z - vj.z) + (p.z - vj.z)(vj.x - vi.x) > -0.0001`; flag 0x400
is a strip. Out per probe 0x504 bytes: count, then up to 32 hits of 0x28
(normal, +0x10 height, +0x18 zone set, +0x20 material, +0x24 node), unsorted.
Each model caches its last polygon per probe (header +0x58). Always returns 0.
Water is a separate flat polygon (y 0, soil 1).

## Feet (0x462a10, Foot::GroundUpdate)

World position (0x4ed530); ground = the ski parabola at the foot's z
(0x475de0, which also picks the contact: mid when within +0x2f4 / 2, else
front or back); water from the same probe (0x475ea0). Tilt to the contact
normal (0x462de0, with detail bits 2 and 4). Clearance = (y - ground) * n.y -
sole; state +0x94: 0 air, 1 touching (clearance falling below 0.25), 2, 3
planted, back to 0 above 0.2. Water depth +0xdc. On touch: soil from the
material, and a node damage callback with 1000 (stepping on things).
Foot::Update 0x463110: the turn-in-place counter-spin (+0xd8 += +0xd4 dt,
else decays by exp(-3 dt)); 1->2 a camera shake for the player, 2->3 a
splash in water or a footprint, the step sound at `0.55 |throttle| + 0.45`.

## Soils (table 0x608c00 by material +0x20)

1 water (surface, not ground; splashes), 10 noslip (no slope slide), 8 lava
(heat only). Footprint decals and step sounds per soil (pilot +0x230). No
soil changes speed.

## Torso, hip, arms

Torso inputs 0x466200 / 0x466240 (twist from controls +0x1c, pitch +0x18, the
free-look +0x30 / +0x2c; a non-zero input cancels the AI aim). Torso::Update
0x464e80, mode +0x84 (1 player, 0 AI aim, 2 frozen):

- Player rate mode: yaw and pitch += dt * input (x 0.3 with HUD flag +0x1514
  bit 4 clear... the zoom, likely), clamped to the definition's limits
  (def +0x174 max yaw, +0x178 max pitch, +0x17c min pitch; 0x464ce0); a pitch
  lock decays pitch by 1.75/s. Absolute mode approaches `max * input` at 1.75/s
  (1.4 when centring). Centre legs (+0x1b0 == 2) turns the legs toward the
  torso through the mech's turn.
- AI aim: asin / atan2 of the aim direction, clamped, approached by
  exp(-2 dt).
- Then the shake/jolt springs (substeps of 1/15 s, def `freq`, `damp_freq`,
  `torso_shake`) and the servo sound.

Hip::Update 0x4623a0: crouch offset approaches its target at 2/s, snaps at
0.05, clamped to [-crouch depth, 0] (def `crouch_height`, else 0.7 hip height).
Arm::Update 0x466510: aim toward the target with def `aim_*` limits and
`aim_slerp_freq`, recoil decay (def `recoil_*`).

## Detail bits (+0x688, 0x41e870 by camera distance; the player all)

< 75 m all (one mech per frame), < 250 m 0x1c / 0x1d, < 800 m or the player's
target 0x10, else 0. Bit 0 sounds, 1 toes, 2 foot spin and tilt, 3 footsteps
and landing effects, 4 seven probes, the full gait and the part animation,
5 the lean.

## Quirks

- +0x2f4 is never initialised; with 0 the feet always use the front or back
  probe. FIX (bug) (Owen, 2026-10-20): the port uses L, so the nearest probe
  wins; `zfix bugs` off keeps the exe's (0).
- The in-water test passes up to 0.7 height above the surface.
- The wading movement sound compares the absolute water y with 0.35 height
  (0x41a88e), so it never plays where the water is at y = 0.
- Distant mechs triple their push every frame while sliding (no dt); most
  pushes (step kick, ledge, slope clamp, jet impulse) are per frame.

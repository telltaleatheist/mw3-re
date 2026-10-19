# Mech collision (buildings, mechs, vehicles, mines)

Mech3.exe 1.2, 2026-10-20. Static map from the asm (Ghidra gets the bool of
0x4225c0, the push decay in 0x419fc0 and the arguments of 0x423a20, 0x4237c0
and 0x5836b0 wrong). Part of `movement.md`. Ported (`mech3/mech_collide`, the queries in
`zclass/cls_di`) and checked in `test_collide.py` and `test_di.py`; the
corrections below came out of that.

Corrections: probes 0-2 sit at -z, so ahead of the mech (forward is -z), and
moving forward (local vz < 0) sweeps them, reversing sweeps 3-5. A ceiling
alone does not count as resolved: Collide re-queries only when a vehicle,
mech, bump, touch or wall resolved (+0x374, +0x375, +0x377), and a ceiling
hit with nothing else goes on to the network record. The mech's own node is
cleared of INTERSECT for the query and set again afterwards, whatever it was.
The segment query keeps or drops every segment by the first one's box: its
bounds test reads fixed addresses (0x60b990..), segment 0's (quirk; only the
first matters when the world does not wrap). The bump also moves the probes
by the push times the frame time, and its own sound is slot 13 (+0x5ec).

The code sits between `mech_anim.cpp` and `mech_data.cpp` (likely its own
file; `mech_collide` would fit the alphabetical link order). Everything works
on a collider object embedded in the mech at +0xfc (ctor 0x4222e0 from the mech
ctor 0x414a39, probe setup 0x422360 at 0x416165 once the model exists).

## In the frame

```
Mech::Update 0x419fc0
  if +0x5b0 (1 in the mech ctor; 0 for network-remote mechs, 0x46e2e7):
    blocked = Collide 0x4225c0()
  if !blocked: integrate the yaw (the angular-velocity quaternion step)
  if +0x374 || +0x375 || +0x377: local velocity +0x3c = 0 except y (restored)
  world velocity +0x30 = rotate(+0x3c by the orientation +0x48); if !blocked: += push +0x5a0
  translate the node by +0x30 * dt (0x4fc1b0, object3d.c), moved hook 0x5a85c0
  if ground byte [+0x41c]+5: SetThrottle(0) when +0x30 . push < 0 (xz)
  else: push -= push * 4.5 * dt (0x5e2000), x and z snap to 0 below 0.01
  landmines 0x413560
```

## The collider (mech +0xfc)

| Off | Meaning |
|---|---|
| +0x0fc | 1 = the last query hit nothing |
| +0x100 | hits on vehicles and other entities (node +0x28 bits 0x40000 / 0x80000) |
| +0x104 | hits on another mech |
| +0x108 | ceilings: probe 10 with normal.y < -0.3 |
| +0x10c | walls |
| +0x114 | nodes with a touch callback (node +0xbc slot 1) |
| +0x118 | 0.1: wall restitution, also the push share in mech hits |
| +0x11c | 0.01: yaw kick per unit of speed on a wall |
| +0x120 | probe count, 11 |
| +0x124 | local probe points (vec3[15], 11 used) |
| +0x1d8 | world probe points this frame |
| +0x28c | world probe points last frame (segment starts) |
| +0x374..+0x377 | resolved: vehicle, mech/bump, ceiling, wall/touch/net. The ground object 0x4765b0 reads 374/375/377 |

A hit list entry is malloc(0x48), prepended (the head is the last hit):
normal +0, contact point +0xc, material +0x20, node +0x24, from +0x28,
to +0x34, probe index +0x40, next +0x44.

**Probes (0x422360)**, from the node's box at node +0xa4 (min xyz, max xyz):
`cx = (minx+maxx)*0.5, cy = (miny+maxy)*0.4, cz = (minz+maxz)*0.5`; front and
rear z are `+-(ground +0x2f0) * 1.1` (ground +0x2f0 = 0.5 * 12, 0.5 * 4 for the
Elemental) when [0x5e24d4] (1) and the ground object exist, else 2 min z and
2 max z. Points 0-2 the rear row (min x, cx, max x at cy), 3-5 the front row,
6-9 the feet at (+-foot0 x, 0.75, +-min z), 10 the top (cx, max y, cz).

**Segments (0x422d20)**: corners 0, 2, 3, 5 when |forward speed| < |yaw rate
* 3.3|; when moving, the rear row (reversing) or the front row, and **only for
the player** the two foot probes on that side; probe 10 always, from mid
height to the top. Each segment runs from where the probe was after last
frame's resolution to where it is now: the move is found after it happens,
then pushed back. The query is the engine's `di_segments` 0x4efcd0
(`zclass\cls_di.c`): segment AABBs, the world's partition cells and children,
per node 0x4f0480 (needs node flags 4 and 0x10 and BBOX_NODE 0x100; with
INTERSECT_BBOX 0x20 the box faces are the whole collision), else the model's
polygons (plane crossing, one-sided unless polygon flag 0x100). Up to 32 hits
per segment in walk order, no distance.

**Sorting (0x423010)**, the first rule that matches: another mech's node ->
mech list; an entity -> vehicle list; probe 10 facing down -> ceiling; a node
with a touch callback -> touch list; a near-vertical face (|n.y| < 0.78) on
probes 0-5, or below 0.78 on probes 0-9 while airborne -> walls. Floors and
ramps never block.

## Responses

**Wall (0x4231c0)**: flatten the normal N and the move D to xz; move the mech
(and every probe) out by -2(N.D)N; the new velocity is the move reflected off
the wall, its along-wall part kept at full speed and the into-wall part times
0.1; push = that minus the current velocity. Throttle: the player's drops to
10 %, an AI's flips (controls vt +0x70). Yaw kick: speed * 0.01 away from the
wall. The player gets a force-feedback jolt (0x4335d0 on [0x608620]).
Quirk: the reflect helper 0x51e280 returns -v when N.v is 0.

**Bump (in 0x4225c0)**, when nothing else resolved: the nearest AiTarget
contact that is a mech (or the Mobile HQ) within 9 m pushes the mech away by
(9 - d) * 1.1, adds |forward speed| * 0.6 to the push, sets the yaw rate to
the turn rate * 1.8 away from it, throttle to 10 %, sound 13 for the player.

**Mech (0x423a20)**, the head entry only, unless allied: r = |v_xz|^2 /
max_speed^2 * 0.02, split by tonnage: you take `other / sum` of it on the
torso front (0.5) and both side torsos (0.25 each), or a leg when a foot
probe hit; the other mech takes `yours / sum` on its torso and gets 0.1 x
your share of your velocity added (a network message 0x18 for remote mechs).
Then the wall response.

**Vehicle (0x4237c0)**: a non-allied vehicle or the Mobile HQ takes
|v_xz|^2 / max_speed^2 * 20 damage (vt +0x160); if that kills it nothing
blocks; otherwise its body is shoved by v * tonnage * body +0x9b0 * 0.1,
and the wall response runs. Dead units are ignored by the foot probes.

**Touch (0x423c90)**: buildings and animated objects with a damage handler
take 20 per frame of contact (0x5836b0 -> node +0xbc slot 1); while they
stand, the wall response blocks.

**Ceiling (0x423600)**: airborne and rising: vertical speed * -0.66, the mech
put just under the contact, 0.002 head damage. On foot: damage by the volume
of the box hit (dx dy dz / 180 / 4): head 0.1, torso 0.4, sides 0.25 each, and
the hip crouches (0x4622a0, sound 12).

After the responses the segments are queried again; the mech stays blocked
while touch, ceiling or wall hits remain, and only then are the probe
positions kept as next frame's starts. A network-synced collision record
(+0xd8) can also fire the wall response.

**Landmines (0x413560)**: `landmines.zrd` (`weapon`, `radius`, default 20 m)
fills a list at 0x604e48; a mech within the radius fires the mine's weapon
at the ground and disarms it.

## Porting order

1. Pure helpers: reflect 0x51e280, normalize 0x4020d0, flat normalize
   0x51e210, TurnRate 0x418f70, probe setup 0x422360, sort 0x423010.
2. The responses 0x4231c0, 0x423a20, 0x4237c0, 0x423600, 0x423c90 on a fake
   mech with recorded part damage.
3. 0x422d20 with `di_segments` stubbed by canned hits; then 0x4225c0 whole.
4. `di_segments` 0x4efcd0 itself (cls_di.c), on the worlds test_gamez loads.

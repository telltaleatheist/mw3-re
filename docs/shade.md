# The mechs' sun shading ("shadow parts")

Mech3.exe 1.2, 0x004e61e0..0x004e6990 (original file unknown: between
zrdrvalve.cpp and zClass's class.c). Ported 2026-10-21 (mac 2) as
`src/mech3/mech_shade.{h,cpp}`; checked by `tests/differential/test_shade.py`
(500 random scripts identical, 13 breaks caught). Census #9 (playtest.md).

## What it does

The options' "shadow parts" (0x0040d1f0, `ShadowParts_SW/_HW`) switches a
mech's lighting to follow whether it stands in the sun:

- Every mech has a shade (+0x68c, 0x24 bytes, vtable 0x005cd5e4, made by the
  build: 0x004e61e0) holding a **level** (+0x10: saved, target, current;
  0x0c bytes, 0x004e6250) and its **turn** (+0x20, the count of shades made
  so far, [0x0060b7e4]). Every mech part has a level of its own (+0x68).
- Each frame the turn moves on by one ([0x0060b7e8], wrapping at the
  count; once a frame by [0x0072f338]). On its turn a mech (0x004e6420)
  casts a ray from its centroid (its world position, the matrix redone when
  stale) 150 m towards the sun (the light's direction +0x84 times -150),
  with its own node left out and the query in first-hit mode excluding
  0x40000 (0x004e6270). Blocked: the target is 0; open: 1.0 ([0x005e9cdc]).
  The mech is flagged while its current level is not the sun's diffuse.
- Every frame the level fades towards the target at 1 a second
  ([0x005e9ce0]), clamped, and the mech's node gets the flag (+0x2c bits 5
  and 6, 0x004eb8a0 / 0x004eb8e0).
- The scene walk calls 0x004e6720 before a flagged node draws and 0x004e6770
  after (installed by the switch, 0x004f7630 / 0x004f7650): the sun's
  diffuse (light +0x9c) is swapped for the level of the node's part
  (0x0040ef30, +0x68) or mech (0x0040ef10, +0x68c +0x10), and the model
  draw's light bake is off meanwhile ([0x005ef740] 0, [0x0060b94c] 1).
- The parts' own rays, fades and flags (0x004e63f0, 0x004e6640, the parts
  loops of 0x004e6480) never run: [0x005e9ce4] is 1. A part's level is
  only reset (to the sun's diffuse, its node unflagged: 0x004e66a0).
- The switch 0x004e6820 takes the world's sun ([0x006098d4] +4, light
  +0x38); on with a sun it installs the calls and restarts the round, off it
  clears them; either way every mech's level is reset (0x004e67c0). The
  option (0x004e6920) is kept in [0x0060b7f0] and applied at once during a
  mission ([0x0060991c] > 0), else at the mission start (0x004e6900, the
  Core state's arg 7); the cleanup switches it off (0x004e6910).

## Quirks (kept)

- A mech in the sun fades to diffuse 1.0, not to the sun's own diffuse.
  The worlds' suns: c1 1.0, c2 1.0, c3 0.2, c4 1.0, c4b 0.7, t1 0.7. So on
  c3 (and less so c4b and training) a mech standing in the open is drawn
  in full light under a dim sun, and in shadow it goes black (0); only
  while it is exactly at the sun's diffuse is it drawn like the world.
  Candidate for Owen (fixes.md): the sun's diffuse as the lit target.
- Only one mech is tested a frame, so with many mechs a mech's shade
  answers late (its turn comes round every N frames); the fade (1 a
  second) hides it.

## In the program

`app_mech.cpp` (WiredShade): the mission's sun light, the mechs' nodes and
centroids, zClass's segment query and node flags, the scene walk's
`pre`/`post`, `zmod::draw_state().static_lights`, `zcls::frames()`. The
build allocates the shade and the parts' levels (`alloc(0x24)`,
`alloc(0xc)`), the mech update runs it (`UpdateCalls::x68c_update`), the
frees delete them. The options' hook (`shadows_set`) and the Core state's
`shadows_reset` (0x004e6900) and the cleanup's 0x004e6910 are wired.
`mission_c1m1_mech_sun_shading_headless`: on with the option, every mech
shaded, the sun's diffuse back after each frame.

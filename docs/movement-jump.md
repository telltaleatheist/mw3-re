# Jump jets, landing, falls, getting up, knockdown

Mech3.exe 1.2, 2026-10-20, from the asm. Part of `movement.md`; what is
ported and checked is listed there.

Axes: the mech's local velocity is +0x3c (x right, y up, z); **forward is
-z** (the jets do `z -= thrust` at a forward throttle, a landing sets the
throttle to `-vz / max_speed`, the torso death tests `+0x44 < -13.89` for
"fast forward"). So `docs/damage.md`'s centre-torso death has fallf and fallb
the other way round: fallf (1) when airborne or moving forward faster than
50 km/h, fallb (2) when moving backward that fast.

## Names

States +0x578 (names at 0x43cc00): 0 limp, 1 stand, 2 stand_step, 3 step,
4 step_walk, 5 walk, 6 walk_trot, 7 trot, 8 trot_run, 9 run, 10 dead,
11 falling, 12 fallenback, 13 fallenfront, 14 suspended (vt +0x104 0x41b980,
player only; undone by +0x100; Update returns at once).

Actions, kind 1..8 (names 0x4316e0, motions `<mech>_<name>`), objects at
`+0x458 + 4 * kind` made at 0x415d05: 1 fallf, 2 fallb, 3 landf, 4 landb,
5 getupf, 6 getupb, 7 collapsel, 8 collapser. The queue is a `std::list<int>`
(+0x47c, head +0x480, size +0x484).

Pilot sounds (0x432990, `pilot + 0x28 + 4 id`): 3 `mech_jumpjet`, 0x12
`mech_falling`, 0x13 `mech_fall`.

## Jump jets (0x4173b0, from the gait's 0x417a80)

```
if jets (+0x3d8) < 1: stop the jet sound (+0x5c4) and flame (+0x620); return 0  (+0x580 not cleared)
assist = player && !MW2JJets option ([0x5f4d04] bit 7, 0x40d1e0)
T = controls GetJumpThrust (vt +0x38); firing = 0
need = (not firing and not airborne) ? 0.25 : 0
if fuel +0x564 > need:
  if T > 0: impulse +0x598 = (T * 8 + gravity [0x5e5d34]) * dt
  firing = T > 0
  if assist: if firing and height above ground (0x41e9c0) > height * 0.5:
                vz -= (throttle < 0 ? -1 : 1) * T * 5 * dt * 0.7       (no clamp)
  elif airborne:  vz -= 5.5 dt * JJ forward (+0x28); vx += 5.5 dt * JJ strafe (+0x24) (each sets firing)
                  vz = clamp(+-max_speed), vx = clamp(+-40)
impulse = clamp(0, 6)
firing: fuel -= burn +0x560 * dt;  on the ground and fuel < 1: fuel += burn * dt * 0.5
fuel = clamp(0, 1); +0x568 (always 0) would make it infinite
sound 3 at 0.3 on firing (detail bit 0); flame effect on the jet node +0x3a8; HUD 0x433870
end of firing with the jump pose +0x454: its vt +0x44(-1); +0x580 = firing; pose the legs, hip, torso
```

Net climb is 8 T m/s^2 against gravity 30. Burn (0x417300 at setup, once per
jump-jet item): `tonnage / (100 * jets)`; 50 t with 4 jets empties in 8 s,
refills at half that rate on the ground.

## The contact solver ("skis", +0x41c, mech_skis.cpp)

0x4162c0 runs 0x4765b0 every frame (also during actions) and stores its
result in +0x584 (airborne). Vertical part:

```
air = impulse > 0 || (point 6 gap >= 4 && !(left foot down && right foot down))
g = gravity, x4 (120) unless air                     sticks the mech to the ground
per contact point (7, or only 6 at low detail):
  if stepped && !air && vy > 0: vy = 0
  vy += impulse
  vy -= g dt; y' = y + vy dt
  above the ground: y = y'; else vy = (ground - y) / max(dt, 1/30), y = ground, stepped = vy > 20
vy = mean of the points' vy, capped at (ceiling [0x5e5d44] - y) / 4     every frame, not only jumping
airborne = impulse > 0 || (|point 6 gap| >= 0.2 && neither foot down (|gap| < 0.2))
```

The ceiling caps the velocity: the mech closes in over a 4 s time constant
and is pushed down when above it.

## Landing (0x4162c0)

Only for a local, non-Elemental mech that was airborne, is not now, and was
falling (vy0 < 0):

```
imp = -vy0 / 80; hard = imp outside 0..1; HUD jolt (0x433800)
side = |vx| > 20
throttle (current, controls +0x10) = -vz / max_speed; if > 0 state = 5 walk
hard or side: rand > (side ? 0.6 : 0.8) -> Fall(vz <= 0 ? 3 : 4), return      20 % / 40 %
imp > 0.7: each leg takes imp * 0.005
detail bit 3: SetThrottle(0), a 50 m ray down: a splash on water, dust otherwise; landing pose
```

## Falls and getting up

```
Queue 0x416780(kind, force): kind's action exists, not dead, local or forced,
    not the same as the last queued -> push_back, net event 0x17
Fall 0x41b660(dir): not in 10/11/12/13: 3 -> fallf + landf, 4 -> fallb + landb
    (another dir: true without queueing); on success the hip resets, HUD; knock cleared
0x41b640 (thigh destroyed): Fall(vz <= 0 ? 4 : 3)   forward -> backward fall
Stagger 0x41b7f0(v): Fall(v . (+0x24, +0x2c) > 0 ? 4 : 3)
DeathFall 0x41b760(k): k, then landf/landb for 1/2
GetUp 0x41b840: in 12/13, active, powered: getupb from 12, getupf from 13
```

ActionUpdate 0x416860 (when the queue is not empty) plays the front action:
on start the state from 0x416660 (fall 11, landf 13, landb 12, death states
when a vital part is gone), the sound, throttle 0, the fall camera, feet and
weapons off for falls; then each frame the momentum +0x488 x 0.6 (per frame),
the solver, Advance(dt), and the root motion: `vx = (root.x - last.x) / dt +
momentum.x`, the same for z; the pose goes to the parts. When it ends: after
landf/landb each leg takes `tonnage / 10000` (`(1 - clamp((100 - t) * 0.01))
* 0.01`), velocity cleared, lying timer +0x590 = 5 s; after getup state 1,
knock cleared, weapons and feet back, controls zeroed. The next action in the
queue must fit (landf after falling or after landb (typo-like; harmless),
getup only from 12/13), else it is dropped.

Update counts +0x590 down while the queue is empty, the mech is in 12/13 and
powered up; below 0 (and local) GetUp. Down time = fall + land + 5 s + getup.

## Knockdown

TakeDamage (states 1..9 only, so not while limping): `knockvec += IMPACT_FORCE
* flat(shot from - to)`, `knock = |knockvec|`. Each gait frame (0x417a80, not
airborne, not during actions, local only):

```
T = (max(tonnage, 10) - 10) / 90 * +0x6c8 + +0x6c4      3.76 at 10 t .. 15.04 at 100 t
if T <= knock: normalise knockvec; player: cockpit sound 24; Stagger(knockvec); knock = 0
```

Decay in Update: `knock -= 6 dt`; below 0.2 it and the vector clear, else the
vector scales with it.

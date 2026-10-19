# Mech movement and physics

Mech3.exe 1.2, 2026-10-20. How throttle and turn become motion, from the asm.
The pieces:

- this file: the frame, controls, gait, turning, the motion integration, the
  animation objects, and what is ported;
- `movement-jump.md`: jump jets, landing, falls, getting up, knockdown;
- `movement-ground.md`: the ground probes ("skis"), the terrain query, the
  feet, torso, hip and arm updates;
- `movement-collision.md`: buildings, mechs, vehicles, touch damage, mines.

Axes: the local velocity +0x3c is (x right, y up, z), **forward is -z**
(`movement-jump.md`). Units are metres and seconds.

## The frame (Mech::Update 0x419fc0, vtable +0x34)

```
dt = +0x6b0 += dt                  accumulated: a frame the update budget 0x5a8910 skips carries over
+0x594 (jet impulse vector) = 0
budget gate 0x5a8910; FinishPowerDown; +0x414 += dt
state 14 (suspended): return.  NARC timer +0x570 -= dt
dead (10) with no action queued: deactivate, stop all sounds, 0x41e350, return
player: camera view keys (0x426780)
controls Update (vt +0x0c) unless dead with actions; 0 -> skip to the tail
0x5a8960; player 0x41ce60(dt)
yaw rate +0x5c += -yaw * 3 dt, 0 below 0.05; +0x58 = +0x60 = 0
not jetting and not airborne: local x velocity +0x3c = 0
if controls +0x58 || +0x59 (enabled): actions queued ? ActionUpdate 0x416860 : Gait 0x4181c0
state 12/13, no actions, powered: lying timer +0x590 -= dt, GetUp at < 0 (local)
no action: Turn 0x418fc0, torso inputs, torso and hip Update
collision (+0x5b0): Collide 0x4225c0
no action: legs and arms Update
powered down: angular and local velocity = 0
lean (detail bit 5): +0x54c toward def lean_angle * |thr| * turn * speed factor
no block: orientation +0x48 = normalize(expmap(dt/2 * w) * q)
collider stop: local velocity = 0 but y
world velocity +0x30 = rotate(q, local) (+ push +0x5a0 unless blocked); move the node by +0x30 dt
push decay or throttle stop (movement-collision.md); landmines; vt +0x14c
weapons; 0x419200; UpdateHeat; movement sounds; controls swap; 0x4e6480; +0x6b0 = 0
tail: vt +0x158(dt), knock decay, 0x413cb0
```

## Throttle and turn (MechControls::Update 0x426960)

The controls object (`controls.md`) holds the commanded targets; every kind
of controls ends its update in this base smoothing (the network class uses
only it):

```
powered down: zero every axis (0x426c80)
throttle: |target - cur| < 0.009: snap; else rate = |cur| <= |target| ? +0x530 : +0x534,
          k = exp(-rate dt) (fast exp: float bits ftol(x * 12102200) + 0x3f800000),
          cur = (1 - k) target + k cur
turn:     cur += (target - cur) * +0x538 (x2 with flag 4) * dt, snap within 0.05
jump:     cur += (target - cur) * (target == 0 ? 9 : 7) * dt, snap within 0.05
```

`controls.md` had 7 and 9 the other way round. +0x530 / +0x534 / +0x538 are
the definition's `accel_rate`, `decel_rate`, `turn_accel_factor` (config
+0x7c..+0x84, copied at 0x4144a5). SetThrottle 0x426b60 clamps to -1..1 and
sets the reverse flag by the sign.

## Speed and gait (Gait 0x4181c0)

The throttle is a fraction of the gait table, not of a speed. Per mech:

```
D = state not 1/2/3 and |throttle target| >= step.hi (+0x4bc)
fallen (12/13) or powered down: return
F = both feet planted (+0x94 == 3); E = turning or torso centring legs (+0x1b0 == 2)
airborne: Move(dt, no anim, throttle); return
f = 1; heat > 0.6: f *= clamp(0.6 / heat, 0.6, 1)
d = shallower foot's water depth; d > 0.1 H: f *= d >= H ? 0.7 : 0.7 (H - d) / H   (wading FF effect)
limping: f *= 0.6; reverse: f *= 0.6
+0x52c = f; speed factor +0x528 += (f - +0x528) * 0.6 dt
thr *= +0x528; spd = |thr|; crouched and spd > step.hi: stand up
leaving stand/step (index <= 3, spd >= walk.lo, !D): copy the step's phase to walk and the
  step/walk blend; wait for a footfall of the step (its event times within 1.25 frame dt
  of the phase t / loop, by direction); until then hold spd at walk.lo - 0.0001 and
  thr2 = clamp(5 thr2, +-5 L)
may stop = !E && (spd <= step.lo || (spd <= walk.lo && target == 0)) && !D
           and, from the step with a foot up, only within 2 dt of the step's loop start
detail bit 4 clear (far away): walk, state 5
limping (+0x5ac 0/1) with a limp blend: stop -> stand; spd >= walk.lo -> limp (state 0,
  index 8, the blend's weight 0.5, thr clamped to trot.lo)
stop: stand (index 2, state 1), +0x58c = 0
spd < walk.lo && !D: step (3), speed (0.4 + thr2) * 2.5, +0x58c = 0
spd < step.hi && !D: step/walk blend (8, state 4), weight (spd - walk.lo) / (step.hi - walk.lo)
spd < trot.lo: walk (4, state 5);  < walk.hi: walk/trot (9, 6);  < run.lo: trot (5, 7)
spd < trot.hi: trot/run (10, 8);   > run.lo: run (6, 9); then +0x58c++
each switch copies the old animation's phase into the new one (0x41ef00)
```

States (+0x578, previous +0x57c) are named in `movement-jump.md`. Animation
slots (+0x424 + 4i, made by 0x415ca0): 0/1 limp left/right, 2 stand, 3 step,
4 walk, 5 trot, 6 run, 7..10 the blends stand/step, step/walk, walk/trot,
trot/run, 11 walk/limp (StartLimp), 12 the jump pose (the jets play it; the stand when missing).

**The gait table** (+0x4a0, 13 pairs lo/hi): the mech data's "trans" pairs
read from each mech's `animations / motion / {stand, step, walk, trot, run}`
(0x431350, mech_data.cpp), with stand.hi forced to 0.01, step to (0, 0.05)
and walk.lo to 0.02 whatever the file says (0x431a70), then scaled by
`base_speed / max_speed` each time the max speed changes (0x4329c0 from
set_max_speed 0x4143c0). So MASC (max speed x1.25) lowers every threshold by
0.8. The mech offsets: +0x4b8 step.lo, +0x4bc step.hi, +0x4c0 walk.lo,
+0x4c4 walk.hi, +0x4c8 trot.lo, +0x4cc trot.hi, +0x4d0 run.lo.

## Move (0x417a80) and root motion

```
grounded, local: knockdown check (movement-jump.md)
anim: SetSpeed(speed) (vt 08), Advance(dt) (vt 10), GetVelocity (vt 20): writes +0x44 only
jump jets 0x4173b0; ground and landing 0x4162c0
no anim or far away: the feet's ground update (0x462a10) and return
sample the feet (in the ski frames), hip (its z dropped), torso, arms, hands, toes
the first 20 frames of walk or faster without jets: blend from the last pose by
  clamp(+0x58c / 20, 0.15, 1), velocity included (stuck at 0.15 in the step/walk blend,
  where +0x58c stays 0); the right foot's rotation is never blended (q0 used twice: bug)
leg IK 0x463e40 per leg; hip, torso, arms, hands, toes set
```

So the speed sets the animation rate and the animation's hip velocity is the
mech's forward speed: `+0x44 = hip velocity(t).z * rate`, rate = speed x
stride multiplier (`s * loop / stride`, set by 0x41ef90 from the max speed).
Stand and step have no velocity keys: they move nothing.

## The animation objects (mech_anim.cpp)

| Class | Size | Vtable |
|---|---|---|
| MechAnim (over CZSIScript) | 0xdc | 0x5bd1f0 |
| BlendAnim (A +0xe0, B +0xe4, weight +0xdc) | 0xe8 | 0x5bd240 |
| LegOneShot (the actions) | 0xf0 | 0x5bd290 |

MechAnim: script +0x04 rate, +0x08 stride multiplier, +0x0c stride, +0x54
loop, +0x88 time t (before the rate), +0x8c stride speed, +0xd0 / +0xd4 the
left and right footfall times, +0xd8 a quarter cycle (0x41fd30). The script's
active byte +0x10 is set by these constructors, and the loop time is forced
to frames / 30 (/ 15 for the one-shots) after loading. Methods: SetSpeed
0x41ee80 (`f = mult x; if f: t = rate t / f; rate = f`), Advance 0x41ef60
(`t += dt`), Seek 0x41fce0 (`t = rate * event[i]`), CopyPhase 0x41ef00,
SetStrideSpeed 0x41ef90. Sampling: `phase = frac(rate t / loop)`, keys lerped
and slerped (0x525ce0); velocity keys `(trans[i] - trans[i-1]) * 30`
(0x5aac60).

## Turning (Turn 0x418fc0)

```
turn (controls +0x14) == 0: return (the yaw rate keeps decaying)
a = 0.85 (airborne)  else a = |throttle|, and below 0.99 walk.lo the throttle (current)
                      is set to +-0.99 walk.lo: turning makes a standing mech creep
w = turn * (1 - 0.8 a) * +0x520           +0x520 = def max_turn_rate * 0.5
detail bit 2 and a pivot leg (0x418e00: the one planted foot, else the lower): the local
  velocity += pivot foot position x (0, w, 0); that foot counter-spins (0x462dc0(-w))
angular velocity +0x58 = (0, w, 0)
```

## Integration

The yaw rate decays by 3/s; the orientation quaternion +0x48 (w, x, y, z)
steps by `expmap(dt/2 * w)` (0x525900 uses the full angle, so this is the
exact rotation) and is normalised with the bit-trick square root (0x525500);
the world velocity is the local one rotated (0x525840) plus the push +0x5a0;
the node moves by velocity * dt (0x4fc1b0). The lean +0x54c (detail bit 5)
rolls the display matrix only.

## MASC (0x41df50)

With MASC equipped: on, max speed x1.25 and the throttle rescaled to keep the
current speed, +0x574 = 0.001; off, the base max speed with no rescale, so the
speed drops at once.

## Ported and checked

All at the game's 24-bit x87 precision, bit for bit unless noted; the calls
into code not ported go through `MoveCalls`, `SkisCalls` and the existing
`HeatCalls` / `DamageCalls`, stubbed and logged identically on both sides.

| Piece | Original | Port | Test |
|---|---|---|---|
| Controls smoothing, SetThrottle, zero | 0x426960, 0x426b60, 0x426c80 | `mech3/mech_control` | `test_controls.py`: 400 runs x 120 frames |
| Gait choice, turning, pivot leg, foot pivot; CopyPhase, Seek, SetSpeed, Advance, SetStrideSpeed | 0x4181c0, 0x418fc0, 0x418e00, 0x462dc0; 0x41ef00, 0x41fce0, 0x41ee80, 0x41ef60, 0x41ef90 | `mech3/mech_move`, `mech_anim` | `test_gait.py`: 30000 calls on the 19 shipped gait tables (`mechdata.py`), every gait and state |
| Ground probes: Update, parabola, height, water, ski pitch, ground pick, integrate, slope slide; the vector helpers | 0x4765b0, 0x475c80, 0x475de0, 0x475ea0, 0x4764a0, 0x477820, 0x477940, 0x477620; 0x4020d0, 0x51e3d0, 0x51e210, 0x525840 | `mech3/mech_skis`, `gamez/zmath/zmth_vector` | `test_skis.py`: 14400 frames walking, jetting and falling on the six worlds, the original's ground query 0x4ee2f0 run on each; plus made-up ground for the pushes. The ski pitch's sine and cosine within 2e-7 (the CRT asin is an x87 stand-in) |
| Move, jump jets, AddJumpJet, landing (with the knockdown it starts) | 0x417a80, 0x4173b0, 0x417300, 0x4162c0 | `mech3/mech_move` | `test_jets.py`: 40000 calls |
| Action queue and state machine: Queue, ActionUpdate, StateForAction, SoundForAction, DeathFall, Stagger, GetUp, KnockedDown on the real queue; the action's Reset, Advance, IsStarted, Stop | 0x416780, 0x416860, 0x416660, 0x416720, 0x41b760, 0x41b7f0, 0x41b840, 0x41b660; 0x420f40, 0x420f80, 0x421000, 0x421060 | `mech3/mech_move`, `mech_anim` | `test_actions.py`: 1500 runs, 114750 calls through falls, lying down, getting up, deaths, remote mechs |

Breaking the port showed each test catches what it covers: accel and decel
swapped, the jump thrust rate, the deeper foot slowing instead of the
shallower, the turn creep at walk.lo, the limp's slot 11, water as ground,
gravity x3 on the ground, the ledge threshold, the ceiling's 4 s, the jet
refill rate, the sideways landing's fall chance, the blend-in floor, landb
after fallb, the time lying down, the stagger direction: 15 of 15 caught.

## Left

In the order they would go:

1. **Mech::Update 0x419fc0** as a whole: the frame order above, the yaw
   decay, the orientation step (expmap 0x525900, product 0x5255d0, the
   bit-trick normalise 0x525500), the lean (0x525210, def `lean_angle`,
   `lean_factor`), the world velocity and the move (0x4fc1b0, object3d
   translate-by), the push decay, the knock decay, the lying countdown, the
   movement sounds (0x41a7xx..0x41a9xx). Everything it calls is now ported
   or a hook.
2. **Collision** (`movement-collision.md`): the collider at +0xfc, probe
   setup 0x422360, segments 0x422d20, sort 0x423010, the responses 0x4231c0,
   0x423a20, 0x4237c0, 0x423600, 0x423c90, Collide 0x4225c0; then the
   engine's segment query 0x4efcd0 (cls_di.c) on the real worlds, as
   `test_skis.py` does for the ground query.
3. **The engine's ground query 0x4ee2f0** (cls_di.c) itself: the port's
   `SkisCalls::query` still needs it to run without the emulator.
4. **The animation sampling** (mech_anim.cpp): the motion keys' phase,
   lerp/slerp (0x525ce0) and velocity keys (0x5aac60), so `MechAnim::velocity`
   and `MechAction::root` give the real hip motion; the blends (vtable
   0x5bd240); the gait table loaders 0x431350 / 0x431a70 and 0x4329c0;
   set_max_speed's rescale and MASC 0x41df50.
5. **The pose on the parts**: Move's pose 0x417bbc.., the jump pose
   0x4177e7.., the actions' pose 0x416fb8.., the leg IK 0x463e40 / 0x463c90,
   and the part updates (torso 0x464e80 with 0x464ce0, hip 0x4623a0, arms
   0x466510, feet 0x462a10 / 0x463110), mapped in `movement-ground.md`.
6. The player's controls (0x427d60, input) and the AI's (0x4e96f0), the
   crouch 0x41bda0, the landing and fall effects (rays, dust, splashes).

# PM's AI structs, field by field (2026-11-08)

The switch's item (3) (`pm-tests.md`, The switch's plan): every field of each struct the PM AI port owns compared
against the exe at least once, or a line saying why not. The structs are those in `src/mech3/zai_*_pm.h` that carry
the exe's offsets (286 fields in 35 structs; the hook classes and the wiring's records hold none of the exe's memory).

Three ways a field counts as compared:

- **out**: read back off the exe's object after the code under test runs and compared with the port's, in the test
  named.
- **start**: compared as the constructor or the start-up leaves it, every word, in `test_pm_ai_defaults` (new with
  this audit: the firing's defaults 0x60b688 from 0x4ba660, the aim 0x4bad70, the fighting state 0x4d9b10, the
  steering 0x4dc0f0 after the start-up's initializers of its statics, the list of who hit a pilot 0x4b2950;
  5 of 5 breaks).
- **in**: set at random by the test as the code's input, so its reading is compared through what the code does with
  it. Where the value comes from in play is then the question, answered per struct.

## The steering and the walking contexts (`zai_steer_pm.h`, `zai_walk_pm.h`)

| Struct | Fields | How |
|---|---|---|
| `SteerPm` | +0x04..+0x38, +0x9c | out: `test_pm_ai_steer` (each step), `test_pm_ai_walk` (+0x04..+0x20, +0x2c, +0x34, +0x38) |
| `SteerPm` | +0x2c..+0x98, the constants | out: `test_pm_ai_walk` (every constant, after the walking context's constructor), start: `test_pm_ai_defaults` (all of +0x04..+0x9c) |
| `SteerPm` | `brain` (+0xa0) | not compared: a pointer, set by the port's wiring to the brain whose orders say whether it steers (0x4fa470, its result compared in `test_pm_ai_steer`) |
| `WalkPm` | +0x18..+0x30, +0x38..+0x4c, `backing` | out: `test_pm_ai_walk` |
| `WalkPm` | `vehicle` +0x08, `pos` +0x0c, `fwd` +0x10 | in: the vehicle and its place and facing, set by the case (the exe keeps a pointer at +0x0c; the port a copy) |
| `WalkPm` | `dir[1]` +0x34 | not compared: the way's height; the walk reads only x and z (`dot`, `cross`) |
| `WalkFightPm` | +0x58..+0x8d | out: `test_pm_ai_walk` (the way's pointer +0x78 as which of the three it is) |
| `WalkGuardPm` | +0x58..+0x72 | out: `test_pm_ai_walk`; `target` +0x50 in |
| `FightStatePm` | +0x08..+0x34 | out: `test_pm_ai_walk`, `test_pm_ai_attack` (+0x08..+0x18); start: `test_pm_ai_defaults` |
| `MoveStatePm`, `DeployStatePm`, `DeployNodePm` | all | out: `test_pm_ai_walk` (the move's +0x0c..+0x24, the deploy's +0x0c..+0x34); `DeployNodePm`'s two places in |
| `CircleStatePm`, `CirclePm` | all | out: `test_pm_ai_walk` |
| `ManoeuvrePm` | +0x0c..+0x18, +0x2c | out: `test_pm_ai_walk`; `no_jets` +0x08 in |
| `GuardPickPm`, `GuardStatePm` | all | out: `test_pm_ai_walk` |
| `BehindPm`, `PassPm`, `CloseInPm` | all | out: `test_pm_ai_walk` (each made by the exe's own constructor, 0x4d8690, 0x4e1750, 0x4df4a0) |
| `KeepRangePm` | all | out: `test_pm_ai_walk` (constructor 0x4d7ac0) |

## Jumping, links, attack (`zai_jump_pm.h`, `zai_node_pm.h`, `zai_attack_pm.h`, `zai_attackers_pm.h`)

| Struct | Fields | How |
|---|---|---|
| `JumpPlanPm` | all, +0x00..+0x38 | out: `test_pm_ai_jump` (every word after 0x4dac40) |
| `JumpPm` | all | out: `test_pm_ai_jump` (after the constructor 0x4daf40 and each phase) |
| `LinkWayPm` | all | in: the way finder's answer, set by the case; the pick's answer compared in `test_pm_ai_link` |
| `AttackStatePm` | `x38`, `hold_at`, `hold_net`, `hold` | out: `test_pm_ai_attack` (the hold point bit for bit, the two pointers as set or not) |
| `AttackBrainPm` | `kind`, `x5c`, `x60`, `mode`, `xfc`, `x108`, `x1f0` | out: `test_pm_ai_attack` |
| `AttackBrainPm` | `delay`, `x58`, `xb4`, `place` | in: the brain's, set by the case |
| `PendingPm` | both | out: `test_pm_ai_attack` (the list pushes logged) |
| `AttackersPm`, `AttackerPm` | the list's records | out: `test_pm_ai_attackers` (every record after each call) |
| `AttackersPm` | `rate_`, `limit_`, `stamp_` | start: `test_pm_ai_defaults` (0.3 a second, 5 s, the clock); then in, `test_pm_ai_attackers` |

## The brain's tick and orders (`zai_brain_pm.h`)

| Struct | Fields | How |
|---|---|---|
| `BrainTickPm` | `mode`, `delay_until`, `wait_until`, `react_at`, `aim_at`, the side's `wait_until` | out: `test_pm_ai_brain` |
| `BrainTickPm` | the pilot's numbers +0x1c..+0xcc (`sight` .. `engage_flags`) | in: `test_pm_ai_brain`. In play they are the brain's own block, read by the same offsets (`pm_params`: `p.get(0x1c)` for +0x1c, and so on), reloaded each frame since the first run (`pm-tests.md`) |
| `BrainTickPm` | `busy`, `home`, `attack_until`, `give_up`, `runs_down`, `radio`, `health_floor`, `leash` | in: `test_pm_ai_brain` (the side pieces 0x4fa5a0, 0x4fab30, 0x4fbe90); seeded from the brain's MW3 twins (`pm_seed`), which the MW3 brain's tests compare |
| `OrdersPm`, `OrderPm` | the deque, `again`, the pending list; `ready`, `net`, `state` | out: `test_pm_ai_brain` (the orders on the exe's own deque and list) |
| `OrderPm` | `x5` +0x05 | not compared: the port keeps its 0 and never reads it; whether anything in the exe does is not checked |
| `TargetsPm` | `target`, `ally`, `enemy`, `marked` | out: `test_pm_ai_brain` (after 0x4e6330) |
| `TargetsPm` | `flags`, `allies`, `enemies` | in: the contact's and the vehicle's, set by the case |

## Firing (`zai_firing_pm.h`)

| Struct | Fields | How |
|---|---|---|
| `FiringPm` | `phase`, `time`, `range`, `wait`, `busy`, `x40`, `cool_until`, `shot_next` | out: `test_pm_ai_step` (each of 40 frames) |
| `FiringPm` | every word +0x00..+0x68 | start: `test_pm_ai_defaults` (0x60b688, which 0x4ba750 copies) |
| `FiringPm` | the rest (`quick`, `delay`, `rest`, the three cones, `too_hot`, `cool_for`, `cool_below`, `shot_every`, `x3c`..`x54`) | in: `test_pm_ai_step` and `test_pm_ai_shot` (as `ShotShooter`'s thresholds) |
| `FiringPm` | `vehicle`, `brain` | not compared: pointers the port's wiring sets (the exe's +0x6c and the controls') |
| `AimPm` | all nine words | out: `test_pm_ai_aim`; start: `test_pm_ai_defaults`, but +0x18 (`active`): the exe's constructor leaves it as `new` gave it, the port starts it `false`. The frame 0x4badc0 writes it and the lead 0x4bb580 reads it; whether a pilot can lead before its first frame is open (Left) |
| `PickFiring` | `aimed`, `until`, `prefer_next` | out: `test_pm_ai_pick` |
| `PickFiring` | `reach`, `prefer`, `plan`, `brain` | in: `test_pm_ai_pick` (the two pointers are the wiring's) |
| `PickPilot` | all | in: `test_pm_ai_pick` (the brain's numbers, its marker) |
| `PlanPm` | `slot`, `score`, `faces`, `in_sight`, `part` | out: `test_pm_ai_pick` (the plan made) |
| `PlanPm` | `dir`, `seen`, `fired` | out: `test_pm_ai_plan` (+0x18..), `test_pm_ai_step` (+0x10) |
| `PlanPm` | `vehicle`, `place` | in: `test_pm_ai_plan` |
| `ShotShooter` | the five thresholds | in: `test_pm_ai_shot`; their values are `FiringPm`'s +0x38..+0x48 (start: `test_pm_ai_defaults`) |
| `WeaponLinePm` | all | in: a view of the weapon's own fields for 0x4b7800, set by `test_pm_ai_aim`'s cases; in play `line_of` reads them by the offsets named, unchecked against the exe beyond reading |

## Left

- `AimPm::active` before the first frame (above): if the lead can run first, the exe reads heap garbage there
  and the port `false`.

- `WeaponLinePm`'s reads in play (`line_of`): by reading only. The weapon's fields are MW3's (`weapons.md`), so the
  offsets are the shared ones; a wrong one would show as a line from the wrong place, which `test_pm_ai_aim`
  would not catch.
- The fields marked "in" with their source in the brain (`BrainTickPm`'s pilot numbers, `AttackBrainPm`): they hold
  what the brain holds at the same offsets in the exe; the brain's loader isn't PM's own (MW3's, `test_ai_readers`).

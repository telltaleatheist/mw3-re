# Mech controls (Mech +0x37c)

Mech3.exe 1.2 (researched by a subagent, spot-checked: GetThrottle 0x428510, SetThrottle 0x426b60), 2026-10-19. Static analysis only (objdump + func2file). Guesses are marked "likely".

## Summary

- `Mech +0x37c` points at the start of a **controls object**. Every kind of controls
  derives from one class, called here `MechControls` (ctor 0x4265a0, mech_anim/mech_data
  area). It has a **virtual base**, called here `IControls` (vtable 0x5bd500, default
  methods in the veh_basic area, 0x4aed60...). The vbptr sits at +4, and the vbtable gives
  the offset to `IControls`. Mech code also calls `MechControls` methods directly (0x426780),
  so the pointer has the `MechControls*` type, not `IControls*`.
- **vtable +0x1c is `GetThrottle()`.** It returns the current, smoothed throttle in -1..1
  (negative means reverse). That makes heat's `|value| > 0.6` test a test on throttle.
- **+0x38 is `GetJumpThrust()`**: the current, smoothed jump-jet thrust (0..1).
- **+0x4c is `Stop()`**: it sets every movement command (throttle, turn, jump, strafe) to
  zero. `Mech::Power` calls it when the mech powers down.
- Concrete classes:

| class (name here) | ctor | size | primary vtable | IControls vtable (at offset) | made by | role |
|---|---|---|---|---|---|---|
| MechControls | 0x4265a0 | 0x74 | 0x5bd4e0 | 0x5bd428 (+0x60) | (base) | shared state + smoothing |
| PlayerControls | 0x427320 | 0x160 | 0x5bd710 (+0x5c: 0x5bd708) | 0x5bd650 (+0x14c) | 0x427280, singleton `[0x6056ec]` | keyboard/joystick input |
| PocketControls (likely name) | 0x4690d0 | 0x1dc | (derives PlayerControls) | 0x5c2ef0 (+0x1c8) | 0x469940 kind 2, for CMD_DIRECT_POCKET | temporary stand-in for the player's controls |
| NetControls (likely) | inline in 0x46d450 | 0x74 | 0x5c34e0 | 0x5c3428 (+0x60) | net msg 0xf handler 0x46d880 | remote mech; values come from packets |
| AI base (likely) | 0x4d8670 | | 0x5cd318 (+0x5c: 0x5cd2c8) | 0x5cd0d8 | | |
| AIControls (likely `zAI...`) | 0x4d8750 | 0x320 | 0x5cd318 (+0x5c: 0x5cd2c8) | 0x5cd210 (+0x30c) | 0x49e208, 0x49fed4 (player.cpp area) | AI pilot |

## Layout (offsets from the start of MechControls)

ctor 0x4265a0(owner, isMostDerived), ret 8.

| off | type | meaning | evidence |
|---|---|---|---|
| +0x00 | vtbl | primary vtable (zClass-like: +0x4 returns this, 0x4d2a70) | 0x4265e3 |
| +0x04 | vbptr | vbtable (0x5bd5b8 for the base; vbtable[1] = 0x5c gives the IControls offset) | 0x4265b6 |
| +0x08 | u32 | control-mode flags (bits 1 and 4 load the joystick setup at 0x6040d0..0x604110) | 0x426650 |
| +0x0c | int | view/state 0..3 (switch in 0x426780; mech calls it from 0x41c6cb...) | 0x426780 |
| +0x10 | float | **throttle, current** (smoothed toward +0x44) | 0x428510, 0x426970 |
| +0x14 | float | **turn, current** (smoothed toward +0x48) | 0x426af0, 0x426a10 |
| +0x18 | float | pitch input, smoothed (player) | 0x428314 |
| +0x1c | float | torso twist input, smoothed, +-1.5 (player) | 0x428351 |
| +0x20 | float | **jump-jet thrust, current** (smoothed toward +0x4c) | 0x428530, 0x426a5f |
| +0x24 | float | JJ strafe (CMD_JJ_LEFT -1 / CMD_JJ_RIGHT +1) | 0x42840e, 0x428570/0x428550 |
| +0x28 | float | JJ forward/back (CMD_JJ_FORWARD +1 / CMD_JJ_BACKWARD -1) | 0x428414, 0x428560/0x428540 |
| +0x2c, +0x30 | float | two more smoothed axes (from joystick 0x427870; likely look/free-look) | 0x4283cb.. |
| +0x34 | u8 | set by IControls +0x10 | 0x428580 |
| +0x38 | int | cleared at mech death, which then calls primary vt+4 (likely an order/wrapper pointer) | 0x41aa61 |
| +0x3c..+0x3f, +0x58..+0x5a | u8 | flags (init 0/1); copied over when the pocket mode ends | 0x469ac7 |
| +0x44 | float | **throttle target**, clamped to -1..1 | 0x426b60, 0x428520 |
| +0x48 | float | **turn target** | 0x426b10, 0x426b00 |
| +0x4c | float | **jump target** (+0x3d byte cleared with it) | 0x426b20 |
| +0x50 | u8 | **reverse**: 1 when the last nonzero throttle target was < 0 | 0x426b60, 0x428500 |
| +0x54 | int | init 1 | |
| +0x60 | IControls | virtual base: vtable, then +0x4 a sub-object (ctor 0x4aec00), then **+0x10 (=+0x70) the owner vehicle** | 0x4265c6..cd, 0x428750 |

## IControls vtable (slots as called through the virtual base)

The getters and setters are shared by every class: the derived vtables hold `this`-adjusting
thunks (`sub ecx,[ecx-4]; sub ecx,0xec/0x168/0x2ac; jmp`) to the same functions. The table
below gives the thunk targets. Field offsets are from the start of MechControls (the
functions' own `[ecx-N]` is relative to the virtual base, at MechControls +0x60).

| slot | function | name | what it does |
|---|---|---|---|
| +0x00 | per class | scalar deleting dtor | Mech dtor 0x414f40 calls it with 1 (0x41522c) |
| +0x04 | 0x410510 (ret 0); AI 0x4e9380 | ? | |
| +0x08 | 0x428750; pocket 0x46a440 | `SetOwner(vehicle)` | stores +0x70. Called by `Mech::SetControls` 0x491120 and by the mech ctor (0x414d81) |
| +0x0c | base 0x426960, player 0x427d60, pocket 0x46a360, net 0x46ef60, AI 0x4e96f0 | `Update(dt)` | see below |
| +0x10 | 0x428580 | set byte +0x34 | |
| +0x14 | 0x428490 | `ClearCommands()` (likely) | SetThrottle(0), SetTurn(0), SetJump(0), +0x3c(0), +0x44(0), ... |
| +0x18 | 0x428500 | `IsReverse()` | byte +0x50 |
| +0x1c | 0x428510 | **`GetThrottle()`** | float +0x10 |
| +0x20 | 0x426b60 | `SetThrottle(v)` | v>0: reverse=0; v<0: reverse=1; +0x44 = clamp(v,-1,1) |
| +0x24 | 0x426af0 | `GetTurn()` | float +0x14 |
| +0x28 | 0x426b10 | `SetTurn(v)` | +0x48 |
| +0x2c | 0x428520 | `GetThrottleTarget()` | +0x44 |
| +0x30 | 0x426b00 | `GetTurnTarget()` | +0x48 |
| +0x34 | 0x426b40 | `SetJump(v)` | 0x426b20(v,0): +0x4c = v, +0x3d = 0 |
| +0x38 | 0x428530 | **`GetJumpThrust()`** | float +0x20 (jump jets 0x4173b0 read it) |
| +0x3c/+0x40 | 0x428560 / 0x428540 | `SetJJForward` / `GetJJForward` | +0x28 |
| +0x44/+0x48 | 0x428570 / 0x428550 | `SetJJStrafe` / `GetJJStrafe` | +0x24 |
| +0x4c | 0x426c10 | **`Stop()`** | zeroes +0x24, +0x28, +0x18, +0x14; SetThrottle(0), SetTurn(0), SetJump(0); byte +0x3c = 0; slot+0x10(0). `Mech::Power` 0x41cc30 calls it |
| +0x50 | 0x4287b0 | `AsMechControls()` | returns this-0x60 (the MechControls start) |
| +0x54 | 0x40bae0 (ret 0); AI 0x4d1e60 | ? | |

The IControls defaults (vtable 0x5bd500) return 0.0f (`[0x5c8ea4]`) or do nothing. Slot +0x2c
defaults to calling +0x1c, and +0x30 to calling +0x24.

## MechControls::Update(dt) @ 0x426960 (base; the net class uses only this)

`owner = [+0x70]`:
- If `owner[0xde]` (powered_down) is set: 0x426c80 zeroes every axis, current and target
  (+0x10/14/18/1c/20/24/28/2c/30/44/48/4c).
- Throttle: `d = target(+0x44) - cur(+0x10)`. If `|d| < 0.009`, cur = target. Otherwise
  `rate = owner[+0x530]` when |target| > |cur|, else `owner[+0x534]`, and
  `k = exp(-rate*dt)` (a Schraudolph fast exp: `float_bits(int(x*12102200)) + 0x3f800000`,
  0x5ac340 is ftol), `cur = (1-k)*target + k*cur`.
  So +0x530 is the mech's throttle-up rate and +0x534 its throttle-down rate (likely
  accel/decel). Both are copied from a mech data block at +0x7c/+0x80 (0x4144a5).
- Turn: `cur(+0x14) += (target(+0x48) - cur) * owner[+0x538] * (flags&4 ? 2 : 1) * dt`,
  snapping to target within 0.05. +0x538 is the turn rate, from data +0x84.
- Jump: `cur(+0x20) += (target(+0x4c) - cur) * (target==0 ? [0x5e259c]=9.0 : [0x5e2598]=7.0) * dt`,
  snapping within 0.05. Ported with the throttle and turn (`mech3/mech_control`,
  `movement.md`).

## PlayerControls::Update(dt) @ 0x427d60

It reads input with 0x51d300(cmd) (input-state query on `[0x63cdb8]`; `&3` = held). The
command ids come from the binding table at 0x411150 (CMD_ names):
- CMD_THROTTLE_UP/DN (0x18/0x19): throttle delta +-0.1, then `SetThrottle(+0x44 + delta)`
  (0x428278). CMD_THROTTLE1..0 (0x1b..0x24) go through the callback 0x426cb0, registered
  in 0x426ce0, which forwards to the singleton's primary vt+0x1c (0x4274d0).
- CMD_TURN_LEFT/RIGHT (2/3): turn +-1, then `SetTurn` (0x4282d9). CMD_JJ_ROTLEFT/RIGHT
  (0x11/0x12) also set turn, but only when the mech has jump jets (`mech+0x3d8 > 0`) and
  is airborne (vt+0x144, likely).
- CMD_JJ_UP (0xc) and CMD_JJ_FORWARD/BACKWARD/LEFT/RIGHT (0xd..0x10): jump = 1, then
  `SetJump` (0x428311). The fwd/back and strafe values are stored straight into +0x28/+0x24.
- CMD_TWIST_LEFT/RIGHT (8/9): +-1.5 into +0x1c. CMD_PITCH_UP/DN (4/5): +-1 into +0x18.
  Both are smoothed with `[0x5e25b4]*dt`.
- CMD_RETICULE_ZOOM (0x14) is handled here too (HUD at mech+0x684).
- 0x427870 adds joystick axes to the same locals.
- At the end it calls `MechControls::Update(dt)` (0x428424).

The singleton `[0x6056ec]` is made by 0x427280(mode), called from 0x491150 (mission.cpp),
with mode 3 or 6. A second call only reapplies the mode (0x426650).

## Who sets Mech +0x37c

| site | what |
|---|---|
| 0x414913 | mech ctor path: = 0 |
| 0x414d7b | mech ctor 0x4149f0: = ctor argument, then IControls::SetOwner(this) (0x414d81) |
| 0x415231 | mech dtor 0x414f40: deletes controls (vt+0 with 1) when the mech owns them, then = 0 |
| 0x41aa7c | 0x419fc0 (likely death/cleanup): if `[ctrl+0x38]`, clear it, ctrl = ctrl->primary vt+4() (likely unwraps an order wrapper) |
| 0x41c689 | mech command handler 0x41be80: lance orders swap the controls through 0x469930(ctrl, kind): CMD_DIRECT_PAUSE 0x51 = 1, FOLLOW 0x52 = 0, POCKET 0x4f = 2, 0x53 (no CMD_ name in the binding table) = 9, DEFENDT 0x55 = 0xb, REFITT 0x56 = 0xc. Only when 0x479480([0x60861c]) is 0 |
| 0x46e130 | net mech class: = 0 |
| 0x491120 | `Mech::SetControls(ctrl)`: store, then ctrl->SetOwner(this). Callers: 0x46ad70 (pocket mode ends, restores the old controls), 0x49de3d (SetControls(0)), 0x49e223 (new AIControls 0x4d8750(0, esi, ebx, 1), player.cpp 0x49dc80) |
| 0x49ff9d, 0x49fff0, 0x4a00e9 | **not mech controls**: 0x49ff80 writes 0/1 flags into a different object (`[edi+0xe4]`) at +0x374/+0x378/+0x37c |

0x469940 kinds: 0, 1, 3..12 wrap the current controls by passing a 12-byte member-function
pointer (0x4ea000, 0x4ea040, ... 0x4ea230, AI code) to 0x469400 or 0x469650. These are
likely lancemate AI orders. Kind 2 toggles PocketControls: on, it saves the player singleton
(+0x15c), takes over `[0x6056ec]`, and sets camera mode 5 (0x40ae70). Off, it copies the
state fields into the wrapped controls (+0x158), puts the singleton back, and deletes itself.

## AI controls

AIControls 0x4d8750(owner, a2, a3, isMostDerived). It calls 0x4d8670 and puts the
virtual base at +0x30c. Its Update (0x4e96f0) adjusts `this` to the sub-object at +0x5c
(vtable 0x5cd2c8, code in 0x4e9xxx/0x4eaxxx, near zrdrvalve.cpp/zclass) and runs
0x4e9710, which is likely the AI brain. PlayerControls also has a +0x5c sub-object
(0x4d9840, vtable 0x5bd708), likely a shared AI/pilot base. I did not trace how the AI
brain sets throttle and turn, but it can only do so through the same IControls setters.

## Open

- IControls +0x04 and +0x54 (AI overrides), +0x10 (byte +0x34), and the full
  ClearCommands body (0x428490 continues past 0x4284ef).
- MechControls +0x0c states 0..3 (0x426780) and the 0x6040d0 joystick block.
- Names of the classes: there is no RTTI. File guesses come from func2file neighbours only.

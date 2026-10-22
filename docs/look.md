# Looking around, torso mode, part targeting, op points

Mech3.exe 1.2, playtest #14..#16 (mac 2, 2026-10-21). The keys are
Mech::command's (0x0041be80, docs/controls.md); this is what they reach.

## The look keys (0x62..0x67) and FREELOOK (L)

Each look key calls the view's mode, vt+0x10 ([0x005f4c8c] +0x58), with a
look: left 0, right 1, ahead 2, back 3, up 4, down 5. FREELOOK out of state 1
calls it with 2. On the mech cameras (0x00421c00, view.md section 2):

- the torso's pitch lock is released (0x00464ac0(0));
- left and right turn the look by the camera's look limits and set the
  pitch lock again; ahead clears the look and centres the torso
  (0x00465a00(1)); up and down pitch it; **back (3) changes nothing** on
  the mech cameras, the look stays where it was;
- the HUD is told (the screen's look), and the look is kept at +0x40.

The area camera (0x004220c0) turns in quarter turns, and there back turns
it round.

**The port's mistake (playtest #14).** The program had wired these keys to
the camera switch 0x0040ae70, not to the mode's vt+0x10, so a look key
switched cameras by number. They now go to `program_view_look`.

## The torso's calls

| Exe | Port | What it does |
|---|---|---|
| 0x00464ac0 | `Torso::set_pitch_lock` | +0xa1 = on and [0x005e4f28] (a constant 1) |
| 0x00464c90 | `Torso::hold_view` | +0xa2; on, the view (+0xa4) taken from the turn (+0x118), the look's angles zeroed |
| 0x00465ea0 | `Torso::set_aim` | the AI's aim (+0x9c, +0x8c, +0x88) and its clamped direction (0x00465d10); none: aim off |
| 0x00465f20 | `Torso::aim_ahead` | the aim (0, 0, -1) ([0x005e4f68]) |
| 0x00465a00 | `Torso::centre` | CENTER_TORSO (Shift+/) 1, CENTER_LEGS (/) 2 |
| 0x00466190 | `Torso::set_mode` | +0x84 (2 when the mech's vt+0x5c says so: never), the servo sound stopped |

**Centring** (0x00465a00) does nothing while the mech is powered down or
without a node. Otherwise +0x1b0 takes the kind. With the player's
controls (+0x84 = 1), the torso's update does the centring
(movement-ground.md). With the AI aiming (+0x84 = 0), it is done at once:
the aim is put straight ahead, or, for the player's mech with the
controls' flag 1, the mouse is put back in the middle (0x0051b440(0, 0)).

## The controls' view state (0x00426780)

`MechControls::set_view`. The controls' +0x0c:
- 0: the torso follows;
- 1: free look in the cockpit;
- 2 / 3: torso mode.

Ctrl+keypad 0 (TOGGLE_TMODE) goes from 0 or 1 to 2, and from 2 or 3 to 0;
FREELOOK goes from 0 to 1 and from 1 back to 0. The cockpit camera's
activation sets the state from the MouseMode option.

- Leaving 0 or 1 puts the mouse back in the middle. Leaving 1 also sets the
  cockpit camera's look rate (+0x3c, the mech's camera 1) back to 6.
- Entering any state sets the torso's mode to 1 (0x00466190). When the view
  is on this mech, its camera's +0x54 is set (look along the torso). The
  mode's vt+0x1c gives the mech camera itself, or null for the base.
- 1 sets the look rate to 8. Outside the cockpit camera it becomes 2
  instead.
- 2 and 3, with the controls' flag 4, become 0.
- Last, the torso holds its view while in 1 (0x00464c90).

**Seen in play (playtest #14).** The cockpit camera's activation runs this
with the MouseMode option (2, torso mode, by default). So the player's torso
is in mode 1, driven by the controls. Before, the port never ran it: the
torso stayed in mode 0, aimed as the AI's, and a keyboard pitch sprang back
when let go. Now it holds where it is left, as in the exe. It drifts on a
little as the controls ease off, and comes back level only with the pitch
lock (the free look's left and right). The HUD's lock and the reticle
tests now steer the pitch both ways.

## Targeting a part (keypad 1..9; 0x0041d810, 0x0041d6f0)

The keys pick the head (8), the centre torso (5), the left and right torso
(4, 6), the arms (7, 9) and the legs' calves (1, 3). `Mech::target_part`:

- **Only with a targeting computer** (equipment kind 14, `tc`; +0x3e4 > 0)
  and a target. The target's mech (0x004e6ff0) must still have the part
  (0x0041d510), and it must not be destroyed (+0x48). Otherwise the choice
  is none.
- The same part chosen again drops the choice (+0xf8 = -1).
- Then the point (0x0041d6f0, `Mech::aim_part`, also every tick from the
  player tick's tail 0x0041aba0): the part's node's world position, kept at
  +0xec. For the side torsos it is a point 0.75 of a half width out along
  the target's torso node, left or right. The half width is **the player's
  own torso's** (+0x39c on ESI, the mech choosing), not the target's.
  When the target has no such part, the choice is dropped and the point
  becomes the target's position (0x005a8630). The HUD draws the mark there
  (hud.md, the lead point).

Without a targeting computer the keypad does nothing, as in the exe.

## Op points (N, Shift+N, Ctrl+N)

NAVPT_NEXT / PREV / NEAR call [0x00609980] vt+0x60 / +0x64 / +0x68 (the
last with the mech's position). [0x00609980] is the mission object's +0xb0
(0x006098d0 + 0xb0): multiplayer's stations (mission-frame.md). Only the
stations' own code writes it (0x00491d00, from 0x0047a6c8, 0x0047a843,
0x0047a89d). In the campaign and Instant Action it is null, and the keys do
nothing. The port checks it the same way (`CommandCalls::stations`) and
logs the call only when a stations object exists. Multiplayer is not
ported.

## The port and the checks

- `mech3/mech_parts.cpp` (the torso), `mech_control.cpp`
  (`MechControls::set_view`), `mech_command.cpp` (`aim_part`,
  `target_part`; part_by_id 0x0041d510 was already in mech_parts).
- `app_view.cpp` answers the free look's calls (the torso's, 0x0041ca30,
  0x0041ca60 on +0x634 bit 0, 0x00426780, and 0x00502da0: [0x0073f75c],
  the animations' "in the cockpit", anim block +0x3c). It also gives the
  controls the view's target, its mode and the mech's cameras. `app_hud`
  reads the chosen part and its point.
- `tests/differential/test_parts.py`: new ops run the original against
  the port on the same mech, bit for bit at 24 bits:
  - the pitch lock, hold view, the aim (with its clamped direction), centre
    (both kinds, powered down, the AI's aim, the mouse), the torso's mode;
  - the controls' view state, all four, with the cockpit camera's look
    rate and the view's camera;
  - the part choice and the part's point (with and without a target, a
    seen vehicle, a mech, a TC; every part id).
- `tests/differential/test_command.py`: the stations null one time in four
  (N and the rest then do nothing).
- `mission_c1m1_default_keys_fire_headless`: each look key leaves the
  view's look at its number, and Ctrl+keypad 0 toggles torso mode; no key
  of #14..#16 reaches unported code. The look keys' 0x0040b310 comes from
  the outside camera X left on, every frame.

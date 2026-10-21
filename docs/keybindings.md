# Key bindings: the audit (playtest #12)

Mech3.exe 1.2; audited 2026-10-21 (mac 2) for Owen. Every command, its
default key (0x00411150, docs/controls.md "The defaults"), who handles it in
the exe, and what the program does with it today. The table comes from
`test_program`'s `mission_c1m1_default_keys_fire_headless`. It presses each
default key in c1 mission 1, modifiers first, and records three things: the
command's callback running, zInput showing the command held, and anything
unported reached during the press (the calls unported on every frame are left
out). Run it with `MW3_KEYS_OUT=<file>` for the raw table. Ctrl+Q and Alt+F4
end the run, so they are only checked as bound.

## How keys reach the game

- **Held keys** (movement, turning, twist, pitch, jump jets, fire) have no
  callback. The player's controls read them each frame
  (`PlayerControls::update`, 0x427d60).
- **The mech's commands**: 0x426ce0 binds 95 commands to 0x426cb0. That
  calls the controls' own command method first (0x4274d0: the throttle keys
  Backspace and 1..0), then the viewed mech's `Mech::command` (0x41be80).
- **The app's keys**: 0x40db60, called by the mission's load, binds X
  (CYCLE_CAM), Shift+V (VMODE_TOGGLE) and ` (MESSAGE_TOGGLE) to 0x40dba0,
  and 0x7e (no key) to 0x4abbc0. 0x40dba0's other cases (0x6d, 0x82..0xa4)
  are debug commands no key or file can bind.
- **The missions'**: 0x48f400 binds Exit Mission (Ctrl+Q, 0x71) and 0x72 to
  0x48f440. It only acts once the mission is won or lost.
- **Quick Exit**: Alt+F4, command 1, no group and no CMD_ name.

No controls file ships with the game (`keys\` is made by the controls
screen), so a fresh install plays the defaults.

## [ ] Enter and keypad Enter, settled

Owen remembers ] as next weapon, [ as previous, Enter as next weapon group
and keypad Enter as fire everything. The exe:

- **] and [** are WEPGRP_NEXT / PREV (0x2a / 0x2b). They move to the next
  or previous **group** that has a weapon able to fire (0x4b04f0, 0x4b0610;
  going back also skips a group whose only weapon has unlimited ammo and no
  beam, the Flamer).
- **Ctrl+] and Ctrl+[** are WEP_NEXT / PREV (0x25 / 0x26): the next or
  previous **weapon within** the current group (0x4b0790, 0x4b07e0). The
  1.2 readme says the same. The 1.0 readme swapped them, and the 1.2 exe's
  defaults agree with the 1.2 readme.
- **Enter** is NEXT_WEAPON (0x27). It selects the weapon after the current
  one **in the HUD's weapon list**, across all groups (the list's +0x1143c,
  which 0x446240 fills), along with its group (0x4b0730, 0x4b1de0).
- **Keypad Enter** is ALPHASTRIKE (0x39): every weapon at once (0x4b0a70),
  unless powered down. This matches Owen.

So the decomp disagrees with Owen's memory on ] / [ and Enter, but explains
it. With the usual grouping of one weapon kind per group, ] moving group by
group looks like "next weapon". Enter, walking the HUD's list, lands in
another group as soon as the current group's weapons run out, which looks
like "next group". The program keeps the exe's bindings and behaviour.

## Fixed in this audit

- **Enter (#6)**: the mech's command handler, the weapons and the damage
  code had no HUD to ask. The program never handed out the pilot's HUD
  (0x430aa0), the weapons list's layout and refresh (0x446240, 0x446520,
  0x4464c0), or its highlighted weapon. `wire_hud` now bridges them (the HUD
  is mac 3's port).
- **X (#5)**: the app's keys were never bound, because the mission's
  0x40db60 was an unported hook. Ported as `register_app_keys` / `app_key`
  (`mech3/app_keys`). The camera key 0x40aed0 then needed the view's camera
  option (0x40ce10) wired. The mission starts in the cockpit (CameraMode 1,
  camera id 1), and X steps cockpit → zipper (2) → tether (0) → cockpit,
  with zipper refused in some game types (byte 0x8e). The "couple of odd
  polygons" in the cockpit view are the cockpit model, #4.
- **1..0 and Backspace**: 0x4274d0 was an unported hook, so the throttle
  keys did nothing. Ported as `PlayerControls::command` and checked by
  `test_throttle_keys.py` (4,000 commands identical). 2 sets 20% though it
  is labelled 10%.
- **Ctrl+Q**: the missions' bind (0x51d2f0 with 0x48f440) was an unported
  hook. It is wired now.

## The table

| Command | Default | Handler (exe) | Fires | Still unported when pressed (owner) | Expected (readme, Owen) |
|---|---|---|---|---|---|
| **Movement** | | | | | |
| 0x18 Forward throttle | UP | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x19 Reverse throttle | DOWN | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x02 Turn Left | LEFT | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x03 Turn Right | RIGHT | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x08 Twist Left | , | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x09 Twist Right | PERIOD | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x04 Pitch Up | - | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x05 Pitch Down | = | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x06 Center torso to legs | Shift/ | Mech::command 0x41be80 | yes | torso centring 0x465a00 (mac 2) |  |
| 0x07 Center Legs | / | Mech::command 0x41be80 | yes | torso centring 0x465a00 (mac 2) |  |
| 0x1a Toggle throttle direction | BACK | 0x4274d0 (the controls), then 0x41be80 | yes | - |  |
| 0x1b 0% throttle | 1 | 0x4274d0 (the controls), then 0x41be80 | yes | - |  |
| 0x1c 10% throttle | 2 | 0x4274d0 (the controls), then 0x41be80 | yes | - | labelled 10%, sets 20% |
| 0x1d 30% throttle | 3 | 0x4274d0 (the controls), then 0x41be80 | yes | - |  |
| 0x1e 40% throttle | 4 | 0x4274d0 (the controls), then 0x41be80 | yes | - |  |
| 0x1f 50% throttle | 5 | 0x4274d0 (the controls), then 0x41be80 | yes | - |  |
| 0x20 60% throttle | 6 | 0x4274d0 (the controls), then 0x41be80 | yes | - |  |
| 0x21 70% throttle | 7 | 0x4274d0 (the controls), then 0x41be80 | yes | - |  |
| 0x22 80% throttle | 8 | 0x4274d0 (the controls), then 0x41be80 | yes | - |  |
| 0x23 90% throttle | 9 | 0x4274d0 (the controls), then 0x41be80 | yes | - |  |
| 0x24 100% throttle | 0 | 0x4274d0 (the controls), then 0x41be80 | yes | - |  |
| 0x0c Jump Jet | J | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x0d Jump Jet Forward | HOME | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x0e Jump Jet Reverse | END | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x0f Jump Jet Left | DELETE | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x10 Jump Jet Right | PG DN | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x11 Rotate Left | INSERT | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x12 Rotate Right | PG UP | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x44 Get up | G | Mech::command 0x41be80 | yes | - |  |
| 0x13 Crouch | C | Mech::command 0x41be80 | yes | - |  |
| 0x3f Startup/Shutdown | S | Mech::command 0x41be80 | yes | - |  |
| 0x6e Toggle MASC | V | Mech::command 0x41be80 | yes | - |  |
| **Weapon** | | | | | |
| 0x3d Fire weapon | SPACE | held: PlayerControls::update 0x427d60 | yes | - |  |
| 0x2a Next weapon group | ] | Mech::command 0x41be80 | yes | - | readme: next weapon group; Owen: next weapon |
| 0x2b Previous weapon group | [ | Mech::command 0x41be80 | yes | - | readme: previous weapon group; Owen: previous weapon |
| 0x26 Previous Weapon in Group | Ctrl-[ | Mech::command 0x41be80 | yes | - | readme 1.2: previous weapon in a group (1.0: next) |
| 0x25 Next Weapon in Group | Ctrl-] | Mech::command 0x41be80 | yes | - | readme 1.2: next weapon in a group (1.0: previous) |
| 0x27 Next weapon | ENTER | Mech::command 0x41be80 | yes | - | Owen: jumps to the next weapon group |
| 0x2c Weapon Group 1 | F1 | Mech::command 0x41be80 | yes | - |  |
| 0x2d Weapon Group 2 | F2 | Mech::command 0x41be80 | yes | - |  |
| 0x2e Weapon Group 3 | F3 | Mech::command 0x41be80 | yes | - |  |
| 0x2f Weapon Group 4 | F4 | Mech::command 0x41be80 | yes | - |  |
| 0x30 Weapon Group 5 | F5 | Mech::command 0x41be80 | yes | - |  |
| 0x31 Assign weapon to group 1 | ShiftF1 | Mech::command 0x41be80 | yes | - |  |
| 0x32 Assign weapon to group 2 | ShiftF2 | Mech::command 0x41be80 | yes | - |  |
| 0x33 Assign weapon to group 3 | ShiftF3 | Mech::command 0x41be80 | yes | - |  |
| 0x34 Assign weapon to group 4 | ShiftF4 | Mech::command 0x41be80 | yes | - |  |
| 0x35 Assign weapon to group 5 | ShiftF5 | Mech::command 0x41be80 | yes | - |  |
| 0x28 Toggle fire mode | \ | Mech::command 0x41be80 | yes | cockpit sound 0x41e770 (mac 2) |  |
| 0x39 Alpha Strike | NUMPAD ENTER | Mech::command 0x41be80 | yes | - | Owen: fires every weapon at once |
| 0x41 Flush Coolant | F | Mech::command 0x41be80 | yes | - |  |
| 0x40 Override Thermal Shutdown | O | Mech::command 0x41be80 | yes | - |  |
| 0x3b Jettison ammo | ShiftK | Mech::command 0x41be80 | yes | - |  |
| **View** | | | | | |
| 0x61 Cycle Camera Mode | X | the app, 0x40dba0 | yes | camera 0x40b310 (mac 2); camera 0x502da0 (mac 2); the controls' view state 0x426780 (mac 2) | Owen: starts inside, X to third person |
| 0x62 Look Left | AltNUMPAD 4 | Mech::command 0x41be80 | yes | camera 0x40b310 (mac 2); camera 0x502da0 (mac 2); the controls' view state 0x426780 (mac 2) |  |
| 0x63 Look Right | AltNUMPAD 6 | Mech::command 0x41be80 | yes | camera 0x502da0 (mac 2); look 0x41ca60 (mac 2); the controls' view state 0x426780 (mac 2) |  |
| 0x64 Look Up | AltNUMPAD 2 | Mech::command 0x41be80 | yes | - |  |
| 0x65 Look down | AltNUMPAD 8 | Mech::command 0x41be80 | yes | camera 0x40b310 (mac 2); look 0x41ca30 (mac 2) |  |
| 0x67 Look back | AltDECIMAL | Mech::command 0x41be80 | yes | the HUD screen asked from the torso 0x430aa0 (mac 2); torso 0x464ac0 (mac 2); torso centring 0x465a00 (mac 2) |  |
| 0x66 Look forward | AltNUMPAD 5 | Mech::command 0x41be80 | yes | camera 0x40b310 (mac 2); camera 0x502da0 (mac 2); the controls' view state 0x426780 (mac 2) |  |
| 0x6c Cycle Resolution | ShiftV | the app, 0x40dba0 | yes | screen mode cycle 0x4ab910 (mac 2, resolution step 3) |  |
| **Targeting** | | | | | |
| 0x46 Target next enemy | E | Mech::command 0x41be80 | yes | - |  |
| 0x4d Target previous enemy | ShiftE | Mech::command 0x41be80 | yes | - |  |
| 0x4c Target nearest enemy | Ctrl-E | Mech::command 0x41be80 | yes | - |  |
| 0x47 Target next ally | W | Mech::command 0x41be80 | yes | - |  |
| 0x4e Target previous ally | ShiftW | Mech::command 0x41be80 | yes | - |  |
| 0x4b Target nearest ally | Ctrl-W | Mech::command 0x41be80 | yes | - |  |
| 0x48 Target object under reticule | Q | Mech::command 0x41be80 | yes | HUD's (?); command (?); the (?) |  |
| 0x49 Clear Target Selection | T | Mech::command 0x41be80 | yes | HUD's (?); command (?); the (?) |  |
| 0x59 Target Head | NUMPAD 8 | Mech::command 0x41be80 | yes | part targeting 0x41d810 (mac 2) |  |
| 0x5d Target Left Arm | NUMPAD 7 | Mech::command 0x41be80 | yes | part targeting 0x41d810 (mac 2) |  |
| 0x5e Target Right Arm | NUMPAD 9 | Mech::command 0x41be80 | yes | part targeting 0x41d810 (mac 2) |  |
| 0x5a Target Center Torso | NUMPAD 5 | Mech::command 0x41be80 | yes | part targeting 0x41d810 (mac 2) |  |
| 0x5b Target Left Torso | NUMPAD 4 | Mech::command 0x41be80 | yes | part targeting 0x41d810 (mac 2) |  |
| 0x5c Target Right Torso | NUMPAD 6 | Mech::command 0x41be80 | yes | part targeting 0x41d810 (mac 2) |  |
| 0x5f Target Left Leg | NUMPAD 1 | Mech::command 0x41be80 | yes | part targeting 0x41d810 (mac 2) |  |
| 0x60 Target Right Leg | NUMPAD 3 | Mech::command 0x41be80 | yes | part targeting 0x41d810 (mac 2) |  |
| **Cockpit** | | | | | |
| 0x14 Zoom Reticule | Z | Mech::command 0x41be80 | yes | HUD's (?); command (?); the (?) |  |
| 0x15 Lock Zoom reticule | NUMPAD 0 | Mech::command 0x41be80 | yes | HUD's (?); command (?); the (?) | readme: zoom reticle window |
| 0x16 Toggle Torso Mode | Ctrl-NUMPAD 0 | Mech::command 0x41be80 | yes | the controls' view state 0x426780 (mac 2) | readme: torso mode |
| 0x17 Freelook | L | Mech::command 0x41be80 | yes | - | readme: freelook |
| 0x74 Toggle Damage display | D | Mech::command 0x41be80 | yes | HUD's (?); command (?); the (?) |  |
| 0x75 Toggle Enemy Damage Display | Ctrl-D | Mech::command 0x41be80 | yes | HUD's (?); command (?); the (?) | readme: enemy damage display |
| 0x68 Toggle Radar size | R | Mech::command 0x41be80 | yes | HUD's (?); command (?); the (?) |  |
| 0x69 Toggle Radar mode | AltR | Mech::command 0x41be80 | yes | +0xb8 (?); vtable (?) |  |
| 0xa5 Toggle Radar map | ShiftR | Mech::command 0x41be80 | yes | HUD's (?); command (?); the (?) |  |
| 0x6b Toggle HUD on/off | H | Mech::command 0x41be80 | yes | - |  |
| 0x3a Toggle AMS on/off | A | Mech::command 0x41be80 | yes | - |  |
| **Communications** | | | | | |
| 0x54 Attack my target | F6 | Mech::command 0x41be80 | yes | lance orders 0x469930 (AI area) |  |
| 0x55 Defend my target | F7 | Mech::command 0x41be80 | yes | lance orders 0x469930 (AI area) |  |
| 0x52 Follow Me | F8 | Mech::command 0x41be80 | yes | lance orders 0x469930 (AI area) |  |
| 0x51 Hold position | F9 | Mech::command 0x41be80 | yes | lance orders 0x469930 (AI area) |  |
| 0x56 Repair in MFB | F10 | Mech::command 0x41be80 | yes | lance orders 0x469930 (AI area) |  |
| 0x4f Lancemate command screen | F11 | Mech::command 0x41be80 | yes | lance orders 0x469930 (AI area) |  |
| 0x57 Lock Awaiting orders | ShiftF11 | Mech::command 0x41be80 | yes | - |  |
| 0x58 Cancel Awaiting orders | Ctrl-F11 | Mech::command 0x41be80 | yes | - |  |
| 0x73 Order MFB | M | Mech::command 0x41be80 | yes | MFB order 0x47a910 (AI area) |  |
| 0x36 Next Op point | N | Mech::command 0x41be80 | yes | +0x60..+0x68 (?); [0x00609980] (?); vtable (?) |  |
| 0x37 Previous Op point | ShiftN | Mech::command 0x41be80 | yes | +0x60..+0x68 (?); [0x00609980] (?); vtable (?) | readme: previous op point |
| 0x38 Nearest Op point | Ctrl-N | Mech::command 0x41be80 | yes | +0x60..+0x68 (?); [0x00609980] (?); vtable (?) | readme: nearest op point |
| 0x76 Send message to team | NUMPAD - | Mech::command 0x41be80 | yes | chat 0x41e4e0 (network) |  |
| 0x77 Send message to all | NUMPAD * | Mech::command 0x41be80 | yes | chat 0x41e4e0 (network) | readme: the chat window |
| **Mission** | | | | | |
| 0x43 Mission Objectives | F12 | Mech::command 0x41be80 | yes | HUD's (?); command (?); the (?) |  |
| 0x71 Exit Mission | Ctrl-Q | the missions, 0x48f440 | bound, not pressed | - |  |
| 0x70 Message Window | ` | the app, 0x40dba0 | yes | - |  |
| **(no group)** | | | | | |
| 0x01 Quick Exit | AltF4 | zInput (Alt+F4) | bound, not pressed | - |  |

## Still not doing their job (the rows in playtest.md)

The "still unported" column is what each press reached and could not run:

- HUD commands, mac 3: Z, keypad 0, F12, Q, T, R, Shift+R, D, Ctrl+D
  (`CommandCalls::hud`, 0x43da40 / 0x43d240 / 0x43d250 / 0x43dc10 and
  0x444330), and Alt+R's radar light (0x41ded0).
- Views, mac 2: the look keys and torso mode (0x426780, 0x40b310,
  0x502da0, 0x41ca30, 0x41ca60), centring the torso or legs (0x465a00),
  look back (0x464ac0).
- Targeting a part, mac 2: keypad 1..9 (0x41d810).
- The op points, mac 2: N, Shift+N, Ctrl+N ([0x609980] vt+0x60..0x68).
- The fire mode's sound, mac 2: \ switches the mode, but its cockpit sound
  (0x41e770) is not ported.
- Lance orders and the MFB, the AI area: F6..F11 (0x469930), M (0x47a910).
- Chat, the network: keypad - and * (0x41e4e0).
- Shift+V, resolution step 3 (0x4ab910).

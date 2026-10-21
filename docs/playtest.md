# Playtest triage

Owen's runs of the real program (`build/mw3 -nofmv`), what he saw, and who
takes it. Items close with the commit that fixes them.

## 2026-10-21, first campaign run (New Campaign → pilot → briefing → c1m1)

Fixed the same morning: the menus' images (28430f1), lost clicks (65285b6),
the briefing's load freeze (5a55624).

| # | Seen | Area | Owner | State |
|---|---|---|---|---|
| 1 | No ripple on the main menu buttons on hover | 2D render, 0x00558dd0 | mac 2 | fixed (74b15e3), Owen to check |
| 2 | "Ok to delete pilot?" is a native OS alert, not the game's box | message box 0x004ac150 | mac 3 | open |
| 3 | Escape in a mission turns the screen purple instead of the pause menu | pause menu, the frame clear | mac 3 | open |
| 4 | The mech isn't visible: a couple of odd polygons where the cockpit view should be | view modes, the cockpit model (MechData +0x228, 0x00430ad0) | mac 2 | open |
| 5 | X doesn't switch views (should start inside, X to third person) | view modes | mac 2 | fixed KEYS: the app's keys (0x40db60) bound and the camera option wired; starts in the cockpit, X: cockpit → zipper → tether (keybindings.md) |
| 6 | Enter doesn't cycle the weapon groups | controls → weapons | mac 2 | fixed KEYS: the HUD bridged to the mech and weapons; Enter is the HUD list's next weapon, ] / [ the groups (keybindings.md) |
| 7 | Numpad 0 doesn't show the zoom reticle | HUD zoom window | mac 3 | open |
| 8 | Weapons stay green on the HUD: no recharge shown | HUD weapon lines | mac 3 | open |
| 9 | "Shutting down" plays but the mech never shuts down; then the speed locks, no slowing, firing or manual shutdown | heat shutdown, the action queue | mac 2 | fixed 5e05304: the power-down's Stop (0x426c10) was never called, so the throttle stayed; heat.md |
| 10 | HUD images not found in the mission: dmg_*_blk, hud_othererrow_* | the image library after the mission load | mac 3 | open |
| 11 | Trucks, APCs and mobile HQs fail to build | vehicles (AI controls) | AI area | waiting |
| 12 | Every key binding checked: does each command do what it should? | controls, all 0xa7 commands | mac 2 | audited KEYS (keybindings.md); every default key fires; 1..0, Backspace and Ctrl+Q fixed; the rest below |
| 13 | HUD keys reach no HUD command: Z, keypad 0, F12, Q, T, R, Shift+R, D, Ctrl+D; Alt+R's radar light | the HUD's commands (0x43da40 ..), 0x41ded0 | mac 3 | open |
| 14 | Look keys, torso mode (Ctrl+keypad 0) and centring (/, Shift+/) do nothing | views 0x426780, 0x40b310, 0x502da0, 0x41ca30/60; torso 0x465a00, 0x464ac0 | mac 2 | open |
| 15 | Keypad 1..9 don't target a part | 0x41d810 | mac 2 | open |
| 16 | N, Shift+N, Ctrl+N don't change the op point | [0x609980] vt+0x60..0x68 | mac 2 | open |
| 17 | \ switches the fire mode silently | cockpit sound 0x41e770 | mac 2 | open |
| 18 | Lance orders (F6..F11) and Order MFB (M) | 0x469930, 0x47a910 | AI area | waiting |

### #12, the key binding audit (Owen, 2026-10-21)

One table in `docs/keybindings.md`: every command (0xa7 of them), its
default key and mouse/joystick binding (0x00411150, `controls.md` "The
defaults"), its handler (the mech's commands 0x0041be80, the HUD's, the
app's), whether the program reaches it, and what it should do (the readmes,
`game/mw3-1.2/readme.txt` and `README-1.0.TXT`; the manual and the Quick
Reference Card if they can be found online, e.g. archive.org). Each command
not reached, or doing something else, becomes a row above. A headless test
presses each default key and checks its command fires.

From Owen's play of the original (expected behaviour): ] selects the next
weapon, [ the previous; Enter jumps to the next weapon group; keypad Enter
fires every weapon at once. He never used Ctrl-[ / Ctrl-] (guessed: whole
groups); the audit settles what they do from 0x00411150 and the handlers.

Already known from the readmes: Zoom Reticle Window is keypad 0 (missing
from the reference card); Ctrl-[ / Ctrl-] select the previous / next weapon
in a group (the 1.0 and 1.2 readmes swap them); Shift-N / Ctrl-N the
previous / nearest op point; Ctrl-P screenshot removed; keypad Ctrl-0 torso
mode; L freelook; Ctrl-D enemy damage display; F9 Stop replaces Ctrl-F10 /
Ctrl-F11; keypad * the chat window; Left/Right Ctrl can't be bound.

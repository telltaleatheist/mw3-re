# Playtest triage

Owen's runs of the real program (`build/mw3 -nofmv`), what he saw, and who
takes it. Items close with the commit that fixes them.

## 2026-10-21, first campaign run (New Campaign → pilot → briefing → c1m1)

Fixed the same morning: the menus' images (28430f1), lost clicks (65285b6),
the briefing's load freeze (5a55624).

| # | Seen | Area | Owner | State |
|---|---|---|---|---|
| 1 | No ripple on the main menu buttons on hover | 2D render, 0x00558dd0 | mac 2 | fixed (74b15e3), Owen to check |
| 2 | "Ok to delete pilot?" is a native OS alert, not the game's box | message box 0x004ac150 | mac 3 | fixed (d1f7f8b), Owen to check |
| 3 | Escape in a mission turns the screen purple instead of the pause menu | pause menu, the frame clear | mac 3 | fixed (36602c6), Owen to check |
| 4 | The mech isn't visible: a couple of odd polygons where the cockpit view should be | view modes, the cockpit model (MechData +0x228, 0x00430ad0) | mac 2 | fixed 144681c: the program could not tell the cockpit view (handle reads it never answered), so it drew the player's own mech around the camera and no cockpit; now the mech is hidden for the world, the cockpit model (0x430ad0) and arms drawn in passes of their own (0x4f73f0) |
| 5 | X doesn't switch views (should start inside, X to third person) | view modes | mac 2 | fixed 258364d: the app's keys (0x40db60) bound and the camera option wired; starts in the cockpit, X: cockpit → zipper → tether (keybindings.md) |
| 6 | Enter doesn't cycle the weapon groups | controls → weapons | mac 2 | fixed 258364d: the HUD bridged to the mech and weapons; Enter is the HUD list's next weapon, ] / [ the groups (keybindings.md) |
| 7 | Numpad 0 doesn't show the zoom reticle | HUD zoom window | mac 3 | fixed with #20 (hud.md section 19); exploit watch: none found (community-issues.md), Owen to check |
| 8 | Weapons stay green on the HUD: no recharge shown | HUD weapon lines | mac 3 | fixed by 258364d (after this run): the list's per-frame refresh waits for a shot's mark (WeaponCalls::hud_fire), unwired before; checked in both 2D paths through the SDL host (hud.md section 13). Also found: the line's destructor 0x00445970 was missing (a crash freeing a HUD under the SDL host). Owen to check |
| 9 | "Shutting down" plays but the mech never shuts down; then the speed locks, no slowing, firing or manual shutdown | heat shutdown, the action queue | mac 2 | fixed 5e05304: the power-down's Stop (0x426c10) was never called, so the throttle stayed; heat.md |
| 10 | HUD images not found in the mission: dmg_*_blk, hud_othererrow_* | the image library after the mission load | mac 3 | as the original (9ff2c4c): the 12 images were never shipped (hud.md section 16); reported once per name; fill-in art is a candidate (fixes.md) |
| 11 | Trucks, APCs and mobile HQs fail to build | vehicles (AI controls) | AI area | waiting |
| 12 | Every key binding checked: does each command do what it should? | controls, all 0xa7 commands | mac 2 | audited 258364d (keybindings.md); every default key fires; 1..0, Backspace and Ctrl+Q fixed; the rest below |
| 13 | HUD keys reach no HUD command: Z, keypad 0, F12, Q, T, R, Shift+R, D, Ctrl+D; Alt+R's radar light | the HUD's commands (0x43da40 ..), 0x41ded0 | mac 3 | fixed (hud.md section 19), Owen to check. Left: F12's box text (0x446ad0), Shift+R's mission maps (0x458590) |
| 14 | Look keys, torso mode (Ctrl+keypad 0) and centring (/, Shift+/) do nothing | views 0x426780, 0x40b310, 0x502da0, 0x41ca30/60; torso 0x465a00, 0x464ac0 | mac 2 | fixed bc64d6f |
| 15 | Keypad 1..9 don't target a part | 0x41d810 | mac 2 | fixed bc64d6f |
| 16 | N, Shift+N, Ctrl+N don't change the op point | [0x609980] vt+0x60..0x68 | mac 2 | fixed bc64d6f |
| 17 | \ switches the fire mode silently | cockpit sound 0x41e770 | mac 2 | fixed bc64d6f |
| 18 | Lance orders (F6..F11) and Order MFB (M) | 0x469930, 0x47a910 | AI area | waiting |
| 19 | The cockpit shows bullet holes from the start; they should appear only when the cockpit is hit | cockpit model (0x00430ad0's damage textures / the cockpit explosion restart), cpit_ani* cycled textures "no GFX data" | mac 2 | fixed c6722e4 |
| 20 | Zoom (keypad 0) doesn't show, but its sound plays over and over | HUD zoom window, #13 | mac 3 | fixed: the controls' zoom hooks were never wired, so the window's flags read "shown" with the key up and the click played every frame; now one click per change (hud.md section 19), Owen to check |
| 21 | One message box path still unported: 0x0048dfe0 (dlgMessage MESSAGEBOX), seen late in a run (quitting via the pause menu?) | message box | mac 3 | fixed: the Core state's quit box, made at every mission start (shown only in multiplayer); ported with its prompt, close and answers (menus.md section 12), Owen to check |

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

## 2026-10-25, second campaign run (c1m1, main 0093aba)

Owen: "otherwise it seems to be working really well".

| # | Seen | Area | Owner | State |
|---|---|---|---|---|
| 22 | Ground textures blink in and out at random spots | render: terrain draw, depth (coplanar overlays?), the partition cull | dev | waiting |
| 23 | Firing is unreliable: some clicks fire nothing until clicked again | controls → weapons: the trigger's edge between frames | dev | as the original: every short click reaches the weapons (mouse or key, five pressed and released in one frame), but a beam fires only from a full charge (0x4b3fc0), so a click while the bar fills is dropped; buffering it is a candidate (fixes.md). Tests: `mission_c1m1_short_clicks_fire_headless` |
| 24 | A laser doesn't hit where the reticle was at the click | the aim point the shot takes (a frame late?) | dev | as the original: the beam fires at exactly the aim point the click saw, turning or not, and a held beam follows the reticle (0x4b41b0); an arm's beam runs from its muzzle to the point under the reticle, so it can hit something in front that the reticle is not on (a narrow torso the ray passes beside). Tests: `mission_c1m1_laser_{lands,turning}_on_the_reticle_headless` |
| 25 | LRMs lock but don't home: in the original, lock, aim at the sky, fire, and the missiles climb and come down on the target | the shot's lock target at spawn (homing 0x585900 is ported) | dev | waiting |
| 26 | F12 opens no objectives text | the objectives box's text 0x446ad0 (keybindings.md) | dev | waiting |
| 27 | Shots at the missile turrets don't destroy them | turret hits / damage (hit test reaching tur_* nodes?) | dev | waiting |
| 28 | No recoil: the Bushwhacker's arm AC/10 should kick the mech back and throw off the aim (less from the torso) | WeaponCalls::recoil (weapon.cpp 204) unwired in the program; the part update 0x466xxx is ported | dev | fixed: the program set no recoil hook; an arm's weapon kicks the arm (0x466610), a hand's its arm (0x4673f0). Test: `mission_c1m1_arm_weapon_recoils_headless` |

## Unported in c1m1 (2026-10-21, census)

`census_c1m1_unported_headless` (mw3tests; `MW3_CENSUS=1`,
`MW3_CENSUS_SECONDS`, `MW3_CENSUS_OUT`) plays c1m1 through the program in
the Thor for 180 s of game time (10,921 frames). It walks, turns, targets
and fires each group, jumps, zooms, looks every way, presses the HUD keys,
torso mode and the camera, then quits with Ctrl+Q. Every unported address
the engine logs is counted; `tools/analysis/census.py` gives each its
original file, its size (pseudocode lines of the entry function) and an AI
mark. 163 distinct; the 20 s run finds the same 163.

What the counts are:
- **Every frame (16 keys).** Mostly the Core frame's plumbing, which the
  program does its own way: the scene and camera draws (0x005674d0,
  0x00567570, 0x004f7350), the software target (0x00557f90), zTwoD's
  layer update (0x005a6790 / 0x005a67c0, misnamed as effects and particles
  in app_hooks.h), the HUD's frame and 3D entries (0x0040fac0, 0x0040e4b0,
  0x004b51f0, 0x00430aa0 +0x144c vt+0x28 the reticle's tick: mac 3's HUD
  path covers them). Multiplayer only: DirectPlay's poll 0x00588930, the
  MFB check 0x0045a270 (game type 3), the chat panel 0x0041ab90. AI: the
  sensors' entity refresh 0x005a85c0. The one real gap is the view's ground
  check 0x004edab0 (below).
- **Once (mission start and quit).** About 130 start-up and shutdown calls.
  Most are plumbing the program does elsewhere (video, render, input,
  reader paths, zEntity's init and shutdown, the movie player, ~20 AI).

Ranked by what a player would notice (AI-area ones left out: lancemates
0x0049e5e0, the valves, targets and nets, the sensors):

| # | What | Addresses (pseudocode lines) | What the player sees |
|---|---|---|---|
| 1 | The mission's objectives (mech_objective.cpp) | load 0x0045c2d0 (23), start 0x0045c4a0, check 0x0045e540, free 0x0045c1d0 (26); the region 0x0045c000..0x0045f800 is ~48 functions, ~3,100 lines | No objectives: c1m1 can be neither won nor lost, no "objective complete" lines; fixed by 5fcc143 (objectives.md pieces 1-4: c1m1 won and lost through its objectives, headless; training's types are piece 5) |
| 2 | The view's ground and water check | 0x004edab0 (148; ported as `zcls::ground_point`, only unwired in app_view), the water 0x004abd90, the water fog 0x0040b0e0 | Chase cameras sink into the ground; no water fog under water (the water hook answers "none" silently) |
| 3 | The skins on the mech's model | 0x00475780 (38), 0x004757f0 (29), 0x00475a80 (13) | Mechs keep the model's default textures; no damage decals on their hulls |
| 4 | The mechs' HUD markers from world.zrd | 0x00459e80 (76) | Map and radar markers from the world's block missing (to confirm with mac 3) |
| 5 | The HUD's text lines cleared | `hud_text_clear` vt+0x7c ×25, `hud_line_clear` ×25 | Old HUD message lines may stay up (mac 3's area; to confirm) |
| 6 | The effects' detail level | 0x00508fb0 (11) | Animations that test the level ("IF level") take the wrong branch: some effects missing or extra; fixed by 125338b (wired to zeff::anim_set_level) |
| 7 | A polygon the mech build adds | 0x004776d0 (19), 8 a mission | Something of each mech's model not made (to trace) |
| 8 | The mechs' LOD scale | 0x00430440 (20) | Model detail switches at the default distances, whatever the option; fixed by 125338b (0x00430440 ported, mech_data) |
| 9 | The shadows option | 0x004e6920 (14) | The shadows' on/off option is not applied; fixed by b058c6b (it is the mechs' sun shading, "shadow parts": mech_shade, docs/shade.md) |
| 10 | The stealth set-up | 0x004a4de0 (72) | Stealth objects not made (none in c1m1; other missions) |

Below that: the model priority 0x0053bb20 (21; draw order), the marker
refresh at build 0x005a8500 (14; `Mech::refresh_markers` is ported, only
the hook is unwired), the launch's pilot calls 0x00478f10 (81) /
0x004782c0 (16), the HUD's free 0x0043b860 (86) at the end, the maps'
list 0x004583b0 (72; Shift+R), the app's sound set 0x0040a800 /
0x0040baf0, the fog's apply [0x0072f3f0].

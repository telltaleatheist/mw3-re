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
| 22 | Ground textures blink in and out at random spots | not reproduced: still and walking (cockpit) and still and turning (outside) on c1m1, still on c1m2 and c2m1 show no ground blink (tools/analysis/blinks.py); needs Owen's mission, place, motion and what a blink looks like | dev | needs repro |
| 23 | Firing is unreliable: some clicks fire nothing until clicked again | controls → weapons: the trigger's edge between frames | dev | reopened (Owen, 2026-10-25): a fully charged weapon also lags; the button has to be held before it fires. Earlier finding: as the original: every short click reaches the weapons (mouse or key, five pressed and released in one frame), but a beam fires only from a full charge (0x4b3fc0), so a click while the bar fills is dropped; buffering it is a candidate (fixes.md). Tests: `mission_c1m1_short_clicks_fire_headless`. Rechecked 2026-10-30 (dev): no lag in the port: from a full charge a click fires on the next frame, headless (10 of 10) and in the program (`--click`, a visible window, 16 ms at 60 Hz); the test now asserts the one frame. Likely what Owen saw, as the original: the manager starts in chain fire (mode 0, weapons.md), where a press fires only the group's current weapon (the HUD's select mark) and steps on at the release; with another weapon of the group full and the current one recharging, a click does nothing and a held button fires when the current one fills. To ask Owen: the fire mode (the `\` key cycles chain, single, group), the group and weapons, fullscreen or windowed |
| 24 | A laser doesn't hit where the reticle was at the click | the aim point the shot takes (a frame late?) | dev | reopened (Owen, 2026-10-25): with the lag, the beam leaves from where the mech was at the click, not where it is when it fires. Earlier finding: as the original: the beam fires at exactly the aim point the click saw, turning or not, and a held beam follows the reticle (0x4b41b0); an arm's beam runs from its muzzle to the point under the reticle, so it can hit something in front that the reticle is not on (a narrow torso the ray passes beside). Tests: `mission_c1m1_laser_{lands,turning}_on_the_reticle_headless`. The trailing beam after a quick click is fixed behind `bugs` (fixes.md, A beam after its button is let go; CHANGES.md); with #23's chain-fire wait, a click on a charging current weapon fires only once it fills, from where the mech is then |
| 25 | LRMs lock but don't home: in the original, lock, aim at the sky, fire, and the missiles climb and come down on the target | the shot's lock target at spawn (homing 0x585900 is ported) | dev | fixed: the program set none of the lock's hooks, so every missile left without a target; now the target selected, its aim point and velocity (live pointers, 0x5a8630 / vt+0xc), its ECM, FCS and the lock warning are wired. The turn is the exe's (14 degrees a second at most), so they come round from a distance, not at point-blank range. Test: `mission_c1m1_lrm_homes_on_the_lock_headless` |
| 26 | F12 opens no objectives text | the objectives box's text 0x446ad0 (keybindings.md) | dev | waiting |
| 27 | Shots at the missile turrets don't destroy them | not reproduced: every c1m1 turret dies to hits on it (vehicles.md, The turrets in the program); tur_srm02's trench walls block shots from across it; needs Owen's turret and weapon | dev | needs repro |
| 28 | No recoil: the Bushwhacker's arm AC/10 should kick the mech back and throw off the aim (less from the torso) | WeaponCalls::recoil (weapon.cpp 204) unwired in the program; the part update 0x466xxx is ported | dev | fixed: the program set no recoil hook; an arm's weapon kicks the arm (0x466610), a hand's its arm (0x4673f0). Test: `mission_c1m1_arm_weapon_recoils_headless` |
| 29 | The HUD corners show neither your own mech nor the target. The MFD (D) should cycle: your mech in 3D from the front, a parts diagram with damage, an armour bar graph (green, red where damaged) | fixed: the MFD view's zView renders through 0x442900 (vtable 0x005c04f8): mode 1 the own mech from the front; TARGET's view draws its target (6c3); modes 2 and 3 were right (hud.md section 22) | dev | fixed, Owen to check |
| 30 | No mission boundaries (yellow and red lines) on the radar | fixed: the radar's reset 0x43d920 ported, the boundary's reg_inside_ (yellow) and reg_outside_ (red) regions on the radar (hud.md section 15) | dev | fixed, Owen to check |

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

## 2026-10-29, the first run on Windows (Owen's PC, fullscreen)

The Windows build (docs/windows.md) from a3a6eb3 on. Movies and menus fine.

| # | Seen | Area | Owner | State |
|---|---|---|---|---|
| W1 | Crash (access violation) during a mission | TARGET's mode-1 view of a turret: its node read as a mech's (app_hud object_node) | dev 2 | fixed cd45f5f |
| W2 | D's damage diagram outlines only one arm at full health; damaged parts show | the 2D layer's cover took opaque 0x001f outlines for the translucent background | dev 2 | fixed 0d38d9a. Note: the bands are the exe's, blue, then green above 0.7, yellow above 0.4, red |
| W3 | No music | the install had no CD tracks (data-only ISO) | lead | fixed: tracks 2 and 3 from Owen's disc dump as music\track02/03.wav in the write dir (PC and Mac) |
| W4 | The PC's install: the 1.2 exe over pre-1.2 data | reader.zbd, interp.zbd, c3 missions differ; patch files missing | lead, dev | the PC now uses the verified 1.2 set; any data version played and named at start (8a3f1f2): the old install is 1.0 data under the 1.2 exe |
| W5 | Finding the install for other players | ours | dev 2 | fixed 51d278b: mw3.cfg, the installer's registry key, the usual folders, then a Browse dialog. Owen to try Browse, a wrong folder, the right one and Cancel (mw3.exe --write-dir C:\temp\x starts with no settings) |
| W6 | The crosshair stays green over turrets and vehicles where it should turn yellow (it does turn red on turrets); right on mechs | the aim pick (0x444330) and the reticle's colour | dev | fixed 0021b4f: the over test reads each class's active flag (turrets, vehicles, targets), not only a mech's |
| W7 | TARGET's view of the Owens: the camera too low, its legs halfway up the window and its head off the top (maybe other mechs too) | TARGET's camera (0x440b00) aimed at a mech's place (its feet), not its centroid (+0x14): every mech, the target triangle too (hud.md section 22, TARGET's camera) | dev 2 | fixed (mac5/target-camera) |
| W8 | From the original (Owen's memory): standing on the op point while the mobile field bases come, they arrive but the game never lets go (the repair or the next step never starts) until you step off | the MFBs' arrival, the op point's script or AI valve, mfb_reach | dev | **closed with W16, the exe's own**: called to the point they docked at, the bases stand on their slots already, their go-to has no way to drive and no node to enter, so the valves set on arriving never are and the places step (0x0045ab70, no give-up) waits for good; fixed behind `bugs` (fixes.md) |
| W9 | Mech lab: switching skins blinks the whole screen black for about a frame | the lab's skin change (texture reload, the lab view's draw that frame) | dev 2 | fixed (mac5/lab-skin-blink): ours. The skin swap (0x0044fd90) locks and unlocks the 3D inside the screen's own lock; the inner unlock handed the port's layer over early, so the rest of that frame's 2D was lost. The exe holds nothing (DirectDraw kept drawing to the back buffer). FIX (A), render-frame.md section 1 |
| W11 | Lancemates seem not to follow orders: F8 (follow me) gets no "lead the way", and F8, attack my target and the rest change nothing; sometimes the MFBs say "roger that lance leader" to M, this time not (the second M) | the lance keys (0x469930) to the selected mate's brain, its orders, the radio (0x4e9fd0, 0x4e7e20); the MFB command (0x47a7a0) and the dock's lines | dev 2 | **c1m4 is the original's.** There the player's team (the mission's aiv "player" record) has no allies, so W never selects Dominic and no order reaches him; in later missions (lancemates from the lance's slots) W, E, then F6 / F8 / F9 all work, ally record kept through E (test, by the keys). **Fixed (port bugs):** (1) Dominic's radio in c1m4: the launch's stand-in mech in the pilot's +0x14 made the brains built during the load see a player without allies, so no radio (0x4e7e20); the exe leaves +0x14 empty then (0x491868). Later missions were not affected (their lancemates are built after the launch). (2) F6 / F7 on a target that is not a mech (a vehicle) were dropped: 0x469650 takes the target's vehicle (0x4e6fe0), the port took its mech. Open: on a targets.zrd object (a turret, a building) the exe gives the order and the brain refuses ("disregard"); the port drops it silently. **MFB: as the exe.** The second M's "contact" comes 5 frames later only if the dock's quiet time has passed (0x45b7d0: 5 s after its last line, or held while a contact is pending, 0x45ac20 / 0x45ace0) and the sound's own MINDELAY allows. Tests `mission_c2m1_lance_order_keys_end_to_end_headless`, `mission_c1m4_lance_orders_reach_dominic_and_he_answers_headless`, `test_lance_dispatch.py` (a non-mech target added) |
| W10 | c1m3 (probably): ordered to the op point, two MFBs parked there correctly, the third drove on toward the enemy and was destroyed | the MFBs' route and arrival on the AI net (net_01, mfb_reach; the chain's c1m3 notes: the 77->32 link), each base's stop at its slot | dev | fixed (a port bug; the exe parks all three): the way finder (0x004c7e80) had no position for the base, so the request's go-to (0x004c84b0) found no way and the bases followed net_01's links by chance; one could reach another's slot and go on (vehicles.md, The request) |
| W16 | c2m2 (probably): only one MFB drove to the point, the other two stayed; called again to the same point to repair a second time, they would not come | the dock's request and places step (0x0045a780, 0x0045ab70; vehicles.md, The request) | dev | **the exe's own, fixed behind `bugs`**: after a repair the close drives each base back onto its slot and the reset holds it there, in its slot's behaviour on the net; called to that point again, its go-to (0x004c84b0) has no way to drive and no node to enter, so its valve (set on arriving) never is, and the places step, which has no give-up, waits for good (a base already on its slot stays, the others drive: "only one moved"). Also W8. FIX (bug): such a base counts as arrived; a base that stands still 30 s short of its slot is given up on. The port's routing in c2m2 is right: all three reach each of the four points (MFB 3 and 4 by a long way round through MFB 2). Test: mission_c2m2_field_bases_answer_a_second_call_headless |
| W18 | "AMS doesn't seem to be working at all" | the mech's AMS frame (0x00419200; weapons.md, AMS) | dev | fixed (a port bug): the update (0x0041a822) calls it through UpdateCalls' equipment_update, which nothing set and which had no fallback (the hook audit's "unset runs the port" was wrong), so no mech's AMS ever fired; ported (`update_ams`) and run when the hook is unset. The radar list, the hostile flag and Ams::find / fire were right. Test: mission_c1m1_ams_shoots_down_incoming_missiles_headless |
| W20 | "When I use the jump jets, the colours and environment change a bit. The lighting changes for a few frames." (f787f1c) | the jets' flame animation `jumpjets` (canned effect 1, on the butt node) and its lights; the GPU layer's fog colour | dev | partly: a port bug fixed, Owen's case not reproduced yet. The original's jets: `viewcheck` plays the puffs (`puff_down`, `puff_line`) only outside the cockpit, or in it for a mech more than 13 m from the player (anim block +0x3c, `range_check_1st`); the puffs start two point lights, `flick_light` (range 2 to 3-4 m, 0.98/0.90/0.80) and `flick_light2` (1 to 2 m), saturated (light-state flags 0x102). A saturated light sets the device fog colour for the polygons it lights; the GPU layer used the frame's last fog colour for every fogged draw, so any such light would tint the whole scene: fixed (FIX (A), fixes.md; `sdl_render3d_fog_colour_is_per_draw`). Not reproduced in play: a Thor's jets over c1m1, c2m1 and c3m1, in the cockpit and with the outside branch forced, gave one fog colour every frame (pixels identical before and after the fix). Asked: Owen's view (cockpit or outside), mission and a shot |
| W12 | The mobile field bases are not on the radar, driving or deployed; once targeted (W) they show | the radar's contact blips (0x0043dd00 from 0x0043e445, hud.md section 15) | dev | fixed (mac4/w12-radar; a port bug): the port took the field base's vtable to be 0x005c8d48, whose vt+0x3c is 0x00410510 (no blip), but that is the vehicles' base class (0x004aeb00, no instance); the field base's own, 0x005c23d8 (0x004589d0), has the vehicles' 0x0045b970, so the exe draws a base with the vehicles' ally blip (Owen remembers blue triangles). TARGET's own contact is drawn on its own path, which is why a targeted base showed. Also its TARGET tint (vt+0x68, 0x004ad390). Test: mission_c1m1_field_bases_repair_the_player_headless (every base on the radar has the ally blip, driving in and parked; break: no blip, 703 frames unmatched) |
| W13 | Widescreen 1920x1080 stretches the picture instead of widening it; at 16:9 the options' items are not in their places (Owen's shots 15..20, d02a386) | the camera's angle across (0x0040d590) and the menus' frame on the wide canvas (resolution.md section 11) | dev 2 | fixed (mac5/w13-wide): both after a mode changed mid-mission (Owen played c2m2 at 640x480, then chose 1920x1080). The camera kept 4:3's 80 degrees across over the 16:9 frame (the exe sets it only at the load: its modes were all 4:3), and the options reached from the pause took the mission canvas's 106 origin while showing at 640x480. Tests: `mission_c1m1_mode_change_keeps_the_camera_shape_headless`, `mission_c1m1_options_from_the_pause_keep_the_menus_frame_headless` |
| W14 | Resizing the window moves everything; the HUD out of place | the frame's fit and the canvas across a resize | dev 2 | not reproduced on the Mac (windows of 1000x1000, 2400x1000 and 1600x900 mid-mission keep the frame's fit and the HUD's places); waiting for Owen's details. `--resize WxH@N` (mac5/resize) for checking |
| W17 | Op 2: told to follow or attack, Dominic "just runs into a wall" | the follow order's steering (0x004d1980), the wall response, the AI nets | dev 2 | fixed (mac5/w17-follow-path, ours behind bugs): the exe steers straight and does not route; behind a wall he bounces off and the contacts' push holds his carrot inside his slack, so he stands short for good (the original's, ai.md section 17.6). The mission nets are unit routes, not a nav graph. Now, stopped short, he walks the clear heading nearest the target until the line is clear. Test `mission_c2m1_dominic_follows_round_walls_headless` (c2m1 51.5 m against the exe's 73; open ground unchanged; the give-up) |
| W27 | c2m4: Dominic "spent the entire mission ramming into a wall, and after I took out all the enemies he popped straight through it"; then "when I turn a corner he just runs into a wall perpetually" (with W17) | the follow's way (ai.md 17.6, W27) | dev 2 | fixed (mac5/w27-breadcrumbs, behind bugs): breadcrumbs of the leader's way; a walked route round the nearest wall on c2m1 and c2m4 brings him into range in 7 and 11 s (the exe never); attack round a wall too. The pop-through not reproduced (no teleport in c2m4; the bump did not push him through); Owen's place and what he did would help Owen's exact spot (c2m4, 2026-10-30): where the water tunnel turns into a ramp before the right turn to the Owens, Dominic "shot up to the ceiling and took head damage"; then at the Owens, following the player out of the tunnel to the Thor on the right, he pushed into the right-hand wall for minutes and popped through. One attempt (Owen: not worth much energy): a mech-wide A* route from the player's start to 40 m short of thor_01 (1037 m off, 1065 m of route) did not reproduce it; both builds follow down it (the exe stalls 10 s once), so the route is not the tunnel and corner he took. Needs the place (a save or the coordinates) to go further |
| W22 | At 16:9 in third person, after the pause "the camera is all over the place... I can't see my mech, like it went to the top left corner" (f787f1c) | the menus' whole-screen 2D layer and the 3D's viewport | dev 2 | fixed (mac5/w22-camera): not the camera (its place, angles, window and the W13 angle all the same after the pause). The pause screen's layer (`SdlScreen2D::present_16`) set the GPU's viewport to the 853x480 canvas and left it; the resume is the same mode, so nothing set it back and the mission drew its 1920x1080 frame's top-left 853x480 over the whole window. At 640x480 the canvas is the frame, so nothing showed. The layer now puts the viewport back; `sdl_layers_keep_the_3d_viewport` (fails without it), and `mission_c1m1_pause_keeps_the_view_at_16_9_headless` checks the views' camera and angles across the pause |
| W19 | "The mouse cursor sits in the centre of the screen now. The aiming reticle works fine" (f787f1c, fullscreen, a mission) | the held mouse's relative mode and the system cursor (sdl_input.cpp refresh, input.md section 8) | dev 2 | fixed (mac5/w19-cursor-fix), not seen on the Mac: since the mouse-release change SdlInput asked SDL for the relative mouse only when its own flag changed, so one SDL refused or let go (a fullscreen switch, the focus) was never asked for again; the system cursor then showed where the game's SetCursorPos puts it each frame (the centre) while the motion still aimed. Each read now checks SDL's state, and a held mouse hides the system cursor itself. `sdl_held_mouse_keeps_the_cursor_hidden` (3 checks fail without) |

## 2026-10-30, our own visual pass (S: shots, not yet triaged)

The 20 campaign missions and t1, each in a scratch write dir with
`--hidden --nosound --frame-ms 33`: the start (frame 200), outside with X
(0x2D at 150, frame 220), and 40 m from the first enemy mech
(`--mech-ahead 40`, then E, D and Ctrl+D, frame 420; t1 has none). Crops in
`/Volumes/Callisto/claude-files/outbox/2026-10-30/visual-S*.png`. Nothing
fixed yet.

| # | Mission, shot | What looks wrong | Crop |
|---|---|---|---|
| S1 | c2m1, c3m5 and c3m1, near the enemy (frame 420); **as the original**: the same texture, rivets and all, lit by the incoming fire's lights (zWeapon's shot and impact lights, IMPACT HEAT's colour) for a few frames (seed 1: partial at 380, full at 420, gone by 500, before the first hit lands at 410). The cage is drawn by the world draw with the world's lights (0x4f73f0, ported and matched); its textures, records and GPU textures never change. Not the decals (with InstallOptions 0x101 too), not the keys, not a second cage (only the player's exists) | The cockpit's struts and dashboard wear a mech's camouflage (c2m1, c3m5: tan or yellow camo) or a strong pink-red (c3m1), where every start and outside shot has them grey metal. Only in the near-enemy runs (combat, E, D and Ctrl+D): a shared texture or material changed? Candidates: the damage decals (new 5c1c5a9), an enemy's skin swap, TARGET's views | S1-*-cockpit-*.png, against S1-c1m1-cockpit-normal |
| S2 | c4m1, c4m2, c4bm4, near the enemy (the mission-failure box); **ours, fixed**: only `--mission` showed it, its launch never set the operation (`launch().op`, 1); a campaign start takes it from the campaign. Now set by the directory as `--briefing`'s: c4m1 says "Operation 4, Mission 1" | The objectives box's title reads "Operation 1, Mission 1/2/4" in Operation 4 and 4b missions; the objectives under it are the mission's own. Maybe only the debug launch's operation number (to check on a campaign start) | S2-*-objectives-title.png |
| S3 | t1m1, the start (frame 200, and every shot); **likely as intended**: the same start as training's own (type 4 starts at the same place; the debug launch cannot run training's objectives), the hall's open front showing its lit inside; Owen to compare | Beside the player a box shows a lit interior (walls with red lights) from outside, against open sky and ground: one-sided interior faces seen from outside, or the player started outside the hall it belongs to | S3-t1m1-interior-box.png |
| S4 | c1m1, outside (X, frame 220) | The sky is a flat dark grey (fog) while the cockpit's, a moment earlier, is bright with clouds; the other missions' outside views are bright. The camera low over water: the water fog? To compare with the original | S4-c1m1-outside-dark-sky.png |
| S5 | Every start in daylight (c1m1 shown) | The weapon list's green text on its translucent green panel is barely readable against a bright sky (fine on dark skies). Possibly as the original; to compare | S5-c1m1-weapon-list.png |
| S6 | c1m2, the start (frame 200); **likely an effect**: beside a smoke column, no lens flare in c1's world; Owen to compare | A small blue crystal with a bright lens-flare glint low on the left, on the ground near the player: an object or a light's flare, to identify | S6-c1m2-sparkle.png |
| S7 | c2m4, near the enemy (frame 420); **ours, fixed**: `--mech-ahead` stood the player on the highest ground at the spot, the cave's roof (under the world's black sky), the Thor below; it now takes the ground within 10 m above the enemy (`chain_player_near`'s `at_its_level`) and the nearest enemy mech in play: the Thor 40 m ahead in the cave | Placed 40 m from a Firefly and turned to face it, the player looks at an empty grass field under a black sky, no mech and no target. Maybe the placement only (the cave mission's outside has no sky), or the mech not drawn | S7-c2m4-near-nothing.png |

Also seen, likely as intended: c2m3's HUD static while being hit; c3m2's
player mech wrecked within the 11 s (placed by an enemy); c4bm5's copper
cockpit (the lava world's light); the mission failures from `--mech-ahead`
taking the player out of bounds. Not a visual issue but ours: `mw3 --help`
prints the `--mission` line twice (one with `--turret-ahead`, one with
`--mech-ahead`, a merge's leftover): fixed. S4 and S5 wait for Owen's
comparison with the original.

### Visual pass 2 (2026-10-30): the front end and 16:9

The main menu, pilot screen (Campaign; Training goes there too), Instant
Action, game options, graphics and audio, controls, credits (frame 900:
they scroll in), the c1m1 briefing, the lab's three tabs (`--lab-mech 9`),
the debriefing (`--briefing c1m1 ... --win 60`) and the chain's briefings
and debriefings; then c1m1, c2m2 and c4bm5 at InGameVMode 13 (1920x1080),
11 (1280x720) and 5 (640x480), the start and near an enemy. The salvage
page needs a second mission (c1m1's briefing greys it); the store is the
lab's stockpile. At 16:9 the view widens, the cockpit keeps its shape and
the HUD's pieces keep to their corners and edges; 640x480 matches the first
pass. Crops: `visual-S8..S13*.png` in the same outbox.

| # | Screen | What looks wrong | Crop |
|---|---|---|---|
| S8 | The lab, criticals tab (`--lab-mech 9`, the CRITICALS click); **as the original**: the line is the criticals page's dialog text at (220, 60), inside the 3D view's window (210, 52)-(430, 288) ([0x005e4688], set once by 0x00450460), and the Lab State draws the turning mech after the screen (0x00448f10 under the lock, then 0x00450690), so its head covers the line as it turns past (mechlab.md, The 3D view) | "12 Criticals Available / 14 Reserved ... Ferro-Fibrous Armor" at the top runs under the 3D mech view: the view drawn over the text | S8-lab-criticals-text-under-mech.png |
| S9 | The debriefing (both `--briefing` and the chain's); **as shipped**: the rank icon is `icon_rank<score / 100000>_green` (0x0047b2f0, 0..4) from rimage.zbd, and rank 0 (Sergeant) is that diagonal-stripe insignia; 1..5 are the bars and the stars | The rank badge beside "Sergeant" is a yellow hatched square, the same in every debriefing: a missing insignia image, or the art as shipped? | S9-debrief-rank-badge.png, S9-chain-debriefing.png |
| S10 | The lab, criticals and groups tabs; **as the original**: one camera (place (0, 5, -11), rotation (0.02, pi, 0), set once at 0x00450321 and never moved) and one window for every tab; the Bushwacker turns 0.8 radians a second, so a shot at another moment shows it side-on, its gun arm to one side and its feet at the window's bottom edge (288). Shots of all three tabs at frame 3000, 4:3 and 16:9, frame it alike (dev2.md item 6) | The mech is framed low and to one side, cut off at the bottom, where the chassis tab shows it whole (a view still moving at frame 2900?) | S10-lab-groups-mech.png |
| S11 | Controls setup; **as the original**: a ListBox (0x0059e430) shows the selected string at its POSITION with LISTSIZE's `above` entries stacked over it, hiding those past either end (twod.md, Sliders and list boxes); the first command selected leaves its three rows above empty | The list starts with three blank rows above "Forward throttle" (the selection kept mid-list?) | S11-controls-blank-rows.png |
| S12 | Missions at 1280x720 (mode 11); **for Owen** (the HUD art scale decision, for-owen.md) | The HUD is drawn at 1x (the whole art scale of 720 / 480), so it covers less of the screen than at 640x480 or at 1920x1080 (2x); readable but small. A design choice for Owen (resolution.md section 8) | S12-hud-1280x720.png against S12-hud-640x480.png |
| S13 | The chain's c1m3 briefing; **timing, ours**: `--briefing c1m3` at frame 2500 shows OP: Able, Baker and Start; the chain shoots as the briefing loads, before the nav points come up | The map shows no nav points or labels, where c1m1's shows OP: Able, Baker and Start (the chain shoots at the briefing's load: maybe before they are drawn) | S13-c1m3-briefing-no-labels.png |
| S14 | Missions at 1280x720 and up (found in the release timing runs, dev 2); **fixed, ours** (FIX (B)): the colour tint's Direct3D quad (0x00558540 -> 0x0057dd80) took the 2D canvas's rect (960x540 at 1920x1080) in the frame's pixels; now through `canvas_to_render`, as the HUD's panels (resolution.md section 8). The end fade draws the still, not the tint, and was right; the menus run at 640x480 | The mission's opening fade darkens only the top-left quarter of the screen at 1920x1080 (a 960x540 rectangle) | outbox/2026-10-30/S14-fade-1080-{before,after}.png |

### Visual pass 3 (2026-10-30): after the HUD scale and the 16:9 default

A regression pass after dev 2's HUD scale (the 853x480 canvas at 16:9, the
art at height/480, sharp filtering) and the 16:9 first start: the 20
campaign missions and t1m1 at InGameVMode 13 (1920x1080), 11 (1280x720)
and 5 (640x480), each with pass 1's three shots (the start at frame 200,
outside with X at 220, 40 m from the first enemy mech at 420 with E, D and
Ctrl+D), plus the objectives box (F12) and the pause menu (Esc) in c1m1,
c3m2 and c4bm5, and c1m1's briefing and debriefing at each size: 210 runs,
`--hidden --nosound --frame-ms 33 --seed 1999`, all exit 0. At every size
the HUD's pieces keep to their corners and edges at the same share of the
screen, nothing overlaps, the radar, nav text and arrows, compass, heat,
speed, the damage views and TARGET read alike, the objectives box is
centred at the bottom at its 480-high size, and the briefing and
debriefing sit 4:3 in their pillarboxes. Crops: `visual-S15..S18*.png` in
the same outbox.

| # | Screen | What looks wrong | Crop |
|---|---|---|---|
| S15 | Any fade from a mission at 1280x720 or 1920x1080 (c3m2's death at frame 420; the debriefing's entry); **ours, fixed** (the HUD scale's regression): the fade's still (0x00579500) is grabbed at the mission's canvas (853x480) and drawn (0x0055b1f0) after the switch to the menus' 640x480; the draw took only a still of the buffer's own size, so it fell through to the census (`not ported: 0x0055b1f0( 0x10100 ...)`) and the fade showed nothing. The still now keeps its size and is scaled to the buffer (`GameEngine::image_draw`; `frame_lock_lays_the_layer_and_the_tint_goes_through_the_ports`) | The frame is a flat dark grey where 640x480 shows the wrecked mech and the world fading | visual-S15-c3m2-death-fade-1080-before-after.png |
| S16 | Every mission at 1280x720 (mode 11); **for Owen** (the HUD scale's filtering): the art is drawn at 1.5x with sharp (nearest) filtering, so one-pixel strokes come out one or two pixels wide by column; text and lines read slightly uneven ("M. Gun" letters of different weights). 1920x1080 (2.25x) shows it less, 640x480 not at all. Choices: as is, integer scaling (1x at 720 with a margin, S12), or a sharp-bilinear pass (nearest to the integer scale, then linear) | The weapon list's and the compass's letters uneven at 720 | visual-S16-weapon-list-720-vs-480.png |
| S17 | t1m1, every shot and size; **likely as intended, to confirm**: since the training slot and its lesson loadout (b20f938) the debug launch runs training's own start, whose first lesson brings the HUD up piece by piece; pass 1 (before) showed the full HUD | Only the clock is drawn: no radar, weapons, heat or crosshair | visual-S17-t1m1-hud-now-vs-pass1.png |
| S18 | The pause menu (Esc), every mission; **likely as the original at 640x480, for Owen at 16:9**: the dialog is 640 wide, centred in the canvas, over the whole frame's blur (no pillarbox); its MECHWARRIOR 3 logo is wider than 640, so at 640x480 the "3" and the last letter's edge are cut at the screen's edge, and at 16:9 the logo shows whole beyond the dialog's 640 | At 640x480 the logo reads "MECHWARRIO"; at 1920x1080 "MECHWARRIOR 3" | visual-S18-pause-480-vs-1080.png |

Also seen, the same at every size (so not the scale's): the weapon list's
last row (c1m1's tenth weapon, LRM 20) sits on the panel's bottom edge,
and the compass's last label is cut at its strip's end ("2:"). The
census's 0x004e6dc0 and 0x0041b263 in c3m2's death (the player wrecked
by frame 420) at every size: left for the census owner.

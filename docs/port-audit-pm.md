# Port audit: the original functions not ported (pm)

Every game function of the original (below the C runtime) that has no port of its own, labelled from its decomp,
its callers and callees, the port's comments and the docs (tools/analysis/port_audit.py; labels with their
reasons in tools/analysis/port_audit_labels.tsv). Addresses and plain words only.

Classes: **folded** (into its callers or a ported class: getters, setters, constructors, destructors, thunks,
container code, or covered where the port names it); **replaced** (SDL or the platform layer); **PORT (preserve)**
(debug and dev tools, unused code: to be ported for preservation, not called by the game); **MISSING LOGIC**
(runs in play and the port does not, or does it differently); **UNSURE** (what would settle it is said).

## Method (pm)

Each PM function is paired with its MW3 twin by analysis-output/pm-diff (pairs.json, addrmap.json). A pair
the diff calls identical takes the MW3 label ("as MW3 ..."). A changed pair under a shared port was read:
those whose control flow differs from the twin's one by one, by file; those whose text differs but not their
control flow either by reading or by **the feature compare** (tools/analysis/pm_feature_compare.py, at commit
7f6e9e10): the calls and data references (MW3's mapped to PM), the immediates and the float constants by their
raw values, as multisets over the function, and the number of field accesses, field stores and distinct field
offsets (the offsets themselves reported apart: PM's layouts shift them). A sample of 40 was read whole; the
functions the compare flags are read one by one, the rest are labelled "folded, by the feature compare". Its
earlier versions counted no field accesses and passed added reads and writes (a new field cleared, a new flag
byte): the 130 they passed and this one flags were taken back to be read. **Residual risk**: the compare cannot
see an access moved from one existing field to another the function already touches, with the counts unchanged;
the offsets are not yet mapped through the pm port's PM structures to check each one against the layout's shift.
A call the address map cannot place counts as "unmapped" on both sides, so a caller passes with an unmapped
callee that differs from its MW3 counterpart: of the 867 folded by the compare, 161 have such calls (68 callee
pairs, matched by their place in the call order: 47 equal by the same compare, the other 21 mispaired by that
order or differing). Each unmapped PM callee has a row of its own here, so a difference in it is labelled on
that row, not on its caller's.
**Checked by test** means a passing PM differential test ran the
function's body (a block trace of the emulator, per entry the test calls) and compares what that body
produces; the label names the compared value. Reaching a function, or running a caller, is not enough.

| Class | Functions |
|---|---|
| folded | 2636 |
| unlabelled | 1716 |
| replaced (SDL or the platform layer) | 231 |
| MISSING LOGIC | 153 |
| MISSING LOGIC (only data the shipped game lacks reaches it) | 112 |
| software renderer | 106 |
| debug or dev tool: PORT (preserve) | 85 |
| unused: PORT (preserve) | 61 |
| dev dialog (MFC), its window plumbing: replaced | 48 |
| dev dialog (MFC), its logic: PORT (preserve) | 35 |
| UNSURE | 33 |

## MISSING LOGIC

- **0x00405f20** (1 functions, 71 decomp lines; MISSING LOGIC): pm only, visible: PM's briefing frame (MW3's 0x00406440, unpaired by the diff; briefing.cpp BriefingScreen::update) is MW3's but for the skip: on Space or S, PM first runs every step left in the script, from the one playing to the end, once each with 60 seconds (vt+0 with 60.0), then stops the voice and clears the script as MW3 does; MW3 and the port only stop (BriefingScript::stop). So skipping PM's briefing leaves the map as the whole script would have drawn it (its objects shown, lines and markers placed); the port leaves it where the skip came
- **0x00408b20** (1 functions, 50 decomp lines; MISSING LOGIC): PM's camera ground check (pm only): the ground under the camera probed from 1024 m above it ([0x006026e0]) in PM, 50 m in MW3 (player_view.cpp:797 keeps MW3's), and PM's set_underwater 0x00408d00 also sets the renderer's water mode (0x005404f0: 2 under water, 1 not) besides the fog
- **0x00408d00, 0x005404f0** (2 functions, 40 decomp lines; MISSING LOGIC): PM's fog under water (pm only): PM's set_underwater 0x00408d00 sets the fog mode [0x00a4e038] to 2 through 0x005404f0, which keeps 2 against anything but 0, so both model draws fog every model, not only flagged ones (gmod_draw.cpp:268 reads the mode); back above water it sets 1. The port's set_underwater is MW3's (player_view.cpp:820), zmod::fog_set_on has no sticky 2, and nothing sets 2, so under water only flagged models are fogged
- **0x0040b120** (1 functions, 47 decomp lines; MISSING LOGIC): a PM value the shared port keeps as MW3's (pm only): the object detail's level 2 sets the camera's LOD scale to 0.2 in PM (0x3e4ccccd), 0.4 in MW3 (options.cpp:260, cam_scale)
- **0x00414ba0** (1 functions, 25 decomp lines; MISSING LOGIC): pm only, minor, effect to check: PM's mech_set_skin (MW3's 0x00414390, mech_build.cpp mech_set_skin; unpaired by the diff), from the mech build 0x00415190, the network (0x0046f510, 0x004701d0), the mission launch 0x00494500 and the player 0x004a0e10 / 0x004a17d0: when the torso's headlight (torso part +0x7c) is there, its node's only child is detached before the skin is applied (0x00479c30, kept at +0x38) and put back after (0x00479c60); the port applies the skin with the child in place. Whether the skin apply (0x004773e0) touches that child is not read; 0x0041b900 (a vt slot) also detaches it
- **0x00415940, 0x005a35f0** (2 functions, 206 decomp lines; MISSING LOGIC): pm only, minor (a mech deleted mid-mission: a network player leaving): PM's mech destructor (0x00415940, MW3's 0x00414f40, mech_build.cpp:225) first clears the owner (+0x6c, +0x70, +0x74) of every live shot that mech fired (0x005a35f0 over zWeapon's pools); MW3 and the port do not (all three free the node's handler table: MW3 0x00583400, PM 0x005a4de0, the port app_mech.cpp:848), so a shot outliving its mech keeps the gone owner. To check: whether the port's shot reads that owner afterwards (its hit credit, its tether)
- **0x0041c630** (1 functions, 21 decomp lines; MISSING LOGIC): as MW3 0x0041b930: the player's mech suspended while the view is off it: the mech's vt+0x104 (0x0041b980, called by the view's attach 0x0040adf0 on the old target) puts the player's mech into state 14 'suspended' (Mech Update returns at once; also the HUD's 0x00430aa0), vt+0x100 (0x0041b930, on the new target) puts its state back. The port's view hooks say these do nothing (app_view.cpp:165) and nothing sets state 14 (mech_update.cpp only reads it). In play the view leaves the player's mech at a mission's end, on a rebuild and for the editor or stealth camera; whether it shows (the mech going on under its controls meanwhile) is to be seen. Port: small
- **0x0041c680** (1 functions, 22 decomp lines; MISSING LOGIC): as MW3 0x0041b980: the player's mech suspended while the view is off it: the mech's vt+0x104 (0x0041b980, called by the view's attach 0x0040adf0 on the old target) puts the player's mech into state 14 'suspended' (Mech Update returns at once; also the HUD's 0x00430aa0), vt+0x100 (0x0041b930, on the new target) puts its state back. The port's view hooks say these do nothing (app_view.cpp:165) and nothing sets state 14 (mech_update.cpp only reads it). In play the view leaves the player's mech at a mission's end, on a rebuild and for the editor or stealth camera; whether it shows (the mech going on under its controls meanwhile) is to be seen. Port: small
- **0x00430c10** (1 functions, 97 decomp lines; MISSING LOGIC): read after the feature compare (7f6e9e10) flagged it: pm only, low object detail (the option at 2; PM's mech build sets [0x006293a8] = (0x0040b1e0 == 2), the flag the pm port keeps as lod_table().low): PM's mech data then loads only the mechlib model's variant 1 (0x00430c10, MW3 0x004308a0: both variants, the second's slot cleared) and resamples each gait to 15 keys (0x00431730, MW3's 60) and each action to 10 (0x00431a00, MW3's 20); the port's mech_data_load, load_gait and load_action_motion (mech_data.cpp:354, kGaitFrames 60, kActionFrames 20) are MW3's at every detail. With the other low-detail pieces (0x00492830's)
- **0x004311e0** (1 functions, 228 decomp lines; MISSING LOGIC): PM values the shared port keeps as MW3's (pm only): the god camera (mode 3) at 120 m and pitched -81 degrees (-1.413716) in PM, 150 m and -90 degrees in MW3 (player_view.cpp:446-447)
- **0x0043b2d0** (1 functions, 71 decomp lines; MISSING LOGIC): pm only, minor, group A, read: the TEXTURES cycle offers the same items by the same bits and card memory, but selects differently: PM selects the screen's stored value per device (app +200, +0x14150 for software, +0x14154 + device*4 for a card) and a card past index 3 first selects 2 MB, with no fallback, so an unset (0) value leaves the selection where it was; MW3 and the port select the last item added (or the card's megabytes), then TextureMemory if set (options_menu.cpp:326-360) (MW3 0x0043a310)
- **0x0043cf50** (1 functions, 57 decomp lines; MISSING LOGIC): PM's HUD ZAPS static (MW3 0x0043be70's version, the hit's static on a HUD channel): unported, and its hook is the census stand-in DamageCalls.zaps_hud, as in MW3 (docs/port-audit.md, BROKEN row 3)
- **0x00440ac0** (1 functions, 33 decomp lines; MISSING LOGIC): as MW3 0x0043f2d0: the radar's map-mode position (HudRadar 0x0043f2d0): stores the mech's position and, with the mission map shown, moves the mech's marker line on the map (0x00491c80, 0x005a6880); not ported, and its hook radar_heading (hud.h:880) is never overridden: see BROKEN
- **0x00451730** (1 functions, 47 decomp lines; MISSING LOGIC): pm only, small, PM's own quirk (port as written, a discoveries.md line): the mech lab's part highlight; in PM the torso parts (1, 7, 8) light the head green with the torso (weight 0.3) and the head (6) lights nothing, where MW3 and the port light the head only for 6 (lab_view.cpp:272-296). PM numbers the parts as MW3 does: its list fill 0x00456760 adds 6 first with the cockpit, life support and sensors texts (0x24e..0x250), then 1, 7, 8, 2, 3, 4, 5, so PM's 6 is the head (MW3 0x004504d0)
- **0x0045a250, 0x0045b100** (2 functions, 223 decomp lines; MISSING LOGIC): pm only, field bases: when a field base (MCB) dies, PM first counts the bases still alive (0x0045b0d0) and, with none left, aborts the dock in progress (0x0045b320, the front state's abort, the port's FieldBases::abort_front); then the campaign's cut as MW3's (the base count by the campaign's vt+0x5c, the share 1 / 0.7 / 0.5, 2 mechs and 300 tons kept per base left). The port's field_base_die (mech_fieldbase.cpp:316) has the death and the cut but no abort, so on PM the dock in progress when the last base dies is not aborted (what the abort undoes: mech_fieldbase_dock.cpp:196)
- **0x0045b240** (1 functions, 44 decomp lines; MISSING LOGIC): pm only, multiplayer: PM's dock registry frame (0x0045b240, from the mission frame 0x004921a0, in MW3's 0x0045a270's place) runs the dock states in every game type and has no multiplayer branch; PM checks the deployment in its refit instead (0x0045be50, ported: DockRefit::may_start). The port's FieldBases::frame (mech_fieldbase_dock.cpp:329) is MW3's on PM too: in multiplayer (type 3) it runs no dock states and, with mobile bases on, MW3's station check and its 'too far from MFB deployment point' line, which PM's frame never does
- **0x00469c80** (1 functions, 63 decomp lines; MISSING LOGIC): pm only, minor, effect to check: the lance command's target marker (cmd_target.flt; MW3's 0x00468100, the port's make_marker, app_pocket.cpp:175): PM gives it side bit 1 outright (MW3 the player's side by 0x004d5af0(0), 1 for side 0 as well), passes it to 0x004e88b0 (a list's add-if-missing on the AI registry), and starts it switched off: its node inactive (0x004fca50(node, 0)) and its +8 byte cleared; the port's marker starts on, as MW3's. Whether anything finds the marker while it is off is to check
- **0x0047be40** (1 functions, 29 decomp lines; MISSING LOGIC): pm only, field bases: the MCB waynet's nearest point (0x0047c960, MW3 0x0047a680, the port's McbWaynet::nearest, mech_waynet.cpp:312): PM walks the waynet's points itself (0x0047be40 with 0x0047bdc0 / 0x0047bde0) and takes the nearest whose hidden byte (+0x11) is clear, selecting nothing when none is; MW3's (0x00479b50, the port's Waynet::nearest, mech_waynet.cpp:195) asks the net's nearest step and falls back to the current point. PM hides the dock points that aren't static MFBs as it counts them (the port's dock_count, mech_waynet.cpp:417), so on PM the port can pick a hidden point where PM would not
- **0x00482ca0** (1 functions, 193 decomp lines; MISSING LOGIC): PM's sound range at low detail (pm only): at start-up PM sets [0x00613488] to 0.5 when the object detail is 2 (0x0040b1e0), else 1 (0x00482ca0 -> 0x00591830), and a sound's default RANGE (50 400) is scaled by it (0x005912a0, MW3's 0x005700f0 unscaled): at low detail every sound without its own RANGE is heard to 25 / 200 m. The port's start-up and the sound set's defaults are MW3's
- **0x004838b0** (1 functions, 184 decomp lines; MISSING LOGIC): group A, read: PM's mission music (pm only): on a disc of more than five tracks PM plays from a random track on to the end of the disc (mode 3, [0x00607f28] = 3), where MW3 and the port play that one track (mode 6, app_states.cpp:390-392); with 2..5 tracks both pick by mission (PM through the pilot's vt+0x48, the port's vt+0x4c: to check it is the same call on PM's layout); a forced -cdN plays in whatever mode PM last set. The developer lines it writes (Dummy Frame, Let's Play) are 0x00410820's, developer only
- **0x00488470, 0x004885b0** (2 functions, 147 decomp lines; MISSING LOGIC): pm only, visible: PM's menu background (vtable 0x005e6ea8, built in the screens: briefing, credits, controls, options; MW3's ScrollImage 0x00438cc0 / 0x0040a210, menu_widgets.cpp) scrolls by time: each update moves round(22.0 * dt + the carried fraction) pixels on each axis (steps +0xbc/+0xc0 = 22.0f, fractions +0xc4/+0xc8, 0.5 at 0x005de818), about 22 px a second; MW3 moves 1 px a frame, which the port keeps (FIX (B) frames owed at 33 ms: about 30 px a second). The view rect is (0, 0, w, h - 1) in PM; the object 8 bytes larger (moving at +0x140)
- **0x0048eaf0, 0x0048f660, 0x0048f970** (3 functions, 607 decomp lines; MISSING LOGIC): pm only, pilot-file compatibility and play (fix list: both), Instant Action's kept line-up: PM keeps the allies' and enemies' mechs in the pilot's game record by item value (the mech number; the field base's -1 or 10; ACCEPT 0x0048eaf0 through 0x00439620) and each enemy's loadout by its place among the mech's loadouts (0x004279e0), not its row in the cycle; it restores them by value (0x0048a2e0; reset 0x0048f970 for the allies, 0x0048f660 for the enemies, the loadout reset to 0 when past the mech's count). The port keeps and restores row indices (action_menu.cpp keep_lineup, show_custom, reset) in the same record fields, which it writes to the pilot file (pilot_games.cpp ActionGame::save). So a pilot saved by the original PM brings back other enemies and allies in the port, and the other way round (rows with none first are one off). Also, with no kept enemies (a new pilot's -1), PM's first enemy is mech 0 with its loadouts listed (hidden ones left out, message 0xd75 for a hidden first); MW3 and the port pick the player's mech. 0x0048f970 and 0x0048eaf0 are put here for this; their other differences are read with the unlabelled pass
- **0x004943e0** (1 functions, 13 decomp lines; MISSING LOGIC): pm only, multiplayer: a network respawn's objectives reload. PM frees them (0x0045d530) and reads them again through the game's vt+0x3c (0x00435140 for the multiplayer game: objectives.zrd's mp<N>, vt+0x58 0x00436580, for every map), then clears the ended and aborting bytes; the port's objectives_again (mission.cpp:532) is MW3's 0x00491050, maps past 2 from mpinfo.zrd, which PM does not ship (no archive holds it), so on PM a respawn on map 3 or later reads no objectives
- **0x004a3200** (1 functions, 83 decomp lines; MISSING LOGIC): pm only, vehicles, traced: PM's vehicle collision record (0x004a3cb0) puts hits on a mech's part nodes (0x0040f210, the part's owner) in a new list +0x358 and other entities' (0x0040f1a0) in +0x354, dropping one whose object's byte +8 is 0; the step (0x004a3200, MW3 0x004a09e0) answers either list with 0x004a4320, which runs the collision response 0x004a3e40 (MW3 0x004a0b40) on +0x354 only, and sets the hit flag (+0x374) for both. So in PM a vehicle that runs into a mech is flagged as hit but gets no response from the mech's parts (MW3 and the port respond to every entity hit: veh_move.cpp vehicle_collision_query, one list); the flag still keeps the ground step's own response (the end of 0x004a3200) from running
- **0x004a7170, 0x004a71f0, 0x004aaf80** (3 functions, 86 decomp lines; MISSING LOGIC): pm only, with bug fixes off: PM's limit clamp (0x004a71f0, with 0x004a7170; from the vehicles' vt+0x90 0x004b10b0, MW3's 0x004ad7c0) gives the limit's direction along the heading's own length, hypot(x, z): run in the emulator against MW3's 0x004a41b0 on vehicle limits (gun pitch, full yaw), the answer agrees on all 2,000 and the direction on every one within the limits, and past them PM's is the unit direction at the limit where MW3's has its hypot(x, y) typo. The port's TurretLimits::clamp (veh_controls.cpp) gives PM's with zfix's fixes on and MW3's typo with them off, and PM's AI reads it (zai_firing_pm.cpp plan_turn: there || dot(out, dir) >= cone), so on the pm branch the original path should be PM's formula unswitched. tests/differential/explore/pm_clamp_cmp.py (2026-11-13)
- **0x004abc60** (1 functions, 93 decomp lines; MISSING LOGIC): pm only, minor, sensors: PM's sensor frame set-up (0x004abc60, MW3's 0x004a8710, inside the update 0x004ac1e0 / MW3 0x004a8cd0) adds the player and handles the stealth but not the view target; PM's per-entity add (0x004ac6c0, MW3 0x004a91b0) skips only the player and the stealth, so a viewed vehicle that isn't the player comes in list order and only when its vehicle answers vt+0xd8. MW3 adds the view second, whatever its answer, and the port does as MW3 (sensors.cpp:482-485, and the loop's skip of the view at :487); the order sets which pairs run first (the player's, the lancemates', the rest)
- **0x004acec0** (1 functions, 18 decomp lines; MISSING LOGIC): pm only, sensors, the same rule as 0x004ad2b0: the range test of the player's and the lancemates' passes (0x004acb90 / 0x004acff0, MW3 0x004a9680 / 0x004a9a60): sensed when everything is shown, or within range, or when the sensing record has bit 8 (a C3 master) and the other bit 0x10 (a C3 slave), at any range; the port's in_range (sensors.cpp:85) has the first two only
- **0x004acf00** (1 functions, 43 decomp lines; MISSING LOGIC): pm only, sensors, the same rule as 0x004ad2b0 for the other direction (the other vehicle sensing the player or the lancemate): within range, everything shown, or C3 master and slave at any range; then MW3's side tests for the contact's kind
- **0x004ad5c0** (1 functions, 108 decomp lines; MISSING LOGIC): pm only, sensors, with 0x004ad2b0's C3 rules: PM's C3 merge (0x004ad5c0, MW3 0x004a9fd0) puts into a master's list only what its C3 slaves saw (the other record's bit 0x10); MW3's merges every C3 unit's, masters too (bits 0x18), and the port does as MW3 (c3_merge, sensors.cpp:373)
- **0x004b0790** (1 functions, 76 decomp lines; MISSING LOGIC): a PM value the shared port keeps as MW3's (pm only): the vehicle's above camera pitched -81 degrees (-1.413716) in PM, -90 in MW3; its 150 m the same in both (veh_basic_object.cpp:500)
- **0x004b4560** (1 functions, 39 decomp lines; MISSING LOGIC): pm only, weapons: selecting a weapon group (0x004b4560, PM's in MW3's 0x004b0730 place, the port's WeaponManager::select_group, weapon.cpp:650): when the group's current weapon is not ready (0x004b7bd0, MW3's 0x004b3a20), PM moves the group on to its next ready weapon (0x004b5e10, MW3's 0x004b1f40) before the HUD's refresh; MW3 and the port keep the current one. Reached from the group keys, the mech build, the weapon planner and two weapon manager steps
- **0x004b5330, 0x004b8ce0** (2 functions, 124 decomp lines; MISSING LOGIC): pm only, AI: the volley planner's weapon test (weapon_planner.cpp:92 shot_of and add_weapon, through in_range's type_ready()): MW3's 0x004b4ba0 needs the weapon's owner, its definition and the reload time passed ([0x0060991c] >= def+0xc4, 0x004b4c70); PM's 0x004b8ce0 needs only the owner. So PM's burst record 0x004b5230 counts a reloading weapon's damage and PM's add-weapon 0x004b5330 takes it as a candidate and into its group's sums; the port keeps MW3's ready test on PM. Reached through PM's volley 0x004b56d0 (MW3 0x004b1860)
- **0x004b5f80** (1 functions, 110 decomp lines; MISSING LOGIC): pm only, weapons, group fire (mode 2): PM's group trigger (0x004b5f80, in MW3's 0x004b2100 place, the port's WeaponGroup::trigger, weapon.cpp:455) fires the best-rated beam first, then each other weapon telling zWeapon whether its definition already fired in this volley (0x005a3680, MW3's 0x00581f90, the port's set_fire_animation): the first of a kind plays its FIRE animation, the repeats its second one (fire.animation2, zwep_shot.cpp:341). The port's trigger fires in order and never sets the choice, so every weapon of a kind plays the first animation; the beams' ground effect is cleared as MW3's
- **0x004b6e80** (1 functions, 44 decomp lines; MISSING LOGIC): pm only, weapons, the player's: PM's per-weapon enable (0x004b6e80, in MW3's 0x004b2f90 place, the port's Weapon::set_enabled, weapon.cpp:1310) adds a firing cone: for the player's own mech it runs the weapon's line to the aim point (0x004b7800 on the manager's +0x0c, the pm port's weapon_line, zai_firing_pm.cpp:922) and switches the weapon off when the cone clamps it (the line's cosine differs from the kept one). The water and side tests are MW3's. The port's set_enabled has only those two, so on PM the player's weapons that cannot bear on the aim point still fire
- **0x004bb6a0** (1 functions, 10 decomp lines; MISSING LOGIC): pm only, AI, traced: a shot that hits an ally (IControls vt+0x98, PM 0x004fbc00 as MW3's 0x004ea500) goes to PM's firing, 0x004bb6a0: the ally kept as the obstacle (+0x28), then PM's stand-down 0x004ba8c0, which keeps the plan when the firing has a name (+0x34 set, +0x30 = +0x10, +0x4c = 0) and otherwise rests, drops the plan, clears the aim and, with the vehicle's +0xd4 set, calls 0x004b4a20(0), phase 5. The port's ally hit (mech_control.cpp:636, zai_brain.h:632) goes to MW3's Firing (obstacle, stand_down: zai_firing.cpp:109), which PM's AI does not use (PM_AI=1 runs PM's firing record, zai_firing_pm.cpp, whose stand-down hook is the plan's end, not this); so under PM's AI an ally hit neither marks the obstacle nor stands the firing down
- **0x004f9090** (1 functions, 22 decomp lines; MISSING LOGIC): pm only: PM's turret turn (0x004f90f0, MW3 0x004e72f0, veh_controls.cpp turret_turn) reads the turret node's world matrix through this, which first brings it up to date (0x004fefa0 when the node's data bit 0x20 is set); MW3 reads data +0x60 as it stands, and so does the port's node_world_matrix hook. Effect: PM's turret aims from this update's matrix, MW3's from the last; to check at fix time
- **0x004fd0d0, 0x004fd160, 0x004fd1f0** (3 functions, 39 decomp lines; MISSING LOGIC): PM's per-node light override in the model draw 0x00507f30 (pm only: the fix goes in PM's draw on the pm branch): node +0x2c bit 7 with an ambient value in bits 9..16 (0x004fd0d0 -> 0x0050e6f0), bit 8 with a diffuse value in bits 17..24 (0x004fd160 / 0x004fd1f0 -> 0x0050e7c0); the shared draw (camera.cpp:79) has none. Counted with the port's PM mechlib reader (probe_pm_mechlib_overrides, all 76 trees, 1,847 nodes): 24 nodes carry it, the cockpit 'grill' of every mech_*_hud.flt (+0x2c 0xb281 in the file: an ambient override of 0x59, no diffuse). No MW3 world or mechlib node and no PM world node sets these bits
- **0x00507f30** (1 functions, 269 decomp lines; MISSING LOGIC): group A, read: PM's model draw (pm only): the per-node light override (node +0x2c, the cockpit grills; see 0x004fd0d0) and PM's fog mode test; the shared draw (camera.cpp:79) has neither
- **0x00509dd0** (1 functions, 187 decomp lines; MISSING LOGIC): group A, read: PM's world draw (pm only): its fog calls are PM's (0x00504450, 0x0050a1e0, and 0x005404f0, the sticky fog mode) where MW3's are 0x00526a60 / 0x00526b80; so the per-frame fog on and off (camera.cpp:1087) cannot clear PM's under-water mode 2: part of the under-water fog row (0x005404f0)
- **0x0050a550** (1 functions, 32 decomp lines; MISSING LOGIC): pm only, render: PM's scene render (0x0050a5d0, MW3's 0x004f6c50 render_scene) draws a world's direct child only when its +0x24 bit 0x80 is set or its cell bounds (+0x4c..+0x52) overlap the walk's visible cells ([0x00639020..0x0063902c]); MW3's 0x004f6c00 and the port's render_world_children (camera.cpp:1149, shared, with PM's other cell differences in the game table) draw every one. Effect: world-level objects past the view's cells, which PM leaves out, are drawn in the port (the far plane and fog still apply)
- **0x00510ab0, 0x0056b230, 0x0056b2d0, 0x0056b540, 0x0056b580, 0x0056b980 ...** (90 functions, 7486 decomp lines; MISSING LOGIC): PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib
- **0x00517b30** (1 functions, 47 decomp lines; MISSING LOGIC): pm only, BROKEN row: PM's node re-read in PM's node format (MW3 0x004ff7a0), the shared port's MW3 format only; see 0x00517ac0
- **0x00541a30** (1 functions, 9 decomp lines; MISSING LOGIC): pm only, low object detail: PM's object-detail setter (0x0040b120, MW3 0x0040d310, the port's set_object_lod, options.cpp:249) also sets [0x006112e4] = 1 below detail 2, 0 at 2 (0x00541a30), and PM's model preparation (0x00541a40, MW3 0x0053e050) reads it: at detail 2 it does not advance the materials' texture cycles (animated textures stay on their frame) and returns before its case-2 path's drawing. The port has neither; with the other low-detail rows (0x00492830's, the camera scale 0.2, the mech data's)
- **0x00572af0** (1 functions, 9 decomp lines; MISSING LOGIC): as MW3 0x00553460: the hardware mode's triangle stripper (0x004fc8b0 -> 0x00552540 over every mechlib model, flag set by zvid_init 0x00579e79 when [0x0072f348]; about 45 functions, 3,500 lines, much of it STL): joins triangles that share an edge with matching material, priority, normals, UVs and colours into strips (0x400, up to 64 vertices). Measured (strip_hits.py): 302 of 770 models change, and on 126 the ground and segment tests stop at a different face (up to 1.5 m away), so the port answers as software mode, not hardware mode. A port, queued after the audit (docs/mechlib.md)
- **0x00576390** (1 functions, 10 decomp lines; MISSING LOGIC): as MW3 0x00554f30: the hardware mode's triangle stripper (0x004fc8b0 -> 0x00552540 over every mechlib model, flag set by zvid_init 0x00579e79 when [0x0072f348]; about 45 functions, 3,500 lines, much of it STL): joins triangles that share an edge with matching material, priority, normals, UVs and colours into strips (0x400, up to 64 vertices). Measured (strip_hits.py): 302 of 770 models change, and on 126 the ground and segment tests stop at a different face (up to 1.5 m away), so the port answers as software mode, not hardware mode. A port, queued after the audit (docs/mechlib.md)
- **0x0057a770** (1 functions, 9 decomp lines; MISSING LOGIC): pm only, renderer, effect to check: PM drops the pending screen tint after each frame's finish (0x0057a770, MW3's tint_clear 0x005587b0, which MW3 never calls; PM's core frame 0x004838b0 calls it after 0x0057a5d0 when [0x0062cf29] is clear). MW3 and the port keep a tint pending until another is set (zrndr_tint.cpp:137), so a tint set once shows on every later frame in the port and on one in PM; which tints are set once (not every frame) is to check
- **0x00598230** (1 functions, 20 decomp lines; MISSING LOGIC): pm only, renderer, known: the script command MipBias (common.gw's MipBias -1) sets Direct3D's mip bias, clamped, with the state's dirty bit 4; the port reads the words and applies nothing (zinterp_parse.cpp:511), listed in docs/fixes.md as not reproduced yet, a candidate for Owen with the renderer's filtering
- **0x005a2a00** (1 functions, 52 decomp lines; MISSING LOGIC): pm only, group A, read: PM's beam release frees both running FIRE animations (+0x90, +0x94); the port frees its one (zwep_beam.cpp:192). Goes with the reader 0x005a6700 (MW3 0x00581400)
- **0x005a2ae0** (1 functions, 375 decomp lines; MISSING LOGIC): pm only, carries the low-detail impact switch (pm only: at object detail 2 (0x0040b1e0 == 2) PM's world load 0x00492830 turns off two zWeapon switches (0x005a52c0 the FIRE block's EFFECT, 0x005a52d0 the impact animations; 0x005a5170 sets both to 1 at start): the impact animations of hits (0x005a3df0), beams (0x005a2ae0) and damage (0x005a4e80) are not played, and a FIRE EFFECT is read as its MODEL (no shipped weapon has one); the pm port has neither switch (mission.cpp:114 load_world, zweapon.cpp)); the rest read after the feature compare's flags: MW3's 0x005814b0 (the beam's step, zwep_beam.cpp) but for a zeroed local grown from 0x175 to 0x196 dwords (PM's larger record) and fsub -1.0 written as fadd 1.0
- **0x005a52c0, 0x005a52d0** (2 functions, 18 decomp lines; MISSING LOGIC): pm only: at object detail 2 (0x0040b1e0 == 2) PM's world load 0x00492830 turns off two zWeapon switches (0x005a52c0 the FIRE block's EFFECT, 0x005a52d0 the impact animations; 0x005a5170 sets both to 1 at start): the impact animations of hits (0x005a3df0), beams (0x005a2ae0) and damage (0x005a4e80) are not played, and a FIRE EFFECT is read as its MODEL (no shipped weapon has one); the pm port has neither switch (mission.cpp:114 load_world, zweapon.cpp)
- **0x005a6c60** (1 functions, 286 decomp lines; MISSING LOGIC): pm only, group A, read: PM's shot spawn plays every FIRE animation (choice 0 all, k the k-th; attached when flag bit 1 and an attach node), then sets the choice back to 0; the port plays one, chosen 0 or 1 (zwep_shot.cpp:340-352). Goes with the reader 0x005a6700 (MW3 0x00585010)
- **0x005a7570** (1 functions, 36 decomp lines; MISSING LOGIC): pm only, minor: PM retires a dead shot a frame late: its retire (MW3 0x00586600) first stamps the shot (+0x638) with zClass's frame count [0x008c4b60, MW3 0x0072f338] + 1 and returns 0, and its def's shot walk (0x005a7570, new in PM) keeps it linked until a later call finds the stamp matching; so the node stays in the world at its last place, its flyout animations run one more frame and the pool slot comes back a frame later. The port retires at once, as MW3 (zwep_shot.cpp:455-466, 491-501)
- **0x00401000, 0x004b2190** (2 functions, 45 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): called only from 0x004b1f90, which are missing-mod
- **0x004010c0** (1 functions, 132 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00401100: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00401430** (1 functions, 44 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00401400: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00401530** (1 functions, 70 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00401500: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00401660** (1 functions, 36 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00401620: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x004016d0** (1 functions, 107 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00401680: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00401990** (1 functions, 58 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00401940: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00401b00** (1 functions, 75 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00401ab0, whose class PM's changes keep: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00401c50** (1 functions, 18 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00401bf0: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00401c90** (1 functions, 110 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00401c30: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00401e90** (1 functions, 48 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00401e30: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00401fb0** (1 functions, 46 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00401f50: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00402140** (1 functions, 21 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00402190: the version-105 AI nets: the older net class (0x004021c0, 0x5c bytes, vtable 0x005bb8d0) and its reader and builder (0x004025b0, ~1,800 decompiled lines in all) with their helpers. Not ported on purpose (docs/ai.md 17.9): no shipped net file is version 105; a hand-made 105 file would reach them
- **0x00402170** (1 functions, 36 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004021c0: the version-105 AI nets: the older net class (0x004021c0, 0x5c bytes, vtable 0x005bb8d0) and its reader and builder (0x004025b0, ~1,800 decompiled lines in all) with their helpers. Not ported on purpose (docs/ai.md 17.9): no shipped net file is version 105; a hand-made 105 file would reach them
- **0x00402330** (1 functions, 18 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004022c0: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00402350** (1 functions, 15 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004022e0: the version-105 AI nets: the older net class (0x004021c0, 0x5c bytes, vtable 0x005bb8d0) and its reader and builder (0x004025b0, ~1,800 decompiled lines in all) with their helpers. Not ported on purpose (docs/ai.md 17.9): no shipped net file is version 105; a hand-made 105 file would reach them
- **0x00402370** (1 functions, 51 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00402300: the version-105 AI nets: the older net class (0x004021c0, 0x5c bytes, vtable 0x005bb8d0) and its reader and builder (0x004025b0, ~1,800 decompiled lines in all) with their helpers. Not ported on purpose (docs/ai.md 17.9): no shipped net file is version 105; a hand-made 105 file would reach them
- **0x00402440** (1 functions, 34 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004023d0: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00402500** (1 functions, 24 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004024a0: the version-105 AI nets: the older net class (0x004021c0, 0x5c bytes, vtable 0x005bb8d0) and its reader and builder (0x004025b0, ~1,800 decompiled lines in all) with their helpers. Not ported on purpose (docs/ai.md 17.9): no shipped net file is version 105; a hand-made 105 file would reach them
- **0x00402540** (1 functions, 20 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004024e0: the version-105 AI nets: the older net class (0x004021c0, 0x5c bytes, vtable 0x005bb8d0) and its reader and builder (0x004025b0, ~1,800 decompiled lines in all) with their helpers. Not ported on purpose (docs/ai.md 17.9): no shipped net file is version 105; a hand-made 105 file would reach them
- **0x004025f0** (1 functions, 9 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): called only from 0x004c5cb0, which are missing-mod
- **0x00402610** (1 functions, 272 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004025b0: the version-105 AI nets: the older net class (0x004021c0, 0x5c bytes, vtable 0x005bb8d0) and its reader and builder (0x004025b0, ~1,800 decompiled lines in all) with their helpers. Not ported on purpose (docs/ai.md 17.9): no shipped net file is version 105; a hand-made 105 file would reach them
- **0x00403d30** (1 functions, 27 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00403cd0: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00403d70** (1 functions, 30 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00403d10: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00403dd0** (1 functions, 21 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00403d60: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00403e30** (1 functions, 21 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00403db0: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00403e70** (1 functions, 16 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00403df0: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00403ea0** (1 functions, 30 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00403e20: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00403ef0** (1 functions, 19 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00403e70: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move
- **0x00410ec0, 0x004c7300** (2 functions, 98 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004c0310, whose class PM's changes keep: the built-in net 0x00604008 (one behaviour: the default one, 0x00604068): what an order goes on when its vehicle has no net and the record's net is not found. The port's builtin_net() is null in the game (WiredBrainHooks does not give one), so Order::reset/Brain::push would call through a null net. Not reached by the shipped missions (every AI has a net; the mission tests pass); a mod's vehicle without a net would. Port: small (a CompiledNet holding the default behaviour)
- **0x004356c0** (1 functions, 14 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): pm only, campaign: an objective's persist name is kept across missions and gates mechs. On completion (0x0045fcf0) PM adds the objective's persist (+0x30; the port reads it, mech_objective.cpp:436, and never uses it) to the campaign's string list (+0xc4, the port's Campaign::strings, read and written with the save, pilot.h:343), and the vehicle loader (0x004a19c0, MW3 0x0049e810, player.cpp) makes a mech whose record has flag bit 4 (p12's second value, zrdraiv.cpp:381) only when its name is in that list (0x004356c0). The port does neither: it makes every such mech. No shipped PM objective has persist (raw search of the reader archives; MW3's c1 has two). Checked (2026-11-13): of the 424 p12 records in every shipped PM aiv.zrd (campaign, standalone and Instant Action), none sets bit 4 (99 set bit 5, the field after it), so no shipped mission is gated; only a mod with persist objectives and bit-4 placements would show the difference
- **0x00460b70** (1 functions, 31 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004bf4e0: the version-105 nets: the editor's reload over them (0x004bed90, the vehicles using the old net kept and handed the new one) and its kept list; not ported on purpose (ai.md 9, 17.9): no shipped 105 file
- **0x004a70c0, 0x004a7390, 0x004a7420, 0x004aaf40** (4 functions, 60 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): pm only, measured: PM's turn limits are a new class (promote1.cpp's CZValidateDir: set 0x004a7390 / 0x004a7420, made by 0x004a6ee0, tested by 0x004a7000 and its thunk 0x004a70c0) in MW3's TurnLimits' place (turret.cpp make/within, MW3 0x004a4b30 and vt+0x08). Run in the emulator against a float copy of the port's (which agrees with MW3's on all 3,000 cases), PM's test differs on 457 of 3,000 (narrow and wide yaw ranges only; the full and fixed kinds agree), and agrees on 2,999 once the yaw range is negated: a turret's yaw arc is mirrored left to right from MW3's (0x004a7390 passes -yhi, -ylo). No shipped PM turret has LIMITS YAW (aiTurrets.zrd: 42 turrets, none; FIRE_LIMITS is the bursts'), the constructor's full turn and the vehicles' full-yaw gun pitch agree, so only a mod's turret with an uneven YAW shows it. tests/differential/explore/pm_limits_cmp.py (2026-11-13)
- **0x004b1b80** (1 functions, 31 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004ae270, whose class PM's changes keep: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md)
- **0x004b1c40** (1 functions, 8 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004ae340: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md)
- **0x004b1c50** (1 functions, 8 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004ae350: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md)
- **0x004b1c60** (1 functions, 16 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004ae380: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md)
- **0x004b1ca0** (1 functions, 101 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004ae3c0: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md)
- **0x004b1ec0** (1 functions, 22 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): called only from 0x004b2280, which are missing-mod
- **0x004b1f80** (1 functions, 8 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004ae6a0: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md)
- **0x004b1f90** (1 functions, 51 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004ae6b0: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md)
- **0x004b21d0** (1 functions, 21 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004ae8e0: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md)
- **0x004b2220** (1 functions, 9 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004ae950: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md)
- **0x004b2240** (1 functions, 11 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004ae930: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md)
- **0x004b2260** (1 functions, 14 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004ae960: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md)
- **0x004b2280** (1 functions, 23 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004ae980: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md)
- **0x004c5cb0** (1 functions, 83 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004bed90: the version-105 nets: the editor's reload over them (0x004bed90, the vehicles using the old net kept and handed the new one) and its kept list; not ported on purpose (ai.md 9, 17.9): no shipped 105 file
- **0x004fcff0, 0x004fd080, 0x004fd110, 0x004fd1a0** (4 functions, 56 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): checked by data: the script commands NodeSetOverrideAmbient, NodeSetOverrideAmbientValue, NodeSetOverrideDiffuse and NodeSetOverrideDiffuseValue (PM's dispatcher 0x005b2d10; node +0x2c bits 7, 9..16, 8, 17..24): no shipped PM script uses them (interp.zbd, 30 scripts) and the PM interp has no entry for them
- **0x00504e70** (1 functions, 49 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004f1cd0, whose class PM's changes keep: under null script commands: WorldSetVirtualPartition's partition rebuild (0x004f1cd0) and the camera commands' setters (0x004f3d00, 0x004f3f70, from 0x00503c10 / 0x00503d50); no shipped script uses them
- **0x00504f50** (1 functions, 16 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004f1d90: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x00504f80** (1 functions, 15 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004f1dc0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x00505c40** (1 functions, 17 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004f2880, whose class PM's changes keep: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x00506670** (1 functions, 17 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004f32a0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x00506970** (1 functions, 9 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004f3550: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x00506e50** (1 functions, 40 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004f39d0, whose class PM's changes keep: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x00507150** (1 functions, 32 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004f3d00, whose class PM's changes keep: under null script commands: WorldSetVirtualPartition's partition rebuild (0x004f1cd0) and the camera commands' setters (0x004f3d00, 0x004f3f70, from 0x00503c10 / 0x00503d50); no shipped script uses them
- **0x00507370** (1 functions, 29 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004f3f70, whose class PM's changes keep: under null script commands: WorldSetVirtualPartition's partition rebuild (0x004f1cd0) and the camera commands' setters (0x004f3d00, 0x004f3f70, from 0x00503c10 / 0x00503d50); no shipped script uses them
- **0x0050b8b0** (1 functions, 19 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004f7fa0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x0050c660** (1 functions, 24 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004f8b80: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x0050db40** (1 functions, 20 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004f9e50: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x0050dc60** (1 functions, 22 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004fd1e0, whose class PM's changes keep: zClass's add-child for an animate (0x004fd8c0) or switch (0x004fd1e0) parent: the port's node_add_child (cls_node.cpp:186) handles worlds, links and object3d and aborts ('not ported yet') for cameras, LoDs, animate and switch parents, though the camera's own add-child is ported (camera.cpp, 0x004f33a0). No shipped data adds such a child at run time (every mission plays); a mod's scripts or data would abort the game
- **0x0050ded0** (1 functions, 29 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004fa1c0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x0050df50** (1 functions, 35 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004fa230: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x0050dff0** (1 functions, 58 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004fa2c0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x0050e110** (1 functions, 41 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004fa3e0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x0050e1f0** (1 functions, 31 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004fdc70, whose class PM's changes keep: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x00515380** (1 functions, 22 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004fd8c0: zClass's add-child for an animate (0x004fd8c0) or switch (0x004fd1e0) parent: the port's node_add_child (cls_node.cpp:186) handles worlds, links and object3d and aborts ('not ported yet') for cameras, LoDs, animate and switch parents, though the camera's own add-child is ported (camera.cpp, 0x004f33a0). No shipped data adds such a child at run time (every mission plays); a mod's scripts or data would abort the game
- **0x00515790** (1 functions, 51 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004fdcd0, whose class PM's changes keep: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x00515910** (1 functions, 24 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004fde50: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x00515960** (1 functions, 24 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004fde90: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x005159b0** (1 functions, 24 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004fded0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x00515a00** (1 functions, 24 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004fdf10: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x00515c00** (1 functions, 98 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): read: PM's LoD fade values (+0x34, +0x38, +0x3c from the ranges +8, +0x10..+0x18, the switches +0x1c..+0x24 and the floors +0x28..+0x30, +0x50 one for all), computed by the LoD copy 0x0050d4e0 (MW3's 0x004f9840, cls_util.cpp copy_lod); the port leaves them zero, which is what every shipped tree computes (docs/pm-map.md, the node copies' Left): only a mod's LoD with those fields set would differ
- **0x005168e0** (1 functions, 113 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x004fe8e0, whose class PM's changes keep: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x0051b340** (1 functions, 17 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): pm only, traced: PM's damage handler on an animation (0x0051b2c0, MW3's 0x00502df0, anim_on_collide zeff_anim_run.cpp:832) first runs 0x0051b340 on every hit: when the def has its second sequence (+0xd4, with events) it stamps the time ([0x008d3f68]), clears +0xb0 and runs that sequence from its start (0x0051fc00, 0x00520800: MW3's 0x00506f40, 0x005079b0); the port reads the sequence (AnimDef::pm_seq2, pm/gamez/zeffect/zeff_anim_init.cpp:126) but never runs it, and no shipped PM animation has one, so only a mod's would show it
- **0x00520500** (1 functions, 100 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x005076c0, whose class PM's changes keep: anim event 37, the fbfx screen wave (0x005076c0 -> 0x005a6760 -> 0x005a6650 / 0x005a5400: up to four 2D waves): the port's hook only logs (app_mission.cpp:2108); no shipped anim.zbd has event 37 (anim_walk: c1..t1 have 0; 38 also 0)
- **0x00520960** (1 functions, 11 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00507dd0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x00526da0** (1 functions, 28 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x0050d610: an effect entry's node and gfx data (zeff_init.cpp:24: no shipped effects file has one): the node made, placed and ticking, its distance test
- **0x00526e50** (1 functions, 43 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x0050d6a0: an effect entry's node and gfx data (zeff_init.cpp:24: no shipped effects file has one): the node made, placed and ticking, its distance test
- **0x00526f60** (1 functions, 15 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x0050d7a0: an effect entry's node and gfx data (zeff_init.cpp:24: no shipped effects file has one): the node made, placed and ticking, its distance test
- **0x00527040** (1 functions, 36 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x0050d880: an effect entry's node and gfx data (zeff_init.cpp:24: no shipped effects file has one): the node made, placed and ticking, its distance test
- **0x00539200** (1 functions, 143 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x0051f750: billboard modes 2 (about x) and 3 (about z): no shipped world uses them; they draw as default models (gmod_draw.cpp:759, render-modeldraw.md:27)
- **0x00540410** (1 functions, 12 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00526aa0: script engine commands for building models and textures at run time (Object3DSetColor, SetShowBackFace, SetMorphVertex, the texture world base and per meter): the engine entries are null in the port (hook_allow: counted) and no shipped script uses them (the census: 0 skipped)
- **0x00540430** (1 functions, 12 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00526ac0: script engine commands for building models and textures at run time (Object3DSetColor, SetShowBackFace, SetMorphVertex, the texture world base and per meter): the engine entries are null in the port (hook_allow: counted) and no shipped script uses them (the census: 0 skipped)
- **0x00548220** (1 functions, 243 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00542060, whose class PM's changes keep: the scripts' texture marks (0x00542060: a hit painted into the hit texture's image) only when a script gives a mark image; none ships (weapon-effects.md); the port logs 0x00542060 if a mod's script turns them on
- **0x00559910** (1 functions, 9 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00539cc0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x0055b630** (1 functions, 113 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x0053b960, whose class PM's changes keep: script engine commands for building models and textures at run time (Object3DSetColor, SetShowBackFace, SetMorphVertex, the texture world base and per meter): the engine entries are null in the port (hook_allow: counted) and no shipped script uses them (the census: 0 skipped)
- **0x0055b810** (1 functions, 21 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x0053bb50: script engine commands for building models and textures at run time (Object3DSetColor, SetShowBackFace, SetMorphVertex, the texture world base and per meter): the engine entries are null in the port (hook_allow: counted) and no shipped script uses them (the census: 0 skipped)
- **0x0055be00** (1 functions, 26 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x0053bd60: script engine commands for building models and textures at run time (Object3DSetColor, SetShowBackFace, SetMorphVertex, the texture world base and per meter): the engine entries are null in the port (hook_allow: counted) and no shipped script uses them (the census: 0 skipped)
- **0x00563760** (1 functions, 157 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00548250, whose class PM's changes keep: the script command MakeShadows (0x005487d0) and the baked shadow builder under it (0x00548250..0x00548650): the script engine's make_shadows entry is null in the port (hook_allow: counted) and no shipped script uses it
- **0x005639e0** (1 functions, 62 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x005484c0: the script command MakeShadows (0x005487d0) and the baked shadow builder under it (0x00548250..0x00548650): the script engine's make_shadows entry is null in the port (hook_allow: counted) and no shipped script uses it
- **0x00563b30** (1 functions, 23 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00548600: the script command MakeShadows (0x005487d0) and the baked shadow builder under it (0x00548250..0x00548650): the script engine's make_shadows entry is null in the port (hook_allow: counted) and no shipped script uses it
- **0x00563b80** (1 functions, 84 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00548650: the script command MakeShadows (0x005487d0) and the baked shadow builder under it (0x00548250..0x00548650): the script engine's make_shadows entry is null in the port (hook_allow: counted) and no shipped script uses it
- **0x00563d20** (1 functions, 23 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x005487d0: the script command MakeShadows (0x005487d0) and the baked shadow builder under it (0x00548250..0x00548650): the script engine's make_shadows entry is null in the port (hook_allow: counted) and no shipped script uses it
- **0x0057a0d0** (1 functions, 13 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x00558120: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x005981f0** (1 functions, 15 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): the script command VideoSetWireFrame, as MW3's 0x00579230 (missing-mod there: the engine entry is null, no shipped script uses it); PM sets the device's fill mode at once (render state 8: 2 wireframe, 3 solid) where MW3 stored a flag
- **0x005b8980** (1 functions, 48 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x005975e0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts
- **0x005c58b0** (1 functions, 15 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x005a5400: anim event 37, the fbfx screen wave (0x005076c0 -> 0x005a6760 -> 0x005a6650 / 0x005a5400: up to four 2D waves): the port's hook only logs (app_mission.cpp:2108); no shipped anim.zbd has event 37 (anim_walk: c1..t1 have 0; 38 also 0)
- **0x005c6b90** (1 functions, 18 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x005a6650: anim event 37, the fbfx screen wave (0x005076c0 -> 0x005a6760 -> 0x005a6650 / 0x005a5400: up to four 2D waves): the port's hook only logs (app_mission.cpp:2108); no shipped anim.zbd has event 37 (anim_walk: c1..t1 have 0; 38 also 0)
- **0x005c6c90** (1 functions, 10 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x005a6760: anim event 37, the fbfx screen wave (0x005076c0 -> 0x005a6760 -> 0x005a6650 / 0x005a5400: up to four 2D waves): the port's hook only logs (app_mission.cpp:2108); no shipped anim.zbd has event 37 (anim_walk: c1..t1 have 0; 38 also 0)
- **0x005c8920** (1 functions, 46 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x005a6a10: Line2D's reader from a dialog .zrd (its COLOR and its end), shared by the line classes' vtables; inferred unused by shipped data (the dialog reader matches the exe on every shipped dialog without it), not checked file by file
- **0x005c9cc0** (1 functions, 20 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x005a7c50: Movie2D's update, draw and erase (a movie playing inside a dialog): no shipped dialog has one (twod.md:674-679)
- **0x005c9d10** (1 functions, 12 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x005a7ca0, whose class PM's changes keep: Movie2D's update, draw and erase (a movie playing inside a dialog): no shipped dialog has one (twod.md:674-679)
- **0x005c9d40** (1 functions, 14 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): as MW3 0x005a7cd0: Movie2D's update, draw and erase (a movie playing inside a dialog): no shipped dialog has one (twod.md:674-679)

## BROKEN (null hook)

Hooks the ported code calls that the program leaves unset (a null field, a census stand-in, or a
base virtual that only logs), which shipped play reaches. "Shown by": the chain runs (the census
of a campaign, Instant Action or training run) or the static check (the data and the callers).

| Hook | Original | Reached by | The original does | In the port | Seen in play | Shown by |
|---|---|---|---|---|---|---|
| DamageCalls.zaps_hud (mech.h) | 0x0043cf50 (MW3 0x0043be70) | the hit wrappers on the player's mech | the HUD static from the hitting weapon's ZAPS_HUD | a census stand-in, as in MW3: nothing shown | yes: PM's weapons.zrd gives ZAPS_HUD to the ER PPC only (0.5 2.0), as MW3's | static (MW3's row) |
| zcls::reread (cls_zbd.cpp:311) on PM's files | 0x00517ac0, 0x00517b30 (MW3 0x004ff730, 0x004ff7a0) | the crater reset (zdec_crater.cpp:362, PM 0x00518b70) from the world history's restore (PM 0x00519020, MW3 0x00500c20) when the record's +0x30 is set | the cut ground nodes read back from the mission's gamez.zbd | returns at once: PM's reader (src/pm/gamez/zclass/cls_zbd.cpp:364) keeps its file list but never sets zbd_path(), and the node re-read is MW3's format; PM re-reads only when exactly one file was read (0x00639c2c == 1), from the first | to check: a restore with craters cut (a reloaded mission) | crater reset |

Run evidence (2026-11-13): the PM chains (campaign kills:99 and standalone-kills:99, PM_AI=1, seed 1999, 33 ms) reached 0 functions outside the known table and skipped 0 script calls, but for one report: the mech build's CZMechFirePoint::CFixup message (mech.cpp line 0xee, rows c1sa3 and c3sa3) goes through the unported() report hook instead of zerr::report. The port's fixup itself runs (mech_build.cpp:699); only the report's routing is missing, with no effect in play.

## PORT (preserve)

### debug or dev tool: PORT (preserve)

- 2 functions, 43 decomp lines (0x0040f780, 0x0040f7a0): as MW3 0x0040ff40, whose class PM's changes keep: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only)
- 2 functions, 42 decomp lines (0x0040f8d0, 0x0040fa20): as MW3 0x0040f150: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only)
- 1 functions, 24 decomp lines (0x00410110): as MW3 0x0040f7f0, whose class PM's changes keep: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only)
- 1 functions, 10 decomp lines (0x00410190): called only from 0x00410110, which are debug
- 1 functions, 10 decomp lines (0x004101c0): as MW3 0x0040f890: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only)
- 1 functions, 10 decomp lines (0x004101e0): as MW3 0x0040f8b0: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only)
- 1 functions, 108 decomp lines (0x00410470): as MW3 0x0040fb50, whose class PM's changes keep: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only)
- 1 functions, 16 decomp lines (0x0041d8f0): as MW3 0x0041ca90: a debug key command (0x6d, case 2 of the key switch 0x0040dba0) toggling the mech's +0x634 bit 1; the release never registers 0x6d (0x0040db60 registers 0x61, 0x6c, 0x70, 0x7e)
- 1 functions, 45 decomp lines (0x0043dbf0): as MW3 0x0043cb10: the HUD's debug overlays (docs/hud.md section 11: the mech's gait, speeds and the like into five debug texts while the debug text shows, command 0x82); hooked in the port (hud_debug, unported)
- 1 functions, 39 decomp lines (0x0043dcf0): as MW3 0x0043cc00: the HUD's debug overlays (docs/hud.md section 11: the mech's gait, speeds and the like into five debug texts while the debug text shows, command 0x82); hooked in the port (hud_debug, unported)
- 1 functions, 31 decomp lines (0x00482240): read: PM's app defaults (MW3 0x0047fb80): no default mech name where MW3 sets 'bushwacker' (app.cpp:72 keeps it), the flags at PM's layout (+0x1a8.., three 1.0 floats); the default mech is reached only by a start straight into a mission (the developers' route), so it shows nowhere in play
- 2 functions, 39 decomp lines (0x004832a0, 0x00483d50): as MW3 0x004810d0, whose class PM's changes keep: the debug console's overlay toggle (0x00608f74; game-loop.md: from 0x00480b20, itself never called)
- 1 functions, 32 decomp lines (0x004a6940): as MW3 0x004a3550: debug key commands of 0x0040dba0, never registered in the release: a position marker printed to the console (0x004a3220), the mechs listed into the developer lines (0x004a33f0, command 0x89; hud.md), a vehicle picked in a dialog (0x004a3550, its destructor 0x004a35e0)
- 1 functions, 20 decomp lines (0x004a69d0): as MW3 0x004a35e0: debug key commands of 0x0040dba0, never registered in the release: a position marker printed to the console (0x004a3220), the mechs listed into the developer lines (0x004a33f0, command 0x89; hud.md), a vehicle picked in a dialog (0x004a3550, its destructor 0x004a35e0)
- 1 functions, 9 decomp lines (0x004a7500): as MW3 0x004a4ca0: the stealth object (the developers' fly camera; back burner in CLAUDE.md): a slot of its
- 1 functions, 9 decomp lines (0x004a7900): as MW3 0x004a50b0: the stealth object (the developers' fly camera; back burner in CLAUDE.md): a slot of its
- 1 functions, 28 decomp lines (0x004a7930): as MW3 0x004a50e0: the debug command 0x7e (0x004abbc0, 121 lines: a console of keyboard text and the network session; registered, but no key reaches it: app_keys.h:14, hud.md) and the stealth object's key handler (0x004a50e0: the fly camera, back burner)
- 1 functions, 11 decomp lines (0x004c7c80): as MW3 0x0058f2d0, whose class PM's changes keep: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 13 decomp lines (0x004c7c90): called only from 0x004c7cf0, which are debug
- 1 functions, 20 decomp lines (0x004c7cf0): called only from 0x004c7c80, which are debug
- 1 functions, 9 decomp lines (0x004f38c0): as MW3 0x004e2310: the valve reader's print method (a vtable slot that fprintf's a value); nothing in the game prints a reader
- 1 functions, 9 decomp lines (0x004f7250): as MW3 0x004e53b0: the valve reader's print method (a vtable slot that fprintf's a value); nothing in the game prints a reader
- 1 functions, 9 decomp lines (0x0050a5f0): as MW3 0x004f6c70: the world's writers (gamez.zbd saved back: the node pools, 0x004fe6c0 -> 0x004fe2d0 -> 0x004fe210; gamez.md) and two debug-only helpers (mechlib's type name for its debug log 0x004fce40; the frame counter for the developer lines 0x004f6c70)
- 1 functions, 35 decomp lines (0x00511180): the node class names for PM's mechlib trace lines (0x00511640 to stderr, 'read node: %s type %s'), its only use; the port leaves the trace lines out (pm/gamez/zclass/cls_mechlib.cpp:15)
- 1 functions, 125 decomp lines (0x00515fb0): as MW3 0x004fe6c0, whose class PM's changes keep: the world's writers (gamez.zbd saved back: the node pools, 0x004fe6c0 -> 0x004fe2d0 -> 0x004fe210; gamez.md) and two debug-only helpers (mechlib's type name for its debug log 0x004fce40; the frame counter for the developer lines 0x004f6c70)
- 1 functions, 189 decomp lines (0x00516260): as MW3 0x004fe2d0, whose class PM's changes keep: the world's writers (gamez.zbd saved back: the node pools, 0x004fe6c0 -> 0x004fe2d0 -> 0x004fe210; gamez.md) and two debug-only helpers (mechlib's type name for its debug log 0x004fce40; the frame counter for the developer lines 0x004f6c70)
- 1 functions, 48 decomp lines (0x00516720): as MW3 0x004fe210, whose class PM's changes keep: the world's writers (gamez.zbd saved back: the node pools, 0x004fe6c0 -> 0x004fe2d0 -> 0x004fe210; gamez.md) and two debug-only helpers (mechlib's type name for its debug log 0x004fce40; the frame counter for the developer lines 0x004f6c70)
- 1 functions, 53 decomp lines (0x00516800): called only from 0x00516260, which are debug
- 1 functions, 11 decomp lines (0x00531810): as MW3 0x00518050, whose class PM's changes keep: the texture directory writer (0x005180a0, 'Error writing texture directory.') and its entry-index helper, part of the world writers (gamez.md:127)
- 1 functions, 53 decomp lines (0x00531870): as MW3 0x005180a0, whose class PM's changes keep: the texture directory writer (0x005180a0, 'Error writing texture directory.') and its entry-index helper, part of the world writers (gamez.md:127)
- 1 functions, 19 decomp lines (0x00533320): read: PM's screenshot grab (the back buffer grabbed through 0x00598600(2), the pixel format's masks and shifts taken, converted by 0x005333e0), from 0x004af600 (not a Ghidra function: mw3%08x.bmp, the first free name), the handler of command 0x86 (MW3's 0x7e plus 8; 0x0040b9f0 registers it); no key reaches it (docs/controls.md, docs/editor.md), as its siblings 0x00533380..0x00533580
- 1 functions, 26 decomp lines (0x00533380): as MW3 0x005199e0: the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it
- 1 functions, 12 decomp lines (0x005333c0): as MW3 0x00519a20: the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it
- 1 functions, 50 decomp lines (0x005333e0): as MW3 0x00519a40: the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it
- 1 functions, 63 decomp lines (0x005334d0): as MW3 0x00519b30: the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it
- 1 functions, 19 decomp lines (0x00533580): as MW3 0x00519be0: the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it
- 1 functions, 24 decomp lines (0x005335c0): as MW3 0x00519c20: the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it
- 1 functions, 137 decomp lines (0x00555ae0): as MW3 0x005360c0, whose class PM's changes keep: the world's writers (gamez.md: 0x004fe8e0's materials 0x005360c0 and models 0x005380e0, saving a world back to a .zbd): a dev tool
- 1 functions, 20 decomp lines (0x00557b30): called only from 0x00515fb0, which are debug
- 1 functions, 302 decomp lines (0x00557d50): as MW3 0x005380e0, whose class PM's changes keep: the world's writers (gamez.md: 0x004fe8e0's materials 0x005360c0 and models 0x005380e0, saving a world back to a .zbd): a dev tool
- 1 functions, 224 decomp lines (0x00596590): as MW3 0x00575b00: the developer's stdout redirect (freopen to a log; editor.md:214)
- 1 functions, 194 decomp lines (0x005ad3d0): group A, read: the network debug view (as MW3's 0x0058ba40, PM's helpers)
- 1 functions, 34 decomp lines (0x005adf40): as MW3 0x0058c590, whose class PM's changes keep: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 89 decomp lines (0x005aecb0): as MW3 0x0058d3c0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 14 decomp lines (0x005afc50): as MW3 0x0058e440: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 26 decomp lines (0x005b0300): as MW3 0x0058ec20: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 19 decomp lines (0x005b0360): as MW3 0x0058ec90: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 11 decomp lines (0x005b03a0): as MW3 0x0058ecd0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 19 decomp lines (0x005b03d0): as MW3 0x0058ed00: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 28 decomp lines (0x005b0410): as MW3 0x0058ed40: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 24 decomp lines (0x005b0470): as MW3 0x0058eda0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 20 decomp lines (0x005b04d0): as MW3 0x0058ee10: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 138 decomp lines (0x005b0500): as MW3 0x0058ee40, whose class PM's changes keep: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 60 decomp lines (0x005b08b0): as MW3 0x0058f170: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 11 decomp lines (0x005b09c0): as MW3 0x0058f270: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 77 decomp lines (0x005b0a30): as MW3 0x0058f420: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 37 decomp lines (0x005b0b90): as MW3 0x0058f580: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 78 decomp lines (0x005b0c60): as MW3 0x0058f2e0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 33 decomp lines (0x005b0dc0): as MW3 0x0058f630: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 10 decomp lines (0x005b0e30): as MW3 0x0058f6a0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 11 decomp lines (0x005b0ed0): as MW3 0x0058f740: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 11 decomp lines (0x005b0f00): as MW3 0x0058f770: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 11 decomp lines (0x005b0f10): as MW3 0x0058f780: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 11 decomp lines (0x005b0f20): as MW3 0x0058f790: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 11 decomp lines (0x005b0f30): as MW3 0x0058f7a0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 11 decomp lines (0x005b0f40): as MW3 0x0058f7b0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 8 decomp lines (0x005b1040): as MW3 0x0058f8b0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 1 functions, 16 decomp lines (0x005b2c60): the script command PrintChildren (PM only): the current node's children's names to the console ('  %s'); no shipped PM script uses it, the port's interpreter does not know it
- 1 functions, 26 decomp lines (0x005b8210): as MW3 0x005960a0: the hidden GameZ Interpreter console (0x005960a0: a 2D console window that runs typed script lines through 0x00595510; its destructor and line runner; toggled by 0x004810d0, which nothing reaches: interp.md:532, discoveries.md:1532)
- 1 functions, 12 decomp lines (0x005b82e0): as MW3 0x00596170: the hidden GameZ Interpreter console (0x005960a0: a 2D console window that runs typed script lines through 0x00595510; its destructor and line runner; toggled by 0x004810d0, which nothing reaches: interp.md:532, discoveries.md:1532)
- 1 functions, 14 decomp lines (0x005b8300): as MW3 0x00596190: the hidden GameZ Interpreter console (0x005960a0: a 2D console window that runs typed script lines through 0x00595510; its destructor and line runner; toggled by 0x004810d0, which nothing reaches: interp.md:532, discoveries.md:1532)
- 1 functions, 9 decomp lines (0x005b8350): called only from 0x005b8300, which are debug
- 1 functions, 19 decomp lines (0x005c02c0): as MW3 0x005a0050: the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683)
- 2 functions, 54 decomp lines (0x005c6df0, 0x005c6e80): as MW3 0x005a3cd0, whose class PM's changes keep: the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683)
- 1 functions, 16 decomp lines (0x005c6f90): as MW3 0x005a3de0, whose class PM's changes keep: the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683)
- 1 functions, 17 decomp lines (0x005c6fe0): as MW3 0x005a3e30, whose class PM's changes keep: the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683)
- 1 functions, 68 decomp lines (0x005c7030): as MW3 0x005a3e80, whose class PM's changes keep: the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683)
- 1 functions, 17 decomp lines (0x005cc5b0): as MW3 0x005a98d0: the developers' loose <name>.zan override for motion scripts (motion.md:36), like the reader's loose .zrd files
- 1 functions, 29 decomp lines (0x005cc880): as MW3 0x005a9bc0: the motion script's text side: the loose .zan reader (0x005a9e10, with 0x005aa210) behind the developers' override, and the writer (0x005a9bc0)
- 1 functions, 168 decomp lines (0x005ccaf0): as MW3 0x005a9e10: the motion script's text side: the loose .zan reader (0x005a9e10, with 0x005aa210) behind the developers' override, and the writer (0x005a9bc0)
- 1 functions, 19 decomp lines (0x005ccef0): as MW3 0x005aa210: the motion script's text side: the loose .zan reader (0x005a9e10, with 0x005aa210) behind the developers' override, and the writer (0x005a9bc0)

### dev dialog (MFC), its logic: PORT (preserve)

- 1 functions, 83 decomp lines (0x00403830): as MW3 0x00403820, whose class PM's changes keep: the editor's 'add vehicle to node' (0x00403820 -> 0x0049d350, editor.md); ours does it as meant: upgrades/editor/add_vehicle.cpp
- 1 functions, 56 decomp lines (0x00403b10): group A, read: the net editor's line delete (as MW3's 0x00403ae0, its 'select first' report dropped)
- 1 functions, 75 decomp lines (0x00403f30): as MW3 0x00403ec0, whose class PM's changes keep: the AI tester dialog's walk over a vehicle's nets (0x00404090 -> 0x00403ec0: their extents)
- 2 functions, 27 decomp lines (0x00404100, 0x004a0530): as MW3 0x00404090: the AI tester dialog's walk over a vehicle's nets (0x00404090 -> 0x00403ec0: their extents)
- 1 functions, 71 decomp lines (0x00409830): as MW3 0x0040bb00: the camera dialog (resource 0x6a: the camera's place and angles in degrees, editable; 0x0040bd00 its window): a dev tool's logic
- 1 functions, 34 decomp lines (0x00433770): as MW3 0x00432c60: a mech list dialog (resource 0x9c, from a debug key command in 0x0040db60): its fill of every mech name (0x00484650)
- 1 functions, 26 decomp lines (0x00433840): as MW3 0x00432d30: a mech list dialog (resource 0x9c, from a debug key command in 0x0040db60): its fill of every mech name (0x00484650)
- 1 functions, 45 decomp lines (0x00485b20): as MW3 0x00483170: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags)
- 1 functions, 14 decomp lines (0x00485bd0): as MW3 0x00483230: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags)
- 1 functions, 12 decomp lines (0x00485c20): as MW3 0x004832e0: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags)
- 1 functions, 9 decomp lines (0x00485d10): as MW3 0x004833f0: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags)
- 1 functions, 12 decomp lines (0x00485d20): as MW3 0x00483400: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags)
- 1 functions, 14 decomp lines (0x00485d30): as MW3 0x00483410: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags)
- 1 functions, 21 decomp lines (0x00485d60): as MW3 0x004834b0: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags)
- 1 functions, 10 decomp lines (0x00485e60): as MW3 0x00483600: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags)
- 1 functions, 32 decomp lines (0x00485ea0): as MW3 0x00483640: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags)
- 1 functions, 131 decomp lines (0x004a07a0): as MW3 0x0049d350, whose class PM's changes keep: the editor's 'add vehicle to node' (0x00403820 -> 0x0049d350, editor.md); ours does it as meant: upgrades/editor/add_vehicle.cpp
- 1 functions, 26 decomp lines (0x004a0b00): as MW3 0x0049d710: the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians
- 1 functions, 19 decomp lines (0x004a0ba0): as MW3 0x0049d7b0: the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians
- 1 functions, 26 decomp lines (0x004a0c00): as MW3 0x0049d810: the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians
- 1 functions, 24 decomp lines (0x004a0cb0): as MW3 0x0049d8d0: the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians
- 1 functions, 24 decomp lines (0x004a0d00): as MW3 0x0049d920: the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians
- 1 functions, 25 decomp lines (0x004a0d50): as MW3 0x0049d970: the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians
- 1 functions, 11 decomp lines (0x004a0da0): as MW3 0x0049d9e0: the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians
- 1 functions, 30 decomp lines (0x004b2ec0): as MW3 0x004af000: a vehicle-pick dev dialog (resource 0x8f, from the debug key's 0x004a3550): its list of zEntity's vehicles
- 1 functions, 9 decomp lines (0x004c17d0): as MW3 0x004bb2e0: the vehicle editor's list fills (each valve, each region by name into a combo box); covered in the port by game_editor_vehicles 0x004bb300 / 0x004bb620 (valve and region lists)
- 1 functions, 12 decomp lines (0x004c1ac0): as MW3 0x004bb5f0: the vehicle editor's list fills (each valve, each region by name into a combo box); covered in the port by game_editor_vehicles 0x004bb300 / 0x004bb620 (valve and region lists)
- 1 functions, 22 decomp lines (0x004c3830): as MW3 0x004bc9f0: the AI tester dialog (0x004b7d80..0x004bcb70, docs/ai.md: a developer tool over a vehicle's AI and net): its helpers' logic
- 1 functions, 19 decomp lines (0x004c39b0): as MW3 0x004bcb70: the AI tester dialog (0x004b7d80..0x004bcb70, docs/ai.md: a developer tool over a vehicle's AI and net): its helpers' logic
- 1 functions, 17 decomp lines (0x004eac90): as MW3 0x004d9d80: an AI parameters dialog (MFC): what it reads into its list (the net's records) and positions; preserve as logic
- 1 functions, 44 decomp lines (0x004eacd0): as MW3 0x004d9dc0: an AI parameters dialog (MFC): what it reads into its list (the net's records) and positions; preserve as logic
- 1 functions, 27 decomp lines (0x004eade0): as MW3 0x004d9ed0: an AI parameters dialog (MFC): what it reads into its list (the net's records) and positions; preserve as logic
- 1 functions, 32 decomp lines (0x004eb070): as MW3 0x004d9fc0, whose class PM's changes keep: an AI parameters dialog (MFC): what it reads into its list (the net's records) and positions; preserve as logic
- 1 functions, 19 decomp lines (0x004f07a0): as MW3 0x004df6a0: the editor's 'add vehicle to node' record (an aiv record built on OK, editor.md); in the release its path returns 0 and nothing is made; ours does it as meant: upgrades/editor/add_vehicle

### unused: PORT (preserve)

- 1 functions, 17 decomp lines (0x00410820): as MW3 0x0040fef0: the timing marks ('Dummy Frame', 'Lets Play!'): act only with [0x005e14f0], 0 in the data (program.md, no effect)
- 1 functions, 9 decomp lines (0x00410880): called only from 0x00410820, which are unused
- 1 functions, 10 decomp lines (0x0048b510): fd0 read: the list box's arrows as MW3's 0x0048c910 plus two PM bytes that keep UP (+0x1e0) and DOWN (+0x1e1) enabled; the pm port has the arrows without them (menu_widgets.cpp:1264-1330). Only 0x0048b510 sets them, from the pilot screen's training case (0x004916c0 case 4), which PM's menu doesn't reach (pm-mech-step.md:472), so no shipped path shows it
- 1 functions, 28 decomp lines (0x004964f0): as MW3 0x00493120: the movie base's run to the end on its own clock (0x00575d30): nothing calls it (fmv.md)
- 1 functions, 76 decomp lines (0x004b5040): as MW3 0x004b1170: the static the AI's planner builds at 0x0060b240 (two reserved vectors): read by nothing (weapon_planner.h:7)
- 1 functions, 9 decomp lines (0x004b51a0): called only from 0x004b5040, which are unused
- 1 functions, 21 decomp lines (0x004c8010): as MW3 0x004c0ed0: the vehicle's +0xd8 record of each in/out query (its constructor and reset): written, never read (docs/symbols.csv: not kept; ai.md 11)
- 1 functions, 12 decomp lines (0x004d6a00): as MW3 0x004cd7b0: the brain's own step at +0x240 (0x004cd770, vtable 0x005cc290): built by the brain's constructor, but nothing orders it (only the constructor takes +0x240), so its make_state, state and run (path following toward a point, 98 lines) never run
- 1 functions, 10 decomp lines (0x004d6a50): as MW3 0x004cd800: the brain's own step at +0x240 (0x004cd770, vtable 0x005cc290): built by the brain's constructor, but nothing orders it (only the constructor takes +0x240), so its make_state, state and run (path following toward a point, 98 lines) never run
- 1 functions, 90 decomp lines (0x004d6a60): as MW3 0x004cd810, whose class PM's changes keep: the brain's own step at +0x240 (0x004cd770, vtable 0x005cc290): built by the brain's constructor, but nothing orders it (only the constructor takes +0x240), so its make_state, state and run (path following toward a point, 98 lines) never run
- 2 functions, 50 decomp lines (0x004f9580, 0x004faa40): as MW3 0x004e95b0: IControls vt+0x84 of the AI controls (drop the target if it is the given entity's, stand the firing down): no caller reaches that slot (docs/ai.md, the AI controls' unreached slots)
- 1 functions, 11 decomp lines (0x00506960): as MW3 0x004f3540: no effect in the original (app_engine.cpp's list): a camera word nothing reads (0x004f3540), zClass's start defaults the port's initialisers already hold (0x004f8f20), a zSave registration with flag 0 (0x004f8f60)
- 1 functions, 10 decomp lines (0x0050ca50): as MW3 0x004f8f20: no effect in the original (app_engine.cpp's list): a camera word nothing reads (0x004f3540), zClass's start defaults the port's initialisers already hold (0x004f8f20), a zSave registration with flag 0 (0x004f8f60)
- 1 functions, 9 decomp lines (0x0050ca90): as MW3 0x004f8f60: no effect in the original (app_engine.cpp's list): a camera word nothing reads (0x004f3540), zClass's start defaults the port's initialisers already hold (0x004f8f20), a zSave registration with flag 0 (0x004f8f60)
- 1 functions, 23 decomp lines (0x0050cc10): the script command CreateUniqueMaterials (PM only, args recursive / polygon): each node's model through 0x0055bcf0, its children too when recursive; no shipped PM script uses it (interp.zbd decoded) and the port's interpreter does not know it
- 1 functions, 17 decomp lines (0x00510710): as MW3 0x004fc4d0: mechlib's material writer at close (0x004fc990 -> 0x004fcca0 -> 0x004fc4d0): dead (mechlib.md)
- 1 functions, 13 decomp lines (0x00510c40): as MW3 0x004fc990: mechlib's material writer at close (0x004fc990 -> 0x004fcca0 -> 0x004fc4d0): dead (mechlib.md)
- 1 functions, 33 decomp lines (0x00510f40): as MW3 0x004fcca0: mechlib's material writer at close (0x004fc990 -> 0x004fcca0 -> 0x004fc4d0): dead (mechlib.md)
- 1 functions, 23 decomp lines (0x00533660): as MW3 0x00519df0: zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4)
- 1 functions, 55 decomp lines (0x005336c0): as MW3 0x00519e50: zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4)
- 1 functions, 20 decomp lines (0x005337f0): as MW3 0x00519f70: zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4)
- 1 functions, 50 decomp lines (0x00533820): as MW3 0x00519fa0: zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4)
- 1 functions, 28 decomp lines (0x00533900): as MW3 0x0051a080: zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4)
- 1 functions, 49 decomp lines (0x00533960): as MW3 0x0051a0d0: zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4)
- 1 functions, 36 decomp lines (0x005347b0): as MW3 0x0051af90: the input module's static object's list walk, whose caller 0x0051af70 nothing reaches (input.md:567)
- 1 functions, 27 decomp lines (0x00537bc0): as MW3 0x0051e160: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 150 decomp lines (0x0053a730): as MW3 0x00520c60: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 166 decomp lines (0x0053ab50): as MW3 0x00521080: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 70 decomp lines (0x0053afc0): as MW3 0x005214f0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 62 decomp lines (0x0053b560): as MW3 0x00521a90: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 87 decomp lines (0x0053b8d0): as MW3 0x00521e10: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 53 decomp lines (0x0053bd40): as MW3 0x00522280: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 16 decomp lines (0x0053be80): as MW3 0x005223c0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 50 decomp lines (0x0053bfa0): as MW3 0x005224b0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 115 decomp lines (0x0053d000): as MW3 0x00523650, whose class PM's changes keep: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 32 decomp lines (0x0053d7c0): as MW3 0x00523df0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 31 decomp lines (0x0053d8c0): as MW3 0x00523ef0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 21 decomp lines (0x0053e8c0): as MW3 0x00524f80: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 17 decomp lines (0x0053e910): as MW3 0x00524fd0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 21 decomp lines (0x0053e960): as MW3 0x00525020: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 9 decomp lines (0x0053e9e0): as MW3 0x005250a0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 9 decomp lines (0x0053ea30): as MW3 0x005250f0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 144 decomp lines (0x0053f900): as MW3 0x00525fa0, whose class PM's changes keep: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 1 functions, 101 decomp lines (0x00540a90): as MW3 0x0053d130: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers
- 1 functions, 124 decomp lines (0x00540ca0): as MW3 0x0053d360: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers
- 1 functions, 44 decomp lines (0x00541830): as MW3 0x0053de50: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers
- 1 functions, 65 decomp lines (0x005418e0): as MW3 0x0053df00: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers
- 1 functions, 779 decomp lines (0x0054aea0): as MW3 0x00530d90, whose class PM's changes keep: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers
- 1 functions, 945 decomp lines (0x0054be20): as MW3 0x00531d90, whose class PM's changes keep: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers
- 1 functions, 1153 decomp lines (0x0054d1c0): as MW3 0x005331e0, whose class PM's changes keep: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers
- 1 functions, 97 decomp lines (0x00559db0): as MW3 0x0053a1a0, whose class PM's changes keep: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers
- 1 functions, 72 decomp lines (0x0055a2f0): as MW3 0x0053a740: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers
- 1 functions, 168 decomp lines (0x0055ad60): as MW3 0x0053b050, whose class PM's changes keep: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers
- 1 functions, 50 decomp lines (0x00563da0): as MW3 0x00548840, whose class PM's changes keep: the puffers' wind step with a dt argument: dead code, never called (puffers.md)
- 1 functions, 16 decomp lines (0x00579360): as MW3 0x00557490: the defines list's constructor and destructor, no caller reaches them (zreader.md:103)
- 1 functions, 39 decomp lines (0x00591ce0): as MW3 0x00571380, whose class PM's changes keep: the Aureal A3D path: its init (CoCreateInstance), 3D placement and error names; only 0x570625 writes the selector [0x72c2cc], and only to 0 (DirectSound) when A3D fails, so A3D never runs (sound.md:38)
- 1 functions, 45 decomp lines (0x00592cc0): as MW3 0x00571f70: the Aureal A3D path: its init (CoCreateInstance), 3D placement and error names; only 0x570625 writes the selector [0x72c2cc], and only to 0 (DirectSound) when A3D fails, so A3D never runs (sound.md:38)
- 1 functions, 197 decomp lines (0x00595360): as MW3 0x005746b0, whose class PM's changes keep: the Aureal A3D path: its init (CoCreateInstance), 3D placement and error names; only 0x570625 writes the selector [0x72c2cc], and only to 0 (DirectSound) when A3D fails, so A3D never runs (sound.md:38)
- 1 functions, 19 decomp lines (0x005acc40): called only from 0x00410880, which are unused
- 1 functions, 9 decomp lines (0x005cc130): as MW3 0x005a92d0: _rmtmp at zSave's stop, unreached

## UNSURE

- 0x0041e010 (1 functions, 9 decomp lines): as MW3 0x0041d220: the mech's (and another class's, vtables 0x005bcf80.., 0x005c3590..) speed and motion queries for the AI, vt+0x124..+0x13c and two part queries over 0x00465bc0 / 0x00465c70: the port answers vt+0x118 and +0x120 (app_mech.h:445); whether the AI asks these others in play is to settle
- 0x0041e070 (1 functions, 13 decomp lines): as MW3 0x0041d280: the mech's (and another class's, vtables 0x005bcf80.., 0x005c3590..) speed and motion queries for the AI, vt+0x124..+0x13c and two part queries over 0x00465bc0 / 0x00465c70: the port answers vt+0x118 and +0x120 (app_mech.h:445); whether the AI asks these others in play is to settle
- 0x0041e0b0 (1 functions, 19 decomp lines): as MW3 0x0041d2c0: the mech's (and another class's, vtables 0x005bcf80.., 0x005c3590..) speed and motion queries for the AI, vt+0x124..+0x13c and two part queries over 0x00465bc0 / 0x00465c70: the port answers vt+0x118 and +0x120 (app_mech.h:445); whether the AI asks these others in play is to settle
- 0x0041e0f0 (1 functions, 9 decomp lines): as MW3 0x0041d300: the mech's (and another class's, vtables 0x005bcf80.., 0x005c3590..) speed and motion queries for the AI, vt+0x124..+0x13c and two part queries over 0x00465bc0 / 0x00465c70: the port answers vt+0x118 and +0x120 (app_mech.h:445); whether the AI asks these others in play is to settle
- 0x0041e180 (1 functions, 23 decomp lines): as MW3 0x0041d380: the mech's (and another class's, vtables 0x005bcf80.., 0x005c3590..) speed and motion queries for the AI, vt+0x124..+0x13c and two part queries over 0x00465bc0 / 0x00465c70: the port answers vt+0x118 and +0x120 (app_mech.h:445); whether the AI asks these others in play is to settle
- 0x0041ed00 (1 functions, 52 decomp lines): as MW3 0x0041dd90: the mech's (and another class's, vtables 0x005bcf80.., 0x005c3590..) speed and motion queries for the AI, vt+0x124..+0x13c and two part queries over 0x00465bc0 / 0x00465c70: the port answers vt+0x118 and +0x120 (app_mech.h:445); whether the AI asks these others in play is to settle
- 0x004d4000 (1 functions, 12 decomp lines): as MW3 0x004c9140: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots
- 0x004d4080 (1 functions, 55 decomp lines): as MW3 0x004c9180: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots
- 0x004d4170 (1 functions, 20 decomp lines): as MW3 0x004c9260: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots
- 0x004d41c0 (1 functions, 36 decomp lines): as MW3 0x004c92b0: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots
- 0x004d4220 (1 functions, 21 decomp lines): as MW3 0x004c9310: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots
- 0x004d4270 (1 functions, 36 decomp lines): as MW3 0x004c9360: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots
- 0x004d4300 (1 functions, 24 decomp lines): as MW3 0x004c9410: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots
- 0x004d4380 (1 functions, 11 decomp lines): as MW3 0x004c9480: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots
- 0x004d4650 (1 functions, 21 decomp lines): as MW3 0x004c9d20: Step's vt+0x30..+0x44 (sets, compares or offers the step's next from a picked node's behaviour, vt+0x60/+0x74): probably the AI net editor's linking; to settle by finding who calls these slots
- 0x004d4690 (1 functions, 20 decomp lines): as MW3 0x004c9d70: Step's vt+0x30..+0x44 (sets, compares or offers the step's next from a picked node's behaviour, vt+0x60/+0x74): probably the AI net editor's linking; to settle by finding who calls these slots
- 0x004d46d0 (1 functions, 14 decomp lines): as MW3 0x004c9db0: Step's vt+0x30..+0x44 (sets, compares or offers the step's next from a picked node's behaviour, vt+0x60/+0x74): probably the AI net editor's linking; to settle by finding who calls these slots
- 0x004d4700 (1 functions, 14 decomp lines): as MW3 0x004c9de0: Step's vt+0x30..+0x44 (sets, compares or offers the step's next from a picked node's behaviour, vt+0x60/+0x74): probably the AI net editor's linking; to settle by finding who calls these slots
- 0x004e75d0 (1 functions, 12 decomp lines): as MW3 0x004d7160: an entity vtable slot (0x005bcf64 and two more vtables, beside 0x004d7270): when on, tints the object (blend weight 0.6, 0x004fb8c0) and switches its blend use (0x004fb840); not found in the port; to settle which slot it is and who calls it
- 0x004eff90 (1 functions, 58 decomp lines): as MW3 0x004dee50: the aiv reader's vtable slot 0x005cd42c (58 lines): a name checked against zEntity's list (0x005a8210) and the reader's records (std::map find 0x004e1a30), no caller found; probably the editor's 'is this name free'; to settle by its slot and callers
- 0x0053fe30 (1 functions, 87 decomp lines): read with group A: a callee of shared ports PM changed (0x00482ca0); whether the port does PM's version, this call included, is settled there
- 0x00594600 (1 functions, 198 decomp lines): read with group A: a callee of shared ports PM changed (0x005941c0); whether the port does PM's version, this call included, is settled there
- 0x00596520 (1 functions, 41 decomp lines): read with group A: a callee of shared ports PM changed (0x0043bb70, 0x004fd6f0, 0x00505c80); whether the port does PM's version, this call included, is settled there
- 0x005969a0 (1 functions, 100 decomp lines): read with group A: a callee of shared ports PM changed (0x00482090); whether the port does PM's version, this call included, is settled there
- 0x005aa410 (1 functions, 43 decomp lines): read with group A: a callee of shared ports PM changed (0x0049faf0, 0x0049fed0); whether the port does PM's version, this call included, is settled there
- 0x005aa4e0 (1 functions, 14 decomp lines): read with group A: a callee of shared ports PM changed (0x0046f910, 0x005aa470); whether the port does PM's version, this call included, is settled there
- 0x005ac170 (1 functions, 280 decomp lines): read with group A: a callee of shared ports PM changed (0x005a9220, 0x005a9620, 0x005a9980); whether the port does PM's version, this call included, is settled there
- 0x005acba0 (1 functions, 21 decomp lines): read with group A: a callee of shared ports PM changed (0x004a0e10, 0x004a2dc0, 0x004dd1e0); whether the port does PM's version, this call included, is settled there
- 0x005b09f0 (1 functions, 16 decomp lines): read with group A: a callee of shared ports PM changed (0x00592910, 0x005a9220, 0x005a9980); whether the port does PM's version, this call included, is settled there
- 0x005b1050 (1 functions, 57 decomp lines): read with group A: a callee of shared ports PM changed (0x00482140); whether the port does PM's version, this call included, is settled there
- 0x005b14e0 (1 functions, 25 decomp lines): read with group A: a callee of shared ports PM changed (0x005c9280); whether the port does PM's version, this call included, is settled there
- 0x005b2050 (1 functions, 74 decomp lines): read with group A: a callee of shared ports PM changed (0x005b19e0, 0x005b1bd0, 0x005b1db0); whether the port does PM's version, this call included, is settled there
- 0x005cb4b0 (1 functions, 11 decomp lines): read with group A: a callee of shared ports PM changed (0x00494b80); whether the port does PM's version, this call included, is settled there

## By module

### mech3ep1 (3423 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00401000 | 36 | MISSING LOGIC (only data the shipped game lacks reaches it) | called only from 0x004b1f90, which are missing-mod |
| 0x004010c0 | 132 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00401100: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00401430 | 44 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00401400: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00401530 | 70 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00401500: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00401660 | 36 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00401620: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x004016d0 | 107 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00401680: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00401990 | 58 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00401940: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00401b00 | 75 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00401ab0, whose class PM's changes keep: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00401c50 | 18 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00401bf0: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00401c90 | 110 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00401c30: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00401e90 | 48 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00401e30: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00401fb0 | 46 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00401f50: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00402100 | 14 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:1166) |
| 0x00402140 | 21 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00402190: the version-105 AI nets: the older net class (0x004021c0, 0x5c bytes, vtable 0x005bb8d0) and its reader and builder (0x004025b0, ~1,800 decompiled lines in all) with their helpers. Not ported on purpose (docs/ai.md 17.9): no shipped net file is version 105; a hand-made 105 file would reach them |
| 0x00402170 | 36 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004021c0: the version-105 AI nets: the older net class (0x004021c0, 0x5c bytes, vtable 0x005bb8d0) and its reader and builder (0x004025b0, ~1,800 decompiled lines in all) with their helpers. Not ported on purpose (docs/ai.md 17.9): no shipped net file is version 105; a hand-made 105 file would reach them |
| 0x00402210 | 18 | unlabelled |  |
| 0x00402230 | 11 | folded | covered by the port where it names it (src/gamez/znetwork/znet_object.h:32) |
| 0x00402250 | 8 | unlabelled |  |
| 0x00402280 | 12 | unlabelled |  |
| 0x00402330 | 18 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004022c0: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00402350 | 15 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004022e0: the version-105 AI nets: the older net class (0x004021c0, 0x5c bytes, vtable 0x005bb8d0) and its reader and builder (0x004025b0, ~1,800 decompiled lines in all) with their helpers. Not ported on purpose (docs/ai.md 17.9): no shipped net file is version 105; a hand-made 105 file would reach them |
| 0x00402370 | 51 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00402300: the version-105 AI nets: the older net class (0x004021c0, 0x5c bytes, vtable 0x005bb8d0) and its reader and builder (0x004025b0, ~1,800 decompiled lines in all) with their helpers. Not ported on purpose (docs/ai.md 17.9): no shipped net file is version 105; a hand-made 105 file would reach them |
| 0x00402440 | 34 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004023d0: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00402500 | 24 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004024a0: the version-105 AI nets: the older net class (0x004021c0, 0x5c bytes, vtable 0x005bb8d0) and its reader and builder (0x004025b0, ~1,800 decompiled lines in all) with their helpers. Not ported on purpose (docs/ai.md 17.9): no shipped net file is version 105; a hand-made 105 file would reach them |
| 0x00402540 | 20 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004024e0: the version-105 AI nets: the older net class (0x004021c0, 0x5c bytes, vtable 0x005bb8d0) and its reader and builder (0x004025b0, ~1,800 decompiled lines in all) with their helpers. Not ported on purpose (docs/ai.md 17.9): no shipped net file is version 105; a hand-made 105 file would reach them |
| 0x00402570 | 23 | folded | as MW3 0x00402510: covered: array and list helpers, destructors (unwind callers), the loading bar and the briefing's map (app_menus.cpp:964 overrides briefing_map_image; the hook's 'not ported yet' comment is stale), the vehicle path's entries drop (app_vehicle.h:145) |
| 0x004025c0 | 17 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:1714) |
| 0x004025f0 | 9 | MISSING LOGIC (only data the shipped game lacks reaches it) | called only from 0x004c5cb0, which are missing-mod |
| 0x00402610 | 272 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004025b0: the version-105 AI nets: the older net class (0x004021c0, 0x5c bytes, vtable 0x005bb8d0) and its reader and builder (0x004025b0, ~1,800 decompiled lines in all) with their helpers. Not ported on purpose (docs/ai.md 17.9): no shipped net file is version 105; a hand-made 105 file would reach them |
| 0x00402bd0 | 32 | folded | covered by the port where it names it (src/mech3/game_editor_tree.cpp:77) |
| 0x00402c50 | 9 | folded | frees the object (a destructor's release) |
| 0x00402ec0 | 12 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:311) |
| 0x00402f10 | 8 | folded | returns a constant or a global |
| 0x00402f20 | 12 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:303) |
| 0x00403170 | 9 | folded | a thunk: one call to another function |
| 0x00403730 | 23 | folded | covered by the port where it names it (src/upgrades/editor/node_params.h:16) |
| 0x00403830 | 83 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x00403820, whose class PM's changes keep: the editor's 'add vehicle to node' (0x00403820 -> 0x0049d350, editor.md); ours does it as meant: upgrades/editor/add_vehicle.cpp |
| 0x00403a70 | 20 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x00403a40: MFC dialog plumbing of dev dialogs (CDialog constructors and destructors, EnableWindow): resource 0x67 and the teleport and net dialogs' destructors |
| 0x00403ac0 | 8 | folded | empty: does nothing |
| 0x00403ad0 | 17 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:357) |
| 0x00403b10 | 56 | dev dialog (MFC), its logic: PORT (preserve) | group A, read: the net editor's line delete (as MW3's 0x00403ae0, its 'select first' report dropped) |
| 0x00403c50 | 41 | folded | as MW3 0x00403c00: covered where the port does it: src/mech3/zainetedit.cpp:107 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00403d30 | 27 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00403cd0: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00403d70 | 30 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00403d10: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00403dd0 | 21 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00403d60: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00403e30 | 21 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00403db0: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00403e70 | 16 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00403df0: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00403ea0 | 30 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00403e20: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00403ef0 | 19 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00403e70: the basic vehicle's own controls (a vehicle made without AI controls: 0x004ac860(0xa70), 0x004ae270, vtables 0x005c8c50 / IControls 0x005c8b98): its path follower 0x00401050 (mode 0 follows the waypoints at +0xa0c; modes 1..3 other approaches; 4 stops) and its vtable's methods. Deferred (docs/vehicles.md, The basic vehicle's own controls): no shipped mission makes such a vehicle (48 missions checked by a log line kept for community missions); a mod's mission would get a vehicle that does not move |
| 0x00403f30 | 75 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x00403ec0, whose class PM's changes keep: the AI tester dialog's walk over a vehicle's nets (0x00404090 -> 0x00403ec0: their extents) |
| 0x00404100 | 18 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x00404090: the AI tester dialog's walk over a vehicle's nets (0x00404090 -> 0x00403ec0: their extents) |
| 0x00404140 | 9 | folded | covered by the port where it names it (src/mech3/entity_bind.h:2) |
| 0x00404160 | 9 | folded | a thunk: one call to another function |
| 0x00404170 | 9 | folded | a thunk: one call to another function |
| 0x004041a0 | 53 | folded | as MW3 0x00404140: covered where the port does it: src/mech3/game_editor_net.cpp:29 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00404260 | 24 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:604) |
| 0x00404310 | 19 | folded | as MW3 0x004042f0: covered: array and list helpers, destructors (unwind callers), the loading bar and the briefing's map (app_menus.cpp:964 overrides briefing_map_image; the hook's 'not ported yet' comment is stale), the vehicle path's entries drop (app_vehicle.h:145) |
| 0x00404340 | 12 | unlabelled |  |
| 0x00404360 | 32 | folded | as MW3 0x00404340: covered: array and list helpers, destructors (unwind callers), the loading bar and the briefing's map (app_menus.cpp:964 overrides briefing_map_image; the hook's 'not ported yet' comment is stale), the vehicle path's entries drop (app_vehicle.h:145) |
| 0x00404600 | 13 | folded | as MW3 0x004bf530: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x00404610 | 11 | folded | covered by the port where it names it (src/mech3/zrdrainet.cpp:195) |
| 0x00404780 | 87 | unlabelled |  |
| 0x00404990 | 15 | unlabelled |  |
| 0x004049d0 | 25 | unlabelled |  |
| 0x00404a20 | 95 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x00404bf0 | 17 | unlabelled |  |
| 0x00404c20 | 19 | unlabelled |  |
| 0x00404c70 | 11 | unlabelled |  |
| 0x00404c90 | 11 | folded | read: the loading screen's status line (the briefing screen's text box at +0x11554, vt+0x7c, when the screen is up: 'Loading Environment', 'CZUnload::LoadMission()', the texture memory), from the mission load 0x00492830 / 0x00493ca0 and 0x00410820; the port's progress_text (mission.cpp, app_mission.cpp:1834 to the briefing line) |
| 0x00404cc0 | 29 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:975) |
| 0x00404d70 | 298 | unlabelled |  |
| 0x004055c0 | 12 | unlabelled |  |
| 0x00405610 | 14 | unlabelled |  |
| 0x00405640 | 14 | unlabelled |  |
| 0x00405670 | 32 | unlabelled |  |
| 0x004056c0 | 8 | folded | empty: does nothing |
| 0x004056d0 | 8 | folded | empty: does nothing |
| 0x00405700 | 8 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:68) |
| 0x00405710 | 12 | folded | covered by the port where it names it (src/mech3/briefing.h:168) |
| 0x00405730 | 22 | unlabelled |  |
| 0x00405870 | 12 | unlabelled |  |
| 0x004058f0 | 12 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:160) |
| 0x00405930 | 11 | unlabelled |  |
| 0x00405940 | 14 | unlabelled |  |
| 0x00405970 | 31 | unlabelled |  |
| 0x00405a20 | 55 | unlabelled |  |
| 0x00405b30 | 12 | unlabelled |  |
| 0x00405b60 | 32 | folded | read: a destructor the linker shares between classes (a vector of 0x4d4-byte widgets at +0x4c4 destroyed and freed, then the base's 0x005c1170), from the lobby, the text box and the script; the port's destructors |
| 0x00405bf0 | 10 | unlabelled |  |
| 0x00405c30 | 20 | unlabelled |  |
| 0x00405c80 | 12 | unlabelled |  |
| 0x00405ca0 | 98 | unlabelled |  |
| 0x00405f20 | 71 | MISSING LOGIC | pm only, visible: PM's briefing frame (MW3's 0x00406440, unpaired by the diff; briefing.cpp BriefingScreen::update) is MW3's but for the skip: on Space or S, PM first runs every step left in the script, from the one playing to the end, once each with 60 seconds (vt+0 with 60.0), then stops the voice and clears the script as MW3 does; MW3 and the port only stop (BriefingScript::stop). So skipping PM's briefing leaves the map as the whole script would have drawn it (its objects shown, lines and markers placed); the port leaves it where the skip came |
| 0x004060d0 | 562 | unlabelled |  |
| 0x00406cb0 | 8 | folded | empty: does nothing |
| 0x00406ce0 | 8 | folded | returns a constant or a global |
| 0x00406d00 | 8 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:315) |
| 0x00406d10 | 31 | replaced (SDL or the platform layer) | as MW3 0x00409b50, whose class PM's changes keep: GDI: a font's metrics from its handle (GetObjectA), the 2D text's Windows path; the port's text goes through ztwod::Host |
| 0x00406e00 | 14 | unlabelled |  |
| 0x00406e60 | 11 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:573) |
| 0x00406eb0 | 9 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:684) |
| 0x00406ec0 | 9 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:685) |
| 0x00406ed0 | 11 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:683) |
| 0x00406f40 | 30 | unlabelled |  |
| 0x00407010 | 51 | unlabelled |  |
| 0x00407120 | 17 | folded | read: MW3's 0x00408050 (MapCircle::set_radius: the radius and its square, the label at x + r, y + r; briefing.cpp), unpaired by the diff, MW3's 0x00409d80 inlined |
| 0x00407160 | 21 | unlabelled |  |
| 0x004071b0 | 12 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:59) |
| 0x00407200 | 19 | unlabelled |  |
| 0x00407250 | 62 | unlabelled |  |
| 0x004073d0 | 12 | unlabelled |  |
| 0x004073f0 | 19 | unlabelled |  |
| 0x00407450 | 12 | unlabelled |  |
| 0x004074b0 | 26 | folded | group A, read: PM does the dropped helpers' work in place: MW3's small getters and setters (of +4, +8, +0xc, +0x10) appear inline in PM's body with the same fields; the rest as MW3's (mech_anim's gait, the briefing's shown test) |
| 0x00407580 | 10 | unlabelled |  |
| 0x004075a0 | 12 | unlabelled |  |
| 0x004075c0 | 10 | unlabelled |  |
| 0x004075e0 | 65 | unlabelled |  |
| 0x004076e0 | 95 | unlabelled |  |
| 0x00407900 | 17 | unlabelled |  |
| 0x00407960 | 19 | unlabelled |  |
| 0x004079b0 | 88 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x00407b90 | 8 | folded | covered by the port where it names it (src/mech3/mech_config.h:24) |
| 0x00407ba0 | 9 | folded | ported, by docs/symbols.csv: ported; vt+4 |
| 0x00407bb0 | 20 | folded | covered by the port where it names it (src/mech3/mech_config.h:168) |
| 0x00407c00 | 8 | unlabelled |  |
| 0x00407c10 | 9 | folded | as MW3 0x00409fb0: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00407c20 | 39 | folded | covered by the port where it names it (src/mech3/briefing.cpp:915) |
| 0x00407f10 | 39 | folded | group A, read: the same as MW3's 0x004091e0 plus PM's vt+0x64 call first (the game's start: Standalone's 0x0040e3f0, the base's reader open 0x004350c0, new in PM); the pm port makes it in load_state_enter (app_menus.cpp:838, 'vt+0x64 at 0x00407f38') with the reader opened by Missions (mission.cpp:407); the movie flag is the same one (MW3 0x00608c88 +0x1c1, PM 0x0062ca98 +0x1bd) |
| 0x00407fa0 | 9 | folded | a thunk: one call to another function |
| 0x00407fb0 | 52 | unlabelled |  |
| 0x00408080 | 29 | unlabelled |  |
| 0x004080e0 | 15 | unlabelled |  |
| 0x00408120 | 111 | unlabelled |  |
| 0x00408360 | 11 | folded | read: a range of 0x4d4-byte elements destroyed, each by its vt+0 (the STL's destroy), from the HUD 0x0043c900 and 0x00405ca0; the port's containers do it |
| 0x00408390 | 19 | folded | as MW3 0x00437580: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004083e0 | 19 | unlabelled |  |
| 0x00408430 | 21 | unlabelled |  |
| 0x004084f0 | 9 | folded | covered by the port where it names it (src/mech3/player_view.h:36) |
| 0x00408510 | 12 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:108) |
| 0x00408530 | 9 | folded | as MW3 0x0040a9e0: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00408540 | 9 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:112) |
| 0x00408550 | 22 | folded | covered by the port where it names it (src/mech3/player_view.h:99) |
| 0x00408580 | 74 | folded | read: a std::vector<int>'s insert (the STL's), from the camera 0x004311e0 and the vehicle 0x004b0790; the port's vectors |
| 0x00408740 | 39 | folded | as MW3 0x0040abf0: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x004087f0 | 38 | unlabelled |  |
| 0x004088e0 | 9 | unlabelled | follows 0x004fd5e0:  |
| 0x00408920 | 11 | unlabelled |  |
| 0x00408940 | 9 | folded | as MW3 0x0040a850: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x00408950 | 12 | folded | as MW3 0x004104a0: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00408980 | 24 | unlabelled |  |
| 0x00408b20 | 50 | MISSING LOGIC | PM's camera ground check (pm only): the ground under the camera probed from 1024 m above it ([0x006026e0]) in PM, 50 m in MW3 (player_view.cpp:797 keeps MW3's), and PM's set_underwater 0x00408d00 also sets the renderer's water mode (0x005404f0: 2 under water, 1 not) besides the fog |
| 0x00408d00 | 25 | MISSING LOGIC | PM's fog under water (pm only): PM's set_underwater 0x00408d00 sets the fog mode [0x00a4e038] to 2 through 0x005404f0, which keeps 2 against anything but 0, so both model draws fog every model, not only flagged ones (gmod_draw.cpp:268 reads the mode); back above water it sets 1. The port's set_underwater is MW3's (player_view.cpp:820), zmod::fog_set_on has no sticky 2, and nothing sets 2, so under water only flagged models are fogged |
| 0x00408f40 | 145 | unlabelled |  |
| 0x00409830 | 71 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x0040bb00: the camera dialog (resource 0x6a: the camera's place and angles in degrees, editable; 0x0040bd00 its window): a dev tool's logic |
| 0x00409a10 | 21 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x0040bd00: MFC dev dialogs' plumbing: resource 0x6a (a dialog of float fields: constructor, DDX, close, DoModal), the teleport dialog's slider (OnHScroll) and destructor, a combo box's pick into the texture memory setting |
| 0x00409a60 | 9 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x00401030: MFC dialog plumbing of dev dialogs (CDialog constructors and destructors, EnableWindow): resource 0x67 and the teleport and net dialogs' destructors |
| 0x00409a70 | 9 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x00401040: MFC dialog plumbing of dev dialogs (CDialog constructors and destructors, EnableWindow): resource 0x67 and the teleport and net dialogs' destructors |
| 0x00409a80 | 12 | unlabelled |  |
| 0x00409aa0 | 18 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x0040bd70: MFC dev dialogs' plumbing: resource 0x6a (a dialog of float fields: constructor, DDX, close, DoModal), the teleport dialog's slider (OnHScroll) and destructor, a combo box's pick into the texture memory setting |
| 0x00409b50 | 8 | folded | returns a constant or a global |
| 0x00409bb0 | 10 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x0040bea0: MFC dev dialogs' plumbing: resource 0x6a (a dialog of float fields: constructor, DDX, close, DoModal), the teleport dialog's slider (OnHScroll) and destructor, a combo box's pick into the texture memory setting |
| 0x00409ec0 | 162 | folded | group A, read: the same logic; PM inlines the compare (0x00409c60 called once with the value read) where MW3 calls 0x0040bf50 per name; the machine fields read are the same ones in the same order (MW3 0x005f4ca8/ac/b8/b4/b0, PM 0x00618768/6c/78/74/70: VIDEO_KB one dword off in both) |
| 0x0040a870 | 41 | folded | read: MW3's 0x0040cb30 (options' apply-all: the same 13 getter-setter pairs in the same order, then the two floats), unpaired by the diff; the port's options_apply_all (options.cpp:556) |
| 0x0040a920 | 14 | folded | read: HWCardDev's setter (the device number stored, hardware chosen when 0 or above, 0x0040b340), from the settings read 0x0040a100 and the options screens; the port's set_hw_card_dev (options.cpp:53) |
| 0x0040ab60 | 8 | folded | covered by the port where it names it (src/mech3/briefing.cpp:824) |
| 0x0040afe0 | 15 | folded | MW3's set_shadow_parts 0x0040d1f0 (options.cpp:219), unpaired by the diff: the same body (the hardware or software slot, then the shadows on or off); not run under a test (trace) |
| 0x0040b120 | 47 | MISSING LOGIC | a PM value the shared port keeps as MW3's (pm only): the object detail's level 2 sets the camera's LOD scale to 0.2 in PM (0x3e4ccccd), 0.4 in MW3 (options.cpp:260, cam_scale) |
| 0x0040b210 | 8 | folded | inside its ported callers (0x0043a060): a small helper their ports do their own way |
| 0x0040b250 | 11 | folded | as MW3 0x0040d410: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040b270 | 35 | unlabelled |  |
| 0x0040b320 | 9 | folded | read: MW3's 0x0040d4a0 (the pixel-double flag, mode 3's 320x240 drawn doubled), which the diff paired with 0x0040a7a0 by mistake; written by the screen-mode setter 0x0040b720 (MW3's 0x0040d810) and read through 0x0040b330; the port's set_pixel_double (options.cpp:290, called at 457) |
| 0x0040b330 | 8 | folded | returns a constant or a global |
| 0x0040b670 | 8 | folded | covered by the port where it names it (src/mech3/app_hooks.h:96) |
| 0x0040b680 | 8 | folded | covered by the port where it names it (src/mech3/app_hooks.h:97) |
| 0x0040b690 | 9 | folded | covered by the port where it names it (src/mech3/options.h:234) |
| 0x0040b6a0 | 8 | folded | covered by the port where it names it (src/mech3/app_hooks.h:89) |
| 0x0040b6b0 | 8 | folded | inside its ported callers (0x00484fa0, 0x004aefe0): a small helper their ports do their own way |
| 0x0040b6c0 | 8 | folded | as MW3 0x0040d7b0: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040b6d0 | 8 | folded | covered by the port where it names it (src/mech3/app_hooks.h:91) |
| 0x0040b6e0 | 9 | folded | a thunk: one call to another function |
| 0x0040b700 | 9 | folded | a thunk: one call to another function |
| 0x0040b720 | 69 | unlabelled |  |
| 0x0040b9f0 | 12 | folded | inside its ported callers (0x00492830): a small helper their ports do their own way |
| 0x0040bd70 | 34 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x0040def0: MFC dev dialogs' plumbing: resource 0x6a (a dialog of float fields: constructor, DDX, close, DoModal), the teleport dialog's slider (OnHScroll) and destructor, a combo box's pick into the texture memory setting |
| 0x0040be40 | 40 | folded | as MW3 0x0040e120: covered where the port does it: src/mech3/game_editor_net.cpp:229 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0040bf30 | 20 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x0040e200: MFC dev dialogs' plumbing: resource 0x6a (a dialog of float fields: constructor, DDX, close, DoModal), the teleport dialog's slider (OnHScroll) and destructor, a combo box's pick into the texture memory setting |
| 0x0040bf90 | 43 | dev dialog (MFC), its window plumbing: replaced | called only from 0x0040bf30, which are devdialog-shell |
| 0x0040c300 | 13 | folded | covered by the port where it names it (src/mech3/pilot_games.cpp:956) |
| 0x0040c340 | 12 | unlabelled |  |
| 0x0040c600 | 23 | folded | covered by the port where it names it (src/mech3/pilot.h:259) |
| 0x0040c660 | 8 | folded | returns a constant or a global |
| 0x0040c670 | 8 | folded | returns a constant or a global |
| 0x0040c6a0 | 11 | folded | covered by the port where it names it (src/mech3/pilot.h:322) |
| 0x0040c6b0 | 12 | unlabelled |  |
| 0x0040c6c0 | 8 | unlabelled |  |
| 0x0040c6d0 | 8 | folded | returns a constant or a global |
| 0x0040c6e0 | 12 | folded | covered by the port where it names it (src/mech3/pilot.h:259) |
| 0x0040c700 | 8 | folded | as MW3 0x00437860: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0040c720 | 9 | replaced (SDL or the platform layer) | follows 0x00596e50: as MW3 0x00576040, whose class PM's changes keep: Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x0040c730 | 8 | unlabelled |  |
| 0x0040c740 | 8 | unlabelled |  |
| 0x0040c750 | 8 | unlabelled |  |
| 0x0040c760 | 12 | unlabelled |  |
| 0x0040c780 | 28 | unlabelled |  |
| 0x0040c800 | 46 | unlabelled |  |
| 0x0040c9c0 | 17 | folded | covered by the port where it names it (src/mech3/mission.cpp:93) |
| 0x0040ca30 | 9 | folded | inside its ported callers (0x0048eaf0): a small helper their ports do their own way |
| 0x0040ca50 | 9 | folded | inside its ported callers (0x0048eaf0): a small helper their ports do their own way |
| 0x0040ca70 | 8 | folded | covered by the port where it names it (src/mech3/briefing.cpp:842) |
| 0x0040ca80 | 23 | folded | as MW3 0x00436a40: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0040cac0 | 10 | folded | covered by the port where it names it (src/mech3/action_menu.cpp:526) |
| 0x0040ce90 | 32 | folded | covered by the port where it names it (src/mech3/mech_dead.cpp:123) |
| 0x0040cf60 | 17 | folded | read: container code inside ported callers (vector copy, insert and reserve, introsort, list clear and find, uninitialised copy) and thunks whose MW3 pair is identical |
| 0x0040cfa0 | 108 | folded | read: container code inside ported callers (vector copy, insert and reserve, introsort, list clear and find, uninitialised copy) and thunks whose MW3 pair is identical |
| 0x0040d230 | 23 | folded | read: container code inside ported callers (vector copy, insert and reserve, introsort, list clear and find, uninitialised copy) and thunks whose MW3 pair is identical |
| 0x0040d280 | 45 | unlabelled |  |
| 0x0040d320 | 110 | unlabelled |  |
| 0x0040d4d0 | 9 | folded | follows 0x0040d510: as MW3 0x0042a570: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0040d4e0 | 9 | folded | a thunk: one call to another function |
| 0x0040d510 | 15 | folded | as MW3 0x0042a570: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0040d530 | 38 | unlabelled |  |
| 0x0040d5e0 | 12 | unlabelled |  |
| 0x0040d600 | 22 | unlabelled |  |
| 0x0040d660 | 117 | folded | read: container code inside ported callers (vector copy, insert and reserve, introsort, list clear and find, uninitialised copy) and thunks whose MW3 pair is identical |
| 0x0040d900 | 8 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:200) |
| 0x0040d920 | 45 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x0040da00 | 36 | folded | covered by the port where it names it (src/mech3/pilot_standalone.cpp:86) |
| 0x0040dac0 | 9 | folded | a thunk: one call to another function |
| 0x0040dae0 | 9 | folded | a thunk: one call to another function |
| 0x0040db00 | 9 | folded | a thunk: one call to another function |
| 0x0040db20 | 12 | unlabelled |  |
| 0x0040db40 | 9 | folded | follows 0x00434b10: checked by test (block trace, 2026-11-13): test_pm_pilot's write 0x0047b610 runs it; the written pilot file compared byte for byte |
| 0x0040db50 | 38 | unlabelled |  |
| 0x0040dc00 | 24 | folded | inside its ported callers (0x0040e250): a small helper their ports do their own way |
| 0x0040dc70 | 81 | unlabelled |  |
| 0x0040de60 | 36 | folded | covered by the port where it names it (src/mech3/pilot_standalone.cpp:86) |
| 0x0040dee0 | 8 | folded | returns a constant or a global |
| 0x0040e1b0 | 12 | unlabelled |  |
| 0x0040e1d0 | 26 | unlabelled |  |
| 0x0040e310 | 8 | unlabelled |  |
| 0x0040e320 | 8 | unlabelled |  |
| 0x0040e330 | 8 | unlabelled |  |
| 0x0040e5e0 | 9 | folded | a thunk: one call to another function |
| 0x0040e5f0 | 17 | folded | covered by the port where it names it (src/mech3/mission.cpp:93) |
| 0x0040e660 | 8 | unlabelled |  |
| 0x0040e670 | 9 | folded | a thunk: one call to another function |
| 0x0040e680 | 9 | folded | a thunk: one call to another function |
| 0x0040e690 | 9 | folded | a thunk: one call to another function |
| 0x0040e6a0 | 9 | folded | covered by the port where it names it (src/mech3/briefing.cpp:824) |
| 0x0040e6b0 | 9 | folded | inside its ported callers (0x0047f420): a small helper their ports do their own way |
| 0x0040e860 | 78 | unlabelled |  |
| 0x0040e9c0 | 31 | unlabelled |  |
| 0x0040ea50 | 9 | unlabelled | follows 0x0040ea60:  |
| 0x0040ea60 | 47 | unlabelled |  |
| 0x0040eb80 | 24 | folded | covered by the port where it names it (src/mech3/game_editor.cpp:46) |
| 0x0040ebf0 | 10 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:307) |
| 0x0040ec40 | 22 | unlabelled |  |
| 0x0040ecb0 | 9 | folded | a thunk: one call to another function |
| 0x0040ecd0 | 21 | unlabelled |  |
| 0x0040ef60 | 56 | unlabelled |  |
| 0x0040f0e0 | 12 | unlabelled |  |
| 0x0040f100 | 14 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x0040e8b0: MFC dev dialogs' plumbing: resource 0x6a (a dialog of float fields: constructor, DDX, close, DoModal), the teleport dialog's slider (OnHScroll) and destructor, a combo box's pick into the texture memory setting |
| 0x0040f130 | 22 | folded | covered by the port where it names it (src/mech3/entity_bind.cpp:51) |
| 0x0040f1a0 | 22 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:552) |
| 0x0040f1d0 | 14 | folded | covered by the port where it names it (src/mech3/entity_bind.cpp:40) |
| 0x0040f1f0 | 14 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:558) |
| 0x0040f210 | 24 | unlabelled |  |
| 0x0040f330 | 9 | folded | a thunk: one call to another function |
| 0x0040f350 | 21 | folded | covered by the port where it names it (src/mech3/entity_bind.cpp:24) |
| 0x0040f3a0 | 21 | folded | covered by the port where it names it (src/mech3/entity_bind.cpp:29) |
| 0x0040f400 | 9 | folded | a thunk: one call to another function |
| 0x0040f420 | 9 | folded | a thunk: one call to another function |
| 0x0040f4c0 | 21 | folded | covered by the port where it names it (src/mech3/entity_bind.cpp:57) |
| 0x0040f580 | 9 | folded | a thunk: one call to another function |
| 0x0040f5a0 | 21 | folded | covered by the port where it names it (src/mech3/entity_bind.cpp:46) |
| 0x0040f5f0 | 9 | folded | a thunk: one call to another function |
| 0x0040f610 | 9 | folded | a thunk: one call to another function |
| 0x0040f630 | 9 | folded | a thunk: one call to another function |
| 0x0040f650 | 9 | folded | a thunk: one call to another function |
| 0x0040f670 | 14 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:534) |
| 0x0040f690 | 9 | folded | a thunk: one call to another function |
| 0x0040f6b0 | 9 | folded | a thunk: one call to another function |
| 0x0040f6d0 | 14 | folded | covered by the port where it names it (src/mech3/entity_bind.cpp:52) |
| 0x0040f780 | 9 | debug or dev tool: PORT (preserve) | follows 0x0040f7a0: as MW3 0x0040ff40, whose class PM's changes keep: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x0040f7a0 | 34 | debug or dev tool: PORT (preserve) | as MW3 0x0040ff40, whose class PM's changes keep: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x0040f840 | 9 | folded | a thunk: one call to another function |
| 0x0040f860 | 20 | unlabelled |  |
| 0x0040f8d0 | 9 | debug or dev tool: PORT (preserve) | follows 0x0040fa20: as MW3 0x0040f150: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x0040f8e0 | 9 | folded | a thunk: one call to another function |
| 0x0040f9c0 | 9 | folded | read: container code inside ported callers (vector copy, insert and reserve, introsort, list clear and find, uninitialised copy) and thunks whose MW3 pair is identical |
| 0x0040f9d0 | 9 | folded | a thunk: one call to another function |
| 0x0040fa00 | 10 | folded | as MW3 0x0040f130: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040fa20 | 33 | debug or dev tool: PORT (preserve) | as MW3 0x0040f150: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x0040fac0 | 12 | folded | covered by the port where it names it (src/mech3/hud.h:62) |
| 0x0040fae0 | 35 | folded | covered by the port where it names it (src/mech3/app_engine.cpp:985) |
| 0x0040fd00 | 75 | unlabelled |  |
| 0x00410030 | 12 | folded | covered by the port where it names it (src/mech3/message_panel.h:18) |
| 0x00410050 | 32 | unlabelled |  |
| 0x00410110 | 24 | debug or dev tool: PORT (preserve) | as MW3 0x0040f7f0, whose class PM's changes keep: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x00410190 | 10 | debug or dev tool: PORT (preserve) | called only from 0x00410110, which are debug |
| 0x004101b0 | 8 | folded | returns a constant or a global |
| 0x004101c0 | 10 | debug or dev tool: PORT (preserve) | as MW3 0x0040f890: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x004101e0 | 10 | debug or dev tool: PORT (preserve) | as MW3 0x0040f8b0: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x004103a0 | 13 | folded | covered by the port where it names it (src/mech3/briefing.cpp:477) |
| 0x00410470 | 108 | debug or dev tool: PORT (preserve) | as MW3 0x0040fb50, whose class PM's changes keep: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x004107a0 | 17 | folded | as MW3 0x0040fe70: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00410820 | 17 | unused: PORT (preserve) | as MW3 0x0040fef0: the timing marks ('Dummy Frame', 'Lets Play!'): act only with [0x005e14f0], 0 in the data (program.md, no effect) |
| 0x00410880 | 9 | unused: PORT (preserve) | called only from 0x00410820, which are unused |
| 0x00410890 | 9 | folded | a thunk: one call to another function |
| 0x004108d0 | 9 | folded | a thunk: one call to another function |
| 0x004108e0 | 9 | folded | a thunk: one call to another function |
| 0x00410920 | 9 | folded | a thunk: one call to another function |
| 0x00410930 | 9 | folded | a thunk: one call to another function |
| 0x00410970 | 9 | folded | a thunk: one call to another function |
| 0x00410980 | 9 | folded | a thunk: one call to another function |
| 0x004109c0 | 9 | folded | a thunk: one call to another function |
| 0x004109d0 | 9 | folded | a thunk: one call to another function |
| 0x00410a10 | 9 | folded | a thunk: one call to another function |
| 0x00410a20 | 9 | folded | a thunk: one call to another function |
| 0x00410a60 | 9 | folded | a thunk: one call to another function |
| 0x00410a70 | 9 | folded | a thunk: one call to another function |
| 0x00410ab0 | 9 | folded | a thunk: one call to another function |
| 0x00410ac0 | 10 | folded | covered by the port where it names it (src/mech3/zai_steps.cpp:8) |
| 0x00410ae0 | 8 | folded | returns a constant or a global |
| 0x00410af0 | 8 | folded | returns a constant or a global |
| 0x00410b20 | 9 | folded | as MW3 0x004104f0: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00410b40 | 9 | folded | as MW3 0x00410520: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00410b60 | 9 | folded | as MW3 0x00410540: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00410b80 | 17 | folded | as MW3 0x00410560: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00410bf0 | 16 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:115) |
| 0x00410c10 | 12 | unlabelled |  |
| 0x00410c40 | 9 | folded | a thunk: one call to another function |
| 0x00410c80 | 9 | folded | a thunk: one call to another function |
| 0x00410c90 | 10 | folded | covered by the port where it names it (src/mech3/zai_brain.h:424) |
| 0x00410cb0 | 9 | folded | a thunk: one call to another function |
| 0x00410cf0 | 9 | folded | a thunk: one call to another function |
| 0x00410d00 | 10 | folded | covered by the port where it names it (src/mech3/zai_brain.h:426) |
| 0x00410d20 | 9 | folded | a thunk: one call to another function |
| 0x00410d60 | 9 | folded | a thunk: one call to another function |
| 0x00410d70 | 9 | folded | a thunk: one call to another function |
| 0x00410db0 | 9 | folded | a thunk: one call to another function |
| 0x00410dc0 | 10 | folded | covered by the port where it names it (src/mech3/zai_brain.cpp:157) |
| 0x00410df0 | 9 | folded | a thunk: one call to another function |
| 0x00410e30 | 9 | folded | a thunk: one call to another function |
| 0x00410e40 | 10 | folded | covered by the port where it names it (src/mech3/zai_brain.cpp:157) |
| 0x00410e60 | 12 | unlabelled |  |
| 0x00410e80 | 9 | folded | a thunk: one call to another function |
| 0x00410ec0 | 9 | MISSING LOGIC (only data the shipped game lacks reaches it) | follows 0x004c7300: as MW3 0x004c0310, whose class PM's changes keep: the built-in net 0x00604008 (one behaviour: the default one, 0x00604068): what an order goes on when its vehicle has no net and the record's net is not found. The port's builtin_net() is null in the game (WiredBrainHooks does not give one), so Order::reset/Brain::push would call through a null net. Not reached by the shipped missions (every AI has a net; the mission tests pass); a mod's vehicle without a net would. Port: small (a CompiledNet holding the default behaviour) |
| 0x00410ed0 | 9 | folded | a thunk: one call to another function |
| 0x00410f00 | 10 | folded | as MW3 0x004106d0: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00410f10 | 12 | unlabelled |  |
| 0x00410f40 | 9 | unlabelled | follows 0x004c7560:  |
| 0x00410f50 | 9 | folded | a thunk: one call to another function |
| 0x00410f80 | 29 | unlabelled |  |
| 0x00411080 | 60 | folded | group A, read: the same logic on PM's layout and helpers (the reader's lookup 0x00578630 for 0x00556740, PM's list helpers, the flare's fade with PM's globals, the gravity command, the objective's distance through PM's HUD getter); the call counts differ by how often a getter is called |
| 0x004111a0 | 18 | folded | as MW3 0x00410920: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x004111d0 | 38 | folded | covered by the port where it names it (src/mech3/controls_cfg.h:24) |
| 0x004112a0 | 15 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x004112c0 | 9 | folded | a thunk: one call to another function |
| 0x00411380 | 24 | folded | as MW3 0x00410b00: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00411650 | 73 | unlabelled |  |
| 0x00411890 | 12 | folded | a thunk: one call to another function |
| 0x004118b0 | 9 | folded | a thunk: one call to another function |
| 0x00411a10 | 148 | unlabelled |  |
| 0x004128e0 | 235 | unlabelled |  |
| 0x00412e20 | 42 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00412ef0 | 58 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00413010 | 106 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004131c0 | 318 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004136f0 | 31 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00413780 | 47 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00413830 | 221 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00413b70 | 51 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00413c40 | 11 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x00413c70 | 9 | folded | follows 0x0040d510: as MW3 0x0042a570: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x00413c80 | 9 | folded | a thunk: one call to another function |
| 0x00413ce0 | 40 | unlabelled |  |
| 0x00414090 | 111 | folded | as MW3 0x004137c0: covered where the port does it: src/mech3/landmines.cpp:53 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00414340 | 14 | folded | as MW3 0x00413a70: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x004147e0 | 12 | unlabelled |  |
| 0x00414810 | 39 | unlabelled |  |
| 0x004148c0 | 28 | unlabelled |  |
| 0x00414950 | 24 | unlabelled |  |
| 0x00414b80 | 11 | folded | ported, by docs/symbols.csv: ported (app_mech DamageCalls::roster_x1c): the roster entry's mech definition's skin set (+0x1c) |
| 0x00414ba0 | 25 | MISSING LOGIC | pm only, minor, effect to check: PM's mech_set_skin (MW3's 0x00414390, mech_build.cpp mech_set_skin; unpaired by the diff), from the mech build 0x00415190, the network (0x0046f510, 0x004701d0), the mission launch 0x00494500 and the player 0x004a0e10 / 0x004a17d0: when the torso's headlight (torso part +0x7c) is there, its node's only child is detached before the skin is applied (0x00479c30, kept at +0x38) and put back after (0x00479c60); the port applies the skin with the child in place. Whether the skin apply (0x004773e0) touches that child is not read; 0x0041b900 (a vt slot) also detaches it |
| 0x00415160 | 17 | unlabelled |  |
| 0x00415190 | 112 | unlabelled |  |
| 0x004158b0 | 8 | unlabelled |  |
| 0x004158c0 | 8 | unlabelled |  |
| 0x004158d0 | 8 | folded | returns a constant or a global |
| 0x004158e0 | 8 | folded | returns a constant or a global |
| 0x004158f0 | 9 | unlabelled | follows 0x004311a0:  |
| 0x00415910 | 9 | unlabelled | follows 0x004311c0:  |
| 0x00415940 | 173 | MISSING LOGIC | pm only, minor (a mech deleted mid-mission: a network player leaving): PM's mech destructor (0x00415940, MW3's 0x00414f40, mech_build.cpp:225) first clears the owner (+0x6c, +0x70, +0x74) of every live shot that mech fired (0x005a35f0 over zWeapon's pools); MW3 and the port do not (all three free the node's handler table: MW3 0x00583400, PM 0x005a4de0, the port app_mech.cpp:848), so a shot outliving its mech keeps the gone owner. To check: whether the port's shot reads that owner afterwards (its hit credit, its tether) |
| 0x00415fc0 | 159 | unlabelled |  |
| 0x00416450 | 14 | folded | covered by the port where it names it (src/mech3/mech_repair.cpp:60) |
| 0x00416480 | 16 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:1036) |
| 0x004164b0 | 9 | folded | covered by the port where it names it (src/mech3/mech_config.h:69) |
| 0x004164c0 | 23 | folded | inside its ported callers (0x00415fc0, 0x004705e0): a small helper their ports do their own way |
| 0x00416540 | 10 | folded | covered by the port where it names it (src/mech3/mech_build.cpp:112) |
| 0x00416560 | 54 | unlabelled |  |
| 0x004166d0 | 20 | folded | covered by the port where it names it (src/mech3/mech_build.cpp:456) |
| 0x00416710 | 83 | unlabelled |  |
| 0x004168b0 | 140 | unlabelled |  |
| 0x00416c70 | 37 | unlabelled |  |
| 0x00417270 | 9 | folded | a thunk: one call to another function |
| 0x00417e00 | 11 | folded | covered by the port where it names it (src/mech3/mech_move.cpp:716) |
| 0x00417e50 | 11 | folded | covered by the port where it names it (src/mech3/mech.h:74) |
| 0x00417e80 | 11 | unlabelled |  |
| 0x00417ea0 | 41 | unlabelled |  |
| 0x00417f50 | 216 | unlabelled |  |
| 0x00419d50 | 12 | unlabelled |  |
| 0x00419dc0 | 11 | folded | covered by the port where it names it (src/mech3/mech_part_update.cpp:725) |
| 0x0041ad30 | 9 | folded | covered by the port where it names it (src/mech3/app_weapons.cpp:246) |
| 0x0041ade0 | 11 | folded | checked by test (block trace, 2026-11-13): test_pm_damage, test_pm_hit and test_pm_heat run it (the damage done, the parts' state, the heat frame by frame compared) |
| 0x0041be50 | 110 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x0041c3f0 | 50 | unlabelled |  |
| 0x0041c4c0 | 24 | unlabelled |  |
| 0x0041c5d0 | 31 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:655) |
| 0x0041c630 | 21 | MISSING LOGIC | as MW3 0x0041b930: the player's mech suspended while the view is off it: the mech's vt+0x104 (0x0041b980, called by the view's attach 0x0040adf0 on the old target) puts the player's mech into state 14 'suspended' (Mech Update returns at once; also the HUD's 0x00430aa0), vt+0x100 (0x0041b930, on the new target) puts its state back. The port's view hooks say these do nothing (app_view.cpp:165) and nothing sets state 14 (mech_update.cpp only reads it). In play the view leaves the player's mech at a mission's end, on a rebuild and for the editor or stealth camera; whether it shows (the mech going on under its controls meanwhile) is to be seen. Port: small |
| 0x0041c680 | 22 | MISSING LOGIC | as MW3 0x0041b980: the player's mech suspended while the view is off it: the mech's vt+0x104 (0x0041b980, called by the view's attach 0x0040adf0 on the old target) puts the player's mech into state 14 'suspended' (Mech Update returns at once; also the HUD's 0x00430aa0), vt+0x100 (0x0041b930, on the new target) puts its state back. The port's view hooks say these do nothing (app_view.cpp:165) and nothing sets state 14 (mech_update.cpp only reads it). In play the view leaves the player's mech at a mission's end, on a rebuild and for the editor or stealth camera; whether it shows (the mech going on under its controls meanwhile) is to be seen. Port: small |
| 0x0041c740 | 69 | unlabelled |  |
| 0x0041c9d0 | 11 | unlabelled |  |
| 0x0041cad0 | 465 | unlabelled |  |
| 0x0041d880 | 13 | unlabelled |  |
| 0x0041d8b0 | 13 | folded | as MW3 0x0041ca60: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0041d8f0 | 16 | debug or dev tool: PORT (preserve) | as MW3 0x0041ca90: a debug key command (0x6d, case 2 of the key switch 0x0040dba0) toggling the mech's +0x634 bit 1; the release never registers 0x6d (0x0040db60 registers 0x61, 0x6c, 0x70, 0x7e) |
| 0x0041dbb0 | 19 | folded | group A, read: the same logic: a callee renamed by the map (0x0051da90 = PM 0x00537420), a direct call made virtual (0x0041bd80 through vt+0x168), PM's layout (+0xe8 for +0xe4, a null check added), a destructor on PM's members |
| 0x0041dc00 | 16 | unlabelled |  |
| 0x0041dc60 | 8 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x0041dc70 | 9 | folded | inside its ported callers (0x004225e0, 0x00422780, 0x00422cd0): a small helper their ports do their own way |
| 0x0041dc80 | 9 | folded | a thunk: one call to another function |
| 0x0041dd40 | 22 | unlabelled |  |
| 0x0041df80 | 14 | unlabelled |  |
| 0x0041dfb0 | 8 | folded | ported, by docs/symbols.csv: ported; Mech::speed_at (vt+0x118): max_speed * share |
| 0x0041dff0 | 8 | folded | returns a constant or a global |
| 0x0041e000 | 8 | folded | ported, by docs/symbols.csv: ported; Mech::stopping_distance (vt+0x120): speed / decel_rate |
| 0x0041e010 | 9 | UNSURE | as MW3 0x0041d220: the mech's (and another class's, vtables 0x005bcf80.., 0x005c3590..) speed and motion queries for the AI, vt+0x124..+0x13c and two part queries over 0x00465bc0 / 0x00465c70: the port answers vt+0x118 and +0x120 (app_mech.h:445); whether the AI asks these others in play is to settle |
| 0x0041e030 | 20 | folded | ported, by docs/symbols.csv: ported; Mech::stop_share (vt+0x12c) |
| 0x0041e070 | 13 | UNSURE | as MW3 0x0041d280: the mech's (and another class's, vtables 0x005bcf80.., 0x005c3590..) speed and motion queries for the AI, vt+0x124..+0x13c and two part queries over 0x00465bc0 / 0x00465c70: the port answers vt+0x118 and +0x120 (app_mech.h:445); whether the AI asks these others in play is to settle |
| 0x0041e0b0 | 19 | UNSURE | as MW3 0x0041d2c0: the mech's (and another class's, vtables 0x005bcf80.., 0x005c3590..) speed and motion queries for the AI, vt+0x124..+0x13c and two part queries over 0x00465bc0 / 0x00465c70: the port answers vt+0x118 and +0x120 (app_mech.h:445); whether the AI asks these others in play is to settle |
| 0x0041e0f0 | 9 | UNSURE | as MW3 0x0041d300: the mech's (and another class's, vtables 0x005bcf80.., 0x005c3590..) speed and motion queries for the AI, vt+0x124..+0x13c and two part queries over 0x00465bc0 / 0x00465c70: the port answers vt+0x118 and +0x120 (app_mech.h:445); whether the AI asks these others in play is to settle |
| 0x0041e110 | 20 | unlabelled |  |
| 0x0041e180 | 23 | UNSURE | as MW3 0x0041d380: the mech's (and another class's, vtables 0x005bcf80.., 0x005c3590..) speed and motion queries for the AI, vt+0x124..+0x13c and two part queries over 0x00465bc0 / 0x00465c70: the port answers vt+0x118 and +0x120 (app_mech.h:445); whether the AI asks these others in play is to settle |
| 0x0041e260 | 9 | folded | covered by the port where it names it (src/mech3/app_hud.cpp:506) |
| 0x0041e270 | 10 | folded | ported, by docs/symbols.csv: ported inline: thrust x 8 + gravity (JumpState) |
| 0x0041e280 | 10 | folded | inside its ported callers (0x004299f0, 0x004db0e0): a small helper their ports do their own way |
| 0x0041e4c0 | 8 | folded | as MW3 0x0041d6e0: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0041eb10 | 14 | unlabelled |  |
| 0x0041ebb0 | 12 | folded | as MW3 0x0041dc40: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0041ed00 | 52 | UNSURE | as MW3 0x0041dd90: the mech's (and another class's, vtables 0x005bcf80.., 0x005c3590..) speed and motion queries for the AI, vt+0x124..+0x13c and two part queries over 0x00465bc0 / 0x00465c70: the port answers vt+0x118 and +0x120 (app_mech.h:445); whether the AI asks these others in play is to settle |
| 0x0041edf0 | 21 | folded | covered by the port where it names it (src/mech3/mech_build.cpp:447) |
| 0x0041ee80 | 12 | folded | inside its ported callers (0x0043cc20, 0x00474f80): a small helper their ports do their own way |
| 0x0041f000 | 38 | unlabelled |  |
| 0x0041f180 | 12 | folded | covered by the port where it names it (src/gamez/ztwod/twod_console.cpp:81) |
| 0x0041f1c0 | 12 | unlabelled |  |
| 0x0041f1f0 | 42 | unlabelled |  |
| 0x0041f740 | 9 | folded | a thunk: one call to another function |
| 0x0041f760 | 8 | folded | returns a constant or a global |
| 0x0041f790 | 9 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:1323) |
| 0x0041f7a0 | 62 | folded | PM's model LOD switch (+0x6b4 the level, +0x6b8 the models; refused while a weapon is busy): mech_lod.cpp:110, lod_switch through switch_model |
| 0x0041f8d0 | 12 | folded | covered by the port where it names it (src/mech3/mech.h:413) |
| 0x0041f900 | 12 | folded | covered by the port where it names it (src/mech3/mech_command.cpp:265) |
| 0x0041f920 | 9 | folded | inside its ported callers (0x004705e0, 0x00471040): a small helper their ports do their own way |
| 0x0041f930 | 8 | folded | covered by the port where it names it (src/mech3/app_hud.cpp:253) |
| 0x0041f940 | 27 | folded | covered by the port where it names it (src/mech3/mech_anim.cpp:578) |
| 0x0041f980 | 21 | folded | covered by the port where it names it (src/mech3/mech_anim.h:27) |
| 0x0041f9e0 | 21 | folded | as MW3 0x0041ed70: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0041fa40 | 23 | folded | covered by the port where it names it (src/mech3/mech_anim.cpp:314) |
| 0x0041fac0 | 10 | folded | as MW3 0x0041ee50: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0041faf0 | 15 | folded | group A, read: PM does the dropped helpers' work in place: MW3's small getters and setters (of +4, +8, +0xc, +0x10) appear inline in PM's body with the same fields; the rest as MW3's (mech_anim's gait, the briefing's shown test) |
| 0x0041fb50 | 9 | folded | as MW3 0x0041eee0: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0041fb70 | 18 | unlabelled |  |
| 0x0041fc00 | 18 | unlabelled |  |
| 0x0041fc70 | 40 | unlabelled |  |
| 0x0041fdc0 | 9 | folded | a thunk: one call to another function |
| 0x0041ffa0 | 57 | folded | group A, read: PM does the dropped helpers' work in place: MW3's small getters and setters (of +4, +8, +0xc, +0x10) appear inline in PM's body with the same fields; the rest as MW3's (mech_anim's gait, the briefing's shown test) |
| 0x004201e0 | 31 | unlabelled |  |
| 0x00420530 | 25 | folded | covered by the port where it names it (src/mech3/mech_anim.h:27) |
| 0x00420930 | 27 | unlabelled |  |
| 0x00420a50 | 17 | unlabelled |  |
| 0x00420aa0 | 87 | unlabelled |  |
| 0x00420e40 | 42 | folded | checked by test (block trace, 2026-11-13): test_pm_mechanim runs it (the animation objects and samplers compared); the compare passes it |
| 0x00420f10 | 15 | folded | covered by the port where it names it (src/mech3/mech_anim.cpp:378) |
| 0x004210d0 | 23 | folded | group A, read: PM does the dropped helpers' work in place: MW3's small getters and setters (of +4, +8, +0xc, +0x10) appear inline in PM's body with the same fields; the rest as MW3's (mech_anim's gait, the briefing's shown test) |
| 0x004211b0 | 21 | folded | group A, read: PM does the dropped helpers' work in place: MW3's small getters and setters (of +4, +8, +0xc, +0x10) appear inline in PM's body with the same fields; the rest as MW3's (mech_anim's gait, the briefing's shown test) |
| 0x004212c0 | 11 | unlabelled |  |
| 0x004213d0 | 18 | unlabelled |  |
| 0x004214c0 | 52 | unlabelled |  |
| 0x00421680 | 39 | unlabelled |  |
| 0x00421870 | 39 | unlabelled |  |
| 0x00421a60 | 31 | unlabelled |  |
| 0x00421bd0 | 18 | unlabelled |  |
| 0x00421ce0 | 16 | unlabelled |  |
| 0x00421e00 | 40 | unlabelled |  |
| 0x00421ef0 | 22 | folded | covered by the port where it names it (src/mech3/mech_anim.cpp:639) |
| 0x00421f60 | 21 | folded | covered by the port where it names it (src/mech3/mech_data.cpp:548) |
| 0x00421fc0 | 20 | folded | group A, read: PM does the dropped helpers' work in place: MW3's small getters and setters (of +4, +8, +0xc, +0x10) appear inline in PM's body with the same fields; the rest as MW3's (mech_anim's gait, the briefing's shown test) |
| 0x00422080 | 36 | unlabelled |  |
| 0x00422260 | 10 | folded | group A, read: PM does the dropped helpers' work in place: MW3's small getters and setters (of +4, +8, +0xc, +0x10) appear inline in PM's body with the same fields; the rest as MW3's (mech_anim's gait, the briefing's shown test) |
| 0x00422380 | 12 | folded | covered by the port where it names it (src/mech3/credits.h:90) |
| 0x00422410 | 12 | unlabelled |  |
| 0x00422430 | 10 | folded | as MW3 0x004215c0: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x004225e0 | 74 | unlabelled |  |
| 0x00422940 | 46 | unlabelled |  |
| 0x00422cd0 | 111 | unlabelled |  |
| 0x00423270 | 9 | folded | a thunk: one call to another function |
| 0x00423280 | 9 | folded | a thunk: one call to another function |
| 0x004232a0 | 40 | unlabelled |  |
| 0x00424030 | 80 | folded | the collider's hit records given back to the free list [0x006293bc] after each query (the chains at +0x110..+0x124): memory housekeeping the port's containers do themselves (mech_collide.cpp:401's port) |
| 0x004243f0 | 50 | unlabelled |  |
| 0x00424520 | 89 | unlabelled |  |
| 0x00424710 | 9 | folded | a thunk: one call to another function |
| 0x00424720 | 9 | folded | a thunk: one call to another function |
| 0x00425660 | 8 | folded | returns a constant or a global |
| 0x00425c60 | 20 | folded | checked by test (block trace, 2026-11-13): test_pm_config calls it directly on every loadout and random changes, its answer compared (and under validate 0x00427360, which loadouts survive) |
| 0x00425d80 | 18 | folded | inside its ported callers (0x00426000): a small helper their ports do their own way |
| 0x00425de0 | 18 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:590) |
| 0x00425e40 | 53 | unlabelled |  |
| 0x004262a0 | 22 | unlabelled |  |
| 0x004262f0 | 204 | unlabelled |  |
| 0x00426600 | 33 | folded | checked by test (block trace, 2026-11-13): test_pm_puton runs it (the put rule per row, the events, armour, loadout items and store compared) |
| 0x00426680 | 11 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:371) |
| 0x004266a0 | 11 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:2001) |
| 0x004267f0 | 130 | unlabelled |  |
| 0x00426b80 | 11 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:879) |
| 0x00426bd0 | 52 | unlabelled |  |
| 0x00427020 | 63 | folded | as MW3 0x004251d0: covered where the port does it: src/mech3/mech_config.cpp:698 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00427420 | 12 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:589) |
| 0x004274d0 | 119 | unlabelled |  |
| 0x00427830 | 91 | unlabelled |  |
| 0x004279e0 | 26 | folded | read: container code (a vector's index of a value; a static list head made) |
| 0x00427c40 | 13 | folded | as MW3 0x00425e00: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00427c60 | 11 | folded | as MW3 0x00425e20: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00427e50 | 12 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:938) |
| 0x00427e80 | 45 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x00428000 | 19 | folded | inside its ported callers (0x0044a010): a small helper their ports do their own way |
| 0x00428190 | 21 | folded | as MW3 0x00426440: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x004281c0 | 24 | folded | read: container code inside ported callers (vector copy, insert and reserve, introsort, list clear and find, uninitialised copy) and thunks whose MW3 pair is identical |
| 0x00428200 | 40 | unlabelled |  |
| 0x004282b0 | 9 | folded | covered by the port where it names it (src/mech3/mech_build.h:99) |
| 0x004285d0 | 58 | unlabelled |  |
| 0x00428750 | 11 | folded | read: a this-adjusting thunk (multiple inheritance: the object 0x60 back, its vt+0x18), the compiler's; the port's classes |
| 0x00428760 | 8 | folded | returns a constant or a global |
| 0x00428770 | 8 | folded | returns a constant or a global |
| 0x00428780 | 9 | folded | covered by the port where it names it (src/mech3/mech_control.h:102) |
| 0x00428790 | 10 | folded | as MW3 0x00426b20: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x004287b0 | 9 | folded | a thunk: one call to another function |
| 0x004287d0 | 11 | folded | covered by the port where it names it (src/mech3/mech_control.cpp:508) |
| 0x00428880 | 14 | folded | as MW3 0x00426bf0: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00428940 | 18 | unlabelled |  |
| 0x004289d0 | 13 | unlabelled |  |
| 0x00428a00 | 111 | folded | checked by test (block trace, 2026-11-13): test_pm_mech_build runs it down the mech's constructor chain (every block allocated compared byte by byte, the call traces) |
| 0x00428f10 | 33 | unlabelled |  |
| 0x00428fd0 | 38 | unlabelled |  |
| 0x004290d0 | 31 | folded | covered by the port where it names it (src/mech3/mech_pocket.cpp:59) |
| 0x00429560 | 170 | unlabelled |  |
| 0x004299f0 | 281 | unlabelled |  |
| 0x0042a110 | 13 | folded | ported, by docs/symbols.csv: ported: PlayerControls::set_record (IControls vt+0x88) |
| 0x0042a140 | 11 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:557) |
| 0x0042a1d0 | 8 | folded | covered by the port where it names it (src/mech3/mech_control.h:184) |
| 0x0042a1e0 | 8 | folded | returns a constant or a global |
| 0x0042a1f0 | 8 | folded | returns a constant or a global |
| 0x0042a200 | 8 | folded | returns a constant or a global |
| 0x0042a210 | 8 | folded | returns a constant or a global |
| 0x0042a220 | 8 | folded | returns a constant or a global |
| 0x0042a230 | 9 | folded | covered by the port where it names it (src/mech3/mech_control.cpp:666) |
| 0x0042a240 | 9 | folded | covered by the port where it names it (src/mech3/mech_control.cpp:667) |
| 0x0042a250 | 9 | folded | covered by the port where it names it (src/mech3/mech_control.cpp:657) |
| 0x0042a3e0 | 8 | folded | as MW3 0x004287b0: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0042a5e0 | 66 | unlabelled |  |
| 0x0042a7d0 | 12 | folded | covered by the port where it names it (src/mech3/credits.h:106) |
| 0x0042a800 | 39 | folded | covered by the port where it names it (src/mech3/credits.h:90) |
| 0x0042a8c0 | 42 | unlabelled |  |
| 0x0042a9c0 | 12 | unlabelled |  |
| 0x0042aa70 | 25 | unlabelled |  |
| 0x0042aad0 | 38 | unlabelled |  |
| 0x0042abb0 | 183 | unlabelled |  |
| 0x0042af50 | 19 | folded | covered by the port where it names it (src/mech3/credits.h:34) |
| 0x0042afa0 | 10 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:502) |
| 0x0042afc0 | 8 | folded | returns a constant or a global |
| 0x0042afd0 | 31 | unlabelled |  |
| 0x0042b060 | 19 | folded | covered by the port where it names it (src/mech3/credits.h:34) |
| 0x0042b0a0 | 11 | folded | covered by the port where it names it (src/mech3/credits.h:61) |
| 0x0042b0d0 | 117 | folded | read: a vector's insert of n copies (MW3 0x00429700, the template credits.h:41 names), its loops compiled otherwise; container code, the port's std::vector |
| 0x0042b460 | 9 | folded | as MW3 0x00429a60: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0042b470 | 108 | folded | as MW3 0x00429a70: covered where the port does it: src/mech3/credits.h:41 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0042b700 | 25 | folded | covered by the port where it names it (src/mech3/credits.h:61) |
| 0x0042b760 | 22 | folded | covered by the port where it names it (src/mech3/credits.h:73) |
| 0x0042b7d0 | 46 | unlabelled |  |
| 0x0042b8c0 | 117 | unlabelled |  |
| 0x0042bb50 | 11 | folded | as MW3 0x00429d90: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0042bb80 | 162 | unlabelled |  |
| 0x0042c210 | 12 | folded | covered by the port where it names it (src/mech3/controls_menu.h:122) |
| 0x0042c230 | 48 | unlabelled |  |
| 0x0042c340 | 12 | unlabelled |  |
| 0x0042c360 | 50 | folded | read: the same as MW3's 0x0042b210 but one step taken from a field (one statement each side, the same value) |
| 0x0042c470 | 48 | folded | as MW3 0x0042b320: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042c580 | 48 | folded | as MW3 0x0042b430: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042c690 | 48 | folded | as MW3 0x0042b540: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042c7a0 | 48 | folded | as MW3 0x0042b650: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042c8d0 | 88 | unlabelled |  |
| 0x0042cb50 | 32 | folded | as MW3 0x0042f7a0: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042cbf0 | 12 | unlabelled |  |
| 0x0042cc10 | 12 | unlabelled |  |
| 0x0042cc30 | 12 | unlabelled |  |
| 0x0042cc50 | 12 | unlabelled |  |
| 0x0042cc70 | 12 | unlabelled |  |
| 0x0042cc90 | 12 | unlabelled |  |
| 0x0042ccb0 | 24 | unlabelled |  |
| 0x0042d090 | 139 | unlabelled |  |
| 0x0042d470 | 15 | folded | as MW3 0x0042f450: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d4a0 | 86 | unlabelled |  |
| 0x0042d950 | 23 | folded | covered by the port where it names it (src/mech3/controls_menu.cpp:65) |
| 0x0042dc30 | 23 | folded | covered by the port where it names it (src/mech3/controls_menu.cpp:65) |
| 0x0042dde0 | 23 | folded | covered by the port where it names it (src/mech3/controls_menu.cpp:65) |
| 0x0042e3b0 | 31 | folded | as MW3 0x0042d210: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e460 | 37 | folded | as MW3 0x0042d2b0: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e5d0 | 11 | folded | as MW3 0x0042d420: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e600 | 11 | folded | as MW3 0x0042d450: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e630 | 9 | folded | a thunk: one call to another function |
| 0x0042e650 | 11 | folded | as MW3 0x0042d4a0: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e680 | 28 | folded | covered by the port where it names it (src/mech3/controls_menu.cpp:109) |
| 0x0042e6e0 | 10 | folded | as MW3 0x0042d530: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e700 | 28 | folded | as MW3 0x0042d550: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e760 | 13 | folded | as MW3 0x0042d5b0: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e790 | 12 | folded | as MW3 0x0042d5e0: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e7c0 | 11 | folded | as MW3 0x0042d610: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e7f0 | 41 | unlabelled |  |
| 0x0042e8a0 | 10 | folded | as MW3 0x0042d6f0: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e8c0 | 10 | folded | as MW3 0x0042d710: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e8e0 | 49 | folded | as MW3 0x0042d730: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042ea40 | 16 | folded | covered by the port where it names it (src/mech3/controls_menu.cpp:259) |
| 0x0042ea70 | 16 | folded | covered by the port where it names it (src/mech3/controls_menu.cpp:262) |
| 0x0042eaa0 | 24 | folded | covered by the port where it names it (src/mech3/controls_menu.cpp:240) |
| 0x0042eb00 | 24 | folded | covered by the port where it names it (src/mech3/controls_menu.cpp:176) |
| 0x0042eb70 | 31 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:500) |
| 0x0042ebf0 | 10 | folded | as MW3 0x0042da50: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042ec10 | 9 | unlabelled | follows 0x0048d0b0:  |
| 0x0042f2e0 | 49 | folded | as MW3 0x0042df60: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042f370 | 12 | folded | covered by the port where it names it (src/mech3/controls_menu.h:208) |
| 0x0042f390 | 38 | unlabelled |  |
| 0x0042f5c0 | 58 | unlabelled |  |
| 0x0042f6a0 | 31 | unlabelled |  |
| 0x0042f7c0 | 20 | unlabelled |  |
| 0x0042f800 | 41 | unlabelled |  |
| 0x0042f890 | 64 | unlabelled |  |
| 0x0042fa70 | 14 | unlabelled |  |
| 0x0042fac0 | 9 | folded | a thunk: one call to another function |
| 0x0042fad0 | 9 | folded | a thunk: one call to another function |
| 0x0042fb00 | 12 | unlabelled |  |
| 0x0042fb30 | 34 | unlabelled |  |
| 0x0042fc10 | 58 | folded | group A, checked by the call map and the call counts (the runtime thunks mapped through the imports): every callee called as often as in MW3; only the imports differ |
| 0x0042fd80 | 84 | unlabelled |  |
| 0x00430030 | 32 | unlabelled |  |
| 0x004301b0 | 18 | folded | as MW3 0x00475bf0: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004301e0 | 19 | folded | as MW3 0x00475c50: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00430550 | 30 | folded | group A, read: the same logic: a callee renamed by the map (0x0051da90 = PM 0x00537420), a direct call made virtual (0x0041bd80 through vt+0x168), PM's layout (+0xe8 for +0xe4, a null check added), a destructor on PM's members |
| 0x00430620 | 16 | folded | read: container code (a vector's index of a value; a static list head made) |
| 0x00430650 | 9 | folded | a thunk: one call to another function |
| 0x004306e0 | 16 | unlabelled |  |
| 0x00430710 | 9 | folded | a thunk: one call to another function |
| 0x00430a70 | 60 | unlabelled |  |
| 0x00430c10 | 97 | MISSING LOGIC | read after the feature compare (7f6e9e10) flagged it: pm only, low object detail (the option at 2; PM's mech build sets [0x006293a8] = (0x0040b1e0 == 2), the flag the pm port keeps as lod_table().low): PM's mech data then loads only the mechlib model's variant 1 (0x00430c10, MW3 0x004308a0: both variants, the second's slot cleared) and resamples each gait to 15 keys (0x00431730, MW3's 60) and each action to 10 (0x00431a00, MW3's 20); the port's mech_data_load, load_gait and load_action_motion (mech_data.cpp:354, kGaitFrames 60, kActionFrames 20) are MW3's at every detail. With the other low-detail pieces (0x00492830's) |
| 0x00430e60 | 69 | unlabelled |  |
| 0x00431020 | 22 | folded | checked by test (block trace, 2026-11-13): test_pm_mech_build runs it down the mech's constructor chain (every block allocated compared byte by byte, the call traces) |
| 0x00431080 | 45 | folded | read: MW3's mech data cache 0x00430d10 unpaired by the diff (found by mech id at +0xc or made, 0x234 bytes, its count +8 up, given when its +0x20 is set), the port's mech_data hook (mech_build.cpp:198); PM counts up before the null test and skips MW3's 0x004303c0 first |
| 0x00431120 | 38 | unlabelled |  |
| 0x004311a0 | 12 | unlabelled |  |
| 0x004311c0 | 12 | unlabelled |  |
| 0x004311e0 | 228 | MISSING LOGIC | PM values the shared port keeps as MW3's (pm only): the god camera (mode 3) at 120 m and pitched -81 degrees (-1.413716) in PM, 150 m and -90 degrees in MW3 (player_view.cpp:446-447) |
| 0x00431700 | 12 | unlabelled |  |
| 0x00431c10 | 17 | folded | covered by the port where it names it (src/mech3/mech_data.cpp:244) |
| 0x00431d70 | 16 | unlabelled |  |
| 0x00431da0 | 26 | folded | covered by the port where it names it (src/mech3/mech_data.cpp:333) |
| 0x00431e20 | 182 | unlabelled |  |
| 0x00432a40 | 75 | folded | as MW3 0x004326d0: covered where the port does it: src/mech3/mech_data.cpp:219 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00432cd0 | 11 | folded | covered by the port where it names it (src/mech3/app_vehicle.cpp:557) |
| 0x00432dd0 | 9 | folded | a thunk: one call to another function |
| 0x00432de0 | 9 | folded | a thunk: one call to another function |
| 0x00432e40 | 20 | unlabelled |  |
| 0x004331c0 | 41 | unlabelled |  |
| 0x00433240 | 45 | unlabelled |  |
| 0x00433400 | 39 | folded | covered by the port where it names it (src/mech3/mech.h:238) |
| 0x00433480 | 33 | folded | covered by the port where it names it (src/mech3/mech_parts.cpp:277) |
| 0x00433500 | 45 | unlabelled |  |
| 0x004335c0 | 36 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:827) |
| 0x00433650 | 22 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x00432b60: the mech list dialog's MFC plumbing (constructor, destructor, DDX, OnOK, DoModal) |
| 0x004336b0 | 12 | unlabelled |  |
| 0x004336d0 | 12 | unlabelled |  |
| 0x004336f0 | 20 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x00432be0: the mech list dialog's MFC plumbing (constructor, destructor, DDX, OnOK, DoModal) |
| 0x00433740 | 9 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x00432c30: the mech list dialog's MFC plumbing (constructor, destructor, DDX, OnOK, DoModal) |
| 0x00433760 | 8 | folded | returns a constant or a global |
| 0x00433770 | 34 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x00432c60: a mech list dialog (resource 0x9c, from a debug key command in 0x0040db60): its fill of every mech name (0x00484650) |
| 0x00433840 | 26 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x00432d30: a mech list dialog (resource 0x9c, from a debug key command in 0x0040db60): its fill of every mech name (0x00484650) |
| 0x004338d0 | 10 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x0040be50: MFC dev dialogs' plumbing: resource 0x6a (a dialog of float fields: constructor, DDX, close, DoModal), the teleport dialog's slider (OnHScroll) and destructor, a combo box's pick into the texture memory setting |
| 0x004338f0 | 13 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x00432df0: the mech list dialog's MFC plumbing (constructor, destructor, DDX, OnOK, DoModal) |
| 0x00433960 | 13 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:2049) |
| 0x00433980 | 15 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:2049) |
| 0x00433a70 | 11 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:399) |
| 0x00433ab0 | 15 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:590) |
| 0x00433af0 | 13 | folded | checked by test (block trace, 2026-11-13): test_pm_mechsave's Load 0x00426000 runs it on every saved loadout, damaged ones too; the loaded loadout compared |
| 0x00433b90 | 165 | unlabelled |  |
| 0x00434370 | 14 | folded | as MW3 0x004338a0: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x004343a0 | 14 | folded | covered by the port where it names it (src/mech3/app_force.cpp:25) |
| 0x004343d0 | 14 | folded | covered by the port where it names it (src/mech3/app_force.cpp:29) |
| 0x00434400 | 14 | folded | as MW3 0x00433930: ported in force_feedback.cpp:172 (water: wading) and app_sensors.cpp:141 (a vehicle sees its target), whose hook fields are allowed null only in tests |
| 0x00434430 | 16 | folded | covered by the port where it names it (src/mech3/force_feedback.cpp:58) |
| 0x00434460 | 11 | folded | as MW3 0x00433990: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00434470 | 41 | unlabelled |  |
| 0x00434540 | 14 | folded | as MW3 0x00433a70: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00434580 | 33 | folded | as MW3 0x00433ab0: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00434600 | 15 | folded | as MW3 0x00433b30: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00434760 | 12 | folded | covered by the port where it names it (src/mech3/pilot.cpp:280) |
| 0x004347b0 | 18 | folded | covered by the port where it names it (src/mech3/pilot_games.cpp:813) |
| 0x00434980 | 9 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x00434ab0 | 22 | folded | checked by test (block trace, 2026-11-13): test_pm_pilot and test_pm_pilot_games run it (the pilot file byte for byte; every pilot, game, store, item and slot dumped and compared) |
| 0x00434b10 | 25 | folded | checked by test (block trace, 2026-11-13): test_pm_pilot's write 0x0047b610 runs it; the written pilot file compared byte for byte |
| 0x00434b70 | 79 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x00434d70 | 30 | folded | covered by the port where it names it (src/mech3/pilot_games.cpp:802) |
| 0x00434df0 | 18 | unlabelled |  |
| 0x00434e20 | 102 | unlabelled |  |
| 0x004350c0 | 30 | unlabelled |  |
| 0x00435140 | 26 | unlabelled |  |
| 0x004353c0 | 8 | unlabelled |  |
| 0x004353d0 | 8 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:2444) |
| 0x004353e0 | 8 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:2444) |
| 0x004353f0 | 12 | unlabelled |  |
| 0x00435410 | 38 | folded | group A, read: the campaign game's destructor on PM's layout (its string list and members freed through PM's helpers 0x00435d10, 0x00434ab0, 0x004731b0); pilot_games.cpp:583 |
| 0x004354e0 | 42 | folded | group A, read: imports only (the C runtime's), the logic as the shared port |
| 0x004355d0 | 17 | folded | covered by the port where it names it (src/mech3/mission.cpp:93) |
| 0x00435680 | 10 | unlabelled |  |
| 0x004356c0 | 14 | MISSING LOGIC (only data the shipped game lacks reaches it) | pm only, campaign: an objective's persist name is kept across missions and gates mechs. On completion (0x0045fcf0) PM adds the objective's persist (+0x30; the port reads it, mech_objective.cpp:436, and never uses it) to the campaign's string list (+0xc4, the port's Campaign::strings, read and written with the save, pilot.h:343), and the vehicle loader (0x004a19c0, MW3 0x0049e810, player.cpp) makes a mech whose record has flag bit 4 (p12's second value, zrdraiv.cpp:381) only when its name is in that list (0x004356c0). The port does neither: it makes every such mech. No shipped PM objective has persist (raw search of the reader archives; MW3's c1 has two). Checked (2026-11-13): of the 424 p12 records in every shipped PM aiv.zrd (campaign, standalone and Instant Action), none sets bit 4 (99 set bit 5, the field after it), so no shipped mission is gated; only a mod with persist objectives and bit-4 placements would show the difference |
| 0x00435990 | 14 | unlabelled |  |
| 0x00435ce0 | 15 | unlabelled |  |
| 0x00435d00 | 9 | folded | ported, by docs/symbols.csv: ported: set_lancemate_count (+0x9c) |
| 0x00435d10 | 9 | folded | a thunk: one call to another function |
| 0x00435d20 | 9 | folded | read: a wrapper of 0x00434df0 (inline where MW3 calls it) |
| 0x00435d30 | 20 | folded | covered by the port where it names it (src/mech3/pilot_games.cpp:841) |
| 0x00435db0 | 8 | folded | returns a constant or a global |
| 0x00435e80 | 67 | unlabelled |  |
| 0x00435fe0 | 8 | folded | covered by the port where it names it (src/mech3/pilot.h:562) |
| 0x00435ff0 | 9 | replaced (SDL or the platform layer) | follows 0x00596e50: as MW3 0x00576040, whose class PM's changes keep: Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00436000 | 8 | folded | covered by the port where it names it (src/mech3/pilot.h:558) |
| 0x00436010 | 8 | unlabelled |  |
| 0x00436020 | 8 | folded | returns a constant or a global |
| 0x00436030 | 12 | unlabelled |  |
| 0x00436050 | 61 | folded | group A, read: a destructor; PM's one more member (0x004731b0) is a container the port's member frees itself |
| 0x004365f0 | 8 | folded | covered by the port where it names it (src/mech3/pilot.h:666) |
| 0x004366f0 | 14 | folded | covered by the port where it names it (src/mech3/pilot_games.cpp:1837) |
| 0x00437990 | 14 | folded | covered by the port where it names it (src/mech3/app_mech.h:179) |
| 0x004379f0 | 9 | folded | a thunk: one call to another function |
| 0x00437b90 | 26 | unlabelled |  |
| 0x00437c10 | 12 | unlabelled |  |
| 0x00437c30 | 8 | folded | returns a constant or a global |
| 0x00437c40 | 8 | folded | returns a constant or a global |
| 0x00437c50 | 8 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:2463) |
| 0x00437c60 | 9 | replaced (SDL or the platform layer) | follows 0x00596e50: as MW3 0x00576040, whose class PM's changes keep: Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00437c70 | 12 | folded | covered by the port where it names it (src/mech3/app_mech.h:179) |
| 0x00437c90 | 12 | folded | covered by the port where it names it (src/mech3/app_mech.h:180) |
| 0x00437cb0 | 9 | folded | a thunk: one call to another function |
| 0x00437cc0 | 12 | unlabelled |  |
| 0x00437ce0 | 35 | folded | group A, read: imports only (the C runtime's), the logic as the shared port |
| 0x00437ea0 | 33 | folded | covered by the port where it names it (src/mech3/pilot.h:694) |
| 0x00437f20 | 94 | unlabelled |  |
| 0x00437f5a | 85 | unlabelled | follows 0x00437f20:  |
| 0x004381a0 | 12 | folded | as MW3 0x004373b0: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004381d0 | 30 | folded | group A, read: PM's mech lab line-up (the player's slot 5 where MW3 has 3, line-up mech 5 only where MW3 sets 3, 8 and 13, the rest -1) is in the pm port: pilot_games.cpp:536 and 553 |
| 0x00438280 | 9 | folded | covered by the port where it names it (src/mech3/pilot.h:27) |
| 0x00438290 | 8 | folded | as MW3 0x004487d0: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004382a0 | 12 | unlabelled |  |
| 0x00438340 | 8 | folded | returns a constant or a global |
| 0x00438350 | 14 | folded | as MW3 0x00437550: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00438790 | 20 | folded | inside its ported callers (0x00438580): a small helper their ports do their own way |
| 0x004387f0 | 9 | unlabelled | follows 0x004731b0:  |
| 0x00438800 | 11 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:233) |
| 0x00438820 | 140 | folded | read: container code inside ported callers (vector copy, insert and reserve, introsort, list clear and find, uninitialised copy) and thunks whose MW3 pair is identical |
| 0x00438ac0 | 75 | folded | as MW3 0x004375e0: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00438bf0 | 15 | unlabelled |  |
| 0x00438c10 | 128 | unlabelled |  |
| 0x004390a0 | 18 | unlabelled |  |
| 0x004390e0 | 21 | unlabelled |  |
| 0x00439260 | 151 | unlabelled |  |
| 0x004396a0 | 12 | unlabelled |  |
| 0x004396c0 | 53 | unlabelled |  |
| 0x00439820 | 12 | unlabelled |  |
| 0x00439840 | 53 | unlabelled |  |
| 0x00439910 | 12 | unlabelled |  |
| 0x00439930 | 26 | unlabelled |  |
| 0x004399a0 | 9 | unlabelled | follows 0x005b1bd0:  |
| 0x004399c0 | 9 | unlabelled | follows 0x005b1bd0:  |
| 0x004399e0 | 9 | unlabelled | follows 0x005b1db0:  |
| 0x00439a20 | 11 | folded | inside its ported callers (0x0043a060): a small helper their ports do their own way |
| 0x00439a30 | 131 | unlabelled |  |
| 0x00439e60 | 14 | unlabelled |  |
| 0x00439ee0 | 11 | unlabelled |  |
| 0x00439f10 | 10 | folded | covered by the port where it names it (src/mech3/options_menu.cpp:271) |
| 0x0043a9a0 | 112 | folded | the graphics screen's DETAIL selection, inline in MW3's 0x004395c0: object detail 0..2 to the list's value 2..0, each item of that value selected (options_menu.cpp:489, select_each_value(level_of(lod))); PM also selects a missing item for detail 2, which no list has; not run under a test (trace) |
| 0x0043adb0 | 12 | folded | covered by the port where it names it (src/mech3/options_menu.h:198) |
| 0x0043add0 | 53 | folded | as MW3 0x00439ce0: covered where the port does it: src/mech3/options_menu.h:198 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0043af30 | 12 | unlabelled |  |
| 0x0043af50 | 12 | unlabelled |  |
| 0x0043af70 | 108 | unlabelled |  |
| 0x0043b1e0 | 71 | unlabelled |  |
| 0x0043b2d0 | 71 | MISSING LOGIC | pm only, minor, group A, read: the TEXTURES cycle offers the same items by the same bits and card memory, but selects differently: PM selects the screen's stored value per device (app +200, +0x14150 for software, +0x14154 + device*4 for a card) and a card past index 3 first selects 2 MB, with no fallback, so an unset (0) value leaves the selection where it was; MW3 and the port select the last item added (or the card's megabytes), then TextureMemory if set (options_menu.cpp:326-360) (MW3 0x0043a310) |
| 0x0043b450 | 71 | unlabelled |  |
| 0x0043b6f0 | 32 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:475) |
| 0x0043b770 | 12 | unlabelled |  |
| 0x0043b790 | 11 | unlabelled |  |
| 0x0043b7c0 | 16 | unlabelled |  |
| 0x0043b880 | 10 | folded | as MW3 0x00448780: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043b8d0 | 12 | unlabelled |  |
| 0x0043b8f0 | 22 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:59) |
| 0x0043bb40 | 12 | folded | as MW3 0x0043aae0: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043bb70 | 59 | unlabelled |  |
| 0x0043be70 | 10 | folded | inside its ported callers (0x0043e590, 0x0043e9a0, 0x0043f220, 0x00444680): a small helper their ports do their own way |
| 0x0043be90 | 15 | folded | as MW3 0x0043ae30: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043bf50 | 9 | folded | a thunk: one call to another function |
| 0x0043bf70 | 9 | folded | a thunk: one call to another function |
| 0x0043bf90 | 9 | folded | a thunk: one call to another function |
| 0x0043bfb0 | 9 | folded | a thunk: one call to another function |
| 0x0043bfd0 | 9 | folded | a thunk: one call to another function |
| 0x0043bff0 | 9 | folded | a thunk: one call to another function |
| 0x0043c010 | 9 | folded | a thunk: one call to another function |
| 0x0043c030 | 9 | folded | follows 0x0043be90: as MW3 0x0043ae30: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043c210 | 99 | folded | group A, read: PM's damage view backgrounds, covered: the shared HUD loads dmg_bg_blue, red and grn (hud.cpp:118-120) and the panels draw them (hud.cpp:1082, 1315-1327); TARGET's second text with PM's lancemate rule and message 0xdd4 (hud.cpp:1305-1311) |
| 0x0043c5c0 | 23 | folded | as MW3 0x0043b530: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043c630 | 38 | folded | as MW3 0x0043b5a0: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043c720 | 22 | unlabelled |  |
| 0x0043c790 | 47 | unlabelled |  |
| 0x0043c8a0 | 21 | unlabelled |  |
| 0x0043c900 | 86 | folded | group A, read: the same logic as the shared port: a destructor swapped for PM's class layout (0x0040a570 for 0x0043a890); the show and hide set the panel's object active through its vtable (vt+0xc) where MW3 calls node_set_active 0x004eb170 directly |
| 0x0043cf50 | 57 | MISSING LOGIC | PM's HUD ZAPS static (MW3 0x0043be70's version, the hit's static on a HUD channel): unported, and its hook is the census stand-in DamageCalls.zaps_hud, as in MW3 (docs/port-audit.md, BROKEN row 3) |
| 0x0043d2b0 | 64 | unlabelled |  |
| 0x0043d710 | 55 | unlabelled |  |
| 0x0043d880 | 116 | unlabelled |  |
| 0x0043dbf0 | 45 | debug or dev tool: PORT (preserve) | as MW3 0x0043cb10: the HUD's debug overlays (docs/hud.md section 11: the mech's gait, speeds and the like into five debug texts while the debug text shows, command 0x82); hooked in the port (hud_debug, unported) |
| 0x0043dcf0 | 39 | debug or dev tool: PORT (preserve) | as MW3 0x0043cc00: the HUD's debug overlays (docs/hud.md section 11: the mech's gait, speeds and the like into five debug texts while the debug text shows, command 0x82); hooked in the port (hud_debug, unported) |
| 0x0043e570 | 9 | folded | a thunk: one call to another function |
| 0x0043e580 | 9 | folded | a thunk: one call to another function |
| 0x0043e590 | 128 | unlabelled |  |
| 0x0043e9a0 | 22 | unlabelled |  |
| 0x0043ea10 | 12 | unlabelled |  |
| 0x0043ea30 | 12 | folded | covered by the port where it names it (src/mech3/options_menu.h:66) |
| 0x0043ec60 | 42 | folded | as MW3 0x0043d810: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043f200 | 11 | folded | as MW3 0x0043dce0: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00440ac0 | 33 | MISSING LOGIC | as MW3 0x0043f2d0: the radar's map-mode position (HudRadar 0x0043f2d0): stores the mech's position and, with the mission map shown, moves the mech's marker line on the map (0x00491c80, 0x005a6880); not ported, and its hook radar_heading (hud.h:880) is never overridden: see BROKEN |
| 0x00440cc0 | 12 | unlabelled |  |
| 0x00440d70 | 22 | unlabelled |  |
| 0x00440e30 | 12 | unlabelled |  |
| 0x00441080 | 12 | unlabelled |  |
| 0x004411d0 | 27 | folded | group A, read: the same logic as the shared port: a destructor swapped for PM's class layout (0x0040a570 for 0x0043a890); the show and hide set the panel's object active through its vtable (vt+0xc) where MW3 calls node_set_active 0x004eb170 directly |
| 0x00441250 | 11 | folded | as MW3 0x0043fa20: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00441440 | 16 | folded | group A, read: the same logic as the shared port: a destructor swapped for PM's class layout (0x0040a570 for 0x0043a890); the show and hide set the panel's object active through its vtable (vt+0xc) where MW3 calls node_set_active 0x004eb170 directly |
| 0x00441490 | 16 | folded | group A, read: the same logic as the shared port: a destructor swapped for PM's class layout (0x0040a570 for 0x0043a890); the show and hide set the panel's object active through its vtable (vt+0xc) where MW3 calls node_set_active 0x004eb170 directly |
| 0x004414d0 | 12 | folded | as MW3 0x0043fca0: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00441900 | 12 | unlabelled |  |
| 0x00441ad0 | 84 | unlabelled |  |
| 0x00441d20 | 188 | unlabelled |  |
| 0x004422f0 | 356 | unlabelled |  |
| 0x00442f10 | 26 | unlabelled |  |
| 0x00442fd0 | 180 | folded | group A, read: PM's damage view backgrounds, covered: the shared HUD loads dmg_bg_blue, red and grn (hud.cpp:118-120) and the panels draw them (hud.cpp:1082, 1315-1327); TARGET's second text with PM's lancemate rule and message 0xdd4 (hud.cpp:1305-1311) |
| 0x00443520 | 8 | folded | as MW3 0x00441d90: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00443530 | 83 | folded | group A, read: PM's damage view backgrounds, covered: the shared HUD loads dmg_bg_blue, red and grn (hud.cpp:118-120) and the panels draw them (hud.cpp:1082, 1315-1327); TARGET's second text with PM's lancemate rule and message 0xdd4 (hud.cpp:1305-1311) |
| 0x00443700 | 12 | unlabelled |  |
| 0x00443720 | 35 | folded | as MW3 0x00441f90: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004437f0 | 84 | folded | group A, read: PM's damage view backgrounds, covered: the shared HUD loads dmg_bg_blue, red and grn (hud.cpp:118-120) and the panels draw them (hud.cpp:1082, 1315-1327); TARGET's second text with PM's lancemate rule and message 0xdd4 (hud.cpp:1305-1311) |
| 0x00443f40 | 12 | unlabelled |  |
| 0x00444000 | 12 | folded | as MW3 0x00442850: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004442d0 | 16 | folded | as MW3 0x00448600: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00444540 | 12 | unlabelled |  |
| 0x00444560 | 40 | folded | as MW3 0x00442d00: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00444640 | 9 | folded | covered by the port where it names it (src/mech3/hud.cpp:1811) |
| 0x00444680 | 44 | unlabelled |  |
| 0x004447a0 | 204 | unlabelled |  |
| 0x00444f40 | 65 | unlabelled |  |
| 0x00445110 | 10 | folded | inside its ported callers (0x00445a50): a small helper their ports do their own way |
| 0x00445560 | 12 | unlabelled |  |
| 0x00445580 | 12 | folded | inside its ported callers (0x00442f10, 0x00445b50, 0x00447da0): a small helper their ports do their own way |
| 0x004455b0 | 9 | folded | a thunk: one call to another function |
| 0x00445c40 | 11 | folded | as MW3 0x00444330: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00446060 | 14 | folded | as MW3 0x00444670: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00446080 | 8 | folded | returns a constant or a global |
| 0x004467b0 | 12 | folded | as MW3 0x00448530: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00446870 | 12 | unlabelled |  |
| 0x00447030 | 27 | unlabelled |  |
| 0x00447200 | 19 | unlabelled |  |
| 0x00447480 | 12 | unlabelled |  |
| 0x00447840 | 29 | unlabelled |  |
| 0x00447ae0 | 44 | unlabelled |  |
| 0x00447c00 | 12 | unlabelled |  |
| 0x00447c20 | 20 | unlabelled |  |
| 0x00447c80 | 36 | unlabelled |  |
| 0x00447da0 | 98 | unlabelled |  |
| 0x00448040 | 9 | folded | covered by the port where it names it (src/mech3/app_hud.cpp:903) |
| 0x00448050 | 21 | unlabelled |  |
| 0x00448230 | 26 | unlabelled |  |
| 0x004483b0 | 12 | unlabelled |  |
| 0x00448d50 | 12 | unlabelled |  |
| 0x00448d70 | 10 | folded | as MW3 0x00447530: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00448da0 | 21 | folded | as MW3 0x00447560: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00449040 | 12 | unlabelled |  |
| 0x00449060 | 68 | folded | as MW3 0x00447820: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00449430 | 8 | folded | covered by the port where it names it (src/mech3/hud.h:413) |
| 0x004495d0 | 12 | unlabelled |  |
| 0x004495f0 | 23 | folded | as MW3 0x00447da0: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00449890 | 29 | unlabelled |  |
| 0x00449940 | 12 | unlabelled |  |
| 0x00449960 | 34 | unlabelled |  |
| 0x00449a20 | 48 | unlabelled |  |
| 0x00449b30 | 52 | unlabelled |  |
| 0x00449c50 | 23 | unlabelled |  |
| 0x00449cb0 | 15 | folded | as MW3 0x004484b0: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00449d00 | 19 | unlabelled |  |
| 0x00449d50 | 9 | unlabelled | follows 0x00444540:  |
| 0x00449d60 | 9 | unlabelled | follows 0x00441080:  |
| 0x00449d70 | 9 | folded | a thunk: one call to another function |
| 0x00449d90 | 9 | folded | a thunk: one call to another function |
| 0x00449da0 | 9 | folded | a thunk: one call to another function |
| 0x00449e50 | 26 | folded | covered by the port where it names it (src/mech3/app_states.h:253) |
| 0x00449ea0 | 12 | folded | covered by the port where it names it (src/mech3/mech_lab.cpp:24) |
| 0x0044a010 | 109 | unlabelled |  |
| 0x0044a280 | 31 | unlabelled |  |
| 0x0044a320 | 17 | folded | read with its caller, the lab state's update (0x0044a010, MW3 0x00448a80, mech_lab.cpp, ported with PM's changes at 0x0044a127 / 0x0044a16f): the lab's two 2D layers drawn, the surface locked round them in software; the port's draw |
| 0x0044a4c0 | 24 | folded | as MW3 0x004490d0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044a550 | 12 | unlabelled |  |
| 0x0044a660 | 147 | unlabelled |  |
| 0x0044a9a0 | 15 | folded | as MW3 0x0044ed90: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044a9d0 | 55 | folded | as MW3 0x00449570: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044abb0 | 139 | folded | read: the same as MW3's 0x00449760, a field 8 bytes on in PM's lab screen (+0x4d4 for +0x4cc) |
| 0x0044b770 | 174 | unlabelled |  |
| 0x0044bb60 | 12 | unlabelled |  |
| 0x0044bb80 | 20 | folded | as MW3 0x0044a7f0: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0044bdb0 | 32 | unlabelled |  |
| 0x0044bea0 | 12 | unlabelled |  |
| 0x0044bfd0 | 28 | folded | as MW3 0x0044ee00: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044c050 | 77 | unlabelled |  |
| 0x0044c3d0 | 13 | folded | as MW3 0x0044b010: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044c410 | 12 | unlabelled |  |
| 0x0044c5e0 | 10 | folded | as MW3 0x0044b210: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044c600 | 12 | folded | covered by the port where it names it (src/mech3/menu_states.h:20) |
| 0x0044c890 | 13 | folded | as MW3 0x0044ec90: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044c8c0 | 10 | folded | as MW3 0x0044ecc0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044c8d0 | 8 | folded | as MW3 0x0044ecd0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044c8e0 | 20 | folded | as MW3 0x0044ece0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044c930 | 74 | unlabelled |  |
| 0x0044cec0 | 277 | unlabelled |  |
| 0x0044d8a0 | 12 | folded | as MW3 0x0044ed40: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044d8d0 | 78 | unlabelled |  |
| 0x0044da50 | 9 | folded | a thunk: one call to another function |
| 0x0044da60 | 12 | folded | covered by the port where it names it (src/mech3/lab_screen.h:465) |
| 0x0044da80 | 20 | folded | as MW3 0x0044f1f0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044daf0 | 13 | folded | as MW3 0x0044f260: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044db20 | 12 | folded | as MW3 0x0044f290: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044db50 | 9 | unlabelled | follows 0x005bdb30:  |
| 0x0044dbb0 | 15 | unlabelled |  |
| 0x0044dce0 | 12 | unlabelled |  |
| 0x0044dd00 | 12 | unlabelled |  |
| 0x0044ddf0 | 12 | unlabelled |  |
| 0x0044de10 | 12 | unlabelled |  |
| 0x0044de30 | 12 | unlabelled |  |
| 0x0044de50 | 84 | unlabelled |  |
| 0x0044e060 | 64 | folded | as MW3 0x0044f890: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e1e0 | 12 | unlabelled |  |
| 0x0044e200 | 28 | folded | as MW3 0x0044fa30: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e280 | 10 | folded | as MW3 0x0044fab0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e2a0 | 13 | folded | as MW3 0x0044fad0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e2d0 | 44 | folded | as MW3 0x0044fb00: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e3d0 | 12 | unlabelled |  |
| 0x0044e3f0 | 12 | unlabelled |  |
| 0x0044e410 | 28 | folded | as MW3 0x0044fc40: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e490 | 12 | unlabelled |  |
| 0x0044e4c0 | 32 | folded | as MW3 0x0044c320: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e570 | 73 | folded | as MW3 0x0044c3d0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e740 | 28 | folded | as MW3 0x0044f5f0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e7e0 | 12 | folded | covered by the port where it names it (src/mech3/lab_screen.h:273) |
| 0x0044e800 | 12 | folded | covered by the port where it names it (src/mech3/lab_screen.h:273) |
| 0x0044e820 | 139 | unlabelled |  |
| 0x0044ec40 | 39 | folded | as MW3 0x0044ef90: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044ed20 | 150 | unlabelled |  |
| 0x0044f140 | 39 | unlabelled |  |
| 0x0044f1f0 | 9 | folded | a thunk: one call to another function |
| 0x0044f740 | 295 | unlabelled |  |
| 0x0044fed0 | 21 | folded | as MW3 0x0044edc0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044ff10 | 16 | unlabelled |  |
| 0x00450280 | 119 | unlabelled |  |
| 0x004505b0 | 12 | folded | covered by the port where it names it (src/mech3/lab_screen.h:259) |
| 0x004505d0 | 20 | folded | covered by the port where it names it (src/mech3/lab_screen.h:259) |
| 0x004506a0 | 21 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:326) |
| 0x004506e0 | 18 | folded | as MW3 0x0044e490: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00450810 | 9 | folded | a thunk: one call to another function |
| 0x00450830 | 22 | folded | as MW3 0x0044e5e0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x004508d0 | 9 | folded | as MW3 0x0044e680: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x004508f0 | 9 | folded | as MW3 0x0044e6a0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00450910 | 8 | folded | returns a constant or a global |
| 0x00450920 | 12 | folded | covered by the port where it names it (src/mech3/lab_screen.h:423) |
| 0x00450a20 | 13 | folded | as MW3 0x0042f3a0: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x00450a60 | 24 | folded | as MW3 0x0044e7b0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00450ab0 | 129 | folded | as MW3 0x0044e800: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00450d10 | 56 | folded | as MW3 0x0044ea70: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00450df0 | 82 | folded | as MW3 0x0044eb30: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00450f40 | 13 | folded | ported, by docs/symbols.csv: ported: the entry (new(0x14)), +0x08 the mech, +0x10 -1 |
| 0x00450f60 | 21 | folded | ported, by docs/symbols.csv: ported in lab_world_unload: the tree and the camo |
| 0x00451110 | 50 | folded | ported, by docs/symbols.csv: ported inline in lab_place_window: the window wrapper's area, size and place |
| 0x00451730 | 47 | MISSING LOGIC | pm only, small, PM's own quirk (port as written, a discoveries.md line): the mech lab's part highlight; in PM the torso parts (1, 7, 8) light the head green with the torso (weight 0.3) and the head (6) lights nothing, where MW3 and the port light the head only for 6 (lab_view.cpp:272-296). PM numbers the parts as MW3 does: its list fill 0x00456760 adds 6 first with the cockpit, life support and sensors texts (0x24e..0x250), then 1, 7, 8, 2, 3, 4, 5, so PM's 6 is the head (MW3 0x004504d0) |
| 0x004518e0 | 45 | unlabelled |  |
| 0x00451e50 | 22 | folded | as MW3 0x004513f0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00452770 | 12 | unlabelled |  |
| 0x00452790 | 27 | unlabelled |  |
| 0x004528e0 | 92 | folded | as MW3 0x004516a0: covered where the port does it: src/mech3/lab_items.cpp:64 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00452ef0 | 12 | unlabelled |  |
| 0x00453530 | 10 | folded | covered by the port where it names it (src/mech3/lab_items.cpp:125) |
| 0x00453550 | 12 | unlabelled |  |
| 0x00453570 | 12 | unlabelled |  |
| 0x00453590 | 28 | folded | as MW3 0x00456880: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00453610 | 28 | folded | as MW3 0x00456900: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00453690 | 32 | unlabelled |  |
| 0x00454160 | 40 | folded | as MW3 0x00452de0: covered where the port does it: src/mech3/lab_screen.cpp:842 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00454380 | 65 | unlabelled |  |
| 0x00454820 | 19 | folded | as MW3 0x00453500: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00454860 | 30 | unlabelled |  |
| 0x00454af0 | 12 | unlabelled |  |
| 0x00454b49 | 24 | unlabelled | follows 0x00454af0:  |
| 0x00454b80 | 12 | unlabelled |  |
| 0x00454ba0 | 8 | folded | returns a constant or a global |
| 0x00454bb0 | 8 | folded | as MW3 0x00453820: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00454bc0 | 9 | folded | as MW3 0x00453830: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00455059 | 80 | folded | follows 0x00454bc0: as MW3 0x00453830: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00455200 | 38 | unlabelled |  |
| 0x004552b0 | 254 | unlabelled |  |
| 0x004565a0 | 10 | folded | as MW3 0x004552e0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x004565c0 | 12 | unlabelled |  |
| 0x004565e0 | 9 | folded | a thunk: one call to another function |
| 0x00456600 | 9 | folded | a thunk: one call to another function |
| 0x00456620 | 26 | folded | as MW3 0x00455360: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00456cb0 | 42 | unlabelled |  |
| 0x00457160 | 9 | folded | a thunk: one call to another function |
| 0x00457240 | 11 | folded | as MW3 0x00455f90: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00457450 | 142 | folded | as MW3 0x004561a0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00457860 | 78 | folded | as MW3 0x004565f0: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00457a40 | 12 | unlabelled |  |
| 0x00457c10 | 33 | unlabelled |  |
| 0x00457cb0 | 91 | unlabelled |  |
| 0x00457ff0 | 12 | unlabelled |  |
| 0x00458010 | 38 | unlabelled |  |
| 0x00458170 | 9 | folded | a thunk: one call to another function |
| 0x004581b0 | 63 | unlabelled |  |
| 0x00458690 | 79 | unlabelled |  |
| 0x00458900 | 21 | folded | as MW3 0x00458010: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00458960 | 12 | unlabelled |  |
| 0x00458980 | 36 | folded | as MW3 0x00457880: the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00459010 | 43 | unlabelled |  |
| 0x00459110 | 74 | folded | checked by test (block trace, 2026-11-13): test_pm_lab_pick, test_pm_lobby_lines and test_pm_puton run it (the firepoint picked, the lines, the put's results compared) |
| 0x004592c0 | 9 | folded | a thunk: one call to another function |
| 0x004592d0 | 9 | folded | a thunk: one call to another function |
| 0x00459300 | 43 | unlabelled |  |
| 0x00459400 | 9 | folded | a thunk: one call to another function |
| 0x00459410 | 12 | folded | covered by the port where it names it (src/mech3/mission_map.h:27) |
| 0x00459940 | 36 | unlabelled |  |
| 0x00459b20 | 13 | folded | read: MW3's 0x004589a0 unpaired by the diff (the dock arm's +0x88 set to 1 or 0 by the flag), the same; called where MW3 calls it in the dock step 0x0045a990 (MW3 0x004598d0, mech_fieldbase_dock.cpp:123) |
| 0x00459b50 | 35 | unlabelled |  |
| 0x00459cc0 | 8 | folded | covered by the port where it names it (src/mech3/app_vehicle.h:136) |
| 0x00459cd0 | 11 | unlabelled |  |
| 0x00459d10 | 25 | folded | as MW3 0x00458b50: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0045a250 | 209 | MISSING LOGIC | pm only, field bases: when a field base (MCB) dies, PM first counts the bases still alive (0x0045b0d0) and, with none left, aborts the dock in progress (0x0045b320, the front state's abort, the port's FieldBases::abort_front); then the campaign's cut as MW3's (the base count by the campaign's vt+0x5c, the share 1 / 0.7 / 0.5, 2 mechs and 300 tons kept per base left). The port's field_base_die (mech_fieldbase.cpp:316) has the death and the cut but no abort, so on PM the dock in progress when the last base dies is not aborted (what the abort undoes: mech_fieldbase_dock.cpp:196) |
| 0x0045a930 | 12 | folded | ported, by docs/symbols.csv: ported (vtable 0x005c2540) |
| 0x0045a950 | 8 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.h:81) |
| 0x0045a980 | 9 | folded | covered by the port where it names it (src/mech3/mech_fieldbase_dock.cpp:50) |
| 0x0045a990 | 41 | unlabelled |  |
| 0x0045ab90 | 21 | unlabelled |  |
| 0x0045abf0 | 9 | folded | read: container code inside ported callers (vector copy, insert and reserve, introsort, list clear and find, uninitialised copy) and thunks whose MW3 pair is identical |
| 0x0045ac00 | 9 | folded | a thunk: one call to another function |
| 0x0045ac20 | 102 | folded | read: the dock registry's start-up (MW3 0x00459b80, mech_fieldbase.cpp:46) with PM's refit state built in (0x0045bc50, the port's DockRefit, FieldBases::refit); the same sound names |
| 0x0045ae80 | 8 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.h:34) |
| 0x0045ae90 | 8 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.h:61) |
| 0x0045aea0 | 34 | folded | as MW3 0x00459db0: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0045af30 | 8 | folded | inside its ported callers (0x0045b0d0): a small helper their ports do their own way |
| 0x0045b0d0 | 22 | unlabelled |  |
| 0x0045b100 | 14 | MISSING LOGIC | pm only, field bases: when a field base (MCB) dies, PM first counts the bases still alive (0x0045b0d0) and, with none left, aborts the dock in progress (0x0045b320, the front state's abort, the port's FieldBases::abort_front); then the campaign's cut as MW3's (the base count by the campaign's vt+0x5c, the share 1 / 0.7 / 0.5, 2 mechs and 300 tons kept per base left). The port's field_base_die (mech_fieldbase.cpp:316) has the death and the cut but no abort, so on PM the dock in progress when the last base dies is not aborted (what the abort undoes: mech_fieldbase_dock.cpp:196) |
| 0x0045b240 | 44 | MISSING LOGIC | pm only, multiplayer: PM's dock registry frame (0x0045b240, from the mission frame 0x004921a0, in MW3's 0x0045a270's place) runs the dock states in every game type and has no multiplayer branch; PM checks the deployment in its refit instead (0x0045be50, ported: DockRefit::may_start). The port's FieldBases::frame (mech_fieldbase_dock.cpp:329) is MW3's on PM too: in multiplayer (type 3) it runs no dock states and, with mobile bases on, MW3's station check and its 'too far from MFB deployment point' line, which PM's frame never does |
| 0x0045b340 | 58 | unlabelled |  |
| 0x0045b540 | 31 | unlabelled |  |
| 0x0045b5d0 | 10 | folded | ported, by docs/symbols.csv: ported |
| 0x0045b9e0 | 13 | folded | as MW3 0x0045ab30: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0045ba20 | 36 | unlabelled |  |
| 0x0045baa0 | 16 | unlabelled |  |
| 0x0045bb30 | 47 | unlabelled |  |
| 0x0045bbf0 | 12 | folded | covered by the port where it names it (src/mech3/mech_net.cpp:1078) |
| 0x0045bc10 | 9 | folded | a thunk: one call to another function |
| 0x0045bc50 | 15 | folded | read: the dock registry's start-up (MW3 0x00459b80, mech_fieldbase.cpp:46) with PM's refit state built in (0x0045bc50, the port's DockRefit, FieldBases::refit); the same sound names |
| 0x0045bcb0 | 9 | unlabelled |  |
| 0x0045c1d0 | 8 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.h:90) |
| 0x0045c290 | 22 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.h:105) |
| 0x0045c2e0 | 72 | unlabelled |  |
| 0x0045c620 | 17 | folded | ported, by docs/symbols.csv: ported |
| 0x0045c720 | 41 | unlabelled |  |
| 0x0045c7d0 | 61 | unlabelled |  |
| 0x0045c920 | 20 | folded | ported, by docs/symbols.csv: ported |
| 0x0045cab0 | 9 | folded | ported, by docs/symbols.csv: ported: DockStep::message (abort, then the message) |
| 0x0045cac0 | 10 | folded | checked by test (block trace, 2026-11-13): test_pm_refit's abort 0x0045c080 runs it; every call out and the dock state's fields compared |
| 0x0045cb10 | 20 | folded | covered by the port where it names it (src/mech3/mech_fieldbase_dock.cpp:298) |
| 0x0045cb40 | 31 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_attack (bit for bit) and test_pm_ai_brain run it |
| 0x0045ccb0 | 12 | unlabelled |  |
| 0x0045ccd0 | 68 | unlabelled |  |
| 0x0045cde0 | 150 | unlabelled |  |
| 0x0045d0d0 | 9 | folded | a thunk: one call to another function |
| 0x0045d0e0 | 12 | unlabelled |  |
| 0x0045d170 | 8 | folded | ported, by docs/symbols.csv: ported: DockStep::done |
| 0x0045d180 | 12 | unlabelled |  |
| 0x0045d1a0 | 12 | unlabelled |  |
| 0x0045d7a0 | 156 | unlabelled |  |
| 0x0045dbd0 | 16 | folded | ported, by docs/symbols.csv: ported; (ECX args) nav by name via [0x00609980] 0x00479be0 |
| 0x0045dc00 | 1548 | unlabelled |  |
| 0x0045f6a0 | 12 | unlabelled |  |
| 0x0045f870 | 58 | folded | as MW3 0x0045f520: covered where the port does it: src/mech3/mech_objective.cpp:202 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0045f980 | 61 | folded | group A, read: the same logic on PM's layout and helpers (the reader's lookup 0x00578630 for 0x00556740, PM's list helpers, the flare's fade with PM's globals, the gravity command, the objective's distance through PM's HUD getter); the call counts differ by how often a getter is called |
| 0x0045ffb0 | 64 | unlabelled |  |
| 0x004601f0 | 14 | unlabelled |  |
| 0x00460270 | 19 | folded | covered by the port where it names it (src/mech3/mech_objective.cpp:235) |
| 0x004602a0 | 19 | folded | covered by the port where it names it (src/mech3/mech_objective.cpp:236) |
| 0x004602d0 | 19 | folded | covered by the port where it names it (src/mech3/mech_objective.cpp:235) |
| 0x00460380 | 19 | folded | covered by the port where it names it (src/mech3/mech_objective.cpp:258) |
| 0x004603d0 | 14 | folded | covered by the port where it names it (src/mech3/mech_objective.cpp:259) |
| 0x00460570 | 109 | unlabelled |  |
| 0x004606f0 | 20 | folded | covered by the port where it names it (src/mech3/mech_objective.h:150) |
| 0x00460730 | 11 | folded | ported, by docs/symbols.csv: ported; the store when the node's +0x24 bit 2 matches |
| 0x00460750 | 20 | folded | covered by the port where it names it (src/mech3/mech_objective.cpp:942) |
| 0x00460b70 | 31 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004bf4e0: the version-105 nets: the editor's reload over them (0x004bed90, the vehicles using the old net kept and handed the new one) and its kept list; not ported on purpose (ai.md 9, 17.9): no shipped 105 file |
| 0x00460bc0 | 31 | folded | ported, by docs/symbols.csv: ported as push_back: std::list node (0xc) before the head, count +1 |
| 0x00460cd0 | 32 | folded | covered by the port where it names it (src/mech3/mech_wake.cpp:21) |
| 0x00460d70 | 12 | unlabelled |  |
| 0x00460dd0 | 25 | unlabelled |  |
| 0x00460e50 | 20 | unlabelled |  |
| 0x00460eb0 | 17 | unlabelled |  |
| 0x004610a0 | 10 | unlabelled |  |
| 0x004610b0 | 8 | folded | returns a constant or a global |
| 0x004610c0 | 8 | folded | returns a constant or a global |
| 0x004610d0 | 8 | folded | returns a constant or a global |
| 0x004610e0 | 8 | folded | returns a constant or a global |
| 0x004610f0 | 8 | folded | returns a constant or a global |
| 0x00461100 | 12 | folded | covered by the port where it names it (src/mech3/mech_parts.cpp:922) |
| 0x004611f0 | 114 | unlabelled |  |
| 0x00461430 | 20 | folded | covered by the port where it names it (src/mech3/app_mech.h:372) |
| 0x004614e0 | 19 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:817) |
| 0x00461510 | 39 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:835) |
| 0x004615b0 | 9 | folded | checked by test (block trace, 2026-11-13): test_pm_parts' hip, leg and part updates run it; the node set (0x00510540: the node and the 12 words of its matrix) logged and compared |
| 0x004617a0 | 237 | unlabelled |  |
| 0x00462090 | 275 | unlabelled |  |
| 0x004628c0 | 54 | folded | group A, read: the same logic; PM looks the owner mech up once at the top (0x00460000's work in place), MW3 at each use; the net event's vtable slot moved (vt+0x168 -> +0x17c) |
| 0x00462a00 | 57 | folded | read: MW3's 0x00461670 (a weapon's heat added when it fires: a timed beam's heat times the frame, else the definition's, with the recoil vt+0x24 for a non-beam), the same branches in another order; the definition at PM's +0x200 for MW3's +0x1f0 |
| 0x00462f30 | 11 | folded | inside its ported callers (0x00462ba0): a small helper their ports do their own way |
| 0x00462f50 | 32 | folded | checked by test (block trace, 2026-11-13): test_pm_damage, test_pm_hit and test_pm_heat run it (the damage done, the parts' state, the heat frame by frame compared) |
| 0x004632f0 | 19 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:961) |
| 0x00463330 | 16 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:777) |
| 0x00463380 | 123 | unlabelled |  |
| 0x00463600 | 20 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:786) |
| 0x00463750 | 12 | unlabelled |  |
| 0x004639f0 | 43 | unlabelled |  |
| 0x00463e70 | 12 | unlabelled |  |
| 0x00463f10 | 33 | unlabelled |  |
| 0x00463fb0 | 8 | folded | returns a constant or a global |
| 0x00464060 | 132 | unlabelled |  |
| 0x00464780 | 201 | unlabelled |  |
| 0x00464de0 | 12 | unlabelled |  |
| 0x00465210 | 60 | unlabelled |  |
| 0x004654d0 | 9 | folded | read: container code inside ported callers (vector copy, insert and reserve, introsort, list clear and find, uninitialised copy) and thunks whose MW3 pair is identical |
| 0x00465610 | 65 | unlabelled |  |
| 0x004659c0 | 12 | unlabelled |  |
| 0x004659e0 | 32 | unlabelled |  |
| 0x00465a60 | 56 | unlabelled |  |
| 0x00465da0 | 11 | folded | covered by the port where it names it (src/mech3/mech.h:467) |
| 0x00465dc0 | 18 | folded | covered by the port where it names it (src/mech3/mech_command.cpp:265) |
| 0x00465df0 | 14 | unlabelled |  |
| 0x00465e00 | 11 | folded | inside its ported callers (0x0041f4d0): a small helper their ports do their own way |
| 0x00465ea0 | 39 | unlabelled |  |
| 0x00465f90 | 12 | folded | covered by the port where it names it (src/mech3/mech_net.cpp:584) |
| 0x00466040 | 9 | unlabelled | follows 0x00462090:  |
| 0x00466060 | 17 | folded | as MW3 0x004647f0: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00467200 | 18 | unlabelled |  |
| 0x00467480 | 8 | folded | returns a constant or a global |
| 0x00467c90 | 12 | unlabelled |  |
| 0x00467d10 | 62 | unlabelled |  |
| 0x004685e0 | 27 | folded | ported, by docs/symbols.csv: ported; ZAPS_HUD kind 3 |
| 0x00468660 | 57 | unlabelled |  |
| 0x00468750 | 17 | folded | covered by the port where it names it (src/mech3/mech_build.h:183) |
| 0x004687c0 | 11 | folded | as MW3 0x00467410: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00468910 | 10 | folded | covered by the port where it names it (src/mech3/mech_part_build.cpp:160) |
| 0x00468930 | 11 | folded | as MW3 0x004673b0: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00468b10 | 22 | folded | as MW3 0x004670a0: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00468bf0 | 10 | folded | as MW3 0x00467180: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00468c30 | 8 | folded | returns a constant or a global |
| 0x00468c50 | 25 | folded | covered by the port where it names it (src/mech3/mech_parts.cpp:985) |
| 0x00468d50 | 12 | unlabelled |  |
| 0x00468d80 | 12 | folded | as MW3 0x004c4c60: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x00468da0 | 127 | folded | read: container code inside ported callers (vector copy, insert and reserve, introsort, list clear and find, uninitialised copy) and thunks whose MW3 pair is identical |
| 0x00468ff0 | 9 | folded | a thunk: one call to another function |
| 0x00469010 | 8 | folded | empty: does nothing |
| 0x00469030 | 8 | folded | empty: does nothing |
| 0x00469050 | 8 | folded | empty: does nothing |
| 0x00469060 | 30 | unlabelled |  |
| 0x00469510 | 104 | unlabelled |  |
| 0x004697f0 | 9 | folded | as MW3 0x00467c80: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004698f0 | 11 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:48) |
| 0x00469920 | 16 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:48) |
| 0x00469950 | 15 | folded | as MW3 0x00467e60: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00469a30 | 13 | folded | checked by test (block trace, 2026-11-13): test_pm_pocket_panel runs it (the commander panel's lines and state compared) |
| 0x00469bc0 | 11 | folded | read: container code inside ported callers (vector copy, insert and reserve, introsort, list clear and find, uninitialised copy) and thunks whose MW3 pair is identical |
| 0x00469c80 | 63 | MISSING LOGIC | pm only, minor, effect to check: the lance command's target marker (cmd_target.flt; MW3's 0x00468100, the port's make_marker, app_pocket.cpp:175): PM gives it side bit 1 outright (MW3 the player's side by 0x004d5af0(0), 1 for side 0 as well), passes it to 0x004e88b0 (a list's add-if-missing on the AI registry), and starts it switched off: its node inactive (0x004fca50(node, 0)) and its +8 byte cleared; the port's marker starts on, as MW3's. Whether anything finds the marker while it is off is to check |
| 0x0046a230 | 13 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:399) |
| 0x0046a270 | 13 | folded | ported, by docs/symbols.csv: ported: the colours (pocket_colours) |
| 0x0046a480 | 108 | folded | group A, read: the same logic and constants (0, 1, 2, 1.1547); PM passes the node and the out vector to its helpers (0x00540880, 0x0040b940, 0x005c45b0) where MW3 leaves them in registers, and hoists the +0x84 offset; the field +0x168 is +0x188 on PM's layout |
| 0x0046a8b0 | 15 | folded | ported, by docs/symbols.csv: ported: the record (member pointer, flags +0xc taken / +0xd refused) |
| 0x0046a8e0 | 36 | folded | covered by the port where it names it (src/mech3/lance_orders.cpp:115) |
| 0x0046a990 | 16 | folded | ported, by docs/symbols.csv: ported: the record with the target at +0x10 |
| 0x0046a9c0 | 36 | folded | ported, by docs/symbols.csv: ported: pocket_order's unit step (on the target) |
| 0x0046ab20 | 40 | folded | ported, by docs/symbols.csv: ported: pocket_order (on a contact made on the target) |
| 0x0046ac20 | 102 | unlabelled |  |
| 0x0046af20 | 94 | unlabelled |  |
| 0x0046b1b0 | 112 | unlabelled |  |
| 0x0046b4b0 | 9 | unlabelled | follows 0x0046b4d0:  |
| 0x0046b4d0 | 182 | unlabelled |  |
| 0x0046b8b0 | 9 | folded | a thunk: one call to another function |
| 0x0046bf70 | 10 | folded | ported, by docs/symbols.csv: ported: pocket_draw's |
| 0x0046c110 | 12 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:290) |
| 0x0046c140 | 27 | folded | ported, by docs/symbols.csv: ported |
| 0x0046c1d0 | 12 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:290) |
| 0x0046c200 | 27 | folded | ported, by docs/symbols.csv: ported |
| 0x0046c290 | 12 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:291) |
| 0x0046c2c0 | 19 | folded | ported, by docs/symbols.csv: ported |
| 0x0046c300 | 12 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:291) |
| 0x0046c360 | 12 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:291) |
| 0x0046c390 | 20 | folded | ported, by docs/symbols.csv: ported |
| 0x0046c4d0 | 39 | unlabelled |  |
| 0x0046c5f0 | 16 | unlabelled |  |
| 0x0046c620 | 31 | unlabelled |  |
| 0x0046c680 | 15 | unlabelled |  |
| 0x0046c6c0 | 9 | folded | follows 0x0046c700: as MW3 0x0046ab70: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0046c6d0 | 9 | folded | a thunk: one call to another function |
| 0x0046c700 | 10 | folded | as MW3 0x0046ab70: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0046c720 | 12 | unlabelled |  |
| 0x0046c920 | 72 | unlabelled |  |
| 0x0046cc50 | 238 | unlabelled |  |
| 0x0046d310 | 47 | unlabelled |  |
| 0x0046d3e0 | 72 | unlabelled |  |
| 0x0046d5e0 | 29 | folded | as MW3 0x0046cca0: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0046d690 | 797 | unlabelled |  |
| 0x0046ecf0 | 46 | folded | as MW3 0x0046cc00: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0046eda0 | 16 | folded | as MW3 0x0046cd40: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0046edd0 | 9 | folded | a thunk: one call to another function |
| 0x0046ee60 | 16 | unlabelled |  |
| 0x0046ee90 | 9 | folded | a thunk: one call to another function |
| 0x0046ef20 | 17 | folded | as MW3 0x0046d0b0: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0046ef40 | 9 | folded | a thunk: one call to another function |
| 0x0046f510 | 157 | unlabelled |  |
| 0x0046f8e0 | 13 | folded | as MW3 0x0046d380: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0046f910 | 27 | unlabelled |  |
| 0x0046fe30 | 32 | folded | covered by the port where it names it (src/mech3/mech_net.cpp:1166) |
| 0x00470110 | 26 | folded | covered by the port where it names it (src/mech3/mech_net.cpp:1244) |
| 0x00470170 | 28 | unlabelled |  |
| 0x004703c0 | 57 | unlabelled |  |
| 0x00470500 | 55 | folded | as MW3 0x0046e3c0: covered where the port does it: src/mech3/mech_net.cpp:1184 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004705e0 | 206 | unlabelled |  |
| 0x00470d70 | 9 | folded | a thunk: one call to another function |
| 0x00470f60 | 35 | unlabelled |  |
| 0x00471db0 | 31 | unlabelled |  |
| 0x00471e70 | 53 | unlabelled |  |
| 0x00472760 | 9 | folded | a thunk: one call to another function |
| 0x00472770 | 16 | folded | covered by the port where it names it (src/mech3/app_states.h:85) |
| 0x004727b0 | 8 | folded | returns a constant or a global |
| 0x004727c0 | 16 | folded | as MW3 0x00470690: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00472800 | 9 | folded | a thunk: one call to another function |
| 0x00472b30 | 9 | folded | a thunk: one call to another function |
| 0x00472cb0 | 28 | folded | traced: MW3's 0x00470ab0 unpaired, the network mechs' round robin for the ping (mech_net.cpp:374 net_next_player), the same |
| 0x00472e60 | 22 | folded | read: container code inside ported callers (vector copy, insert and reserve, introsort, list clear and find, uninitialised copy) and thunks whose MW3 pair is identical |
| 0x00472ea0 | 117 | folded | as MW3 0x0049ced0: covered where the port does it: src/mech3/mp_lobby.h:38 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004730b0 | 12 | folded | covered by the port where it names it (src/mech3/mech_net.cpp:624) |
| 0x00473120 | 14 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:13) |
| 0x00473160 | 10 | folded | read with its caller 0x0045a250: the tons kept per base left, 300 (the port's kTonsPerBase, mech_fieldbase_cut.cpp:26) |
| 0x004731b0 | 35 | unlabelled |  |
| 0x00473240 | 34 | folded | covered by the port where it names it (src/mech3/mission_launch.cpp:220) |
| 0x004732b0 | 191 | unlabelled |  |
| 0x004735f0 | 39 | folded | inside its ported callers (0x0045dc00, 0x00460570): a small helper their ports do their own way |
| 0x00473690 | 101 | unlabelled |  |
| 0x004738b0 | 20 | folded | inside its ported callers (0x00473f70, 0x00474010, 0x004745a0, 0x00474810): a small helper their ports do their own way |
| 0x004741a0 | 140 | unlabelled |  |
| 0x00474810 | 132 | unlabelled |  |
| 0x00474ae0 | 26 | folded | as MW3 0x004729d0: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00475100 | 19 | folded | read with its caller 0x0045a250: the store's tons (MW3's 0x0045b900 walk), the port's store_tons (mech_fieldbase_cut.cpp) |
| 0x00475140 | 10 | folded | covered by the port where it names it (src/mech3/mech_config.h:35) |
| 0x004751e0 | 9 | folded | a thunk: one call to another function |
| 0x00475220 | 23 | unlabelled |  |
| 0x00475270 | 15 | folded | checked by test (block trace, 2026-11-13): test_pm_pilot's read 0x0047b2f0 runs it; the file read and written back compared byte for byte |
| 0x004752a0 | 10 | unlabelled |  |
| 0x004754d0 | 47 | folded | group A, read: the salvage's have-enough check gives the same answer (false as soon as one item is short); MW3 reports each shortage (CONFIG: want %d %s, only have %d) and goes on, PM stops at the first without a report: a developer message only |
| 0x004755f0 | 19 | folded | covered by the port where it names it (src/mech3/app_hooks.h:957) |
| 0x00475630 | 32 | folded | covered by the port where it names it (src/mech3/mech_salvage.cpp:148) |
| 0x00475700 | 20 | folded | covered by the port where it names it (src/mech3/mech_salvage.cpp:147) |
| 0x00475750 | 9 | folded | a thunk: one call to another function |
| 0x00475760 | 55 | unlabelled |  |
| 0x004757d0 | 55 | unlabelled |  |
| 0x00475840 | 9 | folded | a thunk: one call to another function |
| 0x00475860 | 16 | unlabelled |  |
| 0x00475890 | 9 | folded | a thunk: one call to another function |
| 0x00475b20 | 35 | folded | read: a skin entry's texture (MW3's 0x00473d40, the port's SkinEntry::load, mech_skin.cpp:248), the same fetch, find and load; PM drops MW3's forcing of the alternate for 'vult_d03_c', which no PM file names, so the port's keeping it changes nothing on PM |
| 0x00475dd0 | 88 | unlabelled |  |
| 0x00475fe0 | 92 | unlabelled |  |
| 0x00476750 | 78 | unlabelled |  |
| 0x00476bf0 | 17 | folded | as MW3 0x00474ef0: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00476c10 | 9 | folded | a thunk: one call to another function |
| 0x00476c60 | 77 | unlabelled |  |
| 0x00476ff0 | 108 | unlabelled |  |
| 0x004773a0 | 23 | folded | read with its caller (the mech data load 0x00430c10): the definition's skins put on the model by its first entry (MW3 0x00475780, the port's skins_apply, mech_data.cpp:371) |
| 0x00477470 | 97 | folded | group A, read: the same logic; the element count's checks are inlined in different branches (the ctor 0x00475be0 called from three of them) |
| 0x004777f0 | 35 | folded | covered by the port where it names it (src/mech3/mech_part_update.cpp:352) |
| 0x004779e0 | 29 | folded | covered by the port where it names it (src/mech3/mech_pose.cpp:228) |
| 0x00477c80 | 86 | unlabelled |  |
| 0x00477dd0 | 31 | unlabelled |  |
| 0x00477ef0 | 15 | folded | covered by the port where it names it (src/mech3/mech_skis.cpp:201) |
| 0x00477f90 | 693 | unlabelled |  |
| 0x004790c0 | 11 | folded | inside its ported callers (0x00477f90): a small helper their ports do their own way |
| 0x004790f0 | 32 | unlabelled |  |
| 0x00479330 | 28 | folded | covered by the port where it names it (src/mech3/mech_headlight.h:4) |
| 0x004793c0 | 19 | folded | inside its ported callers (0x00479440): a small helper their ports do their own way |
| 0x004796c0 | 12 | unlabelled |  |
| 0x00479780 | 46 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x004798d0 | 24 | folded | covered by the port where it names it (src/mech3/mech_headlight.cpp:76) |
| 0x00479940 | 11 | unlabelled |  |
| 0x00479c20 | 8 | folded | inside its ported callers (0x004440b0): a small helper their ports do their own way |
| 0x00479ca0 | 16 | folded | covered by the port where it names it (src/mech3/pilot.h:25) |
| 0x00479cd0 | 9 | folded | a thunk: one call to another function |
| 0x00479d50 | 19 | folded | covered by the port where it names it (src/mech3/pilot.cpp:464) |
| 0x00479d80 | 169 | unlabelled |  |
| 0x0047a080 | 13 | folded | covered by the port where it names it (src/mech3/pilot.cpp:123) |
| 0x0047a6e0 | 66 | unlabelled |  |
| 0x0047a860 | 23 | unlabelled |  |
| 0x0047a8f0 | 51 | unlabelled |  |
| 0x0047a9a0 | 12 | folded | inside its ported callers (0x0047a8f0): a small helper their ports do their own way |
| 0x0047a9c0 | 15 | folded | inside its ported callers (0x0047a8f0): a small helper their ports do their own way |
| 0x0047ad20 | 81 | unlabelled |  |
| 0x0047b6a0 | 11 | folded | as MW3 0x00479420: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0047b6e0 | 17 | folded | as MW3 0x00479460: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0047b710 | 17 | folded | as MW3 0x00479480: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0047b740 | 17 | folded | read: the pilot's training game (its +0x78 when asked, else the current game when its type is 4), PM's getter for the win's next lesson; its callers are ported as PM's (mission.cpp:572 Missions::complete, the training lessons) |
| 0x0047b770 | 17 | folded | as MW3 0x004794a0: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0047b7b0 | 9 | folded | a thunk: one call to another function |
| 0x0047b7d0 | 9 | folded | a thunk: one call to another function |
| 0x0047b810 | 9 | folded | a thunk: one call to another function |
| 0x0047b820 | 9 | folded | a thunk: one call to another function |
| 0x0047b8d0 | 12 | unlabelled |  |
| 0x0047bd00 | 31 | unlabelled |  |
| 0x0047bd60 | 31 | unlabelled |  |
| 0x0047bdc0 | 12 | unlabelled |  |
| 0x0047bde0 | 22 | unlabelled |  |
| 0x0047be40 | 29 | MISSING LOGIC | pm only, field bases: the MCB waynet's nearest point (0x0047c960, MW3 0x0047a680, the port's McbWaynet::nearest, mech_waynet.cpp:312): PM walks the waynet's points itself (0x0047be40 with 0x0047bdc0 / 0x0047bde0) and takes the nearest whose hidden byte (+0x11) is clear, selecting nothing when none is; MW3's (0x00479b50, the port's Waynet::nearest, mech_waynet.cpp:195) asks the net's nearest step and falls back to the current point. PM hides the dock points that aren't static MFBs as it counts them (the port's dock_count, mech_waynet.cpp:417), so on PM the port can pick a hidden point where PM would not |
| 0x0047bf00 | 9 | folded | ported, by docs/symbols.csv: ported |
| 0x0047c0e0 | 59 | folded | group A, read: the same logic; the clock getter (0x0040b680) is read once after the branch where MW3 reads it in each |
| 0x0047c1f0 | 69 | unlabelled |  |
| 0x0047c320 | 72 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x0047c540 | 12 | folded | ported, by docs/symbols.csv: ported |
| 0x0047c720 | 8 | folded | covered by the port where it names it (src/mech3/mech_waynet.h:34) |
| 0x0047c730 | 12 | unlabelled |  |
| 0x0047c750 | 24 | folded | covered by the port where it names it (src/mech3/mech_waynet.h:32) |
| 0x0047c800 | 19 | folded | read: a waynet point's label set (the old one freed, the new one copied, none cleared); the port's WaynetPoint keeps its label as a string |
| 0x0047c860 | 14 | folded | ported, by docs/symbols.csv: ported |
| 0x0047c890 | 12 | unlabelled |  |
| 0x0047c8b0 | 12 | folded | as MW3 0x0047a5a0: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0047c8e0 | 23 | unlabelled |  |
| 0x0047c920 | 23 | unlabelled |  |
| 0x0047ca00 | 12 | folded | as MW3 0x0047a720: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0047ca10 | 24 | unlabelled |  |
| 0x0047ca60 | 16 | folded | as MW3 0x0047a780: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0047cbe0 | 11 | unlabelled |  |
| 0x0047cc10 | 9 | folded | a thunk: one call to another function |
| 0x0047cc20 | 17 | folded | ported, by docs/symbols.csv: ported (mech3 WinMovieState): the Win Movie State (0x00608be8, vtable 0x005c3958) |
| 0x0047cdb0 | 13 | unlabelled |  |
| 0x0047cde0 | 9 | folded | a thunk: one call to another function |
| 0x0047ce00 | 24 | folded | covered by the port where it names it (src/mech3/debriefing.h:15) |
| 0x0047ce60 | 12 | unlabelled |  |
| 0x0047ce80 | 20 | folded | as MW3 0x0047ab90: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0047cf20 | 93 | unlabelled |  |
| 0x0047d2d0 | 12 | unlabelled |  |
| 0x0047d2f0 | 20 | folded | as MW3 0x0047b000: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0047d340 | 12 | unlabelled |  |
| 0x0047d360 | 47 | unlabelled |  |
| 0x0047d890 | 700 | unlabelled |  |
| 0x0047ec10 | 12 | folded | covered by the port where it names it (src/mech3/debriefing.h:79) |
| 0x0047ec30 | 20 | unlabelled |  |
| 0x0047ec80 | 12 | unlabelled |  |
| 0x0047ecb0 | 56 | unlabelled |  |
| 0x0047eea0 | 91 | unlabelled |  |
| 0x0047f210 | 9 | folded | a thunk: one call to another function |
| 0x0047f220 | 16 | folded | covered by the port where it names it (src/mech3/app_states.h:60) |
| 0x0047f260 | 79 | unlabelled |  |
| 0x0047f400 | 9 | folded | a thunk: one call to another function |
| 0x0047f420 | 145 | unlabelled |  |
| 0x0047f740 | 9 | folded | a thunk: one call to another function |
| 0x0047f750 | 10 | folded | covered by the port where it names it (src/mech3/app_states.h:238) |
| 0x0047f770 | 9 | folded | a thunk: one call to another function |
| 0x0047f7c0 | 35 | unlabelled |  |
| 0x0047f870 | 206 | unlabelled |  |
| 0x0047ff80 | 12 | folded | covered by the port where it names it (src/mech3/salvage_screen.cpp:28) |
| 0x0047ffa0 | 22 | folded | covered by the port where it names it (src/mech3/salvage_screen.cpp:28) |
| 0x00480010 | 12 | unlabelled |  |
| 0x00480030 | 20 | folded | as MW3 0x0047dac0: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00480080 | 12 | folded | covered by the port where it names it (src/mech3/salvage_screen.h:33) |
| 0x004800a0 | 10 | folded | covered by the port where it names it (src/mech3/salvage_screen.h:39) |
| 0x004800d0 | 52 | unlabelled |  |
| 0x00480240 | 12 | unlabelled |  |
| 0x00480260 | 191 | unlabelled |  |
| 0x004806c0 | 74 | unlabelled |  |
| 0x00480840 | 77 | unlabelled |  |
| 0x004809f0 | 35 | unlabelled |  |
| 0x00480db0 | 61 | unlabelled |  |
| 0x00480f80 | 12 | unlabelled |  |
| 0x00480fa0 | 27 | folded | as MW3 0x0047ea10: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00481020 | 10 | folded | as MW3 0x0047ea90: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00481050 | 22 | unlabelled |  |
| 0x004810d0 | 12 | unlabelled |  |
| 0x00481690 | 26 | unlabelled |  |
| 0x004816f0 | 16 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:2492) |
| 0x00481730 | 28 | folded | covered by the port where it names it (src/mech3/app_hud.cpp:757) |
| 0x004817b0 | 27 | folded | as MW3 0x0047f2b0: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00481850 | 12 | unlabelled |  |
| 0x00481870 | 10 | folded | as MW3 0x0047f370: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00481880 | 33 | folded | covered by the port where it names it (src/mech3/mech_data.h:115) |
| 0x00481950 | 9 | folded | follows 0x00485130: as MW3 0x004822e0: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00481960 | 9 | folded | a thunk: one call to another function |
| 0x00481990 | 9 | folded | follows 0x004819a0: as MW3 0x00482a60: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x004819a0 | 18 | folded | as MW3 0x00482a60: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x004819e0 | 12 | unlabelled |  |
| 0x00481a00 | 9 | folded | a thunk: one call to another function |
| 0x00481a30 | 9 | folded | follows 0x004892f0: as MW3 0x00486920: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00481a40 | 9 | folded | a thunk: one call to another function |
| 0x00481a70 | 11 | folded | as MW3 0x00482ad0: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00481ac0 | 9 | folded | a thunk: one call to another function |
| 0x00481af0 | 9 | folded | follows 0x00481b00: as MW3 0x00482c10: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00481b00 | 16 | folded | as MW3 0x00482c10: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00481b40 | 9 | folded | a thunk: one call to another function |
| 0x00481b70 | 9 | unlabelled | follows 0x00483630:  |
| 0x00481b80 | 9 | folded | a thunk: one call to another function |
| 0x00481bb0 | 9 | folded | follows 0x00481bc0: as MW3 0x00482b40: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00481bc0 | 22 | folded | as MW3 0x00482b40: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00481c20 | 12 | unlabelled |  |
| 0x00481c40 | 20 | folded | as MW3 0x00482bc0: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00481c90 | 9 | folded | a thunk: one call to another function |
| 0x00481cc0 | 9 | folded | follows 0x00481cd0: as MW3 0x00482af0: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00481cd0 | 17 | folded | as MW3 0x00482af0: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00481d20 | 9 | folded | a thunk: one call to another function |
| 0x00481d50 | 9 | folded | a thunk: one call to another function |
| 0x00481d60 | 9 | folded | a thunk: one call to another function |
| 0x00481d90 | 9 | folded | a thunk: one call to another function |
| 0x00481da0 | 9 | folded | a thunk: one call to another function |
| 0x00481dc0 | 8 | folded | returns a constant or a global |
| 0x00481dd0 | 8 | folded | returns a constant or a global |
| 0x00481df0 | 9 | folded | follows 0x00481e20: group A, read: PM's InitInstance, covered by app.cpp's PM notes (the MechWarrior 3 EP1 settings key :191, DefaultCmdMap :192, the standalone missions :195, detail.zrd :196); the message DLL by name, which the port reads as a file |
| 0x00481e00 | 9 | folded | a thunk: one call to another function |
| 0x00481e20 | 187 | folded | group A, read: PM's InitInstance, covered by app.cpp's PM notes (the MechWarrior 3 EP1 settings key :191, DefaultCmdMap :192, the standalone missions :195, detail.zrd :196); the message DLL by name, which the port reads as a file |
| 0x00482070 | 12 | unlabelled |  |
| 0x00482090 | 33 | unlabelled |  |
| 0x00482140 | 38 | folded | group A, read: imports only (the C runtime's), the logic as the shared port |
| 0x00482230 | 8 | folded | returns a constant or a global |
| 0x00482240 | 31 | debug or dev tool: PORT (preserve) | read: PM's app defaults (MW3 0x0047fb80): no default mech name where MW3 sets 'bushwacker' (app.cpp:72 keeps it), the flags at PM's layout (+0x1a8.., three 1.0 floats); the default mech is reached only by a start straight into a mission (the developers' route), so it shows nowhere in play |
| 0x004822c0 | 9 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x004824c0 | 9 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x004824d0 | 48 | unlabelled |  |
| 0x00482650 | 26 | folded | covered by the port where it names it (src/mech3/app.cpp:400) |
| 0x004826b0 | 36 | unlabelled |  |
| 0x004826e0 | 173 | folded | group A, read: PM's InitInstance, covered by app.cpp's PM notes (the MechWarrior 3 EP1 settings key :191, DefaultCmdMap :192, the standalone missions :195, detail.zrd :196); the message DLL by name, which the port reads as a file |
| 0x00482a60 | 25 | replaced (SDL or the platform layer) | as MW3 0x00480340, whose class PM's changes keep: Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00482ac0 | 19 | replaced (SDL or the platform layer) | as MW3 0x004803a0: Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00482ca0 | 193 | MISSING LOGIC | PM's sound range at low detail (pm only): at start-up PM sets [0x00613488] to 0.5 when the object detail is 2 (0x0040b1e0), else 1 (0x00482ca0 -> 0x00591830), and a sound's default RANGE (50 400) is scaled by it (0x005912a0, MW3's 0x005700f0 unscaled): at low detail every sound without its own RANGE is heard to 25 / 200 m. The port's start-up and the sound set's defaults are MW3's |
| 0x00483020 | 48 | unlabelled |  |
| 0x004832a0 | 9 | debug or dev tool: PORT (preserve) | follows 0x00483d50: as MW3 0x004810d0, whose class PM's changes keep: the debug console's overlay toggle (0x00608f74; game-loop.md: from 0x00480b20, itself never called) |
| 0x004832b0 | 11 | unlabelled |  |
| 0x00483330 | 9 | unlabelled | follows 0x00483340:  |
| 0x00483340 | 63 | unlabelled |  |
| 0x00483470 | 43 | unlabelled |  |
| 0x00483540 | 21 | unlabelled |  |
| 0x004835a0 | 9 | folded | a thunk: one call to another function |
| 0x004835c0 | 22 | unlabelled |  |
| 0x00483630 | 22 | unlabelled |  |
| 0x00483700 | 74 | folded | group A, read: the Core state's start as the shared port (app_states.cpp:271-296): MW3 plays mech_powerup too (its string passed in a register, so the decomp shows none); PM's 0x0040a870 for MW3's option reads |
| 0x004838b0 | 184 | MISSING LOGIC | group A, read: PM's mission music (pm only): on a disc of more than five tracks PM plays from a random track on to the end of the disc (mode 3, [0x00607f28] = 3), where MW3 and the port play that one track (mode 6, app_states.cpp:390-392); with 2..5 tracks both pick by mission (PM through the pilot's vt+0x48, the port's vt+0x4c: to check it is the same call on PM's layout); a forced -cdN plays in whatever mode PM last set. The developer lines it writes (Dummy Frame, Let's Play) are 0x00410820's, developer only |
| 0x00483d50 | 30 | debug or dev tool: PORT (preserve) | as MW3 0x004810d0, whose class PM's changes keep: the debug console's overlay toggle (0x00608f74; game-loop.md: from 0x00480b20, itself never called) |
| 0x00483de0 | 42 | unlabelled |  |
| 0x00483f30 | 124 | unlabelled |  |
| 0x00484530 | 123 | unlabelled |  |
| 0x004848a0 | 173 | folded | group A, read: the frame's video calls are PM's Direct3D 2D path (0x00597d20.., 0x005c0450.., replaced by the SDL layer, as the 0x005c0320 row) where MW3's are DirectDraw's (0x00576d60, 0x00576e70, 0x00576ea0); the rest as the shared port |
| 0x00484ed0 | 34 | unlabelled |  |
| 0x00484fa0 | 51 | unlabelled |  |
| 0x004850a0 | 38 | unlabelled |  |
| 0x00485130 | 17 | folded | as MW3 0x004822e0: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x004852f0 | 31 | folded | as MW3 0x004828d0: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00485480 | 29 | folded | inside its ported callers (0x004826e0): a small helper their ports do their own way |
| 0x004854e0 | 22 | folded | read: PM's disc check (data.tag on any CD-ROM drive, retried with the app's prompt vt+0xc0), from InitInstance 0x004826e0; removed as FIX (trap) like MW3's self-check (app.cpp:168, docs/protection.md, docs/pm-map.md) |
| 0x00485530 | 12 | folded | inside its ported callers (0x004826e0): a small helper their ports do their own way |
| 0x00485550 | 26 | replaced (SDL or the platform layer) | as MW3 0x00482c50: Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x004855b0 | 8 | folded | returns a constant or a global |
| 0x004855c0 | 8 | folded | returns a constant or a global |
| 0x004855d0 | 20 | replaced (SDL or the platform layer) | as MW3 0x00482cc0: Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00485610 | 9 | replaced (SDL or the platform layer) | called only from 0x004855d0, which are sdl |
| 0x00485620 | 164 | folded | group A, read: PM's command line: -noclutter is not offered (app_mission.cpp:2244, the low detail stands in), -verbose is the port's own, the CD check and mech3.err dropped as MW3's FIX (A); PM's added 0x00527230 to read |
| 0x004859b0 | 9 | folded | as MW3 0x00484130: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x004859c0 | 12 | folded | as MW3 0x00484140: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x004859e0 | 12 | unlabelled |  |
| 0x00485a00 | 20 | unlabelled |  |
| 0x00485a50 | 12 | unlabelled |  |
| 0x00485a70 | 10 | replaced (SDL or the platform layer) | as MW3 0x004830c0: Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00485a80 | 15 | replaced (SDL or the platform layer) | as MW3 0x004830d0: Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00485aa0 | 8 | folded | returns a constant or a global |
| 0x00485ab0 | 8 | folded | returns a constant or a global |
| 0x00485af0 | 9 | replaced (SDL or the platform layer) | as MW3 0x00483130: Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00485b20 | 45 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x00483170: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00485ba0 | 12 | unlabelled |  |
| 0x00485bd0 | 14 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x00483230: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00485c00 | 9 | folded | a thunk: one call to another function |
| 0x00485c20 | 12 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x004832e0: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00485c60 | 9 | unlabelled |  |
| 0x00485c90 | 10 | replaced (SDL or the platform layer) | as MW3 0x00483370: Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00485d10 | 9 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x004833f0: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00485d20 | 12 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x00483400: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00485d30 | 14 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x00483410: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00485d50 | 9 | folded | follows 0x004822c0: the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x00485d60 | 21 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x004834b0: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00485db0 | 9 | folded | a thunk: one call to another function |
| 0x00485e10 | 9 | unlabelled |  |
| 0x00485e40 | 9 | unlabelled |  |
| 0x00485e60 | 10 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x00483600: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00485e80 | 14 | unlabelled |  |
| 0x00485ea0 | 32 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x00483640: the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00485f80 | 11 | unlabelled |  |
| 0x00485fb0 | 11 | unlabelled |  |
| 0x00485fe0 | 11 | unlabelled |  |
| 0x00486010 | 11 | unlabelled |  |
| 0x00486040 | 11 | unlabelled |  |
| 0x00486070 | 11 | unlabelled |  |
| 0x004860a0 | 11 | unlabelled |  |
| 0x004860d0 | 11 | unlabelled |  |
| 0x00486100 | 11 | unlabelled |  |
| 0x00486130 | 15 | unlabelled |  |
| 0x00486190 | 15 | unlabelled |  |
| 0x00486260 | 9 | replaced (SDL or the platform layer) | as MW3 0x00484160: Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00486270 | 9 | replaced (SDL or the platform layer) | as MW3 0x00484170: Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x004862d0 | 9 | replaced (SDL or the platform layer) | as MW3 0x004841d0: Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00486300 | 11 | unlabelled |  |
| 0x00486330 | 11 | unlabelled |  |
| 0x00486360 | 9 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:402) |
| 0x00486690 | 39 | folded | as MW3 0x00484540: covered where the port does it: src/mech3/mech_lib.cpp:89 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00486730 | 25 | unlabelled |  |
| 0x004868d0 | 14 | folded | inside its ported callers (0x00479510): a small helper their ports do their own way |
| 0x00486f10 | 51 | unlabelled |  |
| 0x00487480 | 56 | unlabelled |  |
| 0x00487530 | 8 | folded | as MW3 0x00485360: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00487540 | 9 | folded | a thunk: one call to another function |
| 0x00487570 | 12 | folded | as MW3 0x00485390: the equipment callout: the voice for an equipment kind ([0x005e6870 + 0x14 x kind], 0x00485360) played as 0x00485370 plays one; the port has the table (mech_lib.cpp:18-24, equipment_voice) and sound_say, but its hook DamageCalls.equip_voice is a census stand-in: see BROKEN |
| 0x004875b0 | 12 | folded | covered by the port where it names it (src/mech3/app_weapons.cpp:205) |
| 0x00487620 | 12 | folded | covered by the port where it names it (src/mech3/mech_names.cpp:403) |
| 0x00487660 | 12 | folded | covered by the port where it names it (src/mech3/mech_names.cpp:403) |
| 0x004876a0 | 12 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:587) |
| 0x004876e0 | 9 | folded | a thunk: one call to another function |
| 0x004876f0 | 13 | folded | covered by the port where it names it (src/mech3/app.cpp:168) |
| 0x004879a0 | 31 | unlabelled |  |
| 0x00487a40 | 12 | unlabelled |  |
| 0x00487c00 | 18 | unlabelled |  |
| 0x00487c30 | 18 | unlabelled |  |
| 0x00487ce0 | 31 | unlabelled |  |
| 0x00487e60 | 147 | unlabelled |  |
| 0x00488370 | 12 | unlabelled |  |
| 0x004883a0 | 38 | unlabelled |  |
| 0x00488450 | 10 | unlabelled |  |
| 0x00488470 | 34 | MISSING LOGIC | pm only, visible: PM's menu background (vtable 0x005e6ea8, built in the screens: briefing, credits, controls, options; MW3's ScrollImage 0x00438cc0 / 0x0040a210, menu_widgets.cpp) scrolls by time: each update moves round(22.0 * dt + the carried fraction) pixels on each axis (steps +0xbc/+0xc0 = 22.0f, fractions +0xc4/+0xc8, 0.5 at 0x005de818), about 22 px a second; MW3 moves 1 px a frame, which the port keeps (FIX (B) frames owed at 33 ms: about 30 px a second). The view rect is (0, 0, w, h - 1) in PM; the object 8 bytes larger (moving at +0x140) |
| 0x00488530 | 10 | unlabelled |  |
| 0x00488560 | 14 | unlabelled |  |
| 0x004885b0 | 113 | MISSING LOGIC | pm only, visible: PM's menu background (vtable 0x005e6ea8, built in the screens: briefing, credits, controls, options; MW3's ScrollImage 0x00438cc0 / 0x0040a210, menu_widgets.cpp) scrolls by time: each update moves round(22.0 * dt + the carried fraction) pixels on each axis (steps +0xbc/+0xc0 = 22.0f, fractions +0xc4/+0xc8, 0.5 at 0x005de818), about 22 px a second; MW3 moves 1 px a frame, which the port keeps (FIX (B) frames owed at 33 ms: about 30 px a second). The view rect is (0, 0, w, h - 1) in PM; the object 8 bytes larger (moving at +0x140) |
| 0x00488880 | 32 | folded | read: PM's menu background draw, MW3's 0x0040a490 (ScrollImage::draw) with its piece count read from data (0x00609588 = 4, never written) and PM's offsets |
| 0x00488900 | 30 | unlabelled |  |
| 0x004889d0 | 67 | unlabelled |  |
| 0x00488bd0 | 20 | unlabelled |  |
| 0x00488c20 | 12 | folded | covered by the port where it names it (src/mech3/mainmenu.h:37) |
| 0x00488c40 | 163 | unlabelled |  |
| 0x004891d0 | 25 | unlabelled |  |
| 0x00489260 | 15 | unlabelled |  |
| 0x004892d0 | 12 | folded | covered by the port where it names it (src/mech3/mainmenu.h:20) |
| 0x004892f0 | 11 | folded | as MW3 0x00486920: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00489340 | 12 | unlabelled |  |
| 0x00489360 | 10 | folded | as MW3 0x00486970: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00489370 | 9 | folded | covered by the port where it names it (src/mech3/app_states.h:193) |
| 0x00489380 | 128 | unlabelled |  |
| 0x00489610 | 42 | unlabelled |  |
| 0x004896e0 | 56 | unlabelled |  |
| 0x00489860 | 11 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:521) |
| 0x00489960 | 56 | folded | as MW3 0x0048b000: covered where the port does it: src/mech3/pilot_menu.cpp:655 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00489a70 | 10 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:523) |
| 0x00489ac0 | 24 | folded | covered by the port where it names it (src/mech3/pilot_menu.cpp:680) |
| 0x00489b20 | 10 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:517) |
| 0x00489b70 | 24 | folded | covered by the port where it names it (src/mech3/pilot_menu.cpp:726) |
| 0x00489d50 | 12 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:207) |
| 0x0048a050 | 17 | folded | covered by the port where it names it (src/mech3/menu_widgets.cpp:475) |
| 0x0048a330 | 8 | folded | empty: does nothing |
| 0x0048a460 | 13 | unlabelled |  |
| 0x0048ac00 | 12 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:324) |
| 0x0048ade0 | 28 | unlabelled |  |
| 0x0048b250 | 10 | unlabelled |  |
| 0x0048b270 | 10 | unlabelled |  |
| 0x0048b290 | 9 | unlabelled |  |
| 0x0048b2b0 | 12 | folded | covered by the port where it names it (src/mech3/mainmenu.h:20) |
| 0x0048b2d0 | 12 | folded | covered by the port where it names it (src/mech3/mainmenu.h:20) |
| 0x0048b2f0 | 28 | folded | as MW3 0x0048e5b0: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0048b370 | 28 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:279) |
| 0x0048b3f0 | 32 | unlabelled |  |
| 0x0048b510 | 10 | unused: PORT (preserve) | fd0 read: the list box's arrows as MW3's 0x0048c910 plus two PM bytes that keep UP (+0x1e0) and DOWN (+0x1e1) enabled; the pm port has the arrows without them (menu_widgets.cpp:1264-1330). Only 0x0048b510 sets them, from the pilot screen's training case (0x004916c0 case 4), which PM's menu doesn't reach (pm-mech-step.md:472), so no shipped path shows it |
| 0x0048bce0 | 35 | folded | inside its ported callers (0x004916c0): a small helper their ports do their own way |
| 0x0048bd50 | 57 | folded | checked by test (block trace, 2026-11-13): test_pm_lobby_lines' lines 0x0049bb70 run it; the two lists' lines read after and compared |
| 0x0048bdf0 | 9 | folded | read: a widget's select, vt+0xa0 called through (a thunk) |
| 0x0048c450 | 60 | unlabelled |  |
| 0x0048c610 | 12 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:397) |
| 0x0048c860 | 103 | unlabelled |  |
| 0x0048cae0 | 9 | folded | covered by the port where it names it (src/mech3/menu_widgets.cpp:798) |
| 0x0048cb40 | 9 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:347) |
| 0x0048cb50 | 13 | folded | covered by the port where it names it (src/mech3/menu_widgets.cpp:798) |
| 0x0048cbd0 | 39 | unlabelled |  |
| 0x0048cd10 | 12 | folded | covered by the port where it names it (src/mech3/quit_box.h:19) |
| 0x0048cd30 | 20 | unlabelled |  |
| 0x0048cd80 | 13 | unlabelled |  |
| 0x0048ce40 | 9 | folded | a thunk: one call to another function |
| 0x0048ce50 | 9 | folded | a thunk: one call to another function |
| 0x0048cef0 | 21 | folded | covered by the port where it names it (src/mech3/app_states.h:13) |
| 0x0048cf40 | 12 | unlabelled |  |
| 0x0048d0b0 | 22 | unlabelled |  |
| 0x0048d110 | 30 | folded | group A, read: the frame's video calls are PM's Direct3D 2D path (0x00597d20.., 0x005c0450.., replaced by the SDL layer, as the 0x005c0320 row) where MW3's are DirectDraw's (0x00576d60, 0x00576e70, 0x00576ea0); the rest as the shared port |
| 0x0048d220 | 9 | folded | a thunk: one call to another function |
| 0x0048d230 | 27 | folded | covered by the port where it names it (src/mech3/app_states.h:303) |
| 0x0048d290 | 9 | folded | a thunk: one call to another function |
| 0x0048d590 | 196 | unlabelled |  |
| 0x0048dca0 | 43 | unlabelled |  |
| 0x0048dd60 | 43 | unlabelled |  |
| 0x0048de20 | 45 | unlabelled |  |
| 0x0048ded0 | 139 | unlabelled |  |
| 0x0048e260 | 12 | folded | covered by the port where it names it (src/mech3/action_menu.h:59) |
| 0x0048e290 | 9 | folded | a thunk: one call to another function |
| 0x0048e3e0 | 12 | unlabelled |  |
| 0x0048e410 | 10 | unlabelled |  |
| 0x0048e430 | 41 | unlabelled |  |
| 0x0048e520 | 12 | unlabelled |  |
| 0x0048e540 | 12 | unlabelled |  |
| 0x0048e560 | 20 | folded | as MW3 0x00489900: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0048e5b0 | 20 | unlabelled |  |
| 0x0048e600 | 12 | unlabelled |  |
| 0x0048e620 | 27 | folded | covered by the port where it names it (src/mech3/action_menu.h:78) |
| 0x0048e6a0 | 10 | unlabelled |  |
| 0x0048e6c0 | 78 | unlabelled |  |
| 0x0048e840 | 20 | unlabelled |  |
| 0x0048e890 | 12 | folded | covered by the port where it names it (src/mech3/action_menu.h:100) |
| 0x0048e8b0 | 81 | unlabelled |  |
| 0x0048eaf0 | 291 | MISSING LOGIC | pm only, pilot-file compatibility and play (fix list: both), Instant Action's kept line-up: PM keeps the allies' and enemies' mechs in the pilot's game record by item value (the mech number; the field base's -1 or 10; ACCEPT 0x0048eaf0 through 0x00439620) and each enemy's loadout by its place among the mech's loadouts (0x004279e0), not its row in the cycle; it restores them by value (0x0048a2e0; reset 0x0048f970 for the allies, 0x0048f660 for the enemies, the loadout reset to 0 when past the mech's count). The port keeps and restores row indices (action_menu.cpp keep_lineup, show_custom, reset) in the same record fields, which it writes to the pilot file (pilot_games.cpp ActionGame::save). So a pilot saved by the original PM brings back other enemies and allies in the port, and the other way round (rows with none first are one off). Also, with no kept enemies (a new pilot's -1), PM's first enemy is mech 0 with its loadouts listed (hidden ones left out, message 0xd75 for a hidden first); MW3 and the port pick the player's mech. 0x0048f970 and 0x0048eaf0 are put here for this; their other differences are read with the unlabelled pass |
| 0x0048f130 | 33 | unlabelled |  |
| 0x0048f1c0 | 31 | unlabelled |  |
| 0x0048f240 | 17 | unlabelled |  |
| 0x0048f2a0 | 24 | unlabelled |  |
| 0x0048f300 | 66 | unlabelled |  |
| 0x0048f660 | 142 | MISSING LOGIC | pm only, pilot-file compatibility and play (fix list: both), Instant Action's kept line-up: PM keeps the allies' and enemies' mechs in the pilot's game record by item value (the mech number; the field base's -1 or 10; ACCEPT 0x0048eaf0 through 0x00439620) and each enemy's loadout by its place among the mech's loadouts (0x004279e0), not its row in the cycle; it restores them by value (0x0048a2e0; reset 0x0048f970 for the allies, 0x0048f660 for the enemies, the loadout reset to 0 when past the mech's count). The port keeps and restores row indices (action_menu.cpp keep_lineup, show_custom, reset) in the same record fields, which it writes to the pilot file (pilot_games.cpp ActionGame::save). So a pilot saved by the original PM brings back other enemies and allies in the port, and the other way round (rows with none first are one off). Also, with no kept enemies (a new pilot's -1), PM's first enemy is mech 0 with its loadouts listed (hidden ones left out, message 0xd75 for a hidden first); MW3 and the port pick the player's mech. 0x0048f970 and 0x0048eaf0 are put here for this; their other differences are read with the unlabelled pass |
| 0x0048f970 | 174 | MISSING LOGIC | pm only, pilot-file compatibility and play (fix list: both), Instant Action's kept line-up: PM keeps the allies' and enemies' mechs in the pilot's game record by item value (the mech number; the field base's -1 or 10; ACCEPT 0x0048eaf0 through 0x00439620) and each enemy's loadout by its place among the mech's loadouts (0x004279e0), not its row in the cycle; it restores them by value (0x0048a2e0; reset 0x0048f970 for the allies, 0x0048f660 for the enemies, the loadout reset to 0 when past the mech's count). The port keeps and restores row indices (action_menu.cpp keep_lineup, show_custom, reset) in the same record fields, which it writes to the pilot file (pilot_games.cpp ActionGame::save). So a pilot saved by the original PM brings back other enemies and allies in the port, and the other way round (rows with none first are one off). Also, with no kept enemies (a new pilot's -1), PM's first enemy is mech 0 with its loadouts listed (hidden ones left out, message 0xd75 for a hidden first); MW3 and the port pick the player's mech. 0x0048f970 and 0x0048eaf0 are put here for this; their other differences are read with the unlabelled pass |
| 0x0048fd60 | 13 | folded | covered by the port where it names it (src/mech3/action_menu.cpp:144) |
| 0x0048fd90 | 19 | folded | read: before the Instant Action screen's reset, the world and scenario cycles (and the descriptions) select the pilot's kept world and scenario (its game's +0x38, +0x3c); the port's reset does the same first from action_setup(), which is the pilot's in PM (action_menu.cpp ActionScreen::reset) |
| 0x0048fe80 | 9 | folded | follows 0x0048fe90: the lab file box name bar's focus setter: the edit bar's 0x005c80c0 plus a byte at +0x68c that none of the bar's 80 virtual methods reads (written only); the port's set_focus (lab_filebox.cpp:41); not run under a test (trace) |
| 0x0048fe90 | 10 | folded | the lab file box name bar's focus setter: the edit bar's 0x005c80c0 plus a byte at +0x68c that none of the bar's 80 virtual methods reads (written only); the port's set_focus (lab_filebox.cpp:41); not run under a test (trace) |
| 0x0048fed0 | 10 | folded | covered by the port where it names it (src/mech3/lab_filebox.cpp:44) |
| 0x0048fef0 | 36 | unlabelled |  |
| 0x00490040 | 36 | unlabelled |  |
| 0x00490120 | 36 | unlabelled |  |
| 0x00490460 | 12 | unlabelled |  |
| 0x00490530 | 10 | folded | inside its ported callers (0x00490760): a small helper their ports do their own way |
| 0x004906f0 | 15 | unlabelled |  |
| 0x00490750 | 10 | folded | as MW3 0x004c4db0: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x00490980 | 116 | unlabelled |  |
| 0x00490d60 | 9 | folded | a thunk: one call to another function |
| 0x00490d70 | 12 | unlabelled |  |
| 0x00490d90 | 64 | unlabelled |  |
| 0x00490f80 | 15 | unlabelled |  |
| 0x00490fc0 | 43 | unlabelled |  |
| 0x00491270 | 32 | unlabelled |  |
| 0x004912f0 | 21 | folded | covered by the port where it names it (src/mech3/pilot_menu.cpp:84) |
| 0x00491340 | 35 | unlabelled |  |
| 0x004913e0 | 35 | unlabelled |  |
| 0x004916c0 | 159 | unlabelled |  |
| 0x00491d90 | 32 | unlabelled |  |
| 0x00491e10 | 9 | folded | a thunk: one call to another function |
| 0x00491e20 | 29 | unlabelled |  |
| 0x00491ea0 | 17 | folded | read: the pilot screen's mode set, MW3's 0x00488ad0 without the training branch (PM has no training): the games list shown and filled (0x004916c0); the port's PilotState::set_mode, whose training-only hiding does nothing in PM's modes (pilot_menu.cpp show_lists shows those three cycles only in training) |
| 0x00491ee0 | 9 | unlabelled |  |
| 0x00491ef0 | 9 | folded | a thunk: one call to another function |
| 0x00491f20 | 9 | unlabelled | follows 0x00491f50:  |
| 0x00491f30 | 9 | folded | a thunk: one call to another function |
| 0x00491f50 | 57 | unlabelled |  |
| 0x00492080 | 36 | unlabelled |  |
| 0x00492130 | 14 | unlabelled |  |
| 0x004921a0 | 77 | unlabelled |  |
| 0x00492490 | 90 | unlabelled |  |
| 0x00492710 | 32 | unlabelled |  |
| 0x004927f0 | 11 | folded | inside its ported callers (0x004486e0): a small helper their ports do their own way |
| 0x00492810 | 14 | folded | inside its ported callers (0x004486e0): a small helper their ports do their own way |
| 0x00493280 | 20 | unlabelled |  |
| 0x004932c0 | 162 | unlabelled |  |
| 0x004936f0 | 57 | folded | read: MW3's world unload 0x00490480 unpaired by the diff (the same steps in the same order: AI lists, mech types and definitions, the stealth, sounds, effects, animations, decals, weapons, the targets and view, the classes, models and textures, the HUD lines), with PM's clutter freed first when clutter is on; the port's Missions::unload carries that step (mission.cpp:665) |
| 0x004938e0 | 68 | unlabelled |  |
| 0x00493a60 | 9 | folded | inside its ported callers (0x004938e0): a small helper their ports do their own way |
| 0x00493a70 | 9 | folded | a thunk: one call to another function |
| 0x00493bd0 | 24 | unlabelled |  |
| 0x00493c80 | 14 | unlabelled |  |
| 0x004943e0 | 13 | MISSING LOGIC | pm only, multiplayer: a network respawn's objectives reload. PM frees them (0x0045d530) and reads them again through the game's vt+0x3c (0x00435140 for the multiplayer game: objectives.zrd's mp<N>, vt+0x58 0x00436580, for every map), then clears the ended and aborting bytes; the port's objectives_again (mission.cpp:532) is MW3's 0x00491050, maps past 2 from mpinfo.zrd, which PM does not ship (no archive holds it), so on PM a respawn on map 3 or later reads no objectives |
| 0x00494450 | 12 | folded | as MW3 0x00491120: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00494490 | 9 | unlabelled | follows 0x004944c0:  |
| 0x004944a0 | 9 | folded | a thunk: one call to another function |
| 0x004944c0 | 10 | unlabelled |  |
| 0x00494500 | 222 | unlabelled |  |
| 0x00494b80 | 105 | unlabelled |  |
| 0x00494e20 | 9 | folded | a thunk: one call to another function |
| 0x00494e30 | 16 | folded | covered by the port where it names it (src/mech3/app_states.h:73) |
| 0x00494e70 | 50 | unlabelled |  |
| 0x00494f80 | 9 | folded | a thunk: one call to another function |
| 0x00494fa0 | 31 | folded | covered by the port where it names it (src/mech3/mission.cpp:649) |
| 0x004950f0 | 12 | folded | ported, by docs/symbols.csv: ported in the program (the missions' +0xb0, reselected) |
| 0x00495190 | 24 | folded | group A, read: the same logic; PM's sun is a 12-byte node handle (vtable 0x005e82d0) freed by its destructor 0x00495db0, which frees the node only when its owns-byte +8 is set, and 0x00495ba0 makes it with +8 = 0: a plain free as MW3's |
| 0x00495ba0 | 79 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x00495db0 | 12 | unlabelled |  |
| 0x00495dd0 | 32 | unlabelled |  |
| 0x00495ec0 | 81 | unlabelled |  |
| 0x00496100 | 12 | unlabelled |  |
| 0x00496120 | 43 | unlabelled |  |
| 0x00496220 | 14 | unlabelled |  |
| 0x00496270 | 62 | unlabelled |  |
| 0x00496400 | 45 | unlabelled |  |
| 0x004964f0 | 28 | unused: PORT (preserve) | as MW3 0x00493120: the movie base's run to the end on its own clock (0x00575d30): nothing calls it (fmv.md) |
| 0x00496590 | 12 | folded | as MW3 0x004931c0: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x004965b0 | 33 | unlabelled |  |
| 0x00496620 | 17 | unlabelled |  |
| 0x00496650 | 9 | unlabelled | follows 0x00496220:  |
| 0x00496660 | 162 | unlabelled |  |
| 0x00496c00 | 9 | unlabelled | follows 0x00498020:  |
| 0x00496c10 | 11 | folded | as MW3 0x004938d0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00496c70 | 9 | unlabelled | follows 0x00498020:  |
| 0x00496c80 | 11 | unlabelled |  |
| 0x00496cb0 | 12 | unlabelled |  |
| 0x00496cd0 | 12 | unlabelled |  |
| 0x00496cf0 | 12 | unlabelled |  |
| 0x00496d10 | 12 | unlabelled |  |
| 0x00496d30 | 73 | unlabelled |  |
| 0x00496f30 | 28 | folded | as MW3 0x004941e0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00496fb0 | 28 | folded | as MW3 0x00494260: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00497030 | 20 | folded | as MW3 0x004942e0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00497080 | 40 | folded | as MW3 0x00494330: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00497150 | 12 | unlabelled |  |
| 0x00497270 | 222 | unlabelled |  |
| 0x00497720 | 226 | unlabelled |  |
| 0x00497f40 | 15 | folded | covered by the port where it names it (src/mech3/mp_connect.cpp:33) |
| 0x00497f70 | 30 | folded | as MW3 0x00495240: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00498020 | 24 | unlabelled |  |
| 0x00498190 | 71 | unlabelled |  |
| 0x00498300 | 108 | unlabelled |  |
| 0x004987f0 | 9 | folded | as MW3 0x00495b00: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x004988a0 | 38 | folded | covered by the port where it names it (src/mech3/pilot.h:567) |
| 0x00498920 | 9 | folded | as MW3 0x00495d10: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00498930 | 18 | folded | covered by the port where it names it (src/mech3/app_mech.h:198) |
| 0x00498960 | 29 | folded | as MW3 0x00495d70: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x004989f0 | 28 | folded | as MW3 0x00495df0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00498a60 | 33 | folded | covered by the port where it names it (src/mech3/mp_lobby.cpp:1411) |
| 0x00498af0 | 38 | folded | as MW3 0x00495ee0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00498b70 | 274 | unlabelled |  |
| 0x00499550 | 17 | folded | inside its ported callers (0x00498b70): a small helper their ports do their own way |
| 0x00499580 | 34 | folded | as MW3 0x004967e0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00499610 | 14 | folded | as MW3 0x004968d0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00499650 | 12 | unlabelled |  |
| 0x00499670 | 12 | unlabelled |  |
| 0x00499690 | 12 | unlabelled |  |
| 0x004996b0 | 12 | unlabelled |  |
| 0x004996d0 | 12 | unlabelled |  |
| 0x004996f0 | 12 | unlabelled |  |
| 0x00499710 | 12 | unlabelled |  |
| 0x00499730 | 12 | unlabelled |  |
| 0x00499750 | 12 | unlabelled |  |
| 0x00499770 | 28 | folded | as MW3 0x00496a70: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x004997f0 | 28 | folded | as MW3 0x00496af0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00499870 | 12 | folded | covered by the port where it names it (src/mech3/mp_lobby.h:87) |
| 0x00499890 | 109 | unlabelled |  |
| 0x00499be0 | 28 | folded | as MW3 0x00496e20: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00499c60 | 28 | folded | as MW3 0x00496ea0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00499ce0 | 28 | folded | as MW3 0x00496f20: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00499d60 | 28 | folded | as MW3 0x00496fa0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00499de0 | 28 | folded | as MW3 0x00497020: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00499e60 | 28 | folded | as MW3 0x004970a0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00499ee0 | 73 | unlabelled |  |
| 0x0049a070 | 28 | folded | as MW3 0x004972d0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049a0f0 | 28 | folded | as MW3 0x00497350: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049a170 | 12 | unlabelled |  |
| 0x0049a190 | 12 | unlabelled |  |
| 0x0049a1b0 | 103 | unlabelled |  |
| 0x0049b3c0 | 15 | folded | inside its ported callers (0x0049a3c0): a small helper their ports do their own way |
| 0x0049bb50 | 13 | folded | as MW3 0x00499010: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049c880 | 93 | folded | as MW3 0x00499280: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049cd50 | 23 | unlabelled |  |
| 0x0049cf00 | 20 | unlabelled |  |
| 0x0049cf60 | 21 | unlabelled |  |
| 0x0049cff0 | 85 | unlabelled |  |
| 0x0049d130 | 9 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x0049d140 | 19 | folded | as MW3 0x00499a00: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049d170 | 22 | unlabelled |  |
| 0x0049db50 | 15 | folded | covered by the port where it names it (src/mech3/mp_lobby.cpp:1492) |
| 0x0049db90 | 42 | folded | read: the lobby's new team (MW3 0x0049a300), ported as PM's (mp_lobby.cpp:1314: its data five words, PM's 0x14 bytes for MW3's 8) |
| 0x0049e100 | 40 | folded | as MW3 0x0049a7a0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049e3f0 | 12 | folded | covered by the port where it names it (src/mech3/mp_lobby.cpp:561) |
| 0x0049e950 | 49 | folded | as MW3 0x0049b1c0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049ea10 | 35 | folded | covered by the port where it names it (src/mech3/mp_lobby.cpp:1018) |
| 0x0049ea90 | 33 | folded | covered by the port where it names it (src/mech3/mp_lobby.cpp:1009) |
| 0x0049eb10 | 8 | folded | returns a constant or a global |
| 0x0049ecb0 | 49 | folded | the lobby's own data state write (mp_lobby.cpp:1234, my_data / set_my_data): kind 1 read, word 2 set, written back; PM takes data over 7 bytes, the port 12 or more (the lobby's own data is always longer); not run under a test (trace) |
| 0x0049eea0 | 14 | unlabelled |  |
| 0x0049f810 | 9 | folded | as MW3 0x0049bfe0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049f830 | 12 | unlabelled |  |
| 0x0049f850 | 12 | unlabelled |  |
| 0x0049f870 | 28 | folded | as MW3 0x0049c040: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049f8f0 | 28 | folded | as MW3 0x0049c0c0: the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049f980 | 12 | folded | covered by the port where it names it (src/mech3/mp_lobby.h:248) |
| 0x0049faf0 | 114 | unlabelled |  |
| 0x0049fd60 | 72 | unlabelled |  |
| 0x0049fe90 | 13 | unlabelled |  |
| 0x0049fed0 | 116 | unlabelled |  |
| 0x004a0120 | 54 | folded | read: the multiplayer state's constructor (MW3 0x0049c900, the state pilot_menu.cpp:573 names): its three screens allocated at PM's sizes (0x1443c, 0x167b8, 0x123d8 for MW3's 0x13b04, 0x1505c, 0x11f3c), one more field cleared, its vtable PM's twin; the port's screens are its own classes |
| 0x004a0210 | 12 | folded | covered by the port where it names it (src/mech3/pilot_menu.cpp:589) |
| 0x004a02b0 | 15 | folded | inside its ported callers (0x0049ef20): a small helper their ports do their own way |
| 0x004a02e0 | 17 | folded | covered by the port where it names it (src/mech3/pilot_menu.cpp:600) |
| 0x004a03c0 | 12 | folded | covered by the port where it names it (src/mech3/pilot_menu.cpp:651) |
| 0x004a03e0 | 45 | unlabelled |  |
| 0x004a04f0 | 15 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x0049d100: MFC plumbing of the editor's dialogs (DDX, UpdateData, OnDestroy, DoModal, a static list) |
| 0x004a0530 | 9 | dev dialog (MFC), its logic: PORT (preserve) | follows 0x00404100: as MW3 0x00404090: the AI tester dialog's walk over a vehicle's nets (0x00404090 -> 0x00403ec0: their extents) |
| 0x004a0540 | 16 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x0049d150: MFC plumbing of the editor's dialogs (DDX, UpdateData, OnDestroy, DoModal, a static list) |
| 0x004a0570 | 15 | folded | as MW3 0x0049d190: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x004a05a0 | 15 | folded | covered by the port where it names it (src/mech3/sound_extra.cpp:32) |
| 0x004a05d0 | 11 | folded | as MW3 0x0049d1f0: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x004a05e0 | 18 | folded | as MW3 0x0049d210: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x004a0620 | 18 | folded | covered by the port where it names it (src/mech3/zainet_node.h:82) |
| 0x004a0660 | 38 | folded | as MW3 0x0049d290: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x004a0730 | 17 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x0049d5d0: MFC plumbing of the editor's dialogs (DDX, UpdateData, OnDestroy, DoModal, a static list) |
| 0x004a0750 | 9 | folded | a thunk: one call to another function |
| 0x004a07a0 | 131 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x0049d350, whose class PM's changes keep: the editor's 'add vehicle to node' (0x00403820 -> 0x0049d350, editor.md); ours does it as meant: upgrades/editor/add_vehicle.cpp |
| 0x004a0a20 | 12 | unlabelled |  |
| 0x004a0a40 | 16 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x0049d660: MFC plumbing of the editor's dialogs (DDX, UpdateData, OnDestroy, DoModal, a static list) |
| 0x004a0ae0 | 8 | folded | covered by the port where it names it (src/mech3/app_states.h:113) |
| 0x004a0af0 | 8 | folded | returns a constant or a global |
| 0x004a0b00 | 26 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x0049d710: the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians |
| 0x004a0ba0 | 19 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x0049d7b0: the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians |
| 0x004a0c00 | 26 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x0049d810: the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians |
| 0x004a0c90 | 12 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x0049d8b0: MFC plumbing of the editor's dialogs (DDX, UpdateData, OnDestroy, DoModal, a static list) |
| 0x004a0cb0 | 24 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x0049d8d0: the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians |
| 0x004a0d00 | 24 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x0049d920: the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians |
| 0x004a0d50 | 25 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x0049d970: the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians |
| 0x004a0da0 | 11 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x0049d9e0: the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians |
| 0x004a0dd0 | 15 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x004a0e10 | 385 | unlabelled |  |
| 0x004a1670 | 12 | folded | covered by the port where it names it (src/mech3/player.cpp:350) |
| 0x004a16b0 | 8 | folded | as MW3 0x0049fe70: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x004a16d0 | 11 | folded | covered by the port where it names it (src/mech3/mech_control.cpp:595) |
| 0x004a1730 | 33 | unlabelled |  |
| 0x004a19c0 | 262 | unlabelled |  |
| 0x004a1f50 | 11 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:631) |
| 0x004a1f90 | 98 | unlabelled |  |
| 0x004a2220 | 318 | unlabelled |  |
| 0x004a2990 | 46 | unlabelled |  |
| 0x004a2a40 | 38 | unlabelled |  |
| 0x004a2bd0 | 27 | unlabelled |  |
| 0x004a2c40 | 55 | unlabelled |  |
| 0x004a2dc0 | 102 | unlabelled |  |
| 0x004a3040 | 26 | unlabelled |  |
| 0x004a3200 | 83 | MISSING LOGIC | pm only, vehicles, traced: PM's vehicle collision record (0x004a3cb0) puts hits on a mech's part nodes (0x0040f210, the part's owner) in a new list +0x358 and other entities' (0x0040f1a0) in +0x354, dropping one whose object's byte +8 is 0; the step (0x004a3200, MW3 0x004a09e0) answers either list with 0x004a4320, which runs the collision response 0x004a3e40 (MW3 0x004a0b40) on +0x354 only, and sets the hit flag (+0x374) for both. So in PM a vehicle that runs into a mech is flagged as hit but gets no response from the mech's parts (MW3 and the port respond to every entity hit: veh_move.cpp vehicle_collision_query, one list); the flag still keeps the ground step's own response (the end of 0x004a3200) from running |
| 0x004a33a0 | 36 | folded | PM's vehicle collision lists freed (both, +0x358 and +0x354) and the empty test over the four lists: the bookkeeping of PM's split, whose effect is labelled at 0x004a3cb0 / 0x004a3200 |
| 0x004a3440 | 357 | unlabelled |  |
| 0x004a3bd0 | 33 | folded | PM's vehicle collision lists freed (both, +0x358 and +0x354) and the empty test over the four lists: the bookkeeping of PM's split, whose effect is labelled at 0x004a3cb0 / 0x004a3200 |
| 0x004a3e00 | 9 | folded | a thunk: one call to another function |
| 0x004a3e20 | 9 | folded | a thunk: one call to another function |
| 0x004a4320 | 9 | folded | a thunk: one call to another function |
| 0x004a45e0 | 222 | unlabelled |  |
| 0x004a4ab0 | 294 | unlabelled |  |
| 0x004a5510 | 137 | unlabelled |  |
| 0x004a5780 | 184 | unlabelled |  |
| 0x004a5b20 | 32 | folded | covered by the port where it names it (src/mech3/veh_move.cpp:156) |
| 0x004a5e90 | 13 | folded | covered by the port where it names it (src/mech3/veh_controls.h:178) |
| 0x004a65a0 | 44 | unlabelled |  |
| 0x004a66b0 | 80 | unlabelled |  |
| 0x004a6810 | 71 | unlabelled |  |
| 0x004a6940 | 32 | debug or dev tool: PORT (preserve) | as MW3 0x004a3550: debug key commands of 0x0040dba0, never registered in the release: a position marker printed to the console (0x004a3220), the mechs listed into the developer lines (0x004a33f0, command 0x89; hud.md), a vehicle picked in a dialog (0x004a3550, its destructor 0x004a35e0) |
| 0x004a69d0 | 20 | debug or dev tool: PORT (preserve) | as MW3 0x004a35e0: debug key commands of 0x0040dba0, never registered in the release: a position marker printed to the console (0x004a3220), the mechs listed into the developer lines (0x004a33f0, command 0x89; hud.md), a vehicle picked in a dialog (0x004a3550, its destructor 0x004a35e0) |
| 0x004a6a20 | 76 | unlabelled |  |
| 0x004a6d30 | 9 | unlabelled |  |
| 0x004a6dd0 | 17 | folded | covered by the port where it names it (src/mech3/zai_firing_pm.cpp:972) |
| 0x004a6e20 | 9 | unlabelled |  |
| 0x004a6e90 | 11 | folded | covered by the port where it names it (src/mech3/zai_firing_pm.cpp:972) |
| 0x004a6ec0 | 11 | folded | checked by test (block trace, 2026-11-13): test_pm_firepoints calls it (an aim cone's constructor, two vectors and the cone set) with the aim cone 0x004a6ee0, floats compared within 1e-5; test_pm_archives / config run it in the loadout reads |
| 0x004a70c0 | 11 | MISSING LOGIC (only data the shipped game lacks reaches it) | pm only, measured: PM's turn limits are a new class (promote1.cpp's CZValidateDir: set 0x004a7390 / 0x004a7420, made by 0x004a6ee0, tested by 0x004a7000 and its thunk 0x004a70c0) in MW3's TurnLimits' place (turret.cpp make/within, MW3 0x004a4b30 and vt+0x08). Run in the emulator against a float copy of the port's (which agrees with MW3's on all 3,000 cases), PM's test differs on 457 of 3,000 (narrow and wide yaw ranges only; the full and fixed kinds agree), and agrees on 2,999 once the yaw range is negated: a turret's yaw arc is mirrored left to right from MW3's (0x004a7390 passes -yhi, -ylo). No shipped PM turret has LIMITS YAW (aiTurrets.zrd: 42 turrets, none; FIRE_LIMITS is the bursts'), the constructor's full turn and the vehicles' full-yaw gun pitch agree, so only a mod's turret with an uneven YAW shows it. tests/differential/explore/pm_limits_cmp.py (2026-11-13) |
| 0x004a70e0 | 26 | folded | covered by the port where it names it (src/mech3/veh_basic_object.cpp:684) |
| 0x004a7170 | 23 | MISSING LOGIC | pm only, with bug fixes off: PM's limit clamp (0x004a71f0, with 0x004a7170; from the vehicles' vt+0x90 0x004b10b0, MW3's 0x004ad7c0) gives the limit's direction along the heading's own length, hypot(x, z): run in the emulator against MW3's 0x004a41b0 on vehicle limits (gun pitch, full yaw), the answer agrees on all 2,000 and the direction on every one within the limits, and past them PM's is the unit direction at the limit where MW3's has its hypot(x, y) typo. The port's TurretLimits::clamp (veh_controls.cpp) gives PM's with zfix's fixes on and MW3's typo with them off, and PM's AI reads it (zai_firing_pm.cpp plan_turn: there // dot(out, dir) >= cone), so on the pm branch the original path should be PM's formula unswitched. tests/differential/explore/pm_clamp_cmp.py (2026-11-13) |
| 0x004a71f0 | 54 | MISSING LOGIC | pm only, with bug fixes off: PM's limit clamp (0x004a71f0, with 0x004a7170; from the vehicles' vt+0x90 0x004b10b0, MW3's 0x004ad7c0) gives the limit's direction along the heading's own length, hypot(x, z): run in the emulator against MW3's 0x004a41b0 on vehicle limits (gun pitch, full yaw), the answer agrees on all 2,000 and the direction on every one within the limits, and past them PM's is the unit direction at the limit where MW3's has its hypot(x, y) typo. The port's TurretLimits::clamp (veh_controls.cpp) gives PM's with zfix's fixes on and MW3's typo with them off, and PM's AI reads it (zai_firing_pm.cpp plan_turn: there // dot(out, dir) >= cone), so on the pm branch the original path should be PM's formula unswitched. tests/differential/explore/pm_clamp_cmp.py (2026-11-13) |
| 0x004a7300 | 26 | folded | covered by the port where it names it (src/mech3/veh_basic_object.cpp:684) |
| 0x004a7390 | 23 | MISSING LOGIC (only data the shipped game lacks reaches it) | pm only, measured: PM's turn limits are a new class (promote1.cpp's CZValidateDir: set 0x004a7390 / 0x004a7420, made by 0x004a6ee0, tested by 0x004a7000 and its thunk 0x004a70c0) in MW3's TurnLimits' place (turret.cpp make/within, MW3 0x004a4b30 and vt+0x08). Run in the emulator against a float copy of the port's (which agrees with MW3's on all 3,000 cases), PM's test differs on 457 of 3,000 (narrow and wide yaw ranges only; the full and fixed kinds agree), and agrees on 2,999 once the yaw range is negated: a turret's yaw arc is mirrored left to right from MW3's (0x004a7390 passes -yhi, -ylo). No shipped PM turret has LIMITS YAW (aiTurrets.zrd: 42 turrets, none; FIRE_LIMITS is the bursts'), the constructor's full turn and the vehicles' full-yaw gun pitch agree, so only a mod's turret with an uneven YAW shows it. tests/differential/explore/pm_limits_cmp.py (2026-11-13) |
| 0x004a7420 | 17 | MISSING LOGIC (only data the shipped game lacks reaches it) | pm only, measured: PM's turn limits are a new class (promote1.cpp's CZValidateDir: set 0x004a7390 / 0x004a7420, made by 0x004a6ee0, tested by 0x004a7000 and its thunk 0x004a70c0) in MW3's TurnLimits' place (turret.cpp make/within, MW3 0x004a4b30 and vt+0x08). Run in the emulator against a float copy of the port's (which agrees with MW3's on all 3,000 cases), PM's test differs on 457 of 3,000 (narrow and wide yaw ranges only; the full and fixed kinds agree), and agrees on 2,999 once the yaw range is negated: a turret's yaw arc is mirrored left to right from MW3's (0x004a7390 passes -yhi, -ylo). No shipped PM turret has LIMITS YAW (aiTurrets.zrd: 42 turrets, none; FIRE_LIMITS is the bursts'), the constructor's full turn and the vehicles' full-yaw gun pitch agree, so only a mod's turret with an uneven YAW shows it. tests/differential/explore/pm_limits_cmp.py (2026-11-13) |
| 0x004a7480 | 22 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:237) |
| 0x004a74e0 | 12 | unlabelled |  |
| 0x004a7500 | 9 | debug or dev tool: PORT (preserve) | as MW3 0x004a4ca0: the stealth object (the developers' fly camera; back burner in CLAUDE.md): a slot of its |
| 0x004a7520 | 8 | folded | returns a constant or a global |
| 0x004a7530 | 29 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:238) |
| 0x004a75e0 | 20 | folded | covered by the port where it names it (src/mech3/game_editor_net.h:27) |
| 0x004a7630 | 72 | unlabelled |  |
| 0x004a77f0 | 12 | folded | covered by the port where it names it (src/mech3/app_vehicle.cpp:1084) |
| 0x004a7900 | 9 | debug or dev tool: PORT (preserve) | as MW3 0x004a50b0: the stealth object (the developers' fly camera; back burner in CLAUDE.md): a slot of its |
| 0x004a7930 | 28 | debug or dev tool: PORT (preserve) | as MW3 0x004a50e0: the debug command 0x7e (0x004abbc0, 121 lines: a console of keyboard text and the network session; registered, but no key reaches it: app_keys.h:14, hud.md) and the stealth object's key handler (0x004a50e0: the fly camera, back burner) |
| 0x004a79c0 | 20 | folded | covered by the port where it names it (src/mech3/app_vehicle.cpp:493) |
| 0x004a7a00 | 27 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x004a7e70 | 22 | folded | covered by the port where it names it (src/mech3/veh_stealth.cpp:190) |
| 0x004a8090 | 26 | folded | covered by the port where it names it (src/mech3/veh_stealth.cpp:99) |
| 0x004a8110 | 50 | unlabelled |  |
| 0x004a82a0 | 20 | folded | covered by the port where it names it (src/mech3/app_vehicle.cpp:422) |
| 0x004a8370 | 13 | folded | covered by the port where it names it (src/mech3/veh_stealth.h:22) |
| 0x004a83a0 | 8 | folded | returns a constant or a global |
| 0x004a83c0 | 9 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.cpp:263) |
| 0x004a83e0 | 8 | folded | returns a constant or a global |
| 0x004a8400 | 9 | folded | covered by the port where it names it (src/mech3/veh_controls.cpp:462) |
| 0x004a8440 | 8 | folded | as MW3 0x004a5c50: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004a8a40 | 51 | folded | group A, read: the teleport dialog; PM takes IDOK only (== 1 where MW3 takes any nonzero), which the port already does (teleport.cpp:183-197) |
| 0x004a8b60 | 20 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004046d0: MFC dialog plumbing of dev dialogs (CDialog constructors and destructors, EnableWindow): resource 0x67 and the teleport and net dialogs' destructors |
| 0x004a8c30 | 12 | unlabelled |  |
| 0x004a8cd0 | 8 | folded | returns a constant or a global |
| 0x004a8ce0 | 27 | unlabelled |  |
| 0x004a8f00 | 20 | unlabelled |  |
| 0x004a8f70 | 21 | unlabelled |  |
| 0x004a9370 | 10 | unlabelled |  |
| 0x004a9380 | 13 | folded | ported, by docs/symbols.csv: ported: vt+0x98; 1 active, else 0 (shared by the linker) |
| 0x004a93a0 | 8 | folded | ported, by docs/symbols.csv: ported: vt+0x3c; [0x609a38 + 4 side] |
| 0x004a93b0 | 12 | folded | covered by the port where it names it (src/mech3/app_turret.cpp:322) |
| 0x004a95f0 | 413 | unlabelled |  |
| 0x004aa080 | 176 | unlabelled |  |
| 0x004aa670 | 116 | folded | group A, read: the same logic and constants (3.0 and 8.0 the turn rate, 1.0, cos 0.89); PM's test reads 'in range and dot >= 0.89' where MW3's reads 'not (out of range or dot <= 0.89)', so only a dot of exactly 0.89 differs; PM passes the node helpers' arguments where MW3 leaves them in registers |
| 0x004aabe0 | 119 | unlabelled |  |
| 0x004aaea0 | 10 | unlabelled |  |
| 0x004aaf40 | 9 | MISSING LOGIC (only data the shipped game lacks reaches it) | follows 0x004a70c0: pm only, measured: PM's turn limits are a new class (promote1.cpp's CZValidateDir: set 0x004a7390 / 0x004a7420, made by 0x004a6ee0, tested by 0x004a7000 and its thunk 0x004a70c0) in MW3's TurnLimits' place (turret.cpp make/within, MW3 0x004a4b30 and vt+0x08). Run in the emulator against a float copy of the port's (which agrees with MW3's on all 3,000 cases), PM's test differs on 457 of 3,000 (narrow and wide yaw ranges only; the full and fixed kinds agree), and agrees on 2,999 once the yaw range is negated: a turret's yaw arc is mirrored left to right from MW3's (0x004a7390 passes -yhi, -ylo). No shipped PM turret has LIMITS YAW (aiTurrets.zrd: 42 turrets, none; FIRE_LIMITS is the bursts'), the constructor's full turn and the vehicles' full-yaw gun pitch agree, so only a mod's turret with an uneven YAW shows it. tests/differential/explore/pm_limits_cmp.py (2026-11-13) |
| 0x004aaf60 | 9 | folded | a thunk: one call to another function |
| 0x004aaf80 | 9 | MISSING LOGIC | follows 0x004a71f0: pm only, with bug fixes off: PM's limit clamp (0x004a71f0, with 0x004a7170; from the vehicles' vt+0x90 0x004b10b0, MW3's 0x004ad7c0) gives the limit's direction along the heading's own length, hypot(x, z): run in the emulator against MW3's 0x004a41b0 on vehicle limits (gun pitch, full yaw), the answer agrees on all 2,000 and the direction on every one within the limits, and past them PM's is the unit direction at the limit where MW3's has its hypot(x, y) typo. The port's TurretLimits::clamp (veh_controls.cpp) gives PM's with zfix's fixes on and MW3's typo with them off, and PM's AI reads it (zai_firing_pm.cpp plan_turn: there // dot(out, dir) >= cone), so on the pm branch the original path should be PM's formula unswitched. tests/differential/explore/pm_clamp_cmp.py (2026-11-13) |
| 0x004aafa0 | 9 | folded | a thunk: one call to another function |
| 0x004aafc0 | 8 | folded | ported, by docs/symbols.csv: ported: vt+0xa0 |
| 0x004aafd0 | 16 | unlabelled |  |
| 0x004ab0e0 | 13 | folded | covered by the port where it names it (src/mech3/mech_command.cpp:195) |
| 0x004ab1d0 | 11 | folded | covered by the port where it names it (src/mech3/sensors.cpp:142) |
| 0x004ab1f0 | 18 | folded | as MW3 0x004a7c90: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004ab230 | 17 | folded | covered by the port where it names it (src/mech3/sensors.cpp:247) |
| 0x004ab260 | 87 | folded | as MW3 0x004a7d00: covered where the port does it: src/mech3/sensors.cpp:142 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004ab480 | 42 | folded | read: the same loop as MW3's 0x004a7cd0 walk (the first match taken and kept at +8), PM's breaking where MW3's tests in the condition |
| 0x004ab720 | 12 | folded | covered by the port where it names it (src/gamez/znetwork/znet_object.h:8) |
| 0x004ab750 | 73 | unlabelled |  |
| 0x004ab910 | 67 | folded | as MW3 0x004a83c0: covered where the port does it: src/mech3/sensors.cpp:457 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004aba80 | 85 | unlabelled |  |
| 0x004abc30 | 12 | folded | as MW3 0x004a86e0: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004abc60 | 93 | MISSING LOGIC | pm only, minor, sensors: PM's sensor frame set-up (0x004abc60, MW3's 0x004a8710, inside the update 0x004ac1e0 / MW3 0x004a8cd0) adds the player and handles the stealth but not the view target; PM's per-entity add (0x004ac6c0, MW3 0x004a91b0) skips only the player and the stealth, so a viewed vehicle that isn't the player comes in list order and only when its vehicle answers vt+0xd8. MW3 adds the view second, whatever its answer, and the port does as MW3 (sensors.cpp:482-485, and the loop's skip of the view at :487); the order sets which pairs run first (the player's, the lancemates', the rest) |
| 0x004abe80 | 98 | folded | read: MW3's 0x004a8990 unpaired by the diff (a vehicle added to the sensor frame: its records grown, the entity's made), called where MW3's set-up calls it (0x004abc60) |
| 0x004ac130 | 27 | folded | traced: the sensors' static contact vector (32 slots), MW3's 0x004a8c30 with each slot built through a helper; the port's sensors keep their own container (sensors.cpp:179) |
| 0x004ac190 | 9 | folded | a thunk: one call to another function |
| 0x004ac1e0 | 237 | unlabelled |  |
| 0x004ac6c0 | 50 | folded | as MW3 0x004a91b0: covered where the port does it: src/mech3/sensors.cpp:179 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004ac7b0 | 93 | unlabelled |  |
| 0x004ac9d0 | 22 | folded | read: the pair pass's side test (whether a contact of that kind counts: shown-all, the vehicle's vt+0xc0, the records' side bits), MW3's inline in 0x004a92c0 (the port's pair) |
| 0x004acb90 | 107 | unlabelled |  |
| 0x004acdc0 | 18 | folded | covered by the port where it names it (src/mech3/sensors.cpp:277) |
| 0x004ace10 | 29 | folded | covered by the port where it names it (src/mech3/sensors.cpp:281) |
| 0x004ace80 | 16 | folded | read: a contact made (0x004aca30, labelled there) and pushed on the sensing record's list; the wrapper's own steps are MW3's |
| 0x004acec0 | 18 | MISSING LOGIC | pm only, sensors, the same rule as 0x004ad2b0: the range test of the player's and the lancemates' passes (0x004acb90 / 0x004acff0, MW3 0x004a9680 / 0x004a9a60): sensed when everything is shown, or within range, or when the sensing record has bit 8 (a C3 master) and the other bit 0x10 (a C3 slave), at any range; the port's in_range (sensors.cpp:85) has the first two only |
| 0x004acf00 | 43 | MISSING LOGIC | pm only, sensors, the same rule as 0x004ad2b0 for the other direction (the other vehicle sensing the player or the lancemate): within range, everything shown, or C3 master and slave at any range; then MW3's side tests for the contact's kind |
| 0x004acff0 | 143 | unlabelled |  |
| 0x004ad5c0 | 108 | MISSING LOGIC | pm only, sensors, with 0x004ad2b0's C3 rules: PM's C3 merge (0x004ad5c0, MW3 0x004a9fd0) puts into a master's list only what its C3 slaves saw (the other record's bit 0x10); MW3's merges every C3 unit's, masters too (bits 0x18), and the port does as MW3 (c3_merge, sensors.cpp:373) |
| 0x004ad840 | 55 | folded | read: the C3 merge's contact copy for the union (a contact taken over into the master's list, its box and distance from the master's vehicle), the steps MW3's merge has inline (c3_contact, sensors.cpp); with 0x004ad5c0 |
| 0x004ada00 | 16 | folded | ported, by docs/symbols.csv: ported: a null contact, or one whose vehicle is the one going |
| 0x004ada30 | 16 | folded | ported, by docs/symbols.csv: ported: one entity's vehicle (vt+0x2c) told (vt+0xd4) |
| 0x004adc90 | 19 | folded | as MW3 0x004aa740: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004adcd0 | 11 | unlabelled |  |
| 0x004add00 | 11 | folded | as MW3 0x004aa9a0: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004add30 | 16 | folded | as MW3 0x004aa9d0: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004add70 | 100 | folded | read: a vector's insert of n copies (the template instantiated for 4-byte contacts), container code; the port's std::vector |
| 0x004adfd0 | 138 | folded | read: a vector's insert of n copies for the sensors' 0x44-byte records (MW3 0x004aaa10, the same template), its element copy 17 words; container code |
| 0x004ae370 | 120 | unlabelled |  |
| 0x004ae570 | 58 | folded | as MW3 0x004ab0f0: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004ae670 | 120 | unlabelled |  |
| 0x004ae850 | 12 | folded | inside its ported callers (0x004ac1e0): a small helper their ports do their own way |
| 0x004ae870 | 102 | unlabelled |  |
| 0x004aea00 | 11 | folded | as MW3 0x004ab1d0: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aea20 | 15 | folded | as MW3 0x0054bde0: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x004aea50 | 140 | folded | as MW3 0x004ab1f0: covered where the port does it: src/crt/msvc_sort.h:8 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004aecf0 | 69 | folded | read: the C3 merge's contact copy for the union (a contact taken over into the master's list, its box and distance from the master's vehicle), the steps MW3's merge has inline (c3_contact, sensors.cpp); with 0x004ad5c0 |
| 0x004aee60 | 10 | folded | covered by the port where it names it (src/mech3/sensors.cpp:403) |
| 0x004aee80 | 18 | folded | covered by the port where it names it (src/mech3/sensors.cpp:403) |
| 0x004aeeb0 | 75 | folded | as MW3 0x004ab500: covered where the port does it: src/crt/msvc_sort.h:7 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004aefe0 | 124 | unlabelled |  |
| 0x004af210 | 44 | unlabelled |  |
| 0x004af2d0 | 43 | unlabelled |  |
| 0x004af360 | 25 | unlabelled |  |
| 0x004af510 | 22 | folded | covered by the port where it names it (src/mech3/mech_build.cpp:327) |
| 0x004af6b0 | 9 | folded | inside its ported callers (0x004af590): a small helper their ports do their own way |
| 0x004af770 | 57 | unlabelled |  |
| 0x004af9f0 | 15 | folded | read: MW3's music play 0x004ac290 unpaired by the diff (the request kept, the track played when CD music is on, else stopped), the port's music::play (music.cpp:17) |
| 0x004afb30 | 31 | unlabelled |  |
| 0x004afc30 | 13 | folded | as MW3 0x004ac2c0: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004afc60 | 9 | folded | a thunk: one call to another function |
| 0x004afc90 | 42 | unlabelled |  |
| 0x004afec0 | 55 | unlabelled |  |
| 0x004b00e0 | 9 | folded | covered by the port where it names it (src/mech3/veh_controls.cpp:145) |
| 0x004b00f0 | 27 | unlabelled |  |
| 0x004b01f0 | 22 | folded | as MW3 0x004ac860: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004b0220 | 9 | folded | frees the object (a destructor's release) |
| 0x004b0560 | 67 | unlabelled |  |
| 0x004b0790 | 76 | MISSING LOGIC | a PM value the shared port keeps as MW3's (pm only): the vehicle's above camera pitched -81 degrees (-1.413716) in PM, -90 in MW3; its 150 m the same in both (veh_basic_object.cpp:500) |
| 0x004b0c70 | 33 | folded | group A, read: the same logic: a callee renamed by the map (0x0051da90 = PM 0x00537420), a direct call made virtual (0x0041bd80 through vt+0x168), PM's layout (+0xe8 for +0xe4, a null check added), a destructor on PM's members |
| 0x004b0d90 | 71 | unlabelled |  |
| 0x004b1010 | 9 | folded | covered by the port where it names it (src/mech3/veh_controls.h:224) |
| 0x004b1030 | 13 | unlabelled |  |
| 0x004b10b0 | 9 | unlabelled |  |
| 0x004b1130 | 53 | folded | as MW3 0x004ad840: covered where the port does it: src/mech3/veh_basic_object.h:38 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004b1220 | 72 | unlabelled |  |
| 0x004b1390 | 9 | folded | follows 0x004b1130: as MW3 0x004ad840: covered where the port does it: src/mech3/veh_basic_object.h:38 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004b13a0 | 9 | folded | a thunk: one call to another function |
| 0x004b13c0 | 19 | folded | as MW3 0x004ade90: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b1400 | 12 | unlabelled |  |
| 0x004b1420 | 10 | folded | covered by the port where it names it (src/mech3/veh_basic_object.cpp:241) |
| 0x004b1440 | 9 | unlabelled |  |
| 0x004b1470 | 9 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.cpp:138) |
| 0x004b14d0 | 12 | folded | covered by the port where it names it (src/mech3/veh_basic_object.cpp:143) |
| 0x004b14f0 | 18 | folded | covered by the port where it names it (src/mech3/veh_basic_object.cpp:143) |
| 0x004b1530 | 15 | folded | covered by the port where it names it (src/mech3/veh_basic_object.cpp:154) |
| 0x004b16d0 | 10 | folded | covered by the port where it names it (src/mech3/app_sensors.cpp:145) |
| 0x004b16f0 | 11 | folded | ported, by docs/symbols.csv: ported: IControls +0x04 zeroed |
| 0x004b17c0 | 12 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.cpp:258) |
| 0x004b17e0 | 148 | folded | as MW3 0x004aded0: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b1b80 | 31 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004ae270, whose class PM's changes keep: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md) |
| 0x004b1c40 | 8 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004ae340: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md) |
| 0x004b1c50 | 8 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004ae350: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md) |
| 0x004b1c60 | 16 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004ae380: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md) |
| 0x004b1ca0 | 101 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004ae3c0: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md) |
| 0x004b1ec0 | 22 | MISSING LOGIC (only data the shipped game lacks reaches it) | called only from 0x004b2280, which are missing-mod |
| 0x004b1f80 | 8 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004ae6a0: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md) |
| 0x004b1f90 | 51 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004ae6b0: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md) |
| 0x004b2190 | 9 | MISSING LOGIC (only data the shipped game lacks reaches it) | called only from 0x004b1f90, which are missing-mod |
| 0x004b21a0 | 9 | folded | a thunk: one call to another function |
| 0x004b21c0 | 9 | folded | as MW3 0x004c02a0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004b21d0 | 21 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004ae8e0: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md) |
| 0x004b2220 | 9 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004ae950: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md) |
| 0x004b2230 | 8 | folded | returns a constant or a global |
| 0x004b2240 | 11 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004ae930: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md) |
| 0x004b2260 | 14 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004ae960: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md) |
| 0x004b2280 | 23 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004ae980: the basic vehicle's own controls (vtable 0x005c8c50): their constructor 0x004ae270 (the port logs 'has no AI controls: its own are not ported'), the zrd settings 0x004ae3c0 / 0x004ae6b0, the path state and members, the destructor. Deferred: no shipped mission makes such a vehicle (vehicles.md) |
| 0x004b2410 | 12 | unlabelled |  |
| 0x004b24e0 | 12 | folded | covered by the port where it names it (src/mech3/zai_attackers_pm.cpp:52) |
| 0x004b2510 | 9 | folded | covered by the port where it names it (src/mech3/zai_attackers_pm.cpp:13) |
| 0x004b2530 | 26 | folded | covered by the port where it names it (src/mech3/zai_attackers_pm.cpp:13) |
| 0x004b2580 | 19 | folded | covered by the port where it names it (src/mech3/mech_control.cpp:696) |
| 0x004b2940 | 9 | folded | ported, by docs/symbols.csv: ported: HitRecord::within(1) (the manoeuvre's direct call) |
| 0x004b2980 | 12 | unlabelled |  |
| 0x004b29a0 | 26 | folded | as MW3 0x004c5090: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004b2a00 | 27 | unlabelled |  |
| 0x004b2a70 | 17 | folded | covered by the port where it names it (src/mech3/zai_brain.cpp:1619) |
| 0x004b2ab0 | 11 | folded | ported, by docs/symbols.csv: ported: MechControls::on_death (vt+0xa8: the tick once more) |
| 0x004b2ac0 | 10 | folded | covered by the port where it names it (src/mech3/mech.h:162) |
| 0x004b2ad0 | 11 | folded | as MW3 0x004aed70: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b2ae0 | 11 | folded | covered by the port where it names it (src/mech3/zai_brain.h:765) |
| 0x004b2af0 | 20 | folded | covered by the port where it names it (src/mech3/app_sensors.cpp:128) |
| 0x004b2b10 | 26 | folded | covered by the port where it names it (src/mech3/app_sensors.cpp:125) |
| 0x004b2c90 | 25 | folded | covered by the port where it names it (src/mech3/app_sensors.cpp:142) |
| 0x004b2ce0 | 22 | folded | as MW3 0x004aee70: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b2d70 | 15 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:2266) |
| 0x004b2da0 | 9 | folded | inside its ported callers (0x0041b930): a small helper their ports do their own way |
| 0x004b2dc0 | 22 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004aef00: the vehicle-pick dialog's MFC plumbing |
| 0x004b2e20 | 12 | unlabelled |  |
| 0x004b2e40 | 9 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004aef80: the vehicle-pick dialog's MFC plumbing |
| 0x004b2e60 | 8 | folded | returns a constant or a global |
| 0x004b2e70 | 18 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004aefb0: the vehicle-pick dialog's MFC plumbing |
| 0x004b2ec0 | 30 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x004af000: a vehicle-pick dev dialog (resource 0x8f, from the debug key's 0x004a3550): its list of zEntity's vehicles |
| 0x004b2f60 | 9 | unlabelled |  |
| 0x004b2f70 | 47 | unlabelled |  |
| 0x004b3020 | 33 | folded | covered by the port where it names it (src/mech3/weapon.cpp:1327) |
| 0x004b3080 | 19 | replaced (SDL or the platform layer) | as MW3 0x004af1a0: Win32: the exe's version info for the window title and menus (GetFileVersionInfoA; protection.md) and the OK/Cancel MessageBoxExA helper (the game's own box replaces it, W16) |
| 0x004b3110 | 8 | unlabelled |  |
| 0x004b3120 | 14 | unlabelled |  |
| 0x004b3180 | 9 | unlabelled | follows 0x004b2f70:  |
| 0x004b3190 | 9 | folded | a thunk: one call to another function |
| 0x004b31b0 | 9 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x004b31c0 | 9 | folded | a thunk: one call to another function |
| 0x004b31d0 | 9 | unlabelled | follows 0x004b3110:  |
| 0x004b31e0 | 130 | unlabelled |  |
| 0x004b34d0 | 200 | folded | group A, read: weapons.zrd's reader as the shared port; every key PM reads (SHOW_ON_RADAR, SALVO and its count, delta, radius, delay, truecount, BURST, ISNARC, IMPACT_FORCE, RECOIL, ZAPS_HUD, U_FIRE_TIME, U_RECHARGE_TIME, U_RECHARGE_SND, U_DESIGNATE_AIRSTRIKE, U_LOOPED) is read in weapon.cpp; MW3 reads them too (the strings referenced another way) |
| 0x004b3c80 | 20 | folded | as MW3 0x004afe10: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b3d60 | 48 | unlabelled |  |
| 0x004b43a0 | 52 | unlabelled |  |
| 0x004b4480 | 52 | unlabelled |  |
| 0x004b4560 | 39 | MISSING LOGIC | pm only, weapons: selecting a weapon group (0x004b4560, PM's in MW3's 0x004b0730 place, the port's WeaponManager::select_group, weapon.cpp:650): when the group's current weapon is not ready (0x004b7bd0, MW3's 0x004b3a20), PM moves the group on to its next ready weapon (0x004b5e10, MW3's 0x004b1f40) before the HUD's refresh; MW3 and the port keep the current one. Reached from the group keys, the mech build, the weapon planner and two weapon manager steps |
| 0x004b4690 | 13 | folded | covered by the port where it names it (src/mech3/mech_command.cpp:230) |
| 0x004b4a80 | 10 | folded | covered by the port where it names it (src/mech3/app_weapons.cpp:554) |
| 0x004b4a90 | 122 | unlabelled |  |
| 0x004b4f10 | 50 | unlabelled |  |
| 0x004b4fe0 | 35 | unlabelled |  |
| 0x004b5040 | 76 | unused: PORT (preserve) | as MW3 0x004b1170: the static the AI's planner builds at 0x0060b240 (two reserved vectors): read by nothing (weapon_planner.h:7) |
| 0x004b51a0 | 9 | unused: PORT (preserve) | called only from 0x004b5040, which are unused |
| 0x004b51b0 | 9 | folded | a thunk: one call to another function |
| 0x004b5210 | 13 | folded | ported, by docs/symbols.csv: ported inline; Shot's initialisers |
| 0x004b5300 | 14 | folded | ported, by docs/symbols.csv: ported; group_out |
| 0x004b5330 | 116 | MISSING LOGIC | pm only, AI: the volley planner's weapon test (weapon_planner.cpp:92 shot_of and add_weapon, through in_range's type_ready()): MW3's 0x004b4ba0 needs the weapon's owner, its definition and the reload time passed ([0x0060991c] >= def+0xc4, 0x004b4c70); PM's 0x004b8ce0 needs only the owner. So PM's burst record 0x004b5230 counts a reloading weapon's damage and PM's add-weapon 0x004b5330 takes it as a candidate and into its group's sums; the port keeps MW3's ready test on PM. Reached through PM's volley 0x004b56d0 (MW3 0x004b1860) |
| 0x004b5640 | 44 | folded | ported, by docs/symbols.csv: ported inline; the groups left (remove_if) |
| 0x004b56d0 | 145 | unlabelled |  |
| 0x004b59f0 | 12 | folded | as MW3 0x004b1ba0: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b5bc0 | 13 | folded | covered by the port where it names it (src/mech3/weapon.h:124) |
| 0x004b5bf0 | 37 | folded | inside its ported callers (0x004bb700): a small helper their ports do their own way |
| 0x004b5e10 | 52 | unlabelled |  |
| 0x004b5ea0 | 48 | unlabelled |  |
| 0x004b5f80 | 110 | MISSING LOGIC | pm only, weapons, group fire (mode 2): PM's group trigger (0x004b5f80, in MW3's 0x004b2100 place, the port's WeaponGroup::trigger, weapon.cpp:455) fires the best-rated beam first, then each other weapon telling zWeapon whether its definition already fired in this volley (0x005a3680, MW3's 0x00581f90, the port's set_fire_animation): the first of a kind plays its FIRE animation, the repeats its second one (fire.animation2, zwep_shot.cpp:341). The port's trigger fires in order and never sets the choice, so every weapon of a kind plays the first animation; the beams' ground effect is cleared as MW3's |
| 0x004b64f0 | 142 | unlabelled |  |
| 0x004b67a0 | 14 | folded | covered by the port where it names it (src/mech3/weapon.h:181) |
| 0x004b67b0 | 16 | unlabelled |  |
| 0x004b67e0 | 14 | folded | read: a vector's size (0 for none), container code |
| 0x004b6800 | 12 | folded | covered by the port where it names it (src/mech3/weapon.h:176) |
| 0x004b6820 | 140 | unlabelled |  |
| 0x004b6ad0 | 65 | unlabelled |  |
| 0x004b6e80 | 44 | MISSING LOGIC | pm only, weapons, the player's: PM's per-weapon enable (0x004b6e80, in MW3's 0x004b2f90 place, the port's Weapon::set_enabled, weapon.cpp:1310) adds a firing cone: for the player's own mech it runs the weapon's line to the aim point (0x004b7800 on the manager's +0x0c, the pm port's weapon_line, zai_firing_pm.cpp:922) and switches the weapon off when the cone clamps it (the line's cosine differs from the kept one). The water and side tests are MW3's. The port's set_enabled has only those two, so on PM the player's weapons that cannot bear on the aim point still fire |
| 0x004b6f60 | 8 | folded | returns a constant or a global |
| 0x004b6f70 | 69 | unlabelled |  |
| 0x004b72d0 | 14 | unlabelled |  |
| 0x004b73f0 | 94 | unlabelled |  |
| 0x004b7600 | 42 | unlabelled |  |
| 0x004b76d0 | 54 | unlabelled |  |
| 0x004b7a90 | 17 | folded | inside its ported callers (0x004bcd40): a small helper their ports do their own way |
| 0x004b7ac0 | 16 | unlabelled |  |
| 0x004b7b10 | 9 | unlabelled | follows 0x004b7ac0:  |
| 0x004b7da0 | 44 | folded | group A, read: a weapon's constructor as the shared port; the per-second numbers MW3 works out inline PM keeps in helpers (0x004b8e70 the damage, 0x004b8f10 the heat by the second at +0x1c), covered with PM's firing (zai_wire_firing_pm.cpp:111) |
| 0x004b7e70 | 8 | folded | covered by the port where it names it (src/mech3/weapon.h:218) |
| 0x004b7e80 | 12 | folded | covered by the port where it names it (src/mech3/weapon.h:216) |
| 0x004b7ea0 | 45 | folded | group A, read: a weapon's constructor as the shared port; the per-second numbers MW3 works out inline PM keeps in helpers (0x004b8e70 the damage, 0x004b8f10 the heat by the second at +0x1c), covered with PM's firing (zai_wire_firing_pm.cpp:111) |
| 0x004b8030 | 39 | unlabelled |  |
| 0x004b80f0 | 81 | unlabelled |  |
| 0x004b8310 | 29 | unlabelled |  |
| 0x004b87c0 | 104 | unlabelled |  |
| 0x004b8ad0 | 9 | folded | a thunk: one call to another function |
| 0x004b8ae0 | 9 | folded | a thunk: one call to another function |
| 0x004b8c20 | 11 | unlabelled |  |
| 0x004b8ce0 | 8 | MISSING LOGIC | pm only, AI: the volley planner's weapon test (weapon_planner.cpp:92 shot_of and add_weapon, through in_range's type_ready()): MW3's 0x004b4ba0 needs the weapon's owner, its definition and the reload time passed ([0x0060991c] >= def+0xc4, 0x004b4c70); PM's 0x004b8ce0 needs only the owner. So PM's burst record 0x004b5230 counts a reloading weapon's damage and PM's add-weapon 0x004b5330 takes it as a candidate and into its group's sums; the port keeps MW3's ready test on PM. Reached through PM's volley 0x004b56d0 (MW3 0x004b1860) |
| 0x004b8d20 | 11 | folded | covered by the port where it names it (src/mech3/mech_salvage.h:37) |
| 0x004b8d30 | 11 | folded | covered by the port where it names it (src/mech3/weapon.h:119) |
| 0x004b8df0 | 9 | folded | a thunk: one call to another function |
| 0x004b8ed0 | 27 | folded | inside its ported callers (0x004b64f0, 0x004b6820): a small helper their ports do their own way |
| 0x004b8f10 | 25 | folded | inside its ported callers (0x004b7da0, 0x004b7ea0): a small helper their ports do their own way |
| 0x004b8f50 | 16 | folded | as MW3 0x004b4ed0: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b9210 | 54 | folded | as MW3 0x004b4f00: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b92f0 | 11 | folded | as MW3 0x0054bb00: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x004b9320 | 28 | folded | as MW3 0x004c5180: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004b9360 | 24 | folded | as MW3 0x004c51c0: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004b9390 | 17 | folded | read: a volley record's copy (5 words, nothing for a null slot), container code for the planner's vector |
| 0x004b93b0 | 10 | folded | as MW3 0x004b4fe0: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b93d0 | 12 | folded | covered by the port where it names it (src/mech3/weapon.h:12) |
| 0x004b93f0 | 15 | folded | covered by the port where it names it (src/mech3/lab_view.cpp:53) |
| 0x004b9420 | 9 | folded | as MW3 0x004b5050: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b9430 | 9 | folded | a thunk: one call to another function |
| 0x004b9450 | 9 | unlabelled | follows 0x00506200:  |
| 0x004b9470 | 45 | folded | as MW3 0x004b5060: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b95e0 | 10 | folded | as MW3 0x004b51b0: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b9600 | 10 | folded | as MW3 0x004b51d0: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b9670 | 9 | folded | follows 0x004b96b0: as MW3 0x004b5260: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b9680 | 9 | folded | a thunk: one call to another function |
| 0x004b96a0 | 9 | folded | a base class's plain destructor: it sets the vtable (MW3 0x005bc0e0's twin, vtables.json), which MW3 inlines (MW3's deleting one 0x00409af0 is PM 0x004071e0); not run under a test (trace) |
| 0x004b96b0 | 11 | folded | as MW3 0x004b5260: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b96d0 | 9 | folded | a thunk: one call to another function |
| 0x004b96e0 | 14 | folded | as MW3 0x004b5290: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b9710 | 13 | unlabelled |  |
| 0x004b9740 | 56 | folded | as MW3 0x004b52e0: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b98a0 | 50 | unlabelled |  |
| 0x004b99c0 | 8 | folded | as MW3 0x004b5560: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b99e0 | 16 | unlabelled |  |
| 0x004b9a10 | 15 | unlabelled |  |
| 0x004b9a50 | 23 | folded | as MW3 0x004b5580: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b9ac0 | 12 | unlabelled |  |
| 0x004b9ae0 | 12 | unlabelled |  |
| 0x004b9b00 | 20 | folded | as MW3 0x004b5630: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b9b50 | 9 | folded | as MW3 0x004b5680: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b9b70 | 8 | folded | returns a constant or a global |
| 0x004b9d40 | 8 | folded | inside its ported callers (0x004b9de0, 0x004ba9e0, 0x004bcd40, 0x004bd1c0): a small helper their ports do their own way |
| 0x004b9d50 | 8 | folded | as MW3 0x00409850: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x004b9d60 | 8 | folded | inside its ported callers (0x004ba9e0): a small helper their ports do their own way |
| 0x004b9d70 | 8 | folded | inside its ported callers (0x004ba9e0): a small helper their ports do their own way |
| 0x004b9d80 | 29 | folded | inside its ported callers (0x004b9de0): a small helper their ports do their own way |
| 0x004b9e40 | 26 | folded | ported, by docs/symbols.csv: ported; line_cone [0x0060b2c8] (10, 30, 10, 200) |
| 0x004b9ee0 | 9 | folded | ported, by docs/symbols.csv: ported; the start-up's call |
| 0x004b9fe0 | 18 | folded | ported, by docs/symbols.csv: ported; volley_kind (0x004b1860 through the volley_pick hook) |
| 0x004ba030 | 24 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_pick runs it real inside the pick 0x004bc2e0, at 53 bits, exact; every target asked for, what faces and sight are asked of, the plan made (its contact, score, flags, part) and the firing's fields compared against the port (zai_firing_pm) |
| 0x004ba0a0 | 9 | folded | as MW3 0x004b5ac0: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004ba0b0 | 9 | folded | as MW3 0x004b5ad0: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004ba0c0 | 9 | folded | as MW3 0x004b5ae0: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004ba110 | 11 | folded | ported, by docs/symbols.csv: ported; [0x0060b2b4] = 0.25 |
| 0x004ba140 | 14 | folded | ported, by docs/symbols.csv: ported; [0x0060b2b8] the rejected record |
| 0x004ba190 | 73 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_pick runs it real inside the pick 0x004bc2e0, at 53 bits, exact; every target asked for, what faces and sight are asked of, the plan made (its contact, score, flags, part) and the firing's fields compared against the port (zai_firing_pm) |
| 0x004ba340 | 100 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_pick runs it real inside the pick 0x004bc2e0, at 53 bits, exact; every target asked for, what faces and sight are asked of, the plan made (its contact, score, flags, part) and the firing's fields compared against the port (zai_firing_pm) |
| 0x004ba5c0 | 9 | folded | as MW3 0x004b6370: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004ba5d0 | 11 | folded | covered by the port where it names it (src/mech3/zai_wire_pm.cpp:263) |
| 0x004ba5f0 | 15 | folded | covered by the port where it names it (src/mech3/zai_brain.h:25) |
| 0x004ba630 | 10 | folded | ported, by docs/symbols.csv: ported; LookAround::start |
| 0x004ba890 | 10 | folded | covered by the port where it names it (src/mech3/zai_brain.cpp:1000) |
| 0x004ba970 | 12 | folded | covered by the port where it names it (src/mech3/zai_firing.cpp:303) |
| 0x004ba990 | 19 | folded | as MW3 0x004b6ac0: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004ba9c0 | 9 | unlabelled |  |
| 0x004baaa0 | 13 | folded | as MW3 0x004b6b10: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004bacc0 | 28 | unlabelled |  |
| 0x004bad40 | 17 | folded | ported, by docs/symbols.csv: ported as a constant: [0x0060b2ec] = 2/100 (and [0x0060b2b0] = cos(atan(2/100)), never read) |
| 0x004bad70 | 20 | folded | ported, by docs/symbols.csv: ported; Aim's initialisers |
| 0x004bb6a0 | 10 | MISSING LOGIC | pm only, AI, traced: a shot that hits an ally (IControls vt+0x98, PM 0x004fbc00 as MW3's 0x004ea500) goes to PM's firing, 0x004bb6a0: the ally kept as the obstacle (+0x28), then PM's stand-down 0x004ba8c0, which keeps the plan when the firing has a name (+0x34 set, +0x30 = +0x10, +0x4c = 0) and otherwise rests, drops the plan, clears the aim and, with the vehicle's +0xd4 set, calls 0x004b4a20(0), phase 5. The port's ally hit (mech_control.cpp:636, zai_brain.h:632) goes to MW3's Firing (obstacle, stand_down: zai_firing.cpp:109), which PM's AI does not use (PM_AI=1 runs PM's firing record, zai_firing_pm.cpp, whose stand-down hook is the plan's end, not this); so under PM's AI an ally hit neither marks the obstacle nor stands the firing down |
| 0x004bb6b0 | 21 | folded | PM's firing, covered by its port (zai_firing_pm.cpp: the step 0x004bb700 at :636, the volley pick 0x004bcd40 at :553, the plan 0x004bc2e0 at :444, the contact 0x004bbe30 at :335, the record 0x004bc930 at :502 and its counts, the shooter's numbers at zai_firing_pm.h:146) and its container code (vector insert and erase, find_if, remove_copy_if, std::sort 0x004bd920) |
| 0x004bbdc0 | 30 | folded | PM's firing, covered by its port (zai_firing_pm.cpp: the step 0x004bb700 at :636, the volley pick 0x004bcd40 at :553, the plan 0x004bc2e0 at :444, the contact 0x004bbe30 at :335, the record 0x004bc930 at :502 and its counts, the shooter's numbers at zai_firing_pm.h:146) and its container code (vector insert and erase, find_if, remove_copy_if, std::sort 0x004bd920) |
| 0x004bbe30 | 34 | folded | PM's firing, covered by its port (zai_firing_pm.cpp: the step 0x004bb700 at :636, the volley pick 0x004bcd40 at :553, the plan 0x004bc2e0 at :444, the contact 0x004bbe30 at :335, the record 0x004bc930 at :502 and its counts, the shooter's numbers at zai_firing_pm.h:146) and its container code (vector insert and erase, find_if, remove_copy_if, std::sort 0x004bd920) |
| 0x004bbf20 | 25 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_pick runs it real inside the pick 0x004bc2e0, at 53 bits, exact; every target asked for, what faces and sight are asked of, the plan made (its contact, score, flags, part) and the firing's fields compared against the port (zai_firing_pm) |
| 0x004bbfa0 | 116 | unlabelled |  |
| 0x004bc230 | 17 | folded | inside its ported callers (0x004bbfa0): a small helper their ports do their own way |
| 0x004bc260 | 12 | folded | inside its ported callers (0x004bc2e0): a small helper their ports do their own way |
| 0x004bc2a0 | 12 | folded | PM's firing, covered by its port (zai_firing_pm.cpp: the step 0x004bb700 at :636, the volley pick 0x004bcd40 at :553, the plan 0x004bc2e0 at :444, the contact 0x004bbe30 at :335, the record 0x004bc930 at :502 and its counts, the shooter's numbers at zai_firing_pm.h:146) and its container code (vector insert and erase, find_if, remove_copy_if, std::sort 0x004bd920) |
| 0x004bc8c0 | 10 | folded | inside its ported callers (0x004bb700): a small helper their ports do their own way |
| 0x004bc8e0 | 8 | folded | returns a constant or a global |
| 0x004bc8f0 | 11 | unlabelled |  |
| 0x004bc910 | 9 | folded | a thunk: one call to another function |
| 0x004bc920 | 8 | folded | returns a constant or a global |
| 0x004bc930 | 76 | folded | PM's firing, covered by its port (zai_firing_pm.cpp: the step 0x004bb700 at :636, the volley pick 0x004bcd40 at :553, the plan 0x004bc2e0 at :444, the contact 0x004bbe30 at :335, the record 0x004bc930 at :502 and its counts, the shooter's numbers at zai_firing_pm.h:146) and its container code (vector insert and erase, find_if, remove_copy_if, std::sort 0x004bd920) |
| 0x004bca70 | 21 | unlabelled |  |
| 0x004bcad0 | 96 | folded | PM's firing, covered by its port (zai_firing_pm.cpp: the step 0x004bb700 at :636, the volley pick 0x004bcd40 at :553, the plan 0x004bc2e0 at :444, the contact 0x004bbe30 at :335, the record 0x004bc930 at :502 and its counts, the shooter's numbers at zai_firing_pm.h:146) and its container code (vector insert and erase, find_if, remove_copy_if, std::sort 0x004bd920) |
| 0x004bd170 | 16 | folded | PM's firing, covered by its port (zai_firing_pm.cpp: the step 0x004bb700 at :636, the volley pick 0x004bcd40 at :553, the plan 0x004bc2e0 at :444, the contact 0x004bbe30 at :335, the record 0x004bc930 at :502 and its counts, the shooter's numbers at zai_firing_pm.h:146) and its container code (vector insert and erase, find_if, remove_copy_if, std::sort 0x004bd920) |
| 0x004bd1b0 | 9 | folded | a thunk: one call to another function |
| 0x004bd220 | 11 | unlabelled |  |
| 0x004bd240 | 15 | unlabelled |  |
| 0x004bd280 | 14 | unlabelled |  |
| 0x004bd2b0 | 11 | folded | inside its ported callers (0x004bcd40): a small helper their ports do their own way |
| 0x004bd2e0 | 12 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_pick's pick 0x004bc2e0 runs it; the plan made (contact, score, flags, part) and the firing's fields compared |
| 0x004bd310 | 180 | folded | PM's firing, covered by its port (zai_firing_pm.cpp: the step 0x004bb700 at :636, the volley pick 0x004bcd40 at :553, the plan 0x004bc2e0 at :444, the contact 0x004bbe30 at :335, the record 0x004bc930 at :502 and its counts, the shooter's numbers at zai_firing_pm.h:146) and its container code (vector insert and erase, find_if, remove_copy_if, std::sort 0x004bd920) |
| 0x004bd610 | 30 | folded | PM's firing, covered by its port (zai_firing_pm.cpp: the step 0x004bb700 at :636, the volley pick 0x004bcd40 at :553, the plan 0x004bc2e0 at :444, the contact 0x004bbe30 at :335, the record 0x004bc930 at :502 and its counts, the shooter's numbers at zai_firing_pm.h:146) and its container code (vector insert and erase, find_if, remove_copy_if, std::sort 0x004bd920) |
| 0x004bd650 | 24 | folded | as MW3 0x0054be10: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x004bd680 | 26 | folded | as MW3 0x004b7920: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bd6c0 | 14 | unlabelled |  |
| 0x004bd6f0 | 16 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_pick runs it real inside the pick 0x004bc2e0, at 53 bits, exact; every target asked for, what faces and sight are asked of, the plan made (its contact, score, flags, part) and the firing's fields compared against the port (zai_firing_pm) |
| 0x004bd710 | 31 | folded | as MW3 0x004b7990: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bd760 | 47 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_pick runs it real inside the pick 0x004bc2e0, at 53 bits, exact; every target asked for, what faces and sight are asked of, the plan made (its contact, score, flags, part) and the firing's fields compared against the port (zai_firing_pm) |
| 0x004bd870 | 12 | folded | as MW3 0x004b7ac0: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bd8a0 | 13 | folded | PM's firing, covered by its port (zai_firing_pm.cpp: the step 0x004bb700 at :636, the volley pick 0x004bcd40 at :553, the plan 0x004bc2e0 at :444, the contact 0x004bbe30 at :335, the record 0x004bc930 at :502 and its counts, the shooter's numbers at zai_firing_pm.h:146) and its container code (vector insert and erase, find_if, remove_copy_if, std::sort 0x004bd920) |
| 0x004bd8d0 | 31 | folded | PM's firing, covered by its port (zai_firing_pm.cpp: the step 0x004bb700 at :636, the volley pick 0x004bcd40 at :553, the plan 0x004bc2e0 at :444, the contact 0x004bbe30 at :335, the record 0x004bc930 at :502 and its counts, the shooter's numbers at zai_firing_pm.h:146) and its container code (vector insert and erase, find_if, remove_copy_if, std::sort 0x004bd920) |
| 0x004bd920 | 302 | folded | PM's firing, covered by its port (zai_firing_pm.cpp: the step 0x004bb700 at :636, the volley pick 0x004bcd40 at :553, the plan 0x004bc2e0 at :444, the contact 0x004bbe30 at :335, the record 0x004bc930 at :502 and its counts, the shooter's numbers at zai_firing_pm.h:146) and its container code (vector insert and erase, find_if, remove_copy_if, std::sort 0x004bd920) |
| 0x004bddd0 | 22 | folded | as MW3 0x004b7af0: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bde20 | 132 | unlabelled |  |
| 0x004be050 | 56 | folded | as MW3 0x004b7b40: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004be110 | 37 | folded | as MW3 0x004b7bf0: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004be1a0 | 9 | folded | inside its ported callers (0x004ba750): a small helper their ports do their own way |
| 0x004be2b0 | 59 | unlabelled |  |
| 0x004be440 | 9 | unlabelled | follows 0x004be2b0:  |
| 0x004be450 | 9 | folded | a thunk: one call to another function |
| 0x004be470 | 22 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004be4e0 | 53 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004be8f0 | 12 | unlabelled |  |
| 0x004be910 | 12 | unlabelled |  |
| 0x004be940 | 20 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b8400: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004be990 | 76 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b8460: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004bebb0 | 17 | unlabelled |  |
| 0x004bec10 | 32 | unlabelled |  |
| 0x004bec90 | 8 | folded | returns a constant or a global |
| 0x004bef80 | 44 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b8a20, whose class PM's changes keep: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004bf0b0 | 14 | folded | as MW3 0x004b8b70: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bf0e0 | 10 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b8ba0: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004bf100 | 28 | unlabelled |  |
| 0x004bf250 | 8 | folded | returns a constant or a global |
| 0x004bf350 | 25 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b8e10: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004bf3d0 | 12 | unlabelled |  |
| 0x004bf3f0 | 81 | unlabelled |  |
| 0x004bf780 | 8 | folded | returns a constant or a global |
| 0x004bf7f0 | 10 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b92d0: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004bf810 | 40 | unlabelled |  |
| 0x004bf990 | 8 | folded | returns a constant or a global |
| 0x004bf9a0 | 10 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b9470: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004bfb90 | 8 | folded | returns a constant or a global |
| 0x004bfba0 | 10 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b9680: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004bfbc0 | 30 | unlabelled |  |
| 0x004bfd80 | 8 | folded | returns a constant or a global |
| 0x004bfd90 | 10 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b9870: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004bfe20 | 8 | folded | returns a constant or a global |
| 0x004bfe30 | 10 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b9910: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004bfe50 | 61 | unlabelled |  |
| 0x004c0110 | 8 | folded | returns a constant or a global |
| 0x004c0120 | 10 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b9c10: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004c0140 | 8 | folded | returns a constant or a global |
| 0x004c0150 | 10 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b9c60: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004c01b0 | 8 | folded | returns a constant or a global |
| 0x004c01c0 | 10 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b9cd0: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004c0250 | 8 | folded | returns a constant or a global |
| 0x004c0260 | 10 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b9d70: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004c02c0 | 8 | folded | returns a constant or a global |
| 0x004c02d0 | 10 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004b9de0: the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004c0310 | 8 | folded | returns a constant or a global |
| 0x004c0320 | 10 | unlabelled |  |
| 0x004c0500 | 9 | folded | as MW3 0x00409860: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x004c0510 | 16 | unlabelled |  |
| 0x004c0540 | 58 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004c0660 | 80 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004c07d0 | 41 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004c0880 | 58 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004c09b0 | 318 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004c0ee0 | 48 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004c0fb0 | 31 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004c1040 | 195 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004c1350 | 51 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004c1410 | 47 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004c14c0 | 89 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004c16a0 | 19 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004c16e0 | 13 | folded | as MW3 0x004bb1e0: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004c1700 | 32 | folded | as MW3 0x004bb200: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004c1780 | 16 | folded | covered by the port where it names it (src/mech3/game_editor_vehicles.cpp:887) |
| 0x004c17d0 | 9 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x004bb2e0: the vehicle editor's list fills (each valve, each region by name into a combo box); covered in the port by game_editor_vehicles 0x004bb300 / 0x004bb620 (valve and region lists) |
| 0x004c17f0 | 16 | unlabelled |  |
| 0x004c1870 | 59 | folded | as MW3 0x004bb390: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004c1ac0 | 12 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x004bb5f0: the vehicle editor's list fills (each valve, each region by name into a combo box); covered in the port by game_editor_vehicles 0x004bb300 / 0x004bb620 (valve and region lists) |
| 0x004c1af0 | 16 | unlabelled |  |
| 0x004c1cb0 | 148 | unlabelled |  |
| 0x004c21d0 | 14 | folded | as MW3 0x004bbd70: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004c2230 | 19 | folded | as MW3 0x004bbdd0: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004c22c0 | 16 | folded | as MW3 0x004bbe60: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004c2330 | 14 | folded | read: one section (+0x50..+0x5c) of the vehicle editor's changed-settings test (0x004c1cb0, MW3 0x004bb7e0 with the sections compared inline; game_editor_vehicles.cpp) |
| 0x004c2370 | 27 | folded | as MW3 0x004bbed0: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004c24b0 | 15 | folded | as MW3 0x004bc010: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004c2530 | 12 | folded | as MW3 0x004bc0d0: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004c2570 | 14 | folded | as MW3 0x004bc110: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004c28e0 | 9 | folded | a thunk: one call to another function |
| 0x004c2920 | 24 | folded | as MW3 0x004bc420: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004c2980 | 16 | unlabelled |  |
| 0x004c2e10 | 31 | folded | as MW3 0x004bc960: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004c2ea0 | 8 | unlabelled |  |
| 0x004c2ec0 | 8 | unlabelled |  |
| 0x004c2ee0 | 137 | unlabelled |  |
| 0x004c31c0 | 26 | unlabelled |  |
| 0x004c3280 | 9 | unlabelled | follows 0x004c2ee0:  |
| 0x004c3290 | 25 | unlabelled |  |
| 0x004c32f0 | 15 | unlabelled |  |
| 0x004c3320 | 23 | unlabelled |  |
| 0x004c3380 | 25 | unlabelled |  |
| 0x004c33e0 | 23 | unlabelled |  |
| 0x004c3440 | 31 | unlabelled |  |
| 0x004c34f0 | 23 | unlabelled |  |
| 0x004c3550 | 75 | unlabelled |  |
| 0x004c3680 | 17 | unlabelled |  |
| 0x004c36c0 | 36 | folded | covered by the port where it names it (src/mech3/zainet_inout.cpp:643) |
| 0x004c3750 | 31 | unlabelled |  |
| 0x004c37c0 | 31 | unlabelled |  |
| 0x004c3830 | 22 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x004bc9f0: the AI tester dialog (0x004b7d80..0x004bcb70, docs/ai.md: a developer tool over a vehicle's AI and net): its helpers' logic |
| 0x004c3970 | 11 | folded | as MW3 0x004bcb30: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004c3990 | 9 | folded | covered by the port where it names it (src/mech3/zrdrainet.h:18) |
| 0x004c39b0 | 19 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x004bcb70: the AI tester dialog (0x004b7d80..0x004bcb70, docs/ai.md: a developer tool over a vehicle's AI and net): its helpers' logic |
| 0x004c3db0 | 16 | folded | as MW3 0x004bcf90: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c3dc0 | 16 | folded | as MW3 0x004bcfa0: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c3dd0 | 16 | folded | as MW3 0x004bcfb0: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c3de0 | 16 | folded | covered by the port where it names it (src/mech3/zainetedit.h:53) |
| 0x004c3df0 | 16 | unlabelled |  |
| 0x004c3e00 | 12 | unlabelled |  |
| 0x004c3e30 | 12 | unlabelled |  |
| 0x004c3e40 | 12 | unlabelled |  |
| 0x004c4000 | 47 | unlabelled |  |
| 0x004c4140 | 9 | unlabelled |  |
| 0x004c4160 | 10 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:303) |
| 0x004c4180 | 26 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:257) |
| 0x004c41f0 | 26 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:546) |
| 0x004c4260 | 50 | folded | ported, by docs/symbols.csv: ported: zainetedit (vtable 0x005ca480) |
| 0x004c4460 | 18 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:435) |
| 0x004c4570 | 24 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:430) |
| 0x004c4b90 | 11 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:368) |
| 0x004c5110 | 56 | folded | as MW3 0x004be230: covered where the port does it: src/mech3/zainetedit.cpp:254 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004c51e0 | 146 | folded | read: the net compile (MW3 0x004be300 from 0x004be670), ported with PM's changes (zainetedit.cpp:419: the via-node net 0x004c7a10, has_vias); its extra calls are those and PM's allocator |
| 0x004c54c0 | 49 | folded | as MW3 0x004be5a0: covered where the port does it: src/mech3/zainetedit.cpp:463 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004c5550 | 16 | folded | as MW3 0x004be630: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c55b0 | 205 | unlabelled |  |
| 0x004c5a20 | 40 | folded | as MW3 0x004beb00: covered where the port does it: src/mech3/zainetedit.cpp:450 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004c5ab0 | 42 | folded | as MW3 0x004beb90: covered where the port does it: src/mech3/zainetedit.cpp:451 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004c5b30 | 59 | folded | as MW3 0x004bec10: covered where the port does it: src/mech3/zainetedit.cpp:450 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004c5c80 | 12 | folded | as MW3 0x004bed60: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c5cb0 | 83 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004bed90: the version-105 nets: the editor's reload over them (0x004bed90, the vehicles using the old net kept and handed the new one) and its kept list; not ported on purpose (ai.md 9, 17.9): no shipped 105 file |
| 0x004c5e50 | 14 | unlabelled |  |
| 0x004c5e80 | 16 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:81) |
| 0x004c5ef0 | 8 | folded | the same as its MW3 twin after the address map (nothing differs); not run under a test (trace) |
| 0x004c5f00 | 81 | unlabelled |  |
| 0x004c60a0 | 117 | unlabelled |  |
| 0x004c63e0 | 8 | folded | covered by the port where it names it (src/mech3/zainetedit.h:51) |
| 0x004c63f0 | 16 | folded | as MW3 0x0049ce90: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x004c6430 | 9 | unlabelled | follows 0x004c3e00:  |
| 0x004c6440 | 21 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:577) |
| 0x004c64a0 | 12 | unlabelled |  |
| 0x004c64c0 | 20 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:555) |
| 0x004c6510 | 8 | folded | returns a constant or a global |
| 0x004c6520 | 9 | folded | a thunk: one call to another function |
| 0x004c6530 | 11 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:567) |
| 0x004c65d0 | 13 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:487) |
| 0x004c6630 | 42 | folded | as MW3 0x004bf750: covered where the port does it: src/mech3/game_editor_net.cpp:493 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004c6700 | 20 | replaced (SDL or the platform layer) | as MW3 0x004bf820: the AI net editor's file box (CFileDialogNet 0x004bf750: constructor, destructor, message map, class); replaced (FIX (A), ai.md) |
| 0x004c6750 | 8 | replaced (SDL or the platform layer) | as MW3 0x004bf870: the AI net editor's file box (CFileDialogNet 0x004bf750: constructor, destructor, message map, class); replaced (FIX (A), ai.md) |
| 0x004c6760 | 8 | folded | returns a constant or a global |
| 0x004c6770 | 12 | replaced (SDL or the platform layer) | as MW3 0x004bf890: the AI net editor's file box (CFileDialogNet 0x004bf750: constructor, destructor, message map, class); replaced (FIX (A), ai.md) |
| 0x004c67b0 | 12 | unlabelled |  |
| 0x004c67d0 | 8 | replaced (SDL or the platform layer) | as MW3 0x004bf8f0: the AI net editor's file box (CFileDialogNet 0x004bf750: constructor, destructor, message map, class); replaced (FIX (A), ai.md) |
| 0x004c67e0 | 8 | folded | returns a constant or a global |
| 0x004c67f0 | 107 | unlabelled |  |
| 0x004c6a90 | 12 | unlabelled |  |
| 0x004c6ab0 | 107 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x004c6d60 | 48 | folded | as MW3 0x004bfe40: covered where the port does it: src/mech3/mech_waynet.cpp:147 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004c6ed0 | 12 | unlabelled |  |
| 0x004c6ef0 | 10 | folded | covered by the port where it names it (src/mech3/briefing.cpp:180) |
| 0x004c6f10 | 9 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port; a PM change around it is its caller's label |
| 0x004c6f20 | 11 | folded | covered by the port where it names it (src/mech3/zai_brain.cpp:234) |
| 0x004c6f40 | 12 | unlabelled |  |
| 0x004c6f60 | 10 | folded | as MW3 0x004c0060: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c7240 | 10 | folded | covered by the port where it names it (src/mech3/zai_steps.h:63) |
| 0x004c7260 | 12 | unlabelled |  |
| 0x004c7280 | 10 | folded | as MW3 0x004c02d0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c7290 | 9 | folded | a thunk: one call to another function |
| 0x004c72a0 | 22 | unlabelled |  |
| 0x004c7300 | 89 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004c0310, whose class PM's changes keep: the built-in net 0x00604008 (one behaviour: the default one, 0x00604068): what an order goes on when its vehicle has no net and the record's net is not found. The port's builtin_net() is null in the game (WiredBrainHooks does not give one), so Order::reset/Brain::push would call through a null net. Not reached by the shipped missions (every AI has a net; the mission tests pass); a mod's vehicle without a net would. Port: small (a CompiledNet holding the default behaviour) |
| 0x004c7520 | 14 | unlabelled |  |
| 0x004c7560 | 89 | unlabelled |  |
| 0x004c77a0 | 10 | folded | as MW3 0x004c07a0: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c77b0 | 14 | unlabelled |  |
| 0x004c77e0 | 12 | unlabelled |  |
| 0x004c78c0 | 12 | folded | covered by the port where it names it (src/mech3/zai_attack.cpp:70) |
| 0x004c78f0 | 12 | folded | covered by the port where it names it (src/mech3/zai_attack.h:40) |
| 0x004c7910 | 10 | unlabelled |  |
| 0x004c7a10 | 11 | folded | covered by the port where it names it (src/mech3/zai_steps.h:279) |
| 0x004c7a40 | 12 | unlabelled |  |
| 0x004c7a60 | 10 | unlabelled |  |
| 0x004c7a70 | 22 | unlabelled |  |
| 0x004c7ad0 | 11 | folded | covered by the port where it names it (src/mech3/zai_brain.cpp:236) |
| 0x004c7af0 | 12 | unlabelled |  |
| 0x004c7b10 | 10 | unlabelled |  |
| 0x004c7b20 | 35 | folded | covered by the port where it names it (src/mech3/zainet_inout.cpp:752) |
| 0x004c7c00 | 27 | folded | covered by the port where it names it (src/mech3/zainet_inout.cpp:752) |
| 0x004c7c80 | 11 | debug or dev tool: PORT (preserve) | as MW3 0x0058f2d0, whose class PM's changes keep: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x004c7c90 | 13 | debug or dev tool: PORT (preserve) | called only from 0x004c7cf0, which are debug |
| 0x004c7cf0 | 20 | debug or dev tool: PORT (preserve) | called only from 0x004c7c80, which are debug |
| 0x004c7d60 | 30 | folded | covered by the port where it names it (src/mech3/zainet_inout.cpp:752) |
| 0x004c7dc0 | 15 | unlabelled |  |
| 0x004c7df0 | 12 | unlabelled |  |
| 0x004c7e00 | 12 | unlabelled |  |
| 0x004c7e10 | 11 | unlabelled |  |
| 0x004c7e30 | 12 | unlabelled |  |
| 0x004c7e50 | 22 | unlabelled |  |
| 0x004c7e90 | 34 | folded | inside its ported callers (0x004cbbb0): a small helper their ports do their own way |
| 0x004c7f30 | 29 | folded | PM's inout query record's constructor (0x54 bytes): MW3's 0x004c0ed0 plus PM's fields +0x3c..+0x50 zeroed; the port keeps only the read fields (InOutLast, zainet_inout.h:174, null and false by default); not run under a test (trace) |
| 0x004c8010 | 21 | unused: PORT (preserve) | as MW3 0x004c0ed0: the vehicle's +0xd8 record of each in/out query (its constructor and reset): written, never read (docs/symbols.csv: not kept; ai.md 11) |
| 0x004c80a0 | 11 | folded | ported, by docs/symbols.csv: ported: zainet_inout lerp (x and z, a's height) |
| 0x004c80f0 | 9 | folded | ported, by docs/symbols.csv: ported: zainet_inout dist_xz |
| 0x004c8130 | 9 | unlabelled | follows 0x004c8140:  |
| 0x004c8140 | 33 | unlabelled |  |
| 0x004c8230 | 21 | folded | inside its ported callers (0x004c9af0): a small helper their ports do their own way |
| 0x004c82b0 | 22 | folded | inside its ported callers (0x004cb8b0, 0x004cbbb0): a small helper their ports do their own way |
| 0x004c8340 | 17 | unlabelled |  |
| 0x004c8370 | 30 | folded | covered by the port where it names it (src/mech3/zainet_inout.cpp:490) |
| 0x004c8450 | 32 | unlabelled |  |
| 0x004c8540 | 10 | unlabelled |  |
| 0x004c8570 | 13 | unlabelled |  |
| 0x004c85b0 | 46 | unlabelled |  |
| 0x004c86e0 | 13 | folded | inside its ported callers (0x004c9600): a small helper their ports do their own way |
| 0x004c8720 | 56 | unlabelled |  |
| 0x004c8940 | 14 | folded | inside its ported callers (0x004c9600): a small helper their ports do their own way |
| 0x004c8990 | 59 | unlabelled |  |
| 0x004c8b90 | 14 | unlabelled |  |
| 0x004c8be0 | 15 | unlabelled |  |
| 0x004c9220 | 10 | folded | inside its ported callers (0x004c8c30): a small helper their ports do their own way |
| 0x004c9260 | 22 | unlabelled |  |
| 0x004c9570 | 10 | unlabelled |  |
| 0x004c9580 | 9 | unlabelled |  |
| 0x004c9590 | 9 | folded | a thunk: one call to another function |
| 0x004c95b0 | 13 | folded | as MW3 0x004c2820: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c9a40 | 12 | unlabelled |  |
| 0x004c9a70 | 17 | unlabelled |  |
| 0x004ca4e0 | 45 | unlabelled |  |
| 0x004caa60 | 15 | unlabelled |  |
| 0x004caac0 | 16 | unlabelled |  |
| 0x004cab20 | 62 | unlabelled |  |
| 0x004caca0 | 37 | folded | as MW3 0x004c2e90: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004cad50 | 21 | unlabelled |  |
| 0x004cade0 | 10 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_boundary's query 0x004cc7f0, the via lists 0x004cf350 and 0x004ccaf0 run it; every corner's words, the answer and the query's state compared |
| 0x004cae10 | 9 | unlabelled | follows 0x004cab20:  |
| 0x004cae40 | 12 | folded | as MW3 0x004c2670: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004cae70 | 19 | folded | covered by the port where it names it (src/mech3/zainet_inout.cpp:382) |
| 0x004cb0e0 | 113 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x004cb4a0 | 12 | unlabelled |  |
| 0x004cb4d0 | 11 | unlabelled |  |
| 0x004cb510 | 65 | unlabelled |  |
| 0x004cb700 | 11 | folded | inside its ported callers (0x004cc7f0): a small helper their ports do their own way |
| 0x004cb730 | 14 | unlabelled |  |
| 0x004cb780 | 9 | folded | a thunk: one call to another function |
| 0x004cb7b0 | 38 | folded | ported, by docs/symbols.csv: ported: zainet_inout inout_set_state (0x004c2a30) |
| 0x004cb840 | 15 | folded | read: the inout state's polygon walk (0x004ce020, MW3's 0x004c3bd0 with the walk inline): each polygon whose name matches is marked tested when its kind is the message's (0x004cb780 / 0x004cb7b0), as the port's inout_set_state (zainet_inout.cpp:792) |
| 0x004cc970 | 17 | folded | inside its ported callers (0x004cb8b0, 0x004cbbb0): a small helper their ports do their own way |
| 0x004ccac0 | 12 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_walk's drive 0x004dcd20 and the walk's steps run it (test_pm_repair_state's behaviour 0x004e0790 too); the steering's fields and the calls to the controls compared |
| 0x004cce60 | 10 | folded | as MW3 0x004c3250: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004cce70 | 9 | folded | follows 0x004ce0e0: as MW3 0x004c3c90: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004cce80 | 9 | folded | a thunk: one call to another function |
| 0x004ccea0 | 12 | folded | as MW3 0x004c3290: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004cd2f0 | 161 | unlabelled |  |
| 0x004cd720 | 11 | folded | as MW3 0x004c3600: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004cd740 | 106 | unlabelled |  |
| 0x004cdda0 | 8 | folded | as MW3 0x004c3910: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004cdff0 | 11 | folded | as MW3 0x004c3b50: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004ce0e0 | 11 | folded | as MW3 0x004c3c90: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004ce110 | 12 | unlabelled |  |
| 0x004ce140 | 39 | unlabelled |  |
| 0x004ce270 | 12 | folded | ported, by docs/symbols.csv: ported: zainet_inout inout_mission_load (listener 0x0060b338 slot +0x08) |
| 0x004ce380 | 19 | folded | inside its ported callers (0x004ce320): a small helper their ports do their own way |
| 0x004ce410 | 8 | folded | empty: does nothing |
| 0x004ce420 | 11 | unlabelled |  |
| 0x004ce4d0 | 28 | unlabelled |  |
| 0x004ce570 | 13 | folded | as MW3 0x004c3f70: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004ce5b0 | 40 | folded | as MW3 0x004c3fb0: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004ce6f0 | 21 | folded | as MW3 0x004c40e0: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004ce780 | 9 | folded | as MW3 0x004c4170: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004ce7a0 | 9 | unlabelled | follows 0x004ce7c0:  |
| 0x004ce7c0 | 30 | unlabelled |  |
| 0x004ce8a0 | 23 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:2550) |
| 0x004ced50 | 16 | folded | inside its ported callers (0x004cedd0): a small helper their ports do their own way |
| 0x004cefc0 | 52 | unlabelled |  |
| 0x004cf270 | 10 | folded | as MW3 0x004c4980: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004cf280 | 9 | folded | follows 0x004ce780: as MW3 0x004c4170: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004cf290 | 9 | folded | a thunk: one call to another function |
| 0x004cf2b0 | 9 | unlabelled | follows 0x004ce7a0: follows 0x004ce7c0:  |
| 0x004cf2c0 | 8 | folded | inside its ported callers (0x004be620): a small helper their ports do their own way |
| 0x004cf2d0 | 9 | unlabelled | follows 0x004cf2f0: follows 0x004ce7c0:  |
| 0x004cf2f0 | 9 | unlabelled | follows 0x004ce7c0:  |
| 0x004cf310 | 9 | folded | a thunk: one call to another function |
| 0x004cf330 | 9 | folded | as MW3 0x004c4a40: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004cf730 | 88 | unlabelled |  |
| 0x004cfb40 | 19 | unlabelled |  |
| 0x004cfb80 | 11 | folded | inside its ported callers (0x004c9af0): a small helper their ports do their own way |
| 0x004cfbb0 | 19 | unlabelled |  |
| 0x004cfc00 | 9 | folded | frees the object (a destructor's release) |
| 0x004cfc10 | 154 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_boundary's polygon 0x004c9af0 runs it (the corners' vector); every corner's words compared |
| 0x004d0030 | 11 | folded | covered by the port where it names it (src/mech3/zainet_inout.cpp:232) |
| 0x004d0060 | 152 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_boundary's via lists 0x004cf350 run it; every corner's words and the via lists compared |
| 0x004d0370 | 18 | unlabelled |  |
| 0x004d03c0 | 18 | unlabelled |  |
| 0x004d0410 | 18 | unlabelled |  |
| 0x004d0460 | 18 | unlabelled |  |
| 0x004d04b0 | 64 | folded | as MW3 0x004c5560: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004d05e0 | 123 | unlabelled |  |
| 0x004d0820 | 21 | folded | as MW3 0x0054bb30: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x004d0870 | 107 | unlabelled |  |
| 0x004d0b00 | 12 | unlabelled |  |
| 0x004d0b20 | 11 | folded | covered by the port where it names it (src/mech3/zainet_region.cpp:38) |
| 0x004d0bc0 | 9 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:225) |
| 0x004d0cc0 | 10 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port; a PM change around it is its caller's label |
| 0x004d0cf0 | 12 | unlabelled |  |
| 0x004d0d20 | 9 | folded | a thunk: one call to another function |
| 0x004d0e20 | 12 | unlabelled |  |
| 0x004d0e40 | 9 | unlabelled |  |
| 0x004d0ee0 | 8 | folded | returns a constant or a global |
| 0x004d0ef0 | 8 | folded | returns a constant or a global |
| 0x004d0f50 | 8 | folded | covered by the port where it names it (src/mech3/app_states.h:103) |
| 0x004d0f60 | 11 | folded | as MW3 0x004c5b70: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004d0f70 | 12 | unlabelled |  |
| 0x004d10e0 | 9 | folded | a thunk: one call to another function |
| 0x004d1100 | 9 | folded | a thunk: one call to another function |
| 0x004d1110 | 9 | folded | a thunk: one call to another function |
| 0x004d1140 | 18 | folded | covered by the port where it names it (src/mech3/zainet_region.cpp:87) |
| 0x004d1170 | 34 | folded | as MW3 0x004c5db0: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004d12f0 | 9 | folded | a thunk: one call to another function |
| 0x004d1300 | 47 | unlabelled |  |
| 0x004d13b0 | 12 | unlabelled |  |
| 0x004d13d0 | 154 | unlabelled |  |
| 0x004d1670 | 57 | unlabelled |  |
| 0x004d1790 | 167 | unlabelled |  |
| 0x004d1fc0 | 11 | folded | covered by the port where it names it (src/mech3/zrdrainet.cpp:367) |
| 0x004d1fe0 | 46 | unlabelled |  |
| 0x004d20c0 | 10 | folded | covered by the port where it names it (src/mech3/zainet_node.h:29) |
| 0x004d20e0 | 12 | unlabelled |  |
| 0x004d2100 | 10 | folded | covered by the port where it names it (src/mech3/zainet.cpp:36) |
| 0x004d2120 | 10 | folded | covered by the port where it names it (src/mech3/zainet_node.h:9) |
| 0x004d2130 | 12 | unlabelled |  |
| 0x004d2230 | 21 | folded | covered by the port where it names it (src/mech3/zainet.h:59) |
| 0x004d2290 | 26 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:91) |
| 0x004d23e0 | 9 | folded | covered by the port where it names it (src/mech3/zainet.cpp:61) |
| 0x004d24c0 | 9 | folded | covered by the port where it names it (src/mech3/zainet.cpp:84) |
| 0x004d24e0 | 27 | folded | covered by the port where it names it (src/mech3/zainet.cpp:85) |
| 0x004d2550 | 23 | folded | as MW3 0x004c7110: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004d25d0 | 9 | unlabelled |  |
| 0x004d2620 | 9 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:27) |
| 0x004d2690 | 14 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:75) |
| 0x004d26b0 | 9 | folded | follows 0x004c5ef0: the same as its MW3 twin after the address map (nothing differs); not run under a test (trace) |
| 0x004d2710 | 8 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:78) |
| 0x004d2720 | 9 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:81) |
| 0x004d2740 | 9 | folded | a thunk: one call to another function |
| 0x004d2750 | 20 | folded | covered by the port where it names it (src/mech3/zainet.cpp:130) |
| 0x004d2780 | 55 | unlabelled |  |
| 0x004d2960 | 19 | folded | covered by the port where it names it (src/mech3/zainet_node.cpp:110) |
| 0x004d29a0 | 18 | folded | covered by the port where it names it (src/mech3/zainet_node.cpp:117) |
| 0x004d29e0 | 120 | folded | as MW3 0x004c75c0: covered where the port does it: src/mech3/zainet_node.cpp:40 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d2ba0 | 13 | folded | covered by the port where it names it (src/mech3/zainet_node.h:13) |
| 0x004d2bd0 | 11 | folded | as MW3 0x004c7790: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004d2bf0 | 9 | unlabelled | follows 0x004d2130:  |
| 0x004d2c00 | 10 | folded | as MW3 0x004c77c0: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004d2c20 | 12 | unlabelled |  |
| 0x004d2c50 | 8 | folded | returns a constant or a global |
| 0x004d2c60 | 11 | folded | covered by the port where it names it (src/upgrades/editor/node_params.h:8) |
| 0x004d2c80 | 33 | folded | covered by the port where it names it (src/upgrades/editor/node_params.h:7) |
| 0x004d2d00 | 13 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004c78c0: an AI dev dialog's window close (PostNcDestroy, [0x0060b420] cleared) |
| 0x004d2da0 | 11 | folded | covered by the port where it names it (src/mech3/zai_brain_pm.cpp:227) |
| 0x004d2db0 | 27 | folded | ported, by docs/symbols.csv: ported; LinkWay::measure (first half) |
| 0x004d2e40 | 11 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port; a PM change around it is its caller's label |
| 0x004d2f40 | 15 | folded | ported, by docs/symbols.csv: ported inline; LinkWay |
| 0x004d2f80 | 9 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port; a PM change around it is its caller's label |
| 0x004d2f90 | 9 | folded | a thunk: one call to another function |
| 0x004d2fe0 | 19 | folded | covered by the port where it names it (src/gamez/znetwork/znet_object.cpp:8) |
| 0x004d3010 | 23 | folded | as MW3 0x004c7bb0: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004d3050 | 9 | folded | covered by the port where it names it (src/mech3/zai_brain.h:709) |
| 0x004d3060 | 12 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.h:371) |
| 0x004d3170 | 26 | folded | ported, by docs/symbols.csv: ported: zai_path (vtable 0x005cb178) |
| 0x004d31d0 | 12 | folded | covered by the port where it names it (src/mech3/zainet_inout.cpp:752) |
| 0x004d31f0 | 54 | folded | as MW3 0x004c7d80: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004d3580 | 54 | unlabelled |  |
| 0x004d3820 | 45 | unlabelled |  |
| 0x004d39a0 | 11 | folded | as MW3 0x004c8600: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d39c0 | 20 | unlabelled |  |
| 0x004d3a20 | 12 | unlabelled |  |
| 0x004d3a40 | 10 | folded | covered by the port where it names it (src/mech3/zai_brain.h:389) |
| 0x004d3a60 | 9 | folded | the same as its MW3 twin 0x004c8680: the vtable it sets is MW3 0x005cb238's twin (vtables.json); not run under a test (trace) |
| 0x004d3ac0 | 20 | unlabelled |  |
| 0x004d3b10 | 10 | folded | as MW3 0x004c8700: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d3b30 | 12 | folded | covered by the port where it names it (src/mech3/zainet_inout.cpp:86) |
| 0x004d3b50 | 10 | folded | as MW3 0x004c8740: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d3b60 | 10 | folded | as MW3 0x004c8750: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d3b80 | 20 | unlabelled |  |
| 0x004d3bd0 | 10 | folded | as MW3 0x004c8790: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d3bf0 | 12 | unlabelled |  |
| 0x004d3c10 | 10 | folded | as MW3 0x004c87d0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d3c20 | 8 | folded | returns a constant or a global |
| 0x004d3c30 | 33 | unlabelled |  |
| 0x004d3cd0 | 20 | unlabelled |  |
| 0x004d3d20 | 10 | folded | as MW3 0x004c88c0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d3d40 | 8 | folded | returns a constant or a global |
| 0x004d3d50 | 12 | unlabelled |  |
| 0x004d3d70 | 10 | folded | as MW3 0x004c8910: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d3d80 | 20 | unlabelled |  |
| 0x004d3dd0 | 10 | folded | as MW3 0x004c8940: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d3df0 | 8 | folded | returns a constant or a global |
| 0x004d3e00 | 12 | unlabelled |  |
| 0x004d3e20 | 10 | folded | as MW3 0x004c8990: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d3e30 | 9 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x004d3e40 | 94 | unlabelled |  |
| 0x004d3fc0 | 18 | folded | covered by the port where it names it (src/mech3/zai_steps.h:212) |
| 0x004d4000 | 12 | UNSURE | as MW3 0x004c9140: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots |
| 0x004d4010 | 22 | unlabelled |  |
| 0x004d4080 | 55 | UNSURE | as MW3 0x004c9180: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots |
| 0x004d4170 | 20 | UNSURE | as MW3 0x004c9260: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots |
| 0x004d41c0 | 36 | UNSURE | as MW3 0x004c92b0: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots |
| 0x004d4220 | 21 | UNSURE | as MW3 0x004c9310: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots |
| 0x004d4270 | 36 | UNSURE | as MW3 0x004c9360: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots |
| 0x004d4300 | 24 | UNSURE | as MW3 0x004c9410: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots |
| 0x004d4360 | 11 | folded | covered by the port where it names it (src/mech3/zai_steps.h:208) |
| 0x004d4380 | 11 | UNSURE | as MW3 0x004c9480: the node steps' vt+0x30..+0x50 (vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4; like Step's 0x004c9d20 family): set a link slot (+0x20) from a picked node's behaviour (vt+0x60, +0x74), find or count links, the node's position set: probably the AI net editor's linking; to settle by finding who calls these slots |
| 0x004d4650 | 21 | UNSURE | as MW3 0x004c9d20: Step's vt+0x30..+0x44 (sets, compares or offers the step's next from a picked node's behaviour, vt+0x60/+0x74): probably the AI net editor's linking; to settle by finding who calls these slots |
| 0x004d4690 | 20 | UNSURE | as MW3 0x004c9d70: Step's vt+0x30..+0x44 (sets, compares or offers the step's next from a picked node's behaviour, vt+0x60/+0x74): probably the AI net editor's linking; to settle by finding who calls these slots |
| 0x004d46d0 | 14 | UNSURE | as MW3 0x004c9db0: Step's vt+0x30..+0x44 (sets, compares or offers the step's next from a picked node's behaviour, vt+0x60/+0x74): probably the AI net editor's linking; to settle by finding who calls these slots |
| 0x004d4700 | 14 | UNSURE | as MW3 0x004c9de0: Step's vt+0x30..+0x44 (sets, compares or offers the step's next from a picked node's behaviour, vt+0x60/+0x74): probably the AI net editor's linking; to settle by finding who calls these slots |
| 0x004d4720 | 8 | folded | as MW3 0x004cb470: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4730 | 11 | folded | as MW3 0x004c9e00: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4750 | 11 | folded | as MW3 0x004c9e20: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4770 | 8 | folded | as MW3 0x004c9e40: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4780 | 11 | folded | as MW3 0x004c9e50: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d47e0 | 11 | folded | as MW3 0x004c9eb0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4800 | 22 | unlabelled |  |
| 0x004d4860 | 11 | folded | as MW3 0x004c9ef0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4880 | 12 | unlabelled |  |
| 0x004d48a0 | 10 | folded | as MW3 0x004c9f30: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d48f0 | 11 | folded | as MW3 0x004c9f80: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4910 | 22 | unlabelled |  |
| 0x004d49f0 | 12 | unlabelled |  |
| 0x004d4a10 | 10 | folded | as MW3 0x004ca060: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4a50 | 11 | folded | as MW3 0x004ca0a0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4a70 | 22 | unlabelled |  |
| 0x004d4ad0 | 24 | folded | as MW3 0x004ca0e0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4b50 | 12 | unlabelled |  |
| 0x004d4b70 | 10 | folded | as MW3 0x004ca180: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4b80 | 11 | folded | as MW3 0x004ca190: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4bb0 | 12 | folded | as MW3 0x004ca1c0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4be0 | 22 | unlabelled |  |
| 0x004d4cc0 | 11 | folded | as MW3 0x004ca290: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4ce0 | 12 | unlabelled |  |
| 0x004d4d00 | 10 | folded | as MW3 0x004ca2d0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4d20 | 9 | folded | a thunk: one call to another function |
| 0x004d4d30 | 9 | folded | a thunk: one call to another function |
| 0x004d4d60 | 10 | folded | as MW3 0x004ca370: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4da0 | 20 | unlabelled |  |
| 0x004d4df0 | 10 | folded | as MW3 0x004ca3b0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d4e20 | 12 | unlabelled |  |
| 0x004d4e40 | 10 | folded | as MW3 0x004ca400: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004d4e50 | 11 | folded | ported, by docs/symbols.csv: ported: AfterManoeuvreState::resume |
| 0x004d4e60 | 10 | unlabelled |  |
| 0x004d4e80 | 53 | folded | ported, by docs/symbols.csv: ported: zai_attack ManoeuvreState (its first part) |
| 0x004d4f80 | 12 | unlabelled |  |
| 0x004d5020 | 8 | folded | returns a constant or a global |
| 0x004d54e0 | 20 | unlabelled |  |
| 0x004d55b0 | 12 | unlabelled |  |
| 0x004d55e0 | 24 | unlabelled |  |
| 0x004d5640 | 65 | folded | as MW3 0x004cb950: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d5770 | 13 | folded | as MW3 0x004cbd90: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d57a0 | 73 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_regions' lookup by name 0x004d0fc0 runs it; the regions built and the inside answers compared |
| 0x004d5960 | 22 | unlabelled |  |
| 0x004d59c0 | 11 | folded | as MW3 0x004cbc50: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d59e0 | 12 | unlabelled |  |
| 0x004d5a00 | 10 | folded | as MW3 0x004cbe90: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d5a60 | 21 | folded | as MW3 0x004cbcf0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d5ab0 | 21 | folded | as MW3 0x004cbd40: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d5b00 | 12 | unlabelled |  |
| 0x004d5b20 | 14 | folded | covered by the port where it names it (src/mech3/zai_attack.h:193) |
| 0x004d5b50 | 9 | folded | a thunk: one call to another function |
| 0x004d5b60 | 9 | folded | a thunk: one call to another function |
| 0x004d5b90 | 10 | folded | covered by the port where it names it (src/mech3/zai_attack.h:87) |
| 0x004d5bb0 | 20 | unlabelled |  |
| 0x004d5c00 | 10 | folded | covered by the port where it names it (src/mech3/zai_attack.h:355) |
| 0x004d5c20 | 12 | unlabelled |  |
| 0x004d5c40 | 10 | unlabelled |  |
| 0x004d5c50 | 8 | folded | returns a constant or a global |
| 0x004d5da0 | 16 | folded | as MW3 0x004ccc30: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d5df0 | 12 | unlabelled |  |
| 0x004d5e10 | 10 | folded | as MW3 0x004cccd0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d5e20 | 22 | unlabelled |  |
| 0x004d5ef0 | 12 | unlabelled |  |
| 0x004d5f10 | 10 | folded | as MW3 0x004ccd90: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d5fa0 | 11 | folded | as MW3 0x004cce30: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d5fc0 | 22 | unlabelled |  |
| 0x004d6020 | 11 | folded | as MW3 0x004cce70: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d6040 | 12 | unlabelled |  |
| 0x004d6060 | 10 | folded | as MW3 0x004cceb0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d6070 | 14 | unlabelled |  |
| 0x004d6420 | 44 | folded | as MW3 0x004cd2a0: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004d64e0 | 26 | folded | as MW3 0x004cd360: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004d6560 | 15 | folded | as MW3 0x004cd3e0: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004d6610 | 21 | folded | ported, by docs/symbols.csv: ported: zai_steps (vtable 0x005cc0e0) |
| 0x004d6670 | 12 | unlabelled |  |
| 0x004d6690 | 19 | folded | as MW3 0x004cd520: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004d66e0 | 22 | unlabelled |  |
| 0x004d6800 | 11 | folded | as MW3 0x004cd640: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d6820 | 12 | unlabelled |  |
| 0x004d6840 | 10 | folded | as MW3 0x004cd680: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d6850 | 10 | unlabelled |  |
| 0x004d6880 | 8 | folded | as MW3 0x004cd6b0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d6890 | 12 | folded | as MW3 0x004cd6c0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d68b0 | 10 | folded | as MW3 0x004cd6e0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d68d0 | 22 | unlabelled |  |
| 0x004d6930 | 11 | folded | as MW3 0x004cd720: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d6950 | 12 | unlabelled |  |
| 0x004d6970 | 10 | folded | as MW3 0x004cd760: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d6980 | 10 | folded | as MW3 0x004cd770: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d69a0 | 22 | unlabelled |  |
| 0x004d6a00 | 12 | unused: PORT (preserve) | as MW3 0x004cd7b0: the brain's own step at +0x240 (0x004cd770, vtable 0x005cc290): built by the brain's constructor, but nothing orders it (only the constructor takes +0x240), so its make_state, state and run (path following toward a point, 98 lines) never run |
| 0x004d6a30 | 12 | unlabelled |  |
| 0x004d6a50 | 10 | unused: PORT (preserve) | as MW3 0x004cd800: the brain's own step at +0x240 (0x004cd770, vtable 0x005cc290): built by the brain's constructor, but nothing orders it (only the constructor takes +0x240), so its make_state, state and run (path following toward a point, 98 lines) never run |
| 0x004d6a60 | 90 | unused: PORT (preserve) | as MW3 0x004cd810, whose class PM's changes keep: the brain's own step at +0x240 (0x004cd770, vtable 0x005cc290): built by the brain's constructor, but nothing orders it (only the constructor takes +0x240), so its make_state, state and run (path following toward a point, 98 lines) never run |
| 0x004d6ca0 | 46 | folded | as MW3 0x004cda50: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d6d50 | 12 | unlabelled |  |
| 0x004d6d70 | 19 | folded | as MW3 0x004cdb20: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d6dc0 | 22 | unlabelled |  |
| 0x004d6e60 | 9 | folded | a thunk: one call to another function |
| 0x004d6e70 | 11 | folded | as MW3 0x004cdbd0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d6e90 | 12 | unlabelled |  |
| 0x004d6eb0 | 10 | folded | as MW3 0x004cdc10: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d6f00 | 12 | folded | as MW3 0x004cdc20: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d6f30 | 12 | unlabelled |  |
| 0x004d6f50 | 11 | folded | as MW3 0x004cdc70: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d6f60 | 22 | unlabelled |  |
| 0x004d6fc0 | 11 | folded | as MW3 0x004cdca0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d6fe0 | 12 | unlabelled |  |
| 0x004d7000 | 10 | folded | as MW3 0x004cdce0: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d74f0 | 9 | folded | covered by the port where it names it (src/mech3/zai_attack.cpp:1197) |
| 0x004d7560 | 76 | folded | as MW3 0x004ce2a0: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004d7750 | 36 | folded | covered by the port where it names it (src/mech3/zai_node.cpp:147) |
| 0x004d7810 | 11 | folded | as MW3 0x00409e90: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x004d7960 | 63 | unlabelled |  |
| 0x004d7a90 | 12 | unlabelled |  |
| 0x004d7b80 | 12 | unlabelled |  |
| 0x004d7ba0 | 10 | unlabelled |  |
| 0x004d7bb0 | 23 | unlabelled |  |
| 0x004d8520 | 22 | unlabelled |  |
| 0x004d8660 | 12 | unlabelled |  |
| 0x004d8680 | 10 | folded | as MW3 0x004cad20: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004d8690 | 62 | unlabelled |  |
| 0x004d87c0 | 12 | unlabelled |  |
| 0x004d87f0 | 42 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x004d88a0 | 17 | folded | covered by the port where it names it (src/mech3/zai_walk_pm.cpp:441) |
| 0x004d8950 | 17 | folded | inside its ported callers (0x004d89d0): a small helper their ports do their own way |
| 0x004d8c40 | 22 | folded | covered by the port where it names it (src/mech3/zai_wire_states_pm.h:114) |
| 0x004d8d00 | 12 | unlabelled |  |
| 0x004d8d20 | 10 | folded | covered by the port where it names it (src/mech3/mech_control.h:182) |
| 0x004d8d60 | 28 | folded | covered by the port where it names it (src/mech3/zai_attack.cpp:713) |
| 0x004d91e0 | 22 | folded | covered by the port where it names it (src/mech3/zai_walk_pm.h:314) |
| 0x004d9240 | 12 | unlabelled |  |
| 0x004d9260 | 13 | folded | as MW3 0x004c8a30: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d9290 | 10 | folded | as MW3 0x004c8a60: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004d9b60 | 12 | unlabelled |  |
| 0x004dab80 | 12 | unlabelled |  |
| 0x004daba0 | 21 | unlabelled |  |
| 0x004dad70 | 22 | folded | as MW3 0x004cb250: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004dadc0 | 11 | folded | as MW3 0x004cb2a0: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004dae20 | 22 | unlabelled |  |
| 0x004dae80 | 15 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_jump's constructor 0x004daf40 and run 0x004db0e0 run it; the state's fields compared after every step |
| 0x004daec0 | 14 | unlabelled |  |
| 0x004daf00 | 19 | unlabelled |  |
| 0x004db0a0 | 8 | unlabelled |  |
| 0x004db0b0 | 12 | unlabelled |  |
| 0x004db0d0 | 10 | unlabelled |  |
| 0x004dbe00 | 27 | folded | covered by the port where it names it (src/mech3/zai_walk_pm.cpp:618) |
| 0x004dbe90 | 12 | unlabelled |  |
| 0x004dbeb0 | 21 | unlabelled |  |
| 0x004dbf80 | 14 | folded | as MW3 0x004ce5b0: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004dbfa0 | 14 | folded | as MW3 0x004ce5d0: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004dbfc0 | 21 | unlabelled |  |
| 0x004dc020 | 20 | unlabelled |  |
| 0x004dc070 | 20 | unlabelled |  |
| 0x004dc0c0 | 14 | folded | as MW3 0x004ce5f0: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004dc0e0 | 14 | folded | as MW3 0x004ce610: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004dc1f0 | 11 | folded | covered by the port where it names it (src/mech3/zai_brain.h:493) |
| 0x004dc270 | 11 | folded | inside its ported callers (0x004d9a90, 0x004dcfe0, 0x004ddab0, 0x004de0e0 ...): a small helper their ports do their own way |
| 0x004dc3b0 | 32 | folded | inside its ported callers (0x004dc9d0): a small helper their ports do their own way |
| 0x004dc760 | 14 | folded | as MW3 0x004ce9f0: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004dcb90 | 17 | folded | covered by the port where it names it (src/mech3/zai_attack.cpp:411) |
| 0x004dccc0 | 12 | unlabelled |  |
| 0x004dcce0 | 10 | folded | as MW3 0x004cee40: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004dd1e0 | 89 | unlabelled |  |
| 0x004dd410 | 67 | unlabelled |  |
| 0x004dd530 | 43 | folded | as MW3 0x004cf560: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004dd5e0 | 96 | unlabelled |  |
| 0x004dd930 | 12 | unlabelled |  |
| 0x004dd950 | 17 | unlabelled |  |
| 0x004dd9a0 | 11 | folded | ported, by docs/symbols.csv: ported: zai_steps (vtable 0x005cc7a8) |
| 0x004dd9d0 | 12 | unlabelled |  |
| 0x004dda00 | 22 | unlabelled |  |
| 0x004dda60 | 10 | folded | ported, by docs/symbols.csv: ported; BackupState (the node state's ctor, vtable 0x005cc860) |
| 0x004dda80 | 12 | unlabelled |  |
| 0x004ddaa0 | 10 | folded | as MW3 0x004cfac0: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004dde40 | 13 | unlabelled |  |
| 0x004dde90 | 9 | folded | follows 0x004ddec0: as MW3 0x004cfed0: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004ddea0 | 9 | folded | a thunk: one call to another function |
| 0x004ddec0 | 10 | folded | as MW3 0x004cfed0: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004ddef0 | 10 | folded | as MW3 0x004cff00: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004ddf00 | 31 | folded | ported, by docs/symbols.csv: ported: zai_steps (vtable 0x005cc898) |
| 0x004ddf90 | 12 | unlabelled |  |
| 0x004ddfb0 | 10 | folded | as MW3 0x004cffc0: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004de010 | 22 | unlabelled |  |
| 0x004de070 | 11 | folded | ported, by docs/symbols.csv: ported; DeployState (+0x28 the node) |
| 0x004de0a0 | 8 | unlabelled |  |
| 0x004de0b0 | 12 | unlabelled |  |
| 0x004de0d0 | 10 | folded | as MW3 0x004d0090: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004de920 | 24 | folded | inside its ported callers (0x004dd5e0): a small helper their ports do their own way |
| 0x004de980 | 17 | unlabelled |  |
| 0x004de9c0 | 12 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port; a PM change around it is its caller's label |
| 0x004de9d0 | 15 | folded | read: the same, but the sub-object it points to is at +0xffc in PM's mech (MW3's +0xf70), its layout |
| 0x004dea90 | 8 | folded | as MW3 0x004d1e60: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004deb80 | 22 | unlabelled |  |
| 0x004ded40 | 12 | folded | covered by the port where it names it (src/mech3/zainet_region.h:18) |
| 0x004ded60 | 21 | unlabelled |  |
| 0x004df320 | 22 | unlabelled |  |
| 0x004df380 | 46 | unlabelled |  |
| 0x004df460 | 8 | folded | inside its ported callers (0x004c9600): a small helper their ports do their own way |
| 0x004df470 | 12 | unlabelled |  |
| 0x004df490 | 10 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:81) |
| 0x004df4a0 | 49 | unlabelled |  |
| 0x004df580 | 12 | unlabelled |  |
| 0x004df5b0 | 25 | folded | covered by the port where it names it (src/mech3/zai_drive_pm.cpp:772) |
| 0x004df620 | 48 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x004df6f0 | 34 | folded | covered by the port where it names it (src/mech3/zai_drive_pm.cpp:772) |
| 0x004df7e0 | 17 | folded | inside its ported callers (0x004df860): a small helper their ports do their own way |
| 0x004dfca0 | 22 | unlabelled |  |
| 0x004dfdc0 | 12 | unlabelled |  |
| 0x004dfde0 | 24 | unlabelled |  |
| 0x004dfe40 | 14 | folded | covered by the port where it names it (src/mech3/zai_walk_pm.cpp:610) |
| 0x004dfe90 | 12 | unlabelled |  |
| 0x004e02a0 | 10 | folded | as MW3 0x004cbe10: AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004e0550 | 12 | unlabelled |  |
| 0x004e0ff0 | 22 | folded | covered by the port where it names it (src/mech3/zai_wire_states_pm.h:165) |
| 0x004e1150 | 12 | unlabelled |  |
| 0x004e1170 | 10 | folded | as MW3 0x004d13a0: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e15e0 | 22 | unlabelled |  |
| 0x004e1640 | 46 | unlabelled |  |
| 0x004e1720 | 12 | unlabelled |  |
| 0x004e1740 | 10 | unlabelled |  |
| 0x004e1750 | 49 | unlabelled |  |
| 0x004e1830 | 12 | unlabelled |  |
| 0x004e19d0 | 17 | folded | covered by the port where it names it (src/mech3/zai_drive_pm.cpp:730) |
| 0x004e1cf0 | 12 | unlabelled |  |
| 0x004e1d10 | 21 | unlabelled |  |
| 0x004e1dc0 | 22 | unlabelled |  |
| 0x004e1e20 | 11 | unlabelled |  |
| 0x004e1e50 | 12 | unlabelled |  |
| 0x004e1e70 | 10 | unlabelled |  |
| 0x004e1e80 | 9 | folded | a thunk: one call to another function |
| 0x004e1e90 | 8 | unlabelled |  |
| 0x004e1ea0 | 11 | folded | inside its ported callers (0x004c4c20): a small helper their ports do their own way |
| 0x004e1ed0 | 12 | unlabelled |  |
| 0x004e1ef0 | 10 | unlabelled |  |
| 0x004e1f00 | 22 | unlabelled |  |
| 0x004e1f60 | 11 | folded | covered by the port where it names it (src/mech3/zai_attack.h:171) |
| 0x004e1f90 | 12 | unlabelled |  |
| 0x004e1fb0 | 10 | unlabelled |  |
| 0x004e1fc0 | 11 | folded | covered by the port where it names it (src/mech3/zai_attack_pm.cpp:17) |
| 0x004e2180 | 11 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:385) |
| 0x004e21a0 | 16 | folded | covered by the port where it names it (src/mech3/zai_target.h:131) |
| 0x004e21c0 | 79 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x004e26a0 | 11 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:645) |
| 0x004e26c0 | 20 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:370) |
| 0x004e26f0 | 21 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:487) |
| 0x004e2730 | 54 | unlabelled |  |
| 0x004e2820 | 73 | unlabelled |  |
| 0x004e29d0 | 54 | folded | as MW3 0x004d26f0: covered where the port does it: src/mech3/zai_target.cpp:481 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004e2b30 | 28 | folded | as MW3 0x004d2840: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e2bd0 | 30 | folded | as MW3 0x004d28e0: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e2d80 | 12 | unlabelled |  |
| 0x004e2e20 | 8 | folded | returns a constant or a global |
| 0x004e2f50 | 34 | unlabelled |  |
| 0x004e3190 | 40 | unlabelled |  |
| 0x004e3410 | 20 | folded | as MW3 0x004d2fe0: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e34b0 | 11 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:480) |
| 0x004e34e0 | 12 | unlabelled |  |
| 0x004e3500 | 23 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:510) |
| 0x004e3560 | 40 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:719) |
| 0x004e3700 | 20 | folded | as MW3 0x004d32c0: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e3760 | 8 | folded | covered by the port where it names it (src/mech3/entity_classes.cpp:44) |
| 0x004e3770 | 11 | folded | covered by the port where it names it (src/mech3/entity_classes.cpp:44) |
| 0x004e37a0 | 8 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:582) |
| 0x004e37b0 | 8 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:582) |
| 0x004e37c0 | 8 | folded | covered by the port where it names it (src/mech3/app_sensors.cpp:161) |
| 0x004e3aa0 | 9 | folded | a thunk: one call to another function |
| 0x004e3ac0 | 20 | folded | as MW3 0x004d35f0: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e3b10 | 16 | folded | as MW3 0x004d3640: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e3b50 | 13 | folded | read: a container element's copy (a vector's construct: 3 or 2 words, nothing for a null slot); the port's containers |
| 0x004e3b70 | 119 | folded | as MW3 0x004d3680: covered where the port does it: src/mech3/zai_target.cpp:322 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004e3d80 | 25 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:383) |
| 0x004e3de0 | 55 | folded | as MW3 0x004d38f0: covered where the port does it: src/mech3/zai_target.cpp:322 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004e3ef0 | 40 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:383) |
| 0x004e3f80 | 39 | folded | as MW3 0x004d3a80: covered where the port does it: src/mech3/zai_target.cpp:322 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004e41f0 | 9 | folded | a thunk: one call to another function |
| 0x004e4240 | 9 | folded | a thunk: one call to another function |
| 0x004e4250 | 9 | folded | a thunk: one call to another function |
| 0x004e4280 | 34 | folded | as MW3 0x004d3db0: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e4430 | 27 | folded | covered by the port where it names it (src/mech3/zai_valve.h:42) |
| 0x004e44a0 | 12 | folded | as MW3 0x004d4040: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e44c0 | 12 | unlabelled |  |
| 0x004e45c0 | 15 | unlabelled |  |
| 0x004e46d0 | 9 | folded | a thunk: one call to another function |
| 0x004e4860 | 9 | folded | a thunk: one call to another function |
| 0x004e4880 | 11 | folded | covered by the port where it names it (src/mech3/zai_steps.cpp:62) |
| 0x004e48a0 | 63 | folded | as MW3 0x004d44b0: covered where the port does it: src/mech3/zainet_node.cpp:40 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004e49d0 | 10 | folded | as MW3 0x004d45b0: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e49e0 | 20 | folded | covered by the port where it names it (src/mech3/zainet_node.h:10) |
| 0x004e4a50 | 43 | folded | as MW3 0x004d4630: covered where the port does it: src/mech3/zainet_node.cpp:106 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004e4ad0 | 9 | unlabelled | follows 0x004e4ae0:  |
| 0x004e4ae0 | 10 | unlabelled |  |
| 0x004e4b00 | 16 | folded | covered by the port where it names it (src/mech3/zai_valve.cpp:85) |
| 0x004e4b40 | 9 | folded | a thunk: one call to another function |
| 0x004e4b60 | 20 | folded | as MW3 0x004d46b0: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e4ba0 | 10 | folded | ported, by docs/symbols.csv: ported inline: the claim's temporary record zeroed |
| 0x004e4bb0 | 18 | unlabelled |  |
| 0x004e4c50 | 22 | folded | ported, by docs/symbols.csv: ported (anim_owner_release): every record of the owner let go; the valve's test-and-store does nothing |
| 0x004e4ca0 | 9 | folded | a thunk: one call to another function |
| 0x004e4cb0 | 9 | folded | a thunk: one call to another function |
| 0x004e4cd0 | 32 | folded | as MW3 0x004d4870: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e4d40 | 28 | folded | covered by the port where it names it (src/mech3/zai_anim_owner.h:27) |
| 0x004e4db0 | 12 | unlabelled |  |
| 0x004e4e70 | 39 | folded | ported, by docs/symbols.csv: ported (anim_owner_message): the owner's vt+0x74, 0x11 viaNode, 0x2a..0x31 the valve off/on, 0x39..0x3c the inout tests |
| 0x004e4f60 | 61 | unlabelled |  |
| 0x004e50a0 | 9 | folded | a thunk: one call to another function |
| 0x004e50b0 | 129 | unlabelled |  |
| 0x004e52b0 | 20 | unlabelled |  |
| 0x004e52f0 | 85 | folded | group A, read: the same logic; PM passes the owner and the callback (0x00521bb0 with 0x004e4f40) where MW3's 0x00508f00 takes them in registers, and drops two locals |
| 0x004e5490 | 15 | unlabelled |  |
| 0x004e54c0 | 10 | folded | ported, by docs/symbols.csv: ported: zainet (vtable 0x005ccda8) |
| 0x004e54e0 | 12 | unlabelled |  |
| 0x004e5500 | 10 | folded | covered by the port where it names it (src/mech3/zainet.h:36) |
| 0x004e5510 | 22 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:27) |
| 0x004e5660 | 12 | unlabelled |  |
| 0x004e5670 | 23 | folded | as MW3 0x004d52b0: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e56a0 | 23 | folded | as MW3 0x004d5300: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e56d0 | 64 | folded | as MW3 0x004d5350: covered where the port does it: src/mech3/app_mission.cpp:699 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004e5890 | 12 | unlabelled |  |
| 0x004e58b0 | 11 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004d5530: the AI and editor dialogs' MFC base (vtable 0x005cce00: CDialog constructor, modeless Create, close with the cursor hidden, DoModal, OnInitDialog with the cursor shown) |
| 0x004e58e0 | 10 | unlabelled |  |
| 0x004e58f0 | 15 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004d5570: the AI and editor dialogs' MFC base (vtable 0x005cce00: CDialog constructor, modeless Create, close with the cursor hidden, DoModal, OnInitDialog with the cursor shown) |
| 0x004e5920 | 12 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004d55a0: the AI and editor dialogs' MFC base (vtable 0x005cce00: CDialog constructor, modeless Create, close with the cursor hidden, DoModal, OnInitDialog with the cursor shown) |
| 0x004e5940 | 10 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004d55c0: the AI and editor dialogs' MFC base (vtable 0x005cce00: CDialog constructor, modeless Create, close with the cursor hidden, DoModal, OnInitDialog with the cursor shown) |
| 0x004e5950 | 13 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004d55d0: the AI and editor dialogs' MFC base (vtable 0x005cce00: CDialog constructor, modeless Create, close with the cursor hidden, DoModal, OnInitDialog with the cursor shown) |
| 0x004e5970 | 15 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:487) |
| 0x004e5990 | 15 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:487) |
| 0x004e59c0 | 28 | folded | as MW3 0x004d5640: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e5a10 | 9 | folded | a thunk: one call to another function |
| 0x004e5a60 | 68 | folded | as MW3 0x004d56e0: covered where the port does it: src/mech3/mech.h:102 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004e5c00 | 12 | unlabelled |  |
| 0x004e5c20 | 38 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x004e5d00 | 56 | unlabelled |  |
| 0x004e5f70 | 16 | folded | covered by the port where it names it (src/mech3/sensors.cpp:653) |
| 0x004e63e0 | 71 | folded | read: the previous-ally / previous-enemy keys (PM's commands 0x4d, 0x4e; MW3's 0x4d5ee0 and 0x4d6260, the port's prev_ally / prev_enemy, zai_target.h:95-96), unpaired by the diff: the mirror of the next key 0x004e6840 (read whole, the same logic), with the same differences (PM's loops break where MW3's set flags, the side tests 0x004e6a50 / 0x004e6a80 inlined in places, the target set at +0xcc) |
| 0x004e6520 | 31 | unlabelled |  |
| 0x004e65d0 | 71 | folded | read: the previous-ally / previous-enemy keys (PM's commands 0x4d, 0x4e; MW3's 0x4d5ee0 and 0x4d6260, the port's prev_ally / prev_enemy, zai_target.h:95-96), unpaired by the diff: the mirror of the next key 0x004e6840 (read whole, the same logic), with the same differences (PM's loops break where MW3's set flags, the side tests 0x004e6a50 / 0x004e6a80 inlined in places, the target set at +0xcc) |
| 0x004e6840 | 110 | folded | read: the next-ally key (PM's command 0x47; MW3's 0x46 runs 0x004d64b0, the port's AiTarget::next_ally, zai_target.h:97), unpaired by the diff: the same scan forward from the target, the same per-side tests (mode 2 when both fail, modes past 2 pass), the same wrap-round scan and the nearest-ally fallback (0x004e6df0, MW3's 0x004d6970); PM's loops break where MW3's set flags, and the target set is one vtable slot on (+0xcc) |
| 0x004e6ab0 | 106 | folded | ported, by docs/symbols.csv: ported: E |
| 0x004e6ed0 | 50 | folded | ported, by docs/symbols.csv: ported: Ctrl+E |
| 0x004e7170 | 35 | unlabelled |  |
| 0x004e75d0 | 12 | UNSURE | as MW3 0x004d7160: an entity vtable slot (0x005bcf64 and two more vtables, beside 0x004d7270): when on, tints the object (blend weight 0.6, 0x004fb8c0) and switches its blend use (0x004fb840); not found in the port; to settle which slot it is and who calls it |
| 0x004e7670 | 8 | unlabelled |  |
| 0x004e76d0 | 9 | folded | covered by the port where it names it (src/mech3/zai_target.h:110) |
| 0x004e77d0 | 19 | folded | as MW3 0x004d7330: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e7800 | 9 | folded | inside its ported callers (0x004e5f90): a small helper their ports do their own way |
| 0x004e7820 | 9 | folded | inside its ported callers (0x004e5f90): a small helper their ports do their own way |
| 0x004e7840 | 9 | folded | inside its ported callers (0x004e5f90): a small helper their ports do their own way |
| 0x004e7860 | 9 | folded | inside its ported callers (0x004e5f90): a small helper their ports do their own way |
| 0x004e7880 | 112 | folded | as MW3 0x004d7360: covered where the port does it: src/crt/msvc_sort.h:7 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004e7f40 | 78 | folded | as MW3 0x004d7a40: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e80c0 | 275 | folded | as MW3 0x004d7bc0: covered where the port does it: src/mech3/sensors.cpp:586 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004e8460 | 84 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x004e86a0 | 12 | unlabelled |  |
| 0x004e86c0 | 24 | folded | covered by the port where it names it (src/mech3/app_pocket.cpp:73) |
| 0x004e8730 | 9 | folded | a thunk: one call to another function |
| 0x004e8750 | 9 | folded | a thunk: one call to another function |
| 0x004e8760 | 47 | folded | as MW3 0x004d8280: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e8810 | 12 | folded | as MW3 0x004d8330: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e88b0 | 28 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:517) |
| 0x004e8910 | 48 | folded | as MW3 0x004d8420: covered where the port does it: src/mech3/zrdrvalve.h:36 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004e8990 | 9 | folded | follows 0x004e8760: as MW3 0x004d8280: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e89a0 | 9 | folded | a thunk: one call to another function |
| 0x004e8a20 | 19 | folded | covered by the port where it names it (src/mech3/zai_list.h:5) |
| 0x004e8a50 | 12 | unlabelled |  |
| 0x004e8a70 | 32 | folded | covered by the port where it names it (src/mech3/zai_list.h:6) |
| 0x004e8b30 | 33 | folded | covered by the port where it names it (src/mech3/mech_control.cpp:550) |
| 0x004e8c00 | 31 | unlabelled |  |
| 0x004e8cc0 | 23 | folded | as MW3 0x004d8820: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e8e00 | 9 | unlabelled | follows 0x004285d0:  |
| 0x004e8ee0 | 23 | folded | as MW3 0x004d8c40: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e9130 | 17 | folded | as MW3 0x004d8d30: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e9150 | 20 | folded | as MW3 0x004d8d50: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e9170 | 20 | folded | as MW3 0x004d8d70: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e9190 | 23 | folded | as MW3 0x004d8d90: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e91d0 | 21 | folded | as MW3 0x004d8dc0: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e9210 | 18 | folded | as MW3 0x004d8df0: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e9230 | 9 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port; a PM change around it is its caller's label |
| 0x004e9240 | 10 | folded | as MW3 0x004d8e20: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e9260 | 10 | unlabelled |  |
| 0x004e93e0 | 10 | folded | as MW3 0x004d8fe0: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e9400 | 15 | folded | as MW3 0x004d9000: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e9420 | 26 | folded | as MW3 0x004d9020: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e9480 | 31 | folded | as MW3 0x004d9090: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e9590 | 16 | folded | as MW3 0x004d91e0: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e95c0 | 17 | folded | as MW3 0x004d9210: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e95f0 | 17 | folded | as MW3 0x004d9250: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e9660 | 29 | folded | as MW3 0x004d92f0: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e96b0 | 29 | folded | as MW3 0x004d9360: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e9770 | 69 | folded | as MW3 0x004d9480: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004e9860 | 69 | folded | as MW3 0x004d9580: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004e9950 | 106 | folded | as MW3 0x004d9680: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004e9ae0 | 32 | folded | ported, by docs/symbols.csv: ported: PlayerControls::settings (an AiParams with the defaults) |
| 0x004e9be0 | 37 | folded | covered by the port where it names it (src/mech3/game_editor_vehicles.cpp:536) |
| 0x004e9e70 | 11 | folded | as MW3 0x004d9bf0: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e9fc0 | 9 | folded | a thunk: one call to another function |
| 0x004e9fd0 | 24 | folded | as MW3 0x004d9cd0: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004ea030 | 17 | folded | as MW3 0x004d9d40: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004ea070 | 9 | folded | inside its ported callers (0x004d9d00, 0x004dedf0, 0x004e0790, 0x004e1fd0): a small helper their ports do their own way |
| 0x004ea080 | 9 | folded | covered by the port where it names it (src/mech3/app_sensors.cpp:162) |
| 0x004ea090 | 10 | folded | an owning pointer's move (the source's taken and nulled), the brain's ownership (zai_brain_pm.cpp, the port's own); not run under a test (trace) |
| 0x004ea0b0 | 16 | unlabelled |  |
| 0x004ea0e0 | 9 | folded | inside its ported callers (0x004fb260): a small helper their ports do their own way |
| 0x004ea0f0 | 43 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_brain calls it directly, test_pm_ai_walk's walk step 0x004d9d00 too; the calls in order and the brain's fields compared |
| 0x004ea180 | 43 | unlabelled |  |
| 0x004ea3a0 | 87 | unlabelled |  |
| 0x004ea5c0 | 16 | unlabelled |  |
| 0x004ea690 | 10 | unlabelled |  |
| 0x004ea8a0 | 18 | unlabelled |  |
| 0x004ea8d0 | 99 | unlabelled |  |
| 0x004eab10 | 8 | unlabelled |  |
| 0x004eab40 | 23 | folded | covered by the port where it names it (src/mech3/zai_attack_pm.cpp:17) |
| 0x004eac00 | 34 | folded | covered by the port where it names it (src/mech3/zai_brain.cpp:1059) |
| 0x004eac90 | 17 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x004d9d80: an AI parameters dialog (MFC): what it reads into its list (the net's records) and positions; preserve as logic |
| 0x004eacd0 | 44 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x004d9dc0: an AI parameters dialog (MFC): what it reads into its list (the net's records) and positions; preserve as logic |
| 0x004eade0 | 27 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x004d9ed0: an AI parameters dialog (MFC): what it reads into its list (the net's records) and positions; preserve as logic |
| 0x004eae40 | 17 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004d9f30: the same AI parameters dialog's MFC plumbing (UpdateData, a flag cleared on close) |
| 0x004eaf90 | 23 | folded | covered by the port where it names it (src/mech3/app_pocket.cpp:73) |
| 0x004eaff0 | 9 | folded | a thunk: one call to another function |
| 0x004eb020 | 9 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x004d9f70: the same AI parameters dialog's MFC plumbing (UpdateData, a flag cleared on close) |
| 0x004eb030 | 9 | folded | a thunk: one call to another function |
| 0x004eb070 | 32 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x004d9fc0, whose class PM's changes keep: an AI parameters dialog (MFC): what it reads into its list (the net's records) and positions; preserve as logic |
| 0x004eb290 | 8 | folded | the same as its MW3 twin 0x004da1f0 but returns this (MW3's returns nothing; no caller reads it in MW3); not run under a test (trace) |
| 0x004eb2a0 | 11 | folded | as MW3 0x004da200: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004eb2c0 | 44 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004eb390 | 15 | folded | as MW3 0x004da2f0: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004eb3c0 | 22 | folded | as MW3 0x004da320: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004eb420 | 22 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004eb490 | 44 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004eb560 | 22 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004eb5d0 | 88 | unlabelled |  |
| 0x004eb7a0 | 8 | folded | returns a constant or a global |
| 0x004eb7b0 | 14 | folded | as MW3 0x004da740: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004eb7f0 | 10 | folded | as MW3 0x004da790: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004eb810 | 12 | unlabelled |  |
| 0x004eb830 | 10 | folded | as MW3 0x004da7d0: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004eb860 | 10 | folded | as MW3 0x004da800: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004eb880 | 12 | unlabelled |  |
| 0x004eb950 | 9 | folded | a thunk: one call to another function |
| 0x004eb960 | 195 | unlabelled |  |
| 0x004ebcf0 | 24 | folded | as MW3 0x004dac70: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ebd40 | 24 | folded | as MW3 0x004dacb0: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ebec0 | 9 | unlabelled | follows 0x004d1790:  |
| 0x004ebed0 | 24 | folded | as MW3 0x004dae40: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ebf70 | 118 | folded | ported, by docs/symbols.csv: ported: zrdrainet |
| 0x004ec1d0 | 19 | folded | as MW3 0x004db140: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ec240 | 319 | unlabelled |  |
| 0x004ecaa0 | 238 | folded | read: the editor's net node text writer (MW3 0x004db8d0), PM's with three more properties (via, terminus, static_mfb); the port writes nets with its own writer (net_save.h:25), which writes those three (net_save.cpp:155-161) |
| 0x004ed2d0 | 9 | folded | a thunk: one call to another function |
| 0x004ed2e0 | 44 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ed3b0 | 9 | folded | a thunk: one call to another function |
| 0x004ed3e0 | 11 | folded | covered by the port where it names it (src/mech3/zrdrainet.h:91) |
| 0x004ed3f0 | 11 | folded | as MW3 0x004dc270: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ed410 | 11 | folded | as MW3 0x004dc290: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ed790 | 13 | folded | as MW3 0x004dc620: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ed7b0 | 14 | folded | as MW3 0x004dc640: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ed7e0 | 14 | folded | as MW3 0x004dc670: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ed7f0 | 11 | folded | as MW3 0x004dc680: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ed850 | 14 | folded | as MW3 0x004dc6e0: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ed860 | 11 | folded | as MW3 0x004dc6f0: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ed870 | 8 | folded | returns a constant or a global |
| 0x004ed880 | 319 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004eddb0 | 59 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004eded0 | 59 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004edff0 | 32 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ee080 | 75 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ee1d0 | 319 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ee700 | 32 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ee790 | 198 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004eeaa0 | 47 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004eeb50 | 51 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004eec10 | 47 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004eecc0 | 24 | folded | as MW3 0x004ddc20: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004eed30 | 171 | folded | as MW3 0x004ddc90: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ef160 | 12 | folded | read: a container element's copy (a vector's construct: 3 or 2 words, nothing for a null slot); the port's containers |
| 0x004ef180 | 21 | unlabelled |  |
| 0x004ef1d0 | 12 | unlabelled |  |
| 0x004ef200 | 9 | folded | covered by the port where it names it (src/mech3/zai_drive_pm.cpp:499) |
| 0x004ef220 | 16 | folded | as MW3 0x004de100: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ef250 | 10 | folded | as MW3 0x004de130: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ef270 | 75 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ef3c0 | 20 | folded | as MW3 0x004de260: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ef420 | 198 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ef730 | 42 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ef7c0 | 51 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004efa60 | 21 | unlabelled |  |
| 0x004efab0 | 18 | folded | as MW3 0x004de980: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004efc20 | 12 | unlabelled |  |
| 0x004efeb0 | 40 | unlabelled |  |
| 0x004eff70 | 12 | unlabelled |  |
| 0x004eff90 | 58 | UNSURE | as MW3 0x004dee50: the aiv reader's vtable slot 0x005cd42c (58 lines): a name checked against zEntity's list (0x005a8210) and the reader's records (std::map find 0x004e1a30), no caller found; probably the editor's 'is this name free'; to settle by its slot and callers |
| 0x004f00e0 | 77 | unlabelled |  |
| 0x004f06c0 | 9 | folded | a thunk: one call to another function |
| 0x004f0720 | 9 | folded | a thunk: one call to another function |
| 0x004f0740 | 12 | folded | covered by the port where it names it (src/mech3/zrdraiv.h:62) |
| 0x004f0770 | 9 | folded | a thunk: one call to another function |
| 0x004f07a0 | 19 | dev dialog (MFC), its logic: PORT (preserve) | as MW3 0x004df6a0: the editor's 'add vehicle to node' record (an aiv record built on OK, editor.md); in the release its path returns 0 and nothing is made; ours does it as meant: upgrades/editor/add_vehicle |
| 0x004f07f0 | 10 | folded | covered by the port where it names it (src/mech3/game_editor_vehicles.cpp:590) |
| 0x004f0810 | 11 | folded | covered by the port where it names it (src/mech3/game_editor_vehicles.cpp:554) |
| 0x004f0830 | 486 | unlabelled |  |
| 0x004f18d0 | 8 | folded | returns a constant or a global |
| 0x004f18e0 | 16 | folded | covered by the port where it names it (src/mech3/zai_wire_states_pm.h:103) |
| 0x004f1a40 | 9 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x004f1a60 | 8 | folded | returns a constant or a global |
| 0x004f1a80 | 9 | folded | a thunk: one call to another function |
| 0x004f1aa0 | 11 | folded | covered by the port where it names it (src/mech3/zrdraiv.h:96) |
| 0x004f1ad0 | 12 | unlabelled |  |
| 0x004f1b00 | 9 | folded | a thunk: one call to another function |
| 0x004f1b30 | 9 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:1247) |
| 0x004f1b40 | 14 | folded | covered by the port where it names it (src/mech3/zrdraiv.cpp:529) |
| 0x004f1b80 | 9 | folded | a thunk: one call to another function |
| 0x004f1ba0 | 9 | folded | a thunk: one call to another function |
| 0x004f1bd0 | 17 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:1247) |
| 0x004f2030 | 16 | folded | as MW3 0x004e0c20: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f2060 | 100 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f21f0 | 59 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f2310 | 42 | folded | as MW3 0x004e0f20: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f2390 | 319 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f2920 | 32 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f29b0 | 41 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f2a70 | 221 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f2de0 | 56 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f2e60 | 51 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f2f20 | 47 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f2fd0 | 24 | unlabelled |  |
| 0x004f3070 | 9 | folded | a thunk: one call to another function |
| 0x004f3090 | 9 | folded | a thunk: one call to another function |
| 0x004f30c0 | 21 | folded | as MW3 0x004e1d00: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f3110 | 12 | unlabelled |  |
| 0x004f3130 | 111 | unlabelled |  |
| 0x004f3370 | 12 | folded | as MW3 0x0042f430: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x004f3390 | 88 | folded | read: the valve file's release (zrdrvalve.cpp:573), the same frees; PM frees each through 0x004f3370 (free and clear) where MW3 inlines them |
| 0x004f3570 | 24 | folded | as MW3 0x004e20d0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f35c0 | 22 | unlabelled |  |
| 0x004f3630 | 117 | unlabelled |  |
| 0x004f3880 | 15 | folded | as MW3 0x004e22e0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f38b0 | 11 | unlabelled |  |
| 0x004f38c0 | 9 | debug or dev tool: PORT (preserve) | as MW3 0x004e2310: the valve reader's print method (a vtable slot that fprintf's a value); nothing in the game prints a reader |
| 0x004f4d20 | 15 | folded | covered by the port where it names it (src/mech3/zrdrvalve.cpp:563) |
| 0x004f4d60 | 15 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_valves' valve builder 0x004f5f50 runs it on every shipped valves.zrd and crafted records; every call out, every valve's value and the words written on the brains and targets compared |
| 0x004f4d90 | 14 | unlabelled |  |
| 0x004f4dc0 | 10 | folded | as MW3 0x004e3150: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f4de0 | 15 | folded | as MW3 0x004e30b0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f4e20 | 15 | unlabelled |  |
| 0x004f4e50 | 14 | unlabelled |  |
| 0x004f4e80 | 10 | folded | as MW3 0x004e3070: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f4ea0 | 12 | unlabelled |  |
| 0x004f4ed0 | 15 | folded | as MW3 0x004e3180: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f4f70 | 36 | folded | inside its ported callers (0x004f5f50): a small helper their ports do their own way |
| 0x004f5030 | 12 | unlabelled |  |
| 0x004f50f0 | 11 | folded | as MW3 0x004e3310: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5110 | 12 | unlabelled |  |
| 0x004f5150 | 16 | folded | as MW3 0x004e3350: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5180 | 18 | folded | as MW3 0x004e33c0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f51c0 | 12 | unlabelled |  |
| 0x004f51e0 | 14 | folded | as MW3 0x004e3450: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5240 | 16 | folded | inside its ported callers (0x004f5f50): a small helper their ports do their own way |
| 0x004f5260 | 19 | folded | as MW3 0x004e3560: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f52b0 | 12 | unlabelled |  |
| 0x004f52d0 | 22 | folded | as MW3 0x004e35f0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5360 | 16 | folded | inside its ported callers (0x004f5f50): a small helper their ports do their own way |
| 0x004f5380 | 19 | folded | as MW3 0x004e3700: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f53d0 | 12 | unlabelled |  |
| 0x004f53f0 | 14 | folded | as MW3 0x004e3790: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f54b0 | 16 | folded | inside its ported callers (0x004f5f50): a small helper their ports do their own way |
| 0x004f54d0 | 19 | folded | as MW3 0x004e38d0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5520 | 12 | unlabelled |  |
| 0x004f5540 | 14 | folded | as MW3 0x004e3960: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f55d0 | 16 | folded | inside its ported callers (0x004f5f50): a small helper their ports do their own way |
| 0x004f55f0 | 22 | folded | as MW3 0x004e3a90: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5650 | 12 | unlabelled |  |
| 0x004f5670 | 14 | folded | as MW3 0x004e3b40: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f56a0 | 42 | unlabelled |  |
| 0x004f57e0 | 16 | folded | inside its ported callers (0x004f5f50): a small helper their ports do their own way |
| 0x004f5800 | 22 | folded | as MW3 0x004e3d20: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5860 | 10 | folded | as MW3 0x004e3d70: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5890 | 26 | folded | as MW3 0x004e3da0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f58f0 | 16 | folded | as MW3 0x004e3e00: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5990 | 9 | folded | as MW3 0x004e3f00: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f59e0 | 25 | folded | as MW3 0x004e3f50: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5a40 | 9 | folded | as MW3 0x004e31f0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5ac0 | 26 | folded | as MW3 0x004e4020: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5b20 | 21 | folded | covered by the port where it names it (src/mech3/zrdrvalve.cpp:401) |
| 0x004f5b80 | 104 | unlabelled |  |
| 0x004f5d30 | 12 | folded | as MW3 0x004e42e0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5d50 | 8 | folded | as MW3 0x004e4300: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5d60 | 12 | unlabelled |  |
| 0x004f5d80 | 22 | folded | as MW3 0x004e4310: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5e00 | 11 | folded | as MW3 0x004e4390: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5e20 | 17 | folded | as MW3 0x004e43b0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5e50 | 11 | folded | as MW3 0x004e43e0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5e70 | 17 | folded | as MW3 0x004e4400: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5ea0 | 17 | folded | as MW3 0x004e4430: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5ed0 | 18 | folded | as MW3 0x004e4460: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5f00 | 8 | folded | inside its ported callers (0x004c9600): a small helper their ports do their own way |
| 0x004f5f10 | 9 | unlabelled |  |
| 0x004f5f20 | 12 | folded | covered by the port where it names it (src/mech3/zai_valve.h:20) |
| 0x004f5f40 | 9 | folded | as MW3 0x004e42c0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f5f50 | 317 | unlabelled |  |
| 0x004f6c10 | 42 | folded | as MW3 0x004e4f90: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f6cc0 | 42 | folded | as MW3 0x004e4d20: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f6d70 | 42 | folded | as MW3 0x004e4df0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f6e20 | 42 | folded | as MW3 0x004e4ec0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f6ed0 | 16 | folded | as MW3 0x0049cc80: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x004f6f00 | 46 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_valves' valve builder 0x004f5f50 runs it on every shipped valves.zrd and crafted records; every call out, every valve's value and the words written on the brains and targets compared |
| 0x004f6fa0 | 29 | folded | as MW3 0x004e5140: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f7010 | 28 | folded | inside its ported callers (0x004f38e0): a small helper their ports do their own way |
| 0x004f7070 | 27 | folded | as MW3 0x004e51c0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f70e0 | 20 | folded | as MW3 0x004e5240: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f7130 | 53 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f7250 | 9 | debug or dev tool: PORT (preserve) | as MW3 0x004e53b0: the valve reader's print method (a vtable slot that fprintf's a value); nothing in the game prints a reader |
| 0x004f7270 | 9 | folded | a thunk: one call to another function |
| 0x004f7290 | 70 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f7490 | 10 | folded | as MW3 0x004e55d0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f74b0 | 17 | folded | as MW3 0x004e55f0: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f74e0 | 12 | unlabelled |  |
| 0x004f7500 | 10 | unlabelled |  |
| 0x004f7510 | 59 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f7630 | 319 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f7b60 | 32 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f7bf0 | 47 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f7d90 | 30 | unlabelled |  |
| 0x004f7e30 | 21 | folded | as MW3 0x004e6110: the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004f7e70 | 12 | unlabelled |  |
| 0x004f7e90 | 42 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004f7f30 | 11 | unlabelled |  |
| 0x004f7f50 | 12 | unlabelled |  |
| 0x004f7f80 | 8 | folded | returns a constant or a global |
| 0x004f7fb0 | 9 | unlabelled |  |
| 0x004f7fd0 | 8 | unlabelled |  |
| 0x004f8020 | 12 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:2042) |
| 0x004f8040 | 10 | folded | as MW3 0x004e6240: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004f8050 | 12 | folded | ported, by docs/symbols.csv: ported; 0x0c, 0x004e66a0(0) |
| 0x004f8070 | 40 | unlabelled |  |
| 0x004f81a0 | 10 | folded | ported, by docs/symbols.csv: ported; node +0x2c bits 5 and 6 |
| 0x004f81d0 | 14 | folded | covered by the port where it names it (src/mech3/mech_shade.cpp:100) |
| 0x004f8210 | 19 | folded | ported, by docs/symbols.csv: ported; from the centroid's world position |
| 0x004f8490 | 10 | folded | ported, by docs/symbols.csv: ported in shade_before |
| 0x004f84b0 | 9 | folded | ported, by docs/symbols.csv: ported in shade_after |
| 0x004f8690 | 9 | folded | a thunk: one call to another function |
| 0x004f86a0 | 9 | folded | a thunk: one call to another function |
| 0x004f86f0 | 16 | folded | as MW3 0x004e6960: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004f8720 | 9 | folded | a thunk: one call to another function |
| 0x004f87a0 | 9 | folded | a thunk: one call to another function |
| 0x004f87b0 | 9 | folded | a thunk: one call to another function |
| 0x004f87e0 | 14 | folded | covered by the port where it names it (src/mech3/sound_extra.cpp:32) |
| 0x004f8820 | 26 | folded | covered by the port where it names it (src/mech3/sound_extra.cpp:32) |
| 0x004f8970 | 31 | unlabelled |  |
| 0x004f8b50 | 75 | folded | as MW3 0x004e6dc0: covered where the port does it: src/mech3/app_mech.h:142 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004f8c70 | 51 | folded | ported, by docs/symbols.csv: ported: lance_orders contact_on |
| 0x004f8d80 | 20 | folded | as MW3 0x004e6ff0: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004f8da0 | 20 | folded | MW3's 0x004e7010 unpaired (a contact's vehicle as a mech: vt+0x2c then vt+0x58, PM's vehicle class has it at vt+0x60), for the network state's target player (+0x34; app_mech.cpp:1089, net_mech_of's owner); not run under a test (trace) |
| 0x004f8e30 | 50 | folded | read: the same as its MW3 twin, its copies taken in another form (one-sided statements only a loop's form and a vtable pointer's place) |
| 0x004f8f60 | 31 | unlabelled |  |
| 0x004f9020 | 20 | folded | as MW3 0x004e7290: AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004f9090 | 22 | MISSING LOGIC | pm only: PM's turret turn (0x004f90f0, MW3 0x004e72f0, veh_controls.cpp turret_turn) reads the turret node's world matrix through this, which first brings it up to date (0x004fefa0 when the node's data bit 0x20 is set); MW3 reads data +0x60 as it stands, and so does the port's node_world_matrix hook. Effect: PM's turret aims from this update's matrix, MW3's from the last; to check at fix time |
| 0x004f90f0 | 53 | unlabelled |  |
| 0x004f9290 | 38 | unlabelled |  |
| 0x004f9450 | 9 | folded | a thunk: one call to another function |
| 0x004f9460 | 9 | folded | a thunk: one call to another function |
| 0x004f9510 | 9 | folded | a thunk: one call to another function |
| 0x004f9530 | 9 | unlabelled | follows 0x004fbe10:  |
| 0x004f9540 | 9 | unlabelled | follows 0x004fa770:  |
| 0x004f9550 | 9 | unlabelled | follows 0x004fa190:  |
| 0x004f9560 | 9 | unlabelled | follows 0x004fa110:  |
| 0x004f9570 | 9 | folded | a thunk: one call to another function |
| 0x004f9580 | 9 | unused: PORT (preserve) | follows 0x004faa40: as MW3 0x004e95b0: IControls vt+0x84 of the AI controls (drop the target if it is the given entity's, stand the firing down): no caller reaches that slot (docs/ai.md, the AI controls' unreached slots) |
| 0x004f9590 | 9 | folded | a thunk: one call to another function |
| 0x004f95a0 | 9 | folded | a thunk: one call to another function |
| 0x004f95b0 | 9 | folded | a thunk: one call to another function |
| 0x004f95c0 | 9 | folded | a thunk: one call to another function |
| 0x004f95d0 | 9 | folded | a thunk: one call to another function |
| 0x004f95e0 | 9 | folded | a thunk: one call to another function |
| 0x004f95f0 | 9 | folded | a thunk: one call to another function |
| 0x004f9600 | 9 | folded | a thunk: one call to another function |
| 0x004f96d0 | 65 | folded | as MW3 0x004e78d0: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004f97c0 | 12 | unlabelled |  |
| 0x004f97e0 | 56 | folded | as MW3 0x004e79d0: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004f98d0 | 17 | folded | as MW3 0x004e7b00: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004f9900 | 17 | unlabelled |  |
| 0x004f9960 | 17 | folded | read: the mission listeners' slot +0x10 called on each (0x004f9a20), the port's mission_started (app_states.cpp:817 names 0x004f9960) |
| 0x004f9990 | 17 | unlabelled |  |
| 0x004f99c0 | 17 | folded | read: the same as its MW3 twin, its copies taken in another form (one-sided statements only a loop's form and a vtable pointer's place) |
| 0x004f99f0 | 11 | folded | as MW3 0x004e7c10: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004f9a00 | 11 | folded | as MW3 0x004e7c20: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004f9a10 | 11 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:2234) |
| 0x004f9a20 | 11 | unlabelled |  |
| 0x004f9a30 | 11 | unlabelled |  |
| 0x004f9a40 | 11 | unlabelled |  |
| 0x004f9b90 | 24 | folded | as MW3 0x004e7d80: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004f9bd0 | 12 | folded | as MW3 0x004e7dc0: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004f9c00 | 16 | folded | as MW3 0x004e7df0: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004f9c30 | 55 | folded | group A, read: the same logic; the field +0xd8 is +0xf8 on PM's layout, and the two free paths are joined |
| 0x004f9d30 | 9 | folded | a thunk: one call to another function |
| 0x004f9d40 | 9 | folded | a thunk: one call to another function |
| 0x004f9d80 | 14 | unlabelled |  |
| 0x004f9da0 | 12 | unlabelled |  |
| 0x004f9df0 | 10 | folded | as MW3 0x004e8030: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004f9e10 | 12 | unlabelled |  |
| 0x004f9e30 | 10 | folded | as MW3 0x004e8070: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004f9e50 | 11 | folded | as MW3 0x004e8090: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004f9ec0 | 15 | unlabelled |  |
| 0x004f9f60 | 8 | unlabelled |  |
| 0x004f9f70 | 10 | folded | as MW3 0x004e8180: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004fa060 | 16 | folded | as MW3 0x004e8270: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004fa090 | 32 | folded | read: the brain's orders object's constructor (brain +0x128, from 0x004dd1e0, MW3 0x004e82a0 at +0x160): MW3's with PM's again byte +0x30 and pending list +0x34 added and the argument moved to +0x40; the port's OrdersPm (zai_brain_pm.h) carries the deque, again and pending |
| 0x004fa100 | 9 | folded | a thunk: one call to another function |
| 0x004fa110 | 11 | unlabelled |  |
| 0x004fa130 | 11 | folded | covered by the port where it names it (src/mech3/zai_wire_states_pm.h:32) |
| 0x004fa150 | 11 | folded | covered by the port where it names it (src/mech3/zai_brain_pm.cpp:295) |
| 0x004fa170 | 11 | folded | covered by the port where it names it (src/mech3/zai_brain_pm.cpp:301) |
| 0x004fa190 | 11 | unlabelled |  |
| 0x004fa1b0 | 9 | unlabelled | follows 0x004ea8d0:  |
| 0x004fa1d0 | 9 | unlabelled | follows 0x004eab10:  |
| 0x004fa1e0 | 9 | folded | a thunk: one call to another function |
| 0x004fa200 | 8 | folded | as MW3 0x004e8dc0: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004fa770 | 40 | unlabelled |  |
| 0x004faa20 | 9 | folded | a thunk: one call to another function |
| 0x004faa40 | 41 | unused: PORT (preserve) | as MW3 0x004e95b0: IControls vt+0x84 of the AI controls (drop the target if it is the given entity's, stand the firing down): no caller reaches that slot (docs/ai.md, the AI controls' unreached slots) |
| 0x004fafe0 | 9 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x004faff0 | 97 | unlabelled |  |
| 0x004fb3d0 | 9 | folded | a thunk: one call to another function |
| 0x004fb3f0 | 19 | folded | as MW3 0x004e9fd0: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004fb420 | 19 | unlabelled |  |
| 0x004fb4a0 | 12 | folded | covered by the port where it names it (src/mech3/zai_brain.cpp:1232) |
| 0x004fb4f0 | 19 | unlabelled |  |
| 0x004fb570 | 19 | unlabelled |  |
| 0x004fb5d0 | 19 | unlabelled |  |
| 0x004fb630 | 19 | unlabelled |  |
| 0x004fb690 | 12 | folded | covered by the port where it names it (src/mech3/zai_repair.cpp:232) |
| 0x004fb6e0 | 12 | unlabelled |  |
| 0x004fb730 | 19 | unlabelled |  |
| 0x004fb790 | 19 | unlabelled |  |
| 0x004fb7f0 | 19 | unlabelled |  |
| 0x004fb9b0 | 25 | unlabelled |  |
| 0x004fba80 | 25 | unlabelled |  |
| 0x004fbaf0 | 9 | folded | as MW3 0x004ea4a0: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004fbb00 | 25 | unlabelled |  |
| 0x004fbb70 | 20 | unlabelled |  |
| 0x004fbbc0 | 20 | unlabelled |  |
| 0x004fbe10 | 10 | unlabelled |  |
| 0x004fbe40 | 9 | folded | as MW3 0x004ea740: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004fbe50 | 9 | folded | as MW3 0x004ea750: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004fbe60 | 9 | folded | read: sets the brain's byte +0x158, the orders' again flag (OrdersPm::again in zai_brain_pm.h: run the last order once more), from 0x004d3c30 and 0x004d6070 |
| 0x004fbe70 | 9 | unlabelled |  |
| 0x004fbf30 | 23 | folded | covered by the port where it names it (src/mech3/zai_brain.h:591) |
| 0x004fbfd0 | 74 | folded | checked by test (block trace, 2026-11-13): test_pm_ai_brain runs it (from 0x004fafe0); the calls in order and the brain's fields compared at 53 bits |
| 0x004fc130 | 17 | folded | inside its ported callers (0x004faff0): a small helper their ports do their own way |
| 0x004fc150 | 31 | folded | as MW3 0x004ea870: the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004fc1c0 | 11 | unlabelled |  |
| 0x004fc1e0 | 22 | replaced (SDL or the platform layer) | as MW3 0x004ea9b0: the start's video device picker: an MFC CDialog with a list box (0x004eaab0 and its constructor, destructor, data exchange and init); the SDL GPU device replaces it |
| 0x004fc240 | 12 | unlabelled |  |
| 0x004fc260 | 20 | replaced (SDL or the platform layer) | as MW3 0x004eaa30: the start's video device picker: an MFC CDialog with a list box (0x004eaab0 and its constructor, destructor, data exchange and init); the SDL GPU device replaces it |
| 0x004fc2b0 | 9 | replaced (SDL or the platform layer) | as MW3 0x004eaa80: the start's video device picker: an MFC CDialog with a list box (0x004eaab0 and its constructor, destructor, data exchange and init); the SDL GPU device replaces it |
| 0x004fc2d0 | 8 | folded | returns a constant or a global |
| 0x004fc2e0 | 51 | replaced (SDL or the platform layer) | as MW3 0x004eaab0: the start's video device picker: an MFC CDialog with a list box (0x004eaab0 and its constructor, destructor, data exchange and init); the SDL GPU device replaces it |
| 0x004fc3f0 | 44 | replaced (SDL or the platform layer) | as MW3 0x004eaba0: the start's video device picker: an MFC CDialog with a list box (0x004eaab0 and its constructor, destructor, data exchange and init); the SDL GPU device replaces it |
| 0x004fc4d0 | 15 | dev dialog (MFC), its window plumbing: replaced | as MW3 0x00432da0: the mech list dialog's MFC plumbing (constructor, destructor, DDX, OnOK, DoModal) |
| 0x004fc650 | 30 | replaced (SDL or the platform layer) | group A, read: the fatal abort's message box and exit (PM's console mode flushes and exits at once); the port's crash box stands in |

### zclass (302 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x004fc750 | 56 | unlabelled |  |
| 0x004fc860 | 36 | unlabelled |  |
| 0x004fc910 | 9 | folded | as MW3 0x004eb030: covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x004fc920 | 8 | folded | returns a constant or a global |
| 0x004fc930 | 28 | unlabelled |  |
| 0x004fc9b0 | 51 | unlabelled |  |
| 0x004fcb40 | 18 | unlabelled |  |
| 0x004fcb90 | 18 | unlabelled |  |
| 0x004fcbe0 | 34 | unlabelled |  |
| 0x004fcd10 | 14 | folded | covered by the port where it names it (src/gamez/zdeclient/zdec_crater.cpp:374) |
| 0x004fcd50 | 34 | unlabelled |  |
| 0x004fce00 | 34 | unlabelled |  |
| 0x004fceb0 | 25 | unlabelled |  |
| 0x004fcf20 | 18 | unlabelled |  |
| 0x004fcf70 | 14 | unlabelled |  |
| 0x004fcfb0 | 14 | folded | read: MW3's node getters and setters with the null-node report, the same bodies with their arguments on the stack (0x004fd280 is MW3's 0x004eb710, the INTERSECT bit; 0x004fd4a0 is 0x004eb8a0, +0x2c bit 5; 0x00506a60 is 0x004f3630, a camera's window; 0x004fcfb0 is 0x004eb6a0, the node's zone byte +0x30, unpaired by the diff; 0x005067c0 is 0x004f33e0, the camera case of 0x004fd7a0's switch on the class (MW3's 0x004ebb40, cls_node.cpp), through 0x004fd8d0 = MW3's 0x004ebc30); their callers' ports read and write the same fields (zai_brain.h node_intersect, mech_parts set_flags2c, app_hud.cpp:196, zone_visible(raw[0x30])) |
| 0x004fcff0 | 14 | MISSING LOGIC (only data the shipped game lacks reaches it) | checked by data: the script commands NodeSetOverrideAmbient, NodeSetOverrideAmbientValue, NodeSetOverrideDiffuse and NodeSetOverrideDiffuseValue (PM's dispatcher 0x005b2d10; node +0x2c bits 7, 9..16, 8, 17..24): no shipped PM script uses them (interp.zbd, 30 scripts) and the PM interp has no entry for them |
| 0x004fd040 | 13 | folded | covered by the port where it names it (src/gamez/zgeometry/zgeo_model.cpp:446) |
| 0x004fd080 | 14 | MISSING LOGIC (only data the shipped game lacks reaches it) | checked by data: the script commands NodeSetOverrideAmbient, NodeSetOverrideAmbientValue, NodeSetOverrideDiffuse and NodeSetOverrideDiffuseValue (PM's dispatcher 0x005b2d10; node +0x2c bits 7, 9..16, 8, 17..24): no shipped PM script uses them (interp.zbd, 30 scripts) and the PM interp has no entry for them |
| 0x004fd0d0 | 13 | MISSING LOGIC | PM's per-node light override in the model draw 0x00507f30 (pm only: the fix goes in PM's draw on the pm branch): node +0x2c bit 7 with an ambient value in bits 9..16 (0x004fd0d0 -> 0x0050e6f0), bit 8 with a diffuse value in bits 17..24 (0x004fd160 / 0x004fd1f0 -> 0x0050e7c0); the shared draw (camera.cpp:79) has none. Counted with the port's PM mechlib reader (probe_pm_mechlib_overrides, all 76 trees, 1,847 nodes): 24 nodes carry it, the cockpit 'grill' of every mech_*_hud.flt (+0x2c 0xb281 in the file: an ambient override of 0x59, no diffuse). No MW3 world or mechlib node and no PM world node sets these bits |
| 0x004fd110 | 14 | MISSING LOGIC (only data the shipped game lacks reaches it) | checked by data: the script commands NodeSetOverrideAmbient, NodeSetOverrideAmbientValue, NodeSetOverrideDiffuse and NodeSetOverrideDiffuseValue (PM's dispatcher 0x005b2d10; node +0x2c bits 7, 9..16, 8, 17..24): no shipped PM script uses them (interp.zbd, 30 scripts) and the PM interp has no entry for them |
| 0x004fd160 | 13 | MISSING LOGIC | PM's per-node light override in the model draw 0x00507f30 (pm only: the fix goes in PM's draw on the pm branch): node +0x2c bit 7 with an ambient value in bits 9..16 (0x004fd0d0 -> 0x0050e6f0), bit 8 with a diffuse value in bits 17..24 (0x004fd160 / 0x004fd1f0 -> 0x0050e7c0); the shared draw (camera.cpp:79) has none. Counted with the port's PM mechlib reader (probe_pm_mechlib_overrides, all 76 trees, 1,847 nodes): 24 nodes carry it, the cockpit 'grill' of every mech_*_hud.flt (+0x2c 0xb281 in the file: an ambient override of 0x59, no diffuse). No MW3 world or mechlib node and no PM world node sets these bits |
| 0x004fd1a0 | 14 | MISSING LOGIC (only data the shipped game lacks reaches it) | checked by data: the script commands NodeSetOverrideAmbient, NodeSetOverrideAmbientValue, NodeSetOverrideDiffuse and NodeSetOverrideDiffuseValue (PM's dispatcher 0x005b2d10; node +0x2c bits 7, 9..16, 8, 17..24): no shipped PM script uses them (interp.zbd, 30 scripts) and the PM interp has no entry for them |
| 0x004fd1f0 | 13 | MISSING LOGIC | PM's per-node light override in the model draw 0x00507f30 (pm only: the fix goes in PM's draw on the pm branch): node +0x2c bit 7 with an ambient value in bits 9..16 (0x004fd0d0 -> 0x0050e6f0), bit 8 with a diffuse value in bits 17..24 (0x004fd160 / 0x004fd1f0 -> 0x0050e7c0); the shared draw (camera.cpp:79) has none. Counted with the port's PM mechlib reader (probe_pm_mechlib_overrides, all 76 trees, 1,847 nodes): 24 nodes carry it, the cockpit 'grill' of every mech_*_hud.flt (+0x2c 0xb281 in the file: an ambient override of 0x59, no diffuse). No MW3 world or mechlib node and no PM world node sets these bits |
| 0x004fd230 | 18 | unlabelled |  |
| 0x004fd280 | 14 | folded | read: MW3's node getters and setters with the null-node report, the same bodies with their arguments on the stack (0x004fd280 is MW3's 0x004eb710, the INTERSECT bit; 0x004fd4a0 is 0x004eb8a0, +0x2c bit 5; 0x00506a60 is 0x004f3630, a camera's window; 0x004fcfb0 is 0x004eb6a0, the node's zone byte +0x30, unpaired by the diff; 0x005067c0 is 0x004f33e0, the camera case of 0x004fd7a0's switch on the class (MW3's 0x004ebb40, cls_node.cpp), through 0x004fd8d0 = MW3's 0x004ebc30); their callers' ports read and write the same fields (zai_brain.h node_intersect, mech_parts set_flags2c, app_hud.cpp:196, zone_visible(raw[0x30])) |
| 0x004fd2c0 | 18 | unlabelled |  |
| 0x004fd310 | 14 | unlabelled |  |
| 0x004fd350 | 18 | unlabelled |  |
| 0x004fd3a0 | 18 | unlabelled |  |
| 0x004fd3f0 | 14 | unlabelled |  |
| 0x004fd430 | 14 | unlabelled |  |
| 0x004fd4a0 | 14 | folded | read: MW3's node getters and setters with the null-node report, the same bodies with their arguments on the stack (0x004fd280 is MW3's 0x004eb710, the INTERSECT bit; 0x004fd4a0 is 0x004eb8a0, +0x2c bit 5; 0x00506a60 is 0x004f3630, a camera's window; 0x004fcfb0 is 0x004eb6a0, the node's zone byte +0x30, unpaired by the diff; 0x005067c0 is 0x004f33e0, the camera case of 0x004fd7a0's switch on the class (MW3's 0x004ebb40, cls_node.cpp), through 0x004fd8d0 = MW3's 0x004ebc30); their callers' ports read and write the same fields (zai_brain.h node_intersect, mech_parts set_flags2c, app_hud.cpp:196, zone_visible(raw[0x30])) |
| 0x004fd4f0 | 14 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:205) |
| 0x004fd540 | 16 | unlabelled |  |
| 0x004fd590 | 18 | unlabelled |  |
| 0x004fd5e0 | 59 | unlabelled |  |
| 0x004fd6f0 | 34 | unlabelled |  |
| 0x004fd7a0 | 60 | unlabelled |  |
| 0x004fd8d0 | 69 | unlabelled |  |
| 0x004fda40 | 136 | unlabelled |  |
| 0x004fdd60 | 119 | unlabelled |  |
| 0x004fdfe0 | 105 | unlabelled |  |
| 0x004fe220 | 117 | unlabelled |  |
| 0x004fe4c0 | 17 | unlabelled |  |
| 0x004fe510 | 104 | unlabelled |  |
| 0x004feba0 | 163 | unlabelled |  |
| 0x004fefa0 | 147 | unlabelled |  |
| 0x004ff2f0 | 26 | unlabelled |  |
| 0x004ff620 | 25 | unlabelled |  |
| 0x004ff730 | 9 | folded | covered by the port where it names it (src/mech3/mech_command.cpp:313) |
| 0x004ff740 | 8 | folded | returns a constant or a global |
| 0x004ff770 | 9 | folded | covered by the port where it names it (src/pm/gamez/zmodel/gmod_draw.cpp:320) |
| 0x004ff7d0 | 9 | folded | ported, by docs/symbols.csv: ported as zcls::segment_options().first_hit ([0x0060b968]) |
| 0x004ff7e0 | 9 | folded | ported, by docs/symbols.csv: ported inline in gmod_draw: node +0xc0 = [0x5e9e2c] (update_frame) after the static bake |
| 0x004ff7f0 | 13 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.h:89) |
| 0x004ff810 | 11 | folded | ported, by docs/symbols.csv: ported as zcls::segment_options().exclude ([0x0060b974]) |
| 0x004ff8d0 | 87 | unlabelled |  |
| 0x004ffb40 | 24 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.cpp:825) |
| 0x004ffb90 | 52 | unlabelled |  |
| 0x004ffc80 | 35 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.cpp:825) |
| 0x004ffd20 | 161 | unlabelled |  |
| 0x005000d0 | 21 | folded | read: a hit record pushed with PM's owner (+0x28: the node's +0x40, or with flag 0x200000 the first up its single-child chain to have one) and the count raised, from the ground and segment tests 0x004ffd20 / 0x00501ab0 (MW3's 0x004eddb0 / 0x004ef570 push without an owner); the port's hit pushes set it through zmod::hit_owner (gmod_hit.cpp:24; cls_di.cpp:646, 1038) |
| 0x00500140 | 59 | unlabelled |  |
| 0x00500270 | 54 | unlabelled |  |
| 0x005003a0 | 187 | unlabelled |  |
| 0x005007d0 | 24 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.cpp:958) |
| 0x00500830 | 48 | unlabelled |  |
| 0x00500900 | 40 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.cpp:958) |
| 0x005009e0 | 184 | unlabelled |  |
| 0x00501030 | 213 | unlabelled |  |
| 0x005017e0 | 27 | folded | read: a cell's clutter records tested by the segment query (each record 0x5c bytes through 0x00501840, or the record tree through 0x00501920 when +0x50 is set; stopping at the first hit when first-hit is on), from the cell walk 0x00501030 (MW3's 0x004eed90); the port's clutter_walk in the same queries (cls_di.cpp:96, used at 510, 827, 962; docs/pm-map.md) |
| 0x00501840 | 48 | unlabelled |  |
| 0x00501920 | 39 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.cpp:1290) |
| 0x00501a30 | 26 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.cpp:1290) |
| 0x00501ab0 | 160 | unlabelled |  |
| 0x00501e70 | 78 | unlabelled |  |
| 0x00502030 | 59 | unlabelled |  |
| 0x00502190 | 75 | unlabelled |  |
| 0x00502360 | 116 | unlabelled |  |
| 0x00502590 | 231 | unlabelled |  |
| 0x00502a90 | 27 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.cpp:506) |
| 0x00502b00 | 47 | unlabelled |  |
| 0x00502bd0 | 49 | unlabelled |  |
| 0x00502d20 | 199 | unlabelled |  |
| 0x005033b0 | 191 | unlabelled |  |
| 0x00503820 | 32 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.cpp:1441) |
| 0x005038c0 | 176 | unlabelled |  |
| 0x00503c20 | 14 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.cpp:825) |
| 0x00503ce0 | 36 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.cpp:958) |
| 0x00503ea0 | 26 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.cpp:1290) |
| 0x00504100 | 58 | unlabelled |  |
| 0x005043d0 | 30 | folded | covered by the port where it names it (src/pm/gamez/zclass/cls_clutter.cpp:355) |
| 0x005044d0 | 31 | folded | covered by the port where it names it (src/gamez/zgame/zgame_table.h:93) |
| 0x00504d60 | 31 | folded | the script's NewWorld (PM split it out of the dispatcher): MW3's world create 0x004f1bd0 (cls_world.cpp:402) with PM's world data 16 bytes longer, PM's second area (World::pm_area2), the same values; not run under a test (trace, 2026-11-13) |
| 0x00504e70 | 49 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004f1cd0, whose class PM's changes keep: under null script commands: WorldSetVirtualPartition's partition rebuild (0x004f1cd0) and the camera commands' setters (0x004f3d00, 0x004f3f70, from 0x00503c10 / 0x00503d50); no shipped script uses them |
| 0x00504f50 | 16 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004f1d90: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x00504f80 | 15 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004f1dc0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x00505230 | 18 | unlabelled |  |
| 0x00505300 | 85 | folded | read: covered where PM's port names it (cls_world.h:19, cls_clutter.h:60 and 120, cls_util.cpp:222-223 the copy refusals) or container code (list erase and copy, a vector clear, a thunk, a member setter) |
| 0x005054e0 | 18 | folded | covered by the port where it names it (src/gamez/zdeclient/zdec_crater.cpp:370) |
| 0x00505550 | 49 | unlabelled |  |
| 0x005056b0 | 9 | folded | covered by the port where it names it (src/gamez/zeffect/zeff_anim_events.h:45) |
| 0x005056d0 | 9 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:135) |
| 0x00505820 | 95 | unlabelled |  |
| 0x00505ab0 | 71 | unlabelled |  |
| 0x00505c40 | 17 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004f2880, whose class PM's changes keep: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x00505c80 | 58 | unlabelled |  |
| 0x00505ea0 | 80 | unlabelled |  |
| 0x00506000 | 82 | unlabelled |  |
| 0x00506160 | 28 | folded | covered by the port where it names it (src/pm/gamez/zeffect/zeff_anim_events.cpp:43) |
| 0x00506200 | 67 | unlabelled |  |
| 0x00506390 | 28 | folded | read: MW3's 0x004f2d50 (world_add_light: the light on the world's lists, the world on the light's; cls_world.cpp) with PM's offsets (world +0xbc/+0xc0/+0xc4, light +0x8c/+0x90), unpaired by the diff; from the light-state anim event (0x0051b850, the port's zeff_anim_events.cpp ev_light_state) and the anim save 0x00525d14 |
| 0x00506430 | 72 | unlabelled |  |
| 0x00506670 | 17 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004f32a0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x005066b0 | 34 | unlabelled |  |
| 0x00506760 | 23 | folded | group A, read: the same logic as the shared port; PM marks the node changed through its own helper chain (0x00506ba0, 0x004ff6d0 -> 0x004fb600) where MW3's camera_mark_changed 0x004f3760 calls 0x004fb600 itself (camera.cpp's ports) |
| 0x005067c0 | 22 | folded | read: MW3's node getters and setters with the null-node report, the same bodies with their arguments on the stack (0x004fd280 is MW3's 0x004eb710, the INTERSECT bit; 0x004fd4a0 is 0x004eb8a0, +0x2c bit 5; 0x00506a60 is 0x004f3630, a camera's window; 0x004fcfb0 is 0x004eb6a0, the node's zone byte +0x30, unpaired by the diff; 0x005067c0 is 0x004f33e0, the camera case of 0x004fd7a0's switch on the class (MW3's 0x004ebb40, cls_node.cpp), through 0x004fd8d0 = MW3's 0x004ebc30); their callers' ports read and write the same fields (zai_brain.h node_intersect, mech_parts set_flags2c, app_hud.cpp:196, zone_visible(raw[0x30])) |
| 0x00506810 | 9 | folded | a thunk: one call to another function |
| 0x00506830 | 36 | unlabelled |  |
| 0x005068d0 | 35 | unlabelled |  |
| 0x00506960 | 11 | unused: PORT (preserve) | as MW3 0x004f3540: no effect in the original (app_engine.cpp's list): a camera word nothing reads (0x004f3540), zClass's start defaults the port's initialisers already hold (0x004f8f20), a zSave registration with flag 0 (0x004f8f60) |
| 0x00506970 | 9 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004f3550: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x00506980 | 49 | unlabelled |  |
| 0x00506a60 | 23 | folded | read: MW3's node getters and setters with the null-node report, the same bodies with their arguments on the stack (0x004fd280 is MW3's 0x004eb710, the INTERSECT bit; 0x004fd4a0 is 0x004eb8a0, +0x2c bit 5; 0x00506a60 is 0x004f3630, a camera's window; 0x004fcfb0 is 0x004eb6a0, the node's zone byte +0x30, unpaired by the diff; 0x005067c0 is 0x004f33e0, the camera case of 0x004fd7a0's switch on the class (MW3's 0x004ebb40, cls_node.cpp), through 0x004fd8d0 = MW3's 0x004ebc30); their callers' ports read and write the same fields (zai_brain.h node_intersect, mech_parts set_flags2c, app_hud.cpp:196, zone_visible(raw[0x30])) |
| 0x00506ae0 | 41 | unlabelled |  |
| 0x00506c10 | 40 | folded | group A, read: the same logic as the shared port; PM marks the node changed through its own helper chain (0x00506ba0, 0x004ff6d0 -> 0x004fb600) where MW3's camera_mark_changed 0x004f3760 calls 0x004fb600 itself (camera.cpp's ports) |
| 0x00506cd0 | 44 | unlabelled |  |
| 0x00506d90 | 44 | folded | group A, read: the same logic as the shared port; PM marks the node changed through its own helper chain (0x00506ba0, 0x004ff6d0 -> 0x004fb600) where MW3's camera_mark_changed 0x004f3760 calls 0x004fb600 itself (camera.cpp's ports) |
| 0x00506e50 | 40 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004f39d0, whose class PM's changes keep: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x00506ef0 | 33 | unlabelled |  |
| 0x00506f80 | 32 | unlabelled |  |
| 0x00507010 | 56 | unlabelled |  |
| 0x00507150 | 32 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004f3d00, whose class PM's changes keep: under null script commands: WorldSetVirtualPartition's partition rebuild (0x004f1cd0) and the camera commands' setters (0x004f3d00, 0x004f3f70, from 0x00503c10 / 0x00503d50); no shipped script uses them |
| 0x005071e0 | 49 | unlabelled |  |
| 0x005072e0 | 32 | unlabelled |  |
| 0x00507370 | 29 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004f3f70, whose class PM's changes keep: under null script commands: WorldSetVirtualPartition's partition rebuild (0x004f1cd0) and the camera commands' setters (0x004f3d00, 0x004f3f70, from 0x00503c10 / 0x00503d50); no shipped script uses them |
| 0x005073f0 | 34 | unlabelled |  |
| 0x00507480 | 29 | unlabelled |  |
| 0x00507500 | 29 | unlabelled |  |
| 0x00507580 | 24 | unlabelled |  |
| 0x005079a0 | 88 | folded | group A, read: the shared port's logic; the sphere's frustum test MW3 calls directly (0x00541760) PM calls through [0x00611354], whose only writers are its five variants (0x00546a00, 0x00546d50, 0x005470a0, 0x005473f0, 0x00547740): the same plane tests in another order, each culling plane moved to the front; the same codes and clip mask bits. Only the code returned for a sphere outside several planes can differ, and the callers test 0 and 0x20 alone, so at most a flag-0x80 node outside a side plane and the 0x20 plane is drawn though off screen |
| 0x00507b70 | 94 | folded | group A, read: the shared port's logic; the sphere's frustum test MW3 calls directly (0x00541760) PM calls through [0x00611354], whose only writers are its five variants (0x00546a00, 0x00546d50, 0x005470a0, 0x005473f0, 0x00547740): the same plane tests in another order, each culling plane moved to the front; the same codes and clip mask bits. Only the code returned for a sphere outside several planes can differ, and the callers test 0 and 0x20 alone, so at most a flag-0x80 node outside a side plane and the 0x20 plane is drawn though off screen |
| 0x00507d60 | 88 | folded | group A, read: the shared port's logic; the sphere's frustum test MW3 calls directly (0x00541760) PM calls through [0x00611354], whose only writers are its five variants (0x00546a00, 0x00546d50, 0x005470a0, 0x005473f0, 0x00547740): the same plane tests in another order, each culling plane moved to the front; the same codes and clip mask bits. Only the code returned for a sphere outside several planes can differ, and the callers test 0 and 0x20 alone, so at most a flag-0x80 node outside a side plane and the 0x20 plane is drawn though off screen |
| 0x00507f30 | 269 | MISSING LOGIC | group A, read: PM's model draw (pm only): the per-node light override (node +0x2c, the cockpit grills; see 0x004fd0d0) and PM's fog mode test; the shared draw (camera.cpp:79) has neither |
| 0x005084d0 | 96 | folded | group A, read: the shared port's logic; the sphere's frustum test MW3 calls directly (0x00541760) PM calls through [0x00611354], whose only writers are its five variants (0x00546a00, 0x00546d50, 0x005470a0, 0x005473f0, 0x00547740): the same plane tests in another order, each culling plane moved to the front; the same codes and clip mask bits. Only the code returned for a sphere outside several planes can differ, and the callers test 0 and 0x20 alone, so at most a flag-0x80 node outside a side plane and the 0x20 plane is drawn though off screen |
| 0x00508680 | 266 | folded | group A, read: the shared port's logic; the sphere's frustum test MW3 calls directly (0x00541760) PM calls through [0x00611354], whose only writers are its five variants (0x00546a00, 0x00546d50, 0x005470a0, 0x005473f0, 0x00547740): the same plane tests in another order, each culling plane moved to the front; the same codes and clip mask bits. Only the code returned for a sphere outside several planes can differ, and the callers test 0 and 0x20 alone, so at most a flag-0x80 node outside a side plane and the 0x20 plane is drawn though off screen |
| 0x00508cb0 | 58 | folded | group A, read: the shared port's logic; the sphere's frustum test MW3 calls directly (0x00541760) PM calls through [0x00611354], whose only writers are its five variants (0x00546a00, 0x00546d50, 0x005470a0, 0x005473f0, 0x00547740): the same plane tests in another order, each culling plane moved to the front; the same codes and clip mask bits. Only the code returned for a sphere outside several planes can differ, and the callers test 0 and 0x20 alone, so at most a flag-0x80 node outside a side plane and the 0x20 plane is drawn though off screen |
| 0x00508de0 | 68 | folded | group A, read: the shared port's logic; the sphere's frustum test MW3 calls directly (0x00541760) PM calls through [0x00611354], whose only writers are its five variants (0x00546a00, 0x00546d50, 0x005470a0, 0x005473f0, 0x00547740): the same plane tests in another order, each culling plane moved to the front; the same codes and clip mask bits. Only the code returned for a sphere outside several planes can differ, and the callers test 0 and 0x20 alone, so at most a flag-0x80 node outside a side plane and the 0x20 plane is drawn though off screen |
| 0x00509030 | 32 | folded | covered by the port where it names it (src/gamez/zclass/camera.cpp:498) |
| 0x005090b0 | 94 | unlabelled |  |
| 0x005092a0 | 274 | folded | group A, read: the shared port's logic; the sphere's frustum test MW3 calls directly (0x00541760) PM calls through [0x00611354], whose only writers are its five variants (0x00546a00, 0x00546d50, 0x005470a0, 0x005473f0, 0x00547740): the same plane tests in another order, each culling plane moved to the front; the same codes and clip mask bits. Only the code returned for a sphere outside several planes can differ, and the callers test 0 and 0x20 alone, so at most a flag-0x80 node outside a side plane and the 0x20 plane is drawn though off screen |
| 0x00509830 | 286 | folded | group A, read: the shared port's logic; the sphere's frustum test MW3 calls directly (0x00541760) PM calls through [0x00611354], whose only writers are its five variants (0x00546a00, 0x00546d50, 0x005470a0, 0x005473f0, 0x00547740): the same plane tests in another order, each culling plane moved to the front; the same codes and clip mask bits. Only the code returned for a sphere outside several planes can differ, and the callers test 0 and 0x20 alone, so at most a flag-0x80 node outside a side plane and the 0x20 plane is drawn though off screen |
| 0x00509dd0 | 187 | MISSING LOGIC | group A, read: PM's world draw (pm only): its fog calls are PM's (0x00504450, 0x0050a1e0, and 0x005404f0, the sticky fog mode) where MW3's are 0x00526a60 / 0x00526b80; so the per-frame fog on and off (camera.cpp:1087) cannot clear PM's under-water mode 2: part of the under-water fog row (0x005404f0) |
| 0x0050a1e0 | 24 | folded | covered by the port where it names it (src/gamez/zgame/zgame_table.h:86) |
| 0x0050a550 | 32 | MISSING LOGIC | pm only, render: PM's scene render (0x0050a5d0, MW3's 0x004f6c50 render_scene) draws a world's direct child only when its +0x24 bit 0x80 is set or its cell bounds (+0x4c..+0x52) overlap the walk's visible cells ([0x00639020..0x0063902c]); MW3's 0x004f6c00 and the port's render_world_children (camera.cpp:1149, shared, with PM's other cell differences in the game table) draw every one. Effect: world-level objects past the view's cells, which PM leaves out, are drawn in the port (the far plane and fog still apply) |
| 0x0050a5f0 | 9 | debug or dev tool: PORT (preserve) | as MW3 0x004f6c70: the world's writers (gamez.zbd saved back: the node pools, 0x004fe6c0 -> 0x004fe2d0 -> 0x004fe210; gamez.md) and two debug-only helpers (mechlib's type name for its debug log 0x004fce40; the frame counter for the developer lines 0x004f6c70) |
| 0x0050a600 | 49 | unlabelled |  |
| 0x0050a6c0 | 28 | folded | group A, read: the shared port's logic; the sphere's frustum test MW3 calls directly (0x00541760) PM calls through [0x00611354], whose only writers are its five variants (0x00546a00, 0x00546d50, 0x005470a0, 0x005473f0, 0x00547740): the same plane tests in another order, each culling plane moved to the front; the same codes and clip mask bits. Only the code returned for a sphere outside several planes can differ, and the callers test 0 and 0x20 alone, so at most a flag-0x80 node outside a side plane and the 0x20 plane is drawn though off screen |
| 0x0050a750 | 102 | software renderer | as MW3 0x004f6dc0, whose class PM's changes keep: the camera's software scene walk (0x004f6dc0 without a device; hud.md: the software path) |
| 0x0050a9e0 | 136 | unlabelled |  |
| 0x0050ad10 | 33 | unlabelled |  |
| 0x0050b010 | 15 | folded | as MW3 0x004f7630: covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x0050b030 | 15 | folded | as MW3 0x004f7650: covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x0050b140 | 36 | unlabelled |  |
| 0x0050b1c0 | 26 | folded | read: MW3's 0x004f77d0 (the list nodes' free pool: one taken off it, or a new one), its new one from calloc(1, 0x10) where MW3 grows a block table; from the list adds 0x0050b9b0 / 0x0050b9e0 / 0x0050ba90 (MW3's 0x004f8050...; list.cpp); the port's lists allocate their own |
| 0x0050b8b0 | 19 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004f7fa0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x0050b8f0 | 20 | folded | read: a scan of list 6 for its last node with no children (+0x54 == 0) whose answer is dropped (0x0050b931), run at each find by name (0x0050b920, MW3's 0x004f7fe0) while [0x0060db84] is 1 (never written): no effect |
| 0x0050b920 | 47 | unlabelled |  |
| 0x0050bb20 | 20 | folded | as MW3 0x004f81e0: covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x0050bce0 | 52 | unlabelled |  |
| 0x0050bdf0 | 280 | unlabelled |  |
| 0x0050c3b0 | 79 | folded | read: teardown frees at the list system's end (each list's nodes and the free pools released, their counts cleared; from 0x0050c340, MW3's 0x004f8ab0, and 0x0050caf0, MW3's 0x004f8fb0); the port's objects free themselves |
| 0x0050c5d0 | 15 | unlabelled |  |
| 0x0050c660 | 24 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004f8b80: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x0050c6c0 | 9 | folded | a thunk: one call to another function |
| 0x0050c730 | 9 | folded | a thunk: one call to another function |
| 0x0050c790 | 56 | unlabelled |  |
| 0x0050ca50 | 10 | unused: PORT (preserve) | as MW3 0x004f8f20: no effect in the original (app_engine.cpp's list): a camera word nothing reads (0x004f3540), zClass's start defaults the port's initialisers already hold (0x004f8f20), a zSave registration with flag 0 (0x004f8f60) |
| 0x0050ca70 | 9 | folded | as MW3 0x004f8f40: covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x0050ca90 | 9 | unused: PORT (preserve) | as MW3 0x004f8f60: no effect in the original (app_engine.cpp's list): a camera word nothing reads (0x004f3540), zClass's start defaults the port's initialisers already hold (0x004f8f20), a zSave registration with flag 0 (0x004f8f60) |
| 0x0050caa0 | 14 | folded | as MW3 0x004f8f70: covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x0050cb90 | 26 | unlabelled |  |
| 0x0050cc10 | 23 | unused: PORT (preserve) | the script command CreateUniqueMaterials (PM only, args recursive / polygon): each node's model through 0x0055bcf0, its children too when recursive; no shipped PM script uses it (interp.zbd decoded) and the port's interpreter does not know it |
| 0x0050cdc0 | 128 | unlabelled |  |
| 0x0050d1f0 | 67 | unlabelled |  |
| 0x0050d340 | 10 | folded | read: covered where PM's port names it (cls_world.h:19, cls_clutter.h:60 and 120, cls_util.cpp:222-223 the copy refusals) or container code (list erase and copy, a vector clear, a thunk, a member setter) |
| 0x0050d360 | 10 | folded | read: covered where PM's port names it (cls_world.h:19, cls_clutter.h:60 and 120, cls_util.cpp:222-223 the copy refusals) or container code (list erase and copy, a vector clear, a thunk, a member setter) |
| 0x0050d380 | 65 | unlabelled |  |
| 0x0050d4c0 | 10 | folded | covered by the port where it names it (src/gamez/zclass/cls_util.cpp:221) |
| 0x0050d4e0 | 66 | unlabelled |  |
| 0x0050d650 | 10 | folded | covered by the port where it names it (src/gamez/zeffect/zeff_init.h:6) |
| 0x0050d670 | 10 | folded | covered by the port where it names it (src/gamez/zclass/cls_util.cpp:224) |
| 0x0050d690 | 52 | unlabelled |  |
| 0x0050d7c0 | 24 | unlabelled |  |
| 0x0050db40 | 20 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004f9e50: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x0050db90 | 37 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:565) |
| 0x0050dbbb | 30 | folded | follows 0x0050db90: covered by the port where it names it (src/gamez/zclass/cls_node.cpp:565) |
| 0x0050dc60 | 22 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004fd1e0, whose class PM's changes keep: zClass's add-child for an animate (0x004fd8c0) or switch (0x004fd1e0) parent: the port's node_add_child (cls_node.cpp:186) handles worlds, links and object3d and aborts ('not ported yet') for cameras, LoDs, animate and switch parents, though the camera's own add-child is ported (camera.cpp, 0x004f33a0). No shipped data adds such a child at run time (every mission plays); a mod's scripts or data would abort the game |
| 0x0050ded0 | 29 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004fa1c0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x0050df50 | 35 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004fa230: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x0050dff0 | 58 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004fa2c0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x0050e110 | 41 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004fa3e0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x0050e1f0 | 31 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004fdc70, whose class PM's changes keep: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x0050e270 | 22 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:565) |
| 0x0050e5f0 | 38 | unlabelled |  |
| 0x0050e6a0 | 22 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:566) |
| 0x0050eef0 | 27 | unlabelled |  |
| 0x0050f270 | 84 | unlabelled |  |
| 0x0050f3f0 | 28 | unlabelled |  |
| 0x0050f870 | 57 | unlabelled |  |
| 0x0050f960 | 22 | unlabelled |  |
| 0x0050f9c0 | 28 | unlabelled |  |
| 0x0050fa20 | 28 | unlabelled |  |
| 0x0050fa80 | 35 | unlabelled |  |
| 0x0050fb10 | 69 | unlabelled |  |
| 0x0050fc70 | 29 | unlabelled |  |
| 0x0050fcf0 | 29 | unlabelled |  |
| 0x0050fd70 | 29 | unlabelled |  |
| 0x0050fdf0 | 35 | unlabelled |  |
| 0x0050fe80 | 53 | unlabelled |  |
| 0x0050ff80 | 28 | unlabelled |  |
| 0x0050ffe0 | 52 | unlabelled |  |
| 0x005100f0 | 28 | unlabelled |  |
| 0x00510150 | 58 | unlabelled |  |
| 0x00510270 | 33 | unlabelled |  |
| 0x00510300 | 49 | unlabelled |  |
| 0x00510400 | 60 | unlabelled |  |
| 0x00510540 | 54 | unlabelled |  |
| 0x00510630 | 23 | unlabelled |  |
| 0x005106b0 | 12 | folded | covered by the port where it names it (src/gamez/zclass/cls_mechlib.cpp:127) |
| 0x00510710 | 17 | unused: PORT (preserve) | as MW3 0x004fc4d0: mechlib's material writer at close (0x004fc990 -> 0x004fcca0 -> 0x004fc4d0): dead (mechlib.md) |
| 0x00510830 | 34 | folded | covered by the port where it names it (src/gamez/zclass/cls_mechlib.h:38) |
| 0x00510ab0 | 23 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00510b00 | 23 | folded | covered by the port where it names it (src/pm/gamez/zclass/cls_mechlib.h:20) |
| 0x00510b50 | 16 | unlabelled |  |
| 0x00510c40 | 13 | unused: PORT (preserve) | as MW3 0x004fc990: mechlib's material writer at close (0x004fc990 -> 0x004fcca0 -> 0x004fc4d0): dead (mechlib.md) |
| 0x00510f40 | 33 | unused: PORT (preserve) | as MW3 0x004fcca0: mechlib's material writer at close (0x004fc990 -> 0x004fcca0 -> 0x004fc4d0): dead (mechlib.md) |
| 0x00511180 | 35 | debug or dev tool: PORT (preserve) | the node class names for PM's mechlib trace lines (0x00511640 to stderr, 'read node: %s type %s'), its only use; the port leaves the trace lines out (pm/gamez/zclass/cls_mechlib.cpp:15) |
| 0x00511640 | 27 | folded | covered by the port where it names it (src/pm/gamez/zclass/cls_mechlib.cpp:15) |
| 0x005116a0 | 9 | folded | covered by the port where it names it (src/pm/gamez/zclass/cls_clutter.h:117) |
| 0x005116e0 | 9 | folded | follows 0x0040d510: as MW3 0x0042a570: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x005116f0 | 9 | folded | a thunk: one call to another function |
| 0x00511710 | 50 | unlabelled |  |
| 0x005117f0 | 9 | folded | a thunk: one call to another function |
| 0x00511800 | 9 | folded | a thunk: one call to another function |
| 0x00511820 | 63 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00511940 | 29 | unlabelled |  |
| 0x005119a0 | 46 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00511a60 | 24 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00512c00 | 25 | folded | covered by the port where it names it (src/pm/gamez/zclass/cls_clutter.cpp:432) |
| 0x00512d90 | 61 | unlabelled |  |
| 0x00512eb0 | 9 | unlabelled | follows 0x005a4b90:  |
| 0x00512ec0 | 9 | folded | a thunk: one call to another function |
| 0x00512ee0 | 55 | unlabelled |  |
| 0x00513030 | 123 | folded | read: covered where PM's port names it (cls_world.h:19, cls_clutter.h:60 and 120, cls_util.cpp:222-223 the copy refusals) or container code (list erase and copy, a vector clear, a thunk, a member setter) |
| 0x005139d0 | 27 | folded | covered by the port where it names it (src/pm/gamez/zclass/cls_clutter.h:71) |
| 0x00513a30 | 9 | folded | inside its ported callers (0x00492830): a small helper their ports do their own way |
| 0x00513a40 | 50 | unlabelled |  |
| 0x00513b40 | 15 | unlabelled |  |
| 0x00513b80 | 55 | unlabelled |  |
| 0x00513d90 | 65 | folded | read: covered where PM's port names it (cls_world.h:19, cls_clutter.h:60 and 120, cls_util.cpp:222-223 the copy refusals) or container code (list erase and copy, a vector clear, a thunk, a member setter) |
| 0x00513ed0 | 58 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00514000 | 58 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00514120 | 16 | folded | read: covered where PM's port names it (cls_world.h:19, cls_clutter.h:60 and 120, cls_util.cpp:222-223 the copy refusals) or container code (list erase and copy, a vector clear, a thunk, a member setter) |
| 0x00514170 | 19 | folded | read: covered where PM's port names it (cls_world.h:19, cls_clutter.h:60 and 120, cls_util.cpp:222-223 the copy refusals) or container code (list erase and copy, a vector clear, a thunk, a member setter) |
| 0x005141c0 | 318 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x005146f0 | 31 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00514780 | 197 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00514a90 | 47 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00514b40 | 30 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00514b90 | 51 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00514c50 | 28 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:567) |
| 0x00514cb0 | 28 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:565) |
| 0x00514d10 | 39 | unlabelled |  |
| 0x00514ef0 | 22 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:567) |
| 0x00514f40 | 47 | unlabelled |  |
| 0x00514fe0 | 42 | unlabelled |  |
| 0x00515080 | 29 | unlabelled |  |
| 0x005150e0 | 28 | unlabelled |  |
| 0x00515140 | 56 | unlabelled |  |
| 0x00515380 | 22 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004fd8c0: zClass's add-child for an animate (0x004fd8c0) or switch (0x004fd1e0) parent: the port's node_add_child (cls_node.cpp:186) handles worlds, links and object3d and aborts ('not ported yet') for cameras, LoDs, animate and switch parents, though the camera's own add-child is ported (camera.cpp, 0x004f33a0). No shipped data adds such a child at run time (every mission plays); a mod's scripts or data would abort the game |
| 0x005153d0 | 28 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:566) |
| 0x00515730 | 22 | unlabelled |  |
| 0x00515790 | 51 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004fdcd0, whose class PM's changes keep: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x00515910 | 24 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004fde50: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x00515960 | 24 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004fde90: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x005159b0 | 24 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004fded0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x00515a00 | 24 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004fdf10: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x00515ba0 | 9 | folded | read: covered where PM's port names it (cls_world.h:19, cls_clutter.h:60 and 120, cls_util.cpp:222-223 the copy refusals) or container code (list erase and copy, a vector clear, a thunk, a member setter) |
| 0x00515bb0 | 9 | folded | inside its ported callers (0x004fd5e0, 0x0050d4e0, 0x005b2d10): a small helper their ports do their own way |
| 0x00515bd0 | 9 | folded | MW3's 0x004fe0b0 (0x004ebc30, then 0), unpaired by the diff: the same wrapper, in the class switches of the node remove 0x004fd7a0 and the list delete 0x0050bdf0 (their MW3 twins call 0x004fe0b0 at the same case); not run under a test (trace) |
| 0x00515c00 | 98 | MISSING LOGIC (only data the shipped game lacks reaches it) | read: PM's LoD fade values (+0x34, +0x38, +0x3c from the ranges +8, +0x10..+0x18, the switches +0x1c..+0x24 and the floors +0x28..+0x30, +0x50 one for all), computed by the LoD copy 0x0050d4e0 (MW3's 0x004f9840, cls_util.cpp copy_lod); the port leaves them zero, which is what every shipped tree computes (docs/pm-map.md, the node copies' Left): only a mod's LoD with those fields set would differ |
| 0x00515e20 | 11 | folded | covered by the port where it names it (src/pm/gamez/zeffect/zeff_anim_init.cpp:353) |
| 0x00515e40 | 25 | folded | read: teardown frees at the list system's end (each list's nodes and the free pools released, their counts cleared; from 0x0050c340, MW3's 0x004f8ab0, and 0x0050caf0, MW3's 0x004f8fb0); the port's objects free themselves |
| 0x00515fb0 | 125 | debug or dev tool: PORT (preserve) | as MW3 0x004fe6c0, whose class PM's changes keep: the world's writers (gamez.zbd saved back: the node pools, 0x004fe6c0 -> 0x004fe2d0 -> 0x004fe210; gamez.md) and two debug-only helpers (mechlib's type name for its debug log 0x004fce40; the frame counter for the developer lines 0x004f6c70) |
| 0x00516260 | 189 | debug or dev tool: PORT (preserve) | as MW3 0x004fe2d0, whose class PM's changes keep: the world's writers (gamez.zbd saved back: the node pools, 0x004fe6c0 -> 0x004fe2d0 -> 0x004fe210; gamez.md) and two debug-only helpers (mechlib's type name for its debug log 0x004fce40; the frame counter for the developer lines 0x004f6c70) |
| 0x00516720 | 48 | debug or dev tool: PORT (preserve) | as MW3 0x004fe210, whose class PM's changes keep: the world's writers (gamez.zbd saved back: the node pools, 0x004fe6c0 -> 0x004fe2d0 -> 0x004fe210; gamez.md) and two debug-only helpers (mechlib's type name for its debug log 0x004fce40; the frame counter for the developer lines 0x004f6c70) |
| 0x00516800 | 53 | debug or dev tool: PORT (preserve) | called only from 0x00516260, which are debug |
| 0x005168e0 | 113 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x004fe8e0, whose class PM's changes keep: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x00517960 | 30 | folded | covered by the port where it names it (src/pm/gamez/zclass/cls_zbd.cpp:104) |
| 0x00517b30 | 47 | MISSING LOGIC | pm only, BROKEN row: PM's node re-read in PM's node format (MW3 0x004ff7a0), the shared port's MW3 format only; see 0x00517ac0 |

### zmodel (278 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00540450 | 16 | unlabelled |  |
| 0x005404f0 | 15 | MISSING LOGIC | PM's fog under water (pm only): PM's set_underwater 0x00408d00 sets the fog mode [0x00a4e038] to 2 through 0x005404f0, which keeps 2 against anything but 0, so both model draws fog every model, not only flagged ones (gmod_draw.cpp:268 reads the mode); back above water it sets 1. The port's set_underwater is MW3's (player_view.cpp:820), zmod::fog_set_on has no sticky 2, and nothing sets 2, so under water only flagged models are fogged |
| 0x00540740 | 9 | folded | MW3's fog_apply 0x00526de0 (gmod_fog.cpp:56), unpaired: the world update's fog step; it ran under test_pm_world_box's world update 0x00504fb0, which compares only the pending cells, so not counted as checked |
| 0x00540750 | 14 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x00540790 | 14 | folded | covered by the port where it names it (src/gamez/ztwod/twod_host.h:133) |
| 0x00540870 | 10 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.h:329) |
| 0x00540a90 | 101 | unused: PORT (preserve) | as MW3 0x0053d130: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x00540ca0 | 124 | unused: PORT (preserve) | as MW3 0x0053d360: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x00540ea0 | 54 | software renderer | as MW3 0x0053d560, whose class PM's changes keep: the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x00541020 | 134 | software renderer | as MW3 0x0053d750, whose class PM's changes keep: the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x005415b0 | 52 | unlabelled |  |
| 0x00541830 | 44 | unused: PORT (preserve) | as MW3 0x0053de50: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x005418e0 | 65 | unused: PORT (preserve) | as MW3 0x0053df00: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x00541a30 | 9 | MISSING LOGIC | pm only, low object detail: PM's object-detail setter (0x0040b120, MW3 0x0040d310, the port's set_object_lod, options.cpp:249) also sets [0x006112e4] = 1 below detail 2, 0 at 2 (0x00541a30), and PM's model preparation (0x00541a40, MW3 0x0053e050) reads it: at detail 2 it does not advance the materials' texture cycles (animated textures stay on their frame) and returns before its case-2 path's drawing. The port has neither; with the other low-detail rows (0x00492830's, the camera scale 0.2, the mech data's) |
| 0x00541a40 | 622 | software renderer | as MW3 0x0053e050, whose class PM's changes keep: the software model draw (0x0053e050, gmod_init.h: [0x0074a2e0] in software), its lighting (0x005465c0 with 0x00544060) and its third fog routine (0x005557f0, render-fogsort.md) |
| 0x00543010 | 61 | unlabelled |  |
| 0x005432b0 | 217 | unlabelled |  |
| 0x00546d50 | 85 | unlabelled |  |
| 0x005470a0 | 85 | unlabelled |  |
| 0x005473f0 | 85 | unlabelled |  |
| 0x00547740 | 85 | unlabelled |  |
| 0x00548220 | 243 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00542060, whose class PM's changes keep: the scripts' texture marks (0x00542060: a hit painted into the hit texture's image) only when a script gives a mark image; none ships (weapon-effects.md); the port logs 0x00542060 if a mod's script turns them on |
| 0x005488d0 | 63 | folded | as MW3 0x004c3520: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x00548990 | 64 | unlabelled |  |
| 0x00548e00 | 11 | unlabelled |  |
| 0x00548f50 | 69 | software renderer | called only from 0x00541a40, which are software |
| 0x005490e0 | 72 | software renderer | called only from 0x00541a40, which are software |
| 0x00549290 | 72 | software renderer | called only from 0x00541a40, which are software |
| 0x00549f30 | 105 | software renderer | called only from 0x00541a40, which are software |
| 0x0054a3b0 | 9 | software renderer | called only from 0x00541a40, which are software |
| 0x0054a3d0 | 9 | software renderer | called only from 0x00541a40, which are software |
| 0x0054a3f0 | 9 | software renderer | called only from 0x00541a40, which are software |
| 0x0054ae80 | 9 | folded | a thunk: one call to another function |
| 0x0054aea0 | 779 | unused: PORT (preserve) | as MW3 0x00530d90, whose class PM's changes keep: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0054be20 | 945 | unused: PORT (preserve) | as MW3 0x00531d90, whose class PM's changes keep: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0054d1c0 | 1153 | unused: PORT (preserve) | as MW3 0x005331e0, whose class PM's changes keep: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0054fe70 | 12 | software renderer | as MW3 0x00535ed0: the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x0054fe90 | 10 | software renderer | as MW3 0x00535ef0: the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x005502d0 | 1161 | software renderer | called only from 0x0054a3b0, 0x0054a3d0, 0x0054a3f0, which are software |
| 0x00555930 | 8 | folded | returns a constant or a global |
| 0x00555940 | 8 | folded | returns a constant or a global |
| 0x00555a10 | 25 | folded | covered by the port where it names it (src/mech3/mech_net.cpp:968) |
| 0x00555ae0 | 137 | debug or dev tool: PORT (preserve) | as MW3 0x005360c0, whose class PM's changes keep: the world's writers (gamez.md: 0x004fe8e0's materials 0x005360c0 and models 0x005380e0, saving a world back to a .zbd): a dev tool |
| 0x00556420 | 28 | unlabelled |  |
| 0x005564c0 | 8 | folded | empty: does nothing |
| 0x00556550 | 159 | unlabelled |  |
| 0x00556d30 | 57 | folded | as MW3 0x005372c0: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00556f80 | 33 | replaced (SDL or the platform layer) | as MW3 0x00537520: the used textures' system surfaces released after the launch (DirectDraw; the port's record is one Render3D handle: app_engine.cpp no effect) |
| 0x00557040 | 39 | unlabelled |  |
| 0x00557120 | 43 | unlabelled |  |
| 0x00557200 | 17 | unlabelled |  |
| 0x00557240 | 16 | unlabelled |  |
| 0x00557290 | 137 | unlabelled |  |
| 0x005576e0 | 109 | folded | group A, read: the same logic on PM's layout and helpers (the reader's lookup 0x00578630 for 0x00556740, PM's list helpers, the flare's fade with PM's globals, the gravity command, the objective's distance through PM's HUD getter); the call counts differ by how often a getter is called |
| 0x00557af0 | 10 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_const.cpp:484) |
| 0x00557b00 | 11 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_const.cpp:485) |
| 0x00557b20 | 11 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.h:331) |
| 0x00557b30 | 20 | debug or dev tool: PORT (preserve) | called only from 0x00515fb0, which are debug |
| 0x00557be0 | 51 | folded | read: container and list code (vector insert and copy, list add and teardown, the puffers' vectors) and small helpers inside their ported callers: a sticky mode setter (2 kept), the pool's growth (gmod_const.cpp:191, PM's port), the puffers' set (gmod_puff.cpp:170 as MW3's), the reader's get-and-read wrappers |
| 0x00557d50 | 302 | debug or dev tool: PORT (preserve) | as MW3 0x005380e0, whose class PM's changes keep: the world's writers (gamez.md: 0x004fe8e0's materials 0x005360c0 and models 0x005380e0, saving a world back to a .zbd): a dev tool |
| 0x00558440 | 187 | unlabelled |  |
| 0x005590e0 | 8 | folded | returns a constant or a global |
| 0x00559910 | 9 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00539cc0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x00559940 | 11 | folded | as MW3 0x00539cf0: covered: zModel's reset of its materials (module-starts.md: the port's initialisers), small helpers and list initialisers beside ported code |
| 0x005599a0 | 9 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:509) |
| 0x005599b0 | 9 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:509) |
| 0x005599c0 | 8 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:535) |
| 0x00559af0 | 80 | unlabelled |  |
| 0x00559db0 | 97 | unused: PORT (preserve) | as MW3 0x0053a1a0, whose class PM's changes keep: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0055a2f0 | 72 | unused: PORT (preserve) | as MW3 0x0053a740: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0055ac70 | 67 | unlabelled |  |
| 0x0055ad60 | 168 | unused: PORT (preserve) | as MW3 0x0053b050, whose class PM's changes keep: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0055b630 | 113 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x0053b960, whose class PM's changes keep: script engine commands for building models and textures at run time (Object3DSetColor, SetShowBackFace, SetMorphVertex, the texture world base and per meter): the engine entries are null in the port (hook_allow: counted) and no shipped script uses them (the census: 0 skipped) |
| 0x0055b810 | 21 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x0053bb50: script engine commands for building models and textures at run time (Object3DSetColor, SetShowBackFace, SetMorphVertex, the texture world base and per meter): the engine entries are null in the port (hook_allow: counted) and no shipped script uses them (the census: 0 skipped) |
| 0x0055b850 | 39 | unlabelled |  |
| 0x0055b8f0 | 27 | unlabelled |  |
| 0x0055bc30 | 46 | unlabelled |  |
| 0x0055bcf0 | 58 | unlabelled |  |
| 0x0055be00 | 26 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x0053bd60: script engine commands for building models and textures at run time (Object3DSetColor, SetShowBackFace, SetMorphVertex, the texture world base and per meter): the engine entries are null in the port (hook_allow: counted) and no shipped script uses them (the census: 0 skipped) |
| 0x0055d2d0 | 168 | unlabelled |  |
| 0x0055d740 | 21 | unlabelled |  |
| 0x0055da40 | 190 | unlabelled |  |
| 0x0055e720 | 85 | unlabelled |  |
| 0x0055e940 | 151 | unlabelled |  |
| 0x0055ece0 | 631 | software renderer | as MW3 0x00544060, whose class PM's changes keep: the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x00561540 | 434 | software renderer | as MW3 0x005465c0, whose class PM's changes keep: the software model draw (0x0053e050, gmod_init.h: [0x0074a2e0] in software), its lighting (0x005465c0 with 0x00544060) and its third fog routine (0x005557f0, render-fogsort.md) |
| 0x005625e0 | 23 | folded | covered by the port where it names it (src/pm/gamez/zmodel/gmod_draw.cpp:719) |
| 0x00563740 | 11 | folded | inside its ported callers (0x00507f30): a small helper their ports do their own way |
| 0x00563760 | 157 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00548250, whose class PM's changes keep: the script command MakeShadows (0x005487d0) and the baked shadow builder under it (0x00548250..0x00548650): the script engine's make_shadows entry is null in the port (hook_allow: counted) and no shipped script uses it |
| 0x005639e0 | 62 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x005484c0: the script command MakeShadows (0x005487d0) and the baked shadow builder under it (0x00548250..0x00548650): the script engine's make_shadows entry is null in the port (hook_allow: counted) and no shipped script uses it |
| 0x00563b30 | 23 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00548600: the script command MakeShadows (0x005487d0) and the baked shadow builder under it (0x00548250..0x00548650): the script engine's make_shadows entry is null in the port (hook_allow: counted) and no shipped script uses it |
| 0x00563b80 | 84 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00548650: the script command MakeShadows (0x005487d0) and the baked shadow builder under it (0x00548250..0x00548650): the script engine's make_shadows entry is null in the port (hook_allow: counted) and no shipped script uses it |
| 0x00563d20 | 23 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x005487d0: the script command MakeShadows (0x005487d0) and the baked shadow builder under it (0x00548250..0x00548650): the script engine's make_shadows entry is null in the port (hook_allow: counted) and no shipped script uses it |
| 0x00563da0 | 50 | unused: PORT (preserve) | as MW3 0x00548840, whose class PM's changes keep: the puffers' wind step with a dt argument: dead code, never called (puffers.md) |
| 0x00563f10 | 214 | unlabelled |  |
| 0x00564460 | 17 | folded | as MW3 0x00548d90: covered: zModel's reset of its materials (module-starts.md: the port's initialisers), small helpers and list initialisers beside ported code |
| 0x00564510 | 17 | folded | as MW3 0x00548e60: covered: zModel's reset of its materials (module-starts.md: the port's initialisers), small helpers and list initialisers beside ported code |
| 0x00564710 | 118 | unlabelled |  |
| 0x00564a10 | 34 | unlabelled |  |
| 0x00564ac0 | 31 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_puff.cpp:184) |
| 0x00564c00 | 56 | unlabelled |  |
| 0x00565320 | 346 | unlabelled |  |
| 0x00565f00 | 9 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x00565f30 | 11 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_puff.h:139) |
| 0x00565f50 | 11 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_puff.h:140) |
| 0x00565f70 | 11 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_puff.h:141) |
| 0x00565f90 | 11 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_puff.h:142) |
| 0x00565fb0 | 11 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_puff.cpp:103) |
| 0x00565fd0 | 9 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x00566020 | 9 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x00566030 | 9 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x00566040 | 9 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x00566050 | 9 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x00566060 | 9 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x00566070 | 9 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x00566080 | 9 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x005660e0 | 20 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_puff.cpp:127) |
| 0x00566130 | 9 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x00566140 | 9 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x00566160 | 9 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_puff.h:156) |
| 0x005661b0 | 74 | unlabelled |  |
| 0x00566380 | 74 | folded | read: container and list code (vector insert and copy, list add and teardown, the puffers' vectors) and small helpers inside their ported callers: a sticky mode setter (2 kept), the pool's growth (gmod_const.cpp:191, PM's port), the puffers' set (gmod_puff.cpp:170 as MW3's), the reader's get-and-read wrappers |
| 0x00566540 | 27 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x005665b0 | 34 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x00566640 | 23 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x005666b0 | 27 | folded | inside its ported callers (0x0051bbf0): a small helper their ports do their own way |
| 0x00566740 | 9 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x00566750 | 11 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x00566760 | 11 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x00566770 | 28 | folded | as MW3 0x0054b680: covered: zModel's reset of its materials (module-starts.md: the port's initialisers), small helpers and list initialisers beside ported code |
| 0x005667e0 | 50 | folded | read: container and list code (vector insert and copy, list add and teardown, the puffers' vectors) and small helpers inside their ported callers: a sticky mode setter (2 kept), the pool's growth (gmod_const.cpp:191, PM's port), the puffers' set (gmod_puff.cpp:170 as MW3's), the reader's get-and-read wrappers |
| 0x005668d0 | 16 | folded | read: container and list code (vector insert and copy, list add and teardown, the puffers' vectors) and small helpers inside their ported callers: a sticky mode setter (2 kept), the pool's growth (gmod_const.cpp:191, PM's port), the puffers' set (gmod_puff.cpp:170 as MW3's), the reader's get-and-read wrappers |
| 0x00566910 | 40 | folded | inside its ported callers (0x00565320): a small helper their ports do their own way |
| 0x00566990 | 35 | folded | inside its ported callers (0x00565320): a small helper their ports do their own way |
| 0x00566a00 | 11 | folded | read: container and list code (vector insert and copy, list add and teardown, the puffers' vectors) and small helpers inside their ported callers: a sticky mode setter (2 kept), the pool's growth (gmod_const.cpp:191, PM's port), the puffers' set (gmod_puff.cpp:170 as MW3's), the reader's get-and-read wrappers |
| 0x00566a20 | 19 | folded | as MW3 0x004dd6a0: the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x00566a60 | 180 | folded | read: container and list code (vector insert and copy, list add and teardown, the puffers' vectors) and small helpers inside their ported callers: a sticky mode setter (2 kept), the pool's growth (gmod_const.cpp:191, PM's port), the puffers' set (gmod_puff.cpp:170 as MW3's), the reader's get-and-read wrappers |
| 0x00567290 | 145 | unlabelled |  |
| 0x00569290 | 368 | unlabelled |  |
| 0x00569b00 | 420 | unlabelled |  |
| 0x0056b230 | 15 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x0056b2d0 | 121 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x0056b540 | 21 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x0056b580 | 155 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x0056b980 | 801 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x0056d040 | 106 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x0056d260 | 1317 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005705a0 | 214 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00570b70 | 722 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00572ae0 | 8 | folded | returns a constant or a global |
| 0x00572af0 | 9 | MISSING LOGIC | as MW3 0x00553460: the hardware mode's triangle stripper (0x004fc8b0 -> 0x00552540 over every mechlib model, flag set by zvid_init 0x00579e79 when [0x0072f348]; about 45 functions, 3,500 lines, much of it STL): joins triangles that share an edge with matching material, priority, normals, UVs and colours into strips (0x400, up to 64 vertices). Measured (strip_hits.py): 302 of 770 models change, and on 126 the ground and segment tests stop at a different face (up to 1.5 m away), so the port answers as software mode, not hardware mode. A port, queued after the audit (docs/mechlib.md) |
| 0x00572b40 | 49 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00572c20 | 69 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00572df0 | 174 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005730e0 | 11 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573100 | 22 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573150 | 11 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573170 | 10 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005731a0 | 12 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005731c0 | 12 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005731e0 | 12 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573210 | 8 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573230 | 8 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573250 | 35 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005732c0 | 40 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573340 | 90 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573550 | 12 | unlabelled |  |
| 0x00573580 | 13 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005735a0 | 58 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573700 | 18 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573760 | 24 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005737d0 | 95 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573aa0 | 39 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573b70 | 27 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573bc0 | 11 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573bf0 | 110 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573f30 | 10 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00573f50 | 351 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574560 | 38 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574620 | 34 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005746c0 | 10 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005746e0 | 63 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574870 | 46 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574900 | 9 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574910 | 25 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574950 | 57 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574a40 | 24 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00574ab0 | 25 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574b00 | 25 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574b40 | 56 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574c30 | 24 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00574ca0 | 25 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574cf0 | 110 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574e90 | 20 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574ee0 | 16 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574f10 | 29 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574f60 | 16 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574f90 | 9 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574fa0 | 29 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00574ff0 | 44 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005750b0 | 44 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00575170 | 16 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005751c0 | 8 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005751d0 | 10 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005751f0 | 81 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00575390 | 10 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005753b0 | 75 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00575510 | 318 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00575a40 | 58 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00575b60 | 19 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00575bc0 | 58 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00575cf0 | 318 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576220 | 58 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576340 | 16 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576390 | 10 | MISSING LOGIC | as MW3 0x00554f30: the hardware mode's triangle stripper (0x004fc8b0 -> 0x00552540 over every mechlib model, flag set by zvid_init 0x00579e79 when [0x0072f348]; about 45 functions, 3,500 lines, much of it STL): joins triangles that share an edge with matching material, priority, normals, UVs and colours into strips (0x400, up to 64 vertices). Measured (strip_hits.py): 302 of 770 models change, and on 126 the ground and segment tests stop at a different face (up to 1.5 m away), so the port answers as software mode, not hardware mode. A port, queued after the audit (docs/mechlib.md) |
| 0x005763b0 | 31 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576440 | 200 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576750 | 47 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576800 | 31 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576890 | 195 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576ba0 | 47 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576c50 | 14 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576c80 | 14 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576cb0 | 31 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576d10 | 51 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576dd0 | 30 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576e20 | 51 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576ee0 | 11 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576f00 | 12 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576f20 | 11 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576f50 | 11 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x00576f70 | 43 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005771a0 | 123 | software renderer | as MW3 0x005557f0, whose class PM's changes keep: the software model draw (0x0053e050, gmod_init.h: [0x0074a2e0] in software), its lighting (0x005465c0 with 0x00544060) and its third fog routine (0x005557f0, render-fogsort.md) |
| 0x00577690 | 17 | folded | as MW3 0x00555cd0: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x005776c0 | 13 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x005776e0 | 30 | folded | as MW3 0x00555d20: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577750 | 19 | folded | as MW3 0x00555d90: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x005777a0 | 33 | folded | as MW3 0x00555de0: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577810 | 16 | folded | as MW3 0x00555e50: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577830 | 17 | folded | as MW3 0x00555e70: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577870 | 12 | folded | as MW3 0x00555ea0: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577890 | 28 | folded | read: container and list code (vector insert and copy, list add and teardown, the puffers' vectors) and small helpers inside their ported callers: a sticky mode setter (2 kept), the pool's growth (gmod_const.cpp:191, PM's port), the puffers' set (gmod_puff.cpp:170 as MW3's), the reader's get-and-read wrappers |
| 0x005778e0 | 12 | folded | as MW3 0x00555f10: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577900 | 27 | folded | covered by the port where it names it (src/gamez/zreader/zreader.cpp:595) |
| 0x00577950 | 33 | folded | inside its ported callers (0x0058fb60): a small helper their ports do their own way |
| 0x005779d0 | 25 | folded | as MW3 0x00556000: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577a00 | 35 | folded | as MW3 0x00555f70: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577a80 | 15 | folded | as MW3 0x00555fe0: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577aa0 | 29 | folded | as MW3 0x00556070: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577b00 | 23 | folded | as MW3 0x005560d0: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577b50 | 25 | folded | as MW3 0x00556120: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577b90 | 9 | folded | follows 0x00577b00: as MW3 0x005560d0: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577bb0 | 11 | folded | as MW3 0x00556160: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577bc0 | 13 | unlabelled |  |
| 0x00577bf0 | 34 | folded | read: container and list code (vector insert and copy, list add and teardown, the puffers' vectors) and small helpers inside their ported callers: a sticky mode setter (2 kept), the pool's growth (gmod_const.cpp:191, PM's port), the puffers' set (gmod_puff.cpp:170 as MW3's), the reader's get-and-read wrappers |
| 0x00577c70 | 15 | unlabelled |  |
| 0x00577cc0 | 9 | folded | a thunk: one call to another function |
| 0x00577ce0 | 36 | unlabelled |  |
| 0x00577d80 | 12 | unlabelled |  |
| 0x00577da0 | 19 | folded | as MW3 0x005561a0: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00577df0 | 36 | folded | read: container and list code (vector insert and copy, list add and teardown, the puffers' vectors) and small helpers inside their ported callers: a sticky mode setter (2 kept), the pool's growth (gmod_const.cpp:191, PM's port), the puffers' set (gmod_puff.cpp:170 as MW3's), the reader's get-and-read wrappers |
| 0x00577e50 | 38 | unlabelled |  |
| 0x00577f00 | 39 | unlabelled |  |
| 0x00577f80 | 35 | unlabelled |  |
| 0x00578000 | 24 | unlabelled |  |
| 0x00578040 | 12 | unlabelled |  |
| 0x00578070 | 9 | folded | RdrSetPath / RdrAddPath (PM split them out of the dispatcher, one setter 0x00577da0 with a clear flag in place of MW3's two): as MW3 0x005561a0 / 0x005561e0, folded there (the reader reads through the mounts); not run under a test (trace) |
| 0x00578090 | 9 | folded | RdrSetPath / RdrAddPath (PM split them out of the dispatcher, one setter 0x00577da0 with a clear flag in place of MW3's two): as MW3 0x005561a0 / 0x005561e0, folded there (the reader reads through the mounts); not run under a test (trace) |
| 0x005780b0 | 13 | unlabelled |  |
| 0x005780e0 | 36 | folded | covered by the port where it names it (src/pm/gamez/zreader/zrdr_main.cpp:151) |
| 0x005782a0 | 19 | folded | covered by the port where it names it (src/pm/gamez/zreader/zrdr_main.cpp:49) |
| 0x00578630 | 76 | unlabelled |  |
| 0x00578950 | 9 | folded | a thunk: one call to another function |
| 0x005790d0 | 12 | folded | read: container and list code (vector insert and copy, list add and teardown, the puffers' vectors) and small helpers inside their ported callers: a sticky mode setter (2 kept), the pool's growth (gmod_const.cpp:191, PM's port), the puffers' set (gmod_puff.cpp:170 as MW3's), the reader's get-and-read wrappers |
| 0x005790f0 | 12 | folded | read: container and list code (vector insert and copy, list add and teardown, the puffers' vectors) and small helpers inside their ported callers: a sticky mode setter (2 kept), the pool's growth (gmod_const.cpp:191, PM's port), the puffers' set (gmod_puff.cpp:170 as MW3's), the reader's get-and-read wrappers |
| 0x00579110 | 12 | folded | read: container and list code (vector insert and copy, list add and teardown, the puffers' vectors) and small helpers inside their ported callers: a sticky mode setter (2 kept), the pool's growth (gmod_const.cpp:191, PM's port), the puffers' set (gmod_puff.cpp:170 as MW3's), the reader's get-and-read wrappers |
| 0x00579130 | 12 | folded | checked by test (block trace, 2026-11-13): test_pm_zreader calls it directly on every key of every shipped reader entry; the value read compared |
| 0x00579150 | 12 | folded | checked by test (block trace, 2026-11-13): test_pm_zreader calls it directly on every key of every shipped reader entry; the value read compared |

### znetwork (169 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005a91a0 | 24 | folded | covered by the port where it names it (src/gamez/znetwork/znet_dplay.h:10) |
| 0x005a9220 | 41 | unlabelled |  |
| 0x005a9480 | 29 | unlabelled |  |
| 0x005a94f0 | 20 | folded | inside its ported callers (0x0049a3c0, 0x0049df10): a small helper their ports do their own way |
| 0x005a9520 | 16 | unlabelled |  |
| 0x005a9560 | 21 | folded | covered by the port where it names it (src/mech3/mp_connect.cpp:351) |
| 0x005a95c0 | 19 | unlabelled |  |
| 0x005a9620 | 76 | unlabelled |  |
| 0x005a97d0 | 26 | unlabelled |  |
| 0x005a9850 | 51 | unlabelled |  |
| 0x005a9980 | 272 | unlabelled |  |
| 0x005aa110 | 52 | unlabelled |  |
| 0x005aa2f0 | 20 | folded | covered by the port where it names it (src/gamez/znetwork/znet_dplay.h:10) |
| 0x005aa360 | 14 | replaced (SDL or the platform layer) | as MW3 0x005895b0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005aa380 | 18 | unlabelled |  |
| 0x005aa3b0 | 43 | replaced (SDL or the platform layer) | as MW3 0x00589600: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005aa410 | 43 | UNSURE | read with group A: a callee of shared ports PM changed (0x0049faf0, 0x0049fed0); whether the port does PM's version, this call included, is settled there |
| 0x005aa470 | 12 | unlabelled |  |
| 0x005aa4e0 | 14 | UNSURE | read with group A: a callee of shared ports PM changed (0x0046f910, 0x005aa470); whether the port does PM's version, this call included, is settled there |
| 0x005aa5d0 | 15 | folded | covered by the port where it names it (src/gamez/znetwork/znet_dplay.cpp:336) |
| 0x005aa600 | 8 | folded | returns a constant or a global |
| 0x005aa660 | 18 | folded | the network's team list walk (each team's id to the callback), for the lobby's team lines (PM 0x0049bb70; pilot_games.cpp:1673 walks the port's teams); not run under a test (trace) |
| 0x005aa6a0 | 31 | folded | inside its ported callers (0x005a9980): a small helper their ports do their own way |
| 0x005aa700 | 12 | folded | inside its ported callers (0x005a9980): a small helper their ports do their own way |
| 0x005aa730 | 30 | replaced (SDL or the platform layer) | as MW3 0x005899b0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005aa7d0 | 87 | replaced (SDL or the platform layer) | as MW3 0x00589a50: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005aa980 | 11 | replaced (SDL or the platform layer) | as MW3 0x00589c00: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005aa9a0 | 15 | replaced (SDL or the platform layer) | as MW3 0x00589c20: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005aa9d0 | 20 | replaced (SDL or the platform layer) | as MW3 0x00589c50: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005aaa20 | 15 | replaced (SDL or the platform layer) | as MW3 0x00589ca0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005aaa60 | 12 | replaced (SDL or the platform layer) | as MW3 0x00589ce0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005aaa80 | 55 | replaced (SDL or the platform layer) | as MW3 0x00589d00: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005aab60 | 16 | replaced (SDL or the platform layer) | as MW3 0x00589df0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005aaba0 | 25 | folded | traced: MW3's 0x00589e30 with the same body (the network's held object released after its last use, through its vt+8), called from the same place in MW3's lobby scores 0x00498550 as from PM's 0x0049b730; the port's network layer keeps no such object |
| 0x005aac00 | 9 | folded | follows 0x0040d510: as MW3 0x0042a570: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x005aac10 | 9 | folded | a thunk: one call to another function |
| 0x005aac40 | 9 | folded | follows 0x0040d510: as MW3 0x0042a570: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x005aac50 | 9 | folded | a thunk: one call to another function |
| 0x005aac70 | 14 | replaced (SDL or the platform layer) | as MW3 0x00586de0: DirectPlay's session description and player list (multiplayer-port.md:59-60): the ENet transport stands in |
| 0x005aac90 | 8 | folded | returns a constant or a global |
| 0x005aad50 | 17 | folded | covered by the port where it names it (src/gamez/znetwork/znet_session.cpp:64) |
| 0x005aadd0 | 16 | replaced (SDL or the platform layer) | as MW3 0x00586f30: DirectPlay's session description and player list (multiplayer-port.md:59-60): the ENet transport stands in |
| 0x005aaf60 | 31 | replaced (SDL or the platform layer) | as MW3 0x005870c0: DirectPlay's session description and player list (multiplayer-port.md:59-60): the ENet transport stands in |
| 0x005aafe0 | 67 | folded | as MW3 0x00587140: the game list's search, ours in the ENet transport (net_enet.cpp:441) |
| 0x005ab110 | 19 | folded | covered by the port where it names it (src/gamez/znetwork/znet_session.cpp:59) |
| 0x005ab150 | 29 | replaced (SDL or the platform layer) | as MW3 0x00587300: DirectPlay's session description and player list (multiplayer-port.md:59-60): the ENet transport stands in |
| 0x005ab1d0 | 30 | folded | covered by the port where it names it (src/gamez/znetwork/znet_session.cpp:96) |
| 0x005ab250 | 32 | replaced (SDL or the platform layer) | as MW3 0x005873f0: DirectPlay's session description and player list (multiplayer-port.md:59-60): the ENet transport stands in |
| 0x005ab2b0 | 18 | replaced (SDL or the platform layer) | as MW3 0x00587450: DirectPlay's session description and player list (multiplayer-port.md:59-60): the ENet transport stands in |
| 0x005ab2e0 | 37 | folded | covered by the port where it names it (src/gamez/znetwork/znet_session.cpp:96) |
| 0x005ab3a0 | 83 | replaced (SDL or the platform layer) | as MW3 0x00587580: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ab4a0 | 55 | replaced (SDL or the platform layer) | as MW3 0x00587680: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ab510 | 22 | folded | covered by the port where it names it (src/gamez/znetwork/znet_session.cpp:79) |
| 0x005ab560 | 15 | folded | covered by the port where it names it (src/gamez/znetwork/znet_dplay.cpp:157) |
| 0x005ab580 | 11 | replaced (SDL or the platform layer) | as MW3 0x00587760: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ab5a0 | 71 | replaced (SDL or the platform layer) | as MW3 0x00587780, whose class PM's changes keep: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ab760 | 14 | folded | as MW3 0x00475c20: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x005ab790 | 14 | replaced (SDL or the platform layer) | as MW3 0x00589e80: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ab7b0 | 12 | unlabelled |  |
| 0x005ab7d0 | 14 | unlabelled |  |
| 0x005ab800 | 8 | replaced (SDL or the platform layer) | called only from 0x005abe90, which are sdl |
| 0x005ab810 | 23 | replaced (SDL or the platform layer) | as MW3 0x00589f00: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ab860 | 15 | replaced (SDL or the platform layer) | as MW3 0x00589f50: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ab880 | 46 | replaced (SDL or the platform layer) | as MW3 0x00589f70: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ab920 | 32 | replaced (SDL or the platform layer) | as MW3 0x0058a020: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ab990 | 17 | replaced (SDL or the platform layer) | as MW3 0x0058a090: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ab9d0 | 10 | replaced (SDL or the platform layer) | as MW3 0x0058a0d0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ab9e0 | 13 | replaced (SDL or the platform layer) | as MW3 0x0058a0e0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005aba10 | 62 | replaced (SDL or the platform layer) | as MW3 0x0058a110: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005abb40 | 13 | replaced (SDL or the platform layer) | as MW3 0x0058a240: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005abb70 | 19 | replaced (SDL or the platform layer) | as MW3 0x0058a270: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005abbb0 | 12 | unlabelled |  |
| 0x005abbd0 | 14 | replaced (SDL or the platform layer) | as MW3 0x0058a2d0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005abc00 | 28 | replaced (SDL or the platform layer) | as MW3 0x0058a340: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005abc90 | 12 | replaced (SDL or the platform layer) | as MW3 0x0058a3d0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005abcd0 | 26 | unlabelled |  |
| 0x005abd30 | 10 | replaced (SDL or the platform layer) | as MW3 0x0058a450: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005abd50 | 12 | unlabelled |  |
| 0x005abd70 | 24 | replaced (SDL or the platform layer) | as MW3 0x0058a4a0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005abde0 | 12 | unlabelled |  |
| 0x005abe00 | 18 | replaced (SDL or the platform layer) | as MW3 0x0058a530: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005abe50 | 26 | replaced (SDL or the platform layer) | as MW3 0x0058a5b0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005abe90 | 35 | replaced (SDL or the platform layer) | as MW3 0x0058a5f0, whose class PM's changes keep: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005abfa0 | 35 | replaced (SDL or the platform layer) | as MW3 0x0058a6f0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ac060 | 15 | replaced (SDL or the platform layer) | as MW3 0x0058a7b0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ac080 | 8 | folded | returns a constant or a global |
| 0x005ac0a0 | 19 | replaced (SDL or the platform layer) | as MW3 0x0058a7f0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ac0e0 | 19 | replaced (SDL or the platform layer) | as MW3 0x0058a830: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005ac120 | 8 | folded | empty: does nothing |
| 0x005ac130 | 15 | folded | covered by the port where it names it (src/gamez/znetwork/znet_dplay.cpp:272) |
| 0x005ac170 | 280 | UNSURE | read with group A: a callee of shared ports PM changed (0x005a9220, 0x005a9620, 0x005a9980); whether the port does PM's version, this call included, is settled there |
| 0x005ac8f0 | 19 | folded | as MW3 0x0058ae90: covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x005ac920 | 67 | unlabelled |  |
| 0x005aca50 | 12 | unlabelled |  |
| 0x005acb00 | 44 | folded | ported, by docs/symbols.csv: ported (the dock's net_element): the first element of that name (vt+0x24) |
| 0x005acba0 | 21 | UNSURE | read with group A: a callee of shared ports PM changed (0x004a0e10, 0x004a2dc0, 0x004dd1e0); whether the port does PM's version, this call included, is settled there |
| 0x005acc40 | 19 | unused: PORT (preserve) | called only from 0x00410880, which are unused |
| 0x005acc70 | 25 | folded | as MW3 0x0058b220: covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x005acd30 | 29 | folded | covered by the port where it names it (src/gamez/znetwork/znet_object.cpp:15) |
| 0x005acd90 | 16 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:13) |
| 0x005aced0 | 12 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:528) |
| 0x005acef0 | 11 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:507) |
| 0x005acf10 | 126 | unlabelled |  |
| 0x005ad3d0 | 194 | debug or dev tool: PORT (preserve) | group A, read: the network debug view (as MW3's 0x0058ba40, PM's helpers) |
| 0x005ad8e0 | 209 | unlabelled |  |
| 0x005adf20 | 12 | unlabelled |  |
| 0x005adf40 | 34 | debug or dev tool: PORT (preserve) | as MW3 0x0058c590, whose class PM's changes keep: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005adfc0 | 9 | unlabelled |  |
| 0x005adfd0 | 10 | unlabelled |  |
| 0x005adfe0 | 77 | unlabelled |  |
| 0x005ae380 | 23 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:53) |
| 0x005ae730 | 65 | folded | as MW3 0x0058ce10: covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x005aec40 | 25 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:112) |
| 0x005aecb0 | 89 | debug or dev tool: PORT (preserve) | as MW3 0x0058d3c0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005af220 | 19 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:31) |
| 0x005af2f0 | 144 | unlabelled |  |
| 0x005af690 | 87 | folded | as MW3 0x0058de40: covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x005af810 | 144 | unlabelled |  |
| 0x005afbb0 | 28 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:58) |
| 0x005afc20 | 12 | unlabelled |  |
| 0x005afc40 | 8 | folded | as MW3 0x004d7250: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x005afc50 | 14 | debug or dev tool: PORT (preserve) | as MW3 0x0058e440: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005afc70 | 21 | unlabelled |  |
| 0x005afca0 | 8 | folded | returns a constant or a global |
| 0x005afee0 | 18 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:141) |
| 0x005aff20 | 45 | folded | as MW3 0x0058e7b0: covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x005affa0 | 35 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:159) |
| 0x005b0020 | 18 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:180) |
| 0x005b0060 | 34 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:261) |
| 0x005b00d0 | 30 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:267) |
| 0x005b0140 | 9 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:253) |
| 0x005b0150 | 13 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:257) |
| 0x005b0180 | 10 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:173) |
| 0x005b01a0 | 9 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:167) |
| 0x005b01b0 | 9 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:175) |
| 0x005b0300 | 26 | debug or dev tool: PORT (preserve) | as MW3 0x0058ec20: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0360 | 19 | debug or dev tool: PORT (preserve) | as MW3 0x0058ec90: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b03a0 | 11 | debug or dev tool: PORT (preserve) | as MW3 0x0058ecd0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b03d0 | 19 | debug or dev tool: PORT (preserve) | as MW3 0x0058ed00: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0410 | 28 | debug or dev tool: PORT (preserve) | as MW3 0x0058ed40: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0470 | 24 | debug or dev tool: PORT (preserve) | as MW3 0x0058eda0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b04d0 | 20 | debug or dev tool: PORT (preserve) | as MW3 0x0058ee10: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0500 | 138 | debug or dev tool: PORT (preserve) | as MW3 0x0058ee40, whose class PM's changes keep: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b08b0 | 60 | debug or dev tool: PORT (preserve) | as MW3 0x0058f170: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b09c0 | 11 | debug or dev tool: PORT (preserve) | as MW3 0x0058f270: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b09f0 | 16 | UNSURE | read with group A: a callee of shared ports PM changed (0x00592910, 0x005a9220, 0x005a9980); whether the port does PM's version, this call included, is settled there |
| 0x005b0a30 | 77 | debug or dev tool: PORT (preserve) | as MW3 0x0058f420: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0b90 | 37 | debug or dev tool: PORT (preserve) | as MW3 0x0058f580: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0c60 | 78 | debug or dev tool: PORT (preserve) | as MW3 0x0058f2e0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0da0 | 12 | folded | inside its ported callers (0x005adfe0): a small helper their ports do their own way |
| 0x005b0dc0 | 33 | debug or dev tool: PORT (preserve) | as MW3 0x0058f630: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0e30 | 10 | debug or dev tool: PORT (preserve) | as MW3 0x0058f6a0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0ed0 | 11 | debug or dev tool: PORT (preserve) | as MW3 0x0058f740: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0ef0 | 11 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:50) |
| 0x005b0f00 | 11 | debug or dev tool: PORT (preserve) | as MW3 0x0058f770: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0f10 | 11 | debug or dev tool: PORT (preserve) | as MW3 0x0058f780: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0f20 | 11 | debug or dev tool: PORT (preserve) | as MW3 0x0058f790: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0f30 | 11 | debug or dev tool: PORT (preserve) | as MW3 0x0058f7a0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0f40 | 11 | debug or dev tool: PORT (preserve) | as MW3 0x0058f7b0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b0f50 | 11 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:50) |
| 0x005b1040 | 8 | debug or dev tool: PORT (preserve) | as MW3 0x0058f8b0: the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x005b1050 | 57 | UNSURE | read with group A: a callee of shared ports PM changed (0x00482140); whether the port does PM's version, this call included, is settled there |
| 0x005b1180 | 12 | unlabelled |  |
| 0x005b11a0 | 18 | folded | as MW3 0x0058fa40: covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x005b1200 | 12 | folded | covered by the port where it names it (src/gamez/zgame/zgameapp.cpp:59) |
| 0x005b1220 | 72 | folded | as MW3 0x0058fac0: covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x005b13d0 | 8 | folded | covered by the port where it names it (src/gamez/zgame/zgameapp.h:96) |
| 0x005b1440 | 38 | folded | covered by the port where it names it (src/gamez/zgame/zgameapp.h:59) |
| 0x005b14e0 | 25 | UNSURE | read with group A: a callee of shared ports PM changed (0x005c9280); whether the port does PM's version, this call included, is settled there |

### ztwod (145 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005bf790 | 25 | unlabelled |  |
| 0x005bf850 | 51 | unlabelled |  |
| 0x005bfa00 | 13 | folded | read: covered where PM's port names it (cls_world.h:19, cls_clutter.h:60 and 120, cls_util.cpp:222-223 the copy refusals) or container code (list erase and copy, a vector clear, a thunk, a member setter) |
| 0x005bfa40 | 19 | folded | as MW3 0x00429ce0: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x005bfa80 | 12 | unlabelled |  |
| 0x005bfab0 | 11 | folded | covered by the port where it names it (src/gamez/ztwod/twod_control.cpp:21) |
| 0x005bfd50 | 15 | unlabelled |  |
| 0x005bfdc0 | 9 | folded | read: covered where PM's port names it (cls_world.h:19, cls_clutter.h:60 and 120, cls_util.cpp:222-223 the copy refusals) or container code (list erase and copy, a vector clear, a thunk, a member setter) |
| 0x005c02c0 | 19 | debug or dev tool: PORT (preserve) | as MW3 0x005a0050: the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683) |
| 0x005c0320 | 11 | replaced (SDL or the platform layer) | PM's 2D drawing through Direct3D (0x005c0320: [0x008c4b70] and [0x008d3aa9] set; its lines, rects, blits, scene begin and end through the device's function table [0x008c4bd8..]; else the software 2D path): the SDL 2D host draws the 2D layer |
| 0x005c0340 | 12 | unlabelled |  |
| 0x005c0360 | 55 | replaced (SDL or the platform layer) | PM's 2D drawing through Direct3D (0x005c0320: [0x008c4b70] and [0x008d3aa9] set; its lines, rects, blits, scene begin and end through the device's function table [0x008c4bd8..]; else the software 2D path): the SDL 2D host draws the 2D layer |
| 0x005c0450 | 29 | replaced (SDL or the platform layer) | PM's 2D drawing through Direct3D (0x005c0320: [0x008c4b70] and [0x008d3aa9] set; its lines, rects, blits, scene begin and end through the device's function table [0x008c4bd8..]; else the software 2D path): the SDL 2D host draws the 2D layer |
| 0x005c04d0 | 16 | replaced (SDL or the platform layer) | PM's 2D drawing through Direct3D (0x005c0320: [0x008c4b70] and [0x008d3aa9] set; its lines, rects, blits, scene begin and end through the device's function table [0x008c4bd8..]; else the software 2D path): the SDL 2D host draws the 2D layer |
| 0x005c0500 | 11 | folded | inside its ported callers (0x005c1360): a small helper their ports do their own way |
| 0x005c08f0 | 158 | replaced (SDL or the platform layer) | PM's 2D drawing through Direct3D (0x005c0320: [0x008c4b70] and [0x008d3aa9] set; its lines, rects, blits, scene begin and end through the device's function table [0x008c4bd8..]; else the software 2D path): the SDL 2D host draws the 2D layer |
| 0x005c0b70 | 25 | replaced (SDL or the platform layer) | PM's 2D drawing through Direct3D (0x005c0320: [0x008c4b70] and [0x008d3aa9] set; its lines, rects, blits, scene begin and end through the device's function table [0x008c4bd8..]; else the software 2D path): the SDL 2D host draws the 2D layer |
| 0x005c0bc0 | 16 | replaced (SDL or the platform layer) | PM's 2D drawing through Direct3D (0x005c0320: [0x008c4b70] and [0x008d3aa9] set; its lines, rects, blits, scene begin and end through the device's function table [0x008c4bd8..]; else the software 2D path): the SDL 2D host draws the 2D layer |
| 0x005c0bf0 | 40 | replaced (SDL or the platform layer) | called only from 0x005c04d0, which are sdl |
| 0x005c0db0 | 12 | unlabelled |  |
| 0x005c1230 | 9 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:319) |
| 0x005c1d90 | 40 | unlabelled |  |
| 0x005c2280 | 32 | folded | covered by the port where it names it (src/mech3/debriefing.cpp:490) |
| 0x005c2320 | 34 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:2204) |
| 0x005c2640 | 69 | unlabelled |  |
| 0x005c2830 | 48 | unlabelled |  |
| 0x005c28f0 | 48 | unlabelled |  |
| 0x005c2a20 | 9 | folded | as MW3 0x005a1cd0: covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005c2a40 | 13 | folded | as MW3 0x005a1cf0: covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005c2bd0 | 48 | unlabelled |  |
| 0x005c2ce0 | 45 | unlabelled |  |
| 0x005c2df0 | 27 | unlabelled |  |
| 0x005c2ea0 | 24 | unlabelled |  |
| 0x005c2f20 | 31 | unlabelled |  |
| 0x005c3000 | 9 | folded | a thunk: one call to another function |
| 0x005c3060 | 70 | unlabelled |  |
| 0x005c3210 | 13 | unlabelled |  |
| 0x005c3250 | 58 | unlabelled |  |
| 0x005c3330 | 14 | unlabelled |  |
| 0x005c3370 | 40 | unlabelled |  |
| 0x005c3430 | 123 | unlabelled |  |
| 0x005c3bf0 | 26 | unlabelled |  |
| 0x005c3c70 | 21 | unlabelled |  |
| 0x005c3ce0 | 89 | unlabelled |  |
| 0x005c4030 | 93 | replaced (SDL or the platform layer) | PM's 2D drawing through Direct3D (0x005c0320: [0x008c4b70] and [0x008d3aa9] set; its lines, rects, blits, scene begin and end through the device's function table [0x008c4bd8..]; else the software 2D path): the SDL 2D host draws the 2D layer |
| 0x005c4290 | 35 | folded | covered by the port where it names it (src/mech3/briefing.cpp:228) |
| 0x005c4720 | 18 | unlabelled |  |
| 0x005c4760 | 225 | replaced (SDL or the platform layer) | PM's 2D drawing through Direct3D (0x005c0320: [0x008c4b70] and [0x008d3aa9] set; its lines, rects, blits, scene begin and end through the device's function table [0x008c4bd8..]; else the software 2D path): the SDL 2D host draws the 2D layer |
| 0x005c4cb0 | 56 | unlabelled |  |
| 0x005c4e70 | 8 | folded | returns a constant or a global |
| 0x005c4e80 | 9 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:588) |
| 0x005c4e90 | 18 | unlabelled |  |
| 0x005c4ed0 | 231 | unlabelled |  |
| 0x005c5460 | 49 | unlabelled |  |
| 0x005c5590 | 127 | folded | as MW3 0x0054bb70: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x005c57d0 | 17 | folded | read: covered where PM's port names it (cls_world.h:19, cls_clutter.h:60 and 120, cls_util.cpp:222-223 the copy refusals) or container code (list erase and copy, a vector clear, a thunk, a member setter) |
| 0x005c5870 | 10 | unlabelled |  |
| 0x005c5890 | 11 | folded | inside its ported callers (0x004879a0): a small helper their ports do their own way |
| 0x005c58b0 | 15 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x005a5400: anim event 37, the fbfx screen wave (0x005076c0 -> 0x005a6760 -> 0x005a6650 / 0x005a5400: up to four 2D waves): the port's hook only logs (app_mission.cpp:2108); no shipped anim.zbd has event 37 (anim_walk: c1..t1 have 0; 38 also 0) |
| 0x005c58f0 | 23 | unlabelled |  |
| 0x005c5970 | 118 | unlabelled |  |
| 0x005c5b40 | 12 | unlabelled |  |
| 0x005c5c10 | 36 | unlabelled |  |
| 0x005c6050 | 12 | unlabelled |  |
| 0x005c6080 | 205 | unlabelled |  |
| 0x005c6620 | 12 | unlabelled |  |
| 0x005c6640 | 10 | folded | as MW3 0x005a6100: covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005c6650 | 194 | unlabelled |  |
| 0x005c6b40 | 10 | folded | covered by the port where it names it (src/gamez/ztwod/twod_console.cpp:77) |
| 0x005c6b60 | 12 | folded | ported, by docs/symbols.csv: ported: the tint's +0x38 colour, +0x40 alpha, shown for 0 s (vt+0x60) |
| 0x005c6b90 | 18 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x005a6650: anim event 37, the fbfx screen wave (0x005076c0 -> 0x005a6760 -> 0x005a6650 / 0x005a5400: up to four 2D waves): the port's hook only logs (app_mission.cpp:2108); no shipped anim.zbd has event 37 (anim_walk: c1..t1 have 0; 38 also 0) |
| 0x005c6bf0 | 11 | folded | covered by the port where it names it (src/gamez/ztwod/twod_console.cpp:66) |
| 0x005c6c10 | 14 | folded | covered by the port where it names it (src/gamez/ztwod/twod_console.cpp:75) |
| 0x005c6c40 | 9 | folded | a thunk: one call to another function |
| 0x005c6c50 | 9 | folded | a thunk: one call to another function |
| 0x005c6c90 | 10 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x005a6760: anim event 37, the fbfx screen wave (0x005076c0 -> 0x005a6760 -> 0x005a6650 / 0x005a5400: up to four 2D waves): the port's hook only logs (app_mission.cpp:2108); no shipped anim.zbd has event 37 (anim_walk: c1..t1 have 0; 38 also 0) |
| 0x005c6df0 | 9 | debug or dev tool: PORT (preserve) | follows 0x005c6e80: as MW3 0x005a3cd0, whose class PM's changes keep: the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683) |
| 0x005c6e00 | 9 | folded | a thunk: one call to another function |
| 0x005c6e20 | 20 | unlabelled |  |
| 0x005c6e80 | 45 | debug or dev tool: PORT (preserve) | as MW3 0x005a3cd0, whose class PM's changes keep: the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683) |
| 0x005c6f90 | 16 | debug or dev tool: PORT (preserve) | as MW3 0x005a3de0, whose class PM's changes keep: the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683) |
| 0x005c6fe0 | 17 | debug or dev tool: PORT (preserve) | as MW3 0x005a3e30, whose class PM's changes keep: the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683) |
| 0x005c7030 | 68 | debug or dev tool: PORT (preserve) | as MW3 0x005a3e80, whose class PM's changes keep: the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683) |
| 0x005c7180 | 9 | folded | a thunk: one call to another function |
| 0x005c71b0 | 9 | folded | a thunk: one call to another function |
| 0x005c71c0 | 9 | folded | a thunk: one call to another function |
| 0x005c7200 | 54 | unlabelled |  |
| 0x005c7360 | 12 | folded | covered by the port where it names it (src/gamez/ztwod/twod_console.h:43) |
| 0x005c7380 | 38 | folded | group A, read: the same logic: a callee renamed by the map (0x0051da90 = PM 0x00537420), a direct call made virtual (0x0041bd80 through vt+0x168), PM's layout (+0xe8 for +0xe4, a null check added), a destructor on PM's members |
| 0x005c76d0 | 11 | folded | covered by the port where it names it (src/gamez/ztwod/twod_console.cpp:29) |
| 0x005c76f0 | 12 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:514) |
| 0x005c7700 | 38 | unlabelled |  |
| 0x005c7860 | 8 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:536) |
| 0x005c7a30 | 8 | folded | empty: does nothing |
| 0x005c7fb0 | 12 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:726) |
| 0x005c7fd0 | 11 | unlabelled |  |
| 0x005c8110 | 49 | unlabelled |  |
| 0x005c82b0 | 9 | folded | a thunk: one call to another function |
| 0x005c8340 | 9 | folded | a thunk: one call to another function |
| 0x005c8440 | 22 | replaced (SDL or the platform layer) | PM's 2D drawing through Direct3D (0x005c0320: [0x008c4b70] and [0x008d3aa9] set; its lines, rects, blits, scene begin and end through the device's function table [0x008c4bd8..]; else the software 2D path): the SDL 2D host draws the 2D layer |
| 0x005c84e0 | 112 | replaced (SDL or the platform layer) | PM's 2D drawing through Direct3D (0x005c0320: [0x008c4b70] and [0x008d3aa9] set; its lines, rects, blits, scene begin and end through the device's function table [0x008c4bd8..]; else the software 2D path): the SDL 2D host draws the 2D layer |
| 0x005c87b0 | 64 | unlabelled |  |
| 0x005c8920 | 46 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x005a6a10: Line2D's reader from a dialog .zrd (its COLOR and its end), shared by the line classes' vtables; inferred unused by shipped data (the dialog reader matches the exe on every shipped dialog without it), not checked file by file |
| 0x005c8a40 | 9 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:620) |
| 0x005c8a50 | 24 | unlabelled |  |
| 0x005c8ae0 | 23 | unlabelled |  |
| 0x005c8c20 | 43 | unlabelled |  |
| 0x005c8e50 | 10 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x005c9130 | 12 | unlabelled |  |
| 0x005c9280 | 104 | unlabelled |  |
| 0x005c9570 | 10 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:846) |
| 0x005c9590 | 10 | folded | inside its ported callers (0x0048cec0): a small helper their ports do their own way |
| 0x005c9650 | 9 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:846) |
| 0x005c9660 | 10 | folded | as MW3 0x005a7670: covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005c9680 | 10 | folded | as MW3 0x005a7690: covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005c96a0 | 10 | folded | as MW3 0x005a76b0: covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005c96c0 | 10 | folded | as MW3 0x005a76d0: covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005c96e0 | 10 | folded | as MW3 0x005a76f0: covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005c9700 | 10 | folded | as MW3 0x005a7710: covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005c9720 | 10 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:823) |
| 0x005c97e0 | 12 | unlabelled |  |
| 0x005c9a60 | 12 | unlabelled |  |
| 0x005c9aa0 | 17 | unlabelled |  |
| 0x005c9b20 | 12 | unlabelled |  |
| 0x005c9b40 | 26 | unlabelled |  |
| 0x005c9bb0 | 48 | unlabelled |  |
| 0x005c9cc0 | 20 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x005a7c50: Movie2D's update, draw and erase (a movie playing inside a dialog): no shipped dialog has one (twod.md:674-679) |
| 0x005c9d10 | 12 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x005a7ca0, whose class PM's changes keep: Movie2D's update, draw and erase (a movie playing inside a dialog): no shipped dialog has one (twod.md:674-679) |
| 0x005c9d40 | 14 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x005a7cd0: Movie2D's update, draw and erase (a movie playing inside a dialog): no shipped dialog has one (twod.md:674-679) |
| 0x005c9e30 | 212 | replaced (SDL or the platform layer) | called only from 0x005c08f0, which are sdl |
| 0x005ca3c0 | 17 | replaced (SDL or the platform layer) | called only from 0x0059c230, 0x005c9e30, which are sdl |
| 0x005ca3e0 | 74 | replaced (SDL or the platform layer) | PM's 2D drawing through Direct3D (0x005c0320: [0x008c4b70] and [0x008d3aa9] set; its lines, rects, blits, scene begin and end through the device's function table [0x008c4bd8..]; else the software 2D path): the SDL 2D host draws the 2D layer |
| 0x005ca590 | 35 | unlabelled |  |
| 0x005ca600 | 83 | replaced (SDL or the platform layer) | called only from 0x005c9e30, which are sdl |
| 0x005ca7f0 | 95 | unlabelled |  |
| 0x005caa10 | 8 | folded | empty: does nothing |
| 0x005caa30 | 9 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005caa40 | 9 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005caa70 | 44 | folded | as MW3 0x005a7e10: zEntity's base constructor (frames, and with a parent the registration 0x005a8390 + 0x005a7f50); the port's AiTarget initialisers stand in (app_engine.cpp:312). targets.zrd's CZAiTargets are not on the port's entity list (ai.md:1537), so their vt+0x34 (0x005a8000, the step by velocity +0x30 and spin +0x58) does not run; checked harmless: both start as the zero vector [0x0063d870] (BSS), targets.zrd has no velocity or spin key (ai.md:153) and no script command sets an entity's (interp.md: only CameraSetRotate and GlobalWindStaticVelocity) |
| 0x005cab90 | 12 | unlabelled |  |
| 0x005cabb0 | 11 | folded | as MW3 0x005a7f50: covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005cabd0 | 11 | folded | as MW3 0x005a7f70: covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005cabf0 | 24 | unlabelled |  |
| 0x005cad20 | 9 | unlabelled | follows 0x004fd3f0:  |
| 0x005cad40 | 10 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:518) |

### zvideo (110 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00598050 | 78 | replaced (SDL or the platform layer) | as MW3 0x00576f90, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005981d0 | 13 | replaced (SDL or the platform layer) | as MW3 0x00579210: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005981f0 | 15 | MISSING LOGIC (only data the shipped game lacks reaches it) | the script command VideoSetWireFrame, as MW3's 0x00579230 (missing-mod there: the engine entry is null, no shipped script uses it); PM sets the device's fill mode at once (render state 8: 2 wireframe, 3 solid) where MW3 stored a flag |
| 0x00598230 | 20 | MISSING LOGIC | pm only, renderer, known: the script command MipBias (common.gw's MipBias -1) sets Direct3D's mip bias, clamped, with the state's dirty bit 4; the port reads the words and applies nothing (zinterp_parse.cpp:511), listed in docs/fixes.md as not reproduced yet, a candidate for Owen with the renderer's filtering |
| 0x005982a0 | 11 | replaced (SDL or the platform layer) | as MW3 0x00579260: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005982d0 | 11 | folded | covered by the port where it names it (src/gamez/zfmv/fmv_stream.cpp:92) |
| 0x00598300 | 11 | replaced (SDL or the platform layer) | as MW3 0x005792a0: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00598420 | 10 | folded | inside its ported callers (0x00547dd0): a small helper their ports do their own way |
| 0x00598470 | 24 | unlabelled |  |
| 0x00598530 | 27 | folded | covered by the port where it names it (src/gamez/zvideo/zvid_ddd3d.cpp:974) |
| 0x00598600 | 88 | replaced (SDL or the platform layer) | as MW3 0x00579500: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00598760 | 104 | replaced (SDL or the platform layer) | as MW3 0x00579660: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00598940 | 23 | replaced (SDL or the platform layer) | as MW3 0x00579840: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00598990 | 12 | replaced (SDL or the platform layer) | as MW3 0x00579880: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00598b10 | 11 | folded | as MW3 0x005799f0: covered by the program's overrides: the load screen's driver and mode lines (app_mission.cpp:1896), ClearScreenBuffer (app_engine.h:508), video shutdown and restore (app_engine.h:547, :549), a weapon hit's health after (zweapon.h:171) |
| 0x00598b30 | 8 | folded | returns a constant or a global |
| 0x00598b70 | 15 | replaced (SDL or the platform layer) | as MW3 0x00579ab0: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00598c00 | 14 | replaced (SDL or the platform layer) | as MW3 0x00579b40: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00598c90 | 65 | unlabelled |  |
| 0x00598d70 | 14 | replaced (SDL or the platform layer) | called only from 0x0059ae30, which are sdl |
| 0x00598fb0 | 11 | replaced (SDL or the platform layer) | as MW3 0x00577340: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005992e0 | 27 | folded | covered by the port where it names it (src/gamez/zvideo/zvid_dd.h:28) |
| 0x00599340 | 20 | folded | covered by the port where it names it (src/gamez/zvideo/zvid_dd.h:29) |
| 0x00599390 | 27 | replaced (SDL or the platform layer) | as MW3 0x005776d0, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005993f0 | 20 | replaced (SDL or the platform layer) | as MW3 0x00577730, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005995a0 | 47 | replaced (SDL or the platform layer) | as MW3 0x005778d0, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00599710 | 83 | replaced (SDL or the platform layer) | as MW3 0x00577a10, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00599950 | 24 | replaced (SDL or the platform layer) | as MW3 0x00577c40, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005999c0 | 11 | folded | as MW3 0x00577cb0: covered by the program's overrides: the load screen's driver and mode lines (app_mission.cpp:1896), ClearScreenBuffer (app_engine.h:508), video shutdown and restore (app_engine.h:547, :549), a weapon hit's health after (zweapon.h:171) |
| 0x00599a50 | 25 | replaced (SDL or the platform layer) | as MW3 0x00577d40, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00599ac0 | 11 | replaced (SDL or the platform layer) | as MW3 0x00577db0: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00599b00 | 177 | replaced (SDL or the platform layer) | as MW3 0x00577df0, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059a070 | 38 | replaced (SDL or the platform layer) | as MW3 0x00578510, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059a150 | 31 | replaced (SDL or the platform layer) | as MW3 0x005785f0: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059a1e0 | 32 | replaced (SDL or the platform layer) | as MW3 0x00578670, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059a260 | 20 | replaced (SDL or the platform layer) | as MW3 0x005786f0, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059a2b0 | 90 | replaced (SDL or the platform layer) | as MW3 0x00578740, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059a470 | 34 | folded | covered by the port where it names it (src/gamez/zvideo/zvid_ddd3d.cpp:1021) |
| 0x0059a530 | 16 | replaced (SDL or the platform layer) | as MW3 0x00578960, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059a800 | 28 | replaced (SDL or the platform layer) | as MW3 0x00578c00, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059ab90 | 10 | replaced (SDL or the platform layer) | as MW3 0x00578f60: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059ac20 | 8 | folded | returns a constant or a global |
| 0x0059ac30 | 8 | folded | returns a constant or a global |
| 0x0059ac60 | 11 | folded | as MW3 0x00579040: covered by the program's overrides: the load screen's driver and mode lines (app_mission.cpp:1896), ClearScreenBuffer (app_engine.h:508), video shutdown and restore (app_engine.h:547, :549), a weapon hit's health after (zweapon.h:171) |
| 0x0059ae30 | 38 | replaced (SDL or the platform layer) | as MW3 0x00579bd0, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059aef0 | 12 | folded | read: teardown frees at the app's end (a model table freed and its count cleared; the video's buffer freed), from the app's cleanup (0x00483020 / 0x00482090); the port's objects free themselves |
| 0x0059af10 | 79 | replaced (SDL or the platform layer) | as MW3 0x00579c60, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059b140 | 8 | folded | returns a constant or a global |
| 0x0059b150 | 11 | replaced (SDL or the platform layer) | as MW3 0x00579eb0: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059b190 | 14 | folded | as MW3 0x00579ef0: covered by the program's overrides: the load screen's driver and mode lines (app_mission.cpp:1896), ClearScreenBuffer (app_engine.h:508), video shutdown and restore (app_engine.h:547, :549), a weapon hit's health after (zweapon.h:171) |
| 0x0059b1c0 | 16 | replaced (SDL or the platform layer) | as MW3 0x00579f20: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059b450 | 77 | replaced (SDL or the platform layer) | as MW3 0x0057a1a0: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059b640 | 15 | replaced (SDL or the platform layer) | as MW3 0x0057a390: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059b670 | 12 | folded | as MW3 0x0057a3c0: covered by the program's overrides: the load screen's driver and mode lines (app_mission.cpp:1896), ClearScreenBuffer (app_engine.h:508), video shutdown and restore (app_engine.h:547, :549), a weapon hit's health after (zweapon.h:171) |
| 0x0059b740 | 23 | unlabelled |  |
| 0x0059b870 | 12 | folded | covered by the port where it names it (src/pm/gamez/zmodel/gmod_draw.h:132) |
| 0x0059b8a0 | 347 | replaced (SDL or the platform layer) | as MW3 0x0057a900, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059c230 | 18 | replaced (SDL or the platform layer) | as MW3 0x0057b100: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059c920 | 106 | replaced (SDL or the platform layer) | called only from 0x0059c230, 0x0059ce80, which are sdl |
| 0x0059cb10 | 148 | replaced (SDL or the platform layer) | as MW3 0x0057b7b0, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059ce80 | 15 | replaced (SDL or the platform layer) | as MW3 0x0057ba80: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059cf60 | 9 | replaced (SDL or the platform layer) | as MW3 0x0057bb60: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0059d0f0 | 16 | unlabelled |  |
| 0x0059d3a0 | 34 | unlabelled |  |
| 0x0059d9d0 | 170 | unlabelled |  |
| 0x0059e0c0 | 167 | unlabelled |  |
| 0x0059f260 | 130 | unlabelled |  |
| 0x005a0040 | 14 | folded | covered by the port where it names it (src/gamez/zvideo/zvid_ddd3d.cpp:307) |
| 0x005a0065 | 134 | folded | follows 0x005a0040: covered by the port where it names it (src/gamez/zvideo/zvid_ddd3d.cpp:307) |
| 0x005a03b0 | 333 | folded | ported (read): PM's shadow and light-map draw, the device table's [0x008c4bfc] slot (0x005a0390 calls it; MW3's slot holds 0x0057eeb0, draw_shadowed): the polygon, then each set over it with its own u, v (a shadow multiplied, a light map added); the pm port's zvid_tl.cpp:185 (DeviceCall::kSets, gmod_draw.h:123), its Direct3D texture stages replaced by one shader (FIX (A), docs/fixes.md, the light map row) |
| 0x005a1050 | 84 | replaced (SDL or the platform layer) | as MW3 0x00580350, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005a1220 | 19 | replaced (SDL or the platform layer) | read: Direct3D's viewport set from the camera's window (a D3DVIEWPORT2 at 0x00b0a2f4: origin and size, the clip from the window's corners; IDirect3DViewport3::SetViewport2, vt+0x44), with hardware on ([0x008c4b70]), from the window apply 0x00548990; the port sets its Render3D clip instead (src/pm/gamez/zmodel/gmod_draw.cpp walk_draw, FIX (A)) |
| 0x005a12a0 | 29 | replaced (SDL or the platform layer) | called only from 0x005c0450, 0x005c0b70, which are sdl |
| 0x005a1350 | 181 | replaced (SDL or the platform layer) | as MW3 0x0057a3d0, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005a1740 | 525 | replaced (SDL or the platform layer) | as MW3 0x00580550, whose class PM's changes keep: DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005a2600 | 30 | replaced (SDL or the platform layer) | called only from 0x0059b8a0, which are sdl |
| 0x005a2680 | 42 | replaced (SDL or the platform layer) | called only from 0x005a2770, which are sdl |
| 0x005a2700 | 31 | replaced (SDL or the platform layer) | called only from 0x005a2680, which are sdl |
| 0x005a2770 | 20 | replaced (SDL or the platform layer) | called only from 0x005a2600, which are sdl |
| 0x005a2a00 | 52 | MISSING LOGIC | pm only, group A, read: PM's beam release frees both running FIRE animations (+0x90, +0x94); the port frees its one (zwep_beam.cpp:192). Goes with the reader 0x005a6700 (MW3 0x00581400) |
| 0x005a2ae0 | 375 | MISSING LOGIC | pm only, carries the low-detail impact switch (pm only: at object detail 2 (0x0040b1e0 == 2) PM's world load 0x00492830 turns off two zWeapon switches (0x005a52c0 the FIRE block's EFFECT, 0x005a52d0 the impact animations; 0x005a5170 sets both to 1 at start): the impact animations of hits (0x005a3df0), beams (0x005a2ae0) and damage (0x005a4e80) are not played, and a FIRE EFFECT is read as its MODEL (no shipped weapon has one); the pm port has neither switch (mission.cpp:114 load_world, zweapon.cpp)); the rest read after the feature compare's flags: MW3's 0x005814b0 (the beam's step, zwep_beam.cpp) but for a zeroed local grown from 0x175 to 0x196 dwords (PM's larger record) and fsub -1.0 written as fadd 1.0 |
| 0x005a34b0 | 8 | folded | returns a constant or a global |
| 0x005a34c0 | 8 | folded | returns a constant or a global |
| 0x005a34d0 | 30 | unlabelled |  |
| 0x005a3550 | 46 | unlabelled |  |
| 0x005a35f0 | 33 | MISSING LOGIC | pm only, minor (a mech deleted mid-mission: a network player leaving): PM's mech destructor (0x00415940, MW3's 0x00414f40, mech_build.cpp:225) first clears the owner (+0x6c, +0x70, +0x74) of every live shot that mech fired (0x005a35f0 over zWeapon's pools); MW3 and the port do not (all three free the node's handler table: MW3 0x00583400, PM 0x005a4de0, the port app_mech.cpp:848), so a shot outliving its mech keeps the gone owner. To check: whether the port's shot reads that owner afterwards (its hit credit, its tether) |
| 0x005a36c0 | 10 | folded | inside its ported callers (0x004b73f0): a small helper their ports do their own way |
| 0x005a36e0 | 27 | unlabelled |  |
| 0x005a37a0 | 59 | unlabelled |  |
| 0x005a38a0 | 43 | unlabelled |  |
| 0x005a39c0 | 29 | folded | read: MW3's 0x005822c0 (a weapon's burst at a point: a zeroed hit record at the point, health 1.0, then 0x005821c0) not paired by the diff, its record grown to 0x196 dwords; the anim event's detonate (zeff_anim_events.cpp ev_detonate_weapon) runs it through the port's zwep::burst_at (app_mission.cpp:2170) |
| 0x005a3a30 | 35 | unlabelled |  |
| 0x005a4540 | 43 | unlabelled |  |
| 0x005a47a0 | 34 | unlabelled |  |
| 0x005a49d0 | 33 | unlabelled |  |
| 0x005a4a40 | 33 | unlabelled |  |
| 0x005a4aa0 | 28 | unlabelled |  |
| 0x005a4b10 | 9 | unlabelled | follows 0x005a4b90:  |
| 0x005a4b20 | 9 | folded | a thunk: one call to another function |
| 0x005a4b40 | 25 | unlabelled |  |
| 0x005a4b90 | 18 | unlabelled |  |
| 0x005a4bc0 | 9 | unlabelled | follows 0x005a49d0:  |
| 0x005a4c30 | 12 | unlabelled |  |
| 0x005a4cb0 | 33 | folded | MW3's 0x005832f0 (zweapon.cpp:242, a slot of the node's handler table): the same, plus each new table also put on a list (0x005a4a40) and a null node taking a new table; memory only; not run under a test (trace) |
| 0x005a4dc0 | 9 | folded | inside its ported callers (0x004e37d0, 0x00513310, 0x00524492): a small helper their ports do their own way |
| 0x005a4de0 | 19 | folded | read: MW3's 0x00583400 (remove_handlers: the node's own handler table cleared from it and its children, then freed) not paired by the diff, the table also taken off PM's list of tables (0x005a4aa0) before the free; the port's zwep::remove_handlers, called from the same teardowns (app_mech.cpp:848, app_turret.cpp, app_vehicle.cpp, zai_target.cpp, zeff_anim_init.cpp:706) |
| 0x005a5150 | 11 | folded | as MW3 0x00583730: covered by the program's overrides: the load screen's driver and mode lines (app_mission.cpp:1896), ClearScreenBuffer (app_engine.h:508), video shutdown and restore (app_engine.h:547, :549), a weapon hit's health after (zweapon.h:171) |
| 0x005a5260 | 12 | unlabelled |  |
| 0x005a52c0 | 9 | MISSING LOGIC | pm only: at object detail 2 (0x0040b1e0 == 2) PM's world load 0x00492830 turns off two zWeapon switches (0x005a52c0 the FIRE block's EFFECT, 0x005a52d0 the impact animations; 0x005a5170 sets both to 1 at start): the impact animations of hits (0x005a3df0), beams (0x005a2ae0) and damage (0x005a4e80) are not played, and a FIRE EFFECT is read as its MODEL (no shipped weapon has one); the pm port has neither switch (mission.cpp:114 load_world, zweapon.cpp) |
| 0x005a52d0 | 9 | MISSING LOGIC | pm only: at object detail 2 (0x0040b1e0 == 2) PM's world load 0x00492830 turns off two zWeapon switches (0x005a52c0 the FIRE block's EFFECT, 0x005a52d0 the impact animations; 0x005a5170 sets both to 1 at start): the impact animations of hits (0x005a3df0), beams (0x005a2ae0) and damage (0x005a4e80) are not played, and a FIRE EFFECT is read as its MODEL (no shipped weapon has one); the pm port has neither switch (mission.cpp:114 load_world, zweapon.cpp) |

### zinput (104 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00533b90 | 92 | unlabelled |  |
| 0x00533de0 | 42 | unlabelled |  |
| 0x00533e60 | 191 | unlabelled |  |
| 0x00534380 | 13 | folded | read: a keyboard wait (loop until a key comes, 0x00533e60) that its only caller, the controls menu's key capture (0x0042cd30), calls with 0, which skips it; the port's capture is its own (controls_menu.cpp) |
| 0x00534780 | 9 | folded | the same as its MW3 twin after the address map (nothing differs); not run under a test (trace) |
| 0x00534790 | 9 | folded | a thunk: one call to another function |
| 0x005347b0 | 36 | unused: PORT (preserve) | as MW3 0x0051af90: the input module's static object's list walk, whose caller 0x0051af70 nothing reaches (input.md:567) |
| 0x00534830 | 14 | folded | as MW3 0x0051b020: the input module's static object's constructor (zeroes its lists); the port's members start empty |
| 0x005348b0 | 40 | unlabelled |  |
| 0x005349f0 | 8 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:71) |
| 0x00534a00 | 8 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:71) |
| 0x00534a10 | 12 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:88) |
| 0x00534a30 | 12 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:88) |
| 0x00534a50 | 12 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:88) |
| 0x00534a70 | 9 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:82) |
| 0x00534a80 | 9 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:81) |
| 0x00534a90 | 9 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:83) |
| 0x00535760 | 8 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:136) |
| 0x00535780 | 8 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:136) |
| 0x005357a0 | 8 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:136) |
| 0x005357d0 | 8 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:148) |
| 0x00535800 | 8 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:148) |
| 0x00535810 | 8 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:148) |
| 0x00535c70 | 9 | folded | a thunk: one call to another function |
| 0x00535f00 | 130 | unlabelled |  |
| 0x005369a0 | 18 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:397) |
| 0x005369d0 | 18 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:396) |
| 0x00536a00 | 18 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:395) |
| 0x00536a40 | 9 | folded | a thunk: one call to another function |
| 0x00536a60 | 9 | folded | a thunk: one call to another function |
| 0x00536a80 | 9 | folded | a thunk: one call to another function |
| 0x00536aa0 | 9 | folded | a thunk: one call to another function |
| 0x00536ac0 | 9 | folded | a thunk: one call to another function |
| 0x00536ae0 | 9 | folded | a thunk: one call to another function |
| 0x00536b00 | 9 | folded | a thunk: one call to another function |
| 0x00536b20 | 9 | folded | a thunk: one call to another function |
| 0x00536b40 | 9 | folded | a thunk: one call to another function |
| 0x00536b60 | 9 | folded | a thunk: one call to another function |
| 0x00536b80 | 9 | folded | a thunk: one call to another function |
| 0x00536ba0 | 9 | folded | a thunk: one call to another function |
| 0x00536bc0 | 9 | folded | a thunk: one call to another function |
| 0x00536be0 | 9 | folded | a thunk: one call to another function |
| 0x00536c00 | 11 | folded | covered by the port where it names it (src/gamez/zinput/zinput.h:224) |
| 0x00536c30 | 9 | folded | a thunk: one call to another function |
| 0x00536c50 | 9 | folded | a thunk: one call to another function |
| 0x00536c70 | 9 | folded | a thunk: one call to another function |
| 0x00536c90 | 9 | folded | a thunk: one call to another function |
| 0x00536cb0 | 9 | folded | a thunk: one call to another function |
| 0x00536cd0 | 9 | folded | a thunk: one call to another function |
| 0x00537430 | 20 | replaced (SDL or the platform layer) | as MW3 0x0051daa0: DirectInput joystick properties (range and dead zone per axis); the port sets the dead zones through the platform (zin_joy.cpp:60) |
| 0x00537480 | 22 | replaced (SDL or the platform layer) | as MW3 0x0051dae0: DirectInput joystick properties (range and dead zone per axis); the port sets the dead zones through the platform (zin_joy.cpp:60) |
| 0x005374d0 | 22 | folded | covered by the port where it names it (src/gamez/zinput/zin_joy.cpp:47) |
| 0x00537520 | 21 | folded | covered by the port where it names it (src/gamez/zinput/zin_joy.cpp:46) |
| 0x00537760 | 8 | folded | returns a constant or a global |
| 0x00537780 | 97 | unlabelled |  |
| 0x00537aa0 | 10 | folded | covered by the port where it names it (src/mech3/zai_repair.cpp:201) |
| 0x00537ae0 | 10 | folded | ported, by docs/symbols.csv: ported inline (zai_node flat_distance) |
| 0x00537bc0 | 27 | unused: PORT (preserve) | as MW3 0x0051e160: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x00537c20 | 18 | folded | as MW3 0x0051e1c0: the 2D normalise, inline in its ported caller (0x00524247) |
| 0x00537dd0 | 16 | folded | covered by the port where it names it (src/mech3/mech_net.cpp:707) |
| 0x00537e60 | 22 | folded | inside its ported callers (0x004d92c0, 0x004d9a90, 0x004da1c0, 0x004dcd20): a small helper their ports do their own way |
| 0x00538230 | 67 | unlabelled |  |
| 0x00539200 | 143 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x0051f750: billboard modes 2 (about x) and 3 (about z): no shipped world uses them; they draw as default models (gmod_draw.cpp:759, render-modeldraw.md:27) |
| 0x0053a730 | 150 | unused: PORT (preserve) | as MW3 0x00520c60: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053ab50 | 166 | unused: PORT (preserve) | as MW3 0x00521080: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053afc0 | 70 | unused: PORT (preserve) | as MW3 0x005214f0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053b560 | 62 | unused: PORT (preserve) | as MW3 0x00521a90: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053b8d0 | 87 | unused: PORT (preserve) | as MW3 0x00521e10: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053bc30 | 46 | folded | as MW3 0x00522170: pitch and yaw to a direction, ported where the game uses it (mission_world.cpp:82, mech_objective.cpp:122); but the HUD's hook for it, radar_view_dir (hud.h:876), is never overridden: see BROKEN |
| 0x0053bce0 | 31 | folded | covered by the port where it names it (src/gamez/zeffect/zeff_anim_events.cpp:393) |
| 0x0053bd40 | 53 | unused: PORT (preserve) | as MW3 0x00522280: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053be80 | 16 | unused: PORT (preserve) | as MW3 0x005223c0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053bf40 | 11 | folded | covered by the port where it names it (src/mech3/player_view.cpp:107) |
| 0x0053bfa0 | 50 | unused: PORT (preserve) | as MW3 0x005224b0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053c780 | 47 | folded | read: the box's eight x, z corners through a 3x4 matrix (MW3's corner transform 0x00522d60 written out), from PM's node_box_xz 0x004fdd60 (MW3's 0x004ec150; class.cpp node_box_xz, which carries PM's box +0x70 and composes the node's matrix with the stack's as PM does, then zmth::transform_box_xz) |
| 0x0053d000 | 115 | unused: PORT (preserve) | as MW3 0x00523650, whose class PM's changes keep: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053d7c0 | 32 | unused: PORT (preserve) | as MW3 0x00523df0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053d8c0 | 31 | unused: PORT (preserve) | as MW3 0x00523ef0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053db5d | 34 | folded | a piece of the ported function before it (Ghidra split it there) (0x0053d9f0) |
| 0x0053de0d | 33 | folded | a piece of the ported function before it (Ghidra split it there) (0x0053d9f0) |
| 0x0053df5d | 83 | folded | a piece of the ported function before it (Ghidra split it there) (0x0053d9f0) |
| 0x0053e2ed | 96 | folded | a piece of the ported function before it (Ghidra split it there) (0x0053d9f0) |
| 0x0053e8c0 | 21 | unused: PORT (preserve) | as MW3 0x00524f80: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053e910 | 17 | unused: PORT (preserve) | as MW3 0x00524fd0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053e960 | 21 | unused: PORT (preserve) | as MW3 0x00525020: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053e9e0 | 9 | unused: PORT (preserve) | as MW3 0x005250a0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053ea30 | 9 | unused: PORT (preserve) | as MW3 0x005250f0: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053eae0 | 29 | folded | covered by the port where it names it (src/gamez/zmath/zmth_vector.cpp:286) |
| 0x0053ee40 | 27 | unlabelled |  |
| 0x0053ef90 | 16 | folded | read: MW3's 0x005255d0 (the quaternion product a b: the same terms on each of the four lines, its arguments on the stack), which the address map lacks; its 21 callers (the player view's and others) are the ports of MW3's callers of 0x005255d0 (zmth_matrix.cpp:416, player_view.cpp:30) |
| 0x0053f020 | 16 | folded | MW3's 0x00525690 unpaired (a quaternion's conjugate product, the same terms; the first component's four products are summed in another order, rounding only), in another calling convention; not run under a test (trace) |
| 0x0053f310 | 35 | unlabelled |  |
| 0x0053f900 | 144 | unused: PORT (preserve) | as MW3 0x00525fa0, whose class PM's changes keep: zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0053fe30 | 87 | UNSURE | read with group A: a callee of shared ports PM changed (0x00482ca0); whether the port does PM's version, this call included, is settled there |
| 0x00540090 | 16 | folded | read: the model code's GfxFlags pointer bound to the registry's value of that name (GfxFlags_HW / GfxFlags_SW, 0x005b9070) or to a local default, from the app's settings read (0x00482ca0) and the graphics screen; the port keeps GfxFlags in its options (options.cpp) and the model code reads them through its hooks |
| 0x005400c0 | 10 | folded | read: teardown frees at the app's end (a model table freed and its count cleared; the video's buffer freed), from the app's cleanup (0x00483020 / 0x00482090); the port's objects free themselves |
| 0x005400e0 | 32 | unlabelled |  |
| 0x00540200 | 56 | unlabelled |  |
| 0x00540310 | 8 | folded | empty: does nothing |
| 0x00540320 | 8 | folded | returns a constant or a global |
| 0x00540330 | 11 | software renderer | as MW3 0x005269e0: the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x00540360 | 9 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.h:325) |
| 0x00540410 | 12 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00526aa0: script engine commands for building models and textures at run time (Object3DSetColor, SetShowBackFace, SetMorphVertex, the texture world base and per meter): the engine entries are null in the port (hook_allow: counted) and no shipped script uses them (the census: 0 skipped) |
| 0x00540430 | 12 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00526ac0: script engine commands for building models and textures at run time (Object3DSetColor, SetShowBackFace, SetMorphVertex, the texture world base and per meter): the engine entries are null in the port (hook_allow: counted) and no shipped script uses them (the census: 0 skipped) |

### zsound (100 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00590820 | 11 | unlabelled |  |
| 0x005908b0 | 9 | unlabelled | follows 0x005a4b90:  |
| 0x005908c0 | 9 | folded | a thunk: one call to another function |
| 0x005908e0 | 9 | folded | read: a setter of a global that nothing in PM reads (pm.asm: one or two writes, no read), so writing it or not changes nothing |
| 0x005908f0 | 18 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_parm.cpp:139) |
| 0x00590920 | 18 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_parm.cpp:145) |
| 0x00590b90 | 35 | folded | inside its ported callers (0x00590ae0): a small helper their ports do their own way |
| 0x00590c10 | 58 | unlabelled |  |
| 0x00590d20 | 33 | folded | inside its ported callers (0x005920e0): a small helper their ports do their own way |
| 0x00590f60 | 9 | folded | a thunk: one call to another function |
| 0x00590f70 | 9 | folded | a thunk: one call to another function |
| 0x00591120 | 10 | folded | inside its ported callers (0x00590f80): a small helper their ports do their own way |
| 0x00591140 | 9 | unlabelled |  |
| 0x00591160 | 44 | folded | as MW3 0x0056ffc0: covered where named: the sound set's build (zsnd_init.cpp:260, 265), the archive load (zsnd_parm.cpp:63), 3D placement (zsnd_3d.cpp:138), the group chain step (zsnd_grp.cpp:170) |
| 0x00591220 | 15 | folded | inside its ported callers (0x00590d70): a small helper their ports do their own way |
| 0x00591250 | 22 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_parm.cpp:3) |
| 0x005915e0 | 25 | unlabelled |  |
| 0x00591ce0 | 39 | unused: PORT (preserve) | as MW3 0x00571380, whose class PM's changes keep: the Aureal A3D path: its init (CoCreateInstance), 3D placement and error names; only 0x570625 writes the selector [0x72c2cc], and only to 0 (DirectSound) when A3D fails, so A3D never runs (sound.md:38) |
| 0x00591dd0 | 87 | unlabelled |  |
| 0x00591f30 | 9 | folded | a thunk: one call to another function |
| 0x00592040 | 12 | folded | inside its ported callers (0x00592170): a small helper their ports do their own way |
| 0x00592490 | 16 | folded | as MW3 0x005716d0: zSound's static containers and their cleanup (vector and list heads made at start-up, the sets' pointer list freed); the port's members construct and free themselves |
| 0x005924c0 | 9 | folded | a thunk: one call to another function |
| 0x00592540 | 16 | unlabelled |  |
| 0x00592570 | 9 | folded | a thunk: one call to another function |
| 0x005925e0 | 20 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_3d.cpp:27) |
| 0x00592600 | 41 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x005926d0 | 31 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_3d.cpp:95) |
| 0x00592720 | 76 | folded | ported, by docs/symbols.csv: ported: 2500/s, ends only on an exact landing |
| 0x00592910 | 52 | unlabelled |  |
| 0x00592cc0 | 45 | unused: PORT (preserve) | as MW3 0x00571f70: the Aureal A3D path: its init (CoCreateInstance), 3D placement and error names; only 0x570625 writes the selector [0x72c2cc], and only to 0 (DirectSound) when A3D fails, so A3D never runs (sound.md:38) |
| 0x00592da0 | 114 | folded | as MW3 0x00572050: covered where named: the sound set's build (zsnd_init.cpp:260, 265), the archive load (zsnd_parm.cpp:63), 3D placement (zsnd_3d.cpp:138), the group chain step (zsnd_grp.cpp:170) |
| 0x005930f0 | 16 | folded | as MW3 0x005723a0: zSound's static containers and their cleanup (vector and list heads made at start-up, the sets' pointer list freed); the port's members construct and free themselves |
| 0x00593120 | 9 | folded | a thunk: one call to another function |
| 0x005935b0 | 19 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_cd.cpp:24) |
| 0x00593940 | 22 | unlabelled |  |
| 0x005939a0 | 36 | unlabelled |  |
| 0x00593a90 | 34 | unlabelled |  |
| 0x00593b60 | 26 | unlabelled |  |
| 0x00593be0 | 14 | replaced (SDL or the platform layer) | as MW3 0x00572ec0: the CD music volume through the Windows mixer and aux API (sound.md:264: the music output's own volume stands in) |
| 0x00593c00 | 37 | replaced (SDL or the platform layer) | as MW3 0x00572ee0, whose class PM's changes keep: the CD music volume through the Windows mixer and aux API (sound.md:264: the music output's own volume stands in) |
| 0x00593cb0 | 35 | unlabelled |  |
| 0x00593d60 | 8 | folded | covered by the port where it names it (src/mech3/app_hooks.h:517) |
| 0x00593d80 | 40 | replaced (SDL or the platform layer) | as MW3 0x00573060: the CD music volume through the Windows mixer and aux API (sound.md:264: the music output's own volume stands in) |
| 0x00593e60 | 18 | replaced (SDL or the platform layer) | as MW3 0x00573140, whose class PM's changes keep: the CD music volume through the Windows mixer and aux API (sound.md:264: the music output's own volume stands in) |
| 0x00593e90 | 28 | replaced (SDL or the platform layer) | as MW3 0x00573170, whose class PM's changes keep: the CD music volume through the Windows mixer and aux API (sound.md:264: the music output's own volume stands in) |
| 0x00593f10 | 26 | replaced (SDL or the platform layer) | as MW3 0x005731f0, whose class PM's changes keep: the CD music volume through the Windows mixer and aux API (sound.md:264: the music output's own volume stands in) |
| 0x00594150 | 33 | folded | traced: SOUND_GROUPS read into the sound group list ([0x008c1b28], each group by 0x005941c0); the port's read_groups (zsnd_init.cpp:245, its comment naming 0x00594150) |
| 0x005941c0 | 303 | unlabelled |  |
| 0x00594600 | 198 | UNSURE | read with group A: a callee of shared ports PM changed (0x005941c0); whether the port does PM's version, this call included, is settled there |
| 0x005949c0 | 27 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_grp.cpp:141) |
| 0x00594a10 | 90 | folded | ported, by docs/symbols.csv: ported: weighted rand pick, dynamic weights |
| 0x00594ba0 | 99 | unlabelled |  |
| 0x00594d40 | 21 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_grp.cpp:162) |
| 0x00594d90 | 23 | unlabelled |  |
| 0x00594de0 | 16 | unlabelled |  |
| 0x00594e10 | 27 | unlabelled |  |
| 0x00594e60 | 22 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_grp.cpp:46) |
| 0x00594eb0 | 15 | unlabelled |  |
| 0x00594f10 | 9 | folded | checked by test (block trace, 2026-11-13): test_pm_sound's 0x0058f390 runs it; every DirectSound call, result and the whole state compared |
| 0x00594f20 | 58 | unlabelled |  |
| 0x00595060 | 15 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_play.cpp:263) |
| 0x005950a0 | 11 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_grp.cpp:257) |
| 0x005950e0 | 9 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x00595100 | 65 | folded | group A, read: the same logic on PM's layout and helpers (the reader's lookup 0x00578630 for 0x00556740, PM's list helpers, the flare's fade with PM's globals, the gravity command, the objective's distance through PM's HUD getter); the call counts differ by how often a getter is called |
| 0x00595240 | 27 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_grp.cpp:247) |
| 0x005952a0 | 9 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x00595310 | 12 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_cd.cpp:32) |
| 0x00595360 | 197 | unused: PORT (preserve) | as MW3 0x005746b0, whose class PM's changes keep: the Aureal A3D path: its init (CoCreateInstance), 3D placement and error names; only 0x570625 writes the selector [0x72c2cc], and only to 0 (DirectSound) when A3D fails, so A3D never runs (sound.md:38) |
| 0x00595940 | 16 | folded | as MW3 0x00574c90: zSound's static containers and their cleanup (vector and list heads made at start-up, the sets' pointer list freed); the port's members construct and free themselves |
| 0x00595970 | 9 | folded | a thunk: one call to another function |
| 0x005959e0 | 23 | folded | covered by the port where it names it (src/gamez/zsound/zsnd.h:355) |
| 0x00595a30 | 12 | replaced (SDL or the platform layer) | as MW3 0x00574d90: DirectSound buffers: made from a WAV (0x00574ff0) and released (0x00574d90, 0x00574f10); SDL audio holds the port's |
| 0x00595ba0 | 21 | replaced (SDL or the platform layer) | as MW3 0x00574f10: DirectSound buffers: made from a WAV (0x00574ff0) and released (0x00574d90, 0x00574f10); SDL audio holds the port's |
| 0x00595c60 | 15 | unlabelled |  |
| 0x00595ca0 | 101 | replaced (SDL or the platform layer) | as MW3 0x00574ff0: DirectSound buffers: made from a WAV (0x00574ff0) and released (0x00574d90, 0x00574f10); SDL audio holds the port's |
| 0x00596510 | 9 | unlabelled |  |
| 0x00596520 | 41 | UNSURE | read with group A: a callee of shared ports PM changed (0x0043bb70, 0x004fd6f0, 0x00505c80); whether the port does PM's version, this call included, is settled there |
| 0x00596590 | 224 | debug or dev tool: PORT (preserve) | as MW3 0x00575b00: the developer's stdout redirect (freopen to a log; editor.md:214) |
| 0x00596780 | 9 | replaced (SDL or the platform layer) | as MW3 0x00575d00: Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00596790 | 8 | replaced (SDL or the platform layer) | as MW3 0x00575d10: Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x005967c0 | 69 | unlabelled |  |
| 0x005968d0 | 56 | replaced (SDL or the platform layer) | as MW3 0x00575d40, whose class PM's changes keep: Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x005969a0 | 100 | UNSURE | read with group A: a callee of shared ports PM changed (0x00482090); whether the port does PM's version, this call included, is settled there |
| 0x00596c00 | 43 | replaced (SDL or the platform layer) | as MW3 0x00575e30: Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00596d30 | 16 | replaced (SDL or the platform layer) | as MW3 0x00575f30: Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00596d70 | 12 | replaced (SDL or the platform layer) | as MW3 0x00575f70: Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00596de0 | 9 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x00596e00 | 15 | replaced (SDL or the platform layer) | as MW3 0x00575ff0: Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00596e50 | 20 | replaced (SDL or the platform layer) | as MW3 0x00576040, whose class PM's changes keep: Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00596ea0 | 11 | folded | covered by the port where it names it (src/mech3/app_hooks.h:573) |
| 0x00596ec0 | 9 | folded | a thunk: one call to another function |
| 0x00596ed0 | 18 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_create.cpp:129) |
| 0x00596f10 | 12 | folded | as MW3 0x00576100: zUtil's search path lists: each user keeps its own path string (zsnd_init.cpp:223, mech_fieldbase.h:295, zinterp.cpp:240) and the file finder is the platform's (zimg_library.h:75, zsnd_parm.cpp:17, FIX (A)); the default list's free has nothing to free (mission-frame.md:184) |
| 0x00596f30 | 10 | folded | covered by the port where it names it (src/gamez/zreader/zreader.cpp:642) |
| 0x00596f50 | 44 | folded | MW3's path list add 0x00576140 (folded there: each user keeps its own path, the file finder is the platform's), its loop restructured: read whole, every case (both lists nil, one nil, the same list) adds to the list and then to the global list as MW3's |
| 0x00597070 | 12 | folded | covered by the port where it names it (src/mech3/app_engine.cpp:325) |
| 0x00597090 | 18 | folded | as MW3 0x00576270: zUtil's search path lists: each user keeps its own path string (zsnd_init.cpp:223, mech_fieldbase.h:295, zinterp.cpp:240) and the file finder is the platform's (zimg_library.h:75, zsnd_parm.cpp:17, FIX (A)); the default list's free has nothing to free (mission-frame.md:184) |
| 0x005970d0 | 46 | folded | as MW3 0x005762b0: zUtil's search path lists: each user keeps its own path string (zsnd_init.cpp:223, mech_fieldbase.h:295, zinterp.cpp:240) and the file finder is the platform's (zimg_library.h:75, zsnd_parm.cpp:17, FIX (A)); the default list's free has nothing to free (mission-frame.md:184) |
| 0x005971d0 | 16 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:263) |

### zrender (82 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00587780 | 88 | software renderer | as MW3 0x005662d0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x005879c0 | 54 | software renderer | as MW3 0x00566510, whose class PM's changes keep: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00587ae0 | 135 | software renderer | as MW3 0x00566620: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00587d50 | 184 | software renderer | as MW3 0x00566890, whose class PM's changes keep: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00588170 | 114 | software renderer | as MW3 0x00566c90: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x005883f0 | 67 | software renderer | as MW3 0x00566f10: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00588590 | 142 | software renderer | as MW3 0x005670a0, whose class PM's changes keep: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x005889d0 | 25 | software renderer | as MW3 0x005674e0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00588a30 | 21 | software renderer | as MW3 0x00567530: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00588a70 | 9 | folded | ported, by docs/symbols.csv: ported: zrndr::flares_clear (the frame's start, 0x004817cd) |
| 0x00588a80 | 36 | software renderer | as MW3 0x00567580: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00588af0 | 27 | software renderer | as MW3 0x005675e0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00588c50 | 155 | unlabelled |  |
| 0x00589110 | 40 | unlabelled |  |
| 0x00589290 | 34 | folded | group A, read: the same logic on PM's layout and helpers (the reader's lookup 0x00578630 for 0x00556740, PM's list helpers, the flare's fade with PM's globals, the gravity command, the objective's distance through PM's HUD getter); the call counts differ by how often a getter is called |
| 0x00589310 | 47 | software renderer | as MW3 0x00567dc0, whose class PM's changes keep: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x005893f0 | 60 | unlabelled |  |
| 0x005895f0 | 8 | folded | empty: does nothing |
| 0x00589600 | 51 | unlabelled |  |
| 0x00589770 | 25 | software renderer | as MW3 0x00568230: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x005897e0 | 25 | software renderer | as MW3 0x005682a0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x005898c0 | 51 | unlabelled |  |
| 0x00589a40 | 25 | software renderer | as MW3 0x005684f0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00589ab0 | 14 | software renderer | as MW3 0x00568560: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00589b20 | 247 | software renderer | as MW3 0x005685c0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00589f30 | 254 | software renderer | as MW3 0x005689d0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058c8e0 | 182 | software renderer | as MW3 0x0056b500: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058cbe0 | 174 | software renderer | as MW3 0x0056b800: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058cee0 | 187 | software renderer | as MW3 0x0056bb00: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058d220 | 179 | software renderer | as MW3 0x0056be40: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058d5c0 | 35 | software renderer | as MW3 0x0056c1d0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058d710 | 49 | software renderer | as MW3 0x0056c320: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058d810 | 49 | software renderer | as MW3 0x0056c420: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058d910 | 179 | software renderer | as MW3 0x0056c520: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058dc20 | 179 | software renderer | as MW3 0x0056c830: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058dfb0 | 37 | software renderer | as MW3 0x0056cbc0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e030 | 37 | software renderer | as MW3 0x0056cc40: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e0b0 | 37 | software renderer | as MW3 0x0056ccc0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e130 | 37 | software renderer | as MW3 0x0056cd40: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e1b0 | 37 | software renderer | as MW3 0x0056cdc0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e230 | 37 | software renderer | as MW3 0x0056ce40: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e2b0 | 37 | software renderer | as MW3 0x0056cec0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e370 | 86 | software renderer | as MW3 0x0056cf80: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e510 | 86 | software renderer | as MW3 0x0056d120: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e6b0 | 37 | software renderer | as MW3 0x0056d2c0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e730 | 38 | software renderer | as MW3 0x0056d340: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e7b0 | 37 | software renderer | as MW3 0x0056d3c0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e830 | 37 | software renderer | as MW3 0x0056d440: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e8b0 | 37 | software renderer | as MW3 0x0056d4c0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e930 | 37 | software renderer | as MW3 0x0056d540: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058e9b0 | 37 | software renderer | as MW3 0x0056d5c0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058ea30 | 37 | software renderer | as MW3 0x0056d640: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058eab0 | 90 | software renderer | as MW3 0x0056d6c0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058eca0 | 90 | software renderer | as MW3 0x0056d8b0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058ee90 | 35 | software renderer | as MW3 0x0056dab0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058ef30 | 35 | software renderer | as MW3 0x0056db50: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058efd0 | 35 | software renderer | as MW3 0x0056dbf0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058f070 | 35 | software renderer | as MW3 0x0056dc90: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058f110 | 35 | software renderer | as MW3 0x0056dd30: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058f1b0 | 35 | software renderer | as MW3 0x0056ddd0: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058f250 | 35 | software renderer | as MW3 0x0056de70: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058f2f0 | 35 | software renderer | as MW3 0x0056df10: the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0058f390 | 83 | unlabelled |  |
| 0x0058f560 | 15 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_play.cpp:23) |
| 0x0058f590 | 64 | replaced (SDL or the platform layer) | as MW3 0x0056e180, whose class PM's changes keep: DirectSound: a sound's buffer duplicated or reused for another play (0x0056e180, 0x0056e4d0) and the buffer's volume and frequency calls (0x0056ef90); SDL's audio layer replaces them (zsnd_play) |
| 0x0058f6d0 | 49 | unlabelled |  |
| 0x0058f870 | 8 | folded | returns a constant or a global |
| 0x0058f880 | 19 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_play.cpp:82) |
| 0x0058f8e0 | 68 | replaced (SDL or the platform layer) | as MW3 0x0056e4d0, whose class PM's changes keep: DirectSound: a sound's buffer duplicated or reused for another play (0x0056e180, 0x0056e4d0) and the buffer's volume and frequency calls (0x0056ef90); SDL's audio layer replaces them (zsnd_play) |
| 0x0058fa20 | 70 | unlabelled |  |
| 0x0058fc20 | 12 | folded | inside its ported callers (0x0058fb60): a small helper their ports do their own way |
| 0x0058fc70 | 32 | unlabelled |  |
| 0x0058fd60 | 60 | unlabelled |  |
| 0x0058fe70 | 61 | unlabelled |  |
| 0x0058ff90 | 168 | unlabelled |  |
| 0x00590290 | 36 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_play.cpp:112) |
| 0x00590310 | 27 | folded | inside its ported callers (0x00590530): a small helper their ports do their own way |
| 0x00590390 | 28 | replaced (SDL or the platform layer) | as MW3 0x0056ef90, whose class PM's changes keep: DirectSound: a sound's buffer duplicated or reused for another play (0x0056e180, 0x0056e4d0) and the buffer's volume and frequency calls (0x0056ef90); SDL's audio layer replaces them (zsnd_play) |
| 0x00590410 | 29 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_play.cpp:127) |
| 0x00590610 | 37 | folded | inside its ported callers (0x0058ff90): a small helper their ports do their own way |
| 0x00590670 | 20 | folded | as MW3 0x0056f230: covered: a sound's play with the master volume (zsnd_play.cpp:134), the playing list's node |
| 0x005906a0 | 17 | folded | read: a 40-byte value copied into a new std::list node (the STL's construct), from the sound play 0x0058ff90's list inserts; MW3's 0x0056eb10 does it inline, the port's list does it itself |

### zimage (71 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00530b70 | 16 | folded | covered by the port where it names it (src/frontend/sdl/sdl_twod_host.h:36) |
| 0x00530bb0 | 12 | replaced (SDL or the platform layer) | as MW3 0x005172c0: zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00530bd0 | 12 | replaced (SDL or the platform layer) | as MW3 0x005172e0: zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00530bf0 | 11 | replaced (SDL or the platform layer) | as MW3 0x00517300: zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00530c00 | 26 | replaced (SDL or the platform layer) | as MW3 0x00517310: zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00530c60 | 47 | replaced (SDL or the platform layer) | as MW3 0x00517370: zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00530d10 | 48 | replaced (SDL or the platform layer) | as MW3 0x00517410: zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00530d90 | 61 | replaced (SDL or the platform layer) | as MW3 0x005174b0, whose class PM's changes keep: zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00530e70 | 28 | replaced (SDL or the platform layer) | as MW3 0x005175e0: zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00530ee0 | 16 | replaced (SDL or the platform layer) | as MW3 0x00517650: zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00530f10 | 18 | replaced (SDL or the platform layer) | as MW3 0x00517680: zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00530f40 | 9 | folded | covered by the port where it names it (src/frontend/sdl/sdl_twod_host.cpp:67) |
| 0x00530f50 | 8 | unlabelled |  |
| 0x00530f70 | 9 | folded | covered by the port where it names it (src/gamez/zimage/zimg_library.cpp:285) |
| 0x00530f80 | 14 | folded | covered by the port where it names it (src/gamez/zimage/zimg_library.cpp:338) |
| 0x00530fa0 | 9 | folded | covered by the port where it names it (src/gamez/zimage/zimg_library.cpp:284) |
| 0x00530fb0 | 13 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_cull.cpp:363) |
| 0x00530fd0 | 12 | folded | covered by the port where it names it (src/frontend/sdl/sdl_twod_host.cpp:67) |
| 0x00531000 | 25 | unlabelled |  |
| 0x005312c0 | 14 | folded | covered by the port where it names it (src/gamez/zimage/zimg_library.cpp:34) |
| 0x00531330 | 30 | folded | covered by the port where it names it (src/gamez/zimage/zimg_texture.cpp:305) |
| 0x005313b0 | 15 | folded | covered by the port where it names it (src/gamez/zimage/zimg_texture.cpp:290) |
| 0x005313e0 | 15 | folded | covered by the port where it names it (src/frontend/sdl/sdl_twod_host.cpp:173) |
| 0x00531410 | 12 | folded | covered by the port where it names it (src/gamez/zimage/zimg_texture.cpp:319) |
| 0x00531420 | 48 | folded | as MW3 0x005177a0: the texture cache by name and its teardown: g_cache (zimg_texture.cpp:296-321), a std::list freed with the program |
| 0x005314a0 | 19 | folded | covered by the port where it names it (src/gamez/zimage/zimg_texture.cpp:317) |
| 0x005314e0 | 61 | unlabelled |  |
| 0x005315c0 | 66 | unlabelled |  |
| 0x00531760 | 16 | folded | as MW3 0x00517be0: the texture cache by name and its teardown: g_cache (zimg_texture.cpp:296-321), a std::list freed with the program |
| 0x005317a0 | 9 | unlabelled | follows 0x005a4b90:  |
| 0x005317b0 | 9 | folded | a thunk: one call to another function |
| 0x005317e0 | 9 | unlabelled | follows 0x005a4b90:  |
| 0x005317f0 | 9 | folded | a thunk: one call to another function |
| 0x00531810 | 11 | debug or dev tool: PORT (preserve) | as MW3 0x00518050, whose class PM's changes keep: the texture directory writer (0x005180a0, 'Error writing texture directory.') and its entry-index helper, part of the world writers (gamez.md:127) |
| 0x00531840 | 11 | folded | covered by the port where it names it (src/pm/gamez/zclass/cls_zbd.cpp:29) |
| 0x00531860 | 8 | folded | returns a constant or a global |
| 0x00531870 | 53 | debug or dev tool: PORT (preserve) | as MW3 0x005180a0, whose class PM's changes keep: the texture directory writer (0x005180a0, 'Error writing texture directory.') and its entry-index helper, part of the world writers (gamez.md:127) |
| 0x00531c50 | 59 | replaced (SDL or the platform layer) | as MW3 0x005184e0, whose class PM's changes keep: the texture table's start-up: zeroes the table and makes the Direct3D default texture record; the port's GPU textures make their own (zimg_texture.cpp:109, fixes.md puffer row) |
| 0x00531d60 | 11 | folded | covered by the port where it names it (src/frontend/sdl/sdl_twod_host.cpp:174) |
| 0x00531e70 | 12 | folded | covered by the port where it names it (src/gamez/zimage/zimg_library.cpp:376) |
| 0x00531eb0 | 11 | folded | as MW3 0x005187a0: one-line setters/lookups the port does inline: zImage's library flag (no effect, app_engine.cpp:307), the texture group (mech_lib.cpp:34, app_mission.cpp:1910), the archive file test (zimg_library.cpp:20), the extension cut (zimg_texture.cpp:94) |
| 0x00531ed0 | 98 | unlabelled |  |
| 0x00532150 | 115 | unlabelled |  |
| 0x00532440 | 9 | folded | a thunk: one call to another function |
| 0x00532520 | 19 | unlabelled |  |
| 0x005325a0 | 113 | folded | read: PM's texture lookup by name in a library (from 0x005326d0, MW3's 0x00518db0, zimg_library.cpp): the name lowercased, then a binary search by strcmp over the entries, where MW3 scans with _stricmp; the same answers on the shipped data (all 28 PM image libraries' names are lowercase, sorted and without repeats, checked 2026-11-13). Only an unsorted or mixed-case library (a mod's) would differ: the original misses names the port finds |
| 0x005326d0 | 22 | unlabelled |  |
| 0x005327d0 | 27 | unlabelled |  |
| 0x00532860 | 9 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x00532870 | 8 | folded | returns a constant or a global |
| 0x00532a30 | 116 | folded | as MW3 0x00519150: one-line setters/lookups the port does inline: zImage's library flag (no effect, app_engine.cpp:307), the texture group (mech_lib.cpp:34, app_mission.cpp:1910), the archive file test (zimg_library.cpp:20), the extension cut (zimg_texture.cpp:94) |
| 0x00532b60 | 125 | software renderer | PM's mip chain, in place of MW3's 0x00519270 (link_mips, zimg_texture.cpp:195): MW3 links mips only for a name ending in '_1' and counts its digit up; PM appends '_1' to any texture's name (a 260-byte buffer, MW3's 64), loads each level by name and counts the last character up, and reports '%s: Mip file %s used externally' when a level is already loaded without pixels. Only the software renderer reads the mips (the GPU path does not; mip_scale has no reader), so on PM the port links none, and loads no level images |
| 0x00532d00 | 52 | software renderer | as MW3 0x00519370: the software renderer's texture tables: the nth mip of a chain for 0x00563940 (module-starts.md:158) and the 32 light and fog levels of every paletted texture (software-path.md:30) |
| 0x00532e80 | 36 | software renderer | as MW3 0x00519510: the software renderer's texture tables: the nth mip of a chain for 0x00563940 (module-starts.md:158) and the 32 light and fog levels of every paletted texture (software-path.md:30) |
| 0x00532f20 | 76 | software renderer | as MW3 0x005195b0, whose class PM's changes keep: the software renderer's texture tables: the nth mip of a chain for 0x00563940 (module-starts.md:158) and the 32 light and fog levels of every paletted texture (software-path.md:30) |
| 0x005330c0 | 29 | unlabelled |  |
| 0x00533150 | 26 | software renderer | called only from 0x00580be0, 0x00587d50, which are software |
| 0x005331b0 | 64 | replaced (SDL or the platform layer) | as MW3 0x00519820: the square-only device resize (D3D caps 0x20; render-drawtable.md:124): no modern device needs it |
| 0x00533320 | 19 | debug or dev tool: PORT (preserve) | read: PM's screenshot grab (the back buffer grabbed through 0x00598600(2), the pixel format's masks and shifts taken, converted by 0x005333e0), from 0x004af600 (not a Ghidra function: mw3%08x.bmp, the first free name), the handler of command 0x86 (MW3's 0x7e plus 8; 0x0040b9f0 registers it); no key reaches it (docs/controls.md, docs/editor.md), as its siblings 0x00533380..0x00533580 |
| 0x00533380 | 26 | debug or dev tool: PORT (preserve) | as MW3 0x005199e0: the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it |
| 0x005333c0 | 12 | debug or dev tool: PORT (preserve) | as MW3 0x00519a20: the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it |
| 0x005333e0 | 50 | debug or dev tool: PORT (preserve) | as MW3 0x00519a40: the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it |
| 0x005334d0 | 63 | debug or dev tool: PORT (preserve) | as MW3 0x00519b30: the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it |
| 0x00533580 | 19 | debug or dev tool: PORT (preserve) | as MW3 0x00519be0: the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it |
| 0x005335c0 | 24 | debug or dev tool: PORT (preserve) | as MW3 0x00519c20: the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it |
| 0x00533660 | 23 | unused: PORT (preserve) | as MW3 0x00519df0: zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4) |
| 0x005336c0 | 55 | unused: PORT (preserve) | as MW3 0x00519e50: zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4) |
| 0x005337f0 | 20 | unused: PORT (preserve) | as MW3 0x00519f70: zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4) |
| 0x00533820 | 50 | unused: PORT (preserve) | as MW3 0x00519fa0: zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4) |
| 0x00533900 | 28 | unused: PORT (preserve) | as MW3 0x0051a080: zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4) |
| 0x00533960 | 49 | unused: PORT (preserve) | as MW3 0x0051a0d0: zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4) |

### zreader (60 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005791b0 | 53 | unlabelled |  |
| 0x00579300 | 9 | folded | checked by test (block trace, 2026-11-13): test_pm_zreader's load by name 0x00578630 runs it on every entry of every shipped archive; the tree compared node by node |
| 0x00579310 | 9 | folded | checked by test (block trace, 2026-11-13): the reader's archive opened as the current one (0x00577e50); test_pm_zreader runs it on every shipped archive and compares the tree node by node |
| 0x00579330 | 9 | folded | a wrapper of 0x00577f00 (MW3 0x005573d0), which MW3's twin callers 0x00481fc0 and 0x00491780 call directly; not run under a test (trace) |
| 0x00579340 | 9 | folded | a thunk: one call to another function |
| 0x00579360 | 16 | unused: PORT (preserve) | as MW3 0x00557490: the defines list's constructor and destructor, no caller reaches them (zreader.md:103) |
| 0x00579390 | 9 | folded | a thunk: one call to another function |
| 0x00579410 | 14 | folded | as MW3 0x0056f570: zSound's static containers and their cleanup (vector and list heads made at start-up, the sets' pointer list freed); the port's members construct and free themselves |
| 0x00579880 | 90 | unlabelled |  |
| 0x00579aa0 | 9 | folded | as MW3 0x00557b10: start-up and stop plumbing with no effect in the port: zRender's two buffers freed at stop (app_engine.cpp:326), the pitch store that is only written (app_engine.cpp:311) |
| 0x00579ab0 | 10 | software renderer | as MW3 0x00557b20: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00579ac0 | 130 | software renderer | as MW3 0x00557b30: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00579ed0 | 9 | software renderer | as MW3 0x00557f50: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00579ef0 | 9 | software renderer | as MW3 0x00557f70: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0057a010 | 12 | folded | as MW3 0x00558070: start-up and stop plumbing with no effect in the port: zRender's two buffers freed at stop (app_engine.cpp:326), the pitch store that is only written (app_engine.cpp:311) |
| 0x0057a020 | 12 | replaced (SDL or the platform layer) | as MW3 0x00558080: the render target's size for the FMV and 2D layers: the hosts give it (sdl_twod_host.cpp:380, app_fmv.cpp:79) |
| 0x0057a070 | 21 | software renderer | as MW3 0x005580d0: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0057a0d0 | 13 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00558120: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x0057a0f0 | 13 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:148) |
| 0x0057a120 | 17 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:149) |
| 0x0057a170 | 30 | unlabelled |  |
| 0x0057a200 | 16 | folded | covered by the port where it names it (src/mech3/app_engine.cpp:328) |
| 0x0057a770 | 9 | MISSING LOGIC | pm only, renderer, effect to check: PM drops the pending screen tint after each frame's finish (0x0057a770, MW3's tint_clear 0x005587b0, which MW3 never calls; PM's core frame 0x004838b0 calls it after 0x0057a5d0 when [0x0062cf29] is clear). MW3 and the port keep a tint pending until another is set (zrndr_tint.cpp:137), so a tint set once shows on every later frame in the port and on one in PM; which tints are set once (not every frame) is to check |
| 0x0057c510 | 17 | unlabelled |  |
| 0x0057c950 | 47 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x0057ca30 | 266 | software renderer | as MW3 0x0055aa20, whose class PM's changes keep: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0057d210 | 21 | unlabelled |  |
| 0x0057d270 | 112 | software renderer | as MW3 0x0055b250: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0057dc00 | 294 | software renderer | as MW3 0x0055bbf0, whose class PM's changes keep: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0057e2f0 | 241 | software renderer | as MW3 0x0055c2f0, whose class PM's changes keep: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0057e900 | 289 | software renderer | as MW3 0x0055c900, whose class PM's changes keep: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0057eff0 | 239 | unlabelled |  |
| 0x0057f5e0 | 375 | software renderer | as MW3 0x0055d660, whose class PM's changes keep: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x005800e0 | 371 | software renderer | called only from 0x005883f0, 0x00588590, which are software |
| 0x00580be0 | 538 | software renderer | as MW3 0x0055efa0, whose class PM's changes keep: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00581c10 | 334 | software renderer | as MW3 0x00560010, whose class PM's changes keep: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x005824f0 | 476 | software renderer | as MW3 0x005608f0, whose class PM's changes keep: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00583370 | 463 | software renderer | as MW3 0x00561910, whose class PM's changes keep: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x005841e0 | 10 | folded | covered by the port where it names it (src/gamez/zrender/zrndr_blit.h:47) |
| 0x005844c0 | 10 | folded | covered by the port where it names it (src/gamez/zrender/zrndr_prim2d.cpp:37) |
| 0x00584544 | 9 | folded | a piece of the ported function before it (Ghidra split it there) (0x005844f0) |
| 0x00584570 | 18 | software renderer | as MW3 0x00562d20: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x005845e0 | 111 | software renderer | as MW3 0x00562d90, whose class PM's changes keep: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00585250 | 59 | unlabelled |  |
| 0x00585750 | 28 | unlabelled |  |
| 0x00585880 | 9 | folded | covered by the port where it names it (src/gamez/zrender/zrndr_prim2d.cpp:145) |
| 0x005858a0 | 12 | folded | as MW3 0x00564210: point-in-rect test, inline in the ported caller (zrndr_prim2d.cpp:21) |
| 0x005858d0 | 19 | folded | covered by the port where it names it (src/gamez/zrender/zrndr_prim2d.cpp:145) |
| 0x00585960 | 16 | folded | covered by the port where it names it (src/gamez/zrender/zrndr_prim2d.cpp:145) |
| 0x005859e0 | 42 | software renderer | as MW3 0x00564370: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00585ad0 | 34 | software renderer | as MW3 0x00564460: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00585b70 | 9 | software renderer | as MW3 0x00564500: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00585cc0 | 34 | software renderer | as MW3 0x00564660: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00585d30 | 16 | software renderer | as MW3 0x005646d0: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00585d70 | 104 | software renderer | as MW3 0x00564710: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x005871f0 | 134 | software renderer | as MW3 0x00565d30: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00587520 | 34 | software renderer | as MW3 0x00566070: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x005875b0 | 25 | software renderer | as MW3 0x00566110: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x005876d0 | 13 | software renderer | as MW3 0x00566230: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00587700 | 34 | unlabelled |  |

### zinterp (58 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005b6ec0 | 11 | folded | read: a setter of a global that nothing in PM reads (pm.asm: one or two writes, no read), so writing it or not changes nothing |
| 0x005b6ed0 | 11 | folded | read: a setter of a global that nothing in PM reads (pm.asm: one or two writes, no read), so writing it or not changes nothing |
| 0x005b6ee0 | 11 | folded | read: a setter of a global that nothing in PM reads (pm.asm: one or two writes, no read), so writing it or not changes nothing |
| 0x005b6ef0 | 12 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:2) |
| 0x005b6f80 | 10 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:305) |
| 0x005b6fc0 | 15 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:308) |
| 0x005b6ff0 | 97 | folded | as MW3 0x00594ec0: the script log line's formatter, covered where named (zinterp.cpp:288) |
| 0x005b70d0 | 39 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:326) |
| 0x005b7310 | 26 | unlabelled |  |
| 0x005b7380 | 9 | folded | a thunk: one call to another function |
| 0x005b7430 | 11 | folded | inside its ported callers (0x005b2d10): a small helper their ports do their own way |
| 0x005b7c40 | 29 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:432) |
| 0x005b7ca0 | 28 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:433) |
| 0x005b7db0 | 65 | unlabelled |  |
| 0x005b7ee0 | 18 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:290) |
| 0x005b7f60 | 15 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:294) |
| 0x005b7f80 | 13 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:434) |
| 0x005b80c0 | 28 | unlabelled |  |
| 0x005b8140 | 30 | unlabelled |  |
| 0x005b8210 | 26 | debug or dev tool: PORT (preserve) | as MW3 0x005960a0: the hidden GameZ Interpreter console (0x005960a0: a 2D console window that runs typed script lines through 0x00595510; its destructor and line runner; toggled by 0x004810d0, which nothing reaches: interp.md:532, discoveries.md:1532) |
| 0x005b82c0 | 12 | unlabelled |  |
| 0x005b82e0 | 12 | debug or dev tool: PORT (preserve) | as MW3 0x00596170: the hidden GameZ Interpreter console (0x005960a0: a 2D console window that runs typed script lines through 0x00595510; its destructor and line runner; toggled by 0x004810d0, which nothing reaches: interp.md:532, discoveries.md:1532) |
| 0x005b8300 | 14 | debug or dev tool: PORT (preserve) | as MW3 0x00596190: the hidden GameZ Interpreter console (0x005960a0: a 2D console window that runs typed script lines through 0x00595510; its destructor and line runner; toggled by 0x004810d0, which nothing reaches: interp.md:532, discoveries.md:1532) |
| 0x005b8350 | 9 | debug or dev tool: PORT (preserve) | called only from 0x005b8300, which are debug |
| 0x005b8360 | 9 | folded | a thunk: one call to another function |
| 0x005b8450 | 10 | unlabelled |  |
| 0x005b8570 | 34 | unlabelled |  |
| 0x005b8690 | 95 | unlabelled |  |
| 0x005b8980 | 48 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x005975e0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x005b8a20 | 91 | software renderer | as MW3 0x00597680: SetPaletteName's palette brightness step for the 8-bit palette (0x005975e0's helper; only the software path uses a palette) |
| 0x005b91c0 | 22 | replaced (SDL or the platform layer) | as MW3 0x00597e60: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9210 | 12 | replaced (SDL or the platform layer) | as MW3 0x00597eb0: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9230 | 10 | replaced (SDL or the platform layer) | as MW3 0x00597ed0: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9250 | 39 | replaced (SDL or the platform layer) | as MW3 0x00597ef0: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9290 | 18 | replaced (SDL or the platform layer) | as MW3 0x00597f30: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b92c0 | 13 | replaced (SDL or the platform layer) | as MW3 0x00597f60: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9330 | 53 | replaced (SDL or the platform layer) | as MW3 0x00597fc0, whose class PM's changes keep: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9480 | 24 | replaced (SDL or the platform layer) | as MW3 0x00598100: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b94d0 | 11 | replaced (SDL or the platform layer) | as MW3 0x00598150: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b94e0 | 12 | replaced (SDL or the platform layer) | as MW3 0x00598160: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9510 | 16 | replaced (SDL or the platform layer) | as MW3 0x00598190: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9530 | 17 | replaced (SDL or the platform layer) | as MW3 0x005981c0: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9560 | 12 | replaced (SDL or the platform layer) | as MW3 0x005981e0: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b96a0 | 38 | replaced (SDL or the platform layer) | as MW3 0x00598330: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b96d0 | 35 | folded | covered by the port where it names it (src/gamez/zutil/zutl_reg.h:42) |
| 0x005b9740 | 62 | replaced (SDL or the platform layer) | as MW3 0x005983c0: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9820 | 10 | replaced (SDL or the platform layer) | as MW3 0x005984a0: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9860 | 8 | folded | returns a constant or a global |
| 0x005b98b0 | 8 | folded | returns a constant or a global |
| 0x005b9900 | 35 | replaced (SDL or the platform layer) | as MW3 0x00598580: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9950 | 56 | replaced (SDL or the platform layer) | as MW3 0x005985d0: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9a40 | 54 | replaced (SDL or the platform layer) | as MW3 0x005986c0: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9b50 | 58 | replaced (SDL or the platform layer) | as MW3 0x005987d0: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9c30 | 110 | replaced (SDL or the platform layer) | as MW3 0x005988c0: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9e60 | 12 | replaced (SDL or the platform layer) | as MW3 0x00598ae0: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9e80 | 13 | replaced (SDL or the platform layer) | as MW3 0x00598b00: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005b9eb0 | 68 | replaced (SDL or the platform layer) | as MW3 0x00598b30: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005ba010 | 10 | replaced (SDL or the platform layer) | as MW3 0x00598c80: the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |

### zeffect (51 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00520800 | 79 | unlabelled |  |
| 0x00520960 | 11 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x00507dd0: a script engine command (zinterp.h) the port leaves null (hook_allow: counted): no shipped script uses it (the script census: 0 skipped). The commands build or change the world at run time (worlds, partitions, cameras, windows, displays, sequences, lights, models, palettes); for a mod's scripts |
| 0x00520970 | 118 | unlabelled |  |
| 0x00520bc0 | 49 | unlabelled |  |
| 0x00520f10 | 72 | folded | group A, read: the same logic as MW3's 0x00508310 (both walk the +0x138 / PM +0x108 parent chain while state 2); PM keeps the def's run block behind a pointer (+0xd0, its +0x38 and +0x20) that MW3 holds in place (+0x100, +0xe8: one block at +0xc8) |
| 0x00521050 | 22 | folded | read: MW3's 0x00508460 (an animation restarted: state 6 to 2, 4 to 3, then 0x005210a0 = MW3's 0x00508490) with PM's offsets (+0xa0 for +0x98), unpaired by the diff; the port's zeff::anim_restart (mech_data.h:74, app_weapons.cpp:392) |
| 0x005210d0 | 11 | folded | inside its ported callers (0x0051f900): a small helper their ports do their own way |
| 0x00521100 | 45 | unlabelled |  |
| 0x005213a0 | 104 | unlabelled |  |
| 0x00521590 | 89 | unlabelled |  |
| 0x005216e0 | 12 | folded | covered by the port where it names it (src/mech3/app_weapons.cpp:347) |
| 0x00521870 | 10 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x00521a20 | 10 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x00521b80 | 10 | folded | covered by the port where it names it (src/gamez/zeffect/zeff_anim_run.h:85) |
| 0x00521bd0 | 8 | folded | ported, by docs/symbols.csv: ported as the hooks' anim_callback: +0x6c |
| 0x00521be0 | 8 | folded | ported, by docs/symbols.csv: ported as the hooks' anim_record: +0x70 |
| 0x00521c70 | 56 | folded | read: the animations' event handler table filled (a pointer per event type), PM's dispatch for the post-load pass; the port dispatches by the event's type (zeff_anim_events.cpp) |
| 0x00521f80 | 9 | folded | traced: the animations' world, set by PM's mission load (0x00492830) to world1; its reader is the fog event (0x0051c2f0: the world's fog type, colour, altitude and range), which the port runs on its own world (zeff_anim_events.cpp:1120) |
| 0x00521f90 | 49 | unlabelled |  |
| 0x00522160 | 53 | unlabelled |  |
| 0x00522280 | 52 | unlabelled |  |
| 0x00522390 | 47 | unlabelled |  |
| 0x00522670 | 436 | unlabelled |  |
| 0x00522ea0 | 164 | unlabelled |  |
| 0x00524300 | 29 | unlabelled |  |
| 0x00524380 | 56 | unlabelled |  |
| 0x00524480 | 103 | unlabelled |  |
| 0x00524492 | 99 | unlabelled | follows 0x00524480:  |
| 0x00524710 | 119 | unlabelled |  |
| 0x00524990 | 34 | unlabelled |  |
| 0x00524a20 | 64 | unlabelled |  |
| 0x00524b50 | 48 | unlabelled |  |
| 0x00524be0 | 21 | folded | read: MW3's 0x0050b4e0 (an animation definition found by its name's first n letters and its owner, the unloaded (state 5) left out) with PM's record (0x10c bytes, owner +0x48) and its arguments on the stack; from the AI target 0x004e2820 (MW3's 0x004d25c0) |
| 0x00524c60 | 28 | unlabelled |  |
| 0x00524cf0 | 11 | folded | covered by the port where it names it (src/mech3/zai_target.h:261) |
| 0x00524d10 | 11 | folded | covered by the port where it names it (src/mech3/zai_target.h:312) |
| 0x00524d40 | 13 | folded | covered by the port where it names it (src/gamez/zeffect/zeff_anim_init.cpp:753) |
| 0x00525090 | 169 | unlabelled |  |
| 0x00525460 | 39 | unlabelled |  |
| 0x00525fe0 | 55 | unlabelled |  |
| 0x00526970 | 12 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x00526980 | 12 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x00526a40 | 113 | unlabelled |  |
| 0x00526da0 | 28 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x0050d610: an effect entry's node and gfx data (zeff_init.cpp:24: no shipped effects file has one): the node made, placed and ticking, its distance test |
| 0x00526df0 | 19 | folded | inside its ported callers (0x0051b500, 0x005a27b0, 0x005a3df0, 0x005a6c60): a small helper their ports do their own way |
| 0x00526e50 | 43 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x0050d6a0: an effect entry's node and gfx data (zeff_init.cpp:24: no shipped effects file has one): the node made, placed and ticking, its distance test |
| 0x00526f60 | 15 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x0050d7a0: an effect entry's node and gfx data (zeff_init.cpp:24: no shipped effects file has one): the node made, placed and ticking, its distance test |
| 0x00526fe0 | 21 | folded | covered by the port where it names it (src/gamez/zeffect/zeff_init.cpp:50) |
| 0x00527040 | 36 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x0050d880: an effect entry's node and gfx data (zeff_init.cpp:24: no shipped effects file has one): the node made, placed and ticking, its distance test |
| 0x005270e0 | 42 | unlabelled |  |
| 0x00527230 | 13 | folded | read: a setter of a global that nothing in PM reads (pm.asm: one or two writes, no read), so writing it or not changes nothing |

### zentity (44 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005cad60 | 54 | unlabelled |  |
| 0x005cae90 | 44 | folded | as MW3 0x005a8210: zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005caf00 | 27 | folded | covered by the port where it names it (src/mech3/app_pocket.cpp:123) |
| 0x005caf50 | 14 | folded | as MW3 0x005a82e0: zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005caf80 | 17 | folded | covered by the port where it names it (src/mech3/mech_build.cpp:288) |
| 0x005cafc0 | 12 | unlabelled |  |
| 0x005cafe0 | 10 | folded | covered by the port where it names it (src/mech3/veh_basic_object.cpp:285) |
| 0x005cb140 | 11 | folded | as MW3 0x005a84c0: zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005cb160 | 10 | folded | covered by the port where it names it (src/mech3/mech_place.cpp:110) |
| 0x005cb1c0 | 8 | folded | returns a constant or a global |
| 0x005cb1d0 | 10 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:530) |
| 0x005cb200 | 10 | folded | covered by the port where it names it (src/mech3/mech_place.cpp:108) |
| 0x005cb220 | 10 | folded | as MW3 0x005a85a0: zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005cb310 | 8 | folded | covered by the port where it names it (src/mech3/mech.h:126) |
| 0x005cb320 | 8 | folded | inside its ported callers (0x004e37d0): a small helper their ports do their own way |
| 0x005cb330 | 14 | folded | covered by the port where it names it (src/mech3/mech_place.cpp:116) |
| 0x005cb360 | 15 | folded | covered by the port where it names it (src/mech3/mech.h:95) |
| 0x005cb3a0 | 11 | folded | covered by the port where it names it (src/mech3/veh_controls.h:82) |
| 0x005cb3c0 | 11 | folded | as MW3 0x005a8730: zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005cb3e0 | 49 | MISSING LOGIC | PM's triangle stripper (0x00570b70 and its std::map / set code, about 1,960 instructions): a model without flag 0x80 made into strips (0x2000) under Direct3D. On world models it changes nothing for shipped data (pm-map.md, the models' form; test_pm_strips.py). But PM's mechlib post-pass 0x00510ab0 (MW3's 0x004fc8b0) runs it on every mech model in hardware mode with no vertex limit, as MW3's 0x00552540 does with 4: the same gap as MW3's stripper (docs/mechlib.md), to measure on PM's mechlib |
| 0x005cb4b0 | 11 | UNSURE | read with group A: a callee of shared ports PM changed (0x00494b80); whether the port does PM's version, this call included, is settled there |
| 0x005cb4e0 | 15 | folded | covered by the port where it names it (src/mech3/app_mech.h:341) |
| 0x005cb510 | 15 | folded | covered by the port where it names it (src/mech3/app_mech.h:342) |
| 0x005cb690 | 34 | folded | as MW3 0x005a89c0: zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005cb6f0 | 117 | unlabelled |  |
| 0x005cb930 | 9 | folded | a thunk: one call to another function |
| 0x005cbaa0 | 39 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:75) |
| 0x005cbb40 | 8 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:108) |
| 0x005cbb60 | 77 | folded | as MW3 0x005a8cb0: zSave's list insert, load and stable sort, covered where named (zsave.cpp:86, 108, 132) and std::list's sort helpers |
| 0x005cbc30 | 33 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:102) |
| 0x005cbcc0 | 137 | folded | as MW3 0x005a8e40: zSave's list insert, load and stable sort, covered where named (zsave.cpp:86, 108, 132) and std::list's sort helpers |
| 0x005cbfa0 | 23 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:95) |
| 0x005cc010 | 11 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:112) |
| 0x005cc030 | 13 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:175) |
| 0x005cc070 | 22 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:191) |
| 0x005cc0f0 | 14 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:212) |
| 0x005cc130 | 9 | unused: PORT (preserve) | as MW3 0x005a92d0: _rmtmp at zSave's stop, unreached |
| 0x005cc140 | 135 | unlabelled |  |
| 0x005cc3a0 | 9 | folded | as MW3 0x0049ccb0: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x005cc3e0 | 52 | folded | as MW3 0x005a96d0: zSave's list insert, load and stable sort, covered where named (zsave.cpp:86, 108, 132) and std::list's sort helpers |
| 0x005cc4c0 | 17 | folded | as MW3 0x005a97e0: zSave's list insert, load and stable sort, covered where named (zsave.cpp:86, 108, 132) and std::list's sort helpers |
| 0x005cc5b0 | 17 | debug or dev tool: PORT (preserve) | as MW3 0x005a98d0: the developers' loose <name>.zan override for motion scripts (motion.md:36), like the reader's loose .zrd files |
| 0x005cc600 | 25 | folded | covered by the port where it names it (src/gamez/zsiscript/si_script.cpp:135) |
| 0x005cc650 | 11 | folded | covered by the port where it names it (src/gamez/zsave/zsave.h:8) |

### zdeclient (42 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00517ce0 | 53 | unlabelled |  |
| 0x00517e90 | 179 | unlabelled |  |
| 0x00518350 | 290 | unlabelled |  |
| 0x005189f0 | 12 | folded | a thunk: one call to another function |
| 0x00518a10 | 9 | folded | a thunk: one call to another function |
| 0x00518ab0 | 15 | unlabelled |  |
| 0x00518ad0 | 9 | folded | a thunk: one call to another function |
| 0x00518c60 | 102 | unlabelled |  |
| 0x005190d0 | 44 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00519190 | 58 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x005192c0 | 58 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x005193e0 | 318 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00519910 | 31 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x005199a0 | 197 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00519cb0 | 47 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00519d60 | 51 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00519e20 | 28 | folded | as MW3 0x00501a40: small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x00519e60 | 24 | folded | as MW3 0x00501a80: small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x00519e90 | 17 | folded | read: a 13-word record copy (nothing for a null slot), container code |
| 0x00519eb0 | 182 | unlabelled |  |
| 0x0051a6d0 | 179 | unlabelled |  |
| 0x0051ab70 | 280 | unlabelled |  |
| 0x0051b1f0 | 8 | folded | returns a constant or a global |
| 0x0051b270 | 9 | folded | the same as its MW3 twin (identical, or passed by the feature compare 7f6e9e10), so it is what MW3's twin is in the port |
| 0x0051b280 | 13 | folded | covered by the port where it names it (src/mech3/zai_target.h:258) |
| 0x0051b2a0 | 13 | folded | covered by the port where it names it (src/mech3/zai_target.h:259) |
| 0x0051b2c0 | 20 | unlabelled |  |
| 0x0051b340 | 17 | MISSING LOGIC (only data the shipped game lacks reaches it) | pm only, traced: PM's damage handler on an animation (0x0051b2c0, MW3's 0x00502df0, anim_on_collide zeff_anim_run.cpp:832) first runs 0x0051b340 on every hit: when the def has its second sequence (+0xd4, with events) it stamps the time ([0x008d3f68]), clears +0xb0 and runs that sequence from its start (0x0051fc00, 0x00520800: MW3's 0x00506f40, 0x005079b0); the port reads the sequence (AnimDef::pm_seq2, pm/gamez/zeffect/zeff_anim_init.cpp:126) but never runs it, and no shipped PM animation has one, so only a mod's would show it |
| 0x0051c2f0 | 21 | unlabelled |  |
| 0x0051c4d0 | 126 | unlabelled |  |
| 0x0051cd60 | 477 | unlabelled |  |
| 0x0051deb0 | 23 | folded | covered by the port where it names it (src/gamez/zeffect/zeff_anim_events.cpp:625) |
| 0x0051df10 | 138 | folded | read: covered where the port names it (briefing.cpp:513 and 871 PM's loadouts, pilot.h:346 the standalone rows, mech_damage.cpp:225 the headlight, app_hud.cpp:534 the radar's front, mech_config.cpp:951, pilot_games.cpp:635, mech_headlight.cpp:50, mech_waynet.h:70, mission_world.cpp:249, app_vehicle.cpp:623 the stealth controls, zai_brain.cpp:239, zainet_inout.cpp:382, zai_node.cpp:146, zai_walk_pm.cpp:441, zai_drive_pm.cpp:772, zrdraiv.cpp:581, mech_build.cpp:283, app_pocket.cpp:172, zrdraiv.h:66, zai_brain.cpp:1033, pm zeff_anim_events.h:28, zsnd_3d.cpp:27, zsnd_play.cpp:203 and 221) |
| 0x0051e720 | 14 | unlabelled |  |
| 0x0051e750 | 48 | unlabelled |  |
| 0x0051e890 | 42 | unlabelled |  |
| 0x0051f750 | 40 | unlabelled |  |
| 0x0051f830 | 43 | unlabelled |  |
| 0x0051fce0 | 145 | unlabelled |  |
| 0x00520080 | 53 | unlabelled |  |
| 0x00520500 | 100 | MISSING LOGIC (only data the shipped game lacks reaches it) | as MW3 0x005076c0, whose class PM's changes keep: anim event 37, the fbfx screen wave (0x005076c0 -> 0x005a6760 -> 0x005a6650 / 0x005a5400: up to four 2D waves): the port's hook only logs (app_mission.cpp:2108); no shipped anim.zbd has event 37 (anim_walk: c1..t1 have 0; 38 also 0) |
| 0x005207b0 | 8 | folded | returns a constant or a global |

### zfmv (41 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005ba110 | 450 | unlabelled |  |
| 0x005ba7f0 | 16 | unlabelled |  |
| 0x005ba830 | 12 | unlabelled |  |
| 0x005ba850 | 19 | unlabelled |  |
| 0x005ba8a0 | 11 | unlabelled |  |
| 0x005ba960 | 47 | unlabelled |  |
| 0x005baa70 | 14 | folded | ported, by docs/symbols.csv: ported: timeGetTime() x 0.001f |
| 0x005baab0 | 14 | folded | ported, by docs/symbols.csv: ported |
| 0x005baaf0 | 9 | folded | ported, by docs/symbols.csv: ported: 0x00598d30(flag) |
| 0x005bab80 | 12 | unlabelled |  |
| 0x005bac80 | 9 | unlabelled |  |
| 0x005bacb0 | 33 | unlabelled |  |
| 0x005badd0 | 42 | unlabelled |  |
| 0x005baec0 | 12 | unlabelled |  |
| 0x005bb070 | 28 | folded | ported, by docs/symbols.csv: ported: 0x0055a540(1) |
| 0x005bb100 | 28 | folded | ported, by docs/symbols.csv: ported: 0x0055a540(2) |
| 0x005bb190 | 12 | folded | inside its ported callers (0x005ba110): a small helper their ports do their own way |
| 0x005bb1b0 | 8 | unlabelled |  |
| 0x005bb1d0 | 13 | unlabelled |  |
| 0x005bb3d0 | 37 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:632) |
| 0x005bb510 | 19 | folded | covered by the port where it names it (src/gamez/ztwod/twod_shapes.cpp:159) |
| 0x005bb540 | 37 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:632) |
| 0x005bb680 | 19 | folded | covered by the port where it names it (src/gamez/ztwod/twod_shapes.cpp:159) |
| 0x005bb7c0 | 12 | unlabelled |  |
| 0x005bba70 | 33 | unlabelled |  |
| 0x005bbcd0 | 12 | unlabelled |  |
| 0x005bbcf0 | 86 | unlabelled |  |
| 0x005bca10 | 105 | unlabelled |  |
| 0x005bccc0 | 316 | unlabelled |  |
| 0x005bd7b0 | 12 | unlabelled |  |
| 0x005bdb30 | 179 | unlabelled |  |
| 0x005be050 | 12 | unlabelled |  |
| 0x005be0c0 | 19 | unlabelled |  |
| 0x005be3f0 | 112 | unlabelled |  |
| 0x005be700 | 68 | unlabelled |  |
| 0x005be8e0 | 19 | unlabelled |  |
| 0x005be940 | 9 | unlabelled | follows 0x005be950:  |
| 0x005be950 | 68 | unlabelled |  |
| 0x005beb50 | 52 | unlabelled |  |
| 0x005beca0 | 12 | unlabelled |  |
| 0x005becd0 | 38 | unlabelled |  |

### zweapon (33 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005a52e0 | 521 | unlabelled |  |
| 0x005a62a0 | 74 | unlabelled |  |
| 0x005a6410 | 9 | unlabelled | follows 0x005a6420:  |
| 0x005a6420 | 63 | unlabelled |  |
| 0x005a6550 | 9 | unlabelled | follows 0x005a6420:  |
| 0x005a6650 | 42 | unlabelled |  |
| 0x005a69f0 | 9 | folded | covered by the port where it names it (src/gamez/zweapon/zwep_init.h:250) |
| 0x005a6a00 | 37 | folded | group A, read: the same logic as MW3's 0x00584df0 on PM's 8-byte-longer effect block (flyout model +0x110, model animation +0x100): a pooled copy when animated, then under the shot's node; PM passes the add-child's arguments where MW3 leaves them in registers |
| 0x005a6c20 | 20 | unlabelled |  |
| 0x005a6c60 | 286 | MISSING LOGIC | pm only, group A, read: PM's shot spawn plays every FIRE animation (choice 0 all, k the k-th; attached when flag bit 1 and an attach node), then sets the choice back to 0; the port plays one, chosen 0 or 1 (zwep_shot.cpp:340-352). Goes with the reader 0x005a6700 (MW3 0x00585010) |
| 0x005a7570 | 36 | MISSING LOGIC | pm only, minor: PM retires a dead shot a frame late: its retire (MW3 0x00586600) first stamps the shot (+0x638) with zClass's frame count [0x008c4b60, MW3 0x0072f338] + 1 and returns 0, and its def's shot walk (0x005a7570, new in PM) keeps it linked until a later call finds the stamp matching; so the node stays in the world at its last place, its flyout animations run one more frame and the pool slot comes back a frame later. The port retires at once, as MW3 (zwep_shot.cpp:455-466, 491-501) |
| 0x005a75c0 | 129 | unlabelled |  |
| 0x005a79a0 | 162 | unlabelled |  |
| 0x005a8090 | 58 | unlabelled |  |
| 0x005a8230 | 58 | unlabelled |  |
| 0x005a8430 | 29 | unlabelled |  |
| 0x005a8510 | 71 | unlabelled |  |
| 0x005a8820 | 20 | unlabelled |  |
| 0x005a88b0 | 30 | folded | covered by the port where it names it (src/mech3/app_engine.h:59) |
| 0x005a8a90 | 16 | folded | read: a static list head made (zWeapon's list at [0x008cdcd4], empty, its count 0): container start-up; the port's list is its own |
| 0x005a8ac0 | 9 | folded | a thunk: one call to another function |
| 0x005a8d20 | 16 | replaced (SDL or the platform layer) | as MW3 0x00587ca0, whose class PM's changes keep: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005a8d50 | 9 | folded | a thunk: one call to another function |
| 0x005a8dd0 | 9 | folded | follows 0x0040d510: as MW3 0x0042a570: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x005a8de0 | 9 | folded | a thunk: one call to another function |
| 0x005a8e10 | 9 | unlabelled | follows 0x005a4b90:  |
| 0x005a8e20 | 9 | folded | a thunk: one call to another function |
| 0x005a8e40 | 28 | folded | covered by the port where it names it (src/gamez/znetwork/znet_session.cpp:248) |
| 0x005a8ec0 | 34 | folded | covered by the port where it names it (src/gamez/znetwork/znet_session.cpp:248) |
| 0x005a8f30 | 42 | folded | as MW3 0x00587ed0: covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x005a8fc0 | 78 | replaced (SDL or the platform layer) | as MW3 0x00587fb0: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005a9090 | 55 | replaced (SDL or the platform layer) | as MW3 0x00588070: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005a9150 | 19 | replaced (SDL or the platform layer) | as MW3 0x00588130: DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |

### zgame (33 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005b1550 | 140 | unlabelled |  |
| 0x005b18b5 | 11 | unlabelled | follows 0x005b1550:  |
| 0x005b18d3 | 58 | replaced (SDL or the platform layer) | as MW3 0x00590120: the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005b1964 | 11 | unlabelled | follows 0x005b1550:  |
| 0x005b19c0 | 17 | unlabelled |  |
| 0x005b19e0 | 102 | unlabelled |  |
| 0x005b1bd0 | 99 | unlabelled |  |
| 0x005b1db0 | 101 | unlabelled |  |
| 0x005b1fa0 | 42 | folded | as MW3 0x00590780: zGameApp's small helpers inside ported callers or static teardown: a buffer cursor's init (0x00590850, from 0x00590250 / 0x00590400 / 0x005905a0), its drain at exit, the registry company name 'Zipper Interactive' (zutl_reg's company), two destructors |
| 0x005b2050 | 74 | UNSURE | read with group A: a callee of shared ports PM changed (0x005b19e0, 0x005b1bd0, 0x005b1db0); whether the port does PM's version, this call included, is settled there |
| 0x005b21b0 | 31 | folded | inside its ported callers (0x005b19e0, 0x005b1bd0, 0x005b1db0): a small helper their ports do their own way |
| 0x005b2220 | 12 | folded | as MW3 0x00590850: zGameApp's small helpers inside ported callers or static teardown: a buffer cursor's init (0x00590850, from 0x00590250 / 0x00590400 / 0x005905a0), its drain at exit, the registry company name 'Zipper Interactive' (zutl_reg's company), two destructors |
| 0x005b2260 | 11 | unlabelled |  |
| 0x005b2280 | 26 | replaced (SDL or the platform layer) | as MW3 0x00590880: the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005b22e0 | 8 | replaced (SDL or the platform layer) | as MW3 0x005908e0: the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005b22f0 | 8 | replaced (SDL or the platform layer) | as MW3 0x00590900: the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005b2300 | 15 | replaced (SDL or the platform layer) | as MW3 0x00590920: the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005b2340 | 12 | unlabelled |  |
| 0x005b2360 | 27 | replaced (SDL or the platform layer) | as MW3 0x00590980, whose class PM's changes keep: the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005b23e0 | 14 | replaced (SDL or the platform layer) | as MW3 0x005909f0: the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005b2400 | 9 | folded | as MW3 0x00590a10: zGameApp's small helpers inside ported callers or static teardown: a buffer cursor's init (0x00590850, from 0x00590250 / 0x00590400 / 0x005905a0), its drain at exit, the registry company name 'Zipper Interactive' (zutl_reg's company), two destructors |
| 0x005b2440 | 41 | replaced (SDL or the platform layer) | as MW3 0x00590a50: the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005b24f0 | 10 | replaced (SDL or the platform layer) | as MW3 0x00590b40: the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005b2510 | 15 | replaced (SDL or the platform layer) | as MW3 0x00590b60: the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005b2530 | 22 | replaced (SDL or the platform layer) | as MW3 0x00590b80: the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005b2580 | 13 | replaced (SDL or the platform layer) | as MW3 0x00590bd0: the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005b2620 | 14 | folded | covered by the port where it names it (src/gamez/zgame/zgameapp.cpp:182) |
| 0x005b2650 | 12 | unlabelled |  |
| 0x005b2670 | 20 | folded | as MW3 0x00590cc0: zGameApp's small helpers inside ported callers or static teardown: a buffer cursor's init (0x00590850, from 0x00590250 / 0x00590400 / 0x005905a0), its drain at exit, the registry company name 'Zipper Interactive' (zutl_reg's company), two destructors |
| 0x005b26c0 | 12 | unlabelled |  |
| 0x005b26e0 | 20 | folded | as MW3 0x00590d30: zGameApp's small helpers inside ported callers or static teardown: a buffer cursor's init (0x00590850, from 0x00590250 / 0x00590400 / 0x005905a0), its drain at exit, the registry company name 'Zipper Interactive' (zutl_reg's company), two destructors |
| 0x005b2830 | 231 | unlabelled |  |
| 0x005b2c60 | 16 | debug or dev tool: PORT (preserve) | the script command PrintChildren (PM only): the current node's children's names to the console ('  %s'); no shipped PM script uses it, the port's interpreter does not know it |

### zutil (31 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00597340 | 31 | unlabelled |  |
| 0x005974e0 | 20 | unlabelled |  |
| 0x00597b70 | 34 | folded | covered by the port where it names it (src/gamez/zutil/zutl_zar.cpp:187) |
| 0x00597c10 | 9 | folded | as MW3 0x00576c50: zUtil and zVideo entries the port does its own way, covered where named: the CRC table's start (app_engine.cpp:314), the search paths' free (app_engine.cpp:323), the mode switch (video_set_mode, app_engine.cpp:724), the frame and lab clears (app_engine.h:196, 562) |
| 0x00597c20 | 9 | folded | as MW3 0x00576c60: zUtil and zVideo entries the port does its own way, covered where named: the CRC table's start (app_engine.cpp:314), the search paths' free (app_engine.cpp:323), the mode switch (video_set_mode, app_engine.cpp:724), the frame and lab clears (app_engine.h:196, 562) |
| 0x00597c90 | 8 | folded | returns a constant or a global |
| 0x00597ca0 | 18 | replaced (SDL or the platform layer) | read: zVideo's display entries through the renderer table (0x00b09ec4.., the mode set with its fallback, the surface locks and unlocks of the two buffers, a flip counted per frame), MW3's 0x00576c50..0x00576f90 family unpaired by the diff (labelled there: the port does its display its own way, the platform layer); func2file places them by zutl_zar.cpp, but they are video code |
| 0x00597ce0 | 8 | folded | returns a constant or a global |
| 0x00597cf0 | 8 | folded | returns a constant or a global |
| 0x00597d00 | 8 | folded | returns a constant or a global |
| 0x00597d10 | 8 | folded | returns a constant or a global |
| 0x00597d20 | 9 | replaced (SDL or the platform layer) | read: zVideo's display entries through the renderer table (0x00b09ec4.., the mode set with its fallback, the surface locks and unlocks of the two buffers, a flip counted per frame), MW3's 0x00576c50..0x00576f90 family unpaired by the diff (labelled there: the port does its display its own way, the platform layer); func2file places them by zutl_zar.cpp, but they are video code |
| 0x00597d30 | 9 | replaced (SDL or the platform layer) | read: zVideo's display entries through the renderer table (0x00b09ec4.., the mode set with its fallback, the surface locks and unlocks of the two buffers, a flip counted per frame), MW3's 0x00576c50..0x00576f90 family unpaired by the diff (labelled there: the port does its display its own way, the platform layer); func2file places them by zutl_zar.cpp, but they are video code |
| 0x00597dc0 | 9 | replaced (SDL or the platform layer) | read: zVideo's display entries through the renderer table (0x00b09ec4.., the mode set with its fallback, the surface locks and unlocks of the two buffers, a flip counted per frame), MW3's 0x00576c50..0x00576f90 family unpaired by the diff (labelled there: the port does its display its own way, the platform layer); func2file places them by zutl_zar.cpp, but they are video code |
| 0x00597dd0 | 8 | folded | returns a constant or a global |
| 0x00597de0 | 8 | folded | returns a constant or a global |
| 0x00597df0 | 8 | folded | returns a constant or a global |
| 0x00597e00 | 8 | folded | returns a constant or a global |
| 0x00597e10 | 8 | folded | returns a constant or a global |
| 0x00597e20 | 8 | folded | returns a constant or a global |
| 0x00597e30 | 9 | replaced (SDL or the platform layer) | read: zVideo's display entries through the renderer table (0x00b09ec4.., the mode set with its fallback, the surface locks and unlocks of the two buffers, a flip counted per frame), MW3's 0x00576c50..0x00576f90 family unpaired by the diff (labelled there: the port does its display its own way, the platform layer); func2file places them by zutl_zar.cpp, but they are video code |
| 0x00597e50 | 17 | replaced (SDL or the platform layer) | as MW3 0x00576de0: DirectDraw's back buffer: lock and unlock (app_engine.h:560, app_fmv.cpp:53), their raw forms, the flip (sdl_platform.h:68), the restore, the window's style for full screen, the size clamp |
| 0x00597ef0 | 9 | replaced (SDL or the platform layer) | read: zVideo's display entries through the renderer table (0x00b09ec4.., the mode set with its fallback, the surface locks and unlocks of the two buffers, a flip counted per frame), MW3's 0x00576c50..0x00576f90 family unpaired by the diff (labelled there: the port does its display its own way, the platform layer); func2file places them by zutl_zar.cpp, but they are video code |
| 0x00597f00 | 8 | folded | returns a constant or a global |
| 0x00597f10 | 8 | folded | returns a constant or a global |
| 0x00597f20 | 9 | replaced (SDL or the platform layer) | called only from 0x00598600, which are sdl |
| 0x00597f30 | 9 | replaced (SDL or the platform layer) | called only from 0x00598600, which are sdl |
| 0x00597f40 | 12 | replaced (SDL or the platform layer) | read: zVideo's display entries through the renderer table (0x00b09ec4.., the mode set with its fallback, the surface locks and unlocks of the two buffers, a flip counted per frame), MW3's 0x00576c50..0x00576f90 family unpaired by the diff (labelled there: the port does its display its own way, the platform layer); func2file places them by zutl_zar.cpp, but they are video code |
| 0x00597f80 | 12 | replaced (SDL or the platform layer) | as MW3 0x00576ed0: DirectDraw's back buffer: lock and unlock (app_engine.h:560, app_fmv.cpp:53), their raw forms, the flip (sdl_platform.h:68), the restore, the window's style for full screen, the size clamp |
| 0x00597f90 | 24 | replaced (SDL or the platform layer) | as MW3 0x00576ee0: DirectDraw's back buffer: lock and unlock (app_engine.h:560, app_fmv.cpp:53), their raw forms, the flip (sdl_platform.h:68), the restore, the window's style for full screen, the size clamp |
| 0x00598020 | 17 | replaced (SDL or the platform layer) | as MW3 0x00576f70: DirectDraw's back buffer: lock and unlock (app_engine.h:560, app_fmv.cpp:53), their raw forms, the flip (sdl_platform.h:68), the restore, the window's style for full screen, the size clamp |

### zgeometry (29 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00527c60 | 28 | folded | group A, read: the shared port's logic; PM's realloc wrapper (0x00596520) where MW3 calls the C runtime's realloc; the crater clip's unstrip (0x005726f0, pm::zmod::model_unstrip, pm-map.md) and the material sort (0x0055c470, PM's gmod_const.cpp:219) |
| 0x00527d40 | 51 | folded | group A, read: the shared port's logic; PM's realloc wrapper (0x00596520) where MW3 calls the C runtime's realloc; the crater clip's unstrip (0x005726f0, pm::zmod::model_unstrip, pm-map.md) and the material sort (0x0055c470, PM's gmod_const.cpp:219) |
| 0x00527ff0 | 50 | folded | group A, read: the shared port's logic; PM's realloc wrapper (0x00596520) where MW3 calls the C runtime's realloc; the crater clip's unstrip (0x005726f0, pm::zmod::model_unstrip, pm-map.md) and the material sort (0x0055c470, PM's gmod_const.cpp:219) |
| 0x00528120 | 56 | unlabelled |  |
| 0x00528210 | 78 | unlabelled |  |
| 0x005283c0 | 178 | folded | group A, read: the shared port's logic; PM's realloc wrapper (0x00596520) where MW3 calls the C runtime's realloc; the crater clip's unstrip (0x005726f0, pm::zmod::model_unstrip, pm-map.md) and the material sort (0x0055c470, PM's gmod_const.cpp:219) |
| 0x00528790 | 39 | unlabelled |  |
| 0x005288a0 | 26 | folded | group A, read: the shared port's logic; PM's realloc wrapper (0x00596520) where MW3 calls the C runtime's realloc; the crater clip's unstrip (0x005726f0, pm::zmod::model_unstrip, pm-map.md) and the material sort (0x0055c470, PM's gmod_const.cpp:219) |
| 0x00528920 | 170 | unlabelled |  |
| 0x005290b0 | 92 | folded | group A, read: the shared port's logic; PM's realloc wrapper (0x00596520) where MW3 calls the C runtime's realloc; the crater clip's unstrip (0x005726f0, pm::zmod::model_unstrip, pm-map.md) and the material sort (0x0055c470, PM's gmod_const.cpp:219) |
| 0x00529410 | 189 | folded | checked by test (block trace, 2026-11-13): test_pm_crater_cut's cut 0x00528920 runs it on the PM exe and compares the results, the outline as the clip left it and every model field exactly; its call counts differ from MW3's (0x00529860 2 -> 5, 0x0052d1d0 6 -> 9, 0x0052e7d0 5 -> 12) as PM's compiler repeats tails MW3 shares, the same calls in each path |
| 0x0052a810 | 902 | unlabelled |  |
| 0x0052c400 | 16 | folded | covered by the port where it names it (src/gamez/zgeometry/zgeo_weiler.h:55) |
| 0x0052c430 | 16 | folded | covered by the port where it names it (src/gamez/zgeometry/zgeo_weiler.h:55) |
| 0x0052c460 | 24 | folded | covered by the port where it names it (src/gamez/zgeometry/zgeo_weiler.h:55) |
| 0x0052d500 | 45 | unlabelled |  |
| 0x0052dd60 | 124 | unlabelled |  |
| 0x0052e7b0 | 10 | folded | covered by the port where it names it (src/gamez/zgeometry/zgeo_weiler.h:55) |
| 0x0052f3a0 | 26 | folded | covered by the port where it names it (src/gamez/zgeometry/zgeo_convexify.cpp:133) |
| 0x0052f810 | 9 | folded | a thunk: one call to another function |
| 0x0052fbb0 | 17 | folded | covered by the port where it names it (src/gamez/zgeometry/zgeo_convexify.h:13) |
| 0x0052fbf0 | 201 | unlabelled |  |
| 0x005307d0 | 12 | folded | as MW3 0x00516f00: zImage's start, stop and per-mission entries, covered where named: shutdown (memory only, app_engine.cpp:324), the texture group begin (app_mission.cpp:1912), the textures' free (app_mission.cpp:1874), the texture directory as the 2D host's mounts (sdl_twod_host.cpp:205, FIX (A)) |
| 0x005307f0 | 9 | folded | as MW3 0x00516f20: zImage's start, stop and per-mission entries, covered where named: shutdown (memory only, app_engine.cpp:324), the texture group begin (app_mission.cpp:1912), the textures' free (app_mission.cpp:1874), the texture directory as the 2D host's mounts (sdl_twod_host.cpp:205, FIX (A)) |
| 0x00530840 | 13 | folded | as MW3 0x00516f50: zImage's start, stop and per-mission entries, covered where named: shutdown (memory only, app_engine.cpp:324), the texture group begin (app_mission.cpp:1912), the textures' free (app_mission.cpp:1874), the texture directory as the 2D host's mounts (sdl_twod_host.cpp:205, FIX (A)) |
| 0x00530870 | 28 | replaced (SDL or the platform layer) | as MW3 0x00516f80: zimg_winfonts' entries read from fonts.zrd's WINDOWS_FONTS and their GDI fonts (with the list in zImage): the SDL text host keeps its own |
| 0x005308e0 | 92 | replaced (SDL or the platform layer) | as MW3 0x00516ff0: zimg_winfonts' entries read from fonts.zrd's WINDOWS_FONTS and their GDI fonts (with the list in zImage): the SDL text host keeps its own |
| 0x00530b20 | 13 | replaced (SDL or the platform layer) | as MW3 0x00517230: zimg_winfonts' entries read from fonts.zrd's WINDOWS_FONTS and their GDI fonts (with the list in zImage): the SDL text host keeps its own |
| 0x00530b40 | 20 | replaced (SDL or the platform layer) | as MW3 0x00517250: zimg_winfonts' entries read from fonts.zrd's WINDOWS_FONTS and their GDI fonts (with the list in zImage): the SDL text host keeps its own |

### zsiscript (5 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005cc670 | 22 | folded | covered by the port where it names it (src/mech3/mech_anim.cpp:412) |
| 0x005cc880 | 29 | debug or dev tool: PORT (preserve) | as MW3 0x005a9bc0: the motion script's text side: the loose .zan reader (0x005a9e10, with 0x005aa210) behind the developers' override, and the writer (0x005a9bc0) |
| 0x005ccaf0 | 168 | debug or dev tool: PORT (preserve) | as MW3 0x005a9e10: the motion script's text side: the loose .zan reader (0x005a9e10, with 0x005aa210) behind the developers' override, and the writer (0x005a9bc0) |
| 0x005ccef0 | 19 | debug or dev tool: PORT (preserve) | as MW3 0x005aa210: the motion script's text side: the loose .zan reader (0x005a9e10, with 0x005aa210) behind the developers' override, and the writer (0x005a9bc0) |
| 0x005cd090 | 27 | unlabelled |  |

### zerror (5 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00527250 | 9 | folded | a thunk: one call to another function |
| 0x00527270 | 15 | unlabelled |  |
| 0x00527490 | 22 | unlabelled |  |
| 0x00527500 | 38 | unlabelled |  |
| 0x005275d0 | 30 | unlabelled |  |


# Port audit: the original functions not ported (mw3)

Every game function of the original (below the C runtime) that has no port of its own, labelled from its decomp,
its callers and callees, the port's comments and the docs (tools/analysis/port_audit.py; labels with their
reasons in tools/analysis/port_audit_labels.tsv). Addresses and plain words only.

Classes: **folded** (into its callers or a ported class: getters, setters, constructors, destructors, thunks,
container code, or covered where the port names it); **replaced** (SDL or the platform layer); **PORT (preserve)**
(debug and dev tools, unused code: to be ported for preservation, not called by the game); **MISSING LOGIC**
(runs in play and the port does not, or does it differently); **UNSURE** (what would settle it is said).

| Class | Functions |
|---|---|
| folded | 3023 |
| replaced (SDL or the platform layer) | 221 |
| software renderer | 113 |
| unused: PORT (preserve) | 82 |
| debug or dev tool: PORT (preserve) | 73 |
| dev dialog (MFC), its logic: PORT (preserve) | 54 |
| dev dialog (MFC), its window plumbing: replaced | 50 |
| MISSING LOGIC (only data the shipped game lacks reaches it) | 8 |
| unlabelled | 2 |

## MISSING LOGIC

- **0x004fd1e0, 0x004fd8c0** (2 functions, 54 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): zClass's add-child for an animate (0x004fd8c0) or switch (0x004fd1e0) parent: the port's node_add_child (cls_node.cpp:186) handles worlds, links and object3d and aborts ('not ported yet') for cameras, LoDs, animate and switch parents, though the camera's own add-child is ported (camera.cpp, 0x004f33a0). No shipped data adds such a child at run time (every mission plays); a mod's scripts or data would abort the game
- **0x0051f390, 0x0051f750** (2 functions, 276 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): billboard modes 2 (about x) and 3 (about z): no shipped world uses them; they draw as default models (gmod_draw.cpp:759, render-modeldraw.md:27)
- **0x005a6a10** (1 functions, 45 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): Line2D's reader from a dialog .zrd (its COLOR and its end), shared by the line classes' vtables; inferred unused by shipped data (the dialog reader matches the exe on every shipped dialog without it), not checked file by file
- **0x005a7c50, 0x005a7ca0, 0x005a7cd0** (3 functions, 47 decomp lines; MISSING LOGIC (only data the shipped game lacks reaches it)): Movie2D's update, draw and erase (a movie playing inside a dialog): no shipped dialog has one (twod.md:674-679)

## BROKEN (null hook)

Hooks the ported code calls that the program leaves unset (a null field, a census stand-in, or a
base virtual that only logs), which shipped play reaches. "Shown by": the chain runs (the census
of a campaign, Instant Action or training run) or the static check (the data and the callers).

| Hook | Original | Reached by | The original does | In the port | Seen in play | Shown by |
|---|---|---|---|---|---|---|

Checked and not broken (2026-11-13): the census of the campaign by objectives (20 missions), every Instant Action world and scenario, and training reached 0 functions outside the known table and skipped 0 script calls. The script entries left null (hook_allow counted:) are used only under ifndef USEZBD (the world built from scripts; the game loads gamez.zbd) and in the Support\*.gw and dev*.gs scripts. The never-overridden virtuals left are no effect (render_use_gfx_flags: [0x008008d8] unread in hardware), multiplayer (the field bases' station repairs behind mp_repairs), the software path's views (app_states 0x00481290, 0x00481a10), a failed spawn's delete, the mech lab without its screen, the empty mission_complete report, and translate_to_anim_root, which no shipped targets.zrd has.

Fixed 2026-11-13 (mac5/broken-five), the five rows this table had: the radar's view wedge over the mission map (radar_view_dir on zmth::look_dir, 0x00522170, test_look_dir.py) and the mech's heading line (HudRadar::set_heading, 0x0043f2d0, run in test_hud.py), the ZAPS_HUD static (Hud::zap_static, 0x0043be70, test_hud.py op 44), the equipment callout and the engine-hit voice (0x00485390 with 0x00485360, 0x00485370; test_equip_voice.py).

## PORT (preserve)

### debug or dev tool: PORT (preserve)

- 9 functions, 241 decomp lines (0x0040ef60, 0x0040f010, 0x0040f150, 0x0040f7f0 ...): the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only)
- 1 functions, 17 decomp lines (0x0041ca90): a debug key command (0x6d, case 2 of the key switch 0x0040dba0) toggling the mech's +0x634 bit 1; the release never registers 0x6d (0x0040db60 registers 0x61, 0x6c, 0x70, 0x7e)
- 1 functions, 25 decomp lines (0x0041cee0): read: the mech's vt+0xf8 (the player's view pick: the crosshair's ground point, +0x1d48 of 0x00430aa0's object, when this is the viewed mech), called only by the AI net editor's commands 0x00402d60 and 0x00403160 (game_editor_net.cpp:146, 229) through the viewed vehicle; the port's editor answers for the stealth object only (app_vehicle.cpp:475)
- 2 functions, 87 decomp lines (0x0043cb10, 0x0043cc00): the HUD's debug overlays (docs/hud.md section 11: the mech's gait, speeds and the like into five debug texts while the debug text shows, command 0x82); hooked in the port (hud_debug, unported)
- 1 functions, 34 decomp lines (0x004810d0): the debug console's overlay toggle (0x00608f74; game-loop.md: from 0x00480b20, itself never called)
- 4 functions, 172 decomp lines (0x004a3220, 0x004a33f0, 0x004a3550, 0x004a35e0): debug key commands of 0x0040dba0, never registered in the release: a position marker printed to the console (0x004a3220), the mechs listed into the developer lines (0x004a33f0, command 0x89; hud.md), a vehicle picked in a dialog (0x004a3550, its destructor 0x004a35e0)
- 2 functions, 18 decomp lines (0x004a4ca0, 0x004a50b0): the stealth object (the developers' fly camera; back burner in CLAUDE.md): a slot of its
- 2 functions, 149 decomp lines (0x004a50e0, 0x004abbc0): the debug command 0x7e (0x004abbc0, 121 lines: a console of keyboard text and the network session; registered, but no key reaches it: app_keys.h:14, hud.md) and the stealth object's key handler (0x004a50e0: the fly camera, back burner)
- 2 functions, 18 decomp lines (0x004e2310, 0x004e53b0): the valve reader's print method (a vtable slot that fprintf's a value); nothing in the game prints a reader
- 8 functions, 290 decomp lines (0x00519990, 0x005199e0, 0x00519a20, 0x00519a40 ...): the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it
- 1 functions, 225 decomp lines (0x00575b00): the developer's stdout redirect (freopen to a log; editor.md:214)
- 27 functions, 837 decomp lines (0x0058c590, 0x0058d3c0, 0x0058e440, 0x0058e460 ...): the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists)
- 3 functions, 56 decomp lines (0x005960a0, 0x00596170, 0x00596190): the hidden GameZ Interpreter console (0x005960a0: a 2D console window that runs typed script lines through 0x00595510; its destructor and line runner; toggled by 0x004810d0, which nothing reaches: interp.md:532, discoveries.md:1532)
- 6 functions, 173 decomp lines (0x005a0050, 0x005a3c40, 0x005a3cd0, 0x005a3de0 ...): the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683)
- 1 functions, 17 decomp lines (0x005a98d0): the developers' loose <name>.zan override for motion scripts (motion.md:36), like the reader's loose .zrd files
- 3 functions, 219 decomp lines (0x005a9bc0, 0x005a9e10, 0x005aa210): the motion script's text side: the loose .zan reader (0x005a9e10, with 0x005aa210) behind the developers' override, and the writer (0x005a9bc0)

### dev dialog (MFC), its logic: PORT (preserve)

- 2 functions, 229 decomp lines (0x00403820, 0x0049d350): the editor's 'add vehicle to node' (0x00403820 -> 0x0049d350, editor.md); ours does it as meant: upgrades/editor/add_vehicle.cpp
- 3 functions, 103 decomp lines (0x00403ec0, 0x00404090, 0x0049d140): the AI tester dialog's walk over a vehicle's nets (0x00404090 -> 0x00403ec0: their extents)
- 1 functions, 68 decomp lines (0x0040bb00): the camera dialog (resource 0x6a: the camera's place and angles in degrees, editable; 0x0040bd00 its window): a dev tool's logic
- 2 functions, 61 decomp lines (0x00432c60, 0x00432d30): a mech list dialog (resource 0x9c, from a debug key command in 0x0040db60): its fill of every mech name (0x00484650)
- 16 functions, 250 decomp lines (0x00483170, 0x00483200, 0x00483230, 0x004832e0 ...): the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags)
- 7 functions, 160 decomp lines (0x0049d710, 0x0049d7b0, 0x0049d810, 0x0049d8d0 ...): the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians
- 1 functions, 40 decomp lines (0x004af000): a vehicle-pick dev dialog (resource 0x8f, from the debug key's 0x004a3550): its list of zEntity's vehicles
- 2 functions, 24 decomp lines (0x004bb2e0, 0x004bb5f0): the vehicle editor's list fills (each valve, each region by name into a combo box); covered in the port by game_editor_vehicles 0x004bb300 / 0x004bb620 (valve and region lists)
- 3 functions, 54 decomp lines (0x004bc9f0, 0x004bcb50, 0x004bcb70): the AI tester dialog (0x004b7d80..0x004bcb70, docs/ai.md: a developer tool over a vehicle's AI and net): its helpers' logic
- 12 functions, 305 decomp lines (0x004c9140, 0x004c9180, 0x004c9260, 0x004c92b0 ...): read: the steps' link editing (Step's vt+0x30..+0x44, vtable 0x005cb3d0, and the node steps' vt+0x30..+0x50, vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4): set, compare, find or count a link (+0x0c / +0x20) from a picked element's behaviour (its vt+0x60 then +0x74), the node's position set: the AI path editor's (ZAiNetEdit's editing side, to port behind --debug, docs/ai.md The developer tools). No run-time caller found: the port's AI, emulator-checked on every shipped net (compile, pilot, orders), has no such slots, and no passing PM AI test reaches the PM twins (block trace); the virtual calls through these offsets could not all be typed statically
- 4 functions, 126 decomp lines (0x004d9d80, 0x004d9dc0, 0x004d9ed0, 0x004d9fc0): an AI parameters dialog (MFC): what it reads into its list (the net's records) and positions; preserve as logic
- 1 functions, 20 decomp lines (0x004df6a0): the editor's 'add vehicle to node' record (an aiv record built on OK, editor.md); in the release its path returns 0 and nothing is made; ours does it as meant: upgrades/editor/add_vehicle

### unused: PORT (preserve)

- 1 functions, 17 decomp lines (0x0040fef0): the timing marks ('Dummy Frame', 'Lets Play!'): act only with [0x005e14f0], 0 in the data (program.md, no effect)
- 1 functions, 14 decomp lines (0x0041d280): read: the mech's vt+0x124 (the stopping distance now: |the controls' speed x +0x518| through vt+0x120); its one caller, the field base dock 0x00459990 (mech_fieldbase_dock.cpp:152), calls it on the base, a vehicle (0x004ad050, ported), so on a mech nothing reaches it (the calls through +0x124 in the exe: that one only)
- 4 functions, 72 decomp lines (0x0041d2c0, 0x0041d300, 0x0041d320, 0x0041d380): read: the mech's vt+0x130 (a turn rate from a speed and a share, floored), vt+0x134 and vt+0x138 (it from the controls' speed or a share) and vt+0x13c (the share that turns by a rate): no call in the exe goes through +0x134, +0x138 or +0x13c, and +0x130's callers are those two and the vehicles' own (0x004ad250, 0x004ad290 on their vtable), so on a mech nothing reaches them
- 1 functions, 52 decomp lines (0x0041dd90): read: the mech's vt+0x94 (a world direction clamped into the torso's arc through 0x00465c70, the vehicles' 0x004ad7f0 counterpart): the 20 calls through +0x94 in the exe are on other classes (IControls' on_hit, the 2D widgets, the behaviours' attack test, the edit bar, a part), none on a mech; the port's aim uses vt+0x90 (Mech::clamp_aim, 0x0041dca0)
- 1 functions, 27 decomp lines (0x00493120): the movie base's run to the end on its own clock (0x00575d30): nothing calls it (fmv.md)
- 5 functions, 216 decomp lines (0x004a3fb0, 0x004a3fe0, 0x004a43c0, 0x004a4660 ...): the turn limits' clamp (vt+0x04) for the Fixed, Narrow and Wide kinds (vtables 0x005c8310/20/30): the turret never asks clamp (turret.h:31) and the AI's track keeps its answer only for a dot product it drops (vehicles.md, zai_firing.cpp:554)
- 2 functions, 97 decomp lines (0x004b1170, 0x004b12e0): the static the AI's planner builds at 0x0060b240 (two reserved vectors): read by nothing (weapon_planner.h:7)
- 2 functions, 45 decomp lines (0x004c0ed0, 0x004c0f70): the vehicle's +0xd8 record of each in/out query (its constructor and reset): written, never read (docs/symbols.csv: not kept; ai.md 11)
- 4 functions, 138 decomp lines (0x004cd790, 0x004cd7b0, 0x004cd800, 0x004cd810): the brain's own step at +0x240 (0x004cd770, vtable 0x005cc290): built by the brain's constructor, but nothing orders it (only the constructor takes +0x240), so its make_state, state and run (path following toward a point, 98 lines) never run
- 1 functions, 12 decomp lines (0x004d7160): read: AiTarget's vt+0x6c (vtable 0x005bcf64 and five more: the mechs, the network mech, turrets): the whole node tinted (blend 0.6, 0x004fb8c0) and its blend switched (0x004fb840), never overridden; the target window tints through vt+0x68 instead (the mech's 0x0041b9d0, the turret's 0x004a75b0; the port's hud_object_view_tint, app_sensors.cpp:454), and no call through +0x6c with a flag on an entity was found
- 1 functions, 58 decomp lines (0x004dee50): read: the aiv reader's vt+0x04 (vtable 0x005cd428; the reader base's is 0x004da730, return 1): 0 when a zEntity already has the name (0x005a8210) or the vehicle editor's cache (+0x24, none while playing) holds a record under it, else 1; no caller found (no call through +0x04 near any of the 13 references to the aiv file [0x0060b5e0], no direct call); the port's AivFile has no such slot (zrdraiv.h)
- 2 functions, 57 decomp lines (0x004e77a0, 0x004e95b0): IControls vt+0x84 of the AI controls (drop the target if it is the given entity's, stand the firing down): no caller reaches that slot (docs/ai.md, the AI controls' unreached slots)
- 3 functions, 34 decomp lines (0x004f3540, 0x004f8f20, 0x004f8f60): no effect in the original (app_engine.cpp's list): a camera word nothing reads (0x004f3540), zClass's start defaults the port's initialisers already hold (0x004f8f20), a zSave registration with flag 0 (0x004f8f60)
- 3 functions, 78 decomp lines (0x004fc4d0, 0x004fc990, 0x004fcca0): mechlib's material writer at close (0x004fc990 -> 0x004fcca0 -> 0x004fc4d0): dead (mechlib.md)
- 1 functions, 15 decomp lines (0x005190e0): frees a chain of loaded textures; its only caller 0x005374d0 is unreached
- 6 functions, 224 decomp lines (0x00519df0, 0x00519e50, 0x00519f70, 0x00519fa0 ...): zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4)
- 1 functions, 51 decomp lines (0x0051af90): the input module's static object's list walk, whose caller 0x0051af70 nothing reaches (input.md:567)
- 18 functions, 1112 decomp lines (0x0051e160, 0x00520c60, 0x00521080, 0x005214f0 ...): zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193)
- 15 functions, 3789 decomp lines (0x00530d90, 0x00531d90, 0x005331e0, 0x00534745 ...): large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers
- 1 functions, 52 decomp lines (0x00548840): the puffers' wind step with a dt argument: dead code, never called (puffers.md)
- 2 functions, 33 decomp lines (0x00557450, 0x00557490): the defines list's constructor and destructor, no caller reaches them (zreader.md:103)
- 1 functions, 211 decomp lines (0x005706f0): sounds.zrd's SYNTAX 1 reader: no shipped or community data uses syntax 1 (symbols.csv, sound.md:41)
- 3 functions, 285 decomp lines (0x00571380, 0x00571f70, 0x005746b0): the Aureal A3D path: its init (CoCreateInstance), 3D placement and error names; only 0x570625 writes the selector [0x72c2cc], and only to 0 (DirectSound) when A3D fails, so A3D never runs (sound.md:38)
- 2 functions, 86 decomp lines (0x00596a70, 0x00596c80): list teardown helpers whose callers (0x00596930, 0x00596c60) nothing reaches
- 1 functions, 9 decomp lines (0x005a92d0): _rmtmp at zSave's stop, unreached

## UNSURE


## By module

### mech3 (2543 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00401000 | 12 | dev dialog (MFC), its window plumbing: replaced | MFC dialog plumbing of dev dialogs (CDialog constructors and destructors, EnableWindow): resource 0x67 and the teleport and net dialogs' destructors |
| 0x00401020 | 8 | folded | returns a constant or a global |
| 0x00401030 | 9 | dev dialog (MFC), its window plumbing: replaced | MFC dialog plumbing of dev dialogs (CDialog constructors and destructors, EnableWindow): resource 0x67 and the teleport and net dialogs' destructors |
| 0x00401040 | 9 | dev dialog (MFC), its window plumbing: replaced | MFC dialog plumbing of dev dialogs (CDialog constructors and destructors, EnableWindow): resource 0x67 and the teleport and net dialogs' destructors |
| 0x004020b0 | 14 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:1095) |
| 0x00402140 | 14 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00402510 | 28 | folded | covered: array and list helpers, destructors (unwind callers), the loading bar and the briefing's map (app_menus.cpp:964 overrides briefing_map_image; the hook's 'not ported yet' comment is stale), the vehicle path's entries drop (app_vehicle.h:145) |
| 0x00402590 | 9 | folded | a thunk: one call to another function |
| 0x00402b20 | 16 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:286) |
| 0x00402b40 | 11 | folded | covered by the port where it names it (src/gamez/znetwork/znet_object.h:32) |
| 0x00402b70 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00402bf0 | 32 | folded | covered by the port where it names it (src/mech3/game_editor_tree.cpp:77) |
| 0x00402ec0 | 12 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:303) |
| 0x00402f10 | 8 | folded | returns a constant or a global |
| 0x00402f20 | 14 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:295) |
| 0x00403140 | 9 | folded | a thunk: one call to another function |
| 0x00403710 | 23 | folded | covered by the port where it names it (src/upgrades/editor/node_params.h:16) |
| 0x00403820 | 81 | dev dialog (MFC), its logic: PORT (preserve) | the editor's 'add vehicle to node' (0x00403820 -> 0x0049d350, editor.md); ours does it as meant: upgrades/editor/add_vehicle.cpp |
| 0x00403a40 | 21 | dev dialog (MFC), its window plumbing: replaced | MFC dialog plumbing of dev dialogs (CDialog constructors and destructors, EnableWindow): resource 0x67 and the teleport and net dialogs' destructors |
| 0x00403a90 | 18 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:349) |
| 0x00403c00 | 57 | folded | covered where the port does it: src/mech3/zainetedit.cpp:107 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00403eb0 | 9 | folded | frees the object (a destructor's release) |
| 0x00403ec0 | 75 | dev dialog (MFC), its logic: PORT (preserve) | the AI tester dialog's walk over a vehicle's nets (0x00404090 -> 0x00403ec0: their extents) |
| 0x00404090 | 19 | dev dialog (MFC), its logic: PORT (preserve) | the AI tester dialog's walk over a vehicle's nets (0x00404090 -> 0x00403ec0: their extents) |
| 0x004040d0 | 14 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.h:354) |
| 0x00404100 | 9 | folded | a thunk: one call to another function |
| 0x00404110 | 9 | folded | a thunk: one call to another function |
| 0x00404140 | 53 | folded | covered where the port does it: src/mech3/game_editor_net.cpp:29 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00404200 | 27 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:596) |
| 0x004042f0 | 20 | folded | covered: array and list helpers, destructors (unwind callers), the loading bar and the briefing's map (app_menus.cpp:964 overrides briefing_map_image; the hook's 'not ported yet' comment is stale), the vehicle path's entries drop (app_vehicle.h:145) |
| 0x00404320 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00404340 | 34 | folded | covered: array and list helpers, destructors (unwind callers), the loading bar and the briefing's map (app_menus.cpp:964 overrides briefing_map_image; the hook's 'not ported yet' comment is stale), the vehicle path's entries drop (app_vehicle.h:145) |
| 0x00404600 | 14 | folded | covered by the port where it names it (src/mech3/zrdrainet.cpp:180) |
| 0x004046b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004046d0 | 21 | dev dialog (MFC), its window plumbing: replaced | MFC dialog plumbing of dev dialogs (CDialog constructors and destructors, EnableWindow): resource 0x67 and the teleport and net dialogs' destructors |
| 0x004047a0 | 8 | folded | returns a constant or a global |
| 0x00404ac0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004050d0 | 18 | folded | covered: array and list helpers, destructors (unwind callers), the loading bar and the briefing's map (app_menus.cpp:964 overrides briefing_map_image; the hook's 'not ported yet' comment is stale), the vehicle path's entries drop (app_vehicle.h:145) |
| 0x00405160 | 29 | folded | covered: array and list helpers, destructors (unwind callers), the loading bar and the briefing's map (app_menus.cpp:964 overrides briefing_map_image; the hook's 'not ported yet' comment is stale), the vehicle path's entries drop (app_vehicle.h:145) |
| 0x00405d30 | 8 | folded | returns a constant or a global |
| 0x00405d40 | 8 | folded | empty: does nothing |
| 0x00405ef0 | 49 | folded | covered: array and list helpers, destructors (unwind callers), the loading bar and the briefing's map (app_menus.cpp:964 overrides briefing_map_image; the hook's 'not ported yet' comment is stale), the vehicle path's entries drop (app_vehicle.h:145) |
| 0x00405fd0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00406000 | 9 | folded | a thunk: one call to another function |
| 0x00406020 | 25 | folded | covered by the port where it names it (src/gamez/ztwod/twod_textbox.cpp:195) |
| 0x00406070 | 12 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:170) |
| 0x004060a0 | 9 | folded | a thunk: one call to another function |
| 0x004060c0 | 25 | folded | covered: array and list helpers, destructors (unwind callers), the loading bar and the briefing's map (app_menus.cpp:964 overrides briefing_map_image; the hook's 'not ported yet' comment is stale), the vehicle path's entries drop (app_vehicle.h:145) |
| 0x00406110 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00406140 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00407d80 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00407db0 | 9 | folded | a thunk: one call to another function |
| 0x00407dd0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00407e00 | 9 | folded | a thunk: one call to another function |
| 0x00407e20 | 9 | folded | a thunk: one call to another function |
| 0x00407e80 | 9 | folded | a thunk: one call to another function |
| 0x00408110 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00408140 | 20 | folded | covered: array and list helpers, destructors (unwind callers), the loading bar and the briefing's map (app_menus.cpp:964 overrides briefing_map_image; the hook's 'not ported yet' comment is stale), the vehicle path's entries drop (app_vehicle.h:145) |
| 0x004083c0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004083f0 | 20 | folded | covered: array and list helpers, destructors (unwind callers), the loading bar and the briefing's map (app_menus.cpp:964 overrides briefing_map_image; the hook's 'not ported yet' comment is stale), the vehicle path's entries drop (app_vehicle.h:145) |
| 0x00408450 | 9 | folded | a thunk: one call to another function |
| 0x004085e0 | 9 | folded | a thunk: one call to another function |
| 0x00408630 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00408660 | 13 | folded | covered by the port where it names it (src/mech3/briefing.cpp:647) |
| 0x004092e0 | 13 | folded | covered by the port where it names it (src/mech3/app_states.cpp:1270) |
| 0x004093d0 | 10 | folded | a getter of a member |
| 0x00409830 | 10 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00409850 | 10 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00409860 | 11 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00409870 | 21 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x004098e0 | 17 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00409910 | 23 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00409980 | 29 | folded | covered by the port where it names it (src/mech3/mech_anim.cpp:579) |
| 0x004099c0 | 20 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:905) |
| 0x00409a00 | 11 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00409a90 | 15 | folded | covered by the port where it names it (src/mech3/briefing.cpp:465) |
| 0x00409ab0 | 11 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00409ac0 | 15 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:976) |
| 0x00409b10 | 12 | folded | sets members (a setter or a small constructor) |
| 0x00409b40 | 8 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:314) |
| 0x00409b50 | 31 | replaced (SDL or the platform layer) | GDI: a font's metrics from its handle (GetObjectA), the 2D text's Windows path; the port's text goes through ztwod::Host |
| 0x00409c60 | 13 | folded | covered by the port where it names it (src/mech3/briefing.cpp:145) |
| 0x00409d80 | 12 | folded | sets members (a setter or a small constructor) |
| 0x00409da0 | 10 | folded | a getter of a member |
| 0x00409db0 | 11 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:572) |
| 0x00409e00 | 11 | folded | sets members (a setter or a small constructor) |
| 0x00409e10 | 10 | folded | a getter of a member |
| 0x00409e20 | 9 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:683) |
| 0x00409e30 | 9 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:684) |
| 0x00409e40 | 11 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:682) |
| 0x00409e60 | 14 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:200) |
| 0x00409e80 | 10 | folded | a getter of a member |
| 0x00409e90 | 13 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00409eb0 | 11 | folded | a thunk: one call to another function |
| 0x00409ed0 | 10 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00409f40 | 8 | folded | covered by the port where it names it (src/mech3/mech_config.h:23) |
| 0x00409f50 | 11 | folded | ported, by docs/symbols.csv: ported; vt+4 |
| 0x00409f60 | 21 | folded | covered by the port where it names it (src/mech3/mech_config.h:166) |
| 0x00409fb0 | 9 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00409fc0 | 10 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040a020 | 34 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040a0c0 | 10 | folded | covered by the port where it names it (src/mech3/briefing.cpp:774) |
| 0x0040a0d0 | 10 | folded | a getter of a member |
| 0x0040a0e0 | 11 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040a0f0 | 10 | folded | a getter of a member |
| 0x0040a100 | 8 | folded | returns a constant or a global |
| 0x0040a510 | 8 | folded | empty: does nothing |
| 0x0040a540 | 8 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:68) |
| 0x0040a550 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0040a570 | 22 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:59) |
| 0x0040a710 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0040a740 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0040a760 | 13 | folded | covered by the port where it names it (src/mech3/briefing.cpp:782) |
| 0x0040a780 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0040a790 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0040a7a0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0040a7b0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0040a7c0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0040a7d0 | 13 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040a7e0 | 14 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040a800 | 10 | folded | a getter of a member |
| 0x0040a810 | 10 | folded | a getter of a member |
| 0x0040a830 | 10 | folded | a getter of a member |
| 0x0040a840 | 10 | folded | a getter of a member |
| 0x0040a850 | 10 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x0040a860 | 10 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x0040a870 | 10 | folded | a getter of a member |
| 0x0040a880 | 10 | folded | a getter of a member |
| 0x0040a8a0 | 10 | folded | a getter of a member |
| 0x0040a8b0 | 27 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x0040a8f0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0040a930 | 10 | folded | a getter of a member |
| 0x0040a940 | 10 | folded | a getter of a member |
| 0x0040a9c0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0040a9e0 | 11 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040a9f0 | 9 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:112) |
| 0x0040aa00 | 23 | folded | covered by the port where it names it (src/mech3/player_view.h:99) |
| 0x0040aa30 | 80 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040abf0 | 40 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040bac0 | 9 | folded | covered by the port where it names it (src/mech3/player_view.h:36) |
| 0x0040bae0 | 8 | folded | returns a constant or a global |
| 0x0040baf0 | 8 | folded | empty: does nothing |
| 0x0040bb00 | 68 | dev dialog (MFC), its logic: PORT (preserve) | the camera dialog (resource 0x6a: the camera's place and angles in degrees, editable; 0x0040bd00 its window): a dev tool's logic |
| 0x0040bd00 | 23 | dev dialog (MFC), its window plumbing: replaced | MFC dev dialogs' plumbing: resource 0x6a (a dialog of float fields: constructor, DDX, close, DoModal), the teleport dialog's slider (OnHScroll) and destructor, a combo box's pick into the texture memory setting |
| 0x0040bd50 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0040bd70 | 20 | dev dialog (MFC), its window plumbing: replaced | MFC dev dialogs' plumbing: resource 0x6a (a dialog of float fields: constructor, DDX, close, DoModal), the teleport dialog's slider (OnHScroll) and destructor, a combo box's pick into the texture memory setting |
| 0x0040be20 | 8 | folded | returns a constant or a global |
| 0x0040be50 | 12 | dev dialog (MFC), its window plumbing: replaced | MFC dev dialogs' plumbing: resource 0x6a (a dialog of float fields: constructor, DDX, close, DoModal), the teleport dialog's slider (OnHScroll) and destructor, a combo box's pick into the texture memory setting |
| 0x0040bea0 | 12 | dev dialog (MFC), its window plumbing: replaced | MFC dev dialogs' plumbing: resource 0x6a (a dialog of float fields: constructor, DDX, close, DoModal), the teleport dialog's slider (OnHScroll) and destructor, a combo box's pick into the texture memory setting |
| 0x0040d3e0 | 8 | folded | covered by the port where it names it (src/mech3/app_hooks.h:126) |
| 0x0040d410 | 11 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040d4b0 | 8 | folded | returns a constant or a global |
| 0x0040d760 | 8 | folded | covered by the port where it names it (src/mech3/app_hooks.h:96) |
| 0x0040d770 | 8 | folded | covered by the port where it names it (src/mech3/app_hooks.h:97) |
| 0x0040d780 | 11 | folded | covered by the port where it names it (src/mech3/options.h:234) |
| 0x0040d790 | 8 | folded | covered by the port where it names it (src/mech3/app_hooks.h:89) |
| 0x0040d7b0 | 8 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040d7c0 | 8 | folded | covered by the port where it names it (src/mech3/app_hooks.h:91) |
| 0x0040d7d0 | 11 | folded | a thunk: one call to another function |
| 0x0040d7f0 | 11 | folded | a thunk: one call to another function |
| 0x0040def0 | 40 | dev dialog (MFC), its window plumbing: replaced | MFC dev dialogs' plumbing: resource 0x6a (a dialog of float fields: constructor, DDX, close, DoModal), the teleport dialog's slider (OnHScroll) and destructor, a combo box's pick into the texture memory setting |
| 0x0040e120 | 42 | folded | covered where the port does it: src/mech3/game_editor_net.cpp:229 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0040e200 | 21 | dev dialog (MFC), its window plumbing: replaced | MFC dev dialogs' plumbing: resource 0x6a (a dialog of float fields: constructor, DDX, close, DoModal), the teleport dialog's slider (OnHScroll) and destructor, a combo box's pick into the texture memory setting |
| 0x0040e260 | 9 | folded | a thunk: one call to another function |
| 0x0040e3b0 | 22 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040e410 | 9 | folded | a thunk: one call to another function |
| 0x0040e840 | 24 | folded | covered by the port where it names it (src/mech3/game_editor.cpp:46) |
| 0x0040e8b0 | 15 | dev dialog (MFC), its window plumbing: replaced | MFC dev dialogs' plumbing: resource 0x6a (a dialog of float fields: constructor, DDX, close, DoModal), the teleport dialog's slider (OnHScroll) and destructor, a combo box's pick into the texture memory setting |
| 0x0040e8e0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0040e8f0 | 23 | folded | covered by the port where it names it (src/mech3/entity_bind.cpp:51) |
| 0x0040e960 | 23 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:536) |
| 0x0040e990 | 15 | folded | covered by the port where it names it (src/mech3/entity_bind.cpp:40) |
| 0x0040e9b0 | 15 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:542) |
| 0x0040eb10 | 9 | folded | a thunk: one call to another function |
| 0x0040eb30 | 21 | folded | covered by the port where it names it (src/mech3/entity_bind.cpp:24) |
| 0x0040eb80 | 21 | folded | covered by the port where it names it (src/mech3/entity_bind.cpp:29) |
| 0x0040ebe0 | 9 | folded | a thunk: one call to another function |
| 0x0040ec00 | 9 | folded | a thunk: one call to another function |
| 0x0040eca0 | 21 | folded | covered by the port where it names it (src/mech3/entity_bind.cpp:57) |
| 0x0040ed60 | 9 | folded | a thunk: one call to another function |
| 0x0040ed80 | 21 | folded | covered by the port where it names it (src/mech3/entity_bind.cpp:46) |
| 0x0040edd0 | 9 | folded | a thunk: one call to another function |
| 0x0040edf0 | 9 | folded | a thunk: one call to another function |
| 0x0040ee10 | 9 | folded | a thunk: one call to another function |
| 0x0040ee30 | 9 | folded | a thunk: one call to another function |
| 0x0040ee50 | 15 | folded | covered by the port where it names it (src/mech3/app_pocket.cpp:213) |
| 0x0040ee70 | 9 | folded | a thunk: one call to another function |
| 0x0040ee90 | 9 | folded | a thunk: one call to another function |
| 0x0040eeb0 | 15 | folded | covered by the port where it names it (src/mech3/entity_bind.cpp:52) |
| 0x0040ef60 | 9 | debug or dev tool: PORT (preserve) | follows 0x0040ff40: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x0040ef80 | 9 | folded | a thunk: one call to another function |
| 0x0040f010 | 9 | debug or dev tool: PORT (preserve) | follows 0x0040f150: the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x0040f020 | 9 | folded | a thunk: one call to another function |
| 0x0040f0f0 | 9 | folded | follows 0x0040f130: covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040f100 | 9 | folded | a thunk: one call to another function |
| 0x0040f130 | 12 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040f150 | 34 | debug or dev tool: PORT (preserve) | the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x0040f1f0 | 36 | folded | covered by the port where it names it (src/mech3/app_engine.cpp:978) |
| 0x0040f710 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0040f730 | 33 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040f7f0 | 23 | debug or dev tool: PORT (preserve) | the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x0040f870 | 10 | debug or dev tool: PORT (preserve) | the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x0040f880 | 8 | folded | returns a constant or a global |
| 0x0040f890 | 10 | debug or dev tool: PORT (preserve) | the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x0040f8b0 | 10 | debug or dev tool: PORT (preserve) | the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x0040fb50 | 101 | debug or dev tool: PORT (preserve) | the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x0040fe70 | 17 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x0040fef0 | 17 | unused: PORT (preserve) | the timing marks ('Dummy Frame', 'Lets Play!'): act only with [0x005e14f0], 0 in the data (program.md, no effect) |
| 0x0040ff40 | 35 | debug or dev tool: PORT (preserve) | the developer text lines (25 rows at [0x005fc848], Arial 10): their font and layers (0x0040f7f0, 0x0040f870, 0x0040f890, 0x0040f8b0) and the per-frame readout 0x0040fb50, which writes the lines every frame but whose layers are never on in the release (hud.md section 20; the port's notes: developer only) |
| 0x0040fff0 | 9 | folded | a thunk: one call to another function |
| 0x00410000 | 9 | folded | a thunk: one call to another function |
| 0x00410010 | 13 | folded | read: an atexit handler (registered at 0x00410000): a static list freed once (0x0058b220), the guard bit [0x006040a4]; the port's objects free themselves |
| 0x00410040 | 9 | folded | a thunk: one call to another function |
| 0x00410050 | 9 | folded | a thunk: one call to another function |
| 0x00410090 | 9 | folded | a thunk: one call to another function |
| 0x004100a0 | 9 | folded | a thunk: one call to another function |
| 0x004100e0 | 9 | folded | a thunk: one call to another function |
| 0x004100f0 | 9 | folded | a thunk: one call to another function |
| 0x00410130 | 9 | folded | a thunk: one call to another function |
| 0x00410140 | 9 | folded | a thunk: one call to another function |
| 0x00410180 | 9 | folded | a thunk: one call to another function |
| 0x00410190 | 9 | folded | a thunk: one call to another function |
| 0x004101d0 | 9 | folded | a thunk: one call to another function |
| 0x004101e0 | 9 | folded | a thunk: one call to another function |
| 0x00410220 | 9 | folded | a thunk: one call to another function |
| 0x00410230 | 9 | folded | a thunk: one call to another function |
| 0x00410270 | 9 | folded | a thunk: one call to another function |
| 0x00410280 | 9 | folded | a thunk: one call to another function |
| 0x004102c0 | 9 | folded | a thunk: one call to another function |
| 0x004102d0 | 9 | folded | a thunk: one call to another function |
| 0x00410310 | 9 | folded | a thunk: one call to another function |
| 0x00410320 | 9 | folded | a thunk: one call to another function |
| 0x00410360 | 9 | folded | a thunk: one call to another function |
| 0x00410370 | 9 | folded | a thunk: one call to another function |
| 0x004103b0 | 9 | folded | a thunk: one call to another function |
| 0x004103c0 | 9 | folded | a thunk: one call to another function |
| 0x00410400 | 9 | folded | a thunk: one call to another function |
| 0x00410410 | 9 | folded | a thunk: one call to another function |
| 0x00410450 | 9 | folded | a thunk: one call to another function |
| 0x00410460 | 9 | folded | a thunk: one call to another function |
| 0x00410490 | 8 | folded | returns a constant or a global |
| 0x004104a0 | 12 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x004104c0 | 8 | folded | returns a constant or a global |
| 0x004104f0 | 9 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00410510 | 8 | folded | returns a constant or a global |
| 0x00410520 | 9 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00410540 | 9 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00410560 | 18 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x004105d0 | 12 | folded | covered by the port where it names it (src/mech3/zai_steps.cpp:8) |
| 0x004105f0 | 16 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:117) |
| 0x00410610 | 12 | folded | covered by the port where it names it (src/mech3/zai_brain.h:413) |
| 0x00410630 | 12 | folded | covered by the port where it names it (src/mech3/zai_brain.h:415) |
| 0x00410650 | 12 | folded | covered by the port where it names it (src/mech3/zai_brain.cpp:167) |
| 0x00410680 | 12 | folded | covered by the port where it names it (src/mech3/zai_brain.cpp:167) |
| 0x004106a0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004106d0 | 12 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x004106e0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00410920 | 18 | folded | covered by ported code beside them: the briefing's list helpers, vector inserts, the cameras' free, the options' getters (implemented in the program's hooks), the message panel's constructors and destructors, the HUD message, the AI steps' base vtable thunks, start-up statics |
| 0x00410950 | 38 | folded | covered by the port where it names it (src/mech3/controls_cfg.h:24) |
| 0x00410a20 | 16 | folded | read: the static constructor of the controls screen's groups vector (begin, end and capacity at [0x00604118..0x00604124] zeroed), which 0x00410a80 / 0x00410b70 / 0x00410f80 use (controls_cfg.cpp: controls_free_groups, controls_new_group, controls_group_count, the port's own vector) |
| 0x00410a40 | 9 | folded | a thunk: one call to another function |
| 0x00410b00 | 25 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00411020 | 12 | folded | a thunk: one call to another function |
| 0x00411040 | 9 | folded | a thunk: one call to another function |
| 0x004125a0 | 43 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00412670 | 58 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004127d0 | 88 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00412930 | 316 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00412e60 | 31 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00412ef0 | 48 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00412fa0 | 221 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00413300 | 52 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004133d0 | 11 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00413400 | 9 | folded | follows 0x0042a570: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x00413410 | 9 | folded | a thunk: one call to another function |
| 0x004137c0 | 144 | folded | covered where the port does it: src/mech3/landmines.cpp:53 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00413a70 | 14 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00413ab0 | 9 | folded | a thunk: one call to another function |
| 0x00413ac0 | 9 | folded | a thunk: one call to another function |
| 0x00413bb0 | 40 | folded | covered by the port where it names it (src/mech3/mech.h:230) |
| 0x00413c30 | 30 | folded | covered by the port where it names it (src/mech3/mech_parts.cpp:265) |
| 0x00413db0 | 34 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:799) |
| 0x00414370 | 11 | folded | ported, by docs/symbols.csv: ported (app_mech DamageCalls::roster_x1c): the roster entry's mech definition's skin set (+0x1c) |
| 0x00414f40 | 171 | folded | covered where the port does it: src/mech3/mech.h:307 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00415ae0 | 12 | folded | covered by the port where it names it (src/mech3/mech_build.cpp:101) |
| 0x00415c70 | 21 | folded | covered by the port where it names it (src/mech3/mech_build.cpp:447) |
| 0x004162b0 | 9 | folded | a thunk: one call to another function |
| 0x00416e6f | 147 | folded | a piece of the ported function before it (Ghidra split it there) (0x00416860) |
| 0x00416e91 | 144 | folded | a piece of the ported function before it (Ghidra split it there) (0x00416860) |
| 0x00417367 | 13 | folded | a piece of the ported function before it (Ghidra split it there) (0x00417300) |
| 0x00417845 | 72 | folded | a piece of the ported function before it (Ghidra split it there) (0x004173b0) |
| 0x00418fa0 | 13 | folded | covered by the port where it names it (src/mech3/mech_part_update.cpp:721) |
| 0x00419e40 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0041b8d0 | 32 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:634) |
| 0x0041ca60 | 15 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0041ca90 | 17 | debug or dev tool: PORT (preserve) | a debug key command (0x6d, case 2 of the key switch 0x0040dba0) toggling the mech's +0x634 bit 1; the release never registers 0x6d (0x0040db60 registers 0x61, 0x6c, 0x70, 0x7e) |
| 0x0041cdf0 | 10 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0041ce00 | 9 | folded | a thunk: one call to another function |
| 0x0041ce10 | 9 | folded | a thunk: one call to another function |
| 0x0041cee0 | 25 | debug or dev tool: PORT (preserve) | read: the mech's vt+0xf8 (the player's view pick: the crosshair's ground point, +0x1d48 of 0x00430aa0's object, when this is the viewed mech), called only by the AI net editor's commands 0x00402d60 and 0x00403160 (game_editor_net.cpp:146, 229) through the viewed vehicle; the port's editor answers for the stealth object only (app_vehicle.cpp:475) |
| 0x0041d1c0 | 8 | folded | ported, by docs/symbols.csv: ported; Mech::speed_at (vt+0x118): max_speed * share |
| 0x0041d200 | 8 | folded | returns a constant or a global |
| 0x0041d210 | 8 | folded | ported, by docs/symbols.csv: ported; Mech::stopping_distance (vt+0x120): speed / decel_rate |
| 0x0041d220 | 9 | folded | read: the mech's vt+0x128 (vtable 0x005bcef8, the network mech's 0x005c3508 too): vt+0x120 of +0x518 x share, the stopping distance at a throttle share; its two callers, the AI attack's 0x004c9880 and 0x004d13c0, are ported as vt+0x120 of vt+0x118(share) (zai_attack.cpp:693, 741), the same value (vt+0x118 is +0x518 x share, 0x0041d1c0) |
| 0x0041d240 | 20 | folded | ported, by docs/symbols.csv: ported; Mech::stop_share (vt+0x12c) |
| 0x0041d280 | 14 | unused: PORT (preserve) | read: the mech's vt+0x124 (the stopping distance now: /the controls' speed x +0x518/ through vt+0x120); its one caller, the field base dock 0x00459990 (mech_fieldbase_dock.cpp:152), calls it on the base, a vehicle (0x004ad050, ported), so on a mech nothing reaches it (the calls through +0x124 in the exe: that one only) |
| 0x0041d2c0 | 19 | unused: PORT (preserve) | read: the mech's vt+0x130 (a turn rate from a speed and a share, floored), vt+0x134 and vt+0x138 (it from the controls' speed or a share) and vt+0x13c (the share that turns by a rate): no call in the exe goes through +0x134, +0x138 or +0x13c, and +0x130's callers are those two and the vehicles' own (0x004ad250, 0x004ad290 on their vtable), so on a mech nothing reaches them |
| 0x0041d300 | 9 | unused: PORT (preserve) | read: the mech's vt+0x130 (a turn rate from a speed and a share, floored), vt+0x134 and vt+0x138 (it from the controls' speed or a share) and vt+0x13c (the share that turns by a rate): no call in the exe goes through +0x134, +0x138 or +0x13c, and +0x130's callers are those two and the vehicles' own (0x004ad250, 0x004ad290 on their vtable), so on a mech nothing reaches them |
| 0x0041d320 | 20 | unused: PORT (preserve) | read: the mech's vt+0x130 (a turn rate from a speed and a share, floored), vt+0x134 and vt+0x138 (it from the controls' speed or a share) and vt+0x13c (the share that turns by a rate): no call in the exe goes through +0x134, +0x138 or +0x13c, and +0x130's callers are those two and the vehicles' own (0x004ad250, 0x004ad290 on their vtable), so on a mech nothing reaches them |
| 0x0041d380 | 24 | unused: PORT (preserve) | read: the mech's vt+0x130 (a turn rate from a speed and a share, floored), vt+0x134 and vt+0x138 (it from the controls' speed or a share) and vt+0x13c (the share that turns by a rate): no call in the exe goes through +0x134, +0x138 or +0x13c, and +0x130's callers are those two and the vehicles' own (0x004ad250, 0x004ad290 on their vtable), so on a mech nothing reaches them |
| 0x0041d480 | 9 | folded | covered by the port where it names it (src/mech3/app_hud.cpp:473) |
| 0x0041d490 | 10 | folded | ported, by docs/symbols.csv: ported inline: thrust x 8 + gravity (JumpState) |
| 0x0041d4b0 | 10 | folded | ported, by docs/symbols.csv: ported inline: (accel - gravity) / 8 (JumpState) |
| 0x0041d6e0 | 10 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0041dc40 | 12 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0041dd90 | 52 | unused: PORT (preserve) | read: the mech's vt+0x94 (a world direction clamped into the torso's arc through 0x00465c70, the vehicles' 0x004ad7f0 counterpart): the 20 calls through +0x94 in the exe are on other classes (IControls' on_hit, the 2D widgets, the behaviours' attack test, the edit bar, a part), none on a mech; the port's aim uses vt+0x90 (Mech::clamp_aim, 0x0041dca0) |
| 0x0041de80 | 20 | folded | covered by the port where it names it (src/mech3/mech_build.cpp:438) |
| 0x0041df10 | 10 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0041e630 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0041e9a0 | 9 | folded | a thunk: one call to another function |
| 0x0041e9b0 | 10 | folded | returns a constant or a global |
| 0x0041e9e0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0041eb50 | 12 | folded | covered by the port where it names it (src/gamez/ztwod/twod_console.cpp:81) |
| 0x0041eb90 | 9 | folded | a thunk: one call to another function |
| 0x0041ebb0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0041ebd0 | 16 | folded | covered by the port where it names it (src/mech3/mech_repair.cpp:60) |
| 0x0041ec00 | 17 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:1004) |
| 0x0041ec30 | 8 | folded | returns a constant or a global |
| 0x0041ec40 | 9 | folded | covered by the port where it names it (src/mech3/mech_config.h:68) |
| 0x0041ec70 | 8 | folded | covered by the port where it names it (src/mech3/mech.h:186) |
| 0x0041ec80 | 8 | folded | returns a constant or a global |
| 0x0041ec90 | 8 | folded | returns a constant or a global |
| 0x0041ecc0 | 9 | folded | a thunk: one call to another function |
| 0x0041ece0 | 9 | folded | a thunk: one call to another function |
| 0x0041ecf0 | 13 | folded | covered by the port where it names it (src/mech3/mech.h:74) |
| 0x0041ed10 | 22 | folded | covered by the port where it names it (src/mech3/mech_anim.h:27) |
| 0x0041ed70 | 22 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0041edd0 | 24 | folded | covered by the port where it names it (src/mech3/mech_anim.cpp:315) |
| 0x0041ee50 | 12 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0041eee0 | 11 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0041f160 | 9 | folded | a thunk: one call to another function |
| 0x0041f800 | 26 | folded | covered by the port where it names it (src/mech3/mech_anim.h:27) |
| 0x0041fb30 | 22 | folded | covered by the port where it names it (src/mech3/zainet_inout.cpp:333) |
| 0x0041fef0 | 15 | folded | covered by the port where it names it (src/mech3/mech_anim.cpp:379) |
| 0x00420020 | 24 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x0042004d | 24 | folded | follows 0x00420020: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00420df0 | 23 | folded | covered by the port where it names it (src/mech3/mech_anim.cpp:640) |
| 0x00420e60 | 22 | folded | covered by the port where it names it (src/mech3/mech_data.cpp:547) |
| 0x00421240 | 10 | folded | a getter of a member |
| 0x00421250 | 10 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00421260 | 10 | folded | a getter of a member |
| 0x00421270 | 10 | folded | returns a constant or a global |
| 0x00421280 | 10 | folded | a getter of a member |
| 0x00421290 | 11 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x004212a0 | 10 | folded | returns a constant or a global |
| 0x004212b0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x00421500 | 10 | folded | a getter of a member |
| 0x00421510 | 11 | folded | covered by the port where it names it (src/mech3/mech_anim.cpp:32) |
| 0x00421520 | 9 | folded | covered by the port where it names it (src/mech3/mech_anim.cpp:33) |
| 0x004215a0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004215c0 | 12 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x004231a0 | 11 | folded | a thunk: one call to another function |
| 0x004231b0 | 11 | folded | a thunk: one call to another function |
| 0x00423cf0 | 8 | folded | returns a constant or a global |
| 0x00424290 | 8 | folded | returns a constant or a global |
| 0x004243c0 | 19 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:521) |
| 0x00424420 | 19 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:553) |
| 0x00424f30 | 11 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:829) |
| 0x004251d0 | 71 | folded | covered where the port does it: src/mech3/mech_config.cpp:698 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004255a0 | 13 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:552) |
| 0x00425b70 | 46 | folded | covered where the port does it: src/mech3/mp_lobby.cpp:990 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00425e00 | 13 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00425e20 | 13 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00426070 | 8 | folded | returns a constant or a global |
| 0x00426080 | 10 | folded | covered by the port where it names it (src/mech3/mp_lobby.cpp:968) |
| 0x00426090 | 12 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:888) |
| 0x004260c0 | 40 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:28) |
| 0x00426180 | 77 | folded | the mech lab's Ctrl+X / Ctrl+I loadout export and import through Windows' file dialogs, replaced by the port's FIX (A) (export\<mech>_<loadout>.m3c; fixes.md, mechlab.md section 1) |
| 0x00426300 | 79 | folded | the mech lab's Ctrl+X / Ctrl+I loadout export and import through Windows' file dialogs, replaced by the port's FIX (A) (export\<mech>_<loadout>.m3c; fixes.md, mechlab.md section 1) |
| 0x00426440 | 23 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00426470 | 25 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00426540 | 13 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:370) |
| 0x00426560 | 13 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:1978) |
| 0x00426ae0 | 9 | folded | a thunk: one call to another function |
| 0x00426af0 | 10 | folded | returns a constant or a global |
| 0x00426b00 | 10 | folded | returns a constant or a global |
| 0x00426b10 | 11 | folded | covered by the port where it names it (src/mech3/mech_control.h:101) |
| 0x00426b20 | 12 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00426b40 | 9 | folded | follows 0x00426b20: covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00426bf0 | 14 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00427410 | 32 | folded | covered by the port where it names it (src/mech3/mech_pocket.cpp:59) |
| 0x00428440 | 15 | folded | ported, by docs/symbols.csv: ported: PlayerControls::set_record (IControls vt+0x88) |
| 0x00428470 | 13 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:506) |
| 0x00428500 | 10 | folded | covered by the port where it names it (src/mech3/mech_control.h:181) |
| 0x00428510 | 10 | folded | returns a constant or a global |
| 0x00428520 | 10 | folded | returns a constant or a global |
| 0x00428530 | 10 | folded | returns a constant or a global |
| 0x00428540 | 10 | folded | returns a constant or a global |
| 0x00428550 | 10 | folded | returns a constant or a global |
| 0x00428560 | 11 | folded | covered by the port where it names it (src/mech3/mech_control.cpp:646) |
| 0x00428570 | 11 | folded | covered by the port where it names it (src/mech3/mech_control.cpp:647) |
| 0x00428580 | 11 | folded | covered by the port where it names it (src/mech3/mech_control.cpp:637) |
| 0x00428740 | 11 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00428750 | 9 | folded | covered by the port where it names it (src/mech3/mech_build.h:99) |
| 0x00428760 | 12 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00428780 | 15 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x004287b0 | 10 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x004287c0 | 14 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x00428c70 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00428c90 | 54 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x00428da0 | 40 | folded | covered by the port where it names it (src/mech3/credits.h:90) |
| 0x00428e60 | 66 | folded | covered where the port does it: src/mech3/credits.h:106 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00428ff0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00429010 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004295a0 | 20 | folded | covered by the port where it names it (src/mech3/credits.h:34) |
| 0x004295e0 | 12 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:501) |
| 0x00429690 | 20 | folded | covered by the port where it names it (src/mech3/credits.h:34) |
| 0x004296d0 | 11 | folded | covered by the port where it names it (src/mech3/credits.h:61) |
| 0x00429700 | 135 | folded | covered where the port does it: src/mech3/credits.h:41 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00429a60 | 9 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00429a70 | 137 | folded | covered where the port does it: src/mech3/credits.h:41 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00429ce0 | 18 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00429d20 | 27 | folded | covered by the port where it names it (src/mech3/credits.h:61) |
| 0x00429d90 | 13 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00429dc0 | 13 | folded | covered by the port where it names it (src/mech3/credits.h:73) |
| 0x00429df0 | 43 | folded | covered where the port does it: src/mech3/credits.h:25 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00429eb0 | 118 | folded | covered where the port does it: src/mech3/credits.h:11 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0042a130 | 8 | folded | returns a constant or a global |
| 0x0042a140 | 14 | folded | sets members (a setter or a small constructor) |
| 0x0042a430 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042a570 | 16 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042adb0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042add0 | 54 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042aee0 | 54 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042aff0 | 54 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042b100 | 54 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042b210 | 52 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042b320 | 52 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042b430 | 52 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042b540 | 52 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042b650 | 52 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042b770 | 54 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042bc30 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042bc50 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042bc70 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042bc90 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042bcb0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042bcd0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042bcf0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042bd10 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042bd30 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042bd50 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042bd70 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042c7a0 | 25 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042ca70 | 25 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042cc10 | 25 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d210 | 31 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d2b0 | 37 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d420 | 11 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d450 | 13 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d480 | 9 | folded | a thunk: one call to another function |
| 0x0042d4a0 | 11 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d4d0 | 28 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d530 | 10 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d550 | 28 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d5b0 | 13 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d5e0 | 12 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d610 | 11 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d6f0 | 10 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d710 | 10 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d730 | 49 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d890 | 16 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d8c0 | 16 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d8f0 | 24 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d960 | 24 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042d9d0 | 32 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042da50 | 12 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042da70 | 9 | folded | a thunk: one call to another function |
| 0x0042df60 | 49 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042dff0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042e010 | 54 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e120 | 54 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e230 | 79 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042e430 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042e450 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042eb50 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042eb70 | 54 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042ec80 | 54 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042eda0 | 79 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042efa0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042efc0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0042f3a0 | 15 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042f3d0 | 26 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042f430 | 14 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042f450 | 15 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042f7a0 | 33 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x0042f840 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00430035 | 8 | folded | a piece of the ported function before it (Ghidra split it there) (0x0042fff0) |
| 0x00430062 | 8 | folded | a piece of the ported function before it (Ghidra split it there) (0x0042fff0) |
| 0x00430080 | 8 | folded | empty: does nothing |
| 0x00430250 | 22 | folded | the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x004302b0 | 9 | folded | a thunk: one call to another function |
| 0x004303c0 | 9 | folded | a thunk: one call to another function |
| 0x00430e30 | 19 | folded | covered by the port where it names it (src/mech3/mech_build.cpp:259) |
| 0x00430e50 | 9 | folded | a thunk: one call to another function |
| 0x00430e60 | 11 | folded | a thunk: one call to another function |
| 0x004317d0 | 19 | folded | covered by the port where it names it (src/mech3/mech_data.cpp:243) |
| 0x004319f0 | 26 | folded | covered by the port where it names it (src/mech3/mech_data.cpp:332) |
| 0x004326d0 | 74 | folded | covered where the port does it: src/mech3/mech_data.cpp:219 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00432940 | 11 | folded | covered by the port where it names it (src/mech3/app_vehicle.cpp:538) |
| 0x00432a50 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00432a80 | 40 | folded | covered by the port where it names it (src/gamez/zsiscript/si_script.cpp:156) |
| 0x00432b60 | 23 | dev dialog (MFC), its window plumbing: replaced | the mech list dialog's MFC plumbing (constructor, destructor, DDX, OnOK, DoModal) |
| 0x00432bc0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00432be0 | 21 | dev dialog (MFC), its window plumbing: replaced | the mech list dialog's MFC plumbing (constructor, destructor, DDX, OnOK, DoModal) |
| 0x00432c30 | 9 | dev dialog (MFC), its window plumbing: replaced | the mech list dialog's MFC plumbing (constructor, destructor, DDX, OnOK, DoModal) |
| 0x00432c50 | 8 | folded | returns a constant or a global |
| 0x00432c60 | 34 | dev dialog (MFC), its logic: PORT (preserve) | a mech list dialog (resource 0x9c, from a debug key command in 0x0040db60): its fill of every mech name (0x00484650) |
| 0x00432d30 | 27 | dev dialog (MFC), its logic: PORT (preserve) | a mech list dialog (resource 0x9c, from a debug key command in 0x0040db60): its fill of every mech name (0x00484650) |
| 0x00432da0 | 16 | dev dialog (MFC), its window plumbing: replaced | the mech list dialog's MFC plumbing (constructor, destructor, DDX, OnOK, DoModal) |
| 0x00432df0 | 14 | dev dialog (MFC), its window plumbing: replaced | the mech list dialog's MFC plumbing (constructor, destructor, DDX, OnOK, DoModal) |
| 0x00432e10 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00432e70 | 16 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:2026) |
| 0x00432e90 | 16 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:2026) |
| 0x00432f70 | 13 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:398) |
| 0x00432fb0 | 16 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:553) |
| 0x00432ff0 | 14 | folded | covered by the port where it names it (src/mech3/mech_config.cpp:521) |
| 0x004338a0 | 16 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x004338d0 | 16 | folded | covered by the port where it names it (src/mech3/app_force.cpp:25) |
| 0x00433900 | 16 | folded | covered by the port where it names it (src/mech3/app_force.cpp:29) |
| 0x00433930 | 16 | folded | ported in force_feedback.cpp:172 (water: wading) and app_sensors.cpp:141 (a vehicle sees its target), whose hook fields are allowed null only in tests |
| 0x00433960 | 17 | folded | covered by the port where it names it (src/mech3/force_feedback.cpp:58) |
| 0x00433990 | 13 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00433a70 | 16 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00433ab0 | 34 | folded | covered by ported code beside them: controls file and landmine helpers, getters and setters, the mech's IControls cast, the animation's members, list helpers, constructors and destructors, the force feedback's release and reset (input.md) |
| 0x00433b30 | 16 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00433ca0 | 12 | folded | covered by the port where it names it (src/mech3/pilot.cpp:238) |
| 0x00433cf0 | 28 | folded | covered by the port where it names it (src/mech3/pilot_games.cpp:718) |
| 0x00433ee0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x00434010 | 16 | folded | covered by the port where it names it (src/mech3/pilot_games.cpp:586) |
| 0x00434040 | 106 | folded | covered where the port does it: src/mech3/pilot_games.cpp:610 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00434270 | 35 | folded | covered by the port where it names it (src/mech3/pilot_games.cpp:707) |
| 0x00434720 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00435230 | 11 | folded | ported, by docs/symbols.csv: ported: set_lancemate_count (+0x9c) |
| 0x004354e0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00436500 | 65 | folded | covered where the port does it: src/mech3/pilot.h:371 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00436740 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00436950 | 62 | folded | covered where the port does it: src/mech3/pilot.h:289 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004369c0 | 62 | folded | covered where the port does it: src/mech3/pilot.h:289 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00436a30 | 8 | folded | covered by the port where it names it (src/mech3/briefing.cpp:795) |
| 0x00436a40 | 25 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00436a80 | 12 | folded | covered by the port where it names it (src/mech3/action_menu.cpp:488) |
| 0x00436ef0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004370d0 | 33 | folded | covered by the port where it names it (src/mech3/pilot.h:441) |
| 0x004373b0 | 12 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004374b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00437550 | 14 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00437580 | 20 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004375e0 | 63 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00437700 | 27 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x00437780 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004377a0 | 24 | folded | covered by the port where it names it (src/mech3/pilot.h:145) |
| 0x00437820 | 11 | folded | covered by the port where it names it (src/mech3/pilot.h:207) |
| 0x00437840 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00437860 | 8 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00437870 | 8 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:2311) |
| 0x00437880 | 8 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:2311) |
| 0x00437890 | 8 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:842) |
| 0x004378f0 | 8 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:843) |
| 0x00437910 | 8 | folded | covered by the port where it names it (src/mech3/pilot.h:369) |
| 0x00437930 | 8 | folded | covered by the port where it names it (src/mech3/pilot.h:373) |
| 0x00437940 | 8 | folded | covered by the port where it names it (src/mech3/pilot.h:369) |
| 0x00437960 | 8 | folded | returns a constant or a global |
| 0x00437970 | 8 | folded | covered by the port where it names it (src/mech3/pilot.h:276) |
| 0x00437990 | 13 | folded | covered by the port where it names it (src/mech3/pilot.h:437) |
| 0x004379b0 | 13 | folded | covered by the port where it names it (src/mech3/pilot.h:437) |
| 0x004379d0 | 9 | folded | a thunk: one call to another function |
| 0x004379e0 | 9 | folded | covered by the port where it names it (src/mech3/pilot.h:169) |
| 0x00438090 | 12 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:248) |
| 0x004380e0 | 9 | folded | a thunk: one call to another function |
| 0x00438470 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00438490 | 98 | folded | covered where the port does it: src/mech3/options_menu.h:66 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00438720 | 54 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x00438830 | 54 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x00438940 | 54 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x00438a50 | 54 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x00438b60 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00438b80 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00438ba0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00438bc0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00438c60 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00438cc0 | 40 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00439490 | 12 | folded | covered by the port where it names it (src/mech3/options_menu.cpp:269) |
| 0x00439cc0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00439ce0 | 54 | folded | covered where the port does it: src/mech3/options_menu.h:198 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00439e50 | 54 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x00439f60 | 54 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x0043a070 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0043a090 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0043a0b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0043a640 | 32 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:474) |
| 0x0043a870 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0043aae0 | 12 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043ae30 | 15 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043aed0 | 9 | folded | a thunk: one call to another function |
| 0x0043aef0 | 9 | folded | a thunk: one call to another function |
| 0x0043af10 | 9 | folded | a thunk: one call to another function |
| 0x0043af30 | 9 | folded | a thunk: one call to another function |
| 0x0043af50 | 9 | folded | a thunk: one call to another function |
| 0x0043af70 | 9 | folded | a thunk: one call to another function |
| 0x0043af90 | 9 | folded | a thunk: one call to another function |
| 0x0043afb0 | 9 | folded | follows 0x0043ae30: the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043b530 | 24 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043b5a0 | 39 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043b690 | 23 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043b700 | 47 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043b800 | 22 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043cb10 | 46 | debug or dev tool: PORT (preserve) | the HUD's debug overlays (docs/hud.md section 11: the mech's gait, speeds and the like into five debug texts while the debug text shows, command 0x82); hooked in the port (hud_debug, unported) |
| 0x0043cc00 | 41 | debug or dev tool: PORT (preserve) | the HUD's debug overlays (docs/hud.md section 11: the mech's gait, speeds and the like into five debug texts while the debug text shows, command 0x82); hooked in the port (hud_debug, unported) |
| 0x0043cca0 | 23 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043d240 | 9 | folded | a thunk: one call to another function |
| 0x0043d250 | 9 | folded | a thunk: one call to another function |
| 0x0043d600 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0043d810 | 41 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043dce0 | 18 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043f4e0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0043f870 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0043fa20 | 13 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0043fca0 | 12 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004400c0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00441d90 | 8 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00441da0 | 77 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00441f70 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00441f90 | 36 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00442790 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00442850 | 14 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00442ce0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00442d00 | 39 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00442df0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x00443ca0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00443cf0 | 9 | folded | a thunk: one call to another function |
| 0x00444330 | 13 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00444550 | 40 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x0044464d | 14 | folded | a piece of the ported function before it (Ghidra split it there) (0x004445f0) |
| 0x00444670 | 15 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00444690 | 10 | folded | a getter of a member |
| 0x00444da0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00444e55 | 132 | folded | a piece of the ported function before it (Ghidra split it there) (0x00444dc0) |
| 0x00445950 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004460a0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004464c0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x00446800 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00447510 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00447530 | 12 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00447560 | 22 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00447800 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00447820 | 69 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00447bf0 | 10 | folded | a getter of a member |
| 0x00447d80 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00447da0 | 24 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00448120 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004484b0 | 17 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00448500 | 11 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00448530 | 14 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00448600 | 16 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x00448760 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00448780 | 12 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004487d0 | 8 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004487f0 | 9 | folded | a thunk: one call to another function |
| 0x00448800 | 9 | folded | a thunk: one call to another function |
| 0x004488b0 | 31 | folded | covered by the port where it names it (src/mech3/app_states.h:252) |
| 0x00448910 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004490d0 | 25 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00449140 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00449570 | 54 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00449760 | 137 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044a7d0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044a7f0 | 20 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0044ab60 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044b010 | 15 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044b050 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044b210 | 12 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044c2f0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044c320 | 32 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044c3d0 | 74 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044c5c0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044c5e0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044cf40 | 9 | folded | a thunk: one call to another function |
| 0x0044e360 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044e380 | 20 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e490 | 19 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e5c0 | 9 | folded | a thunk: one call to another function |
| 0x0044e5e0 | 23 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e680 | 9 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e6a0 | 9 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e6c0 | 8 | folded | returns a constant or a global |
| 0x0044e6d0 | 14 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e7b0 | 22 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044e800 | 147 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044ea70 | 55 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044eb30 | 98 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044ec90 | 15 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044ecc0 | 10 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044ecd0 | 8 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044ece0 | 28 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044ed40 | 14 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044ed70 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044ed90 | 15 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044edc0 | 22 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044ee00 | 29 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044ef90 | 40 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044f1c0 | 9 | folded | a thunk: one call to another function |
| 0x0044f1d0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044f1f0 | 20 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044f260 | 15 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044f290 | 14 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044f4f0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044f510 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044f5f0 | 29 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044f670 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044f690 | 87 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044f890 | 64 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044fa10 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044fa30 | 29 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044fab0 | 12 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044fad0 | 15 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044fb00 | 44 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044fc00 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044fc20 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0044fc40 | 29 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0044fcc0 | 15 | folded | ported, by docs/symbols.csv: ported: the entry (new(0x14)), +0x08 the mech, +0x10 -1 |
| 0x0044fce0 | 22 | folded | ported, by docs/symbols.csv: ported in lab_world_unload: the tree and the camo |
| 0x0044fea0 | 47 | folded | ported, by docs/symbols.csv: ported inline in lab_place_window: the window wrapper's area, size and place |
| 0x004513f0 | 23 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00451530 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00451550 | 28 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004516a0 | 100 | folded | covered where the port does it: src/mech3/lab_items.cpp:64 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00451cc0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00452de0 | 55 | folded | covered where the port does it: src/mech3/lab_screen.cpp:842 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00453500 | 24 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004537f0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00453810 | 8 | folded | returns a constant or a global |
| 0x00453820 | 8 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00453830 | 9 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x0045434e | 132 | folded | a piece of the ported function before it (Ghidra split it there) (0x00453f20) |
| 0x00454b49 | 150 | folded | a piece of the ported function before it (Ghidra split it there) (0x00454600) |
| 0x00454c42 | 80 | folded | a piece of the ported function before it (Ghidra split it there) (0x00454600) |
| 0x004552e0 | 12 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00455300 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00455320 | 9 | folded | a thunk: one call to another function |
| 0x00455340 | 9 | folded | a thunk: one call to another function |
| 0x00455360 | 34 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00455eb0 | 11 | folded | a thunk: one call to another function |
| 0x00455f90 | 11 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x004561a0 | 146 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x004565f0 | 82 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x004567e0 | 12 | folded | covered by the port where it names it (src/mech3/lab_items.cpp:125) |
| 0x00456840 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00456860 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00456880 | 29 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00456900 | 29 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00456980 | 9 | folded | a thunk: one call to another function |
| 0x004569a0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00456a40 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00456fa0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00456fc0 | 42 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00457120 | 9 | folded | a thunk: one call to another function |
| 0x00457860 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00457880 | 42 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00458010 | 22 | folded | the mech lab (lab_screen/lab_view/mech_config, whose comments and mechlab.md name these): its controls' constructors and destructors (the CRT callers are exception unwinds), the store rows' std::sort instances (name and tonnage comparators inlined), item kinds' constant getters, list helpers |
| 0x00458080 | 9 | folded | a thunk: one call to another function |
| 0x00458090 | 9 | folded | a thunk: one call to another function |
| 0x004581e0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00458880 | 9 | folded | a thunk: one call to another function |
| 0x004589a0 | 15 | folded | ported, by docs/symbols.csv: ported: FieldBaseArms::aim (1 open, 0 shut) |
| 0x00458b50 | 26 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004598a0 | 14 | folded | ported, by docs/symbols.csv: ported (vtable 0x005c2540) |
| 0x004598c0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x00459b50 | 9 | folded | a thunk: one call to another function |
| 0x00459b60 | 9 | folded | a thunk: one call to another function |
| 0x00459db0 | 35 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0045a760 | 8 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.h:81) |
| 0x0045a770 | 10 | folded | ported, by docs/symbols.csv: ported |
| 0x0045ab30 | 15 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0045add0 | 8 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.h:90) |
| 0x0045aeb0 | 23 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.h:105) |
| 0x0045b2c0 | 18 | folded | ported, by docs/symbols.csv: ported |
| 0x0045b620 | 21 | folded | ported, by docs/symbols.csv: ported |
| 0x0045b7a0 | 9 | folded | ported, by docs/symbols.csv: ported: DockStep::message (abort, then the message) |
| 0x0045b7b0 | 12 | folded | ported, by docs/symbols.csv: ported: DockStep::abort (0x0047a720, finished) |
| 0x0045b800 | 31 | folded | ported, by docs/symbols.csv: ported as push_back: std::list node (0xc) before the head, count +1 |
| 0x0045b860 | 35 | folded | ported, by docs/symbols.csv: ported inline: the order deque's iterator (the brain's current order) |
| 0x0045b900 | 16 | folded | covered by the port where it names it (src/mech3/mech_fieldbase_cut.cpp:30) |
| 0x0045b950 | 10 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:892) |
| 0x0045b960 | 10 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0045b970 | 8 | folded | covered by the port where it names it (src/mech3/app_vehicle.h:134) |
| 0x0045b990 | 8 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.h:34) |
| 0x0045b9a0 | 8 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.h:61) |
| 0x0045b9b0 | 8 | folded | ported, by docs/symbols.csv: ported: DockStep::done |
| 0x0045bae0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0045c8f0 | 16 | folded | ported, by docs/symbols.csv: ported; (ECX args) nav by name via [0x00609980] 0x00479be0 |
| 0x0045e410 | 59 | folded | covered where the port does it: src/mech3/mech_objective.h:188 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0045e650 | 25 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x0045e700 | 18 | folded | covered by the port where it names it (src/mech3/mech_objective.cpp:234) |
| 0x0045e730 | 18 | folded | covered by the port where it names it (src/mech3/mech_objective.cpp:235) |
| 0x0045e760 | 18 | folded | covered by the port where it names it (src/mech3/mech_objective.cpp:234) |
| 0x0045e800 | 19 | folded | covered by the port where it names it (src/mech3/mech_objective.cpp:257) |
| 0x0045e850 | 15 | folded | covered by the port where it names it (src/mech3/mech_objective.cpp:258) |
| 0x0045eb70 | 41 | folded | covered where the port does it: src/mech3/mech_objective.h:150 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0045ec40 | 13 | folded | ported, by docs/symbols.csv: ported; the store when the node's +0x24 bit 2 matches |
| 0x0045ec60 | 41 | folded | ported, by docs/symbols.csv: ported; into [0x00609990], then freed |
| 0x0045f150 | 9 | folded | ported, by docs/symbols.csv: ported; vt+0x04 = 0x0045e540(0) |
| 0x0045f160 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0045f200 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0045f2a0 | 8 | folded | ported, by docs/symbols.csv: ported; vt+0x04 of the base: done |
| 0x0045f320 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0045f520 | 58 | folded | covered where the port does it: src/mech3/mech_objective.cpp:202 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0045f8c0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0045fa00 | 23 | folded | covered by the port where it names it (src/mech3/mech_wake.cpp:21) |
| 0x0045fd20 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00460000 | 21 | folded | covered by the port where it names it (src/mech3/app_mech.h:329) |
| 0x00460001 | 20 | folded | follows 0x00460000: covered by the port where it names it (src/mech3/app_mech.h:329) |
| 0x00460038 | 8 | folded | follows 0x00460000: covered by the port where it names it (src/mech3/app_mech.h:329) |
| 0x004600b0 | 20 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:789) |
| 0x004600e0 | 40 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:807) |
| 0x00461670 | 54 | folded | covered where the port does it: src/mech3/app_weapons.cpp:229 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00461cf0 | 19 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:914) |
| 0x00461d30 | 17 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:749) |
| 0x00462000 | 21 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:758) |
| 0x00462840 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00462970 | 10 | folded | a getter of a member |
| 0x00463720 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00463e30 | 9 | folded | a thunk: one call to another function |
| 0x00464230 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004646f0 | 14 | folded | sets members (a setter or a small constructor) |
| 0x004647b0 | 9 | folded | a thunk: one call to another function |
| 0x004647d0 | 9 | folded | a thunk: one call to another function |
| 0x004647f0 | 17 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00465bb0 | 10 | folded | ported, by docs/symbols.csv: ported inline; +0x98 |
| 0x00466360 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00466ba0 | 28 | folded | ported, by docs/symbols.csv: ported; ZAPS_HUD kind 3 |
| 0x00466da0 | 18 | folded | covered by the port where it names it (src/mech3/mech_build.h:183) |
| 0x00466df0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00466ee0 | 12 | folded | covered by the port where it names it (src/mech3/mech_part_build.cpp:160) |
| 0x004670a0 | 22 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00467180 | 10 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004671d0 | 8 | folded | returns a constant or a global |
| 0x00467200 | 28 | folded | ported, by docs/symbols.csv: ported; ZAPS_HUD kind 4 |
| 0x00467310 | 13 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00467330 | 10 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00467340 | 8 | folded | returns a constant or a global |
| 0x00467350 | 8 | folded | returns a constant or a global |
| 0x00467360 | 8 | folded | returns a constant or a global |
| 0x00467370 | 8 | folded | returns a constant or a global |
| 0x00467380 | 8 | folded | returns a constant or a global |
| 0x00467390 | 10 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004673a0 | 10 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004673b0 | 11 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004673e0 | 10 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00467410 | 11 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00467470 | 8 | folded | returns a constant or a global |
| 0x00467480 | 9 | folded | a thunk: one call to another function |
| 0x004674a0 | 8 | folded | empty: does nothing |
| 0x004674c0 | 8 | folded | empty: does nothing |
| 0x004674e0 | 8 | folded | empty: does nothing |
| 0x00467c80 | 9 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00467e00 | 11 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:47) |
| 0x00467e30 | 16 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:47) |
| 0x00467e60 | 16 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00467e90 | 48 | folded | ported, by docs/symbols.csv: ported: the panel's line adder |
| 0x00467f70 | 13 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:55) |
| 0x00467fa0 | 50 | folded | ported, by docs/symbols.csv: ported: a lancemate's line |
| 0x00468090 | 10 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:55) |
| 0x004680c0 | 15 | folded | ported, by docs/symbols.csv: ported: an order's line |
| 0x00468100 | 65 | folded | covered where the port does it: src/mech3/app_pocket.cpp:90 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004686e0 | 14 | folded | ported, by docs/symbols.csv: ported: the colours (pocket_colours) |
| 0x00468bd0 | 15 | folded | ported, by docs/symbols.csv: ported: the record (member pointer, flags +0xc taken / +0xd refused) |
| 0x00468c00 | 45 | folded | ported, by docs/symbols.csv: ported: pocket_order's unit step (contact on the player) |
| 0x00468df0 | 17 | folded | ported, by docs/symbols.csv: ported: the record with the target at +0x10 |
| 0x00468e20 | 48 | folded | ported, by docs/symbols.csv: ported: pocket_order's unit step (on the target) |
| 0x00468fa0 | 41 | folded | ported, by docs/symbols.csv: ported: pocket_order (on a contact made on the target) |
| 0x004692f0 | 12 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x00469930 | 9 | folded | a thunk: one call to another function |
| 0x0046a3f0 | 12 | folded | ported, by docs/symbols.csv: ported: pocket_draw's |
| 0x0046a570 | 13 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:271) |
| 0x0046a5a0 | 27 | folded | ported, by docs/symbols.csv: ported |
| 0x0046a640 | 13 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:271) |
| 0x0046a670 | 27 | folded | ported, by docs/symbols.csv: ported |
| 0x0046a710 | 13 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:272) |
| 0x0046a740 | 19 | folded | ported, by docs/symbols.csv: ported |
| 0x0046a780 | 13 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:272) |
| 0x0046a7e0 | 13 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:272) |
| 0x0046a810 | 21 | folded | ported, by docs/symbols.csv: ported |
| 0x0046aa40 | 20 | folded | ported, by docs/symbols.csv: ported: pocket_mission_end's |
| 0x0046ab40 | 9 | folded | follows 0x0046ab70: covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0046ab50 | 9 | folded | a thunk: one call to another function |
| 0x0046ab70 | 12 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0046ab90 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0046b740 | 15 | folded | covered by the port where it names it (src/mech3/pocket_panel.cpp:380) |
| 0x0046b780 | 13 | folded | ported, by docs/symbols.csv: ported: vt+0x98; 1 active, else 0 (shared by the linker) |
| 0x0046cc00 | 47 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0046cca0 | 34 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0046cd40 | 22 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0046cda0 | 9 | folded | a thunk: one call to another function |
| 0x0046d0b0 | 18 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0046d0d0 | 9 | folded | a thunk: one call to another function |
| 0x0046d380 | 15 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0046dd90 | 32 | folded | covered by the port where it names it (src/mech3/mech_net.cpp:1017) |
| 0x0046e030 | 24 | folded | covered by the port where it names it (src/mech3/mech_net.cpp:1086) |
| 0x0046e3c0 | 56 | folded | covered where the port does it: src/mech3/mech_net.cpp:1184 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0046ed60 | 9 | folded | a thunk: one call to another function |
| 0x0046ef50 | 8 | folded | returns a constant or a global |
| 0x00470008 | 33 | folded | a piece of the ported function before it (Ghidra split it there) (0x0046fff0) |
| 0x00470640 | 9 | folded | a thunk: one call to another function |
| 0x00470650 | 17 | folded | covered by the port where it names it (src/mech3/app_states.h:84) |
| 0x00470690 | 18 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004706d0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004706f0 | 9 | folded | a thunk: one call to another function |
| 0x004709c0 | 9 | folded | a thunk: one call to another function |
| 0x00470bc0 | 23 | folded | covered by the port where it names it (src/mech3/mech_net.cpp:309) |
| 0x00470c20 | 14 | folded | covered by the port where it names it (src/mech3/mech_net.cpp:516) |
| 0x00470cc0 | 8 | folded | returns a constant or a global |
| 0x00470cd0 | 8 | folded | returns a constant or a global |
| 0x00472320 | 8 | folded | returns a constant or a global |
| 0x004729d0 | 26 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004730d0 | 10 | folded | covered by the port where it names it (src/mech3/mech_config.h:34) |
| 0x004731e0 | 19 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00473740 | 19 | folded | covered by the port where it names it (src/mech3/app_hooks.h:945) |
| 0x00473780 | 39 | folded | covered by the port where it names it (src/mech3/mech_salvage.cpp:149) |
| 0x00473860 | 21 | folded | covered by the port where it names it (src/mech3/mech_salvage.cpp:148) |
| 0x004738b0 | 19 | folded | ported, by docs/symbols.csv: ported: item_label |
| 0x00473900 | 55 | folded | covered where the port does it: src/mech3/mech_salvage.cpp:100 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00473960 | 55 | folded | covered where the port does it: src/mech3/mech_salvage.cpp:101 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004739c0 | 9 | folded | a thunk: one call to another function |
| 0x004739d0 | 19 | folded | ported, by docs/symbols.csv: ported: item_name; 0x00432f00 maker rating XL |
| 0x00473a20 | 8 | folded | returns a constant or a global |
| 0x00473ae0 | 17 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00473b10 | 9 | folded | a thunk: one call to another function |
| 0x00474ef0 | 18 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00474f10 | 9 | folded | a thunk: one call to another function |
| 0x00475bf0 | 18 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00475c20 | 14 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00475c50 | 21 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00475d10 | 35 | folded | covered by the port where it names it (src/mech3/mech_part_update.cpp:348) |
| 0x00475f20 | 30 | folded | covered by the port where it names it (src/mech3/mech_pose.cpp:228) |
| 0x00476460 | 22 | folded | covered by the port where it names it (src/mech3/mech_build.cpp:254) |
| 0x004779f0 | 17 | folded | covered by the port where it names it (src/mech3/pilot.h:17) |
| 0x00477a20 | 9 | folded | a thunk: one call to another function |
| 0x00477ac0 | 22 | folded | covered by the port where it names it (src/mech3/pilot.cpp:419) |
| 0x00477af0 | 20 | folded | covered by the port where it names it (src/mech3/pilot.cpp:421) |
| 0x004786e0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00479420 | 13 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00479460 | 15 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x00479480 | 15 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004794a0 | 15 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x004794c0 | 15 | folded | covered by the port where it names it (src/mech3/pilot.cpp:88) |
| 0x004795a0 | 9 | folded | a thunk: one call to another function |
| 0x004795c0 | 9 | folded | a thunk: one call to another function |
| 0x00479600 | 9 | folded | a thunk: one call to another function |
| 0x00479610 | 9 | folded | a thunk: one call to another function |
| 0x004796d0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00479b30 | 17 | folded | ported, by docs/symbols.csv: ported (vt+0x64) |
| 0x00479bd0 | 11 | folded | ported, by docs/symbols.csv: ported |
| 0x0047a010 | 84 | folded | ported, by docs/symbols.csv: ported (the elements' push_back) |
| 0x0047a250 | 13 | folded | ported, by docs/symbols.csv: ported |
| 0x0047a460 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047a480 | 28 | folded | ported, by docs/symbols.csv: ported |
| 0x0047a550 | 16 | folded | ported, by docs/symbols.csv: ported |
| 0x0047a580 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047a5a0 | 14 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0047a620 | 37 | folded | ported, by docs/symbols.csv: ported |
| 0x0047a720 | 14 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0047a780 | 18 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0047a910 | 14 | folded | ported, by docs/symbols.csv: ported: MCB_COMMAND 0x73 |
| 0x0047a920 | 8 | folded | covered by the port where it names it (src/mech3/zainet_region.h:45) |
| 0x0047a930 | 8 | folded | covered by the port where it names it (src/mech3/mech_waynet.h:34) |
| 0x0047a950 | 9 | folded | a thunk: one call to another function |
| 0x0047a960 | 18 | folded | ported, by docs/symbols.csv: ported (mech3 WinMovieState): the Win Movie State (0x00608be8, vtable 0x005c3958) |
| 0x0047aaf0 | 9 | folded | a thunk: one call to another function |
| 0x0047ab10 | 25 | folded | covered by the port where it names it (src/mech3/debriefing.h:14) |
| 0x0047ab70 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047ab90 | 21 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0047afe0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047b000 | 20 | folded | covered by ported code beside them: the force feedback's release, the game classes' small virtuals (first enemy, record slots, flags; mission-start.md), list and vector instances (mechdef-*.md name 0x0046cc00, 0x00475bf0, 0x00475c20), static list initialisers, the pilot's game-type getters (0x00479460/80, 0x004794a0, used by the port), part classes' constant virtuals and their damage path, constructors and destructors, the mcbwaynet's calls (app_vehicle.cpp) |
| 0x0047b050 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047c8d0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047c900 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047ce30 | 9 | folded | a thunk: one call to another function |
| 0x0047ce40 | 17 | folded | covered by the port where it names it (src/mech3/app_states.h:59) |
| 0x0047d010 | 9 | folded | a thunk: one call to another function |
| 0x0047d2c0 | 9 | folded | a thunk: one call to another function |
| 0x0047d2d0 | 9 | folded | a thunk: one call to another function |
| 0x0047daa0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047dac0 | 20 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047db10 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047db30 | 12 | folded | covered by the port where it names it (src/mech3/salvage_screen.h:39) |
| 0x0047db60 | 53 | folded | covered where the port does it: src/mech3/salvage_screen.h:33 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0047dcd0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047e9f0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047ea10 | 28 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047ea90 | 12 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047eac0 | 23 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047eb40 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047f140 | 29 | folded | covered by the port where it names it (src/mech3/app_hud.cpp:699) |
| 0x0047f1c0 | 13 | folded | covered by the port where it names it (src/mech3/salvage_screen.cpp:28) |
| 0x0047f1e0 | 23 | folded | covered by the port where it names it (src/mech3/salvage_screen.cpp:28) |
| 0x0047f220 | 12 | folded | covered by the port where it names it (src/mech3/app_states.h:237) |
| 0x0047f240 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047f260 | 20 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047f2b0 | 28 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047f350 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047f370 | 12 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047f380 | 35 | folded | covered by the port where it names it (src/mech3/mech_data.h:115) |
| 0x0047f440 | 9 | folded | follows 0x004822e0: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047f450 | 9 | folded | a thunk: one call to another function |
| 0x0047f480 | 9 | folded | follows 0x00482a60: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047f490 | 9 | folded | a thunk: one call to another function |
| 0x0047f4c0 | 9 | folded | follows 0x00486920: covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0047f4d0 | 9 | folded | a thunk: one call to another function |
| 0x0047f500 | 9 | folded | follows 0x00482ad0: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047f510 | 9 | folded | a thunk: one call to another function |
| 0x0047f540 | 9 | folded | follows 0x00482c10: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047f550 | 9 | folded | a thunk: one call to another function |
| 0x0047f580 | 9 | folded | follows 0x00480ba0: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047f590 | 9 | folded | a thunk: one call to another function |
| 0x0047f5c0 | 9 | folded | follows 0x00482b40: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047f5d0 | 9 | folded | a thunk: one call to another function |
| 0x0047f600 | 9 | folded | follows 0x004824a0: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047f610 | 9 | folded | a thunk: one call to another function |
| 0x0047f630 | 25 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047f6a0 | 9 | folded | follows 0x00482af0: covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x0047f6b0 | 9 | folded | a thunk: one call to another function |
| 0x0047f6e0 | 9 | folded | a thunk: one call to another function |
| 0x0047f6f0 | 9 | folded | a thunk: one call to another function |
| 0x0047f720 | 9 | folded | a thunk: one call to another function |
| 0x0047f730 | 9 | folded | a thunk: one call to another function |
| 0x0047f750 | 8 | folded | returns a constant or a global |
| 0x0047f770 | 9 | folded | a thunk: one call to another function |
| 0x0047f780 | 9 | folded | a thunk: one call to another function |
| 0x0047f9f0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0047fb70 | 8 | folded | returns a constant or a global |
| 0x0047fc10 | 9 | folded | a thunk: one call to another function |
| 0x0047fe00 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0047ff90 | 26 | folded | covered by the port where it names it (src/mech3/app.cpp:398) |
| 0x0047fff0 | 10 | folded | a getter of a member |
| 0x00480020 | 155 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00480340 | 25 | replaced (SDL or the platform layer) | Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x004803a0 | 19 | replaced (SDL or the platform layer) | Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00480b20 | 9 | folded | a thunk: one call to another function |
| 0x00480ba0 | 23 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x004810d0 | 34 | debug or dev tool: PORT (preserve) | the debug console's overlay toggle (0x00608f74; game-loop.md: from 0x00480b20, itself never called) |
| 0x004822e0 | 18 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x004824a0 | 23 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00482500 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00482870 | 17 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x004828d0 | 31 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00482a60 | 19 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00482aa0 | 15 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x00482ad0 | 12 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00482af0 | 18 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00482b40 | 23 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00482ba0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00482bc0 | 21 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00482c10 | 17 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00482c50 | 26 | replaced (SDL or the platform layer) | Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00482cb0 | 8 | folded | returns a constant or a global |
| 0x00482cc0 | 20 | replaced (SDL or the platform layer) | Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00482d00 | 9 | replaced (SDL or the platform layer) | follows 0x00482cc0: Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x004830a0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004830c0 | 12 | replaced (SDL or the platform layer) | Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x004830d0 | 15 | replaced (SDL or the platform layer) | Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x004830f0 | 8 | folded | returns a constant or a global |
| 0x00483130 | 9 | replaced (SDL or the platform layer) | Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00483170 | 44 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00483200 | 12 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00483230 | 14 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00483260 | 23 | dev dialog (MFC), its window plumbing: replaced | the debug menu's About box (resource 0x67) |
| 0x004832c0 | 9 | folded | a thunk: one call to another function |
| 0x004832e0 | 12 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00483330 | 9 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00483370 | 10 | replaced (SDL or the platform layer) | Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x004833f0 | 9 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00483400 | 12 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00483410 | 14 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00483430 | 17 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00483460 | 12 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x004834a0 | 9 | folded | a thunk: one call to another function |
| 0x004834b0 | 21 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00483500 | 9 | folded | a thunk: one call to another function |
| 0x00483570 | 9 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x004835c0 | 9 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00483600 | 10 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00483620 | 14 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00483640 | 32 | dev dialog (MFC), its logic: PORT (preserve) | the frame window's debug menu (editor.md: the debug menu; MFC message map entries 0x005c41..0x005c45): its commands and their check marks or enables (display driver, video modes, a mech picked from a list, the training game, input flags) |
| 0x00484130 | 11 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00484140 | 12 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00484160 | 9 | replaced (SDL or the platform layer) | Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00484170 | 9 | replaced (SDL or the platform layer) | Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x004841d0 | 9 | replaced (SDL or the platform layer) | Win32 and MFC frame plumbing the SDL program replaces: the single-instance check and close (FindWindow Mech3Class; startup.md), the frame window's create, OnCreate, OnSetFocus, OnNcActivate, OnSize, WindowProc, WM_CLOSE |
| 0x00484200 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00484220 | 21 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x00484540 | 42 | folded | covered where the port does it: src/mech3/mech_lib.cpp:89 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x00485370 | 9 | folded | a thunk: one call to another function |
| 0x00485390 | 10 | folded | the equipment callout: the voice for an equipment kind (0x00485360, mech_names.cpp equip_voice_index, into 0x00608fc0) played as 0x00485370 plays one; wired as DamageCalls.equip_voice (app_weapons.cpp), test_equip_voice.py |
| 0x004853d0 | 10 | folded | covered by the port where it names it (src/mech3/app_weapons.cpp:185) |
| 0x00485430 | 12 | folded | covered by the port where it names it (src/mech3/mech_names.cpp:367) |
| 0x00485460 | 12 | folded | covered by the port where it names it (src/mech3/mech_names.cpp:367) |
| 0x00485490 | 12 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:576) |
| 0x004854c0 | 10 | folded | covered by the port where it names it (src/mech3/app.cpp:167) |
| 0x00485a60 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00485a80 | 56 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00485ba0 | 56 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00485cc0 | 56 | folded | covered: the screens' destructors and constructors (unwind callers), the app states' constructors (their names), InitInstance's body (0x00480020, app.cpp's 0x00480000 port), RunDir (the program's data folder replaces it), the world defaults (no effect, app_engine.cpp) |
| 0x00485de0 | 56 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00485f00 | 56 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00486020 | 56 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00486140 | 56 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00486840 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00486860 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00486880 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004868a0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004868c0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004868e0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00486900 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00486920 | 13 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00486950 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00486970 | 12 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00486980 | 11 | folded | sets members (a setter or a small constructor) |
| 0x00487430 | 9 | folded | a thunk: one call to another function |
| 0x00487560 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00487580 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004875a0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004875c0 | 54 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x004876d0 | 54 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x004877e0 | 22 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00487840 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00487870 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00487890 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00489620 | 14 | folded | covered by the port where it names it (src/mech3/action_menu.h:59) |
| 0x00489650 | 9 | folded | a thunk: one call to another function |
| 0x00489880 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004898a0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004898c0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004898e0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00489900 | 20 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00489950 | 54 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x00489a60 | 54 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x00489b70 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00489b90 | 28 | folded | covered by the port where it names it (src/mech3/action_menu.h:78) |
| 0x00489c93 | 101 | folded | a piece of the ported function before it (Ghidra split it there) (0x00489c10) |
| 0x00489e40 | 15 | folded | covered by the port where it names it (src/mech3/action_menu.cpp:137) |
| 0x00489e70 | 20 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00489ec0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00489ee0 | 89 | folded | covered where the port does it: src/mech3/action_menu.h:100 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0048aef0 | 13 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:518) |
| 0x0048b000 | 66 | folded | covered where the port does it: src/mech3/pilot_menu.cpp:655 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0048b120 | 12 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:520) |
| 0x0048b170 | 24 | folded | covered by the port where it names it (src/mech3/pilot_menu.cpp:655) |
| 0x0048b360 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0048b670 | 17 | folded | covered by the port where it names it (src/mech3/menu_widgets.cpp:475) |
| 0x0048b940 | 8 | folded | empty: does nothing |
| 0x0048c050 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0048c230 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0048d0c0 | 11 | folded | a thunk: one virtual call |
| 0x0048d930 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0048d960 | 22 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:315) |
| 0x0048def0 | 12 | folded | sets members (a setter or a small constructor) |
| 0x0048df50 | 9 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:402) |
| 0x0048df60 | 16 | folded | covered by the port where it names it (src/mech3/menu_widgets.cpp:796) |
| 0x0048e180 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0048e1a0 | 56 | folded | covered where the port does it: src/mech3/quit_box.h:19 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0048e360 | 9 | folded | a thunk: one call to another function |
| 0x0048e370 | 9 | folded | a thunk: one call to another function |
| 0x0048e420 | 13 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0048e440 | 9 | folded | a thunk: one call to another function |
| 0x0048e450 | 9 | folded | a thunk: one call to another function |
| 0x0048e460 | 29 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x0048e500 | 10 | folded | covered by the port where it names it (src/mech3/menu_widgets.cpp:787) |
| 0x0048e570 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0048e590 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0048e5b0 | 29 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0048e630 | 29 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:279) |
| 0x0048e840 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0048e860 | 56 | folded | covered where the port does it: src/mech3/mainmenu.h:20 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0048e9c0 | 15 | folded | covered by the port where it names it (src/mech3/mainmenu.cpp:47) |
| 0x0048ea00 | 15 | folded | covered by the port where it names it (src/mech3/mainmenu.cpp:47) |
| 0x0048ea40 | 15 | folded | covered by the port where it names it (src/mech3/mainmenu.cpp:47) |
| 0x0048ea80 | 15 | folded | covered by the port where it names it (src/mech3/mainmenu.cpp:47) |
| 0x0048eac0 | 15 | folded | covered by the port where it names it (src/mech3/mainmenu.cpp:24) |
| 0x0048eb10 | 22 | folded | covered by the port where it names it (src/mech3/app_states.h:12) |
| 0x0048eb60 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0048ee30 | 9 | folded | a thunk: one call to another function |
| 0x0048ee40 | 9 | folded | a thunk: one call to another function |
| 0x0048ee70 | 12 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0048f1b0 | 28 | folded | covered by the port where it names it (src/mech3/app_states.h:302) |
| 0x0048f220 | 9 | folded | a thunk: one call to another function |
| 0x0048f230 | 9 | folded | a thunk: one call to another function |
| 0x0048f260 | 9 | folded | follows 0x0048f290: covered where the port does it: src/mech3/mech_salvage.h:10 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0048f270 | 9 | folded | a thunk: one call to another function |
| 0x0048f290 | 57 | folded | covered where the port does it: src/mech3/mech_salvage.h:10 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0048f370 | 35 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0048fab0 | 13 | folded | covered by the port where it names it (src/mech3/hud.cpp:326) |
| 0x0048fad0 | 15 | folded | covered by the port where it names it (src/mech3/hud.cpp:327) |
| 0x00490720 | 64 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00490930 | 15 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00491120 | 14 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00491a60 | 9 | folded | a thunk: one call to another function |
| 0x00491a70 | 17 | folded | covered by the port where it names it (src/mech3/app_states.h:72) |
| 0x00491bd0 | 9 | folded | a thunk: one call to another function |
| 0x00491d00 | 14 | folded | ported, by docs/symbols.csv: ported in the program (the missions' +0xb0, reselected) |
| 0x00492ac0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00493120 | 27 | unused: PORT (preserve) | the movie base's run to the end on its own clock (0x00575d30): nothing calls it (fmv.md) |
| 0x004931c0 | 12 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x004938d0 | 13 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00493940 | 12 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00493970 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00493990 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004939b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004939d0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004939f0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00493a10 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00493a30 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00493a50 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00493da0 | 54 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00493eb0 | 54 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00493fc0 | 54 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x004940d0 | 54 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x004941e0 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00494260 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x004942e0 | 21 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00494330 | 40 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00494400 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00494447 | 25 | folded | a piece of the ported function before it (Ghidra split it there) (0x00494420) |
| 0x00495200 | 20 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00495240 | 47 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00495b00 | 11 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00495c60 | 39 | folded | covered by the port where it names it (src/mech3/pilot.h:378) |
| 0x00495d10 | 11 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x00495d70 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00495df0 | 28 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00495e60 | 33 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00495ee0 | 39 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x004967e0 | 35 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x004968d0 | 16 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00496910 | 16 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00496950 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00496970 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00496990 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004969b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004969d0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004969f0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00496a10 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00496a30 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00496a50 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00496a70 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00496af0 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00496b70 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00496e20 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00496ea0 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00496f20 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00496fa0 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00497020 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x004970a0 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00497120 | 73 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x004972d0 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00497350 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x004973d0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004973f0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00498160 | 74 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00498240 | 10 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00499010 | 14 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00499030 | 121 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00499270 | 8 | folded | returns a constant or a global |
| 0x00499280 | 78 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x004993e0 | 8 | folded | returns a constant or a global |
| 0x004999f0 | 9 | folded | a thunk: one call to another function |
| 0x00499a00 | 19 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00499a30 | 22 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x00499c86 | 9 | folded | a piece of the ported function before it (Ghidra split it there) (0x00499b10) |
| 0x0049a7a0 | 43 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049b1c0 | 48 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049b2d0 | 37 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049bfd0 | 8 | folded | empty: does nothing |
| 0x0049bfe0 | 9 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049c000 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0049c020 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0049c040 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049c0c0 | 29 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049c150 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0049c900 | 55 | folded | the multiplayer screens (mp_lobby, mp_connect; menus.md): controls' destructors and constructors, list helpers; the named ones ported in those files |
| 0x0049c9e0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0049ca80 | 15 | folded | covered by the port where it names it (src/mech3/mp_lobby.cpp:1067) |
| 0x0049cab0 | 18 | folded | covered by the port where it names it (src/mech3/pilot_menu.cpp:579) |
| 0x0049cb90 | 14 | folded | covered by the port where it names it (src/mech3/pilot_menu.cpp:626) |
| 0x0049cbb0 | 40 | folded | covered by the port where it names it (src/mech3/pilot_menu.cpp:655) |
| 0x0049cc80 | 17 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0049ccb0 | 11 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0049ccc0 | 93 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0049ce90 | 17 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0049ced0 | 137 | folded | covered where the port does it: src/mech3/mp_lobby.h:38 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x0049d100 | 16 | dev dialog (MFC), its window plumbing: replaced | MFC plumbing of the editor's dialogs (DDX, UpdateData, OnDestroy, DoModal, a static list) |
| 0x0049d140 | 9 | dev dialog (MFC), its logic: PORT (preserve) | follows 0x00404090: the AI tester dialog's walk over a vehicle's nets (0x00404090 -> 0x00403ec0: their extents) |
| 0x0049d150 | 17 | dev dialog (MFC), its window plumbing: replaced | MFC plumbing of the editor's dialogs (DDX, UpdateData, OnDestroy, DoModal, a static list) |
| 0x0049d180 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0049d190 | 16 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0049d1c0 | 16 | folded | covered by the port where it names it (src/mech3/sound_extra.cpp:32) |
| 0x0049d1f0 | 13 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0049d210 | 19 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0049d250 | 19 | folded | covered by the port where it names it (src/mech3/game_editor_vehicles.cpp:275) |
| 0x0049d290 | 39 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0049d350 | 148 | dev dialog (MFC), its logic: PORT (preserve) | the editor's 'add vehicle to node' (0x00403820 -> 0x0049d350, editor.md); ours does it as meant: upgrades/editor/add_vehicle.cpp |
| 0x0049d5d0 | 18 | dev dialog (MFC), its window plumbing: replaced | MFC plumbing of the editor's dialogs (DDX, UpdateData, OnDestroy, DoModal, a static list) |
| 0x0049d5f0 | 9 | folded | a thunk: one call to another function |
| 0x0049d640 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0049d660 | 18 | dev dialog (MFC), its window plumbing: replaced | MFC plumbing of the editor's dialogs (DDX, UpdateData, OnDestroy, DoModal, a static list) |
| 0x0049d700 | 8 | folded | returns a constant or a global |
| 0x0049d710 | 27 | dev dialog (MFC), its logic: PORT (preserve) | the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians |
| 0x0049d7b0 | 18 | dev dialog (MFC), its logic: PORT (preserve) | the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians |
| 0x0049d810 | 27 | dev dialog (MFC), its logic: PORT (preserve) | the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians |
| 0x0049d8b0 | 12 | dev dialog (MFC), its window plumbing: replaced | MFC plumbing of the editor's dialogs (DDX, UpdateData, OnDestroy, DoModal, a static list) |
| 0x0049d8d0 | 25 | dev dialog (MFC), its logic: PORT (preserve) | the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians |
| 0x0049d920 | 25 | dev dialog (MFC), its logic: PORT (preserve) | the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians |
| 0x0049d970 | 25 | dev dialog (MFC), its logic: PORT (preserve) | the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians |
| 0x0049d9e0 | 13 | dev dialog (MFC), its logic: PORT (preserve) | the editor's 'add vehicle to node' dialog (editor.md: a vehicle picked from a dialog, 0x0049d350 / 0x0049d8b0): its list fill, its team and target bits from the player's mech (+0xc4, +0xc8), the yaw in radians |
| 0x0049da10 | 137 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0049dc40 | 15 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x0049e4c0 | 11 | folded | covered by the port where it names it (src/mech3/mech_control.cpp:577) |
| 0x0049ec90 | 11 | folded | a thunk: one call to another function |
| 0x0049fe30 | 14 | folded | covered by the port where it names it (src/mech3/player.cpp:321) |
| 0x0049fe70 | 10 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x004a0100 | 31 | unlabelled |  |
| 0x004a0b00 | 9 | folded | a thunk: one call to another function |
| 0x004a0b20 | 9 | folded | a thunk: one call to another function |
| 0x004a1020 | 9 | folded | a thunk: one call to another function |
| 0x004a2770 | 38 | folded | covered by the port where it names it (src/mech3/veh_move.cpp:156) |
| 0x004a2ae0 | 14 | folded | covered by the port where it names it (src/mech3/veh_controls.h:178) |
| 0x004a3220 | 17 | debug or dev tool: PORT (preserve) | debug key commands of 0x0040dba0, never registered in the release: a position marker printed to the console (0x004a3220), the mechs listed into the developer lines (0x004a33f0, command 0x89; hud.md), a vehicle picked in a dialog (0x004a3550, its destructor 0x004a35e0) |
| 0x004a33f0 | 102 | debug or dev tool: PORT (preserve) | debug key commands of 0x0040dba0, never registered in the release: a position marker printed to the console (0x004a3220), the mechs listed into the developer lines (0x004a33f0, command 0x89; hud.md), a vehicle picked in a dialog (0x004a3550, its destructor 0x004a35e0) |
| 0x004a3550 | 32 | debug or dev tool: PORT (preserve) | debug key commands of 0x0040dba0, never registered in the release: a position marker printed to the console (0x004a3220), the mechs listed into the developer lines (0x004a33f0, command 0x89; hud.md), a vehicle picked in a dialog (0x004a3550, its destructor 0x004a35e0) |
| 0x004a35e0 | 21 | debug or dev tool: PORT (preserve) | debug key commands of 0x0040dba0, never registered in the release: a position marker printed to the console (0x004a3220), the mechs listed into the developer lines (0x004a33f0, command 0x89; hud.md), a vehicle picked in a dialog (0x004a3550, its destructor 0x004a35e0) |
| 0x004a3fb0 | 12 | unused: PORT (preserve) | the turn limits' clamp (vt+0x04) for the Fixed, Narrow and Wide kinds (vtables 0x005c8310/20/30): the turret never asks clamp (turret.h:31) and the AI's track keeps its answer only for a dot product it drops (vehicles.md, zai_firing.cpp:554) |
| 0x004a3fe0 | 12 | unused: PORT (preserve) | the turn limits' clamp (vt+0x04) for the Fixed, Narrow and Wide kinds (vtables 0x005c8310/20/30): the turret never asks clamp (turret.h:31) and the AI's track keeps its answer only for a dot product it drops (vehicles.md, zai_firing.cpp:554) |
| 0x004a4000 | 13 | folded | covered by the port where it names it (src/mech3/turret.cpp:95) |
| 0x004a4030 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004a4050 | 11 | folded | covered: the screens' and states' constructors and destructors, list and vector instances (mechdef-configs.md names 0x0049da10), string copies, the missions object's queries (the program's GameEngine overrides them: app_engine.h:128 and the rest), the fade wrapper, the mission cleanup |
| 0x004a4060 | 27 | folded | covered by the port where it names it (src/mech3/veh_basic_object.cpp:684) |
| 0x004a40f0 | 27 | folded | covered by the port where it names it (src/mech3/veh_basic_object.cpp:684) |
| 0x004a4270 | 23 | folded | ported, by docs/symbols.csv: ported (data): the two edges, under 0x4a4580 (0x5c8320) and 0x4a4800 (0x5c8330) |
| 0x004a42c0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004a42f0 | 18 | folded | ported, by docs/symbols.csv: ported (data): one heading (vtable 0x5c8310) |
| 0x004a4330 | 38 | folded | ported, by docs/symbols.csv: ported: the pitch, and along the one heading (1e-5) |
| 0x004a43c0 | 63 | unused: PORT (preserve) | the turn limits' clamp (vt+0x04) for the Fixed, Narrow and Wide kinds (vtables 0x005c8310/20/30): the turret never asks clamp (turret.h:31) and the AI's track keeps its answer only for a dot product it drops (vehicles.md, zai_firing.cpp:554) |
| 0x004a4580 | 12 | folded | covered by the port where it names it (src/mech3/turret.cpp:117) |
| 0x004a45b0 | 41 | folded | ported, by docs/symbols.csv: ported: the pitch, and inside both edges |
| 0x004a4660 | 66 | unused: PORT (preserve) | the turn limits' clamp (vt+0x04) for the Fixed, Narrow and Wide kinds (vtables 0x005c8310/20/30): the turret never asks clamp (turret.h:31) and the AI's track keeps its answer only for a dot product it drops (vehicles.md, zai_firing.cpp:554) |
| 0x004a4800 | 12 | folded | covered by the port where it names it (src/mech3/turret.cpp:117) |
| 0x004a4830 | 47 | folded | ported, by docs/symbols.csv: ported: the pitch, and inside either edge |
| 0x004a48e0 | 63 | unused: PORT (preserve) | the turn limits' clamp (vt+0x04) for the Fixed, Narrow and Wide kinds (vtables 0x005c8310/20/30): the turret never asks clamp (turret.h:31) and the AI's track keeps its answer only for a dot product it drops (vehicles.md, zai_firing.cpp:554) |
| 0x004a4a90 | 9 | folded | a thunk: one call to another function |
| 0x004a4aa0 | 9 | folded | a thunk: one call to another function |
| 0x004a4ac0 | 9 | folded | ported, by docs/symbols.csv: ported: 0x4a4b30(-a, a, -b, b) |
| 0x004a4af0 | 18 | folded | ported, by docs/symbols.csv: ported: a full turn on -pi/2..pi/2 in place |
| 0x004a4c20 | 23 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:229) |
| 0x004a4c80 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004a4ca0 | 9 | debug or dev tool: PORT (preserve) | the stealth object (the developers' fly camera; back burner in CLAUDE.md): a slot of its |
| 0x004a4cc0 | 8 | folded | returns a constant or a global |
| 0x004a4cd0 | 29 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:230) |
| 0x004a4d90 | 21 | folded | covered by the port where it names it (src/mech3/game_editor_net.h:27) |
| 0x004a4fa0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004a50b0 | 9 | debug or dev tool: PORT (preserve) | the stealth object (the developers' fly camera; back burner in CLAUDE.md): a slot of its |
| 0x004a50e0 | 28 | debug or dev tool: PORT (preserve) | the debug command 0x7e (0x004abbc0, 121 lines: a console of keyboard text and the network session; registered, but no key reaches it: app_keys.h:14, hud.md) and the stealth object's key handler (0x004a50e0: the fly camera, back burner) |
| 0x004a5170 | 20 | folded | covered by the port where it names it (src/mech3/app_vehicle.cpp:474) |
| 0x004a51b0 | 30 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004a5260 | 8 | folded | returns a constant or a global |
| 0x004a5270 | 8 | folded | returns a constant or a global |
| 0x004a5640 | 23 | folded | covered by the port where it names it (src/mech3/veh_stealth.cpp:190) |
| 0x004a5850 | 27 | folded | covered by the port where it names it (src/mech3/veh_stealth.cpp:99) |
| 0x004a5a50 | 21 | folded | covered by the port where it names it (src/mech3/app_vehicle.cpp:403) |
| 0x004a5b20 | 13 | folded | covered by the port where it names it (src/mech3/veh_stealth.h:22) |
| 0x004a5b50 | 10 | folded | returns a constant or a global |
| 0x004a5b70 | 11 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.cpp:262) |
| 0x004a5b90 | 10 | folded | returns a constant or a global |
| 0x004a5bb0 | 11 | folded | covered by the port where it names it (src/mech3/veh_controls.cpp:461) |
| 0x004a5bc0 | 8 | folded | empty: does nothing |
| 0x004a5be0 | 8 | folded | returns a constant or a global |
| 0x004a5c10 | 8 | folded | returns a constant or a global |
| 0x004a5c30 | 8 | folded | returns a constant or a global |
| 0x004a5c50 | 10 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004a5fc0 | 8 | folded | ported, by docs/symbols.csv: ported: vt+0x3c; [0x609a38 + 4 side] |
| 0x004a5fd0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004a79e0 | 9 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004a7a00 | 9 | folded | a thunk: one call to another function |
| 0x004a7a20 | 9 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004a7a40 | 9 | folded | a thunk: one call to another function |
| 0x004a7a60 | 8 | folded | ported, by docs/symbols.csv: ported: vt+0xa0 |
| 0x004a7ba0 | 15 | folded | covered by the port where it names it (src/mech3/mech_command.cpp:164) |
| 0x004a7c70 | 13 | folded | covered by the port where it names it (src/mech3/sensors.cpp:142) |
| 0x004a7c90 | 19 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004a7cd0 | 20 | folded | covered by the port where it names it (src/mech3/sensors.cpp:247) |
| 0x004a7d00 | 92 | folded | covered where the port does it: src/mech3/sensors.cpp:142 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004a7f30 | 36 | folded | covered by the port where it names it (src/mech3/sensors.cpp:245) |
| 0x004a81f0 | 85 | folded | covered where the port does it: src/mech3/sensors.cpp:226 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004a83c0 | 71 | folded | covered where the port does it: src/mech3/sensors.cpp:457 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004a86e0 | 14 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004a8710 | 112 | folded | covered where the port does it: src/mech3/sensors.cpp:179 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004a8960 | 19 | folded | covered by the port where it names it (src/mech3/sensors.cpp:485) |
| 0x004a8c30 | 29 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004a8c80 | 9 | folded | a thunk: one call to another function |
| 0x004a91b0 | 66 | folded | covered where the port does it: src/mech3/sensors.cpp:179 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004a98c0 | 16 | folded | covered by the port where it names it (src/mech3/sensors.cpp:277) |
| 0x004a9910 | 37 | folded | covered by the port where it names it (src/mech3/sensors.cpp:281) |
| 0x004aa470 | 17 | folded | ported, by docs/symbols.csv: ported: a null contact, or one whose vehicle is the one going |
| 0x004aa4a0 | 20 | folded | ported, by docs/symbols.csv: ported: one entity's vehicle (vt+0x2c) told (vt+0xd4) |
| 0x004aa700 | 17 | folded | covered by the port where it names it (src/mech3/lab_screen.cpp:2428) |
| 0x004aa740 | 20 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aa780 | 137 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aa9a0 | 11 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aa9d0 | 14 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aaa10 | 138 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aad20 | 126 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aaf20 | 125 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004ab0f0 | 49 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004ab1b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004ab1d0 | 13 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004ab1f0 | 129 | folded | covered where the port does it: src/crt/msvc_sort.h:8 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004ab470 | 15 | folded | covered by the port where it names it (src/mech3/sensors.cpp:401) |
| 0x004ab4a0 | 13 | folded | covered by the port where it names it (src/mech3/sensors.cpp:403) |
| 0x004ab4c0 | 24 | folded | covered by the port where it names it (src/mech3/sensors.cpp:403) |
| 0x004ab500 | 63 | folded | covered where the port does it: src/crt/msvc_sort.h:7 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004ab620 | 38 | folded | covered by the port where it names it (src/crt/msvc_sort.h:7) |
| 0x004abaf0 | 25 | folded | covered by the port where it names it (src/mech3/mech_build.cpp:318) |
| 0x004abbc0 | 121 | debug or dev tool: PORT (preserve) | the debug command 0x7e (0x004abbc0, 121 lines: a console of keyboard text and the network session; registered, but no key reaches it: app_keys.h:14, hud.md) and the stealth object's key handler (0x004a50e0: the fly camera, back burner) |
| 0x004abfe0 | 22 | replaced (SDL or the platform layer) | Win32: the exe's version info for the window title and menus (GetFileVersionInfoA; protection.md) and the OK/Cancel MessageBoxExA helper (the game's own box replaces it, W16) |
| 0x004ac050 | 36 | folded | covered by the port where it names it (src/mech3/app_menus.cpp:1428) |
| 0x004ac2c0 | 13 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004ac2f0 | 9 | folded | a thunk: one call to another function |
| 0x004ac740 | 9 | folded | a thunk: one call to another function |
| 0x004ac860 | 23 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ac890 | 11 | folded | frees the object (a destructor's release) |
| 0x004ad720 | 11 | folded | covered by the port where it names it (src/mech3/veh_controls.h:224) |
| 0x004ad840 | 54 | folded | covered where the port does it: src/mech3/veh_basic_object.h:38 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004adaa0 | 9 | folded | follows 0x004ad840: covered where the port does it: src/mech3/veh_basic_object.h:38 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004adab0 | 9 | folded | a thunk: one call to another function |
| 0x004adad0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004adaf0 | 12 | folded | sets members (a setter or a small constructor) |
| 0x004adb10 | 11 | folded | sets members (a setter or a small constructor) |
| 0x004adb40 | 11 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.cpp:137) |
| 0x004adb90 | 14 | folded | covered by the port where it names it (src/mech3/veh_basic_object.cpp:143) |
| 0x004adbb0 | 19 | folded | covered by the port where it names it (src/mech3/veh_basic_object.cpp:143) |
| 0x004adbf0 | 17 | folded | covered by the port where it names it (src/mech3/veh_basic_object.cpp:154) |
| 0x004add90 | 10 | folded | covered by the port where it names it (src/mech3/app_sensors.cpp:142) |
| 0x004ade70 | 14 | folded | covered by the port where it names it (src/mech3/mech_fieldbase.cpp:257) |
| 0x004ade90 | 20 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aded0 | 156 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004ae340 | 8 | folded | covered by the port where it names it (src/mech3/veh_basic_controls.h:84) |
| 0x004ae360 | 12 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x004ae380 | 16 | folded | covered by the port where it names it (src/mech3/veh_basic_controls.h:80) |
| 0x004ae6a0 | 8 | folded | covered by the port where it names it (src/mech3/veh_basic_controls.h:89) |
| 0x004ae8b0 | 9 | folded | a thunk: one call to another function |
| 0x004ae8c0 | 9 | folded | a thunk: one call to another function |
| 0x004aeb30 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004aec00 | 13 | folded | ported, by docs/symbols.csv: ported: IControls +0x04 zeroed |
| 0x004aec70 | 10 | folded | ported, by docs/symbols.csv: ported: HitRecord::run |
| 0x004aec80 | 9 | folded | ported, by docs/symbols.csv: ported: HitRecord::within(1) (the manoeuvre's direct call) |
| 0x004aed10 | 21 | folded | covered by the port where it names it (src/mech3/zai_brain.cpp:1617) |
| 0x004aed50 | 13 | folded | ported, by docs/symbols.csv: ported: MechControls::on_death (vt+0xa8: the tick once more) |
| 0x004aed60 | 10 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aed70 | 11 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aed80 | 10 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aed90 | 11 | folded | covered by the port where it names it (src/mech3/zai_brain.h:756) |
| 0x004aeda0 | 10 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aedb0 | 10 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aedc0 | 10 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aedd0 | 24 | folded | covered by the port where it names it (src/mech3/app_sensors.cpp:127) |
| 0x004aee00 | 29 | folded | covered by the port where it names it (src/mech3/app_sensors.cpp:124) |
| 0x004aee40 | 25 | folded | ported in force_feedback.cpp:172 (water: wading) and app_sensors.cpp:141 (a vehicle sees its target), whose hook fields are allowed null only in tests |
| 0x004aee70 | 27 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004aef00 | 23 | dev dialog (MFC), its window plumbing: replaced | the vehicle-pick dialog's MFC plumbing |
| 0x004aef60 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004aef80 | 9 | dev dialog (MFC), its window plumbing: replaced | the vehicle-pick dialog's MFC plumbing |
| 0x004aefa0 | 8 | folded | returns a constant or a global |
| 0x004aefb0 | 19 | dev dialog (MFC), its window plumbing: replaced | the vehicle-pick dialog's MFC plumbing |
| 0x004af000 | 40 | dev dialog (MFC), its logic: PORT (preserve) | a vehicle-pick dev dialog (resource 0x8f, from the debug key's 0x004a3550): its list of zEntity's vehicles |
| 0x004af0a0 | 22 | replaced (SDL or the platform layer) | Win32: the exe's version info for the window title and menus (GetFileVersionInfoA; protection.md) and the OK/Cancel MessageBoxExA helper (the game's own box replaces it, W16) |
| 0x004af0f0 | 51 | replaced (SDL or the platform layer) | Win32: the exe's version info for the window title and menus (GetFileVersionInfoA; protection.md) and the OK/Cancel MessageBoxExA helper (the game's own box replaces it, W16) |
| 0x004af1a0 | 20 | replaced (SDL or the platform layer) | Win32: the exe's version info for the window title and menus (GetFileVersionInfoA; protection.md) and the OK/Cancel MessageBoxExA helper (the game's own box replaces it, W16) |
| 0x004af1d0 | 17 | replaced (SDL or the platform layer) | Win32: the exe's version info for the window title and menus (GetFileVersionInfoA; protection.md) and the OK/Cancel MessageBoxExA helper (the game's own box replaces it, W16) |
| 0x004af230 | 8 | folded | returns a constant or a global |
| 0x004af250 | 9 | replaced (SDL or the platform layer) | follows 0x004af0a0: Win32: the exe's version info for the window title and menus (GetFileVersionInfoA; protection.md) and the OK/Cancel MessageBoxExA helper (the game's own box replaces it, W16) |
| 0x004af260 | 9 | folded | a thunk: one call to another function |
| 0x004af280 | 9 | folded | a thunk: one call to another function |
| 0x004af290 | 9 | replaced (SDL or the platform layer) | follows 0x004af1d0: Win32: the exe's version info for the window title and menus (GetFileVersionInfoA; protection.md) and the OK/Cancel MessageBoxExA helper (the game's own box replaces it, W16) |
| 0x004afe10 | 19 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b004f | 29 | folded | a piece of the ported function before it (Ghidra split it there) (0x004b0020) |
| 0x004b0091 | 18 | folded | a piece of the ported function before it (Ghidra split it there) (0x004b0020) |
| 0x004b0c20 | 12 | folded | sets members (a setter or a small constructor) |
| 0x004b1170 | 88 | unused: PORT (preserve) | the static the AI's planner builds at 0x0060b240 (two reserved vectors): read by nothing (weapon_planner.h:7) |
| 0x004b12e0 | 9 | unused: PORT (preserve) | follows 0x004b1170: the static the AI's planner builds at 0x0060b240 (two reserved vectors): read by nothing (weapon_planner.h:7) |
| 0x004b12f0 | 9 | folded | a thunk: one call to another function |
| 0x004b1350 | 15 | folded | ported, by docs/symbols.csv: ported inline; Shot's initialisers |
| 0x004b1440 | 13 | folded | ported, by docs/symbols.csv: ported; group_out |
| 0x004b17d0 | 42 | folded | ported, by docs/symbols.csv: ported inline; the groups left (remove_if) |
| 0x004b1ba0 | 14 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b1d80 | 15 | folded | covered by the port where it names it (src/mech3/weapon.h:124) |
| 0x004b2830 | 14 | folded | covered by the port where it names it (src/mech3/weapon.h:181) |
| 0x004b2870 | 25 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b28b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004b2b60 | 40 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b3010 | 10 | folded | a getter of a member |
| 0x004b3d40 | 8 | folded | covered by the port where it names it (src/mech3/weapon.h:218) |
| 0x004b3d50 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004b4970 | 9 | folded | a thunk: one call to another function |
| 0x004b4980 | 9 | folded | a thunk: one call to another function |
| 0x004b4c20 | 9 | folded | follows 0x004b4c50: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b4c30 | 9 | folded | a thunk: one call to another function |
| 0x004b4c50 | 12 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b4cb0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x004b4cf0 | 13 | folded | covered by the port where it names it (src/mech3/mech_salvage.h:37) |
| 0x004b4d00 | 13 | folded | covered by the port where it names it (src/mech3/weapon.h:119) |
| 0x004b4dc0 | 11 | folded | a thunk: one call to another function |
| 0x004b4ed0 | 15 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b4f00 | 55 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b4fe0 | 12 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b5000 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004b5020 | 16 | folded | ported, by docs/symbols.csv: ported inline (lab_world_load): a world node named, vtable 0x005c9110 (vt+0 0x0040bab0 adds a child) |
| 0x004b5050 | 11 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b5060 | 47 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b51b0 | 12 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b51d0 | 12 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b5230 | 9 | folded | follows 0x004b5260: covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b5240 | 9 | folded | a thunk: one call to another function |
| 0x004b5260 | 13 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b5290 | 16 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b52c0 | 13 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b52e0 | 56 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b5560 | 10 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b5580 | 24 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b55f0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004b5610 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004b5630 | 21 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b5680 | 9 | folded | the HUD (hud/app_hud, hud.md names these): pieces' constructors and destructors (CRT callers: unwinds), setters, the static and gauge helpers, the reticle's queries, the hold, funclets |
| 0x004b56a0 | 8 | folded | returns a constant or a global |
| 0x004b5860 | 22 | folded | ported, by docs/symbols.csv: ported; line_cone [0x0060b2c8] (10, 30, 10, 200) |
| 0x004b5920 | 9 | folded | ported, by docs/symbols.csv: ported; the start-up's call |
| 0x004b5a20 | 28 | folded | ported, by docs/symbols.csv: ported; volley_kind (0x004b1860 through the volley_pick hook) |
| 0x004b5a80 | 18 | folded | ported, by docs/symbols.csv: ported inline; the vehicle takes the target (vt+0xc0) |
| 0x004b5ac0 | 9 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b5ad0 | 9 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b5ae0 | 9 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b5b00 | 11 | folded | ported, by docs/symbols.csv: ported; [0x0060b2b4] = 0.25 |
| 0x004b5b30 | 14 | folded | ported, by docs/symbols.csv: ported; [0x0060b2b8] the rejected record |
| 0x004b6350 | 9 | folded | a thunk: one call to another function |
| 0x004b6370 | 12 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b6390 | 17 | folded | covered by the port where it names it (src/mech3/zai_brain.h:25) |
| 0x004b63d0 | 10 | folded | ported, by docs/symbols.csv: ported; LookAround::start |
| 0x004b6500 | 25 | folded | covered by the port where it names it (src/mech3/zai_firing.h:111) |
| 0x004b6660 | 12 | folded | covered by the port where it names it (src/mech3/zai_brain.cpp:1010) |
| 0x004b6aa0 | 14 | folded | covered by the port where it names it (src/mech3/zai_firing.cpp:303) |
| 0x004b6ac0 | 20 | folded | covered: the stealth's controls (app_vehicle.cpp:604), the turret's slots, the sensors' vector instances (ai.md 15), the vehicles' list instances, the controls' constant virtuals (controls.md), the weapon lists, the firing's thunks, start-up statics |
| 0x004b6b10 | 15 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004b6b30 | 121 | folded | ported, by docs/symbols.csv: ported; the candidates' scorer |
| 0x004b7030 | 17 | folded | ported, by docs/symbols.csv: ported as a constant: [0x0060b2ec] = 2/100 (and [0x0060b2b0] = cos(atan(2/100)), never read) |
| 0x004b7060 | 21 | folded | ported, by docs/symbols.csv: ported; Aim's initialisers |
| 0x004b78d0 | 12 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004b78e0 | 17 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004b7920 | 28 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004b7960 | 19 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004b7990 | 33 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004b79e0 | 48 | folded | ported, by docs/symbols.csv: ported inline; to one place: a swap pass |
| 0x004b7ac0 | 14 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004b7af0 | 24 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004b7b40 | 53 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004b7bf0 | 39 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004b7d80 | 54 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004b7f00 | 22 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004b7f80 | 9 | folded | follows 0x004b7d80: the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004b7f90 | 9 | folded | a thunk: one call to another function |
| 0x004b7fb0 | 52 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004b83c0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004b83e0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004b8400 | 21 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b8460 | 77 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b8760 | 8 | folded | returns a constant or a global |
| 0x004b8a20 | 46 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b8b70 | 16 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004b8ba0 | 12 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b8d10 | 8 | folded | returns a constant or a global |
| 0x004b8e10 | 26 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b8e90 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004b9250 | 8 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b9260 | 8 | folded | returns a constant or a global |
| 0x004b92d0 | 12 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b9460 | 8 | folded | returns a constant or a global |
| 0x004b9470 | 12 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b9670 | 8 | folded | returns a constant or a global |
| 0x004b9680 | 12 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b9860 | 8 | folded | returns a constant or a global |
| 0x004b9870 | 12 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b9900 | 8 | folded | returns a constant or a global |
| 0x004b9910 | 12 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b9c00 | 8 | folded | returns a constant or a global |
| 0x004b9c10 | 12 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b9c50 | 8 | folded | returns a constant or a global |
| 0x004b9c60 | 12 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b9cc0 | 8 | folded | returns a constant or a global |
| 0x004b9cd0 | 12 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b9d60 | 8 | folded | returns a constant or a global |
| 0x004b9d70 | 12 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b9dd0 | 8 | folded | returns a constant or a global |
| 0x004b9de0 | 12 | dev dialog (MFC), its window plumbing: replaced | the vehicle editor's MFC plumbing (its pages' CDialog constructors, its destructor, message map); its layout is the exe's dialog resources (0xa5..0xb7). The port's editor covers the job: game_editor_vehicles (vehicle_editor_open / the page handlers) |
| 0x004b9e00 | 8 | folded | returns a constant or a global |
| 0x004b9ff0 | 58 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ba150 | 84 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ba2d0 | 41 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ba380 | 46 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ba470 | 316 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ba9a0 | 48 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004baa70 | 31 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004bab00 | 196 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004bae30 | 52 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004baef0 | 48 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004bafa0 | 89 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004bb1a0 | 20 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004bb1e0 | 13 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bb200 | 44 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bb290 | 17 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bb2e0 | 11 | dev dialog (MFC), its logic: PORT (preserve) | the vehicle editor's list fills (each valve, each region by name into a combo box); covered in the port by game_editor_vehicles 0x004bb300 / 0x004bb620 (valve and region lists) |
| 0x004bb390 | 58 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bb5f0 | 13 | dev dialog (MFC), its logic: PORT (preserve) | the vehicle editor's list fills (each valve, each region by name into a combo box); covered in the port by game_editor_vehicles 0x004bb300 / 0x004bb620 (valve and region lists) |
| 0x004bbd70 | 16 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bbdd0 | 21 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bbe60 | 18 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bbed0 | 29 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bc010 | 17 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bc090 | 14 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bc0d0 | 14 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bc110 | 15 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bc3e0 | 9 | folded | a thunk: one call to another function |
| 0x004bc420 | 25 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004bc960 | 32 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004bc9f0 | 23 | dev dialog (MFC), its logic: PORT (preserve) | the AI tester dialog (0x004b7d80..0x004bcb70, docs/ai.md: a developer tool over a vehicle's AI and net): its helpers' logic |
| 0x004bcb30 | 11 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004bcb50 | 9 | dev dialog (MFC), its logic: PORT (preserve) | follows 0x004bc9f0: the AI tester dialog (0x004b7d80..0x004bcb70, docs/ai.md: a developer tool over a vehicle's AI and net): its helpers' logic |
| 0x004bcb70 | 22 | dev dialog (MFC), its logic: PORT (preserve) | the AI tester dialog (0x004b7d80..0x004bcb70, docs/ai.md: a developer tool over a vehicle's AI and net): its helpers' logic |
| 0x004bcf80 | 14 | folded | covered by the port where it names it (src/mech3/zainetedit.h:60) |
| 0x004bcf90 | 14 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004bcfa0 | 14 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004bcfb0 | 14 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004bcfc0 | 14 | folded | covered by the port where it names it (src/mech3/zainetedit.h:61) |
| 0x004bcfd0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004bd000 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004bd330 | 9 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:558) |
| 0x004bd340 | 10 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:295) |
| 0x004bd360 | 28 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:249) |
| 0x004bd3d0 | 28 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:538) |
| 0x004bd440 | 50 | folded | ported, by docs/symbols.csv: ported: zainetedit (vtable 0x005ca480) |
| 0x004bd640 | 18 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:429) |
| 0x004bd740 | 25 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:425) |
| 0x004bdd70 | 13 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:367) |
| 0x004be230 | 63 | folded | covered where the port does it: src/mech3/zainetedit.cpp:254 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004be300 | 124 | folded | ported, by docs/symbols.csv: ported: zainetedit Compiler (ai.md section 9) |
| 0x004be5a0 | 60 | folded | covered where the port does it: src/mech3/zainetedit.cpp:463 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004be630 | 18 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004beb00 | 41 | folded | covered where the port does it: src/mech3/zainetedit.cpp:450 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004beb90 | 42 | folded | covered where the port does it: src/mech3/zainetedit.cpp:451 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004bec10 | 69 | folded | covered where the port does it: src/mech3/zainetedit.cpp:450 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004bed60 | 14 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004bef80 | 17 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:83) |
| 0x004beff0 | 10 | folded | a getter of a member |
| 0x004bf4d0 | 8 | folded | covered by the port where it names it (src/mech3/zainetedit.h:59) |
| 0x004bf4e0 | 32 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:438) |
| 0x004bf530 | 14 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004bf540 | 9 | folded | a thunk: one call to another function |
| 0x004bf550 | 22 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:569) |
| 0x004bf5b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004bf5d0 | 20 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:547) |
| 0x004bf620 | 8 | folded | empty: does nothing |
| 0x004bf630 | 8 | folded | returns a constant or a global |
| 0x004bf640 | 9 | folded | a thunk: one call to another function |
| 0x004bf650 | 13 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:559) |
| 0x004bf6f0 | 15 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:479) |
| 0x004bf750 | 43 | folded | covered where the port does it: src/mech3/game_editor_net.cpp:493 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004bf820 | 21 | replaced (SDL or the platform layer) | the AI net editor's file box (CFileDialogNet 0x004bf750: constructor, destructor, message map, class); replaced (FIX (A), ai.md) |
| 0x004bf870 | 8 | replaced (SDL or the platform layer) | the AI net editor's file box (CFileDialogNet 0x004bf750: constructor, destructor, message map, class); replaced (FIX (A), ai.md) |
| 0x004bf880 | 8 | folded | returns a constant or a global |
| 0x004bf890 | 12 | replaced (SDL or the platform layer) | the AI net editor's file box (CFileDialogNet 0x004bf750: constructor, destructor, message map, class); replaced (FIX (A), ai.md) |
| 0x004bf8d0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004bf8f0 | 8 | replaced (SDL or the platform layer) | the AI net editor's file box (CFileDialogNet 0x004bf750: constructor, destructor, message map, class); replaced (FIX (A), ai.md) |
| 0x004bf900 | 8 | folded | returns a constant or a global |
| 0x004bfb90 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004bfbb0 | 114 | folded | ported, by docs/symbols.csv: ported inline in VehicleNet: an id from znet's [300, 400), no starting list |
| 0x004bfe40 | 55 | folded | covered where the port does it: src/mech3/mech_waynet.cpp:147 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004bffc0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004bffe0 | 12 | folded | sets members (a setter or a small constructor) |
| 0x004c0000 | 11 | folded | sets members (a setter or a small constructor) |
| 0x004c0010 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c0030 | 10 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c0040 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c0060 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c02a0 | 11 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c02b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c02d0 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c02e0 | 9 | folded | a thunk: one call to another function |
| 0x004c0540 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c07a0 | 12 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c07d0 | 13 | folded | ported, by docs/symbols.csv: ported: FormationPart |
| 0x004c07f0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c0a40 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c0b30 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c0d40 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c0e10 | 26 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c0e40 | 26 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c0ed0 | 25 | unused: PORT (preserve) | the vehicle's +0xd8 record of each in/out query (its constructor and reset): written, never read (docs/symbols.csv: not kept; ai.md 11) |
| 0x004c0f70 | 20 | unused: PORT (preserve) | the vehicle's +0xd8 record of each in/out query (its constructor and reset): written, never read (docs/symbols.csv: not kept; ai.md 11) |
| 0x004c0fe0 | 14 | folded | ported, by docs/symbols.csv: ported: zainet_inout lerp (x and z, a's height) |
| 0x004c1030 | 18 | folded | ported, by docs/symbols.csv: ported: zainet_inout normalize_xz |
| 0x004c1090 | 9 | folded | ported, by docs/symbols.csv: ported: zainet_inout dist_xz |
| 0x004c10e0 | 9 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c1110 | 17 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c1160 | 23 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c11f0 | 63 | folded | ported, by docs/symbols.csv: ported: zainet_inout make_sides (box, side length, pushed-out corner) |
| 0x004c19f0 | 9 | folded | a thunk: one call to another function |
| 0x004c1a00 | 21 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c1a80 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c1ff0 | 32 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c2260 | 15 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c2290 | 15 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c22c0 | 50 | folded | ported, by docs/symbols.csv: ported: zainet_inout InOutPolygon::nearest (0x004c2470, 0x004c5290) |
| 0x004c2430 | 11 | folded | returns a constant or a global |
| 0x004c2670 | 16 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c27f0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c2820 | 13 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c2860 | 33 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c28f0 | 13 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c2920 | 11 | folded | a thunk: one call to another function |
| 0x004c2940 | 12 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c2970 | 11 | folded | a thunk: one call to another function |
| 0x004c29a0 | 39 | folded | ported, by docs/symbols.csv: ported: zainet_inout inout_set_state (0x004c2a30) |
| 0x004c2a30 | 13 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c2a80 | 30 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c2e00 | 10 | folded | ported, by docs/symbols.csv: ported: zainet_inout InOutArea::query |
| 0x004c2e90 | 22 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3210 | 10 | folded | a getter of a member |
| 0x004c3220 | 22 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3240 | 8 | folded | empty: does nothing |
| 0x004c3250 | 10 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3260 | 9 | folded | follows 0x004c3c90: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3270 | 9 | folded | a thunk: one call to another function |
| 0x004c3290 | 12 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3520 | 64 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c35e0 | 13 | folded | sets members (a setter or a small constructor) |
| 0x004c3600 | 14 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3630 | 27 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3910 | 10 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3ac0 | 12 | folded | sets members (a setter or a small constructor) |
| 0x004c3ae0 | 10 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3af0 | 10 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3b00 | 13 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3b20 | 13 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3b40 | 9 | folded | a thunk: one call to another function |
| 0x004c3b50 | 13 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3c90 | 13 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3cc0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c3cf0 | 30 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3da0 | 13 | folded | ported, by docs/symbols.csv: ported: zainet_inout inout_mission_load (listener 0x0060b338 slot +0x08) |
| 0x004c3dc0 | 8 | folded | returns a constant or a global |
| 0x004c3e30 | 9 | folded | follows 0x004c5090: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3f70 | 14 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c3fb0 | 41 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c40e0 | 19 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c4139 | 16 | folded | follows 0x004c40e0: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c414d | 10 | folded | follows 0x004c40e0: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c4150 | 10 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c4170 | 11 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c4190 | 9 | folded | a thunk: one call to another function |
| 0x004c4240 | 20 | folded | ported, by docs/symbols.csv: ported: the program's InOutHooks::ground |
| 0x004c4590 | 16 | folded | covered by the port where it names it (src/mech3/zainet_inout_view.cpp:39) |
| 0x004c4900 | 12 | folded | sets members (a setter or a small constructor) |
| 0x004c4980 | 10 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c4990 | 9 | folded | follows 0x004c4170: the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c49a0 | 9 | folded | a thunk: one call to another function |
| 0x004c49c0 | 9 | folded | a thunk: one call to another function |
| 0x004c49d0 | 10 | folded | a getter of a member |
| 0x004c49e0 | 9 | folded | a thunk: one call to another function |
| 0x004c4a00 | 9 | folded | a thunk: one call to another function |
| 0x004c4a20 | 9 | folded | a thunk: one call to another function |
| 0x004c4a40 | 11 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c4a60 | 15 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c4a90 | 85 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c4c60 | 14 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c4c80 | 55 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c4d60 | 13 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c4d90 | 10 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c4db0 | 10 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c4dc0 | 107 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5020 | 28 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5090 | 20 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c50e0 | 10 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5100 | 30 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5170 | 10 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5180 | 28 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c51c0 | 24 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c51f0 | 8 | folded | empty: does nothing |
| 0x004c5200 | 8 | folded | empty: does nothing |
| 0x004c5210 | 22 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5250 | 16 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5290 | 30 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c52f0 | 15 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5320 | 21 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5360 | 16 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c53a0 | 15 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c53d0 | 15 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5400 | 17 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5440 | 14 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5470 | 19 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c54d0 | 15 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5500 | 23 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5530 | 15 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5560 | 68 | folded | the AI's mission boundary and region views (zainet_inout, zainet_inout_view; ai.md section 11): vector and list instances, the polygon and area constructors and destructors, the walks the port does as loops, getters, start-up templates |
| 0x004c5720 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c5740 | 13 | folded | covered by the port where it names it (src/mech3/zainet_region.cpp:38) |
| 0x004c57c0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x004c58d0 | 10 | folded | read: the static null edge at [0x0060b3d0] (a RegionEdge with zero ends, vtable 0x005cace0), which its vt+0x10 (0x004c5ad0, AiElem's miss) returns for an index past the edges; the port's regions read only edges within their count (zainet_region.h), its miss the base's null |
| 0x004c5900 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c5930 | 9 | folded | a thunk: one call to another function |
| 0x004c5ad0 | 8 | folded | returns a constant or a global |
| 0x004c5ae0 | 8 | folded | returns a constant or a global |
| 0x004c5b70 | 11 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c5b80 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c5d20 | 11 | folded | a thunk: one call to another function |
| 0x004c5d40 | 9 | folded | a thunk: one call to another function |
| 0x004c5d50 | 9 | folded | a thunk: one call to another function |
| 0x004c5d80 | 19 | folded | covered by the port where it names it (src/mech3/zainet_region.cpp:87) |
| 0x004c5db0 | 35 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c5fa0 | 9 | folded | a thunk: one call to another function |
| 0x004c5fb0 | 37 | folded | covered by the port where it names it (src/mech3/zainet_node.h:30) |
| 0x004c6030 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c6bb0 | 11 | folded | covered by the port where it names it (src/mech3/zrdrainet.cpp:352) |
| 0x004c6ca0 | 12 | folded | covered by the port where it names it (src/mech3/zainet_node.h:29) |
| 0x004c6cc0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c6ce0 | 12 | folded | covered by the port where it names it (src/mech3/zainet.cpp:36) |
| 0x004c6d00 | 12 | folded | covered by the port where it names it (src/mech3/zainet_node.h:9) |
| 0x004c6d10 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c6e10 | 22 | folded | covered by the port where it names it (src/mech3/zainet.h:59) |
| 0x004c6e60 | 27 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:93) |
| 0x004c6fa0 | 9 | folded | covered by the port where it names it (src/mech3/zainet.cpp:61) |
| 0x004c7080 | 9 | folded | covered by the port where it names it (src/mech3/zainet.cpp:84) |
| 0x004c70a0 | 27 | folded | covered by the port where it names it (src/mech3/zainet.cpp:85) |
| 0x004c7110 | 23 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c71d0 | 9 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:27) |
| 0x004c7240 | 14 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:67) |
| 0x004c7260 | 9 | folded | a thunk: one call to another function |
| 0x004c72c0 | 8 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:70) |
| 0x004c72d0 | 9 | folded | covered by the port where it names it (src/mech3/zainetedit.cpp:83) |
| 0x004c72f0 | 9 | folded | a thunk: one call to another function |
| 0x004c7300 | 15 | folded | covered by the port where it names it (src/mech3/zainet.cpp:130) |
| 0x004c7530 | 20 | folded | covered by the port where it names it (src/mech3/zainet_node.cpp:106) |
| 0x004c7570 | 29 | folded | covered by the port where it names it (src/mech3/zainet_node.cpp:113) |
| 0x004c75c0 | 117 | folded | covered where the port does it: src/mech3/zainet_node.cpp:40 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004c7760 | 15 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c7790 | 13 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c77b0 | 9 | folded | a thunk: one call to another function |
| 0x004c77c0 | 12 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c77e0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c7810 | 8 | folded | returns a constant or a global |
| 0x004c7820 | 13 | folded | covered by the port where it names it (src/upgrades/editor/node_params.h:8) |
| 0x004c7840 | 34 | folded | covered by the port where it names it (src/upgrades/editor/node_params.h:7) |
| 0x004c78c0 | 15 | dev dialog (MFC), its window plumbing: replaced | an AI dev dialog's window close (PostNcDestroy, [0x0060b420] cleared) |
| 0x004c7950 | 11 | folded | covered by the port where it names it (src/mech3/zai_node.cpp:103) |
| 0x004c7960 | 27 | folded | ported, by docs/symbols.csv: ported; LinkWay::measure (first half) |
| 0x004c79e0 | 11 | folded | read: a static float made once from a double constant ([0x0060b44c] to [0x0060b46c]), read by nothing: no effect |
| 0x004c7ae0 | 16 | folded | ported, by docs/symbols.csv: ported inline; LinkWay |
| 0x004c7b20 | 9 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c7b30 | 9 | folded | a thunk: one call to another function |
| 0x004c7b80 | 20 | folded | covered by the port where it names it (src/gamez/znetwork/znet_object.cpp:8) |
| 0x004c7bb0 | 24 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c7d00 | 27 | folded | ported, by docs/symbols.csv: ported: zai_path (vtable 0x005cb178) |
| 0x004c7d60 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c7d80 | 55 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c8600 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c8640 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c8660 | 12 | folded | sets members (a setter or a small constructor) |
| 0x004c8680 | 11 | folded | sets members (a setter or a small constructor) |
| 0x004c86e0 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c8700 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c8720 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c8740 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c8750 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c8770 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c8790 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c87b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c87d0 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c87e0 | 8 | folded | returns a constant or a global |
| 0x004c88a0 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c88c0 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c88e0 | 8 | folded | returns a constant or a global |
| 0x004c88f0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c8910 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c8920 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c8940 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c8960 | 8 | folded | returns a constant or a global |
| 0x004c8970 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c8990 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c89a0 | 15 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c89f0 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c8a10 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c8a30 | 15 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c8a60 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c8f70 | 11 | folded | follows 0x004c8f80: covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c8f80 | 102 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c9100 | 19 | folded | covered by the port where it names it (src/mech3/zai_steps.h:209) |
| 0x004c9140 | 11 | dev dialog (MFC), its logic: PORT (preserve) | read: the steps' link editing (Step's vt+0x30..+0x44, vtable 0x005cb3d0, and the node steps' vt+0x30..+0x50, vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4): set, compare, find or count a link (+0x0c / +0x20) from a picked element's behaviour (its vt+0x60 then +0x74), the node's position set: the AI path editor's (ZAiNetEdit's editing side, to port behind --debug, docs/ai.md The developer tools). No run-time caller found: the port's AI, emulator-checked on every shipped net (compile, pilot, orders), has no such slots, and no passing PM AI test reaches the PM twins (block trace); the virtual calls through these offsets could not all be typed statically |
| 0x004c9150 | 14 | folded | ported, by docs/symbols.csv: ported; the node state in place (ai.md 17.4a) |
| 0x004c9180 | 59 | dev dialog (MFC), its logic: PORT (preserve) | read: the steps' link editing (Step's vt+0x30..+0x44, vtable 0x005cb3d0, and the node steps' vt+0x30..+0x50, vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4): set, compare, find or count a link (+0x0c / +0x20) from a picked element's behaviour (its vt+0x60 then +0x74), the node's position set: the AI path editor's (ZAiNetEdit's editing side, to port behind --debug, docs/ai.md The developer tools). No run-time caller found: the port's AI, emulator-checked on every shipped net (compile, pilot, orders), has no such slots, and no passing PM AI test reaches the PM twins (block trace); the virtual calls through these offsets could not all be typed statically |
| 0x004c9260 | 21 | dev dialog (MFC), its logic: PORT (preserve) | read: the steps' link editing (Step's vt+0x30..+0x44, vtable 0x005cb3d0, and the node steps' vt+0x30..+0x50, vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4): set, compare, find or count a link (+0x0c / +0x20) from a picked element's behaviour (its vt+0x60 then +0x74), the node's position set: the AI path editor's (ZAiNetEdit's editing side, to port behind --debug, docs/ai.md The developer tools). No run-time caller found: the port's AI, emulator-checked on every shipped net (compile, pilot, orders), has no such slots, and no passing PM AI test reaches the PM twins (block trace); the virtual calls through these offsets could not all be typed statically |
| 0x004c92b0 | 35 | dev dialog (MFC), its logic: PORT (preserve) | read: the steps' link editing (Step's vt+0x30..+0x44, vtable 0x005cb3d0, and the node steps' vt+0x30..+0x50, vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4): set, compare, find or count a link (+0x0c / +0x20) from a picked element's behaviour (its vt+0x60 then +0x74), the node's position set: the AI path editor's (ZAiNetEdit's editing side, to port behind --debug, docs/ai.md The developer tools). No run-time caller found: the port's AI, emulator-checked on every shipped net (compile, pilot, orders), has no such slots, and no passing PM AI test reaches the PM twins (block trace); the virtual calls through these offsets could not all be typed statically |
| 0x004c9310 | 22 | dev dialog (MFC), its logic: PORT (preserve) | read: the steps' link editing (Step's vt+0x30..+0x44, vtable 0x005cb3d0, and the node steps' vt+0x30..+0x50, vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4): set, compare, find or count a link (+0x0c / +0x20) from a picked element's behaviour (its vt+0x60 then +0x74), the node's position set: the AI path editor's (ZAiNetEdit's editing side, to port behind --debug, docs/ai.md The developer tools). No run-time caller found: the port's AI, emulator-checked on every shipped net (compile, pilot, orders), has no such slots, and no passing PM AI test reaches the PM twins (block trace); the virtual calls through these offsets could not all be typed statically |
| 0x004c9360 | 35 | dev dialog (MFC), its logic: PORT (preserve) | read: the steps' link editing (Step's vt+0x30..+0x44, vtable 0x005cb3d0, and the node steps' vt+0x30..+0x50, vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4): set, compare, find or count a link (+0x0c / +0x20) from a picked element's behaviour (its vt+0x60 then +0x74), the node's position set: the AI path editor's (ZAiNetEdit's editing side, to port behind --debug, docs/ai.md The developer tools). No run-time caller found: the port's AI, emulator-checked on every shipped net (compile, pilot, orders), has no such slots, and no passing PM AI test reaches the PM twins (block trace); the virtual calls through these offsets could not all be typed statically |
| 0x004c93c0 | 8 | folded | returns a constant or a global |
| 0x004c9410 | 37 | dev dialog (MFC), its logic: PORT (preserve) | read: the steps' link editing (Step's vt+0x30..+0x44, vtable 0x005cb3d0, and the node steps' vt+0x30..+0x50, vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4): set, compare, find or count a link (+0x0c / +0x20) from a picked element's behaviour (its vt+0x60 then +0x74), the node's position set: the AI path editor's (ZAiNetEdit's editing side, to port behind --debug, docs/ai.md The developer tools). No run-time caller found: the port's AI, emulator-checked on every shipped net (compile, pilot, orders), has no such slots, and no passing PM AI test reaches the PM twins (block trace); the virtual calls through these offsets could not all be typed statically |
| 0x004c9460 | 11 | folded | covered by the port where it names it (src/mech3/zai_steps.h:205) |
| 0x004c9480 | 11 | dev dialog (MFC), its logic: PORT (preserve) | read: the steps' link editing (Step's vt+0x30..+0x44, vtable 0x005cb3d0, and the node steps' vt+0x30..+0x50, vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4): set, compare, find or count a link (+0x0c / +0x20) from a picked element's behaviour (its vt+0x60 then +0x74), the node's position set: the AI path editor's (ZAiNetEdit's editing side, to port behind --debug, docs/ai.md The developer tools). No run-time caller found: the port's AI, emulator-checked on every shipped net (compile, pilot, orders), has no such slots, and no passing PM AI test reaches the PM twins (block trace); the virtual calls through these offsets could not all be typed statically |
| 0x004c9730 | 14 | folded | ported, by docs/symbols.csv: ported: Behaviour::make_state (VehMode1) |
| 0x004c9850 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c9870 | 12 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004c9894 | 198 | folded | a piece of the ported function before it (Ghidra split it there) (0x004c9880) |
| 0x004c9d20 | 25 | dev dialog (MFC), its logic: PORT (preserve) | read: the steps' link editing (Step's vt+0x30..+0x44, vtable 0x005cb3d0, and the node steps' vt+0x30..+0x50, vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4): set, compare, find or count a link (+0x0c / +0x20) from a picked element's behaviour (its vt+0x60 then +0x74), the node's position set: the AI path editor's (ZAiNetEdit's editing side, to port behind --debug, docs/ai.md The developer tools). No run-time caller found: the port's AI, emulator-checked on every shipped net (compile, pilot, orders), has no such slots, and no passing PM AI test reaches the PM twins (block trace); the virtual calls through these offsets could not all be typed statically |
| 0x004c9d70 | 21 | dev dialog (MFC), its logic: PORT (preserve) | read: the steps' link editing (Step's vt+0x30..+0x44, vtable 0x005cb3d0, and the node steps' vt+0x30..+0x50, vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4): set, compare, find or count a link (+0x0c / +0x20) from a picked element's behaviour (its vt+0x60 then +0x74), the node's position set: the AI path editor's (ZAiNetEdit's editing side, to port behind --debug, docs/ai.md The developer tools). No run-time caller found: the port's AI, emulator-checked on every shipped net (compile, pilot, orders), has no such slots, and no passing PM AI test reaches the PM twins (block trace); the virtual calls through these offsets could not all be typed statically |
| 0x004c9db0 | 14 | dev dialog (MFC), its logic: PORT (preserve) | read: the steps' link editing (Step's vt+0x30..+0x44, vtable 0x005cb3d0, and the node steps' vt+0x30..+0x50, vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4): set, compare, find or count a link (+0x0c / +0x20) from a picked element's behaviour (its vt+0x60 then +0x74), the node's position set: the AI path editor's (ZAiNetEdit's editing side, to port behind --debug, docs/ai.md The developer tools). No run-time caller found: the port's AI, emulator-checked on every shipped net (compile, pilot, orders), has no such slots, and no passing PM AI test reaches the PM twins (block trace); the virtual calls through these offsets could not all be typed statically |
| 0x004c9de0 | 14 | dev dialog (MFC), its logic: PORT (preserve) | read: the steps' link editing (Step's vt+0x30..+0x44, vtable 0x005cb3d0, and the node steps' vt+0x30..+0x50, vtables 0x005cbf3c, 0x005cc6fc, 0x005cc7b4): set, compare, find or count a link (+0x0c / +0x20) from a picked element's behaviour (its vt+0x60 then +0x74), the node's position set: the AI path editor's (ZAiNetEdit's editing side, to port behind --debug, docs/ai.md The developer tools). No run-time caller found: the port's AI, emulator-checked on every shipped net (compile, pilot, orders), has no such slots, and no passing PM AI test reaches the PM twins (block trace); the virtual calls through these offsets could not all be typed statically |
| 0x004c9e00 | 11 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c9e20 | 11 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c9e40 | 8 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c9e50 | 11 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c9eb0 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c9ed0 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c9ef0 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c9f10 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004c9f30 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c9f80 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004c9fa0 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ca040 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004ca060 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ca0a0 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ca0c0 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ca0e0 | 25 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ca160 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004ca180 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ca190 | 11 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ca1c0 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ca1f0 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ca290 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ca2b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004ca2d0 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ca330 | 9 | folded | a thunk: one call to another function |
| 0x004ca340 | 9 | folded | a thunk: one call to another function |
| 0x004ca370 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ca390 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ca3b0 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ca3e0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004ca400 | 12 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004ca410 | 11 | folded | ported, by docs/symbols.csv: ported: AfterManoeuvreState::resume |
| 0x004ca440 | 59 | folded | ported, by docs/symbols.csv: ported: zai_attack ManoeuvreState (its first part) |
| 0x004ca550 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004caac0 | 14 | folded | ported, by docs/symbols.csv: ported: Behaviour::make_state (Manoeuvre) |
| 0x004cab90 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cabc0 | 14 | folded | ported, by docs/symbols.csv: ported: Behaviour::make_state (Manoeuvre2) |
| 0x004cabe0 | 12 | folded | ported, by docs/symbols.csv: ported: ManoeuvreState (no jets) |
| 0x004cac00 | 14 | folded | covered by the port where it names it (src/mech3/zai_attack.h:14) |
| 0x004cac20 | 15 | folded | ported, by docs/symbols.csv: ported; AttackEndState |
| 0x004cac50 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cac70 | 12 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004caca0 | 12 | folded | covered by the port where it names it (src/mech3/zai_attack.h:87) |
| 0x004cacc0 | 14 | folded | covered by the port where it names it (src/mech3/zai_attack.h:87) |
| 0x004cace0 | 12 | folded | ported, by docs/symbols.csv: ported; AttackRearmState |
| 0x004cad00 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cad20 | 12 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004cad70 | 13 | folded | covered by the port where it names it (src/mech3/zai_attack.h:91) |
| 0x004cad90 | 14 | folded | covered by the port where it names it (src/mech3/zai_attack.h:91) |
| 0x004cadb0 | 16 | folded | ported, by docs/symbols.csv: ported; AttackReturnState (the way home) |
| 0x004cade0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cae00 | 12 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004cb250 | 22 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cb2a0 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cb310 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cb470 | 8 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cb480 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cb4a0 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cb950 | 63 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cba40 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cba60 | 88 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cbc30 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cbc50 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cbc70 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cbc90 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cbcf0 | 22 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cbd40 | 22 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cbd90 | 15 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cbdd0 | 9 | folded | a thunk: one call to another function |
| 0x004cbde0 | 9 | folded | a thunk: one call to another function |
| 0x004cbe10 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cbe30 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cbe70 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cbe90 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cbea0 | 8 | folded | returns a constant or a global |
| 0x004cc240 | 12 | folded | ported, by docs/symbols.csv: ported; StationHoldState |
| 0x004cc260 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cc350 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cc370 | 12 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004ccc30 | 26 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cccb0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cccd0 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ccce0 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004ccd70 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004ccd90 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cce30 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cce50 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cce70 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cce90 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cceb0 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cd2a0 | 51 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004cd360 | 34 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004cd3e0 | 15 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004cd4a0 | 22 | folded | ported, by docs/symbols.csv: ported: zai_steps (vtable 0x005cc0e0) |
| 0x004cd500 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cd520 | 20 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004cd560 | 14 | folded | ported, by docs/symbols.csv: ported: Behaviour::make_state (Gate) |
| 0x004cd640 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cd660 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cd680 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cd6b0 | 8 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cd6c0 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cd6e0 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cd700 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cd720 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cd740 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cd760 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cd770 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cd790 | 14 | unused: PORT (preserve) | the brain's own step at +0x240 (0x004cd770, vtable 0x005cc290): built by the brain's constructor, but nothing orders it (only the constructor takes +0x240), so its make_state, state and run (path following toward a point, 98 lines) never run |
| 0x004cd7b0 | 14 | unused: PORT (preserve) | the brain's own step at +0x240 (0x004cd770, vtable 0x005cc290): built by the brain's constructor, but nothing orders it (only the constructor takes +0x240), so its make_state, state and run (path following toward a point, 98 lines) never run |
| 0x004cd7e0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cd800 | 12 | unused: PORT (preserve) | the brain's own step at +0x240 (0x004cd770, vtable 0x005cc290): built by the brain's constructor, but nothing orders it (only the constructor takes +0x240), so its make_state, state and run (path following toward a point, 98 lines) never run |
| 0x004cd810 | 98 | unused: PORT (preserve) | the brain's own step at +0x240 (0x004cd770, vtable 0x005cc290): built by the brain's constructor, but nothing orders it (only the constructor takes +0x240), so its make_state, state and run (path following toward a point, 98 lines) never run |
| 0x004cda50 | 47 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cdb00 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cdb20 | 20 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cdb60 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cdbc0 | 9 | folded | a thunk: one call to another function |
| 0x004cdbd0 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cdbf0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cdc10 | 12 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cdc20 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cdc50 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cdc70 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cdc80 | 14 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cdca0 | 13 | folded | AI steps and their states: constructors, make_state factories, destructors and one-line virtuals of the step classes ported in zai_steps/zai_brain/zai_node/zai_attack (the port's make_state switch, zai_brain.cpp:715..745) |
| 0x004cdcc0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cdce0 | 12 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004ce200 | 14 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004ce230 | 9 | folded | a thunk: one call to another function |
| 0x004ce2a0 | 98 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004ce4a0 | 37 | folded | covered by the port where it names it (src/mech3/zai_node.cpp:147) |
| 0x004ce5b0 | 14 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004ce5d0 | 14 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004ce5f0 | 14 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004ce610 | 14 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004ce690 | 12 | folded | sets members (a setter or a small constructor) |
| 0x004ce7d0 | 17 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004ce860 | 63 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004ce9f0 | 14 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004cecf0 | 18 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004cee20 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cee40 | 12 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004cee50 | 16 | folded | ported, by docs/symbols.csv: ported inline; /along/, /side/, their sum |
| 0x004cf560 | 44 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004cf960 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cf980 | 27 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x004cfa00 | 12 | folded | ported, by docs/symbols.csv: ported: zai_steps (vtable 0x005cc7a8) |
| 0x004cfa30 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cfa60 | 14 | folded | ported, by docs/symbols.csv: ported; the backup state in place |
| 0x004cfa80 | 12 | folded | ported, by docs/symbols.csv: ported; BackupState (the node state's ctor, vtable 0x005cc860) |
| 0x004cfaa0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cfac0 | 12 | folded | covered: the firing's records, the net editor class's forwarding virtuals, vector and list instances, the region registry's helpers, the AI states' and steps' constructors and destructors, start-up statics |
| 0x004cfe90 | 9 | folded | follows 0x004cfed0: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004cfea0 | 9 | folded | a thunk: one call to another function |
| 0x004cfed0 | 12 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004cff00 | 10 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004cff10 | 31 | folded | ported, by docs/symbols.csv: ported: zai_steps (vtable 0x005cc898) |
| 0x004cffa0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004cffc0 | 12 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d0020 | 14 | folded | ported, by docs/symbols.csv: ported; the deploy state in place |
| 0x004d0040 | 13 | folded | ported, by docs/symbols.csv: ported; DeployState (+0x28 the node) |
| 0x004d0070 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d0090 | 12 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d08d0 | 12 | folded | ported, by docs/symbols.csv: ported: vtable 0x005cc970, the refit's (on its stack) |
| 0x004d08f0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d0910 | 11 | folded | sets members (a setter or a small constructor) |
| 0x004d0b80 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d12a0 | 29 | folded | covered by the port where it names it (src/mech3/zai_attack.cpp:713) |
| 0x004d1300 | 14 | folded | ported, by docs/symbols.csv: ported: Behaviour::make_state (VehMode2) |
| 0x004d1380 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d13a0 | 12 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d13b0 | 11 | folded | ported, by docs/symbols.csv: ported: OrbitState::resume |
| 0x004d1630 | 14 | folded | ported, by docs/symbols.csv: ported: Behaviour::make_state (VehMode4) |
| 0x004d16b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d16d0 | 12 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d16e0 | 11 | folded | ported, by docs/symbols.csv: ported: VeerState::resume |
| 0x004d1950 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d1970 | 12 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d1c80 | 12 | folded | covered by the port where it names it (src/mech3/zai_steps.h:63) |
| 0x004d1ca0 | 14 | folded | ported, by docs/symbols.csv: ported; the fight state (make_state for VehAttack) |
| 0x004d1cc0 | 13 | folded | ported, by docs/symbols.csv: ported; FightState |
| 0x004d1ce0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d1d00 | 12 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d1e60 | 10 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d1f40 | 13 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:324) |
| 0x004d1f60 | 17 | folded | covered by the port where it names it (src/mech3/zai_target.h:122) |
| 0x004d1f80 | 94 | folded | covered where the port does it: src/mech3/zrdraiv.cpp:555 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d2460 | 13 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:581) |
| 0x004d2480 | 19 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:309) |
| 0x004d24b0 | 20 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:423) |
| 0x004d26f0 | 68 | folded | covered where the port does it: src/mech3/zai_target.cpp:481 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d2840 | 29 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d28e0 | 29 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d2a80 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d2b20 | 8 | folded | returns a constant or a global |
| 0x004d2fe0 | 20 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d3080 | 12 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:416) |
| 0x004d30b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d30d0 | 24 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:446) |
| 0x004d3130 | 41 | folded | covered where the port does it: src/mech3/zai_target.cpp:635 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d32c0 | 20 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d3320 | 8 | folded | covered by the port where it names it (src/mech3/entity_classes.cpp:44) |
| 0x004d3330 | 11 | folded | covered by the port where it names it (src/mech3/entity_classes.cpp:44) |
| 0x004d3360 | 8 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:566) |
| 0x004d3370 | 8 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:566) |
| 0x004d3380 | 8 | folded | covered by the port where it names it (src/mech3/app_sensors.cpp:158) |
| 0x004d35d0 | 9 | folded | a thunk: one call to another function |
| 0x004d35f0 | 20 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d3640 | 16 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d3680 | 125 | folded | covered where the port does it: src/mech3/zai_target.cpp:322 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d3890 | 26 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:322) |
| 0x004d38f0 | 55 | folded | covered where the port does it: src/mech3/zai_target.cpp:322 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d39f0 | 40 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:322) |
| 0x004d3a80 | 41 | folded | covered where the port does it: src/mech3/zai_target.cpp:322 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d3d20 | 11 | folded | a thunk: one call to another function |
| 0x004d3d70 | 9 | folded | a thunk: one call to another function |
| 0x004d3d80 | 9 | folded | a thunk: one call to another function |
| 0x004d3db0 | 35 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d3fd0 | 28 | folded | covered by the port where it names it (src/mech3/zai_valve.h:42) |
| 0x004d4040 | 12 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d4060 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d4310 | 9 | folded | a thunk: one call to another function |
| 0x004d4490 | 12 | folded | covered by the port where it names it (src/mech3/zai_steps.cpp:62) |
| 0x004d44b0 | 61 | folded | covered where the port does it: src/mech3/zainet_node.cpp:40 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d45b0 | 12 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d45c0 | 21 | folded | covered by the port where it names it (src/mech3/zainet_node.h:10) |
| 0x004d4630 | 48 | folded | covered where the port does it: src/mech3/zainet_node.cpp:106 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d46b0 | 21 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d46f0 | 12 | folded | ported, by docs/symbols.csv: ported inline: the claim's temporary record zeroed |
| 0x004d47e0 | 23 | folded | ported, by docs/symbols.csv: ported (anim_owner_release): every record of the owner let go; the valve's test-and-store does nothing |
| 0x004d4840 | 9 | folded | a thunk: one call to another function |
| 0x004d4850 | 9 | folded | a thunk: one call to another function |
| 0x004d4870 | 34 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d48f0 | 29 | folded | covered by the port where it names it (src/mech3/zai_anim_owner.h:27) |
| 0x004d4960 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d4a20 | 41 | folded | ported, by docs/symbols.csv: ported (anim_owner_message): the owner's vt+0x74, 0x11 viaNode, 0x2a..0x31 the valve off/on, 0x39..0x3c the inout tests |
| 0x004d4c70 | 9 | folded | a thunk: one call to another function |
| 0x004d5110 | 14 | folded | covered by the port where it names it (src/mech3/mech_fieldbase_dock.cpp:292) |
| 0x004d5120 | 12 | folded | ported, by docs/symbols.csv: ported: zainet (vtable 0x005ccda8) |
| 0x004d5140 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d5160 | 12 | folded | covered by the port where it names it (src/mech3/zainet.h:36) |
| 0x004d5170 | 21 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:27) |
| 0x004d52b0 | 26 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d5300 | 26 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d5350 | 65 | folded | covered where the port does it: src/mech3/app_mission.cpp:699 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d5510 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d5530 | 13 | dev dialog (MFC), its window plumbing: replaced | the AI and editor dialogs' MFC base (vtable 0x005cce00: CDialog constructor, modeless Create, close with the cursor hidden, DoModal, OnInitDialog with the cursor shown) |
| 0x004d5560 | 12 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x004d5570 | 16 | dev dialog (MFC), its window plumbing: replaced | the AI and editor dialogs' MFC base (vtable 0x005cce00: CDialog constructor, modeless Create, close with the cursor hidden, DoModal, OnInitDialog with the cursor shown) |
| 0x004d55a0 | 14 | dev dialog (MFC), its window plumbing: replaced | the AI and editor dialogs' MFC base (vtable 0x005cce00: CDialog constructor, modeless Create, close with the cursor hidden, DoModal, OnInitDialog with the cursor shown) |
| 0x004d55c0 | 12 | dev dialog (MFC), its window plumbing: replaced | the AI and editor dialogs' MFC base (vtable 0x005cce00: CDialog constructor, modeless Create, close with the cursor hidden, DoModal, OnInitDialog with the cursor shown) |
| 0x004d55d0 | 14 | dev dialog (MFC), its window plumbing: replaced | the AI and editor dialogs' MFC base (vtable 0x005cce00: CDialog constructor, modeless Create, close with the cursor hidden, DoModal, OnInitDialog with the cursor shown) |
| 0x004d55f0 | 15 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:479) |
| 0x004d5610 | 17 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:479) |
| 0x004d5640 | 29 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d5690 | 9 | folded | a thunk: one call to another function |
| 0x004d56e0 | 69 | folded | covered where the port does it: src/mech3/mech.h:102 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d5880 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d58a0 | 39 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x004d5bc0 | 19 | folded | covered by the port where it names it (src/mech3/sensors.cpp:646) |
| 0x004d6260 | 107 | folded | ported, by docs/symbols.csv: ported: Shift+E |
| 0x004d6710 | 140 | folded | ported, by docs/symbols.csv: ported: E |
| 0x004d6a80 | 68 | folded | ported, by docs/symbols.csv: ported: Ctrl+E |
| 0x004d7160 | 12 | unused: PORT (preserve) | read: AiTarget's vt+0x6c (vtable 0x005bcf64 and five more: the mechs, the network mech, turrets): the whole node tinted (blend 0.6, 0x004fb8c0) and its blend switched (0x004fb840), never overridden; the target window tints through vt+0x68 instead (the mech's 0x0041b9d0, the turret's 0x004a75b0; the port's hud_object_view_tint, app_sensors.cpp:454), and no call through +0x6c with a flag on an entity was found |
| 0x004d7200 | 10 | folded | covered by the port where it names it (src/mech3/app_sensors.cpp:159) |
| 0x004d7220 | 11 | folded | sets members (a setter or a small constructor) |
| 0x004d7230 | 11 | folded | sets members (a setter or a small constructor) |
| 0x004d7240 | 15 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d7250 | 10 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d7290 | 8 | folded | returns a constant or a global |
| 0x004d7330 | 20 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d7360 | 114 | folded | covered where the port does it: src/crt/msvc_sort.h:7 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d7a40 | 82 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d7bc0 | 323 | folded | covered where the port does it: src/mech3/sensors.cpp:586 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d7f80 | 94 | folded | covered where the port does it: src/mech3/app_mech.cpp:2170 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d81c0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d81e0 | 25 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:447) |
| 0x004d8250 | 9 | folded | a thunk: one call to another function |
| 0x004d8270 | 9 | folded | a thunk: one call to another function |
| 0x004d8280 | 48 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d8330 | 12 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d83c0 | 29 | folded | covered by the port where it names it (src/mech3/zai_target.cpp:453) |
| 0x004d8420 | 53 | folded | covered where the port does it: src/mech3/zrdrvalve.h:36 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004d84b0 | 9 | folded | follows 0x004d8280: covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004d84c0 | 9 | folded | a thunk: one call to another function |
| 0x004d8540 | 20 | folded | covered by the port where it names it (src/mech3/zai_list.h:5) |
| 0x004d8570 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004d8590 | 34 | folded | covered by the port where it names it (src/mech3/zai_list.h:6) |
| 0x004d8670 | 34 | folded | covered by the port where it names it (src/mech3/mech_control.cpp:532) |
| 0x004d8820 | 24 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d88a0 | 13 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d88d0 | 9 | folded | a thunk: one call to another function |
| 0x004d88f0 | 18 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d89f0 | 9 | folded | a thunk: one call to another function |
| 0x004d8c40 | 24 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d8d30 | 18 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d8d50 | 21 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d8d70 | 21 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d8d90 | 24 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d8dc0 | 22 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d8df0 | 19 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d8e10 | 11 | folded | sets members (a setter or a small constructor) |
| 0x004d8e20 | 12 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d8e40 | 12 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d8fe0 | 12 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d9000 | 16 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d9020 | 27 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d9090 | 49 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d91e0 | 17 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d9210 | 18 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d9250 | 18 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d92f0 | 30 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d9360 | 30 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d9480 | 71 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004d9580 | 71 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004d9680 | 103 | folded | the vehicle editor's logic and the AI parameters it edits, ported in game_editor_vehicles / zai_params / zrdraiv (their comments name these): the vehicle list, the valve and region lists, the params' comparisons, the vlist's appends and defaults; the firing plan's std algorithms |
| 0x004d9840 | 33 | folded | ported, by docs/symbols.csv: ported: PlayerControls::settings (an AiParams with the defaults) |
| 0x004d9940 | 38 | folded | covered by the port where it names it (src/mech3/game_editor_vehicles.cpp:535) |
| 0x004d9bf0 | 13 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d9cc0 | 9 | folded | a thunk: one call to another function |
| 0x004d9cd0 | 21 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d9d40 | 18 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004d9d80 | 18 | dev dialog (MFC), its logic: PORT (preserve) | an AI parameters dialog (MFC): what it reads into its list (the net's records) and positions; preserve as logic |
| 0x004d9dc0 | 45 | dev dialog (MFC), its logic: PORT (preserve) | an AI parameters dialog (MFC): what it reads into its list (the net's records) and positions; preserve as logic |
| 0x004d9ed0 | 31 | dev dialog (MFC), its logic: PORT (preserve) | an AI parameters dialog (MFC): what it reads into its list (the net's records) and positions; preserve as logic |
| 0x004d9f30 | 18 | dev dialog (MFC), its window plumbing: replaced | the same AI parameters dialog's MFC plumbing (UpdateData, a flag cleared on close) |
| 0x004d9f70 | 9 | dev dialog (MFC), its window plumbing: replaced | the same AI parameters dialog's MFC plumbing (UpdateData, a flag cleared on close) |
| 0x004d9f80 | 9 | folded | a thunk: one call to another function |
| 0x004d9fc0 | 32 | dev dialog (MFC), its logic: PORT (preserve) | an AI parameters dialog (MFC): what it reads into its list (the net's records) and positions; preserve as logic |
| 0x004da1c0 | 14 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004da1f0 | 8 | folded | empty: does nothing |
| 0x004da200 | 11 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004da220 | 45 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004da2f0 | 15 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004da320 | 23 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004da3a0 | 22 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004da410 | 46 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004da500 | 22 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004da730 | 8 | folded | returns a constant or a global |
| 0x004da740 | 14 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004da780 | 12 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004da790 | 12 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004da7b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004da7d0 | 12 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004da800 | 10 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004da820 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004da8f0 | 9 | folded | a thunk: one call to another function |
| 0x004dac70 | 24 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004dacb0 | 26 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004dae30 | 9 | folded | a thunk: one call to another function |
| 0x004dae40 | 23 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004daee0 | 119 | folded | ported, by docs/symbols.csv: ported: zrdrainet |
| 0x004db140 | 20 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004db8d0 | 314 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004dc150 | 9 | folded | a thunk: one call to another function |
| 0x004dc160 | 45 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004dc230 | 9 | folded | a thunk: one call to another function |
| 0x004dc260 | 13 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004dc270 | 13 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004dc290 | 13 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004dc620 | 14 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004dc640 | 15 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004dc670 | 15 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004dc680 | 13 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004dc6e0 | 15 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004dc6f0 | 13 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004dc700 | 8 | folded | returns a constant or a global |
| 0x004dc710 | 317 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004dcc50 | 59 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004dcdc0 | 59 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004dcf30 | 32 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004dcfc0 | 60 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004dd0d0 | 317 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004dd610 | 32 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004dd6a0 | 19 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004dd6e0 | 197 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004dda00 | 48 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ddab0 | 52 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ddb70 | 48 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004ddc20 | 25 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004ddc90 | 174 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004de0b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004de0e0 | 9 | folded | a thunk: one call to another function |
| 0x004de100 | 14 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004de130 | 12 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004de150 | 60 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004de260 | 21 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004de2c0 | 197 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004de5e0 | 42 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004de670 | 52 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004de930 | 14 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004de940 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004de960 | 12 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004de980 | 19 | folded | the AI net files' classes (zrdrainet/zrdraiv): the record cache, its edit (the editor's, ported in zrdrainet.cpp 0x004dc2b0), lookups, constructors, destructors and std::map code; the editor's net writer 0x004db8d0 replaced by the port's exact one (net_save) |
| 0x004deb00 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004dee30 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004dee50 | 58 | unused: PORT (preserve) | read: the aiv reader's vt+0x04 (vtable 0x005cd428; the reader base's is 0x004da730, return 1): 0 when a zEntity already has the name (0x005a8210) or the vehicle editor's cache (+0x24, none while playing) holds a record under it, else 1; no caller found (no call through +0x04 near any of the 13 references to the aiv file [0x0060b5e0], no direct call); the port's AivFile has no such slot (zrdraiv.h) |
| 0x004df5c0 | 9 | folded | a thunk: one call to another function |
| 0x004df620 | 9 | folded | a thunk: one call to another function |
| 0x004df640 | 13 | folded | covered by the port where it names it (src/mech3/zrdraiv.h:62) |
| 0x004df670 | 9 | folded | a thunk: one call to another function |
| 0x004df6a0 | 20 | dev dialog (MFC), its logic: PORT (preserve) | the editor's 'add vehicle to node' record (an aiv record built on OK, editor.md); in the release its path returns 0 and nothing is made; ours does it as meant: upgrades/editor/add_vehicle |
| 0x004df6f0 | 12 | folded | covered by the port where it names it (src/mech3/game_editor_vehicles.cpp:589) |
| 0x004df710 | 11 | folded | covered by the port where it names it (src/mech3/game_editor_vehicles.cpp:553) |
| 0x004e001a | 168 | folded | a piece of the ported function before it (Ghidra split it there) (0x004df730) |
| 0x004e0020 | 190 | folded | covered where the port does it: src/mech3/zrdraiv.cpp:265 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004e0082 | 155 | folded | follows 0x004e0020: covered where the port does it: src/mech3/zrdraiv.cpp:265 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004e0540 | 26 | folded | covered by the port where it names it (src/upgrades/editor/add_vehicle.h:11) |
| 0x004e06e0 | 9 | folded | a thunk: one call to another function |
| 0x004e0700 | 8 | folded | returns a constant or a global |
| 0x004e0720 | 9 | folded | a thunk: one call to another function |
| 0x004e0740 | 12 | folded | covered by the port where it names it (src/mech3/zrdraiv.h:96) |
| 0x004e0770 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e07a0 | 9 | folded | a thunk: one call to another function |
| 0x004e07d0 | 9 | folded | a thunk: one call to another function |
| 0x004e07e0 | 17 | folded | covered by the port where it names it (src/mech3/zrdraiv.cpp:504) |
| 0x004e0820 | 9 | folded | a thunk: one call to another function |
| 0x004e0840 | 9 | folded | a thunk: one call to another function |
| 0x004e0870 | 18 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:1172) |
| 0x004e0c20 | 16 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e0c50 | 88 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e0db0 | 59 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e0f20 | 43 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e0fb0 | 317 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e1550 | 32 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e15e0 | 41 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e16a0 | 221 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e1a30 | 56 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e1ac0 | 52 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e1b80 | 48 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e1c30 | 14 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e1c40 | 15 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e1cb0 | 9 | folded | a thunk: one call to another function |
| 0x004e1cd0 | 9 | folded | a thunk: one call to another function |
| 0x004e1d00 | 22 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e1d50 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e1f10 | 95 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e20d0 | 24 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e2190 | 76 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e22e0 | 16 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e2310 | 9 | debug or dev tool: PORT (preserve) | the valve reader's print method (a vtable slot that fprintf's a value); nothing in the game prints a reader |
| 0x004e2fb0 | 15 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3020 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e3070 | 12 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3090 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e30b0 | 15 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3150 | 12 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3180 | 15 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e31f0 | 11 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3230 | 52 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3310 | 13 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3350 | 28 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e33c0 | 27 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3430 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e3450 | 24 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3500 | 27 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3560 | 28 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e35d0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e35f0 | 31 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e36a0 | 27 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3700 | 28 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3770 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e3790 | 24 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3870 | 27 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e38d0 | 28 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3940 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e3960 | 24 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3a30 | 27 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3a90 | 31 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3b20 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e3b40 | 24 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3cc0 | 27 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3d20 | 23 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3d70 | 12 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3da0 | 27 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3e00 | 27 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3ee0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e3f00 | 11 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e3f50 | 26 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e4020 | 27 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e4080 | 22 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e42c0 | 11 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e42e0 | 12 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e4300 | 10 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e4310 | 23 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e4390 | 12 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e43b0 | 17 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e43e0 | 12 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e4400 | 17 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e4430 | 17 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e4460 | 19 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e4490 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e46d1 | 186 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e4d20 | 42 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e4df0 | 42 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e4ec0 | 42 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e4f50 | 26 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e4f90 | 42 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e5060 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e5080 | 49 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e5140 | 30 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e51c0 | 24 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e5240 | 21 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e5290 | 52 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e53b0 | 9 | debug or dev tool: PORT (preserve) | the valve reader's print method (a vtable slot that fprintf's a value); nothing in the game prints a reader |
| 0x004e53d0 | 9 | folded | a thunk: one call to another function |
| 0x004e53f0 | 81 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e55d0 | 10 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e55f0 | 19 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e5620 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e5640 | 59 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e574f | 20 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e57b0 | 317 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e5cf0 | 32 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e5d80 | 48 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e5f30 | 65 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e6110 | 23 | folded | the valve reader's records, actions and lists: constructors, destructors, factories and list helpers of classes ported in zrdrvalve/zai_valve |
| 0x004e6150 | 42 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x004e6220 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e6240 | 12 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e6250 | 14 | folded | ported, by docs/symbols.csv: ported; 0x0c, 0x004e66a0(0) |
| 0x004e63c0 | 10 | folded | ported, by docs/symbols.csv: ported; node +0x2c bits 5 and 6 |
| 0x004e63f0 | 14 | folded | covered by the port where it names it (src/mech3/mech_shade.cpp:100) |
| 0x004e6420 | 15 | folded | ported, by docs/symbols.csv: ported; from the centroid's world position |
| 0x004e66f0 | 12 | folded | ported, by docs/symbols.csv: ported in shade_before |
| 0x004e6710 | 11 | folded | ported, by docs/symbols.csv: ported in shade_after |
| 0x004e6960 | 17 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e6990 | 9 | folded | a thunk: one call to another function |
| 0x004e6a20 | 9 | folded | a thunk: one call to another function |
| 0x004e6a30 | 9 | folded | a thunk: one call to another function |
| 0x004e6a60 | 15 | folded | covered by the port where it names it (src/mech3/sound_extra.cpp:32) |
| 0x004e6aa0 | 27 | folded | covered by the port where it names it (src/mech3/sound_extra.cpp:32) |
| 0x004e6dc0 | 75 | folded | covered where the port does it: src/mech3/app_mech.h:142 (read: a destructor, a container insert or assign, or the work beside the function that comment names) |
| 0x004e6ee0 | 51 | folded | ported, by docs/symbols.csv: ported: lance_orders contact_on |
| 0x004e6ff0 | 22 | folded | covered: the AI states' constructors and destructors, the deploy listener, the brain's IControls cast (0x004d1e60, its vtable slots ported as BrainHooks), the animation root's move (zai_target), the valves' and owners' lists, the nets' begin and end (app_mission.cpp:697, app_vehicle.cpp:743; player.h's 'not ported' note is stale), saved_or_ally (app_hud.cpp:685), the entity lookup by name, the sensors' sort, list instances |
| 0x004e7010 | 22 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:1036) |
| 0x004e7090 | 41 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e7290 | 20 | folded | AI support code ported or folded: start-up cosines (the port's constants), the params' vlist copies, squares and destructors (zai_params), the brain's destructor chain, the listeners' constructor and destructor, the steps' states |
| 0x004e7660 | 9 | folded | a thunk: one call to another function |
| 0x004e7730 | 9 | folded | a thunk: one call to another function |
| 0x004e7750 | 9 | folded | a thunk: one call to another function |
| 0x004e7760 | 9 | folded | a thunk: one call to another function |
| 0x004e7770 | 9 | folded | a thunk: one call to another function |
| 0x004e7780 | 9 | folded | a thunk: one call to another function |
| 0x004e7790 | 9 | folded | a thunk: one call to another function |
| 0x004e77a0 | 9 | unused: PORT (preserve) | follows 0x004e95b0: IControls vt+0x84 of the AI controls (drop the target if it is the given entity's, stand the firing down): no caller reaches that slot (docs/ai.md, the AI controls' unreached slots) |
| 0x004e77b0 | 9 | folded | a thunk: one call to another function |
| 0x004e77c0 | 9 | folded | a thunk: one call to another function |
| 0x004e77d0 | 9 | folded | a thunk: one call to another function |
| 0x004e77e0 | 9 | folded | a thunk: one call to another function |
| 0x004e77f0 | 9 | folded | a thunk: one call to another function |
| 0x004e7800 | 9 | folded | a thunk: one call to another function |
| 0x004e7810 | 9 | folded | a thunk: one call to another function |
| 0x004e78d0 | 66 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e79b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e79d0 | 57 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e7b00 | 17 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e7b40 | 17 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e7b80 | 17 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e7bc0 | 17 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e7c00 | 13 | folded | a thunk: one virtual call |
| 0x004e7c10 | 13 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e7c20 | 13 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e7c30 | 13 | folded | a thunk: one virtual call |
| 0x004e7d80 | 26 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e7dc0 | 12 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e7df0 | 17 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e7f30 | 9 | folded | a thunk: one call to another function |
| 0x004e7f40 | 9 | folded | a thunk: one call to another function |
| 0x004e7f80 | 19 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e7fd0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e8030 | 12 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e8050 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004e8070 | 12 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e8090 | 13 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e8180 | 12 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e8270 | 18 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e82a0 | 23 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e8da0 | 9 | folded | a thunk: one call to another function |
| 0x004e8dc0 | 10 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004e9590 | 9 | folded | a thunk: one call to another function |
| 0x004e95b0 | 48 | unused: PORT (preserve) | IControls vt+0x84 of the AI controls (drop the target if it is the given entity's, stand the firing down): no caller reaches that slot (docs/ai.md, the AI controls' unreached slots) |
| 0x004e9d80 | 9 | folded | a thunk: one call to another function |
| 0x004e9fb0 | 9 | folded | a thunk: one call to another function |
| 0x004e9fd0 | 20 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea040 | 13 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea070 | 13 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea0b0 | 13 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea0f0 | 13 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea120 | 13 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea150 | 13 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea180 | 13 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea1c0 | 13 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea200 | 13 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea230 | 13 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea4a0 | 9 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea700 | 20 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea740 | 9 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea750 | 9 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea760 | 24 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea7e0 | 14 | folded | sets members (a setter or a small constructor) |
| 0x004ea7f0 | 32 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea870 | 32 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea8e0 | 16 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea910 | 32 | folded | the AI's support code, ported or folded: the mission listeners' slot calls (the program's overrides, app_mission.cpp:1696..1730), the radio's lines and table, the orders' deque, constructors and destructors, the lance orders' wrappers (zai_brain.cpp:1208..1230), the AI switch |
| 0x004ea9b0 | 23 | replaced (SDL or the platform layer) | the start's video device picker: an MFC CDialog with a list box (0x004eaab0 and its constructor, destructor, data exchange and init); the SDL GPU device replaces it |
| 0x004eaa10 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x004eaa30 | 21 | replaced (SDL or the platform layer) | the start's video device picker: an MFC CDialog with a list box (0x004eaab0 and its constructor, destructor, data exchange and init); the SDL GPU device replaces it |
| 0x004eaa80 | 9 | replaced (SDL or the platform layer) | the start's video device picker: an MFC CDialog with a list box (0x004eaab0 and its constructor, destructor, data exchange and init); the SDL GPU device replaces it |
| 0x004eaaa0 | 8 | folded | returns a constant or a global |
| 0x004eaab0 | 63 | replaced (SDL or the platform layer) | the start's video device picker: an MFC CDialog with a list box (0x004eaab0 and its constructor, destructor, data exchange and init); the SDL GPU device replaces it |
| 0x004eaba0 | 44 | replaced (SDL or the platform layer) | the start's video device picker: an MFC CDialog with a list box (0x004eaab0 and its constructor, destructor, data exchange and init); the SDL GPU device replaces it |

### zmodel (143 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005269a0 | 16 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:123) |
| 0x005269d0 | 8 | folded | returns a constant or a global |
| 0x005269e0 | 13 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x00526a00 | 11 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.h:325) |
| 0x00526df0 | 14 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.h:326) |
| 0x005273f0 | 100 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x005277d0 | 126 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x00527d20 | 126 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x005283c0 | 148 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x00529790 | 261 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x0052a250 | 362 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x0052c8d0 | 362 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x00530043 | 521 | folded | a piece of the ported function before it (Ghidra split it there) (0x0052fab0) |
| 0x00530d90 | 832 | unused: PORT (preserve) | large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x00531d90 | 963 | unused: PORT (preserve) | large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x005331e0 | 1175 | unused: PORT (preserve) | large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x00534745 | 8 | unused: PORT (preserve) | follows 0x005331e0: large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x00535c10 | 95 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x00535ed0 | 12 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x00535ef0 | 10 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x00535f00 | 8 | folded | returns a constant or a global |
| 0x00535f10 | 8 | folded | returns a constant or a global |
| 0x00536830 | 80 | folded | covered: zModel's reset of its materials (module-starts.md: the port's initialisers), small helpers and list initialisers beside ported code |
| 0x00536a60 | 8 | folded | empty: does nothing |
| 0x005372c0 | 54 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x005374b0 | 15 | unused: PORT (preserve) | large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x005374d0 | 21 | unused: PORT (preserve) | large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x00537520 | 31 | replaced (SDL or the platform layer) | the used textures' system surfaces released after the launch (DirectDraw; the port's record is one Render3D handle: app_engine.cpp no effect) |
| 0x00538050 | 10 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_const.cpp:601) |
| 0x00538060 | 11 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_const.cpp:602) |
| 0x00538080 | 11 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.h:331) |
| 0x00539140 | 8 | folded | returns a constant or a global |
| 0x00539c72 | 144 | folded | a piece of the ported function before it (Ghidra split it there) (0x005396c0) |
| 0x00539d50 | 11 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:470) |
| 0x00539d60 | 11 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:467) |
| 0x00539d70 | 10 | folded | a getter of a member |
| 0x0053a1a0 | 117 | unused: PORT (preserve) | large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0053a740 | 96 | unused: PORT (preserve) | large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0053b050 | 179 | unused: PORT (preserve) | large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0053bb90 | 23 | unused: PORT (preserve) | large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0053bbd0 | 23 | unused: PORT (preserve) | large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0053ce20 | 17 | folded | covered by the port where it names it (src/gamez/ztwod/twod_host.h:131) |
| 0x0053cf00 | 10 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.h:329) |
| 0x0053d130 | 107 | unused: PORT (preserve) | large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0053d360 | 119 | unused: PORT (preserve) | large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0053d560 | 56 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x0053d6f0 | 21 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x0053d750 | 165 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x0053de50 | 45 | unused: PORT (preserve) | large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0053df00 | 66 | unused: PORT (preserve) | large zModel functions nothing calls or tables (no static reference anywhere): clippers and matrix work beside the software path, material flag setters over a model's materials, light helpers |
| 0x0053e050 | 754 | software renderer | the software model draw (0x0053e050, gmod_init.h: [0x0074a2e0] in software), its lighting (0x005465c0 with 0x00544060) and its third fog routine (0x005557f0, render-fogsort.md) |
| 0x00540082 | 1125 | folded | a piece of the ported function before it (Ghidra split it there) (0x0053f2e0) |
| 0x00544060 | 647 | software renderer | the software model draw's projection and clippers, its own lighting, the span buffer's pick and hidden-surface test, the small polygon reject, palette shading (software-path.md, render-clip3d.md, render-lighting.md: the software renderer, not ported by decision) |
| 0x00545245 | 408 | folded | a piece of the ported function before it (Ghidra split it there) (0x00544ed0) |
| 0x005465c0 | 462 | software renderer | the software model draw (0x0053e050, gmod_init.h: [0x0074a2e0] in software), its lighting (0x005465c0 with 0x00544060) and its third fog routine (0x005557f0, render-fogsort.md) |
| 0x00548840 | 52 | unused: PORT (preserve) | the puffers' wind step with a dt argument: dead code, never called (puffers.md) |
| 0x00548d90 | 17 | folded | covered: zModel's reset of its materials (module-starts.md: the port's initialisers), small helpers and list initialisers beside ported code |
| 0x00548e60 | 17 | folded | covered: zModel's reset of its materials (module-starts.md: the port's initialisers), small helpers and list initialisers beside ported code |
| 0x0054a078 | 10 | folded | a piece of the ported function before it (Ghidra split it there) (0x005496c0) |
| 0x0054a479 | 290 | folded | a piece of the ported function before it (Ghidra split it there) (0x0054a090) |
| 0x0054ab90 | 14 | folded | sets members (a setter or a small constructor) |
| 0x0054abb0 | 14 | folded | sets members (a setter or a small constructor) |
| 0x0054abd0 | 14 | folded | sets members (a setter or a small constructor) |
| 0x0054abf0 | 14 | folded | sets members (a setter or a small constructor) |
| 0x0054ac10 | 14 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_puff.cpp:88) |
| 0x0054ac80 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0054ac90 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0054aca0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0054acb0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0054acc0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0054acd0 | 11 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_puff.cpp:103) |
| 0x0054ad30 | 18 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_puff.cpp:112) |
| 0x0054ad90 | 11 | folded | sets members (a setter or a small constructor) |
| 0x0054ada0 | 12 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_puff.cpp:125) |
| 0x0054afc0 | 85 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x0054b1d0 | 109 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x0054b460 | 87 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x0054b650 | 9 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x0054b660 | 11 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x0054b670 | 11 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x0054b680 | 22 | folded | covered: zModel's reset of its materials (module-starts.md: the port's initialisers), small helpers and list initialisers beside ported code |
| 0x0054b6f0 | 22 | folded | covered: zModel's reset of its materials (module-starts.md: the port's initialisers), small helpers and list initialisers beside ported code |
| 0x0054b750 | 138 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x0054b990 | 107 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x0054bb00 | 13 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x0054bb30 | 28 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x0054bb70 | 147 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x0054bde0 | 15 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x0054be10 | 24 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00550005 | 8 | folded | a piece of the ported function before it (Ghidra split it there) (0x0054ff40) |
| 0x00550120 | 22 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00553470 | 13 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00553560 | 12 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x005535b0 | 17 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00553d40 | 13 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00554130 | 12 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00554420 | 19 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x005544d0 | 28 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00554540 | 12 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00554560 | 22 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00554630 | 17 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00554660 | 33 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x005546c0 | 16 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x005546f0 | 60 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00554810 | 316 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00554d40 | 58 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00554ea0 | 27 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00554f30 | 12 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00554f50 | 31 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00554fe0 | 41 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00555090 | 199 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x005553b0 | 48 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00555460 | 38 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x005554e0 | 52 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00555500 | 45 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x005555a0 | 11 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x005555e0 | 11 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x00555620 | 16 | folded | the hardware mode's triangle stripper's small helpers and its std::map / std::list code, folded into zmod::strip_model (gmod_strip.cpp): edge and record copies and compares, the UV shift, the zone compare, the map and lists as std::map / std::list in the same order |
| 0x005557f0 | 147 | software renderer | the software model draw (0x0053e050, gmod_init.h: [0x0074a2e0] in software), its lighting (0x005465c0 with 0x00544060) and its third fog routine (0x005557f0, render-fogsort.md) |
| 0x00555cd0 | 19 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00555d00 | 13 | folded | read: the static constructor of an STL list's shared head (malloc(0xc), 0x00555d20, the instance count up), zreader's; the port's containers |
| 0x00555d20 | 29 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00555d90 | 19 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00555de0 | 34 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00555e50 | 16 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00555e70 | 18 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00555ea0 | 15 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00555ec0 | 28 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_play.cpp:100) |
| 0x00555f10 | 13 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00555f30 | 26 | folded | covered by the port where it names it (src/gamez/zeffect/zeff_init.cpp:229) |
| 0x00555f70 | 36 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00555fe0 | 16 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00556000 | 27 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00556030 | 26 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00556070 | 30 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x005560d0 | 25 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00556120 | 27 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00556150 | 9 | folded | follows 0x005560d0: covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00556160 | 13 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x005561a0 | 14 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x005561e0 | 13 | folded | covered: the puffers' setters and wind (gmod_puff.cpp:136..157, world-params.md), their vector instances, the materials' free with the models (app_mission.cpp:1868), the reader's link pool (the port's C++ containers; startup.md) and RdrSetPath / RdrAddPath (zinterp.cpp:128, the mounts) |
| 0x00556240 | 38 | folded | covered by the port where it names it (src/gamez/zreader/zreader.cpp:918) |
| 0x00557090 | 10 | folded | covered by the port where it names it (src/mech3/veh_basic_object.h:137) |

### znetwork (141 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00587480 | 51 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00587580 | 84 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00587680 | 56 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005876f0 | 23 | folded | covered by the port where it names it (src/gamez/znetwork/znet_session.cpp:79) |
| 0x00587740 | 16 | folded | covered by the port where it names it (src/gamez/znetwork/znet_dplay.cpp:157) |
| 0x00587760 | 13 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00587780 | 84 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00587950 | 17 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00587980 | 9 | folded | a thunk: one call to another function |
| 0x00587ca0 | 18 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00587ce0 | 9 | folded | a thunk: one call to another function |
| 0x00587d80 | 9 | folded | a thunk: one call to another function |
| 0x00587d90 | 9 | folded | a thunk: one call to another function |
| 0x00587db0 | 25 | folded | covered by the port where it names it (src/gamez/znetwork/znet_session.cpp:248) |
| 0x00587e30 | 34 | folded | covered by the port where it names it (src/gamez/znetwork/znet_session.cpp:248) |
| 0x00587ed0 | 42 | folded | covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x00587fb0 | 80 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00588070 | 46 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00588130 | 21 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00588180 | 24 | folded | covered by the port where it names it (src/gamez/znetwork/znet_dplay.h:10) |
| 0x00588600 | 22 | folded | covered by the port where it names it (src/mech3/mp_connect.cpp:350) |
| 0x00589540 | 21 | folded | covered by the port where it names it (src/gamez/znetwork/znet_dplay.h:10) |
| 0x005895b0 | 15 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x005895c0 | 19 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00589600 | 47 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00589870 | 16 | folded | covered by the port where it names it (src/gamez/znetwork/znet_dplay.cpp:336) |
| 0x005898a0 | 8 | folded | returns a constant or a global |
| 0x00589960 | 14 | folded | covered by the port where it names it (src/gamez/znetwork/znet_dplay.h:40) |
| 0x005899b0 | 43 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00589a50 | 80 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00589c00 | 11 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00589c20 | 16 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00589c50 | 19 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00589ca0 | 15 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00589ce0 | 13 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00589d00 | 57 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00589df0 | 16 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00589e30 | 25 | folded | covered by the port where it names it (src/mech3/mp_lobby.cpp:542) |
| 0x00589e80 | 16 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00589ea0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00589ec0 | 16 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x00589ef0 | 10 | folded | a getter of a member |
| 0x00589f00 | 23 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00589f50 | 16 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x00589f70 | 47 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a020 | 33 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a090 | 19 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a0d0 | 12 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a0e0 | 13 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a110 | 63 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a240 | 15 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a270 | 20 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a2b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0058a2d0 | 25 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a340 | 29 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a3d0 | 13 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a400 | 25 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a450 | 12 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a470 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0058a4a0 | 25 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a510 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0058a530 | 29 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a5b0 | 26 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a5f0 | 31 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a6f0 | 36 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a7b0 | 15 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a7d0 | 8 | folded | returns a constant or a global |
| 0x0058a7f0 | 20 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a830 | 20 | replaced (SDL or the platform layer) | DirectPlay: the sessions, players and groups lists, the providers and connections (IDirectPlay4A), the lobby launch (0x005899b0 / 0x00589a50) and a game service's score report that does not exist (0x00589d00..0x00589e30): the port's ENet transport and its own session and player lists replace them (multiplayer-port.md) |
| 0x0058a870 | 8 | folded | empty: does nothing |
| 0x0058a880 | 15 | folded | covered by the port where it names it (src/gamez/znetwork/znet_dplay.cpp:272) |
| 0x0058a8c0 | 335 | folded | covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x0058ae90 | 20 | folded | covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x0058afe0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0058b0a0 | 53 | folded | ported, by docs/symbols.csv: ported (the dock's net_element): the first element of that name (vt+0x24) |
| 0x0058b220 | 26 | folded | covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x0058b310 | 30 | folded | covered by the port where it names it (src/gamez/znetwork/znet_object.cpp:15) |
| 0x0058b380 | 8 | folded | returns a constant or a global |
| 0x0058b390 | 14 | folded | covered by the port where it names it (src/gamez/znetwork/znet_object.h:8) |
| 0x0058b3c0 | 18 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:13) |
| 0x0058b510 | 14 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:528) |
| 0x0058b530 | 9 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:507) |
| 0x0058c570 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0058c590 | 35 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058c620 | 11 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x0058c630 | 12 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x0058ca30 | 24 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:53) |
| 0x0058ce10 | 74 | folded | covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x0058d350 | 26 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:112) |
| 0x0058d3c0 | 97 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058d9a0 | 23 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:31) |
| 0x0058de40 | 93 | folded | covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x0058e3b0 | 29 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:58) |
| 0x0058e420 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0058e440 | 14 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058e460 | 22 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058e770 | 18 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:141) |
| 0x0058e7b0 | 44 | folded | covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x0058e850 | 36 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:159) |
| 0x0058e8e0 | 19 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:180) |
| 0x0058e920 | 35 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:261) |
| 0x0058e990 | 31 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:267) |
| 0x0058ea10 | 9 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:253) |
| 0x0058ea20 | 15 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:257) |
| 0x0058ea40 | 12 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:173) |
| 0x0058ea60 | 9 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:167) |
| 0x0058ea70 | 9 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.cpp:175) |
| 0x0058ec20 | 27 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058ec90 | 20 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058ecd0 | 13 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058ed00 | 20 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058ed40 | 31 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058eda0 | 21 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058ee10 | 19 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058ee40 | 135 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f170 | 55 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f270 | 11 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f2a0 | 19 | folded | covered by the port where it names it (src/mech3/mech_fieldbase_dock.cpp:292) |
| 0x0058f2d0 | 14 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f2e0 | 79 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f420 | 76 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f580 | 28 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f610 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0058f630 | 34 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f6a0 | 12 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f740 | 11 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f760 | 11 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:50) |
| 0x0058f770 | 11 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f780 | 11 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f790 | 11 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f7a0 | 11 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f7b0 | 11 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f7c0 | 11 | folded | covered by the port where it names it (src/gamez/znetwork/znet_view.h:50) |
| 0x0058f8b0 | 8 | debug or dev tool: PORT (preserve) | the AI net editor's net view (znet_view.h: only the AI net editor makes one): its pieces the port's editor does not use (element and link drawing helpers, picking, its lists) |
| 0x0058f8c0 | 8 | folded | returns a constant or a global |
| 0x0058f8d0 | 58 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x0058fa20 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0058fa40 | 19 | folded | covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x0058faa0 | 13 | folded | covered by the port where it names it (src/gamez/zgame/zgameapp.cpp:59) |
| 0x0058fac0 | 72 | folded | covered: the DirectPlay error names (znet_dplay.cpp:142 names each through localize), the sessions' list clear (znet_session.cpp:248), the net view's inlined steps (znet_view.cpp:68, :151), the net object's static, ZGameApp's base InitInstance and engine start (overridden by the app's 0x00480000 / 0x00480570, game-loop.md) |
| 0x0058fcd0 | 39 | folded | covered by the port where it names it (src/gamez/zgame/zgameapp.h:59) |

### zinput (85 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x0051af60 | 9 | folded | follows 0x0051b020: the input module's static object's constructor (zeroes its lists); the port's members start empty |
| 0x0051af70 | 9 | folded | a thunk: one call to another function |
| 0x0051af90 | 51 | unused: PORT (preserve) | the input module's static object's list walk, whose caller 0x0051af70 nothing reaches (input.md:567) |
| 0x0051b020 | 16 | folded | the input module's static object's constructor (zeroes its lists); the port's members start empty |
| 0x0051b1e0 | 8 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:71) |
| 0x0051b1f0 | 8 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:71) |
| 0x0051b200 | 12 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:88) |
| 0x0051b220 | 12 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:88) |
| 0x0051b240 | 12 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:88) |
| 0x0051b260 | 9 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:82) |
| 0x0051b270 | 9 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:81) |
| 0x0051b280 | 9 | folded | covered by the port where it names it (src/gamez/zinput/zin_init.cpp:83) |
| 0x0051bf20 | 10 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:136) |
| 0x0051bf40 | 10 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:136) |
| 0x0051bf60 | 10 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:136) |
| 0x0051bf90 | 10 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:148) |
| 0x0051bfc0 | 10 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:148) |
| 0x0051bfd0 | 10 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:148) |
| 0x0051c440 | 11 | folded | a thunk: one call to another function |
| 0x0051d130 | 18 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:397) |
| 0x0051d160 | 18 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:396) |
| 0x0051d190 | 18 | folded | covered by the port where it names it (src/gamez/zinput/zin_cmdmap.cpp:395) |
| 0x0051d1d0 | 11 | folded | a thunk: one call to another function |
| 0x0051d1f0 | 11 | folded | a thunk: one call to another function |
| 0x0051d200 | 11 | folded | a thunk: one call to another function |
| 0x0051d210 | 11 | folded | a thunk: one call to another function |
| 0x0051d220 | 11 | folded | a thunk: one call to another function |
| 0x0051d230 | 11 | folded | a thunk: one call to another function |
| 0x0051d240 | 11 | folded | a thunk: one call to another function |
| 0x0051d250 | 11 | folded | a thunk: one call to another function |
| 0x0051d260 | 11 | folded | a thunk: one call to another function |
| 0x0051d270 | 11 | folded | a thunk: one call to another function |
| 0x0051d280 | 12 | folded | a thunk: one call to another function |
| 0x0051d290 | 12 | folded | a thunk: one call to another function |
| 0x0051d2a0 | 12 | folded | a thunk: one call to another function |
| 0x0051d2b0 | 12 | folded | a thunk: one call to another function |
| 0x0051d2c0 | 12 | folded | a thunk: one call to another function |
| 0x0051d2f0 | 12 | folded | a thunk: one call to another function |
| 0x0051d300 | 11 | folded | a thunk: one call to another function |
| 0x0051d310 | 12 | folded | a thunk: one call to another function |
| 0x0051d330 | 12 | folded | a thunk: one call to another function |
| 0x0051d350 | 12 | folded | a thunk: one call to another function |
| 0x0051d370 | 12 | folded | a thunk: one call to another function |
| 0x0051daa0 | 16 | replaced (SDL or the platform layer) | DirectInput joystick properties (range and dead zone per axis); the port sets the dead zones through the platform (zin_joy.cpp:60) |
| 0x0051dae0 | 18 | replaced (SDL or the platform layer) | DirectInput joystick properties (range and dead zone per axis); the port sets the dead zones through the platform (zin_joy.cpp:60) |
| 0x0051db30 | 21 | folded | covered by the port where it names it (src/gamez/zinput/zin_joy.cpp:47) |
| 0x0051db80 | 21 | folded | covered by the port where it names it (src/gamez/zinput/zin_joy.cpp:46) |
| 0x0051dda0 | 8 | folded | returns a constant or a global |
| 0x0051e060 | 13 | folded | covered by the port where it names it (src/mech3/zai_repair.cpp:196) |
| 0x0051e0a0 | 13 | folded | ported, by docs/symbols.csv: ported inline (zai_node flat_distance) |
| 0x0051e160 | 29 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x0051e1c0 | 17 | folded | the 2D normalise, inline in its ported caller (0x00524247) |
| 0x0051e380 | 16 | folded | covered by the port where it names it (src/mech3/mech_net.cpp:596) |
| 0x0051f390 | 136 | MISSING LOGIC (only data the shipped game lacks reaches it) | billboard modes 2 (about x) and 3 (about z): no shipped world uses them; they draw as default models (gmod_draw.cpp:759, render-modeldraw.md:27) |
| 0x0051f750 | 140 | MISSING LOGIC (only data the shipped game lacks reaches it) | billboard modes 2 (about x) and 3 (about z): no shipped world uses them; they draw as default models (gmod_draw.cpp:759, render-modeldraw.md:27) |
| 0x00520082 | 83 | folded | a piece of the ported function before it (Ghidra split it there) (0x0051fee0) |
| 0x00520c60 | 154 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x00521080 | 169 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x005214f0 | 67 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x00521a90 | 61 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x00521e10 | 89 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x00522210 | 30 | folded | covered by the port where it names it (src/gamez/zeffect/zeff_anim_events.cpp:373) |
| 0x00522280 | 49 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x005223c0 | 17 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x00522460 | 16 | folded | covered by the port where it names it (src/mech3/player_view.cpp:107) |
| 0x005224b0 | 50 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x00523650 | 118 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x00523df0 | 32 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x00523ef0 | 33 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x005241b7 | 35 | folded | a piece of the ported function before it (Ghidra split it there) (0x00524020) |
| 0x00524477 | 35 | folded | a piece of the ported function before it (Ghidra split it there) (0x00524020) |
| 0x00524544 | 30 | folded | a piece of the ported function before it (Ghidra split it there) (0x00524020) |
| 0x00524554 | 32 | folded | a piece of the ported function before it (Ghidra split it there) (0x00524020) |
| 0x005245c7 | 85 | folded | a piece of the ported function before it (Ghidra split it there) (0x00524020) |
| 0x00524967 | 97 | folded | a piece of the ported function before it (Ghidra split it there) (0x00524020) |
| 0x00524f80 | 23 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x00524fd0 | 19 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x00525020 | 23 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x005250a0 | 12 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x005250f0 | 11 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x005251a0 | 28 | folded | covered by the port where it names it (src/gamez/zmath/zmth_vector.cpp:286) |
| 0x00525fa0 | 156 | unused: PORT (preserve) | zMath routines nothing calls: vector and quaternion add, subtract, negate and dot, single-axis rotations, a plane from three points, 2D side tests, cameras from euler angles and a position, the projection as a matrix, screen-space box extents, a point-set fit (render-matrix.md:183-193) |
| 0x00526510 | 81 | folded | zModel's start, stop and per-mission free: the render state's defaults (gmod_draw.h:37), the pools freed at the app's stop (memory only, app_engine.cpp:325) and the mission's (models_free, app_mission.cpp:1868) |
| 0x00526780 | 25 | folded | zModel's start, stop and per-mission free: the render state's defaults (gmod_draw.h:37), the pools freed at the app's stop (memory only, app_engine.cpp:325) and the mission's (models_free, app_mission.cpp:1868) |
| 0x00526920 | 25 | folded | zModel's start, stop and per-mission free: the render state's defaults (gmod_draw.h:37), the pools freed at the app's stop (memory only, app_engine.cpp:325) and the mission's (models_free, app_mission.cpp:1868) |

### zsound (78 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x0056f460 | 16 | folded | zSound's static containers and their cleanup (vector and list heads made at start-up, the sets' pointer list freed); the port's members construct and free themselves |
| 0x0056f480 | 9 | folded | a thunk: one call to another function |
| 0x0056f4d0 | 16 | folded | zSound's static containers and their cleanup (vector and list heads made at start-up, the sets' pointer list freed); the port's members construct and free themselves |
| 0x0056f4f0 | 9 | folded | a thunk: one call to another function |
| 0x0056f570 | 19 | folded | zSound's static containers and their cleanup (vector and list heads made at start-up, the sets' pointer list freed); the port's members construct and free themselves |
| 0x0056f760 | 98 | folded | covered where named: the sound set's build (zsnd_init.cpp:260, 265), the archive load (zsnd_parm.cpp:63), 3D placement (zsnd_3d.cpp:138), the group chain step (zsnd_grp.cpp:170) |
| 0x0056f990 | 18 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_parm.cpp:150) |
| 0x0056f9d0 | 89 | folded | covered where named: the sound set's build (zsnd_init.cpp:260, 265), the archive load (zsnd_parm.cpp:63), 3D placement (zsnd_3d.cpp:138), the group chain step (zsnd_grp.cpp:170) |
| 0x0056fbd0 | 11 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_init.cpp:265) |
| 0x0056ffc0 | 52 | folded | covered where named: the sound set's build (zsnd_init.cpp:260, 265), the archive load (zsnd_parm.cpp:63), 3D placement (zsnd_3d.cpp:138), the group chain step (zsnd_grp.cpp:170) |
| 0x00570068 | 43 | folded | follows 0x0056ffc0: covered where named: the sound set's build (zsnd_init.cpp:260, 265), the archive load (zsnd_parm.cpp:63), 3D placement (zsnd_3d.cpp:138), the group chain step (zsnd_grp.cpp:170) |
| 0x005700a0 | 26 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_parm.cpp:150) |
| 0x005706f0 | 211 | unused: PORT (preserve) | sounds.zrd's SYNTAX 1 reader: no shipped or community data uses syntax 1 (symbols.csv, sound.md:41) |
| 0x00571380 | 39 | unused: PORT (preserve) | the Aureal A3D path: its init (CoCreateInstance), 3D placement and error names; only 0x570625 writes the selector [0x72c2cc], and only to 0 (DirectSound) when A3D fails, so A3D never runs (sound.md:38) |
| 0x005715c0 | 9 | folded | a thunk: one call to another function |
| 0x005716d0 | 22 | folded | zSound's static containers and their cleanup (vector and list heads made at start-up, the sets' pointer list freed); the port's members construct and free themselves |
| 0x00571730 | 9 | folded | a thunk: one call to another function |
| 0x00571810 | 20 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_3d.cpp:27) |
| 0x00571830 | 35 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_3d.cpp:27) |
| 0x005718f0 | 32 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_3d.cpp:95) |
| 0x00571940 | 77 | folded | ported, by docs/symbols.csv: ported: 2500/s, ends only on an exact landing |
| 0x00571f70 | 48 | unused: PORT (preserve) | the Aureal A3D path: its init (CoCreateInstance), 3D placement and error names; only 0x570625 writes the selector [0x72c2cc], and only to 0 (DirectSound) when A3D fails, so A3D never runs (sound.md:38) |
| 0x00572050 | 113 | folded | covered where named: the sound set's build (zsnd_init.cpp:260, 265), the archive load (zsnd_parm.cpp:63), 3D placement (zsnd_3d.cpp:138), the group chain step (zsnd_grp.cpp:170) |
| 0x005723a0 | 17 | folded | zSound's static containers and their cleanup (vector and list heads made at start-up, the sets' pointer list freed); the port's members construct and free themselves |
| 0x005723d0 | 9 | folded | a thunk: one call to another function |
| 0x005728b0 | 19 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_cd.cpp:24) |
| 0x00572ec0 | 14 | replaced (SDL or the platform layer) | the CD music volume through the Windows mixer and aux API (sound.md:264: the music output's own volume stands in) |
| 0x00572ee0 | 40 | replaced (SDL or the platform layer) | the CD music volume through the Windows mixer and aux API (sound.md:264: the music output's own volume stands in) |
| 0x00573040 | 8 | folded | covered by the port where it names it (src/mech3/app_hooks.h:508) |
| 0x00573060 | 40 | replaced (SDL or the platform layer) | the CD music volume through the Windows mixer and aux API (sound.md:264: the music output's own volume stands in) |
| 0x00573140 | 18 | replaced (SDL or the platform layer) | the CD music volume through the Windows mixer and aux API (sound.md:264: the music output's own volume stands in) |
| 0x00573170 | 29 | replaced (SDL or the platform layer) | the CD music volume through the Windows mixer and aux API (sound.md:264: the music output's own volume stands in) |
| 0x005731f0 | 27 | replaced (SDL or the platform layer) | the CD music volume through the Windows mixer and aux API (sound.md:264: the music output's own volume stands in) |
| 0x005739f0 | 203 | folded | ported, by docs/symbols.csv: ported |
| 0x00573cc0 | 50 | folded | ported, by docs/symbols.csv: ported: group state machine |
| 0x00573da0 | 30 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_grp.cpp:141) |
| 0x00573e00 | 87 | folded | ported, by docs/symbols.csv: ported: weighted rand pick, dynamic weights |
| 0x00573f90 | 96 | folded | covered where named: the sound set's build (zsnd_init.cpp:260, 265), the archive load (zsnd_parm.cpp:63), 3D placement (zsnd_3d.cpp:138), the group chain step (zsnd_grp.cpp:170) |
| 0x00574120 | 23 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_grp.cpp:162) |
| 0x00574159 | 8 | folded | follows 0x00574120: covered by the port where it names it (src/gamez/zsound/zsnd_grp.cpp:162) |
| 0x00574170 | 24 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_grp.cpp:216) |
| 0x005741c0 | 17 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_grp.cpp:224) |
| 0x005741f0 | 28 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_grp.cpp:204) |
| 0x00574230 | 23 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_grp.cpp:46) |
| 0x00574270 | 15 | folded | covered by the port where it names it (src/gamez/zsound/zsnd.h:317) |
| 0x005743f0 | 16 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_play.cpp:264) |
| 0x00574420 | 11 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_grp.cpp:257) |
| 0x00574450 | 9 | folded | a thunk: one call to another function |
| 0x00574590 | 27 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_grp.cpp:247) |
| 0x005745f0 | 11 | folded | a thunk: one call to another function |
| 0x00574660 | 13 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_cd.cpp:32) |
| 0x005746b0 | 198 | unused: PORT (preserve) | the Aureal A3D path: its init (CoCreateInstance), 3D placement and error names; only 0x570625 writes the selector [0x72c2cc], and only to 0 (DirectSound) when A3D fails, so A3D never runs (sound.md:38) |
| 0x00574c90 | 17 | folded | zSound's static containers and their cleanup (vector and list heads made at start-up, the sets' pointer list freed); the port's members construct and free themselves |
| 0x00574cc0 | 9 | folded | a thunk: one call to another function |
| 0x00574d40 | 24 | folded | covered by the port where it names it (src/gamez/zsound/zsnd.h:336) |
| 0x00574d90 | 14 | replaced (SDL or the platform layer) | DirectSound buffers: made from a WAV (0x00574ff0) and released (0x00574d90, 0x00574f10); SDL audio holds the port's |
| 0x00574f10 | 23 | replaced (SDL or the platform layer) | DirectSound buffers: made from a WAV (0x00574ff0) and released (0x00574d90, 0x00574f10); SDL audio holds the port's |
| 0x00574fd0 | 15 | folded | covered by the port where it names it (src/gamez/zsound/zsnd.h:326) |
| 0x00574ff0 | 101 | replaced (SDL or the platform layer) | DirectSound buffers: made from a WAV (0x00574ff0) and released (0x00574d90, 0x00574f10); SDL audio holds the port's |
| 0x00575b00 | 225 | debug or dev tool: PORT (preserve) | the developer's stdout redirect (freopen to a log; editor.md:214) |
| 0x00575d00 | 11 | replaced (SDL or the platform layer) | Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00575d10 | 8 | replaced (SDL or the platform layer) | Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00575d40 | 63 | replaced (SDL or the platform layer) | Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00575e30 | 39 | replaced (SDL or the platform layer) | Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00575f30 | 17 | replaced (SDL or the platform layer) | Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00575f70 | 12 | replaced (SDL or the platform layer) | Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00575fd0 | 9 | replaced (SDL or the platform layer) | follows 0x00575ff0: Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00575ff0 | 15 | replaced (SDL or the platform layer) | Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00576040 | 21 | replaced (SDL or the platform layer) | Win32 plumbing the port replaces: the MCI notify window's flag (app_engine.h:541) and its reader, the CD-ROM drive search (FIX (A) and (trap): fixes.md:56, 130), the Winsock 2 check (FIX (A), mp_connect.cpp:280), Mech3Msg.dll's LoadLibrary, FreeLibrary and FormatMessage (the port reads the DLL as a file: fixes.md:259, 281; sdl_twod_host.cpp:435) |
| 0x00576080 | 12 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_parm.cpp:222) |
| 0x005760b0 | 19 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_create.cpp:129) |
| 0x00576100 | 12 | folded | zUtil's search path lists: each user keeps its own path string (zsnd_init.cpp:223, mech_fieldbase.h:295, zinterp.cpp:240) and the file finder is the platform's (zimg_library.h:75, zsnd_parm.cpp:17, FIX (A)); the default list's free has nothing to free (mission-frame.md:184) |
| 0x00576120 | 10 | folded | covered by the port where it names it (src/gamez/zreader/zreader.cpp:626) |
| 0x00576140 | 40 | folded | zUtil's search path lists: each user keeps its own path string (zsnd_init.cpp:223, mech_fieldbase.h:295, zinterp.cpp:240) and the file finder is the platform's (zimg_library.h:75, zsnd_parm.cpp:17, FIX (A)); the default list's free has nothing to free (mission-frame.md:184) |
| 0x00576250 | 12 | folded | covered by the port where it names it (src/mech3/app_engine.cpp:326) |
| 0x00576270 | 15 | folded | zUtil's search path lists: each user keeps its own path string (zsnd_init.cpp:223, mech_fieldbase.h:295, zinterp.cpp:240) and the file finder is the platform's (zimg_library.h:75, zsnd_parm.cpp:17, FIX (A)); the default list's free has nothing to free (mission-frame.md:184) |
| 0x005762b0 | 48 | folded | zUtil's search path lists: each user keeps its own path string (zsnd_init.cpp:223, mech_fieldbase.h:295, zinterp.cpp:240) and the file finder is the platform's (zimg_library.h:75, zsnd_parm.cpp:17, FIX (A)); the default list's free has nothing to free (mission-frame.md:184) |
| 0x005763b0 | 17 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:240) |

### ztwod (73 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x0059fa80 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0059fab0 | 11 | folded | covered by the port where it names it (src/gamez/ztwod/twod_control.cpp:21) |
| 0x005a0050 | 16 | debug or dev tool: PORT (preserve) | the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683) |
| 0x005a0660 | 11 | folded | a thunk: one virtual call |
| 0x005a15f0 | 31 | folded | covered by the port where it names it (src/mech3/debriefing.cpp:461) |
| 0x005a1cd0 | 9 | folded | covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005a1cf0 | 13 | folded | covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005a22c0 | 9 | folded | a thunk: one call to another function |
| 0x005a2b90 | 11 | folded | sets members (a setter or a small constructor) |
| 0x005a31e0 | 32 | folded | covered by the port where it names it (src/mech3/briefing.cpp:220) |
| 0x005a3a40 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x005a3ba0 | 10 | folded | a getter of a member |
| 0x005a3bb0 | 11 | folded | sets members (a setter or a small constructor) |
| 0x005a3c40 | 9 | debug or dev tool: PORT (preserve) | follows 0x005a3cd0: the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683) |
| 0x005a3c50 | 9 | folded | a thunk: one call to another function |
| 0x005a3cd0 | 46 | debug or dev tool: PORT (preserve) | the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683) |
| 0x005a3de0 | 15 | debug or dev tool: PORT (preserve) | the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683) |
| 0x005a3e30 | 18 | debug or dev tool: PORT (preserve) | the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683) |
| 0x005a3e80 | 69 | debug or dev tool: PORT (preserve) | the text console layer (Arial) behind the developer lines and the interp console: its line objects, their font, colour and position setters, the line maker (twod.md:38, 683) |
| 0x005a3fd0 | 11 | folded | a thunk: one call to another function |
| 0x005a4000 | 9 | folded | a thunk: one call to another function |
| 0x005a4010 | 9 | folded | a thunk: one call to another function |
| 0x005a4210 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x005a4670 | 11 | folded | covered by the port where it names it (src/gamez/ztwod/twod_console.cpp:29) |
| 0x005a4690 | 12 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:513) |
| 0x005a47f0 | 10 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:535) |
| 0x005a4938 | 8 | folded | a piece of the ported function before it (Ghidra split it there) (0x005a4880) |
| 0x005a4b90 | 25 | folded | covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005a4f80 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x005a4fa0 | 13 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x005a5280 | 9 | folded | a thunk: one call to another function |
| 0x005a5310 | 9 | folded | a thunk: one call to another function |
| 0x005a5620 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x005a5b10 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x005a5e10 | 109 | folded | covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005a60e0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x005a6100 | 12 | folded | covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005a6600 | 12 | folded | covered by the port where it names it (src/gamez/ztwod/twod_console.cpp:77) |
| 0x005a6620 | 14 | folded | ported, by docs/symbols.csv: ported: the tint's +0x38 colour, +0x40 alpha, shown for 0 s (vt+0x60) |
| 0x005a6650 | 18 | folded | covered by the port where it names it (src/gamez/ztwod/twod_weather.cpp:345) |
| 0x005a66b0 | 13 | folded | covered by the port where it names it (src/gamez/ztwod/twod_console.cpp:66) |
| 0x005a66d0 | 14 | folded | sets members (a setter or a small constructor) |
| 0x005a6700 | 9 | folded | a thunk: one call to another function |
| 0x005a6710 | 9 | folded | a thunk: one call to another function |
| 0x005a6860 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x005a69e8 | 8 | folded | a piece of the ported function before it (Ghidra split it there) (0x005a68b0) |
| 0x005a6a10 | 45 | MISSING LOGIC (only data the shipped game lacks reaches it) | Line2D's reader from a dialog .zrd (its COLOR and its end), shared by the line classes' vtables; inferred unused by shipped data (the dialog reader matches the exe on every shipped dialog without it), not checked file by file |
| 0x005a6b20 | 11 | folded | sets members (a setter or a small constructor) |
| 0x005a7130 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x005a7288 | 97 | folded | a piece of the ported function before it (Ghidra split it there) (0x005a7280) |
| 0x005a7560 | 12 | folded | sets members (a setter or a small constructor) |
| 0x005a7580 | 12 | folded | sets members (a setter or a small constructor) |
| 0x005a7640 | 9 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:845) |
| 0x005a7670 | 12 | folded | covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005a7690 | 12 | folded | covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005a76b0 | 12 | folded | covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005a76d0 | 12 | folded | covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005a76f0 | 12 | folded | covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005a7710 | 12 | folded | covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005a7730 | 12 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:822) |
| 0x005a77d0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x005a7ac0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x005a7c50 | 20 | MISSING LOGIC (only data the shipped game lacks reaches it) | Movie2D's update, draw and erase (a movie playing inside a dialog): no shipped dialog has one (twod.md:674-679) |
| 0x005a7ca0 | 15 | MISSING LOGIC (only data the shipped game lacks reaches it) | Movie2D's update, draw and erase (a movie playing inside a dialog): no shipped dialog has one (twod.md:674-679) |
| 0x005a7cd0 | 12 | MISSING LOGIC (only data the shipped game lacks reaches it) | Movie2D's update, draw and erase (a movie playing inside a dialog): no shipped dialog has one (twod.md:674-679) |
| 0x005a7dd0 | 9 | folded | follows 0x005a8750: zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005a7de0 | 9 | folded | a thunk: one call to another function |
| 0x005a7e10 | 47 | folded | zEntity's base constructor (frames, and with a parent the registration 0x005a8390 + 0x005a7f50); the port's AiTarget initialisers stand in (app_engine.cpp:312). targets.zrd's CZAiTargets are not on the port's entity list (ai.md:1537), so their vt+0x34 (0x005a8000, the step by velocity +0x30 and spin +0x58) does not run; checked harmless: both start as the zero vector [0x0063d870] (BSS), targets.zrd has no velocity or spin key (ai.md:153) and no script command sets an entity's (interp.md: only CameraSetRotate and GlobalWindStaticVelocity) |
| 0x005a7f30 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x005a7f50 | 11 | folded | covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005a7f70 | 11 | folded | covered where named or as one function: TextBox's set-by-message-id slots (FontText's run instead, twod.h:398), the edit bar's split piece (0x005a38a0's caller), the snow update's tail (twod_weather.cpp:290), the weather object's destructor, the message box's button handlers (one per button, twod_msgbox.cpp:7), zEntity's list add and remove (app_mech.cpp:314, 840) |
| 0x005a7f90 | 25 | folded | a destructor (the exception unwinds call it; it resets the vtable) |
| 0x005a80c0 | 9 | folded | a thunk: one call to another function |

### zrender (69 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005662d0 | 91 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00566510 | 33 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00566620 | 133 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00566890 | 162 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00566c90 | 114 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00566f10 | 61 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x005670a0 | 120 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x005674e0 | 19 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00567530 | 21 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00567570 | 9 | folded | ported, by docs/symbols.csv: ported: zrndr::flares_clear (the frame's start, 0x004817cd) |
| 0x00567580 | 36 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x005675e0 | 26 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00567dc0 | 41 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x005680b0 | 8 | folded | empty: does nothing |
| 0x00568230 | 24 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x005682a0 | 24 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x005684f0 | 24 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x00568560 | 16 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x005685c0 | 249 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x005689d0 | 256 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056b500 | 184 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056b800 | 175 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056bb00 | 189 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056be40 | 180 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056c1d0 | 37 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056c320 | 51 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056c420 | 51 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056c520 | 181 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056c830 | 181 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056cbc0 | 39 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056cc40 | 39 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056ccc0 | 39 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056cd40 | 39 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056cdc0 | 39 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056ce40 | 39 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056cec0 | 39 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056cf80 | 88 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056d120 | 88 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056d2c0 | 39 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056d340 | 40 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056d3c0 | 39 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056d440 | 39 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056d4c0 | 39 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056d540 | 39 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056d5c0 | 39 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056d640 | 39 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056d6c0 | 92 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056d8b0 | 92 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056dab0 | 37 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056db50 | 37 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056dbf0 | 37 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056dc90 | 37 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056dd30 | 37 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056ddd0 | 37 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056de70 | 37 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056df10 | 37 | software renderer | the software renderer: its sprite and model-light draws (0x00566620..), the scene list of light points and lens flares the software frames draw their own way (0x00566f10, 0x005674e0..0x00567dc0; render.md: the Direct3D flares are ported), the software fog spans and tables, the span filler table (module-starts.md: read only by software code) and the rasteriser's span routines |
| 0x0056e160 | 15 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_play.cpp:23) |
| 0x0056e180 | 65 | replaced (SDL or the platform layer) | DirectSound: a sound's buffer duplicated or reused for another play (0x0056e180, 0x0056e4d0) and the buffer's volume and frequency calls (0x0056ef90); SDL's audio layer replaces them (zsnd_play) |
| 0x0056e2d0 | 53 | folded | ported, by docs/symbols.csv: ported: own voice, a duplicate, or a new one (max 7) |
| 0x0056e470 | 8 | folded | returns a constant or a global |
| 0x0056e480 | 18 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_play.cpp:82) |
| 0x0056e4d0 | 67 | replaced (SDL or the platform layer) | DirectSound: a sound's buffer duplicated or reused for another play (0x0056e180, 0x0056e4d0) and the buffer's volume and frequency calls (0x0056ef90); SDL's audio layer replaces them (zsnd_play) |
| 0x0056e610 | 68 | folded | ported, by docs/symbols.csv: ported (ECX sound, EDX dB volume, position, velocity, start) |
| 0x0056e750 | 32 | folded | ported, by docs/symbols.csv: ported: voice queue, newest first |
| 0x0056ee20 | 38 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_play.cpp:103) |
| 0x0056eea0 | 46 | folded | covered: a sound's play with the master volume (zsnd_play.cpp:134), the playing list's node |
| 0x0056ef90 | 28 | replaced (SDL or the platform layer) | DirectSound: a sound's buffer duplicated or reused for another play (0x0056e180, 0x0056e4d0) and the buffer's volume and frequency calls (0x0056ef90); SDL's audio layer replaces them (zsnd_play) |
| 0x0056f010 | 31 | folded | covered by the port where it names it (src/gamez/zsound/zsnd_play.cpp:118) |
| 0x0056f230 | 19 | folded | covered: a sound's play with the master volume (zsnd_play.cpp:134), the playing list's node |

### zvideo (64 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00576f90 | 79 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005770f0 | 11 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00577340 | 11 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00577630 | 29 | folded | covered by the port where it names it (src/gamez/zvideo/zvid_dd.h:28) |
| 0x00577690 | 21 | folded | covered by the port where it names it (src/gamez/zvideo/zvid_dd.h:29) |
| 0x005776d0 | 29 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00577730 | 21 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005778d0 | 44 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00577a10 | 80 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00577c40 | 21 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00577cb0 | 11 | folded | covered by the program's overrides: the load screen's driver and mode lines (app_mission.cpp:1896), ClearScreenBuffer (app_engine.h:508), video shutdown and restore (app_engine.h:547, :549), a weapon hit's health after (zweapon.h:171) |
| 0x00577d40 | 25 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00577db0 | 13 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00577df0 | 227 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00578510 | 36 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005785f0 | 31 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00578670 | 32 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005786f0 | 18 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00578740 | 72 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005788b0 | 34 | folded | covered by the port where it names it (src/gamez/zvideo/zvid_ddd3d.cpp:983) |
| 0x00578960 | 16 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00578c00 | 36 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00578f60 | 12 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00579000 | 8 | folded | returns a constant or a global |
| 0x00579010 | 8 | folded | returns a constant or a global |
| 0x00579040 | 11 | folded | covered by the program's overrides: the load screen's driver and mode lines (app_mission.cpp:1896), ClearScreenBuffer (app_engine.h:508), video shutdown and restore (app_engine.h:547, :549), a weapon hit's health after (zweapon.h:171) |
| 0x00579210 | 14 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00579260 | 14 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00579280 | 14 | folded | covered by the port where it names it (src/gamez/zfmv/fmv_stream.cpp:92) |
| 0x005792a0 | 14 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00579430 | 30 | folded | covered by the port where it names it (src/gamez/zvideo/zvid_ddd3d.cpp:936) |
| 0x00579500 | 85 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00579660 | 105 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00579840 | 24 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00579880 | 12 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x005799f0 | 11 | folded | covered by the program's overrides: the load screen's driver and mode lines (app_mission.cpp:1896), ClearScreenBuffer (app_engine.h:508), video shutdown and restore (app_engine.h:547, :549), a weapon hit's health after (zweapon.h:171) |
| 0x00579a10 | 15 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00579a40 | 15 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00579a70 | 10 | folded | returns a constant or a global |
| 0x00579ab0 | 17 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00579b40 | 16 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00579bd0 | 30 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00579c60 | 79 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00579ea0 | 8 | folded | returns a constant or a global |
| 0x00579eb0 | 11 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00579ef0 | 14 | folded | covered by the program's overrides: the load screen's driver and mode lines (app_mission.cpp:1896), ClearScreenBuffer (app_engine.h:508), video shutdown and restore (app_engine.h:547, :549), a weapon hit's health after (zweapon.h:171) |
| 0x00579f20 | 16 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0057a1a0 | 80 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0057a390 | 15 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0057a3c0 | 13 | folded | covered by the program's overrides: the load screen's driver and mode lines (app_mission.cpp:1896), ClearScreenBuffer (app_engine.h:508), video shutdown and restore (app_engine.h:547, :549), a weapon hit's health after (zweapon.h:171) |
| 0x0057a3d0 | 164 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0057a900 | 288 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0057b100 | 18 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0057b6c0 | 64 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0057b7b0 | 107 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0057ba80 | 16 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0057bb60 | 9 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x0057e9e0 | 15 | folded | covered by the port where it names it (src/gamez/zvideo/zvid_ddd3d.cpp:298) |
| 0x0057ea00 | 18 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00580350 | 80 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00580550 | 610 | replaced (SDL or the platform layer) | DirectDraw and Direct3D: the mode, surfaces, blits, back buffer locks and grabs, the device and its memory queries, the render states' set-up, the textures' system surfaces, the cursor and window calls, the DirectX version and its error names; the SDL GPU layer replaces them (render.md, fixes.md (A)) |
| 0x00581e30 | 8 | folded | returns a constant or a global |
| 0x00581e40 | 8 | folded | returns a constant or a global |
| 0x00583730 | 11 | folded | covered by the program's overrides: the load screen's driver and mode lines (app_mission.cpp:1896), ClearScreenBuffer (app_engine.h:508), video shutdown and restore (app_engine.h:547, :549), a weapon hit's health after (zweapon.h:171) |

### zinterp (62 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00594dc0 | 9 | folded | a thunk: one call to another function |
| 0x00594e50 | 9 | folded | a thunk: one call to another function |
| 0x00594e90 | 9 | folded | a thunk: one call to another function |
| 0x00594ec0 | 98 | folded | the script log line's formatter, covered where named (zinterp.cpp:288) |
| 0x00594f90 | 33 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:303) |
| 0x00595449 | 22 | folded | a piece of the ported function before it (Ghidra split it there) (0x00595440) |
| 0x00595b10 | 27 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:405) |
| 0x00595b60 | 26 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:406) |
| 0x00595da0 | 19 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:267) |
| 0x00595e20 | 16 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:271) |
| 0x00595e40 | 15 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:407) |
| 0x005960a0 | 27 | debug or dev tool: PORT (preserve) | the hidden GameZ Interpreter console (0x005960a0: a 2D console window that runs typed script lines through 0x00595510; its destructor and line runner; toggled by 0x004810d0, which nothing reaches: interp.md:532, discoveries.md:1532) |
| 0x00596150 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00596170 | 14 | debug or dev tool: PORT (preserve) | the hidden GameZ Interpreter console (0x005960a0: a 2D console window that runs typed script lines through 0x00595510; its destructor and line runner; toggled by 0x004810d0, which nothing reaches: interp.md:532, discoveries.md:1532) |
| 0x00596190 | 15 | debug or dev tool: PORT (preserve) | the hidden GameZ Interpreter console (0x005960a0: a 2D console window that runs typed script lines through 0x00595510; its destructor and line runner; toggled by 0x004810d0, which nothing reaches: interp.md:532, discoveries.md:1532) |
| 0x005961d0 | 31 | replaced (SDL or the platform layer) | a text object's font set from a GDI font handle (GetObjectA into a LOGFONT; twod.h:319): the SDL text host keeps fonts by name |
| 0x00596260 | 9 | folded | a thunk: one call to another function |
| 0x00596270 | 9 | folded | a thunk: one call to another function |
| 0x00596900 | 9 | folded | a thunk: one call to another function |
| 0x00596910 | 9 | folded | a thunk: one call to another function |
| 0x00596930 | 62 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00596a70 | 30 | unused: PORT (preserve) | list teardown helpers whose callers (0x00596930, 0x00596c60) nothing reaches |
| 0x00596b00 | 45 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00596bd0 | 22 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00596c50 | 9 | folded | a thunk: one call to another function |
| 0x00596c60 | 9 | folded | a thunk: one call to another function |
| 0x00596c80 | 56 | unused: PORT (preserve) | list teardown helpers whose callers (0x00596930, 0x00596c60) nothing reaches |
| 0x00596e10 | 58 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00596f70 | 316 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x005974a0 | 31 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00597530 | 48 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x005975e0 | 49 | software renderer | SetPaletteName: the 8-bit palette file into 0x00802918 and its brightness step 0x00597680 (only the software path uses a palette; interp.md, the commands the shipped scripts never use) |
| 0x00597680 | 92 | software renderer | SetPaletteName's palette brightness step for the 8-bit palette (0x005975e0's helper; only the software path uses a palette) |
| 0x00597e60 | 25 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00597eb0 | 12 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00597ed0 | 12 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00597ef0 | 40 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00597f30 | 19 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00597f60 | 14 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00597fc0 | 55 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00598100 | 25 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00598150 | 11 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00598160 | 12 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00598190 | 16 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005981b0 | 8 | folded | returns a constant or a global |
| 0x005981c0 | 17 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005981e0 | 12 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00598330 | 38 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00598360 | 35 | folded | covered by the port where it names it (src/gamez/zutil/zutl_reg.h:42) |
| 0x005983c0 | 59 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005984a0 | 10 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005984e0 | 8 | folded | returns a constant or a global |
| 0x00598530 | 8 | folded | returns a constant or a global |
| 0x00598580 | 35 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005985d0 | 56 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005986c0 | 48 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005987d0 | 59 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x005988c0 | 110 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00598ae0 | 12 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00598b00 | 16 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00598b30 | 66 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |
| 0x00598c80 | 13 | replaced (SDL or the platform layer) | the machine probe (fixes.md:289, (A)): CPUID and its vendor, family and MMX tests, the MHz timing loops (QueryPerformanceCounter, RDTSC, the CMOS port, Intel's cpuspeed with its divide by zero: protection.md:140), GlobalMemoryStatus and the DirectDraw caps; the port fills zutl_reg's Machine itself (zutl_reg.cpp:48) |

### zimage (60 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00517280 | 17 | folded | covered by the port where it names it (src/frontend/sdl/sdl_twod_host.h:36) |
| 0x005172c0 | 14 | replaced (SDL or the platform layer) | zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x005172e0 | 14 | replaced (SDL or the platform layer) | zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00517300 | 13 | replaced (SDL or the platform layer) | zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00517310 | 27 | replaced (SDL or the platform layer) | zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00517370 | 48 | replaced (SDL or the platform layer) | zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00517410 | 47 | replaced (SDL or the platform layer) | zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x005174b0 | 61 | replaced (SDL or the platform layer) | zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x005175e0 | 29 | replaced (SDL or the platform layer) | zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00517650 | 16 | replaced (SDL or the platform layer) | zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x00517680 | 22 | replaced (SDL or the platform layer) | zimg_winfonts: the GDI fonts by name from fonts.zrd's WINDOWS_FONTS (list, find, addref, release, teardown); the SDL text host draws its own fonts (sdl_twod_host.h:36) |
| 0x005176b0 | 31 | folded | covered by the port where it names it (src/gamez/zimage/zimg_texture.cpp:306) |
| 0x00517730 | 16 | folded | covered by the port where it names it (src/gamez/zimage/zimg_texture.cpp:291) |
| 0x00517760 | 16 | folded | covered by the port where it names it (src/frontend/sdl/sdl_twod_host.cpp:173) |
| 0x00517790 | 13 | folded | covered by the port where it names it (src/gamez/zimage/zimg_texture.cpp:320) |
| 0x005177a0 | 47 | folded | the texture cache by name and its teardown: g_cache (zimg_texture.cpp:296-321), a std::list freed with the program |
| 0x00517840 | 20 | folded | covered by the port where it names it (src/gamez/zimage/zimg_texture.cpp:318) |
| 0x005178a0 | 61 | folded | the texture cache by name and its teardown: g_cache (zimg_texture.cpp:296-321), a std::list freed with the program |
| 0x00517be0 | 16 | folded | the texture cache by name and its teardown: g_cache (zimg_texture.cpp:296-321), a std::list freed with the program |
| 0x00517c10 | 9 | folded | covered by the port where it names it (src/frontend/sdl/sdl_twod_host.cpp:67) |
| 0x00517c30 | 12 | folded | covered by the port where it names it (src/gamez/zimage/zimg_library.cpp:295) |
| 0x00517c40 | 15 | folded | covered by the port where it names it (src/gamez/zimage/zimg_library.cpp:348) |
| 0x00517c60 | 12 | folded | covered by the port where it names it (src/gamez/zimage/zimg_library.cpp:294) |
| 0x00517c70 | 16 | folded | covered by the port where it names it (src/gamez/zmodel/gmod_cull.cpp:350) |
| 0x00517c90 | 15 | folded | covered by the port where it names it (src/frontend/sdl/sdl_twod_host.cpp:67) |
| 0x00517f70 | 14 | folded | covered by the port where it names it (src/gamez/zimage/zimg_library.cpp:34) |
| 0x00517fe0 | 9 | folded | a thunk: one call to another function |
| 0x00517ff0 | 9 | folded | a thunk: one call to another function |
| 0x00518020 | 9 | folded | a thunk: one call to another function |
| 0x00518030 | 9 | folded | a thunk: one call to another function |
| 0x005184e0 | 47 | replaced (SDL or the platform layer) | the texture table's start-up: zeroes the table and makes the Direct3D default texture record; the port's GPU textures make their own (zimg_texture.cpp:109, fixes.md puffer row) |
| 0x005185f0 | 13 | folded | covered by the port where it names it (src/frontend/sdl/sdl_twod_host.cpp:174) |
| 0x00518760 | 14 | folded | covered by the port where it names it (src/gamez/zimage/zimg_library.cpp:386) |
| 0x00518790 | 11 | folded | one-line setters/lookups the port does inline: zImage's library flag (no effect, app_engine.cpp:307), the texture group (mech_lib.cpp:34, app_mission.cpp:1910), the archive file test (zimg_library.cpp:20), the extension cut (zimg_texture.cpp:94) |
| 0x005187a0 | 11 | folded | one-line setters/lookups the port does inline: zImage's library flag (no effect, app_engine.cpp:307), the texture group (mech_lib.cpp:34, app_mission.cpp:1910), the archive file test (zimg_library.cpp:20), the extension cut (zimg_texture.cpp:94) |
| 0x00518c50 | 9 | folded | a thunk: one call to another function |
| 0x00518f50 | 11 | folded | one-line setters/lookups the port does inline: zImage's library flag (no effect, app_engine.cpp:307), the texture group (mech_lib.cpp:34, app_mission.cpp:1910), the archive file test (zimg_library.cpp:20), the extension cut (zimg_texture.cpp:94) |
| 0x00518f60 | 8 | folded | returns a constant or a global |
| 0x005190e0 | 15 | unused: PORT (preserve) | frees a chain of loaded textures; its only caller 0x005374d0 is unreached |
| 0x00519120 | 25 | software renderer | the software renderer's texture tables: the nth mip of a chain for 0x00563940 (module-starts.md:158) and the 32 light and fog levels of every paletted texture (software-path.md:30) |
| 0x00519150 | 121 | folded | one-line setters/lookups the port does inline: zImage's library flag (no effect, app_engine.cpp:307), the texture group (mech_lib.cpp:34, app_mission.cpp:1910), the archive file test (zimg_library.cpp:20), the extension cut (zimg_texture.cpp:94) |
| 0x00519370 | 51 | software renderer | the software renderer's texture tables: the nth mip of a chain for 0x00563940 (module-starts.md:158) and the 32 light and fog levels of every paletted texture (software-path.md:30) |
| 0x00519510 | 38 | software renderer | the software renderer's texture tables: the nth mip of a chain for 0x00563940 (module-starts.md:158) and the 32 light and fog levels of every paletted texture (software-path.md:30) |
| 0x005195b0 | 75 | software renderer | the software renderer's texture tables: the nth mip of a chain for 0x00563940 (module-starts.md:158) and the 32 light and fog levels of every paletted texture (software-path.md:30) |
| 0x005197d0 | 11 | software renderer | follows 0x00519510: the software renderer's texture tables: the nth mip of a chain for 0x00563940 (module-starts.md:158) and the 32 light and fog levels of every paletted texture (software-path.md:30) |
| 0x00519820 | 65 | replaced (SDL or the platform layer) | the square-only device resize (D3D caps 0x20; render-drawtable.md:124): no modern device needs it |
| 0x00519990 | 20 | debug or dev tool: PORT (preserve) | the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it |
| 0x005199e0 | 27 | debug or dev tool: PORT (preserve) | the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it |
| 0x00519a20 | 14 | debug or dev tool: PORT (preserve) | the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it |
| 0x00519a40 | 51 | debug or dev tool: PORT (preserve) | the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it |
| 0x00519b30 | 64 | debug or dev tool: PORT (preserve) | the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it |
| 0x00519be0 | 20 | debug or dev tool: PORT (preserve) | the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it |
| 0x00519c20 | 25 | debug or dev tool: PORT (preserve) | the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it |
| 0x00519cc0 | 69 | debug or dev tool: PORT (preserve) | the screenshot writer of the debug command 0x7e (0x004abbc0): copies the front buffer to 24-bit, flips it and saves a .bmp through a save dialog (0x00519cc0); no key reaches it |
| 0x00519df0 | 20 | unused: PORT (preserve) | zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4) |
| 0x00519e50 | 51 | unused: PORT (preserve) | zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4) |
| 0x00519f70 | 20 | unused: PORT (preserve) | zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4) |
| 0x00519fa0 | 52 | unused: PORT (preserve) | zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4) |
| 0x0051a080 | 30 | unused: PORT (preserve) | zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4) |
| 0x0051a0d0 | 51 | unused: PORT (preserve) | zFont, the bitmap fonts from fonts.zrd: loaded at start-up (0x00519e50), split into glyphs, measured by id (0x0051a0d0), freed (0x00519f70), but nothing in 1.2 draws them (app_engine.cpp:318, twod.md section 4) |

### zclass (54 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x004eb030 | 11 | folded | covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x004eb040 | 8 | folded | returns a constant or a global |
| 0x004eb420 | 17 | unlabelled |  |
| 0x004eb6a0 | 17 | folded | covered by the port where it names it (src/gamez/zgeometry/zgeo_model.cpp:431) |
| 0x004eb710 | 17 | folded | ported, by docs/symbols.csv: ported inline; node +0x24 bit 4 (the program's node_intersects) |
| 0x004eb8a0 | 17 | folded | covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x004eb8e0 | 17 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:200) |
| 0x004ebd50 | 38 | folded | covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x004ed950 | 11 | folded | covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x004ed980 | 11 | folded | ported, by docs/symbols.csv: ported inline in gmod_draw: node +0xc0 = [0x5e9e2c] (update_frame) after the static bake |
| 0x004ed9c0 | 11 | folded | ported, by docs/symbols.csv: ported as zcls::segment_options().first_hit ([0x0060b968]) |
| 0x004ed9d0 | 11 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.h:85) |
| 0x004ed9e0 | 15 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.h:86) |
| 0x004eda00 | 13 | folded | ported, by docs/symbols.csv: ported as zcls::segment_options().exclude ([0x0060b974]) |
| 0x004f0020 | 249 | folded | covered: a piece of the ported ray query (0x004f0020 runs in its caller's stack frame), a world's ground value (0x004f2480, from the ported sound 0x004f3170) |
| 0x004f0f00 | 35 | folded | covered by the port where it names it (src/gamez/zclass/cls_di.cpp:1267) |
| 0x004f1810 | 32 | folded | covered by the port where it names it (src/gamez/zclass/cls_world.cpp:220) |
| 0x004f22f0 | 21 | folded | covered by the port where it names it (src/gamez/zdeclient/zdec_crater.cpp:365) |
| 0x004f2480 | 12 | folded | covered: a piece of the ported ray query (0x004f0020 runs in its caller's stack frame), a world's ground value (0x004f2480, from the ported sound 0x004f3170) |
| 0x004f2490 | 12 | folded | covered by the port where it names it (src/mech3/game_editor_net.cpp:127) |
| 0x004f33e0 | 24 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:519) |
| 0x004f3540 | 13 | unused: PORT (preserve) | no effect in the original (app_engine.cpp's list): a camera word nothing reads (0x004f3540), zClass's start defaults the port's initialisers already hold (0x004f8f20), a zSave registration with flag 0 (0x004f8f60) |
| 0x004f3630 | 25 | folded | covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x004f4d4d | 95 | folded | a piece of the ported function before it (Ghidra split it there) (0x004f4ab0) |
| 0x004f5a00 | 31 | folded | covered by the port where it names it (src/gamez/zclass/camera.cpp:488) |
| 0x004f6dc0 | 102 | software renderer | the camera's software scene walk (0x004f6dc0 without a device; hud.md: the software path) |
| 0x004f7630 | 16 | folded | covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x004f7650 | 16 | folded | covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x004f8c30 | 9 | folded | a thunk: one call to another function |
| 0x004f8f20 | 10 | unused: PORT (preserve) | no effect in the original (app_engine.cpp's list): a camera word nothing reads (0x004f3540), zClass's start defaults the port's initialisers already hold (0x004f8f20), a zSave registration with flag 0 (0x004f8f60) |
| 0x004f8f40 | 9 | folded | covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x004f8f60 | 11 | unused: PORT (preserve) | no effect in the original (app_engine.cpp's list): a camera word nothing reads (0x004f3540), zClass's start defaults the port's initialisers already hold (0x004f8f20), a zSave registration with flag 0 (0x004f8f60) |
| 0x004f8f70 | 14 | folded | covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x004f96b0 | 10 | folded | covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x004f96d0 | 10 | folded | covered by the program's overrides or ports: immediate delete (app_mission.cpp:1567), node flags (app_mech.cpp:200), the node box (cls_di.cpp:1267), stamps frozen (app_mech.cpp:212), the scene walk's callbacks (app_mech.cpp:207), the HUD window's rect (app_hud.cpp:194), the deferred-delete list (list.cpp:189), zClass's start, reset and pool flag (list.cpp:338, :350; app_engine.cpp:853), the copy refusals for light and sound nodes (cls_util.cpp:208) |
| 0x004f9820 | 10 | folded | covered by the port where it names it (src/gamez/zclass/cls_util.cpp:207) |
| 0x004f99b0 | 10 | folded | covered by the port where it names it (src/gamez/zclass/cls_util.cpp:206) |
| 0x004f99d0 | 10 | folded | covered by the port where it names it (src/gamez/zclass/cls_util.cpp:210) |
| 0x004f9f60 | 24 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:519) |
| 0x004fa540 | 24 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:519) |
| 0x004fa900 | 24 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:520) |
| 0x004fc470 | 14 | folded | covered by the port where it names it (src/gamez/zclass/cls_mechlib.cpp:126) |
| 0x004fc4d0 | 18 | unused: PORT (preserve) | mechlib's material writer at close (0x004fc990 -> 0x004fcca0 -> 0x004fc4d0): dead (mechlib.md) |
| 0x004fc5d0 | 35 | folded | covered by the port where it names it (src/gamez/zclass/cls_mechlib.h:38) |
| 0x004fc990 | 21 | unused: PORT (preserve) | mechlib's material writer at close (0x004fc990 -> 0x004fcca0 -> 0x004fc4d0): dead (mechlib.md) |
| 0x004fcca0 | 39 | unused: PORT (preserve) | mechlib's material writer at close (0x004fc990 -> 0x004fcca0 -> 0x004fc4d0): dead (mechlib.md) |
| 0x004fd1c0 | 13 | folded | covered by the port where it names it (src/mech3/menu_widgets.h:233) |
| 0x004fd1e0 | 30 | MISSING LOGIC (only data the shipped game lacks reaches it) | zClass's add-child for an animate (0x004fd8c0) or switch (0x004fd1e0) parent: the port's node_add_child (cls_node.cpp:186) handles worlds, links and object3d and aborts ('not ported yet') for cameras, LoDs, animate and switch parents, though the camera's own add-child is ported (camera.cpp, 0x004f33a0). No shipped data adds such a child at run time (every mission plays); a mod's scripts or data would abort the game |
| 0x004fd230 | 30 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:521) |
| 0x004fd440 | 24 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:521) |
| 0x004fd8c0 | 24 | MISSING LOGIC (only data the shipped game lacks reaches it) | zClass's add-child for an animate (0x004fd8c0) or switch (0x004fd1e0) parent: the port's node_add_child (cls_node.cpp:186) handles worlds, links and object3d and aborts ('not ported yet') for cameras, LoDs, animate and switch parents, though the camera's own add-child is ported (camera.cpp, 0x004f33a0). No shipped data adds such a child at run time (every mission plays); a mod's scripts or data would abort the game |
| 0x004fd900 | 30 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:520) |
| 0x004fe0b0 | 9 | folded | covered by the port where it names it (src/gamez/zclass/cls_node.cpp:555) |
| 0x004fe0f0 | 9 | folded | covered by the port where it names it (src/gamez/zclass/list.cpp:354) |

### zreader (50 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00557450 | 16 | unused: PORT (preserve) | the defines list's constructor and destructor, no caller reaches them (zreader.md:103) |
| 0x00557490 | 17 | unused: PORT (preserve) | the defines list's constructor and destructor, no caller reaches them (zreader.md:103) |
| 0x005574c0 | 9 | folded | a thunk: one call to another function |
| 0x00557b10 | 9 | folded | start-up and stop plumbing with no effect in the port: zRender's two buffers freed at stop (app_engine.cpp:326), the pitch store that is only written (app_engine.cpp:311) |
| 0x00557b20 | 10 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00557b30 | 130 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00557f50 | 11 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00557f70 | 9 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00558070 | 14 | folded | start-up and stop plumbing with no effect in the port: zRender's two buffers freed at stop (app_engine.cpp:326), the pitch store that is only written (app_engine.cpp:311) |
| 0x00558080 | 15 | replaced (SDL or the platform layer) | the render target's size for the FMV and 2D layers: the hosts give it (sdl_twod_host.cpp:380, app_fmv.cpp:79) |
| 0x005580d0 | 23 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00558120 | 16 | software renderer | SetPerspectiveTextureAdaptive: the software span's perspective step (read only at 0x0055f8bc), as its siblings 0x005580d0..0x00558170 (interp.md, the commands the shipped scripts never use) |
| 0x00558140 | 13 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:125) |
| 0x00558170 | 17 | folded | covered by the port where it names it (src/gamez/zinterp/zinterp.cpp:126) |
| 0x00558250 | 16 | folded | covered by the port where it names it (src/mech3/app_engine.cpp:329) |
| 0x00558d40 | 23 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0055a950 | 22 | folded | covered by the port where it names it (src/gamez/ztwod/twod_weather.cpp:143) |
| 0x0055aa20 | 262 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0055b250 | 115 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0055bbf0 | 299 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0055c2f0 | 244 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0055c900 | 300 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0055d660 | 387 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0055e310 | 372 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0055efa0 | 570 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0055f000 | 656 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x0055ffff | 8 | software renderer | follows 0x0055f000: the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00560010 | 355 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x005608f0 | 477 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00561910 | 486 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00562940 | 9 | folded | covered by the port where it names it (src/gamez/zrender/zrndr_blit.h:47) |
| 0x00562c80 | 9 | folded | covered by the port where it names it (src/gamez/zrender/zrndr_prim2d.cpp:37) |
| 0x00562d20 | 19 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00562d90 | 114 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00563940 | 36 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x005641f0 | 12 | folded | covered by the port where it names it (src/gamez/zrender/zrndr_prim2d.cpp:145) |
| 0x00564210 | 15 | folded | point-in-rect test, inline in the ported caller (zrndr_prim2d.cpp:21) |
| 0x00564240 | 26 | folded | covered by the port where it names it (src/gamez/zrender/zrndr_prim2d.cpp:145) |
| 0x005642f0 | 16 | folded | covered by the port where it names it (src/gamez/zrender/zrndr_prim2d.cpp:145) |
| 0x00564370 | 38 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00564460 | 31 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00564500 | 9 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00564660 | 38 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x005646d0 | 16 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00564710 | 108 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00565d30 | 145 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00566070 | 36 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00566110 | 26 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00566230 | 15 | software renderer | the software renderer: zRender's reset and span table (0x00557b20/30), its perspective step (SetPerspectiveTextureDeltaX), the mip toggle on a debug key (0x00557f70, only software reads [0x00800814]), the span fillers and depth spans, the 8- and 16-bit polygon fills and blits, the pixel-plotted lights and flares (weapon-effects.md:61), the fog and light table picks, the span buffer's init, clear and free and its screen pick (software-path.md:28,46) |
| 0x00566250 | 19 | folded | covered by the port where it names it (src/mech3/mission_world.h:89) |

### zentity (44 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005a80d0 | 12 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:510) |
| 0x005a80f0 | 54 | folded | ported, by docs/symbols.csv: ported for the turrets (app_turret.cpp): the zone record of the ground under the node, the camera's zone onto it |
| 0x005a8210 | 45 | folded | zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005a8280 | 28 | folded | covered by the port where it names it (src/mech3/app_mission.cpp:1640) |
| 0x005a82b8 | 17 | folded | follows 0x005a8280: covered by the port where it names it (src/mech3/app_mission.cpp:1640) |
| 0x005a82e0 | 14 | folded | zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005a8310 | 19 | folded | covered by the port where it names it (src/mech3/mech_build.cpp:279) |
| 0x005a8350 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x005a8370 | 12 | folded | covered by the port where it names it (src/mech3/veh_basic_object.cpp:285) |
| 0x005a84c0 | 13 | folded | zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005a84e0 | 12 | folded | covered by the port where it names it (src/mech3/mech_place.cpp:102) |
| 0x005a8540 | 8 | folded | returns a constant or a global |
| 0x005a8550 | 10 | folded | covered by the port where it names it (src/mech3/app_mech.cpp:522) |
| 0x005a8580 | 10 | folded | covered by the port where it names it (src/mech3/mech_place.cpp:100) |
| 0x005a85a0 | 10 | folded | zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005a8690 | 10 | folded | a getter of a member |
| 0x005a86a0 | 10 | folded | a getter of a member |
| 0x005a86b0 | 15 | folded | covered by the port where it names it (src/mech3/mech_place.cpp:108) |
| 0x005a86e0 | 16 | folded | covered by the port where it names it (src/mech3/mech.h:95) |
| 0x005a8710 | 11 | folded | covered by the port where it names it (src/mech3/veh_controls.h:82) |
| 0x005a8730 | 11 | folded | zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005a8750 | 47 | folded | zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005a8820 | 13 | folded | zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005a89c0 | 35 | folded | zEntity's list and its entries, covered where named: by name (app_vehicle.h:188), the position getters, the node's name (app_mech.cpp:831), the moved slot vt+0x1c (Mech::entity_moved, app_sensors.cpp:219, app_mech.cpp:526; the sensors' mech fallback at app_sensors.cpp:133 only logs and no port code calls it), the manager's set-up (zent_manager.h:18), add and remove (app_mission.cpp:1822-1826) |
| 0x005a8be0 | 40 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:75) |
| 0x005a8c58 | 29 | folded | follows 0x005a8be0: covered by the port where it names it (src/gamez/zsave/zsave.cpp:75) |
| 0x005a8c90 | 12 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:108) |
| 0x005a8cb0 | 77 | folded | zSave's list insert, load and stable sort, covered where named (zsave.cpp:86, 108, 132) and std::list's sort helpers |
| 0x005a8d58 | 37 | folded | follows 0x005a8cb0: zSave's list insert, load and stable sort, covered where named (zsave.cpp:86, 108, 132) and std::list's sort helpers |
| 0x005a8da0 | 35 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:102) |
| 0x005a8e40 | 145 | folded | zSave's list insert, load and stable sort, covered where named (zsave.cpp:86, 108, 132) and std::list's sort helpers |
| 0x005a9140 | 23 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:95) |
| 0x005a91b0 | 13 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:112) |
| 0x005a91d0 | 13 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:175) |
| 0x005a9210 | 22 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:191) |
| 0x005a9290 | 14 | folded | covered by the port where it names it (src/gamez/zsave/zsave.cpp:212) |
| 0x005a92d0 | 9 | unused: PORT (preserve) | _rmtmp at zSave's stop, unreached |
| 0x005a92e0 | 158 | folded | zSave's list insert, load and stable sort, covered where named (zsave.cpp:86, 108, 132) and std::list's sort helpers |
| 0x005a96a0 | 17 | folded | zSave's list insert, load and stable sort, covered where named (zsave.cpp:86, 108, 132) and std::list's sort helpers |
| 0x005a96d0 | 51 | folded | zSave's list insert, load and stable sort, covered where named (zsave.cpp:86, 108, 132) and std::list's sort helpers |
| 0x005a97e0 | 17 | folded | zSave's list insert, load and stable sort, covered where named (zsave.cpp:86, 108, 132) and std::list's sort helpers |
| 0x005a98d0 | 17 | debug or dev tool: PORT (preserve) | the developers' loose <name>.zan override for motion scripts (motion.md:36), like the reader's loose .zrd files |
| 0x005a9920 | 27 | folded | covered by the port where it names it (src/gamez/zsiscript/si_script.cpp:135) |
| 0x005a9970 | 13 | folded | covered by the port where it names it (src/gamez/zsave/zsave.h:8) |

### zfmv (31 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005996d0 | 12 | folded | ported, by docs/symbols.csv: ported: timeGetTime() x 0.001f |
| 0x00599710 | 12 | folded | ported, by docs/symbols.csv: ported |
| 0x00599750 | 9 | folded | ported, by docs/symbols.csv: ported: 0x00598d30(flag) |
| 0x00599800 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00599bc0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00599d50 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00599d70 | 11 | folded | small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x0059a100 | 29 | folded | ported, by docs/symbols.csv: ported: 0x0055a540(1) |
| 0x0059a190 | 29 | folded | ported, by docs/symbols.csv: ported: 0x0055a540(2) |
| 0x0059a310 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0059a330 | 31 | folded | small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x0059a3b0 | 11 | replaced (SDL or the platform layer) | cutscenes through MCI MPEGVideo (0x0059b080 open, 0x0059b200 stop, 0x0059b280 the error text) and the end that grabs and clears the screen (0x0059a3d0, fmv.md:181); the port's zFMV plays Indeo itself |
| 0x0059a3d0 | 30 | replaced (SDL or the platform layer) | cutscenes through MCI MPEGVideo (0x0059b080 open, 0x0059b200 stop, 0x0059b280 the error text) and the end that grabs and clears the screen (0x0059a3d0, fmv.md:181); the port's zFMV plays Indeo itself |
| 0x0059a4a5 | 32 | folded | a piece of the ported function before it (Ghidra split it there) (0x0059a4a0) |
| 0x0059b040 | 15 | folded | covered by the port where it names it (src/gamez/zfmv/fmv_stream.h:9) |
| 0x0059b070 | 11 | folded | frees the object (a destructor's release) |
| 0x0059b080 | 78 | replaced (SDL or the platform layer) | cutscenes through MCI MPEGVideo (0x0059b080 open, 0x0059b200 stop, 0x0059b280 the error text) and the end that grabs and clears the screen (0x0059a3d0, fmv.md:181); the port's zFMV plays Indeo itself |
| 0x0059b200 | 19 | replaced (SDL or the platform layer) | cutscenes through MCI MPEGVideo (0x0059b080 open, 0x0059b200 stop, 0x0059b280 the error text) and the end that grabs and clears the screen (0x0059a3d0, fmv.md:181); the port's zFMV plays Indeo itself |
| 0x0059b250 | 15 | folded | small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x0059b280 | 43 | replaced (SDL or the platform layer) | cutscenes through MCI MPEGVideo (0x0059b080 open, 0x0059b200 stop, 0x0059b280 the error text) and the end that grabs and clears the screen (0x0059a3d0, fmv.md:181); the port's zFMV plays Indeo itself |
| 0x0059b4d0 | 40 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:631) |
| 0x0059b610 | 21 | folded | covered by the port where it names it (src/gamez/ztwod/twod_shapes.cpp:159) |
| 0x0059b640 | 40 | folded | covered by the port where it names it (src/gamez/ztwod/twod.h:631) |
| 0x0059b780 | 21 | folded | covered by the port where it names it (src/gamez/ztwod/twod_shapes.cpp:159) |
| 0x0059b8c0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0059bdb0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0059d8b0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0059e150 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x0059e9c0 | 17 | folded | small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x0059ea20 | 9 | folded | a thunk: one call to another function |
| 0x0059ed90 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |

### zgame (26 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005900f9 | 11 | folded | a piece of the ported function before it (Ghidra split it there) (0x0058fd70) |
| 0x00590120 | 58 | replaced (SDL or the platform layer) | the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005901ba | 11 | replaced (SDL or the platform layer) | follows 0x00590120: the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x00590780 | 43 | folded | zGameApp's small helpers inside ported callers or static teardown: a buffer cursor's init (0x00590850, from 0x00590250 / 0x00590400 / 0x005905a0), its drain at exit, the registry company name 'Zipper Interactive' (zutl_reg's company), two destructors |
| 0x00590850 | 16 | folded | zGameApp's small helpers inside ported callers or static teardown: a buffer cursor's init (0x00590850, from 0x00590250 / 0x00590400 / 0x005905a0), its drain at exit, the registry company name 'Zipper Interactive' (zutl_reg's company), two destructors |
| 0x00590880 | 26 | replaced (SDL or the platform layer) | the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005908e0 | 8 | replaced (SDL or the platform layer) | the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005908f0 | 8 | folded | returns a constant or a global |
| 0x005908f5 | 8 | folded | follows 0x005908f0: returns a constant or a global |
| 0x00590900 | 8 | replaced (SDL or the platform layer) | the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x00590910 | 8 | folded | returns a constant or a global |
| 0x00590920 | 17 | replaced (SDL or the platform layer) | the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x00590960 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00590980 | 28 | replaced (SDL or the platform layer) | the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x005909f0 | 14 | replaced (SDL or the platform layer) | the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x00590a10 | 9 | folded | zGameApp's small helpers inside ported callers or static teardown: a buffer cursor's init (0x00590850, from 0x00590250 / 0x00590400 / 0x005905a0), its drain at exit, the registry company name 'Zipper Interactive' (zutl_reg's company), two destructors |
| 0x00590a50 | 52 | replaced (SDL or the platform layer) | the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x00590b40 | 10 | replaced (SDL or the platform layer) | the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x00590b60 | 15 | replaced (SDL or the platform layer) | the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x00590b80 | 22 | replaced (SDL or the platform layer) | the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x00590bd0 | 15 | replaced (SDL or the platform layer) | the MFC frame window: its class and message-map getters, constructor (frame_new) and destructor, OnPaint, OnSize (the video resize), OnCreate (the GAMEBMP icon, the mouse), OnDestroy (net, video, CD stop), the window test, and the top-level catch that names the exception in a message box; the SDL window and the port's crash box stand in |
| 0x00590c70 | 14 | folded | covered by the port where it names it (src/gamez/zgame/zgameapp.cpp:182) |
| 0x00590ca0 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00590cc0 | 21 | folded | zGameApp's small helpers inside ported callers or static teardown: a buffer cursor's init (0x00590850, from 0x00590250 / 0x00590400 / 0x005905a0), its drain at exit, the registry company name 'Zipper Interactive' (zutl_reg's company), two destructors |
| 0x00590d10 | 14 | folded | the scalar deleting destructor (the destructor, then delete) |
| 0x00590d30 | 21 | folded | zGameApp's small helpers inside ported callers or static teardown: a buffer cursor's init (0x00590850, from 0x00590250 / 0x00590400 / 0x005905a0), its drain at exit, the registry company name 'Zipper Interactive' (zutl_reg's company), two destructors |

### zdeclient (25 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00500670 | 16 | folded | read: the static constructor of a decal container (an STL tree's head made by 0x00500cd0, the counts at [0x0060bf94..0x0060bf9c] zeroed), which zdec_init's 0x00500850 / 0x00500af0 / 0x00500c60 use; the port's container |
| 0x005006b0 | 9 | folded | a thunk: one call to another function |
| 0x005006f0 | 22 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00500cd0 | 43 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00500da0 | 46 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00500e90 | 58 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00500ff0 | 316 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00501520 | 31 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x005015b0 | 196 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x005018d0 | 48 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00501980 | 52 | folded | C++ library container code (a std::map, set or list instance); the port uses its own containers |
| 0x00501a40 | 28 | folded | small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x00501a80 | 24 | folded | small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x00502d30 | 8 | folded | returns a constant or a global |
| 0x00502da0 | 11 | folded | small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x00502db0 | 13 | folded | covered by the port where it names it (src/mech3/zai_target.h:236) |
| 0x00502dd0 | 13 | folded | covered by the port where it names it (src/mech3/zai_target.h:237) |
| 0x0050414d | 11 | folded | a piece of the ported function before it (Ghidra split it there) (0x00503d50) |
| 0x00504d42 | 203 | folded | a piece of the ported function before it (Ghidra split it there) (0x00504520) |
| 0x00505520 | 73 | folded | small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x005056b0 | 25 | folded | covered by the port where it names it (src/gamez/zeffect/zeff_anim_events.cpp:602) |
| 0x00505710 | 116 | folded | small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x00507220 | 53 | folded | small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x005073e0 | 18 | folded | covered by the port where it names it (src/gamez/zeffect/zeff_anim_events.cpp:1175) |
| 0x00507960 | 8 | folded | returns a constant or a global |

### zutil (25 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00576bc0 | 34 | folded | covered by the port where it names it (src/gamez/zutil/zutl_zar.cpp:176) |
| 0x00576c50 | 9 | folded | zUtil and zVideo entries the port does its own way, covered where named: the CRC table's start (app_engine.cpp:314), the search paths' free (app_engine.cpp:323), the mode switch (video_set_mode, app_engine.cpp:724), the frame and lab clears (app_engine.h:196, 562) |
| 0x00576c60 | 9 | folded | zUtil and zVideo entries the port does its own way, covered where named: the CRC table's start (app_engine.cpp:314), the search paths' free (app_engine.cpp:323), the mode switch (video_set_mode, app_engine.cpp:724), the frame and lab clears (app_engine.h:196, 562) |
| 0x00576c70 | 8 | folded | returns a constant or a global |
| 0x00576c80 | 10 | folded | zUtil and zVideo entries the port does its own way, covered where named: the CRC table's start (app_engine.cpp:314), the search paths' free (app_engine.cpp:323), the mode switch (video_set_mode, app_engine.cpp:724), the frame and lab clears (app_engine.h:196, 562) |
| 0x00576ca0 | 8 | folded | returns a constant or a global |
| 0x00576cb0 | 8 | folded | returns a constant or a global |
| 0x00576cc0 | 8 | folded | returns a constant or a global |
| 0x00576cd0 | 8 | folded | returns a constant or a global |
| 0x00576cf0 | 11 | folded | zUtil and zVideo entries the port does its own way, covered where named: the CRC table's start (app_engine.cpp:314), the search paths' free (app_engine.cpp:323), the mode switch (video_set_mode, app_engine.cpp:724), the frame and lab clears (app_engine.h:196, 562) |
| 0x00576d70 | 8 | folded | returns a constant or a global |
| 0x00576d80 | 8 | folded | returns a constant or a global |
| 0x00576d90 | 8 | folded | returns a constant or a global |
| 0x00576da0 | 8 | folded | returns a constant or a global |
| 0x00576db0 | 8 | folded | returns a constant or a global |
| 0x00576dc0 | 8 | folded | returns a constant or a global |
| 0x00576dd0 | 11 | folded | zUtil and zVideo entries the port does its own way, covered where named: the CRC table's start (app_engine.cpp:314), the search paths' free (app_engine.cpp:323), the mode switch (video_set_mode, app_engine.cpp:724), the frame and lab clears (app_engine.h:196, 562) |
| 0x00576de0 | 17 | replaced (SDL or the platform layer) | DirectDraw's back buffer: lock and unlock (app_engine.h:560, app_fmv.cpp:53), their raw forms, the flip (sdl_platform.h:68), the restore, the window's style for full screen, the size clamp |
| 0x00576e70 | 11 | replaced (SDL or the platform layer) | DirectDraw's back buffer: lock and unlock (app_engine.h:560, app_fmv.cpp:53), their raw forms, the flip (sdl_platform.h:68), the restore, the window's style for full screen, the size clamp |
| 0x00576e80 | 11 | replaced (SDL or the platform layer) | DirectDraw's back buffer: lock and unlock (app_engine.h:560, app_fmv.cpp:53), their raw forms, the flip (sdl_platform.h:68), the restore, the window's style for full screen, the size clamp |
| 0x00576e90 | 11 | replaced (SDL or the platform layer) | DirectDraw's back buffer: lock and unlock (app_engine.h:560, app_fmv.cpp:53), their raw forms, the flip (sdl_platform.h:68), the restore, the window's style for full screen, the size clamp |
| 0x00576ea0 | 12 | replaced (SDL or the platform layer) | DirectDraw's back buffer: lock and unlock (app_engine.h:560, app_fmv.cpp:53), their raw forms, the flip (sdl_platform.h:68), the restore, the window's style for full screen, the size clamp |
| 0x00576ed0 | 12 | replaced (SDL or the platform layer) | DirectDraw's back buffer: lock and unlock (app_engine.h:560, app_fmv.cpp:53), their raw forms, the flip (sdl_platform.h:68), the restore, the window's style for full screen, the size clamp |
| 0x00576ee0 | 24 | replaced (SDL or the platform layer) | DirectDraw's back buffer: lock and unlock (app_engine.h:560, app_fmv.cpp:53), their raw forms, the flip (sdl_platform.h:68), the restore, the window's style for full screen, the size clamp |
| 0x00576f70 | 19 | replaced (SDL or the platform layer) | DirectDraw's back buffer: lock and unlock (app_engine.h:560, app_fmv.cpp:53), their raw forms, the flip (sdl_platform.h:68), the restore, the window's style for full screen, the size clamp |

### zweapon (19 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00584920 | 9 | folded | a thunk: one call to another function |
| 0x00584de0 | 12 | folded | sets members (a setter or a small constructor) |
| 0x00584f42 | 24 | folded | a piece of the ported function before it (Ghidra split it there) (0x00584f40) |
| 0x00586ba0 | 29 | folded | covered by the port where it names it (src/mech3/app_engine.h:59) |
| 0x00586d70 | 9 | folded | follows 0x0042a570: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x00586d80 | 9 | folded | a thunk: one call to another function |
| 0x00586db0 | 9 | folded | follows 0x0042a570: the controls screen (controls_menu, menus.md's table names each): its buttons' handlers ported in its switch, its controls' destructors (CRT callers: exception unwinds), vector and string helpers |
| 0x00586dc0 | 9 | folded | a thunk: one call to another function |
| 0x00586de0 | 15 | replaced (SDL or the platform layer) | DirectPlay's session description and player list (multiplayer-port.md:59-60): the ENet transport stands in |
| 0x00586e00 | 8 | folded | returns a constant or a global |
| 0x00586ed0 | 21 | folded | covered by the port where it names it (src/gamez/znetwork/znet_session.cpp:64) |
| 0x00586f30 | 18 | replaced (SDL or the platform layer) | DirectPlay's session description and player list (multiplayer-port.md:59-60): the ENet transport stands in |
| 0x005870c0 | 32 | replaced (SDL or the platform layer) | DirectPlay's session description and player list (multiplayer-port.md:59-60): the ENet transport stands in |
| 0x00587140 | 80 | folded | the game list's search, ours in the ENet transport (net_enet.cpp:441) |
| 0x005872a0 | 34 | folded | covered by the port where it names it (src/gamez/znetwork/znet_session.cpp:59) |
| 0x00587300 | 29 | replaced (SDL or the platform layer) | DirectPlay's session description and player list (multiplayer-port.md:59-60): the ENet transport stands in |
| 0x00587370 | 31 | folded | covered by the port where it names it (src/gamez/znetwork/znet_session.cpp:96) |
| 0x005873f0 | 33 | replaced (SDL or the platform layer) | DirectPlay's session description and player list (multiplayer-port.md:59-60): the ENet transport stands in |
| 0x00587450 | 19 | replaced (SDL or the platform layer) | DirectPlay's session description and player list (multiplayer-port.md:59-60): the ENet transport stands in |

### zgeometry (15 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005128e0 | 18 | folded | covered by the port where it names it (src/gamez/zgeometry/zgeo_weiler.h:55) |
| 0x00512910 | 18 | folded | covered by the port where it names it (src/gamez/zgeometry/zgeo_weiler.h:55) |
| 0x00512940 | 27 | folded | covered by the port where it names it (src/gamez/zgeometry/zgeo_weiler.h:55) |
| 0x00514dc0 | 13 | folded | sets members (a setter or a small constructor) |
| 0x00515aa0 | 28 | folded | covered by the port where it names it (src/gamez/zgeometry/zgeo_convexify.cpp:133) |
| 0x00515f00 | 9 | folded | a thunk: one call to another function |
| 0x00516290 | 19 | folded | covered by the port where it names it (src/gamez/zgeometry/zgeo_convexify.h:13) |
| 0x00516f00 | 12 | folded | zImage's start, stop and per-mission entries, covered where named: shutdown (memory only, app_engine.cpp:324), the texture group begin (app_mission.cpp:1912), the textures' free (app_mission.cpp:1874), the texture directory as the 2D host's mounts (sdl_twod_host.cpp:205, FIX (A)) |
| 0x00516f20 | 9 | folded | zImage's start, stop and per-mission entries, covered where named: shutdown (memory only, app_engine.cpp:324), the texture group begin (app_mission.cpp:1912), the textures' free (app_mission.cpp:1874), the texture directory as the 2D host's mounts (sdl_twod_host.cpp:205, FIX (A)) |
| 0x00516f30 | 10 | folded | zImage's start, stop and per-mission entries, covered where named: shutdown (memory only, app_engine.cpp:324), the texture group begin (app_mission.cpp:1912), the textures' free (app_mission.cpp:1874), the texture directory as the 2D host's mounts (sdl_twod_host.cpp:205, FIX (A)) |
| 0x00516f50 | 13 | folded | zImage's start, stop and per-mission entries, covered where named: shutdown (memory only, app_engine.cpp:324), the texture group begin (app_mission.cpp:1912), the textures' free (app_mission.cpp:1874), the texture directory as the 2D host's mounts (sdl_twod_host.cpp:205, FIX (A)) |
| 0x00516f80 | 29 | replaced (SDL or the platform layer) | zimg_winfonts' entries read from fonts.zrd's WINDOWS_FONTS and their GDI fonts (with the list in zImage): the SDL text host keeps its own |
| 0x00516ff0 | 92 | replaced (SDL or the platform layer) | zimg_winfonts' entries read from fonts.zrd's WINDOWS_FONTS and their GDI fonts (with the list in zImage): the SDL text host keeps its own |
| 0x00517230 | 15 | replaced (SDL or the platform layer) | zimg_winfonts' entries read from fonts.zrd's WINDOWS_FONTS and their GDI fonts (with the list in zImage): the SDL text host keeps its own |
| 0x00517250 | 21 | replaced (SDL or the platform layer) | zimg_winfonts' entries read from fonts.zrd's WINDOWS_FONTS and their GDI fonts (with the list in zImage): the SDL text host keeps its own |

### zeffect (14 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x00508aa0 | 11 | folded | covered by the port where it names it (src/gamez/zdeclient/zdec_crater.h:76) |
| 0x00508c10 | 9 | folded | a thunk: one call to another function |
| 0x00508da0 | 9 | folded | a thunk: one call to another function |
| 0x00508ee0 | 9 | folded | a thunk: one call to another function |
| 0x00508f20 | 10 | folded | ported, by docs/symbols.csv: ported as the hooks' anim_callback: +0x6c |
| 0x00508f30 | 10 | folded | ported, by docs/symbols.csv: ported as the hooks' anim_record: +0x70 |
| 0x0050b4e0 | 32 | folded | small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x0050b600 | 13 | folded | covered by the port where it names it (src/mech3/zai_target.h:239) |
| 0x0050b620 | 8 | folded | empty: does nothing |
| 0x0050b650 | 13 | folded | covered by the port where it names it (src/gamez/zeffect/zeff_anim_init.cpp:746) |
| 0x0050d1e0 | 13 | folded | small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x0050d1f0 | 13 | folded | small helpers inside ported code: the crater set's static constructor and its STL copy and fill (craters.md:126), the effect lookups and the animation records' flags (app_mission.cpp:1248, app_engine.h:506-507), the cockpit flag (app_view.cpp:206), the key player (zeff_anim_events.cpp:627) and its split piece, the ground probe (zeff_anim_events.cpp:1109), the movie objects' destructors and the stream's rect setter, a list's vector-deleting destructor |
| 0x0050d260 | 13 | folded | covered by the port where it names it (src/gamez/zeffect/zeff_anim_save.cpp:281) |
| 0x0050da50 | 16 | replaced (SDL or the platform layer) | mech3.err (0x0050da50): the port's error reports go to stderr (app_engine.cpp:562) |

### zsiscript (4 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x005a9990 | 23 | folded | covered by the port where it names it (src/mech3/mech_anim.cpp:413) |
| 0x005a9bc0 | 34 | debug or dev tool: PORT (preserve) | the motion script's text side: the loose .zan reader (0x005a9e10, with 0x005aa210) behind the developers' override, and the writer (0x005a9bc0) |
| 0x005a9e10 | 167 | debug or dev tool: PORT (preserve) | the motion script's text side: the loose .zan reader (0x005a9e10, with 0x005aa210) behind the developers' override, and the writer (0x005a9bc0) |
| 0x005aa210 | 18 | debug or dev tool: PORT (preserve) | the motion script's text side: the loose .zan reader (0x005a9e10, with 0x005aa210) behind the developers' override, and the writer (0x005a9bc0) |

### zerror (1 not ported)

| Address | Lines | Class | Why |
|---|---|---|---|
| 0x0050da70 | 9 | folded | a thunk: one call to another function |


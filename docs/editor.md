# The Battlesport game editor and the stealth object

MW3 1.2 still carries an in-game editor titled for a game called Battlesport:
a mouse menu headed "BATTLESPORT GAME EDITOR" over a mission,
with the view on the stealth object, an AI net editor and a vehicle editor.
Nothing in the release reaches it: it opens on debug command 0x9a, which no
key is bound to (0x0040db60 registers only 0x61, 0x70, 0x7e and 0x6c to the
debug handler 0x0040dba0). Mapped 2026-10-31 (dev 2); the entry, exit,
panel, main menu and the view on the stealth ported 2026-11-01 (S4, below;
the AI net and vehicle editors behind its rows come after v1.0). The
port reaches it with Shift+F12 under `--debug-camera` (or `debugcamera=1`
in mw3.cfg) only, as Owen asked: "exactly what they did", a developer's
tool. (F11 is the lancemate command screen in the original's default keys,
Shift+F11 and Ctrl+F11 the awaiting orders; Shift+F12 is bound to nothing,
on every keyboard. Agreed with the lead 2026-11-01.)

Why the name is open. These docs used to say Zipper made Battlesport before
MW3; nothing supports that. The public credits give BattleSport (1996) to
Cyclone Studios, with its ports by Unexpected Development, and don't list it
among Zipper's games. The exe has only the title string, in MW3 and in Pirate's
Moon, so it shows neither that this is the same game nor how the name got here.

Recoil answers it (2026-11-07, strings of its two exes, built November and December
1998; neither was run). Its debug paths are `D:\Proj\GameZRecoil\...` for the engine
and `D:\Proj\Battlesport\...` for the game (`ai_net.cpp`, `Briefing.cpp`, `hud.cpp`,
`map.cpp`, `mission.cpp`, `pickup.cpp`, `player.cpp`, `turret.cpp`). So Battlesport was
Zipper's working name for Recoil's project, and MW3's editor title most likely came
with the code from it. Recoil's exes have "stealth" and an `ai_net.cpp` but none of the
editor's menu text ("PICK A COMMAND", "BATTLESPORT", the net editor's rows): by strings
alone, not a disassembly, the menus aren't in them.

The Windows dialogs are another matter (2026-11-07, read from each exe's resources; whether
code still opens them is not checked). `Mech3.exe` 1.2 holds 38 dialog templates, 17 in Recoil's:
both have AI Network Editor (139), Net Edit (140), Node Edit - Vehicle (141), Map Area
Editor (142), Vehicle Select (143), Camera Properties (106), Teleport (132). MW3 adds Mech Editor (149),
Select Mech (156), Global Light Properties (160), Mech AI (165) with pages p1, p2, p5, Attack, Retreat, Fire,
Valve, Zig Zag, Circle, Jump, Direct and Random Move (166-176, 183), Node Parameters (180), a second Ai
Network Editor (181), Mech Animation Transitions (182) and Enter Cheat Code (138). Pirate's Moon's exe
keeps only Teleport (132) and one dialog of its own (188), though its menus' strings are there.

## The entry and the exit

**Command 0x9a** (0x0040dba0's switch, decoded from its byte table at
0x0040dea4 and jump table 0x0040de34) calls **0x0040e430**:

- [0x005fc314] = 1 (the editor is up);
- a command map pushed (0x0051d3a0 with null: a copy of the current one,
  so every game key still works) with two bindings (0x0051d2c0): Space
  (key 0x39) and mouse button 1 -> command 0x9b (pick), key 0x439 and mouse
  button 2 -> command 0x9c (a trace, empty in the release: 0x005ac5c0);
- the view on the stealth object: [0x00609a2c]'s vt+0x108 (0x004acd90 ->
  0x0040adf0 with the vehicles' current camera, [0x0060b140]);
- the debug lines hidden when they were up ([0x005f4dac] = 0x0040f880, the
  state of [0x005fc33c]; when set, 0x0040f8b0 hides the two debug-line
  layers 0x005fc338 and 0x005fc838). The release never turns them on (hud.md
  section 20), so this does nothing there; corrected 2026-11-01: an earlier
  reading here took it for the camera mode;
- the editor panel's layer on ([0x005f4db0]'s vt+0x04(1));
- 0x0040e7d0: the main menu (below).

**0x0040e7f0** ("EXIT GAME EDIT", its argument unread): the command map
popped (0x0051d490), the panel's layer off (vt+0x04(0)), the debug lines
back when they were up (0x0040f890), [0x005fc314] = 0. It leaves the view
on the stealth object: in the original the camera stays where the editor
left it, and the pilot's mech goes on without its view.

## The panel and its menus

The panel ([0x005f4db0], vtable 0x005bc510, built by 0x0040e270(5, 0x5f),
freed by 0x0040e3b0) holds 25 text rows (0x4b0 bytes each, row object
vtable 0x005bc590, 12 pixels apart from y 0x5f).

A menu is data: `{int count; {char text[20]; void (*handler)(arg);} x count}`.
**0x0040e7b0**(ECX mode, EDX table, the title, "") sets [0x005fc30c] (mode)
and [0x005fc318] (table) and fills the rows (**0x0040e6a0**: every row
cleared with "" at 0x005f4c90, then each entry's text, the panel's width
the widest row's, its height count x 12).

**The frame, 0x0040e4b0** (from the HUD's draw; while [0x005fc314]): the
row under the mouse ((y - 0x5f) / 12, rows 0..24, then the row's vt+0x5c
hit test) lit green (0xff00 in both colours, +0x24c/+0x250, +0x474 to
render again), the one before back in the panel's colours; the title row
is never lit. Space or the left button (command 0x9b's state, 0x0051d300
bit 4: pressed) on a row calls its entry's handler with the mouse's y: a
shown row its own (the input's state cleared first, 0x0051b1c0), the title
the menu's first (none in the main menu); the panel's layer updated
(0x0059fd20 with [0x0072c52c]); in modes 1..3 the net editor's 0.1 s step
(0x00403140); 0x9c held calls the empty trace; then, up or not, the net
editor's frame 0x00404290 (nothing without a net editor).

### The main menu (0x005e0d30, 8 rows; 0x0040e7d0)

| Row | Text | Handler | What it does |
|---|---|---|---|
| 0 | PICK A COMMAND | none | the header |
| 1 | EDIT AI NETS | 0x00402d60 | the AI net editor (below) |
| 2 | EDIT VEHICLES | 0x004040c0 -> 0x004b86e0 | the vehicle editor: a 0x988-byte object (0x004b80f0, vtables 0x005c9898, 0x005c97c0), [0x0060b310]; also debug command 0x9d's |
| 3 | EDIT WEAPONS | 0x004bf620 | the shared empty function: nothing in MW3 |
| 4..6 | (blank) | none | |
| 7 | EXIT GAME EDIT | 0x0040e7f0 | the exit above |

### The AI net editor (0x005e0f18, 13 rows; installed by 0x00403690)

0x00402d60 first picks a point or net node under the mouse (0x0040e120 ->
0x004d55c0, the camera's pick vt+0xf8, 0x0058b1c0), then installs the menu
"AI NET EDITOR" in mode 1. It is the editing side of `CZAiNetEdit`
(0x004bd360, 0x004bd3d0; docs/ai.md: only the builder side is ported).

| Row | Text | Handler |
|---|---|---|
| 0 | Pick CMD or Point | 0x004bf620 (empty) |
| 1 | GOTO Net | 0x004036c0 |
| 2 | Change Net Params | 0x004036f0 |
| 3 | Node Params | 0x00403710 |
| 4 | Add veh to node | 0x00403820 |
| 5 | Move Node | 0x00403ac0 |
| 6 | Delete Node | 0x00403ae0 |
| 7 | Delete Net | 0x004bf620 (empty) |
| 8 | Save Changes | 0x00402e90 |
| 9 | Tree View | 0x00402c70 |
| 10, 11 | (blank) | none |
| 12 | Exit Net Editor | 0x00402ba0 |

**Save Changes does not reach the disk in the release** (checked 2026-11-07,
S5 sitting 1). 0x00402e90 -> 0x00402f20 -> 0x004bd340 runs 0x004bd100, then
0x004d72a0, which is `xor eax, eax; ret`: the net's slot +0x1c, the empty
default of the base class. 0x004bd100 only folds the edits into the file
object's records in memory (0x004bef80 -> 0x004dc2b0; deleted nodes through
0x004dc290). The destructor 0x004bd020 does the same: when the net is dirty
(+0x39) it calls the same stub. The text writers are all still there,
0x004da1d0 (slot +0x1c of the file object; 0x004dad10, the header) and
0x004da7e0 (slot +0x14; 0x004db8d0, a node), but nothing found calls them.
They need a FILE*, the exe has no `_fdopen`, and every `fopen` for writing
is something else: the controls (0x00411e60), location.zrd (0x004a37a0),
stdout (0x00575b00, `freopen`), a zbd save (0x004fe8e0, debug path 0x592423,
not traced further). Pirate's Moon is the same
(0x004c4160, stub 0x0051a4d0), and its writer 0x004ecaa0 adds `via` and
`static_mfb`. So by this reading an edit in the release lasts until the file object goes;
not run.

## The AI net editor, the map (S5 sitting 1, 2026-11-07)

**The object.** `operator new(0x140)`, ctor 0x00402f40, in [0x005f4c6c],
over the selection base 0x0058bf50, with one list of them at [0x005f4c60].

| Offset | What |
|---|---|
| +0x14 | the net (`CZAiNetEdit`, editing side), owned |
| +0xfc, +0x100 | selected nodes, count (0x00403770: the first, a box when more) |
| +0x108, +0x10c | selected lines, count (0x004037b0: its ends, by direction) |
| +0x114, +0x118 | the list of nodes to link to, count (consumed in the frame) |
| +0x128 | the camera's far clip before the editor, restored by 0x00402fd0 |
| +0x130, +0x134 | Move Node's state (0..3) and the node |
| +0x138 | dirty; cleared by Save, carried over a rebuild |
| +0x13c | rebuild asked (0x00403080 deletes this and makes a new one on the same net) |

**The flow.** 0x00402d60: the first pick (0x0040e120 -> 0x004d55c0): an
object under the mouse that is a net (vt+0x48) edits it through 0x004bd360 by
id; else the camera's pick (vt+0xf8) gives a point, a free id from
0x0058b1c0 and a new net with one node (0x00402c90). It frees the old editor,
keeps the new one in [0x005f4c6c] and installs the menu (0x00403690).
0x00404290 runs the frame of every editor but the one given, 0.1 s each,
0x00403080 / 0x00403160 for each. The frame: the view (0x0058c950, the
selection box), the mouse's point on the ground (the camera's vt+0xf8), the
far clip set to 1.5 times the distance to it (x + y + z) when the view looks
down past 70 degrees (1.2217 rad), the saved one otherwise, Move Node's
states, then the links.

| Row | What it does |
|---|---|
| GOTO Net | camera vt+0xdc to the net's place (net vt+4) |
| Change Net Params | net vt+0x5c: a dialog (FIX (A): no MFC) |
| Node Params | the first selected node's vt+0x60 and vt+0x88: a dialog |
| Add veh to node | a vehicle picked from a dialog (0x0049d350, 0x0049d8b0), placed at the node, a compiled net built when there is none (0x004be670) |
| Move Node | 0x00403a90: state 1 waits for the button up, state 2 for a press, state 3 follows the mouse while it is held (vt+0x2c, vt+0x38), sets dirty; letting go ends it |
| Delete Node, line | confirm (OK, Cancel), +0x138 set, net vt+0x58 for a line |
| Save Changes | as above, and clears +0x138 |
| Tree View | 0x00402bf0: one window object, 0x004b5580 (0xa8 bytes) |
| Exit Net Editor | 0x00402ba0: frees the editor and both side windows, back to the main menu |

Every row's box goes through 0x00402ec0 (a format into 128 bytes, then
0x00402f10); the port needs its own for those and the dialogs (FIX (A)). The file box 0x004bf750
(`CFileDialog`, "Zipper Files") is an Open, not a Save; no call to it found.

**Pirate's Moon.** About the same set of functions (46 at these addresses,
50 in MW3), moved 0x10 here. PM's picker 0x00402d70 differs from 0x00402d60:
the camera's pick is vt+0x104, and the id call takes (1, 100, 1), with the
point passed to the net's maker. The fork on `pm` needs reading, not only
addresses. The reader
keys `via` (626 nodes) and `static_mfb` (12) are PM's own.

**The files.** Counted by a script over every mission archive:

| | Files | Nodes | Versions |
|---|---|---|---|
| MW3 | 751 in 53 archives | 19,971 | 308 net 106, 396 reg (393 108, 3 106), 47 waynet 107 |
| PM | 619 in 45 archives | 14,332 | 459 net 106, 115 reg 108, 45 waynet 107 |

No shipped file is version 105, the only version 0x004bcba0 sends to the
reload 0x004bed90: the sitting 4 reload needs reading again for 106.

Three things stand against the original's text writer as a save format:

1. **It rounds.** Positions are `%6.1f`: 128 of 49,710 MW3 values and 127 of
   39,747 PM values do not survive it. Property floats (`%6.3f`) all do.
2. **It is text, and the exe cannot use a text net over a shipped one.**
   0x00556740 asks the open archives first and looks for a loose file only
   when none has the name. A text net_01.zrd is ignored by the original.
3. **A loose net names too much, in the port as well.** The mod rule
   (`mod_loose`) matches by folder and name, and the net reader does come
   through it (RdrFile::load -> zrdr::load; read, not run). But one campaign
   folder holds many archives: 115 MW3 names (80 PM) are in more than one
   archive of a folder, 91 (68) of them with different contents. c1's
   net_01.zrd is in eight archives, so a loose copy replaces it in all eight.

**So a save writes the mission's whole archive** (decided 2026-11-07), as the
mod's own copy, `mods\<name>\zbd\c1\readerm1.zbd`, and never touches a
shared one or the install. Every other entry is copied as it is and the
edited net goes in as a binary tree (zrd's types 1..4: the exact floats, no
limit on links). The log names the archive written. A saved net is
reproducible: a mod holding an unchanged copy of an archive plays as the
original. The port reads the file under the rule that a mod's copy of a game
file is read instead; the original should read the same file dropped in its
`zbd\c1`, untested until there is a real saved archive. The text writers
are ported too, for a readable copy beside it and a byte-for-byte test on
the original's own code (0x004db8d0 run in the emulator). The "BS style"
branch (version 105, three links, padded) is used by no shipped file.

**Functions by sitting.** Counted as what the roots call, not yet ported,
each counted once under the first sitting that needs it. Direct calls and the
net's vtable only, so the other classes' slots will add some.

| Sitting | Functions | Pseudocode lines | Largest |
|---|---|---|---|
| 2 writers | 7 | 420 | 0x004db8d0, 0x004dad10; the archive writer is ported (`zutl_zar`, the saved loadouts' 0x005765b0..) |
| 3 viewing, the reticle pick | 70 | 2,800 | selection base 0x0058bf50, 0x0058ba40, the frame 0x00403160, 0x0058dfd0, 0x0058da70 |
| 4 editing | 83 | 3,200 | 0x004c6680, 0x004dc710, 0x004de2c0, 0x004ddc90, 0x004dc2b0, the vehicle box 0x0049d350 |
| 4 tree view | 3 | 70 | 0x00402bf0, 0x004b5580 |
| 4 the reload as the exe has it | 25 | 960 | 0x004025b0, 0x00404860, 0x004bed90 |

That is about 190 functions, where this page's first estimate for S5 and S6
was part of 140.

**The reload.** 0x004bed90 does nothing unless the version is 105 and no
shipped file is, so in the exe an edit to a shipped net never reaches the
vehicles running on it. Its 25 functions, with the older net class, are for a
format that does not exist in the data, and stay unported as ai.md,
Not ported on purpose, says. What sitting 4 needs instead is ours: 0x004be300, the compile, is ported
already, and vehicles on the old net go over to the new one on 0x004bed90's
pattern (the list at [0x0073655c], vt+0x140, vt+0x64, then vt+4), or the
edit shows at the next mission load. The first is small and is a choice: it
does what the 105 path was for, and the release never did it.

## The stealth object

Made at every mission load ("Creating Stealth", **0x004a4de0**, [0x00609a2c],
vtable 0x005c8460, 0x104 bytes; vehicle.zrd's `stealth`: no model, sounds
or weapons, health 10000, `mode` basic, rates 40 / 65, turns 2.5 / 3.5);
renamed "stealth" by the mission load (0x005a84c0); deleted by the unload
(0x00490480, its destructor 0x004a4fc0). Its +0x09 is 0, so the entity walk
never runs it; its frame runs from the player's tick while the view is on
it. Ported in S1 (mech3/veh_stealth; vehicles.md, The stealth object):

- **0x004a4de0**: the basic vehicle's constructor (0x004ac530) over its
  controls, the HUD markers (0x004ac3d0), a new node named "Stealth" in the
  world, the entity's node and list, the origin, the node out of the
  segment queries (0x004eb6d0) and flag 8 off (0x004eb620), the controls'
  motion mode (BASIC) and state zeroed, the overlay, +0x09 cleared;
  vehicle.zrd's `stealth` (its common_mode through 0x0049ec00, its motion
  block through 0x0049ec90), the points (0x0049f660); placed where aiv.zrd's
  `stealth` record says when the mission has one (0x004e0680, 0x004a3630).
- **The overlay** (0x004a55c0, 0x108 bytes, vtable 0x005c86e8): a zTwoD
  layer, active from the start, holding the image "ret_off" (0x004a5850:
  centred at the origin, shown for good); its frame (0x004a56b0) draws a
  readout of the movement mode (BASIC, TRACK, HOVER, AMPHIB, SUB) through
  [0x005fc848]; turning it off calls 0x0040fb50(0). The readout and that
  call come with the view, S4.

**Its frame** (vt+0x34, **0x004a5050**; ported in S4): the controls' frame
(below), the overlay's reticle (**0x004a5a00**: the image at
(320 - trunc(-320 mouse_x), 240 - trunc(-240 mouse_y)) on the 640x480
screen, then its pick 0x004a58d0, the world point under it through the
view's camera, which only the editors read), the move 0x004a1240 (its
motion mode's step: BASIC) and the node's matrix 0x005a85c0. Unlike a basic
vehicle's frame (0x004a1040) it asks nothing of a path; its vt+0x100 is
empty and its vt+0x104 (**0x004a50d0**: the view letting go of it) turns the
overlay off.

**Its controls** (0x004a51b0 over the base 0x004ac750, 0xa1c bytes;
vtables 0x005c86b0, IControls 0x005c85f8; corrected 2026-11-01: an earlier
reading here took the basic vehicle's own controls, 0x004ae270, for them)
are the base's but for vt+0x1c (0x004d72a0: never destroyed), vt+0x20
(0x004a5270: 1) and the frame, IControls vt+0x0c (0x004a5ae0 ->
**0x004a5280**): each frame the player's own movement commands (0x0051d300:
THROTTLE_UP/DN 0x18/0x19, TURN_LEFT/RIGHT 2/3, PITCH_UP/DN 4/5,
CENTER_TORSO 6) and the mouse (0x0051ba60), clamped to -1..1, smoothed by
0x004abcf0(x, 0.5, 1) into the throttle and turn (IControls +0x20, +0x28)
and two more axes (+0x9c0, +0x9bc). So the stealth is a camera flown with
the original's own keys while the view is on it. The basic vehicle's own
controls (0x004ae270, 0x004ae850, the path modes 0..4 of 0x00401050) are a
driverless vehicle's, not the stealth's; no shipped mission makes one, so
they wait (vehicles.md, The basic vehicle's own controls).

**BASIC** (0x004a1d90, its ground 0x004a1f00), the stealth's movement
mode: the steering (0x004a20d0: the turn into the heading +0x330, wrapped
at 2 pi) or, in camera mode 3 (straight down), north-up panning (0x004a2000:
heading 0, the throttle on the sideways speed, stopped dead before
reversing); the speed step 0x004a2b10 (ported); the move on the horizontal
plane; then the four platform points' ground (0x004a2190), the highest plus
+0x88 of the motion block, pitch and roll 0: a level sled above the ground.

## The port, in sittings (agreed 2026-10-31; scope set by Owen 2026-11-01)

About 140 unported functions for S1..S6 and about 200 (5,500 lines of
pseudocode) for the vehicle editor.

**For v1.0: S1 to S4**, the stealth object and the view on it.

1. **S1** The stealth vehicle 0x004a4de0 with 0x004a55c0, 0x004a5850,
   0x005a84c0, its controls 0x004a51b0 (their frame 0x004a5280 a stub until
   S3), the destructor 0x004a4fc0; made at every mission load, as the
   original's "Creating Stealth" does, deleted at the unload. Done
   2026-11-01: test_veh_stealth.py. Tests:
   construct and read against the original; the program makes it in c1m1;
   the full suites and the campaign runs as the proof that play does not
   change, with the mission load time before and after.
2. **S2** BASIC: 0x004a1d90, 0x004a1f00, 0x004a20d0, 0x004a2000,
   0x004a2190; differential tests on random states. Done 2026-11-01
   (veh_move.cpp, test_veh_basicmode.py; vehicles.md, The stealth object).
3. **S3** The stealth's controls' frame 0x004a5280 (the player's
   movement commands and the mouse) and 0x004abcf0; differential tests.
   Done 2026-11-01 (veh_stealth: `stealth_controls_frame`, `input_curve`;
   test_veh_stealth.py's frames).
   (The basic vehicle's own controls and their path modes, which an
   earlier reading took for the stealth's, wait: vehicles.md.)
4. **S4** The entry 0x0040e430 and the exit 0x0040e7f0 for the camera: its
   command map (0x9b, 0x9c), the camera on the stealth and the camera
   mode's save and restore (0x0040f880, 0x0040f890, 0x0040f8b0); the key
   under `--debug-camera` / `debugcamera=1` only (Shift+F12: the default
   keymap binds F11), no keys of ours
   besides, the mission running on as in the original. The panel and its
   main menu (0x0040e270, 0x0040e6a0, 0x0040e7b0, 0x0040e7d0, the frame
   0x0040e4b0) too if the row object draws with primitives already ported:
   EDIT AI NETS and EDIT VEHICLES then say "not ported yet" in the log only,
   EXIT works; if the panel needs new HUD primitives it waits, and the key
   toggles the camera alone. Test: the key over c1m1 puts the view on the
   stealth and gives it back; without the flag, nothing.
   Done 2026-11-01 (mech3/game_editor; the stealth's frame 0x004a5050 and
   reticle 0x004a5a00 in veh_stealth, test_veh_stealth.py; the view's
   target any vehicle, as the original's: player_view, view.md). Tests:
   test_editor.cpp (the entry's and exit's calls in order, the main menu's
   rows, the hovered row lit, a press on each kind of row) and
   mission_c1m1_game_editor_on_shift_f12_headless. In that test, with
   THROTTLE_UP held for a second, the stealth flies 20.6 m and the pilot's
   mech under it moves 0.30 m, as it does with the key and no editor (0.00 m
   with no key): **the mech drives too**, as in the original. The player's
   tick (0x004a1160) runs the pilot's vehicle every tick and the stealth's
   frame besides while the view is on it, and the editor's command map is
   a copy of the game's, so the same movement commands reach both. Kept.
   **Ours (debug only):** the exit puts the view back on the pilot's mech
   (its 0x0041ce30), as the mech swap does; the original's leaves it on the
   stealth (above). The panel's rows draw through zTwoD's FontText with
   the font "mech_string" (the HUD's own). The pick under the reticle
   (0x004a58d0) and the readout of the movement mode (0x004a56b0) are not
   ported: the first is read only by the editors (S5 on), the second writes
   into the debug lines, which the release never shows.

**After Pirate's Moon is finished (Owen, 2026-11-05):** MW3's version on `main`,
PM's differences on `pm` (its zainetedit.cpp changed: 35 changed functions, 85
new):

5. **S5, S6** The AI net editor: 0x00402d60, 0x0040e120, 0x00403690, the
   handlers above, the tree view; Save Changes to the mod's copy of the
   mission's archive (FIX (A); the map above), saying on screen where it saved; typed fields (names, numbers) bounded
   like any input.
6. **S7** The vehicle editor mapped (0x004b80f0's object, its screens and
   what each control edits and writes, to which file; every save to the
   write dir), then its own sittings (likely 3 to 5), named from that map.
7. **S8** The "GameZ Interpreter" console 0x5960a0, with its log callback
   0x596130, its line runner 0x596190 and its toggle 0x4810d0 (`interp.md`,
   Hidden debug console). The exe's only way in, action 0xa3, is likely
   unreachable, so the key is ours, under a debug flag as the camera's is.

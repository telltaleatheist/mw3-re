# The Battlesport game editor and the stealth object

MW3 1.2 still carries an in-game editor titled for a game called Battlesport:
a mouse menu headed "BATTLESPORT GAME EDITOR" over a mission,
with the view on the stealth object, an AI net editor and a vehicle editor.
Nothing in the release reaches it: it opens on debug command 0x9a, which no
key is bound to (0x0040db60 registers only 0x61, 0x70, 0x7e and 0x6c to the
debug handler 0x0040dba0). Mapped 2026-10-31 (dev 2); the entry, exit,
panel, main menu and the view on the stealth ported 2026-11-01 (S4, below;
the AI net and vehicle editors behind its rows come after v1.0). The
port first reached it with Shift+F12 under `--debug-camera` only, as Owen
asked: "exactly what they did", a developer's tool. Since 2026-11-09 players
reach it only from the MODS screen (The way in, below); `--debug-camera`
stays a developer's command-line flag that no player reaches (mw3.cfg's
`debugcamera=1` was dropped then). (F11 is the lancemate command screen in the original's default keys,
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

The Windows dialogs are another matter (2026-11-07, read from each exe's resources). `Mech3.exe` 1.2 holds 38 dialog templates, 17 in Recoil's:
both have AI Network Editor (139), Net Edit (140), Node Edit - Vehicle (141), Map Area
Editor (142), Vehicle Select (143), Camera Properties (106), Teleport (132). MW3 adds Mech Editor (149),
Select Mech (156), Global Light Properties (160), Mech AI (165) with pages p1, p2, p5, Attack, Retreat, Fire,
Valve, Zig Zag, Circle, Jump, Direct and Random Move (166-176, 183), Node Parameters (180), a second Ai
Network Editor (181), Mech Animation Transitions (182) and Enter Cheat Code (138). Pirate's Moon's exe
keeps only Teleport (132) and one dialog of its own (188), though its menus' strings are there.

**Which of them code still opens** (2026-11-07, dev 2; every `CDialog`
constructor and `Create` in the exe, and every push of a template's id; the
exe has no `DialogBoxParam` or `CreateDialogParam`). "No key" means the
debug command is bound to nothing (`keybindings.md`), as with 0x9a itself.
The debug menu (resource 147, 0x00482d10) is taken off the window at engine
start (0x00480570), before anyone can use it, and resources 148 and 149 have
no load found as menus.

| Status | Template | Way in |
|---|---|---|
| Reachable in the release | Select Video Device (162) | 0x0047fe10 at start-up, when full screen and the saved device says ask (999) |
| Code, no way in | Vehicle Select (143) | command 0x8a, no key |
| | Camera Properties (106) | command 0x8f, no key |
| | Teleport (132) | command 0x95, no key (0x004a37a0 -> 0x0040dfe0; it writes location.zrd) |
| | Select Mech (156) | command 0x84, no key; message id 0x9c65, in no menu |
| | About (103) | the debug menu's Help (0x6b, 0x00483260) |
| | Mech AI (165), its 12 pages p1..Random Move (166-176, 183) | EDIT VEHICLES (0x004040c0 -> 0x004b86e0) and command 0x9d, no key |
| | AI Network Editor (139) | EDIT AI NETS: the pick 0x0040e120 -> 0x004a4c20 |
| | Node Edit - Vehicle (141) | Add veh to node, 0x00403820 -> 0x0049d350 |
| | Node Parameters (180) | Node Params; node slot +0x90, 0x004c7190 -> 0x004c7840 |
| | Ai Network Editor (181) | Change Net Params, 0x004bd330 -> 0x004bf670; also the Mech AI dialog |
| | the tree (155, untitled) | Tree View, 0x00402bf0 -> 0x004b5580 |
| Template only | Controls (118), Graphics and Sound (119), Configure Controls (134), Multi Player Missions (136), Enter Cheat Code (138), Net Edit (140), Map Area Editor (142), New Game (146), Mech Editor (149), the untitled 152, 154, 179, Global Light Properties (160), Mech Animation Transitions (182) | nothing constructs them |

So 1 is live, 23 have working code behind no way in, and 14 are only
templates. Everything behind 0x9a is reachable in the port through Shift+F12;
the other debug commands are not. A template with no code has nothing to
port: it shows what the tools once did, no more.

**zStudio** (github.com/Vorta/zStudio, MIT, a map editor for Recoil with MW3
1.2 source projects; docs read 2026-11-07, not its code; nothing taken). Its
project keeps nets as text in `data/<campaign>/zrdr/aipath/m1`, left at
version 106, and builds that folder into `readerm1.zbd`, one archive to one
mission, as our save does. It rebuilds every member, so an export is
not byte-identical to the shipped archive, and it writes to a separate
folder. It does not clash with our save, which works on whichever archive is
loaded: drop its export in a mod and the editor saves over the mod's own
copy. Two checks when the save exists: open a zStudio-built archive, and
keep every other member's table entry as it stands (name, comment, time).
Its docs put a project path in a member's name for Recoil and don't say so
for MW3, and our lookup matches the name alone. Its MW3 map editing is
deferred and PM is not supported; world editing is not ours to duplicate.

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

### The pointer (ours, 2026-11-11; Owen's playtest: "there's no cursor")

The exe draws no pointer in the editor: the lit row is the pointer. The
mouse is held as in any mission and its counts move the game's mouse
(0x0051b910), so the row under that unseen point lights and takes the
click (measured in the program with SDL's own motion events and no fix:
the row followed, a click on EDIT AI NETS opened its dialog, "the mouse
held, the system's cursor hidden"). A player sees rows and nothing to
point with, and the dialogs over the frame get no cursor at all.

Ours, while the editor is up in a mission (`editor_state().active`):

- the mouse is let go, in a window and fullscreen, the system's cursor is
  shown, and zInput's mouse is put where the cursor is each frame
  (`SdlInput::pointer_free`, the menus' absolute path, 0x0051b4f0), so the
  point the row and the click read is the point the player sees, at any
  window size and shape;
- the game does not move the system's cursor (`set_cursor_pos` does
  nothing then).

The system's cursor rather than the game's menu cursor drawn at the
editor's point: the ImGui dialogs are drawn over the frame and need the
real cursor, and the net editor's picks in the world read the same game
mouse as the panel, so all three agree. The pause over the editor and the
mission after it follow the old rules (held in a mission). The ported
code is unchanged.

`--mouse-at X,Y@N[c]` (ours) moves the system's mouse to a window point at
frame N as SDL reports it, through SdlInput (c: and clicks); `--click-at`
sets the game's mouse and cannot show whether the mouse is held. With it
the program writes "mw3: editor: frame N: up, row R under the mouse (x, y),
D dialogs, the mouse free|held, the system's cursor shown|hidden" on each
change. Tests: sdl_editor_frees_the_mouse,
sdl_program_editor_pointer_points_and_clicks (4:3 and 16:9).

### The main menu (0x005e0d30, 8 rows; 0x0040e7d0)

| Row | Text | Handler | What it does |
|---|---|---|---|
| 0 | PICK A COMMAND | none | the header |
| 1 | EDIT AI NETS | 0x00402d60 | the AI net editor (below) |
| 2 | EDIT VEHICLES | 0x004040c0 -> 0x004b86e0 | the vehicle editor: a 0x988-byte object (0x004b80f0, vtables 0x005c9898, 0x005c97c0), [0x0060b310]; also debug command 0x9d's |
| 3 | EDIT WEAPONS | 0x004bf620 | the shared empty function: nothing in MW3; ours: New Weapon (1a below) |
| 4..6 | (blank) | none | |
| 7 | EXIT GAME EDIT | 0x0040e7f0 | the exit above |

### The AI net editor (0x005e0f18, 13 rows; installed by 0x00403690)

0x00402d60 first asks for a net in the AI Network Editor dialog (0x0040e120
-> 0x004a4c20, template 139; corrected 2026-11-09, sitting 1 read it as a
pick under the mouse), then installs the menu "AI NET EDITOR" in mode 1. It is the editing side of `CZAiNetEdit`
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

**The flow.** 0x00402d60 opens the AI Network Editor dialog (0x0040e120:
0x004a4c20 builds it over the base 0x004d5530, vtable 0x005c8388; 0x004d55c0
is `CDialog::DoModal`). Template 139, 186x63: "Select a network to edit:", a
drop-down list (1072, sorted: CBS_SORT), OK and Cancel. Its init (0x004a4cd0)
adds every net of the list at [0x00603fdc] by name (vt+0x28 the net, vt+0x4c
its name; CB_ADDSTRING, the net as the item's data), then inserts "New Net"
at the top with no net (0x005e8340, CB_INSERTSTRING 0) and selects it. Its
only message, WM_DESTROY (0x004a4d90), keeps the selection's index (+0x64)
and net (+0x68, none for New Net or no selection). Any close counts:
0x0040e120 takes the selection when DoModal returns non-zero, so **Cancel
opens the selected net too**. A net that is not compiled (vt+0x48) is
edited through 0x004bd360 by its id (vt+0x18); New Net takes the camera's
ground pick (vt+0xf8), a free id from 0x0058b1c0 and makes a net with one
node there (0x00402c90). It frees the old editor,
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
Decided 2026-11-07: the first, marked ours in fixes.md and CHANGES.md, inside
the editor only; the 25 functions stay unported and listed here.

Done (sitting 4, 2026-11-09): Save Changes on a running net (version 106,
built) compiles it again from the edit (`build_compiled`, 0x004be300) and
the status adds "running now". `net_hooks().rebuilt` is called twice: before
the old net is dropped, every AI mech's and vehicle's brain that has an order
on it lets go (0x004e9100, the controls' vt+0x64) and is kept
(`program_ai_brains()`, standing in for [0x0073655c]); after the build each
kept one takes the new net (vt+0x04, 0x004e9380). The built net is the
CZAiNetEdit's and outlives the editor (its dtor, 0x004bd020, does not free
it), so leaving the editor leaves the vehicles on it.
Waynets and regions are not rebuilt; their edits show at the next start.
Test: `mission_c1m1_saved_net_hands_its_vehicles_over_headless` (two
brains on net_01 before, both on the new net after, 90 frames on); with no
hook, or no vt+0x04, the mission crashes in the next brain frame.

## The way in (S5 sitting 3a, 2026-11-09)

Ported in `mech3/game_editor_net` (EDIT AI NETS on the main menu calls it):

- **The dialog** (template 139; FIX (A), ours): a form drawn by ImGui
  (S7b), not an MFC dialog, and not modal (the mission runs on, as under the
  editor's menus). Its controls are the template's: the title, "Select a
  network to edit:", the list, OK and Cancel. The list is znet's registry by name,
  sorted case-insensitively (the drop-down's CBS_SORT; Windows sorts with
  CompareString, which may order punctuation otherwise, but no shipped net
  name differs by it), "New Net" at the top and selected. A click in the
  list selects; OK and Cancel both open the selection, as the exe's do.
- **Opening** (the rest of 0x00402d60): a net that is not an edit (vt+0x48)
  is edited through a new one of its id (0x004bd360: `CZAiNetEdit(id, none,
  0)`, the editing side, which takes its own id from 400..499); the old
  editor freed, the new one kept (`net_editor()`, [0x005f4c6c]), its menu
  installed (0x00403690: mode 1, 0x005e0f18; the title and the net's name
  passed and unread). New Net (0x00402c90, the new net's first node) is
  sitting 4's: said in the log, the main menu back.
- **The editor object** (0x00402f40): the net it owns (deleted with it, as
  0x00402fd0 does), the camera's far clip kept and put back, dirty and
  rebuild; its selection base, the net view, came with 3b (below).
- **GOTO Net** (0x004036c0): the view's vehicle (the stealth) put by its
  vt+0xdc (0x004a3630, yaw 0) at the net's place, its first node's (vt+0x04
  0x00402b20; a net without nodes gives the placeholder's, 0 0 0, as the exe).
- **Save Changes** (0x00402e90 -> 0x00402f20 -> 0x004bd340): no editor, "no
  network to save" (a box in the exe, 0x00402ec0; ours the status line);
  else the net's file found in the first open archive that holds it (the
  reader's order), the file object's edited records (its cache, which
  sitting 4 fills) put in by `net_with_edits`, and `save_archive_edit` (The
  save, below); its folder and "on from the next start" in the panel's last
  row (`editor_status`, ours, and the log). Dirty cleared.
- **Exit Net Editor** (0x00402ba0): the editor freed, the main menu. The
  side windows it also closes (the tree view, the node window) are sitting
  4's. The rows still to come say "not ported yet" in the log.
- The panel's frame ignores a shown row past the menu's (the status line;
  in the exe no row past the menu's is ever shown).

Tests: `game_editor_net_dialog_and_menu` (the list sorted under New Net, a
row selecting, Cancel opening the selection, the 13 rows, GOTO's place,
Save's line, Exit, New Net); `mission_c1m1_net_editor_saves_into_its_own_mod_headless`
(c1m1: the editor entered, net_01 opened from the dialog, the stealth put on
node_00, Save Changes writing readerm1.zbd to "editor edits", byte for byte
the install's with no edits, turned on, the marker there, the panel saying
where). Breaks: the list unsorted, Cancel cancelling, GOTO at the second
node, the save skipped.

## The net view and the frame (S5 sitting 3b, 2026-11-09)

The net editor's object holds, at +0x18, the **net view** (the "selection
base" 0x0058bf50, 0x110 bytes): a zTwoD object in an unnamed module at
0x0058b3c0..0x0058f8a0 right after znet_object, ported as
`gamez/znetwork/znet_view` (named after it). It knows the net only as a
numbered object and its elements through their virtuals (`ViewHooks`; the
AI net's nodes answer in `game_editor_net`), so it is engine code, not AI.

- **What it holds**: a *mark* for each node (0x74 bytes: the node, its point
  in view space and on screen, a size and a circle object), a *link* for each
  pair (0x120 bytes: kind 1 one way, 3 both ways, made once by the lower
  node; a link from a higher node is made only when it has none back; a node
  linked to itself makes none), a *cursor* mark at the pick point, the
  cursor's *lines* to the picked marks (kind 2), and its own zTwoD window
  (0x005a2b50(4, 4)) that the circles and the links' arrow objects join in
  the order made: the cursor, then each node's circle (only when the node is
  placed, its kind 0) and the links it starts.
- **A mark's point** is worked out lazily in steps (+0x28): 1 through the
  top of the matrix stack (the camera's view matrix, the frame loads it),
  2 within the line clip's depth or 5 not, 3 projected (0x005234f0) with its
  size (1/z times the clip's height, [0x00730804]) and a radius within 8 and
  an eighth of the height, 4 inside the clip's box or 6 not. A circle shows
  (0x0058ca90) only when it fits inside the clip whole, at the point rounded
  half up; its radius drawn and hit is the mark's radius times its ring
  (0.75, [0x005f2a60]).
- **A link on screen** (0x0058ba40) by its ends' steps: both on screen, the
  line as is; one or both off the box but in depth, cut to the box
  (0x0059b490); an end out of depth, cut to the depth in view space first
  (0x0059b370, the near ends moved before the far ones) and then projected.
  Shorter than 2 pixels, or with an end not placed, it is hidden. Each
  link has two arrow pairs: along (0x0058da70, heads 0.75 radii out from the
  start and 1.75 radii back from the end, pointing to the end) for every
  link, and against (0x0058dfd0, the same from the other end) for both-way
  ones. A head is a chevron of three points (0x0058d9a0) that the line draw
  closes into a triangle when its node shows.
- **Colours** (0x00579370, each view sets them): circles red, green under
  the mouse, yellow picked; a link's line green under the mouse, yellow
  pressed, else blue when its ends see each other (the world's segment query
  0x004eed90 between them) and red when not. FIX (bug): the exe reads the
  first byte of an unwritten 0x500-byte buffer for "see each other"
  (docs/fixes.md); here the answer alone decides. The cursor's lines are red
  (its element answers null).
- **The mouse**, through the window each frame (0.1 s): a press on a circle
  picks it (lit, a cursor line to it, `picks`); a release drops the pick
  unless left shift is held (then the circle stays pressed); shift-clicking
  a circle other than the last pressed drops its pick; a plain left-up over
  the cursor (the pick point, under the mouse) hands the picks over to be
  linked (+0xfc). Hovering marks a node (+0x04 bit 0, a list at +0x108);
  pressing a link's arrow keeps it in a list (+0xf0) nothing reads yet.
- **The editor's frame** (0x00403160, from the step 0x00403080 each frame
  in the net menus): the player view's camera set again (0x004b51f0), the
  line clip set to the screen (its size less 0.9375, [0x005bb930]) and the
  camera's near and far planes, the view matrix loaded, the pick point read
  from the view's vehicle (vt+0xf8: the stealth's reticle pick, below).
  **Looking down 70 degrees or more** (the camera's pitch, [0x005bb938]) the
  far clip is pulled in to 1.5 times the camera's distance to the pick (its
  x, y and z differences added); flatter, the far clip kept at the start
  comes back. Then Move Node's states, the view (0x0058c950), and the
  links: each mark handed over is linked (0x004c6ec0) to the first pick, and
  the editor is made anew on the same net and dirty flag (the step frames it
  again at once). With nothing picked the exe would make a new node at the
  pick (0x004bd1f0); that is sitting 4's (in the log).
- **Move Node** (0x00403ac0 -> 0x00403a90; "Select a node first" without a
  pick, "Using first of N selected nodes" with more, both boxes in the exe,
  the status line here): the first pick follows the pick point. It waits for
  the mouse to leave the node (state 1), then to come back (2, its clicks
  cleared), then (3) puts the node at the pick each frame (0x004c71a0, the
  links stored again, dirty) while the mouse stays over it without a click;
  a click or leaving it ends it.
- **The reticle's pick** (0x004a58d0, wired in the program): the reticle's
  point through the world from the camera's near plane to its far one, the
  nearest crossing (0x004eece0), or the far end when nothing is crossed,
  kept on the stealth's overlay (the image's +0xbc) for vt+0xf8.
- **Not yet** (at 3b; the record cache came with 4a, below): a moved or
  linked node was not in the save; the frame's pixel-doubled branch (0x0040d4b0: the
  projection on the half window) is not the port's (no doubling).

Tests: test_net_view.py (a link between two marks against the exe on
random cameras, clips and ends in front, behind, past the far plane and off
screen, all nine pairs of end states: the marks' steps, the cuts, the
direction, both arrow pairs, the circles' place and hits, the back arrows'
hits; bit for bit at 24 bits; at 53 the floats a few ulps apart and 6 of
the 600 cases' drawn points a pixel off, none of their states, flags or
hits); `net_view_links_and_picks` (the links' kinds and makers, the window,
press, release with and without shift, the hand-over, shift-click);
`net_editor_frame_far_clip_move_and_link` (the far clip both ways, Move
Node's states, a link and the editor made anew, add_link's two cases);
`mission_c1m1_net_editor_view_move_and_link_headless` (net_01's 19 nodes
and 22 links, the reticle's pick on the ground 141 m off, node_00 moved
there, node_01 linked to node_00 and the editor made anew, the far clip
back after). Breaks: the depth cut's order, the heads rounded without the
half, the hit by the radius alone, the pick ignoring the crossing, no link
made, the clicks kept at state 2, the far clip at 1.25, links to oneself.

## The record cache (S5 sitting 4a, 2026-11-09)

What makes an edit reach Save Changes. A node's setters (its place
0x004c71a0, a link 0x004c6fc0, so a move's relink too) fold the node
(0x004befc0 -> 0x004bef80): only while editing and never the placeholder,
the edit made dirty and its record handed to its file's cache by key
`node_NN` of its number (0x004c72f0 -> 0x004c72d0; 0x004dc2b0, ported as
`NetFile::edit`):

- not cached yet: kept as a copy when the file has no such key or the
  record differs from the file's (the compare 0x004c6680, `NetNodeRec::same`);
- cached: when it differs from the copy, the copy takes it, unless it is
  the file's own again, when the copy goes (so an edit undone by hand is no
  edit);
- none (0x004ddc90): the copy goes and a null record marks the key gone;
  the save leaves a gone key and its value out (`net_with_edits`).

The reader (0x004de730 / 0x004de830) gives a cached record before the
file's, so the editor's view and a later read see the edit. Save Changes
first runs the save's fold 0x004bd100 (`CZAiNetEdit::fold_edits`): the
file's header made the edit's, every node folded, a file record whose node
is now the placeholder marked gone, dirty when the file then differs from
what it read (0x004da100: anything cached, or the header not as read; no
tree counts as differing).

Two departures, both FIX (bug) (docs/fixes.md), neither in play: the exe's
compare leaves out wait_time and the valve, so an edit of only those was
thrown away as "the file's own"; and its copy is a new record assigned
(0x004da790, 0x004c63a0), which drops the auxname, so every saved edit lost
it. Here both are compared and a copy keeps the auxname.

Tests: test_net_cache.py (the compare against the exe on 3000 random record
pairs a field apart, every field, names in other buffers; the 134 where the
fix departs counted; three breaks); `net_record_cache_keeps_edits_until_they_are_the_files_again`
(c1's net_01: the file's own not kept, a change kept with its auxname and read
back through the reader, undone it goes, a new key kept, a gone key dropped
by the save, the setters folding, the save's fold marking a node gone);
`mission_c1m1_net_editor_view_move_and_link_headless` now saves: node_00's
move and node_01's link read back from the editor mod's archive, node_02 as
the install has it. Breaks: an undone edit kept, the place not folding.

## Deleting, New Net, a new node (S5 sitting 4b, 2026-11-09)

- **Delete Node** (0x00403ae0): the first pick ("Using first of N selected
  nodes"), or else the first picked line (0x004037b0: an arrow pressed;
  "Using first of N selected lines"; a cursor line's ends the other way
  round). No net: "Select a node or line first". The exe asks with an
  OK/Cancel box, "Confirm node deletion" / "Confirm line deletion"; FIX (A):
  the same question as a modal form with OK and Cancel (S7b; from 4b until
  S7b the status line asked and a second Delete Node confirmed). OK deletes
  the node or line only when it is still the one picked.
  Then dirty, and the editor made anew when something went.
  - A node (0x004bd290 with 0x00403c00): never the net's only node or the
    placeholder; its own links taken off (each to a real node), then every
    node's link to it, its slot back to the placeholder, its key marked
    gone in the cache (the save leaves it out), the node freed. Numbers do
    not move: the other nodes keep theirs, and a later new node takes the
    free slot.
  - A line (0x004bd310): the link from the first end to the second taken
    off (0x004c7020 -> the target's 0x004c7080 -> 0x004c70a0: erased, the
    node folded).
- **A new node** (0x004bd1f0 with none, 0x004c6e60): a node of the
  defaults (0x004c6da0, which our port had taken for the placeholder's own
  constructor; the placeholder is that node with another vtable, 0x005ca388)
  into the first placeholder slot, or after the last, numbered by its slot,
  folded. The frame makes one at the pick when marks are handed over with
  nothing picked; the release's mouse never gets there (a hand-over copies
  the picks).
- **New Net** (0x00402c90, the dialog's first row): at the view vehicle's
  pick, numbered by the first free id from 1 (0x0058b1c0(1, 1, 100)), a net
  `CZAiNetEdit(id, none, 0)` with a new node at the pick, an editor on it,
  and the node's mark pressed (0x0058ebc0) so it starts picked.
- **Saving a new net** (ours, FIX (A) with the save): the game finds a
  mission's nets by number, each start trying net_01 .. net_99 by name in the
  open archives (0x00402170 -> 0x00402240); no list names them. So a net
  whose file read nothing goes in as a new `net_NN.zrd` entry (version 106,
  its name, its nodes) of the first open archive holding a net_NN.zrd, the
  mission's, through the same editor mod as any save; it builds from the
  next start.

Tests: `net_editor_delete_node_line_and_new_node` (a three-node net: nothing
picked does nothing, the first press asks, the second deletes node 1 and
every link to and from it, the editor made anew, a new node takes slot 1, a
picked line taken off the same way, a net's only node kept);
`mission_c1m1_new_net_saved_and_loaded_next_start_headless` (New Net at the
stealth's pick, a second node from the frame's hand-over, Save, and the next
start on the same write folder, the editor mod mounted as the program
mounts it, builds net_07 with both nodes, node_00 where it was put). Breaks:
no new archive entry, New Net's node left at 0 0 0, the links to a deleted
node kept.

## The Ai Network Editor window (S5 sitting 4c, 2026-11-09)

Change Net Params (0x004036f0 -> the net's vt+0x5c, 0x004bd330 ->
0x004bf670) opens template 181, "Ai Network Editor": no parameters at all,
only OK, Cancel and Open. The window (0x68 bytes, [0x0060b31c], made once,
modeless) hosts an editor slot of its own at +0x64 (0x004043c0 puts it on
the list 0x00404140 holds at [0x005f4c60], the main editor's slot first).

- **Opening it** runs 0x004bf650 with the caller's ECX, which 0x004bd330
  zeroed: 0x00404570 then frees whatever the window showed and opens
  nothing (an earlier reading took that argument for the net passed as a
  file name; it is null).
- **Open** (message map 0x005ca498, id 1281, 0x004bf750): a CFileDialog
  "Zipper Files (*.zrd)"; the path picked goes to 0x004bf640 -> 0x00404570:
  the old editor freed, a net read from that file (0x004bd3d0:
  `CZAiNetEdit(-1, path, 0)`) and an editor on it. FIX (A): the file dialog
  is a form listing every net_ and reg_ file the open archives hold, with
  Open and Cancel.
- **Each frame** 0x00404290 steps every editor on the list (0x00403080) but
  the main one while the editor's menus are up (their own step runs it): so
  the window's net is drawn and picked like the edited one, beside it, with
  no menu of its own. With the menus down, the main editor is stepped there
  too, as in the exe.
- **OK and Cancel** close it (DestroyWindow on a modeless dialog,
  0x004d5610 / 0x004d55f0 -> 0x004bf6f0); its dtor (0x004bf5d0 ->
  0x00404420) frees its editor and takes the slot off the list.
- FIX (A): the window is a form drawn by ImGui (S7b; until then the panel's
  rows), with ours, a line naming the net it shows; the net editor's menu
  stays on the panel under it.

Tests: `net_window_open_and_hosted_editor` (the form, Open's list and
Cancel, a file opened as a second editor that the frame steps apart from the
main one, OK freeing it; with the menus down, the main one stepped);
`mission_c1m1_net_window_shows_a_second_net_headless` (c1m1: Open lists the
archives' net files, net_01.zrd among them, the second listed
opened with its 19 nodes as marks, stepped, OK closing). Breaks: the window's
editor not stepped, Open opening nothing.

## Node Params (S5 sitting 4c; ours, 2026-11-09)

In the release Node Params (0x00403710 -> the node's vt+0x90, 0x004c7190 ->
0x004c7840) opens template 180, "Node Parameters", modeless, with OK and
Cancel and nothing else, and never hands it the node: 0x004c7820 calls
0x004bf620, an empty function. Owen's call (2026-11-09): build it as it was
meant to work, as ours (`upgrades/editor/node_params`).

The first pick's record is a form (S7b: drawn by ImGui; until then the
editor panel's rows, with typed text through zInput): placed (kind 0, its x
y z given), x, y, z, speed, wait interval, wait time (each with a box for
whether it is given), the two can-leave words, jump, arrange nodes' spacing,
hidden, fixed, autonext and visible (each "as the default", yes or no),
backup and shunt (check boxes), name and gate (text fields; empty clears
them). OK puts
the record on the node field by field (the node keeps its own auxname), its
place through set_pos and the links stored again, so the fold
(0x004befc0) takes it to the cache and Save Changes keeps it; the editor is
made anew. Cancel leaves the node as it was. Without a pick: "Select a node
first".

Tests: `node_params_form_edits_the_picked_node` (no pick refused; the
form's fields set as the view sets them: a value given, a tri-state, a
place, a name; OK puts them on the node, Cancel does not, an empty name
clears it); `mission_c1m1_node_params_saved_headless`
(net_01's node_00: speed 0.25 and a gate, OK, Save, both read back from the
editor mod's archive). Breaks: the speed not put on the node, the cycle
skipping "no".

## Add veh to node (S5 sitting 4c; ours, 2026-11-09)

In the release Add veh to node (0x00403820) fills template 141, "Node Edit -
Vehicle" (0x0049d350): a vehicle type (the 19 mechs' names, 0x00484650,
then vehicle.zrd's records), x, y, z from the first pick ("Using first of
N selected nodes" as Move Node says), yaw 0, a Team ID and Friend, Foe or
Other. On OK it builds an aiv record (0x004df6a0) and hands it to 0x004e0540
-> 0x004671d0, which returns 0, so the "Entry for %s on net %d is %s" path,
the reader's reload and the vehicle's making (0x004e0680, 0x0049fce0) never
run: nothing is made or written. Owen's call (2026-11-09): as it was meant
to work, as ours (`upgrades/editor/add_vehicle`).

A mission makes a vehicle at its start for every aiv.zrd record but the
player's and the stealth's (player.cpp, 0x0049fce0): the name gives the type
up to its first '_', the first item is the net it follows, then the place,
the yaw and the team (the shipped records: 1 the player's side, 2 the
enemy's), then tags (params, p2, p6, p8, p10, vlist, pilot ...). So OK here:

- names the vehicle `<type>_NN`, the first number from 01 the entry does not
  have;
- writes its record as the shipped ones are: (net (x y z) yaw team) and then
  the tags of the first record of that type in the mission's aiv.zrd,
  copied whole (its AI's settings), or none when the mission has no such
  vehicle (the reader's defaults);
- makes it now, through the aiv reader (0x004df730) and create_vehicle, as
  the mission's start would;
- saves aiv.zrd with the record added after the last into the editor's mod
  (the same save as a net's, so a later Save Changes keeps it: a save now
  goes on from the last one its folder holds), so it is there from the next
  start too. The status line says what was made and where it was saved.

The form (S7b: drawn by ImGui): the type (a list), x, y, z, yaw, the team
id, and the side, Friend, Foe or Other (the team following it: 1, 2, 0).

Tests: `add_vehicle_form_and_record` (no pick refused; the form's fields
from the node, the side setting the team, Cancel; the record
helpers on an aiv.zrd of ours); `mission_c1m1_add_vehicle_made_and_saved_headless`
(net_01's node_00, a ttruck as a foe: ttruck_03 made at the node, one
vehicle more, and read back from the editor mod's archive through our aiv
reader with net 1, team 2, the node's place and ttruck_01's settings);
test_aiv_add_mech3ax.py (in every MW3 reader archive with an aiv.zrd, a
record of each type its records have added as Add vehicle adds it: 240 in 48
archives read by mech3ax's unzbd exactly as written, after every shipped
record, every other member the same). Breaks: the tags dropped, a wrong team.

## The tree view (S5 sitting 4, 2026-11-09)

Tree View (0x00402c70 -> 0x00402bf0) makes, once, a modeless window of
template 155 (an untitled "Dialog": a tree control, OK, Cancel; 0x004b5580,
0xa8 bytes, [0x005f4c70]). It shows the world, not the net: on its
OnInitDialog (0x004b56b0) the world node ([0x00608c84] +4) and its children,
each with theirs (0x004b57d0), then under the world an "area R C" item for
each partition row and column (0x005e8858) holding that cell's nodes. It
only shows: nothing in it can be changed, and OK and Cancel close it
(DestroyWindow, then 0x004b5830 deletes it and clears the global). Exit Net
Editor closes it too (0x00402ba0, the window's vt+0x60; it also closes
[0x005f4c74], which nothing in the exe sets).

FIX (A): a form drawn by ImGui with a tree control, OK and Cancel (S7b;
until then the editor panel's rows, paged). `tree_rows` lists the items
shown, two spaces a level, "+ " closed with children, "- " open, for the
tests.
Ported in `mech3/game_editor_tree`; the world comes through the editor's
hooks (`world_node`).

Test: `mission_c1m1_tree_view_shows_the_world_headless` (world1 first,
closed; opened, its 16 children and 418 area items; an area opened shows
its node right under it; OK closes it). Breaks: no area
items, a press not opening.

## The save (S5 sitting 2, 2026-11-09)

**The text writers, ported** (`zrdrainet.cpp`): the header 0x4da1d0 ->
0x4dad10 (105: no netparams; 107 and 108: netparams first and the file's base
name when none was read; the rest name then netparams) and a node, slot +0x14
0x4da7e0 -> 0x4db8d0(record, FILE*, key, file). Version 105 ("BS style")
keeps three links padded with -1 and reports more (0x311); the rest writes
the node line, then each property that differs from the defaults at
0x5e8af4 (plain fields up to +0x9a; the owned ones after it are other data)
on its own line indented 14 spaces, then `\n<indent>` when any property was
written, then ` )`. Floats compare by fcomp's C3, so a NaN counts as the
default. In the order written: name, speed, wait_interval, wait_time, valve,
canleave, jump, scan_aim, scan_time, scan_hit, hidden, fixed, autonext,
visible, deploy, mcbwait, arrangenodes, backup, gate, shunt, valveunion,
valve_assign, auxname. **It never writes `sound`**, and it writes each
valveunion item as `( 1 "name", count )` with a comma; the `name` it writes
is the localized one. FIX (bug): deploy's, mcbwait's and valve_assign's valve
names are read through their pointers whatever they hold; a null one crashed
it, and the port leaves that line out (fixes.md). Test: test_ai_netwrite.py
(3000 random records and 600 headers against the exe, its fprintf and
sprintf a formatter of the test's own; every line kind, every version).

**The binary save, ours** (`net_save.cpp`). The release's save never reaches
the disk (above), so Save Changes writes the mission's archive as an editor
mod's copy:

- *The net*: the entry's bytes walked as zrd nodes (`net_with_edits`); an
  unedited key and value copied byte for byte, an edited record encoded
  (`put_record`) and new keys appended. A record is encoded with exact values
  (floats as their 32 bits, which is how the reader takes every number), its
  properties as the text writer picks them, plus what the text writer drops:
  the sound (its name from the node as read) and the written name (the
  record keeps the localized one: the read one is kept while it still
  localizes to it), valveunion last (the reader finds keys anywhere in the
  value), a missing valve as an Int 0 (which the reader takes for no name).
- *The archive* (`archive_with_entry`): the entries in their order, each
  other one's bytes, name, comment and file time as they were, the table and
  version 1 after them.
- *Where* (Owen's rule through the lead, 2026-11-09; `editor_mod`,
  `save_archive_edit`): never a folder the editor did not make. The archive
  is the one the game finds (a mod's copy first); its edits go into an editor
  mod of their own, `mods/<source mod's name> edits` (`editor edits` for the
  install), made with the marker `mw3-editor.txt` and a mod.txt (`name=<source>
  edits`, the source's `for=`). An existing folder with the marker is reused
  (its archive saved over, the previous save kept as one `.bak`); one without
  it is someone else's, so `<name> (2)` and on (an ADD MOD folder is the
  author's, marker or not). An editor mod's own copy is saved over in place.
  The mod is turned on in mw3.cfg just before its source (the install's:
  after every mod), so it wins from the next start, and the screen says the
  folder and "on from the next start" (the message is made; the net editor's
  menu, which shows it, comes with sitting 3).

Tests: `net_save_every_shipped_net_round_trips` (all 59 MW3 archives
rewritten as they are: the same files; all 751 net, region and waynet files
saved unedited: the same bytes; every node re-encoded from its record: 724
files the same bytes even so, and all 19,950 nodes, a key's first, read back
as the same records); `net_save_edits_read_back_through_the_reader` (c1m1's
net_01.zrd with a node moved to a place %6.1f would round, a speed changed
and a node added, saved into a copy of readerm1.zbd, opened with
zrdr::open_archive and loaded by NetFile: the edits as saved, every other
node and entry as it was); `net_save_into_the_editors_own_mod` (the install's
archive into `editor edits (2)` past a folder of that name not ours, saved
again with the first as .bak, a mod's into `Foo edits` just before it, its
own copy saved over in place, the order kept, the install and the source mod
untouched). test_net_save_mech3ax.py: mech3ax's unzbd reads every saved
archive (53 with nets; unedited, every member the same JSON; re-encoded,
every node's place and links the same; c1m1's edits as saved). Breaks: a
property dropped, a table comment lost, a foreign folder reused, no .bak, the
mod placed last, and (mech3ax) a re-encoded archive given as unedited.

## The way in (Owen, 2026-11-09)

The editor is reached only through the MODS screen:
- **MODS -> EDITOR:** a mod's or custom campaign's mission, opened with the
  editor up. Its saves go into that mod, one .bak kept (below).
- **A custom campaign played from MODS (ENTER):** every mission of it has the
  editor in the level, by Shift+F12 and the pause menu's switch (ours: a
  button under OPTIONS, patch12_edits.cpp, bound only there; The switch,
  below). Its saves go
  into the campaign's mod, one .bak kept, as EDITOR's do (`editor_from_level`
  sets `net_save_into_mod` from the campaign entered). EXIT GAME EDIT goes
  back to the mission.
- **Everywhere else** there is no editor: the original's campaign, Pirate
  Missions, Instant Action, training and multiplayer. Shift+F12 is bound
  but does nothing there, and the pause has no switch (`editor_reachable`,
  mods_edit.h).
- **`--debug-camera`** is a developer's command-line flag and no player
  reaches it. It opens the editor in any mission, saving into "editor
  edits".
- **A mod without a campaign** has no PLAY of its own: its missions are
  opened through EDITOR, editor up.
- **Tests:**
  - `mods_campaign_mission_has_the_editor_headless`: MODS, ENTER on a
    campaign, its first mission. Shift+F12 opens the editor and closes it
    again; a net saved goes into mods\lost\; the pause offers EDIT MISSION,
    which resumes and opens it.
  - `campaign_menus_to_c1m1_through_the_briefing_headless`: from the main
    menu's CAMPAIGN, editor_reachable is false and Shift+F12's command opens
    nothing.

## The way in from MODS (Owen, 2026-11-09; built 2026-11-09)

How a modder reaches the editor (`upgrades/editor/mods_edit`, the MODS
screen's `edit` and `edit_start`, the program's start in app_engine.cpp).

- **The MODS page offers no way in to edit the original campaign**: no row
  for it there. That is the whole rule: nothing stops anyone changing the
  originals who sets out to (no tamper traps, no read-only marks, no refusing
  an edited install; Owen, 2026-11-09). Shift+F12 in play stays as the
  developers left it (`--debug-camera`), its save still going to "editor
  edits" (The save, below), and editing files by hand stays open.
- **New maps are zStudio's job.** Modders bring them in by ADD MOD or by
  dropping folders into `mods\`; the mod system picks them up as today.
- **On the MODS screen** a selected mod or campaign gets PLAY (today's
  ENTER) and EDIT; the EDITOR tab becomes that EDIT.
  1. EDIT lists the mod's missions: its `campaign=` lines' missions, or else
     its worlds (mod_worlds).
  2. Picking one loads it in editor mode: the mod mounted, the view on the
     stealth, the editor's menu open (no Shift+F12), the mission otherwise
     running as under the editor now.
  3. A mod not mounted this run is turned on and the screen says "restart to
     edit it", as ENTER does for a campaign, until the live remount exists.
- **Save Changes in a mod's mission saves into that mod itself**, keeping one
  `.bak` of the file it replaces; for mods this replaces the "<mod> edits"
  side folder (editor_mod). Never into the install.
- **Exit Net Editor / EXIT GAME EDIT return to the MODS screen** when the
  editor came from it (not to the mission). Replaced 2026-11-11: the exit
  goes on with the mission in play (The switch, below).

### The switch (Owen, 2026-11-11, playtest W2)

Owen pressed the pause's EDITOR with the editor up: it closed the editor,
which then left the mission for MODS, and that MODS screen took no clicks
on his PC (not reproduced on the Mac: the hidden program on the same path
comes back to a MODS screen that takes real mouse events). His decision:
one switch between playing and editing, which never leaves the mission.

- The editor's exit, by any way (EXIT GAME EDIT, Shift+F12, the pause's
  button), puts the view back on the mech and the mission goes on. The
  hook that left for MODS (`editor_after_exit`, `mods_edit_leave`, the
  session's `leave`) is gone.
- The pause's button says what a press does: PLAY MISSION with the editor
  up, EDIT MISSION in play (two records at one place in dlgpause.zrd,
  EDITMISSION and PLAYMISSION, one bound: `editor_switch_edits`). A press
  resumes and flips. Where the editor cannot be reached the pause menu is
  the original's.
- MODS comes back when the mission is left (the pause's QUIT) or ends, for
  EDITOR's missions as for TEST's (`mods_menu_reopen` set at the session's
  start for both).
- Tests: `sdl_program_mission_switch_stays_and_mods_comes_back_alive` (the
  real program: the switch down and up in the mission, QUIT, the game
  started again on MODS, a click there answered; `--then` carries the
  script past the restart), `mods_screen_edit_opens_the_editor_and_saves_
  into_the_mod_headless`. The won way stops at the debriefing in a scripted
  run (its Continue takes no scripted click) and is not in the program
  test.
- **Later**: our own dialogs for most things, weapons first (Our modding
  dialogs, planned), in `src/upgrades/`; editing files the old way keeps
  working beside them.

As built: EDITOR (the screen's art names it so; it is EDIT) lists the
selected campaign's missions, each world and number once (`mods_edit_rows`;
a mod without campaign= lines lists its worlds' `zbd\<world>\readerm<N>.zbd`)
in CUSTOM CAMPAIGNS' box; ENTER opens the chosen one, EDITOR or BACK goes
back to the tables. The mission starts as a debug start does (a stand-in
pilot, no briefing: `set_launch`, then the Load state); the HUD's first frame
opens the editor (`editor_enter`, 0x0040e430, as Shift+F12 does, so its
menus and the stealth are the original's); `net_save_into_mod` sends every
save into the mod (its file replaced, one `.bak`, its place in mw3.cfg's
order left as it is). EXIT GAME EDIT asks to leave; the Core state's next
update cleans the mission up (0x00491780) and puts the main menu up, which
opens MODS again; the chosen pilot and the launch come back however the
mission ended (the main menu's enter ends the session). Exit Net Editor goes
back to the editor's main menu, as in the original.

Test: `mods_screen_edit_opens_the_editor_and_saves_into_the_mod_headless`
(a mod with a campaign on c1 mission 1 and its own copy of readerm1.zbd:
EDIT lists one row, ENTER opens the mission with the editor up, node_00 of
net_01 moved and saved into `mods\ed\zbd\c1\readerm1.zbd`, the old file as
`.bak`, no "editor edits" folder, EXIT GAME EDIT back in the mission and
the pause's QUIT back on MODS with the original pilot). Breaks: the save into the side folder, no editor on the
first frame, MODS not opened again.

## Our modding dialogs (planned; a sketch for Owen, 2026-11-09)

Owen's direction: modders never have to edit a file. Each major modding
feature gets a basic dialog in the Battlesport editor (a form drawn by
ImGui, as the net editor's dialogs are since S7b), marked ours, in
`src/upgrades/editor/`. Each writes the same files a modder would write by
hand (mod.txt rows, reader entries, per-def animation JSON, mech3ax JSON)
into the mod being edited, so a file-edited mod and a dialog-made one are
the same thing, and the original reads whatever it would read of either.
Nothing here is built; it is for Owen to look over first.

**Where they live.** On the MODS screen, EDIT on a mod lists, above its
missions, a few rows of ours: NEW WEAPON, NEW MECH VARIANT, EFFECTS AND
ANIMATIONS, CAMPAIGN MISSIONS, PACK THIS MOD. Each opens its dialog in the
editor's panel over a mission of the mod (the first, or c1 mission 1 when it
has none), since the effects want a world to show in. Shift+F12 in play
does not offer them: they write into a mod, and only MODS's EDIT names one.

**The machinery they share** (built already for the net editor):
- a form of rows, one a field: a press cycles a choice or takes typed text
  through zInput's character callback, Enter keeping it, Esc dropping it
  (Node Params, Add vehicle);
- a paged list to pick from (Open's net files, Add vehicle's types);
- the save into the mod itself with one `.bak` of each file it replaces
  (`net_save_into_mod`, `save_archive_edit` with an entry added or replaced,
  `archive_with_entry`), building on its own last save;
- mod.txt read and written as the MODS screen does (`mods_*`), its lines
  kept in their order, a row added or replaced by name;
- the status line saying what was written and where.

### 1. New weapon (Freako's case first)

- **Copy** an existing weapon from a paged list of every weapon the mounted
  mods and the install have (weapons.zrd's BALLISTICS); the new one starts
  as its copy, named `<that>_B` and so on until free.
- **Stats** as a form of the definition's fields as zWeapon reads them
  (weapons.md: damage, heat, range, refire, speed, ammo per ton, the
  ballistics' and beams' own), each typed, with the copy's value shown; the
  row's `ammo=`, `tech=` and `title=`.
- **Effects and sounds**: the muzzle, trail, impact and flyby picked from
  paged lists of the animation defs and sounds the worlds have (anim names,
  zSound's names), each shown on a mark in front of the camera when picked
  (the def run once at the view's pick point).
- **Writes**: `weapon=<name> ammo=.. tech=.. title=..` in mod.txt; the
  definition as an entry of the mod's `reader.zbd` weapons.zrd (added by
  name, as the original's reader takes an extra entry: modding.md, Both
  ways); a new effect def, if one is made from a copy, through 3 below.
- **Then** it appears in the mech lab from the next start, as a hand-made
  row does.

### 1a. New Weapon on EDIT WEAPONS (built 2026-11-10: main fields, then effects and sounds)

Owen's calls (2026-11-09): it opens only from MODS → EDITOR, not by
Shift+F12; the main fields come first, the effects and sounds in a second
sitting. Built in `upgrades/editor/new_weapon` with its tips in
`help_weapons.cpp`.

**The row.** EDIT WEAPONS (the release's empty 0x004bf620; the row's text
and place kept) calls `new_weapon_open`. In a mission the MODS screen's
EDITOR opened (`mods_edit_session().active`) it opens the form. Anywhere
else, a custom campaign's Shift+F12 and the pause's switch included, the
line under the menu says "EDIT WEAPONS writes into a mod: open one from
MODS, EDITOR".

**The form:**
- **copy of:** weapons.zrd's BALLISTICS names that the lab knows
  (`weapon_index`: the exe's 37, the plain PPC and the mods' rows). They are
  read from the mod's own zbd\reader.zbd if it has one (as its last save
  left it), else from the reader.zbd the game finds.
- Picking one fills every field from that definition: the name the first
  free of `<copy>_B` .. `_Z`, `_B2` .. (free of every weapon and every
  definition), the title the same, ammo and tech from the exe's table for
  that weapon.
- **The row:** name (up to 31), ammo, tech (Clan, Inner Sphere, any, none),
  title. OK checks it as mod.txt's reader does (`weapon_row_parse`) and as a
  mount would (`weapon_row_taken`: a name of the exe's, the plain PPC's or a
  row's), and against weapons.zrd's own names. A refusal shows on the form's
  last line, the form staying up and nothing written.
- **The numbers:** damage (DAMAGE), heat (HEAT's first value), range
  (RANGE), fire rate (FIRE_RATE), velocity (VELOCITY, not written for a
  beam, which keeps its own), Clan tons and slots, Inner Sphere tons and
  slots (MASS clan / sphere: only the techs it is offered in are written; a
  copy with one pair offers it for both), rounds a ton (MASS ammo, written
  with ammo). Every other key is the copy's, and a number keeps the copy's
  type (an Int stays an Int). A NAME key, if the copy had one, becomes the
  new name: the game finds a definition by it (0x581ed0).

**OK writes:**
- **The definition** at the end of BALLISTICS
  (`weapons_with_definition`): every byte as it was but BALLISTICS' item
  count, the new name and definition after its last item. It goes through
  the editor's archive save (`save_archive_edit("zbd\\reader.zbd",
  "weapons.zrd")`): into the mod itself, its earlier reader.zbd kept as
  .bak.
- **mod.txt:** `weapon=<name> ammo=.. tech=.. [title=..]`, put in place of a
  weapon= line naming the same weapon (any case), else after the last line.
  Line ends are kept, and the old file is kept as mod.txt.bak.
- A mod with its own loose zbd\weapons.zrd (which wins in a mod): the form
  reads its text (`weapons_text_bytes`: the text reader, then the tree's
  bytes) and OK puts the pair in it as text
  (`weapons_text_with_definition`: after BALLISTICS' last item, found past
  strings and ';' comments, everything else of the modder's kept), checked by
  reading it back, the old text kept as weapons.zrd.bak. No reader.zbd is
  made then.

The lab offers it from the next start (rows are read at the mount).

**Checked:**
- `test_new_weapon.cpp` (mw3tests):
  - the shipped file: 49 definitions, 38 offered; ERPPC's numbers as the
    file has them (6 t / 2 slots Clan, 7 / 3 Inner Sphere);
  - ERPPC_B appended differs in one byte before it, and zWeapon's reader
    (zwep::init) reads it with its numbers, every other definition as shipped;
  - mod.txt's line replaced or appended (CRLF kept);
  - the form under a session: taken and malformed names refused with nothing
    written; OK's mod.txt and reader.zbd; the next start's mount has the row
    (number 38 on, its ammo and techs) and its definition.
- `test_new_weapon.py`: three copies (a PPC, an autocannon with ammo, a beam),
  every number changed, through the original's own reader 0x583880 in
  Unicorn. Each reads 50 definitions with no error and the port the same,
  field for field (test_weapons_zrd's compare). The new one's damage, range,
  period and MASS are in the exe's WeaponType, and the original's lab view
  of its 37 is unchanged. mech3ax reads the install's reader.zbd with it,
  every other member the same and the weapon last. Breaks caught: the count
  not raised, DAMAGE written to the copy, the sphere pair under clan.
- The help coverage test opens it (11 forms).

**Effects and sounds** (the second sitting, 2026-11-10). Eight slots, each a
list of the names the file's definitions use there (`effect_choices`; what
the shipped weapons use is in every world), "(none)" first, the copy's
picked:
- muzzle and firing sound: FIRE's ANIMATION and SOUND;
- projectile, projectile animation, trail and flyby sound: FLYOUT's MODEL,
  MODEL_ANIMATION, ANIMATION and SOUND;
- impact and impact sound: IMPACT default's ANIMATION and SOUND.

A pick replaces the value's first item (a muzzle's second, "follows the
firepoint", kept), "(none)" leaves the key out, and a key the copy lacks is
added. The other soils' impact blocks stay the copy's.

Picking one shows it once where the view points (the editor's ground pick,
else its camera): an animation by `zeff::anim_find` and `anim_start_at`, as
the weapons' own effects start; a sound by `zsnd::find` and `play_at`. A
model is not shown. A name the mission has not loaded (or a sound with the sound off) says so under the menu.

Tests:
- `new_weapon_effects_and_sounds`: ERPPC's eight as the file has them, the
  choices; picks written and read back, the copy's own untouched.
- `new_weapon_loose_text`: the shipped file as text with comments reads
  back to its exact bytes, and the pair put in the text reads as the binary
  append does.
- `new_weapon_form_effects_and_loose_file`: the preview's calls (an
  animation and a sound shown; a model and "(none)" not); OK into a loose
  file, its comment kept, a .bak, no reader.zbd; the next start reads it.
- `test_new_weapon.py`'s fourth case: six picks through the original's
  0x583880, read as the port reads them.

### 2. Mech variant

- **Copy** a mech (the 19 and the mods' rows); name, title, tech, class and
  mass as a form, the mass in steps of 5 as the row requires.
- **Loadouts**: the copy's saved loadouts offered as the variant's start,
  then edited in the mech lab as any mech's are (the lab writes
  `configs\<name>.mw3`); the dialog itself does not rebuild the lab.
- **Writes**: `mech=<name> tech=.. mass=.. class=.. model=<the copy's> title=..`
  in mod.txt, and `dfn_<name>.zrd` in the mod's reader.zbd as the copy's
  definition with the new name (its parts and hard points unchanged; a
  different model is later work: the models are zStudio's).

### 3. Effects and animations

On dev 1's per-def design (modding.md, Animations a def at a time):
- **Pick** a def by world and name from a paged list (`<node>-<anim>`), or
  copy one under a new name.
- **Edit** its events as a list of rows (each event's type and its few
  numbers, its target names), its puffers' and lights' settings as forms;
  played on a mark in the world after each change (the def started, as the
  editor's view already runs a world).
- **Writes**: the def's JSON as unzbd writes it, `zbd\<world>\anim\<node>-<anim>.json`
  for one world or `zbd\anim\<node>-<anim>.json` for all, and its
  scripts' `.zan.json` beside it, into the mod, so mech3ax reads it and the
  per-def loader merges it.

### 4. A campaign's mission list

- **The list**: the mod's campaign file (campaign= in mod.txt; a new one
  made when it has none) shown as its lines in order: op headings and
  missions, each mission its world and number, briefing, movie and bases.
- **Edit**: rows to add a mission (a paged list of the worlds the game and
  the mounted mods have, and their missions), move a line up or down, drop
  it, set its briefing and movie names, add an op heading; the name and
  closing movie as a form.
- **Checks** as the game's own (`campaign_parse`, a world nobody has said at
  once, not at the next start).
- **Writes**: the campaign's `.txt` in the mod and its `campaign=` line, as a
  modder writes them (modding.md, Custom campaigns).

### 5. Pack this mod

- **Checks** it first as `--check-mod` does (its files against the install,
  its rows, its campaigns), saying what fails.
- **Writes** a shareable `.zip` of the mod folder (mod.txt, its files, our
  marker files left out, `.bak` files left out) beside the mods folder, with
  a checksum list inside (modding.md, the planned step 8), the same archive
  ADD MOD takes back in.

**Order, if Owen agrees**: 1 (new weapon), then 4 (campaign list, small),
then 2 (mech variant), then 5 (pack), then 3 (effects, the largest, waiting
on the per-def loader).

## The vehicle editor, the map (S7, 2026-11-09)

EDIT VEHICLES (0x004040c0) and debug command 0x9d both call 0x004b86e0.
When [0x0060b310] is empty, it makes the editor (`new(0x988)`, 0x004b80f0)
and creates it modeless (0x004d5570: `CDialog::Create(165)`). The editor
is the **Mech AI** dialog (template 165). Its pages are child dialogs on a
tab control, so it is not a property sheet. The source file's name is not
in the exe: func2file puts it between weapon.cpp and zainetedit.cpp.
`tools/analysis/dlgdump.py` prints any template's controls.

**The dialog** (165, 232 x 295 dialog units): the vehicle combo 1239, the
tab control 1236, OK, Apply (12321), Refresh (1237), and Pause (1218, a
push-like check box). Its message map is 0x005c9480:

| Control | Handler | What it does |
|---|---|---|
| tab 1236, TCN_SELCHANGE | 0x004b8770 (not a function in Ghidra) | 0x004b7c80: the old page gathered (UpdateData(1)) and hidden, the new one shown and filled |
| Apply | 0x004b8d40 | 0x004b8dc0 on the selection: gather, store 0x004bb7e0, show again |
| combo 1239, CBN_SELCHANGE | 0x004b8d70 | store into the old selection, then load the new one (0x004b8df0 -> 0x004bc140) |
| Pause | 0x004bc3f0 | the check box read; 0x0058fa80 or 0x0058faa0 (the world paused or run) |
| Refresh | 0x004b9f20 (not a function in Ghidra) | to read |
| OK | 0x004b8d20 (vt+0xcc) | Apply, then DestroyWindow (0x004d5610) |
| destroyed | 0x004b8b70 (PostNcDestroy) | 0x004bc3e0, the object deleted, [0x0060b310] cleared |

**OnInitDialog** (0x004b8790) creates the twelve pages:
- On the main tab: p1 (167, +0x4e8), p2 (166, +0x548), Attack (168,
  +0x5b4), Retreat (169, +0x7f4), p5 (170, +0x854), Fire (171, +0x8b4) and
  Valve (172, +0x914).
- On Attack's own tab 1175: Circle (174), Direct (176), Jump (175), Zig Zag
  (173) and Random Move (183).

Each page is added through 0x004b8a20, which inserts a tab with the page's
title and fits the page into the tab's rect. Then OnInitDialog fills the
lists:
- the vehicles from [0x0073655c] (0x004bb290 / 0x004bb200);
- [0x00603fdc] (0x004bc480, combo +0x1f4);
- the 19 chassis' names (0x004bc860 with 0x00484650, combo +0x274);
- the nets (0x004b7fb0: 0x004ba2d0, 0x004ba150, 0x004bafa0, 0x004b9ff0);
- [0x0060b504] (0x004bb300, +0x398) and [0x0060b3f4] (0x004bb620, +0x3d8);
- the Attack strategy's two fixed strings.

It then selects the first vehicle (0x004b8df0) and shows the first page of
each tab. +0x1d1 (Show InOut) is set when the mission's boundary is already
drawn ([0x0060b334], 0x004c4a20 says whether it is empty; S7d). Which list
is which, regions, valves or targets, is S7e's to read.

**The pages**, as made by the constructor 0x004b80f0. Each page has its own
vtable and its DoDataExchange at vt+0x8c:

| Page | Template | Constructor | DDX | Its own handlers |
|---|---|---|---|---|
| p1: activation, leaving the net to follow, follow ranges | 167 | 0x004b8ba0 | 0x004b8bc0 | none |
| p2: the vehicle (configuration, chassis, skin, pilot, objective conditional, can configure, shutdown on start, home net and Net Edit, net id, Show, Show InOut, initial position x y z yaw, team, Send home, lancemate) | 166 | 0x004b8e10 | 0x004b8eb0 | team edit 1111 (EN_CHANGE) 0x004b9270; team radios 1197..1199 0x004b9290; home net 1238 0x004b9f30; Net Edit 1250 0x004b9f50 (window 181); Send home 1255 0x004b9f70; configuration 1273 0x004b9fb0; chassis 1274 0x004b9fd0 |
| Attack: firing accuracy, FCS/TC multiplier, strategy, throttle, its strategy tab | 168 | 0x004b92d0 | 0x004b92f0 | tab 1175 0x004b9e10 |
| Circle: radius | 174 | 0x004b9c10 | 0x004b9c30 | none |
| Direct: approach, minimum range | 176 | 0x004b9c60 | 0x004b9c80 | none |
| Jump: delays, durations | 175 | 0x004b9cd0 | 0x004b9cf0 | none |
| Zig Zag: length, target distance | 173 | 0x004b9d70 | 0x004b9d90 | none |
| Random Move: radius ("Not yet implemented, behaves like Circle.") | 183 | 0x004b9de0 | Circle's 0x004b9c30 | none |
| Retreat: net distance, health, time, health change; the leave-net disable for a timeout or forever | 169 | 0x004b9470 | 0x004b9490 | none |
| p5: the scan trigger (time, hit, aim; initial and set value), scan and jump parameters, quit scan on trigger pull, collision avoidance | 170 | 0x004b9680 | 0x004b96a0 | none |
| Fire: acquisition range, fire time, cooldown, alpha strike | 171 | 0x004b9870 | 0x004b9890 | none |
| Valve: the valve and its value, set on health, ammo, fired on, target distance, heat, region enters or exits, target LOS | 172 | 0x004b9910 | 0x004b9930 | valve combo 1176, CBN_SELCHANGE 0x004b9f80 and CBN_SELENDOK 0x004b9fa0 |

The page titles p1, p2 and p5 are aiv.zrd's record tags (its reader warns
about p3, p4, p5, p7 and p8; docs/ai.md). Which page maps to which key is
for each page's sitting to record.

**The editor's own data:** the dialog's settings at +0x64 (AiParams,
0x004d98c0), and the last loaded record's copy at +0x294. +0x974..+0x984
are five tri-state flags: 2 means "as the vehicle has it".

**Loading** (0x004bc140): the vehicle is the combo's item data; its
controls are vt+0x140 and their brain vt+0x90 (a vehicle without one is
skipped). Its aiv record comes from the cache (0x004e0680 by the vehicle's
id). When it has none, one is made from the brain's settings (0x004df6f0,
0x004df710) and put in the cache (0x004e06e0). The settings are taken into
+0x64 (0x004d99e0), then the pages are filled: 0x004bc260, 0x004044c0(0),
0x004bc570, 0x004bc640, 0x004bc790 and 0x004bc930.

**Storing** (Apply, 0x004bb7e0) builds a record from +0x64 (0x004d9940).
- It compares that record with the cache's, field by field: the names
  (0x0049d2f0, 0x004d91d0), the orders (0x004bbd70, 0x004bbdd0, 0x004bbe60,
  0x004bbed0, 0x004bc010, 0x004bc090, 0x004bc0d0, 0x004bc110), four words
  at +0x50 and two flag bits. When they differ, it puts the record in the
  cache (0x004e06e0 -> 0x004defc0). The cache edit compares with the record
  the cache already keeps for that vehicle (0x004def10), not with the file's,
  and returns 0 when they are the same. A record with no pilot or no voice
  name is never "the same" (0x0049d2f0 calls a null name unequal, even to
  null), so it is always stored again.
- It compares it the same way with the brain's live settings. When those
  differ, it gives them to the brain (0x004d99e0) and rebuilds the team's
  three masks on the vehicle (+0xc0, +0xc4, +0xc8: the team's bits, allies
  without bit 0, enemies the rest), and clears [0x0060b56c].

**The compares, ported** (S7c, 2026-11-09): `AiParams::same_numbers`
(0x004df1b0; the store's 0x004bbd70..0x004bc110 are the same chain in
pieces), `same_vlist` (0x004d9090 with 0x004d9140) and `same_name`
(0x0049d2f0) in `zai_params`, and `AivFile::edit` (0x004defc0) in
`zrdraiv`. A float word is compared the way the x87 does it in the exe: an
unordered result (a NaN on either side) sets the same flag as "equal", and the
compiled test reads only that, so a NaN equals anything. A NaN cannot come from
the dialog's fields, so this never shows. Test: test_aiv_edit.py (3000 pairs
one word, one flag bit, -0 or a NaN apart, vlists of 0..3 entries one field
apart, names; 0 differ). Breaks: a float word as an int, flag bit 0x40 left
out, two null names equal, the entries' valve left out.

**Nothing is saved.** As with the nets, an edit lives only in the aiv
file's record cache (its map at +0x24) and in the running brain. The exe
has no aiv text writer: the only fprintf near ZRdrAiv.cpp is 0x004e2310's
one line. Saving is ours, built on Add vehicle's `aiv_with_record`
(above) into the editor's mod, as the nets' save is.

**Size:** about 110 functions and 3,000 lines of pseudocode, from
0x004b7c80 to 0x004bcb70, with the cache's 0x004defc0 and 0x004df1b0 and
the AiParams copies. 0x004b8770 and 0x004b9f20 need
`tools/ghidra/CreateAndDecompile.java`. Pirate's Moon's differences are to
be counted in S7c, from analysis-output/pm-diff.

**Left to read in the sittings:** whether Show, Show InOut, Enable Now,
Lancemate and the other DDX-only check boxes do anything beyond setting a
field. Anything the release leaves empty or broken gets made to work as
meant, as ours in `src/upgrades/editor/`.

**The shell, built (S7c, 2026-11-09)** in `mech3/game_editor_vehicles`
(EDIT VEHICLES on the main menu):
- **The form:** "Mech AI", with the vehicle list (the combo 1239), the main
  tab's seven pages as tabs (empty until S7d and S7e), Pause, OK, Apply and
  Refresh. Made once ([0x0060b310]).
- **The hooks** (`VehicleEditorHooks`, wired in app_vehicle):
  - the list: zEntity's list in order, each mech (an AI one by its aiv
    record's name, the player's "player") and each made vehicle with AI
    controls (by its node's name, its record's). The exe's own test
    (0x004bb200: each entity's vt+0x2c then vt+0x40; kept when it is a mech,
    vt+0x50, or its controls, vt+0x140, answer vt+0x54; the item's text the
    entity's +0x04 name) is the one the hooks follow.
  - each one's brain's settings (vt+0x140 -> vt+0x90);
  - the masks (+0xc0..+0xc8);
  - the player's allies and enemies;
  - Pause: the app deactivated or activated, 0x0058fa80 / 0x0058faa0.
- **Load** (0x004bc140) and **store** (0x004bb7e0), as mapped above. The
  five "as the brain has it" words: the exe resets each to 2 after reading
  the next, and resets 0x97c twice, so +0x980's is never reset; ported so.
  The team round trip (0x004bc260, through `team_number_of`, 0x004d71a0)
  sets the Team buttons.
- **Units:** a load takes the ranges' square roots (to the file's units) and
  the store squares them again, so the brain's numbers can come back a bit
  apart. Apply then puts the record on the brain even with nothing edited,
  as the exe does.
- **Refresh** (0x004b9e40) finds the shown vehicle again by its name.
- **Ours:** the editor closes when the mission's vehicles are forgotten; the
  exe's dialog outlives them, and its next Apply would reach freed ones.
- **Tests:**
  - `vehicle_editor_shell_loads_and_applies`: stub hooks; the list, the
    first loaded and its record made and cached, the team buttons, Apply's
    team and masks, a pick storing the old one, Refresh by name, Pause, OK.
  - `mission_c1m1_vehicle_editor_applies_to_a_brain_headless`: nine
    listed, "player" among them; mobilehq_01's pilot set to "Tester" and
    Applied, on its brain and in the cache; 60 frames on; OK closes.
  - test_aiv_edit.py adds 0x004d71a0 (3000 masks, 0 differ).
  - Breaks: no assignment to the brain (both tests fail); no cache edit
    (the mission test fails).

### The vehicle editor's pages: p1 and p2 (S7d, 2026-11-09)

**How the dialog exchanges.** Every page is a child dialog with its own
DoDataExchange (vt+0x8c). The main dialog's (0x004b8680) does the team
round trip (0x004bc260) when showing, then only the page shown on the main
tab (+0x4e0), then Pause (1218). Switching tabs (0x004b7c80) gathers the
page leaving and shows the one coming. So a page not shown keeps what it
showed last until it comes back. Apply, a new pick, Pause and Net Edit
gather the page shown first. A float field goes through MFC's text form
("%.6g") whenever its page is gathered: 12.3456789 comes back as 12.3457.

**The MFC stand-in** (`upgrades/editor/ddx`, FIX (A)): MFC's DDX_* and
DDV_* (mfc42.dll, not in the exe) on controls kept as data: an edit's
text, a button's check, a radio group's choice, a combo's items and
selection. A ported DoDataExchange runs on it line for line, and the ImGui
form draws the controls. What it keeps of MFC 4.2 (dlgdata.cpp) and
MSVCRT:
- **Numbers:** a float is shown as "%.6g", with MSVC's three exponent
  digits ("1e+012") and its "1.#INF", "-1.#IND" texts. A float is read
  back with MSVC's strtod, which also takes 'd' for the exponent
  (`crt::msvc_strtod`). Ints use "%d" and "%u" with strtol / strtoul.
- **What MFC refuses:** spaces and tabs are allowed round a number. A 0 read
  from anything but a leading '0' is refused, so "+0", "-0" and ".0" all
  fail. "%u" refuses a leading '-'. The text is cut to 31 characters before
  it is read (MFC's buffer).
- **A field that fails** gets MFC's box ("Please enter a number.", "Please
  enter an integer.", "Please enter a positive integer.", "Please enter no
  more than 32 characters.", "Please enter an integer between 0 and 100.").
  The pass then stops there, as pDX->Fail() throws. The fields before it are
  taken, the rest are not, and the store runs anyway: 0x004b8dc0 ignores
  UpdateData's answer.
- **DDX_Check:** a value outside 0..2 is shown as 0 and set to 0.
  BM_SETCHECK puts a two-state box's 2 at 1.
- **DDV_MaxChars** also limits the edit (EM_LIMITTEXT).
- **CB_SELECTSTRING** finds the first item the text begins, with no case,
  and leaves the selection alone when none does.
- **CBS_SORT** sorts with no case. Windows' word sort, which skips hyphens
  and apostrophes, is not modelled: no shipped name has either.
- **Notifications:** none are sent while a page exchanges. MFC locks its
  notifications out during UpdateData.

The MFC thunks the pages call: DDX_Control 0x005abd86, DDX_Text float
0x005abd80, int 0x005abfea, UINT 0x005abff6, CString 0x005abdc2,
DDX_Check 0x005abdce, DDX_Radio 0x005abff0, DDV_MaxChars 0x005ac04a,
DDV_MinMaxInt 0x005ac044 (Ghidra's names for the last two are swapped),
CString = 0x005abdbc, UpdateData 0x005abd8c.

**p1** (template 167, 0x004b8bc0), as offsets in the settings (+0x64):

| Control | Field |
|---|---|
| Always active, check 1231 | +0x88 |
| the range in Activation, edit 1209 | +0x1c |
| Range to, list 1229 ("player", "nearest enemy") | +0x8c: 1 for player (item 0) |
| Enable Initial, check 1217 | +0x3c, the record's |
| Enable Now, three-state 1257 | the editor's +0x984 (2: as the brain has it) |
| Leave Net Range to Target, 1210 | +0x20 |
| Return to Net Range from Net, 1211 | +0x24 |
| Approach Range to Target, 1212 | +0x28 |
| Minimum Range to Target, 1214 | +0x2c |
| Disable, check 1251 | +0x54 |

**Enable Initial and Enable Now.** Enable Initial goes into the aiv record.
Enable Now goes only on the brain: the store's first "as the brain has it"
word replaces +0x3c on the brain.

**p2** (template 166, 0x004b8eb0):

| Control | Field |
|---|---|
| Team, radios 1197..1199 | the editor's +0x4dc |
| the team, edit 1111 (read only) | +0x18 (UINT) |
| x, y, z, yaw, 1200..1203 | +0x08, +0x0c, +0x10, +0x14 (the yaw in degrees) |
| Net ID, edit 1117 (read only) | +0x04, the home net |
| Pilot Name, 1243 (at most 32) | the page's CString, into +0xd4 (0x0049d250; empty: none) |
| Show, three-state 1249 | the editor's +0x1cc |
| Objective Conditional 1275, Shutdown on start 1276 | +0xcc bits 4 and 5 (through the page's +0x64 / +0x68) |
| can configure 1279 | +0xcc bit 7 |
| Show InOut 1282 | the editor's +0x1d1 |
| Lancemate, three-state 1283 | +0xcd: bit 1 set shows 2; 2 sets bit 1 and clears bit 0, else bit 1 cleared and bit 0 the check |
| Skin 1284 (0..100) | +0xc8 |
| Configuration 1273, Default Chassis 1274, Home Net 1238 | the lists below |

**p2's lists** are filled at load (0x004bc140) and when p2 is shown. They are
read back when p2 is gathered: the chassis (0x004bc8f0), the loadout
(0x004bc750), then the home net (0x004bc4e0), each only when an item is
picked.
- **Home Net** (0x004bc480, at OnInitDialog and Refresh): every object on
  znet's registry by its net's name ("net_02"), its id as the item's data.
- **Configuration** (0x004bc640 at load; shown by 0x004bc790): the
  vehicle's mech's loadouts (+0x684, 0x00432940: its definition's +0x20)
  except the player's own (+0x1c), each by its index. Nothing for a vehicle
  that is not a mech.
- **Default Chassis** (0x004bc860): "default" (-1) and the 19 chassis
  (0x00484650).
- **The chassis shown** (0x004bc930): chosen by name. A number past the 19
  names "notmech", which matches nothing, so the list keeps what it
  showed.
- **The home net shown** (0x004bc570), the same pass: Show's editor goes on
  the net (or is freed), and the boundary is drawn or taken down.

**p2's handlers** (its message map 0x005c9538):
- **The team box** (1111) answers EN_KILLFOCUS (0x004b9270), not EN_CHANGE:
  it is read only, but clicking into it and out again gathers p2, rounds the
  team (0x004bc260) and shows it again.
- **The radios** (0x004b9290) gather p2. When the choice changed, the team
  becomes the first of the player's allies (Friend), enemies (Foe) or
  neither (Other), or -1 when there is none (0x004bc300). With no player
  they do nothing.
- **Home Net, Configuration, Default Chassis** (CBN_SELCHANGE: 0x004b9f30,
  0x004b9fb0, 0x004b9fd0): gather p2, take the pick, show p2 again.
- **Net Edit** (1250, 0x004b9f50): the whole dialog gathered, then window
  181 shown with no file (0x004bf670 with ECX 0). It is the same window as
  Change Net Params, and here it opens even without the net editor.
- **Send home** (1255, 0x004b9f70 -> 0x004bc960): no gather first. The
  vehicle's brain lets go of the home net (the controls' vt+0x64,
  0x004e9100), the vehicle is put at x, y, z and the yaw last exchanged
  (vt+0xdc: 0x00418f30 for a mech, 0x004a3630 for others), and the brain
  takes the net again (vt+0x04, 0x004e9380). Nothing without the net.

**Show** (the editor's +0x1c8) is a second net editor slot, put on the frame's
list by the editor's constructor (0x004043c0) and taken off by its
destructor (0x00404420).
- **The list** ([0x005f4c60]) holds the main editor's slot first (0x00404140),
  then window 181's and Show's, in the order they were made. Each frame
  steps them all (0x00404290).
- **0x004044c0** puts an editor on the home net, on a new CZAiNetEdit of its
  id, unless the slot shows it already or another editor on the list does
  (0x00404270 -> 0x00404200). A null net frees it (0x00404490).

**Show InOut** ([0x0060b334]; 0x004c4950 draws for a name, takes down for
null; the name "ionet" is never read) draws the current mission's
boundary (S7d part 2, `mech3/zainet_inout_view`, the file between
zainetmechinout.cpp and zaitarget.cpp):
- **0x004c4730:** with a world and the areas read (docs/ai.md section 11), the
  area of 0x004c36c0's mission number becomes walls. One object3d is the
  root, with an object3d and a fresh model (0x00538fe0) under it for each run
  of polygons that fits. The root is taken out of the segment queries
  (0x004eb6d0) and joins the world (its vt+0, 0x0040bab0).
- **The walls** (0x004c42e0, 0x004c43b0, 0x004c45f0): only tested
  polygons, red and blue, are drawn, so yellow and cyan never are. Each side
  from the last corner to this one is a wall 10 m high (0x005e8a68) of two
  triangles. Each triangle is added twice (0x005e8a64), red (0x0050de00
  255,0,0) one way and the polygon's colour the other: blue for kinds 0 and
  2, yellow for 1 and 3.
- **A model** takes polygons while its vertex count (3 a corner) stays under
  1024 (0x005e8a6c); the next polygon starts a new one. An add that fails
  does not stop the rest (0x005e8a60 set). The side triangles' flag
  [0x0060b3a0] is never set.
- **0x004c41b0** takes the root off its parents (the world) and deletes it
  with its walls.
- **Fixes**, FIX (bug), in fixes.md:
  - the area index checked;
  - a polygon too big for any model left out (the exe loops for ever making
    empty models for it);
  - the drawing forgotten at the mission's end (0x004c3dd0): the exe keeps
    the pointer past the world.
- **Tests:**
  - test_inout_view.py: 400 runs of random polygons, starting counts and add
    answers against the exe's builder, the colours and triangles recorded on
    both sides. 0 differ.
  - Breaks, each fails: the wall 9 m high; kind 2 yellow; the 1024 test as
    `>`; the second triangle's swap left out.
  - The c1m1 p2 test checks Show InOut too: 1 model, 1156 triangles, under
    the world; taken down when unchecked.
  - Not looked at on screen yet.

**Ported** in `mech3/game_editor_vehicles` and `game_editor_net` (the slot list):
- the pages' controls, p1 and p2 exchanged as above, and the main
  dialog's exchange;
- the tab switch, the fills, gathers and handlers;
- Show's editor;
- the team round trip with the player's presence (`has_player`; before, no
  allies and no enemies stood in for no player).

The form draws each page's controls (`FormField` kCheck for two- and
three-state boxes, kRadio, kButton, kChoice on a combo, kText with a limit,
read only and a focus-lost call).

**Tests:**
- `test_vehicle_ddx.py`: 3000 random passes of p1 and p2, both directions.
  The exe's 0x004b8bc0 / 0x004b8eb0 run in Unicorn with MFC's calls stood in
  for by a Python model of MFC 4.2 on a table of controls, the port on its
  stand-in. Each pass compares the fields, the controls' texts, checks and
  limits, the controls exchanged in order, the list fills called in order,
  and a failing field's control and message. The conversions are also
  checked alone on hand-picked texts. 0 differ.
- Breaks, each fails: x and z swapped (201 of 600); Lancemate's bit 1 kept
  (39 of 3000); no 31-character cut (6 of 600); the skin's top 101 (4 of 600).
- `vehicle_editor_pages_show_and_take_back`, with stub hooks:
  - p1 and p2 filled from the settings, and the lists: nets, loadouts but
    the player's, chassis;
  - typed and picked fields put on the brain at Apply;
  - Friend's team;
  - "-0" stopping the pass with its box, the fields before it taken;
  - Send home;
  - Enable Now on the brain's +0x3c.
- `mission_c1m1_vehicle_editor_p2_shows_the_net_and_sends_home_headless`:
  - mobilehq_01's home net 2 ("net_02") shown;
  - Show and Apply put a second editor on net_02;
  - x and z typed 20 m off, then Apply and Send home: the vehicle is there 30
    frames on, its brain on net 2;
  - Show unchecked frees the editor.

### The vehicle editor's pages: Attack, Retreat, p5, Fire (S7e part 1, 2026-11-09)

**Attack** (template 168, 0x004b92f0) exchanges its own tab's page shown
(+0x4e4) first, in the same direction, then its fields. Its tab 1175 (+0x41c)
switches pages through 0x004b9e10 -> 0x004b7c80. The pages, in the order
0x004b8790 makes them: Circle 174, Direct 176, Jump 175, Zig Zag 173, Random
Move 183. OnInitDialog's second 0x004b7c80 shows Circle.

**The strategies** are a map made at start-up ([0x0060b2f8], 0x004b7f70 ->
0x004b7d80): 0 Direct, 1 Zig Zag, 2 Circle, 3 Jump, 4 Random Move.
- **The list** (1233, sorted) shows them by name, the number as each item's data
  (0x004b7fb0 copies the map and adds each).
- **Showing** looks the settings' +0x40 up with the map's [] (0x004ba380). A number
  with no strategy gets a new entry with no name, so it selects nothing.
- **Reading back** takes the picked item's data when it is 0..4.

Offsets are in the settings (+0x64):

| Page | Control | Field |
|---|---|---|
| Attack | Firing Accuracy 1272 (0..1000, DDV_MinMaxFloat 0x005abd7a), 1274 | +0xb8, +0xc0 |
| Attack | Throttle 1234, 1248 | +0x94, +0x84 |
| Attack | strategy 1233 | +0x40 |
| Circle, Random Move | 1220 (Circle's DDX 0x004b9c30) | +0x44 |
| Direct (0x004b9c80) | 1212, 1214 | +0x28, +0x2c: p1's approach and minimum ranges |
| Jump (0x004b9cf0) | 1223, 1225, 1226, 1227 | +0x64, +0x68, +0x6c, +0x70: p5's four |
| Zig Zag (0x004b9d90) | 1220, 1219 | +0x44 (Circle's radius too), +0x48 |
| Retreat (0x004b9490) | 1213 | +0x50 |
| Retreat | Net Distance 1221, Health 1222, Time 1224, Health Change 1271 | +0xcc bits 0, 2, 1, 3 |
| Retreat | 1211, 1215, 1270 | +0x24 (p1's 1211 too), +0x4c, +0xb4 |
| Retreat | Leavenet Disable: For timeout 1253 with 1254, Forever 1252 | +0x5c, +0x58, +0x60 |
| p5 (0x004b96a0) | Collision Avoidance Enable 1246, 1228, 1230 | +0x80, +0x74, +0x78 |
| p5 | Scan / Jump radios 1258 | +0xa4 |
| p5 | 1247, 1223, 1225, 1226, 1227 | +0x7c, +0x64, +0x68, +0x6c, +0x70 |
| p5 | Scan Trigger, Initial: Time 1244, Hit 1260, Aim 1261 | +0xa0, +0x9c, +0x98 |
| p5 | Set Value, three-state: Time 1245, Hit 1262, Aim 1263 | the editor's +0x97c, +0x978, +0x974 |
| p5 | Quit scan on trigger pull 1278 | +0xcc bit 6 |
| Fire (0x004b9890) | 1145, 1191, 1216, 1232 | +0x30, +0x34, +0x38, +0x90 |

**What these pages show:**
- **The Set Value boxes** are the store's "as the brain has it" words. Each
  goes only on the brain (the store's +0x98, +0x9c, +0xa0); the Initial box
  beside it goes into the record. That leaves +0x980 (+0xcc bit 2) with no box.
- **A field on two pages** is taken from whichever page is gathered last.
  The tab switch gathers the page leaving, so an edit on one shows on the
  other when that one comes up.
- **DDV_MinMaxFloat** (as MFC's _AfxFailMinMaxReal) runs after the field is
  taken. A value past 0..1000 stays in the settings, the box says "Please enter
  a number between 0 and 1000.", and the pass stops there. Attack's strategy
  list, after it, is not read, and the show pass after the store puts the list
  back on the settings' strategy. A NaN passes.

**Tests:**
- **test_vehicle_ddx.py** covers every page now: 3000 passes over p1, p2,
  Attack, Retreat, p5, Fire and the five strategy pages, with the strategy map
  made by the exe's own start-up code (0x004b7f80). 0 differ.
- **Breaks**, each fails:
  - Retreat's Time on bit 0 (131 of 3000);
  - the strategy's top 4 (6);
  - the DDV before the field is taken (28);
  - p5's Aim box on the Hit word (161).
- **vehicle_editor_attack_retreat_p5_fire:**
  - the list and Circle shown first;
  - Direct showing and taking p1's range;
  - the 0..1000 box stopping the pass before the list;
  - the pick shown again from the settings, then taken;
  - Retreat's bits;
  - p5's Set Value box on the brain only.

### The vehicle editor's Valve page (S7e part 2, 2026-11-09)

**The entry.** Valve (template 172, 0x004b9930) shows one "vlist" entry of
the settings (docs/ai.md 17.5): the one whose valve the valve list names (the
editor's +0x3f8, found by 0x004d93d0). With none, it shows the editor's own
spare entry (+0x3fc), which nothing else reads (0x004bb360).

**The valve list** (1176, sorted, typed into):
- **Its items** are every valve by name (0x004bb300 over [0x0060b504]).
- **A pick** is CBN_SELCHANGE (0x004b9f80): the page gathered, then
  0x004bb470, then the page shown again. CBN_SELENDOK (0x004b9fa0 ->
  0x004bb390) does the same unless the item is the valve already shown.
- **0x004bb470** takes the item's name. With no item picked it takes the
  typed text instead: a name other than "0" (0x005e890c) makes the valve
  (0x004d3c40) and fills the list again.
- **An entry** for that valve is added when the settings have none
  (0x004d9580: the defaults 0x005e8da0 on the valve, then 0x004d8e20: ranges
  to the file's units).
- **0x004bb550** then shows the name, and forgets one the list lacks.
- **Ours:** the typed name is sent with Enter in the edit part. Typing clears
  the list's pick; Win32's own matching while typing is not modelled.

**The region list** (1177, sorted, typed into):
- **Its items** are every region (0x004bb620 over [0x0060b3f4]).
- **Read back** (0x004bb680, after the fields): the picked item's region by
  name (0x004c5d20). Else a typed name other than "0" finds the region or
  reads it from its file (0x004c5c00) and fills the list again. Else none.
- **Shown** (0x004bb780): the entry's region, by name (vt+0x1c). A region the
  list lacks is dropped from the entry.

| Control | Entry field |
|---|---|
| close (zero) / open (one), radios 1178 | flags bit 1 |
| Active 1256 | flags bit 0 |
| Ammo 1181 with 1193 | bit 2, +0x00 |
| Fired on 1182 | bit 3 |
| Health 1180 with 1192 | bit 4, +0x04 |
| Heat 1186 with 1195 | bit 5, +0x08 |
| Target LOS 1190 with 1196 | bit 6, +0x0c (squared in play) |
| Target Distance 1184 with 1194 | bit 7, +0x10 (squared in play) |
| Region: Enters 1188, Exits 1189 | flags2 bits 0 and 1, the region +0x14 |

**Tests:**
- **test_vehicle_ddx.py** runs the Valve page as well, the entry given
  (0x004bb360 stubbed) and the region's show and take recorded. 0 differ.
  Breaks, each fails: Enters on flags2 bit 1 (102 of 3000); 1196 on +0x10
  (167).
- **vehicle_editor_valve_page:**
  - the valves listed;
  - one picked gets an entry of the defaults, shown;
  - Health, its number and "open" put on the brain's vlist at Apply;
  - a typed "gate_9" makes the valve, lists it and adds its entry.

### The vehicle editor's save (ours, S7f, 2026-11-09)

The exe keeps Apply only in aiv.zrd's record cache and in the running brain
(above). Ours saves it (`upgrades/editor/aiv_save`).
- **The notes:** each record the store puts in the cache is noted (the last of
  each vehicle) as its value's bytes, written at once over the mission's
  aiv.zrd as it stands. No pointer into the mission is kept. The notes are
  cleared when the mission's vehicles are forgotten.
- **FIX (bug), the cache's lifetime:** every record the editor keeps in the
  cache (0x004defc0) holds a reference on aiv.zrd. So the file never unloads,
  and the cache outlives the mission: the next mission's vehicle editor loads
  the old records, their orders on freed regions and valves. The cache is
  emptied (0x004df5d0) when the mission's vehicles are forgotten (fixes.md).
- **Save** (the Mech AI form, beside Refresh) Applies the vehicle shown, then
  writes every noted record into the mission's aiv.zrd (the open archive's, as
  this run last saved it). The archive is saved as the nets' save does it, with
  `save_archive_edit`: into the editor's mod, or under MODS EDIT into the mod
  itself, one .bak kept. The form's last line and the panel say where.
- **The record written:** an edited record keeps its bytes group by group
  wherever it reads back to the same values. The groups are the header's kind,
  place, yaw and team, then each tag: params, p2, p6, p8, p9, p10, p11, p12,
  p13, pilot with its can_configure, skin, lancemate, sound_tag, vlist and
  mech3. So the shipped files' message keys (pilot "MSG_AIV_..."), number
  types and tag order stay. A changed group is written anew in the file's units,
  ints and floats as the reader takes them. A group the record lacks is added
  only where the record differs from the reader's defaults.
- **The old tags** p3, p4, p5 and p7 give way to p8, and p4's inline order to
  vlist. No shipped file has them.
- **Tags the reader does not know** are kept as they were.
- **Representation limits** (the format's, not the save's):
  - The yaw is degrees in the file and radians read, so a rewritten one can
    come back within an ulp.
  - A record with no chassis writes p12 "default", which no lookup knows (-1):
    the exe crashes on a p12 with no name.
  - No pilot name drops the tag. With can_configure away from the defaults'
    there is nowhere else to keep the flag, so "" is written, which reads as an
    empty name rather than none.
  - No voice drops sound_tag, so Epona_Rhi, Alan_Mattila and Dominic_Paine get
    their voices back by name (the reader's default).
- **Tests:**
  - test_aiv_save_mech3ax.py: every record of all 48 MW3 aiv.zrd files (768)
    written over itself comes back byte for byte, and mech3ax reads each save
    exactly as the shipped archive. Six seeds then change one group of every
    record (a word, a flag bit, the chassis, the pilot, the voice, an order,
    mech3); each reads back with the change, and mech3ax reads each save with
    every other member and every record's name the same.
  - Breaks, each fails: p8's 5th item from +0x74; the vlist's region and valve
    swapped; every group kept even when changed.
  - mission_c1m1_vehicle_editor_saved_and_read_next_start_headless: owens_01's
    place, pilot, skin and a p1 range saved twice (a .bak left). Between the
    starts nothing holds aiv.zrd (its references 0), so the next start, on the
    same folder, reads the saved file and gives its brain all four.
  - mods_screen_edit_opens_the_editor_and_saves_into_the_mod_headless: the
    vehicle editor's Save under MODS EDIT goes into the mod itself, the net's
    save kept as the .bak.

### Pirate's Moon's vehicle editor (pm, P1, 2026-11-10)

The same dialog and pages, read function by function against MW3's (each
pair's pseudocode with the address map's names and the dialog's offsets put
back): OnInitDialog 0x4becd0, the tab switch 0x4be1b0, every page's exchange
(p1 0x4bf100, p2 0x4bf3f0, Attack 0x4bf810, Retreat 0x4bf9c0, p5 0x4bfbc0,
Fire 0x4bfdb0, Valve 0x4bfe50, Circle and Random Move 0x4c02f0, Direct
0x4c0170, Jump 0x4c01e0, Zig Zag 0x4c0280), the list fills and gathers
0x4c29d0..0x4c2dd0, the load 0x4c2680, the store 0x4c1cb0 and the valve find
0x4e9700 are MW3's logic. The main dialog's fields move: +0x20 from +0x130
(the settings record grew by PM's 0x20-byte target block, between the numbers
and the flags) and +0x40 from +0x378. No page shows the target block.

Two things changed, both ported on pm (`zai_params.cpp`):
- **The record compare 0x4f02b0** (the store's "is this the cached record"
  and aiv.zrd's record cache) is MW3's 0x4df1b0 plus 0x4c25a0 (the target
  block: words 0, 1, 4..7 as floats, 2 and 3 as ints; PM's flag bits +0xed
  2..7 and +0xee bit 0) and +0xee bit 1. Its order differs from MW3's; it has
  no side effects, so the result is the same. 0x4c2570 is MW3's lancemate
  test at PM's offset.
- **The name compare 0x4a06c0** adds a first test, the same pointer: two
  records with no pilot (or no voice) now match, where MW3's 0x49d2f0 never
  matches a none. Its callers on PM are the store, aiv.zrd's cache 0x4f00e0
  and the net node compare 0x4d1ab0 (whose names the port already compared
  that way).

Checked: `test_aiv_edit.py` and `test_vehicle_ddx.py` run on PM's exe with
`MW3_EXE=pm` (the record's PM layout and the dialog's offsets in the tests;
`probe_aiv_same_numbers_pm`). Breaks caught: the target block left out, word 2
compared as a float, +0xee bit 1 left out, MW3's name compare.

### Pirate's Moon's net editor (pm, P2, 2026-11-10)

Read against MW3's the same way. These are MW3's logic: the dialog and its
pick 0x402d70 (New Net's id from 1..100, as MW3's), the editor's ctor and dtor
0x402f40, 0x402fe0, Delete 0x403b10, the slot list 0x4042c0..0x404570, the
builder's open 0x4022b0 and its loop 0x402120, the fold 0x4c3f20, and the node
finds 0x4c4000, 0x4c40a0. PM's compile, waynet and region builds were ported
with the AI (`zainetedit.cpp`, test_pm_ai_nets). What changed:

- **No edit was ever kept.** PM's CZAiNetEdit ctor 0x4c39e0 clears +0x38
  (`editing`) for every net, where MW3's 0x4bcba0 sets it when not building,
  and its dtor 0x4c3e60 drops MW3's fold and save on close. So in PM's
  release a move, an add or a delete never reached the record cache.
  The port keeps both as PM's. Ours turns `editing` on where our editor opens
  a net (`game_editor_net.cpp`'s `editable`), as MW3's did; the save was
  already ours (Save Changes folds, then writes).
- **The node record** is 0xc4 bytes. Its new fields are via (a name, a
  number, a radius kept squared), terminus and static_mfb, at +0x9c..+0xa6;
  the owned pointers follow (name +0xa8, auxname +0xac, via +0xb0, links
  +0xb4, gate +0xb8, the union +0xbc, the assignment +0xc0). The compare
  0x4d1ab0 adds via's name (0x4a06c0), number and radius (fcomp's C3), static_mfb
  and terminus before the links; ported in `NetNodeRec::same`.
- **The save** (`net_save.cpp`, ours) writes the new keys as the reader takes
  them: `via ( name number radius )`, with the radius as read while it still
  squares to the record's, else its square root, and a missing name as an Int
  0; `terminus` with no items; `static_mfb ( on )`. PM's archives are version 2
  (version, count and a CRC after the table, `pm/gamez/zutil/zutl_zar.h`).
  The save reads them, writes version 2, and makes the CRC anew from every
  entry's data when the source's was right (the readers'; the original PM
  checks reader.zbd's) or keeps it (the 0 of the rest). Before this, Save
  Changes on PM refused every mission archive.

Checked:
- test_net_cache.py with MW3_EXE=pm: PM's 0xc4-byte record, every new field
  one apart. Breaks caught: via left out, terminus left out.
- In mw3pmtests, test_net_save.cpp on PM's install:
  - every shipped net archive rewritten as it is is the same file;
  - all 619 nets saved unedited are the same bytes;
  - every node re-encoded reads back the same (14,332 nodes; 626 with via, 12 with static_mfb);
  - the reader opens the edited readerm1.zbd as PM's game, and its CRC checks;
  - the record cache runs on net_05 (PM's net_01 holds two nodes).
  PM's motion.zbd is left out: every entry stores a length of 1, and the
  editor never saves into it.

### Pirate's Moon's net view (pm, P3, 2026-11-10)

No logic change. Every znet_view pair (0x5acde0..0x5b02b0) and the frame
0x403190 differ in their code only: PM's build passes in registers or on the
stack where MW3's passed in other registers (its clip setter 0x5bb1f0 is cdecl,
MW3's 0x59b2f0 takes ECX and EDX), and it inlines a few calls. In the frame, the
line clip's box (0, 0, then the screen less 0.9375) is inline, as MW3's asm
has it (the port's `kEdge`), and the 70-degree test (1.2217305) and the 1.5 are
read from globals of the same values. The ground pick is the world's vt+0x104
(MW3's +0xf8), a hook in the port.

Checked: test_net_view.py with MW3_EXE=pm, at 53 bits, the reference
runtime's (CLAUDE.md). 600 runs, every state, flag and hit the same, 6 drawn
points a pixel off (as MW3's at 53). At 24 bits PM's floats land 1 to 3 ulps
from MW3's (its build keeps and spills x87 values elsewhere); all states and
pixels still agree, so the gap is left alone. Breaks caught: a circle's hit by
its radius alone, a head's point moved. The test takes PM's three zTwoD
vtables from its ctors: the address map gives the arrow's 4 off (0x5f16e0 for
0x5f16dc).

### Pirate's Moon's inout drawing and the rest (pm, P4, 2026-11-10)

**Show InOut drew nothing on pm.** The walls (merged from main) were MW3
models on `node->model`. PM's renderer draws only `node->pm_model` (the game
table's draw_model is PM's 0x00543b10), so the boundary never showed. PM's
own code is a port of MW3's onto its models, now ported on pm
(`zainet_inout_view.cpp`):
- The walls ctor 0x4ce970 is MW3's 0x4c42e0 plus a byte at +0x40, set from
  [0x0060b8d0] (1, never written). The polygon step 0x4cea50 and the corner
  0x4cedd0 then take each corner's offset (+0x00, the place pushed out by
  the polygon's width), not its place (+0x0c).
- The adds 0x4cecc0 / 0x4ced50 call PM's model add 0x55cb70 (cdecl: the
  model, the count, the points, one material by pointer, the back flag, the
  zone). After every add they rebuild the model's material refs (0x55c470),
  added or not.
- The build 0x4cefc0 takes PM's model alloc 0x558f50 and node_set_pm_model
  0x4fcc60.
- The constants are MW3's (10 m, 1024 vertices, two-sided, keep adding, the
  side flag 0), at PM's addresses.

**Add veh to node's types** (ours) run to PM's 25 mechs too (2026-11-11;
it had kept MW3's 19, so six of PM's mechs could not be added), and
test_new_weapon.cpp runs in mw3pmtests on PM's weapons.zrd and its version 2
reader.zbd (49 definitions, 38 offered to copy). The guide on pm says what
PM's editor has and lacks.

**The vehicle editor's Save on PM's aiv.zrd** (2026-11-11,
test_pm_aiv_save_mech3ax.py, the twin of MW3's): all 1,041 records of PM's
45 aiv.zrd files written over themselves come back byte for byte, 6,246
changed ones (six seeds) read back changed by PM's compare 0x4f02b0, and
mech3ax (as PM's) reads every save. PM's `target` tag is no group of
aiv_save's: it is kept as the file has it, and the 438 records holding one
keep it through every save. Two things the test found:
- PM's multiplayer readers (`readermp*.zbd`) hold loose words between
  records (`:spawn points for one base`, a comment the file's compiler
  kept). The game finds a record by its name anywhere (0x4e0680), so it
  reads them; the save took the top list two items at a time and refused
  the 42 records after the words. The aiv paths (the save, Add vehicle) now
  take a record as a name followed by its list and step over loose items
  (`record_pairs_of`, zrd_bytes.h), which leave them where they are.
- The probe read only MW3's 8-byte archive footer (PM's is 12, version 2).
Breaks caught: the target tag dropped, its value replaced, the old pairing.

**Not in PM's editor, so not added** (read from PM's exe): its Node Params
0x4d2c80 makes template 180, which PM's resources do not hold (only 132 and
188), and hands it to an empty function 0x5c7a30 (MW3's 0x4bf620), so it had
no fields, PM's via, terminus and static_mfb among them; Save Changes writes
those keys as read (the net save above). No page of PM's vehicle editor
shows the target block (P1 above).

**The vehicle editor's chassis list** (0x4c2d00) runs to 25, PM's mechs
with MW3's, where MW3's 0x4bc860 stopped at 19; the port on pm had kept 19.

Checked:
- test_inout_view.py with MW3_EXE=pm: PM's polygons (tested +0x46, kind
  +0x48, corners 0x54 bytes), every corner's place 100 m from its offset,
  the rebuilds counted. 400 runs the same (42,132 triangles). Breaks caught:
  the place for the offset, the rebuild left out, the back flag the other
  way.
- `pm_show_inout_builds_pm_models` (mw3pmtests, a whole mission in a child):
  the walls as PM models (2 models, 2,588 polygons, 4 material refs), none
  back-faced, then taken down. Leaving out node_set_pm_model fails it.
- test_editor.cpp now runs in mw3pmtests. On PM, the pick and trace commands
  are 0xa3 / 0xa4 (MW3's 0x9b / 0x9c), the Mad Cat is mech 13, and the
  chassis list holds 26 rows. A list cut back to 19 fails it.

## Teleport (2026-11-10)

**The original's.** The developers' teleport, command 0x95. It is a case of
app_key's switch 0x0040dba0 (0x0040dd17 calls 0x004a37a0), but 0x0040db60
registers no callback for it, so no key reaches it in the release.
- 0x004a37a0 lists the places (0x50-byte records):
  - "Current Location", the player's place (0x005a85e0), not saved;
  - the mission's nav points ([0x00609980], the nav waynet: vt+0x24 the
    label, vt+0x18 the place), yaw 0, saved;
  - location.zrd's `name ((x y z) yaw)` pairs (zrdr::load). Every world's
    reader.zbd ships one (discoveries.md lists the spots). Without one it
    stops with an error box.
- It then runs dialog 132 (0x0040dfe0: the fields at the player's place,
  the yaw passed as 0, shown in degrees).
- On OK the player's vehicle is put there (vt+0xdc, yaw in radians). So
  it is on Cancel: 0x0040dfe0 tests DoModal's result for non-zero
  (`test eax, eax` at 0x0040e05f), and IDCANCEL is 2, so Cancel moves the
  player to the fields as they last were (a pick's, without OnOK's
  UpdateData(TRUE)) and, with the changed flag, saves. Pirate's Moon fixed
  it (its 0x004a8a40 tests `== IDOK`) and kept GOTO Net's `!= 0`.
- With the dialog's changed flag, location.zrd is renamed and rewritten as
  text: `(`, two `;;` lines ("Last updated Sep 20 1999 at 16:29:38", the
  build's __DATE__ and __TIME__ whenever it is saved), then
  `"name" ( ( %8.1f %8.1f %8.1f ) %8.1f )  ;; position/yaw` for every saved
  place, then `)`. The decompiler drops the name and time arguments; the
  asm passes them.

**Dialog 132** (0x00404630; the template: x, y, z, yaw edits 1009..1012,
"Deg.", OK, Cancel, Delete (3), the list 1073 CBS_DROPDOWN | CBS_SORT):
- 0x00404720 exchanges x, y, z and yaw (DDV ±360).
- 0x004047b0 lists the places, each one's data its record, and selects
  "Current Location".
- 0x004049b0 (CBN_SELCHANGE) fills the fields from the pick.
- 0x00404860 (CBN_KILLFOCUS) adds a typed name the list lacks (any case) as
  a place at the fields. An empty one gets message 0x1de.
- 0x00404a20 (Delete) asks with message 0x1df and the list's text; on Yes
  it marks the pick unsaved and deletes it from the list.
- 0x00404ab0 (CBN_EDITCHANGE, the edits' EN_CHANGE) sets the changed flag.
  Windows sends EN_CHANGE for the dialog's own SetWindowText too, so
  OnInitDialog's UpdateData(FALSE) sets it: in effect every OK saved.
- 0x00404aa0: OnDestroy, the cursor hidden again.

**Ported** (`mech3/teleport`: teleport_spots, location_zrd_text,
TeleportDialog, the hooks) **and ours** (`upgrades/editor/teleport`):
- A TELEPORT row in the editor's main menu (its last blank row), so it is
  reached as the editor is (MODS only).
- The dialog is a form: the place list with its typed name, the four
  fields, OK, Cancel, Delete; Delete's question is a Yes/No form.
- OK writes into the mission's mod as a loose `zbd\<world>\location.zrd`
  (the folder of the archive location.zrd came from), read ahead of the
  archive's entry. Its earlier copy is kept as location.zrd.bak.
- FIX (bug): Cancel cancels, as in Pirate's Moon (no move, no save).

Fixes, each FIX (bug) in the code:
- a nav point without a label is left out (the exe copies from null);
- Delete with nothing picked does nothing (the exe writes through CB_ERR,
  at 0x4b);
- the .bak goes beside the file (the exe's rename named no folder, so it
  went to the working directory);
- a location.zrd pair of another shape is left out (unchecked in the exe);
- without a location.zrd the list is the player's place and the nav points
  (the exe stopped).

Tests:
- `teleport_spots_from_c1_and_the_saved_text`: c1's shipped file (four
  places after the player and a nav point); the saved text exactly as the
  exe's format, read back by the text reader to the same places.
- `teleport_dialog_handlers`: the init's flag, the sorted list, a pick, a
  typed name added in its place, an empty name, Delete asked, done and
  refused, nothing picked, OK in radians, a yaw past 360.
- `mission_c1m1_teleport_moves_the_player_headless`: the row's form lists
  c1m1's nav points (Able, Baker, Start) and c1's places; Mission 2 Start
  and OK puts the player at (-1408.7, -10.0, -845.3) and saves into the mod.
- The help coverage (12 forms).

### Pirate's Moon's teleport (pm, 2026-11-10)

Read against MW3's: the command 0x004a8450 (case 0x9d of app_key's switch
0x0040ba30; 0x0040b9f0 registers 0x69, 0x78, 0x74 and 0x86, MW3's plus 8,
so no key reaches it here either) and dialog 132 at 0x004a8a40 (DoModal),
0x004a8bb0 (ctor), 0x004a8c50 (DDX), 0x004a8ce0 (init), 0x004a8f00 (pick),
0x004a8d70 (typed name), 0x004a8f70 (Delete), 0x004a9000 (changed),
0x004a8ff0 (OnDestroy). The message map (0x005eabc8) and the dialog's
layout are MW3's; the pick, Delete and init are MW3's code. What changed:
- **Nav points are not saved.** The record gains a byte at +0x4d, 1 for
  every place but the nav points ([0x0062daf0], each by 0x005acad0), and
  the save skips a record with +0x4d clear as it skips +0x4c. MW3's wrote
  the nav points into location.zrd on every save, so the next open listed
  them twice.
- **Cancel cancels.** 0x004a8a40 returns DoModal == IDOK; MW3's 0x0040dfe0
  took any non-zero result (GOTO Net's 0x0040be40 kept MW3's `!= 0`).
  Ghidra drops the save as unreachable: the flag is written through the
  wrapper's fourth argument, which it does not follow; the asm at
  0x004a8844 tests it.
- **The place is read by 0x00578b20**: a list of at least three numbers
  (Int or Float), or the pair is left out; a word among them keeps the
  buffer's number, the last place's. The yaw is read raw, as MW3's.
- **A null nav label** is checked, but the record is still added with its
  name unset.
- **A typed name's record** (0x004a8d70) takes the fields' place but not
  the yaw (+0x0c is left as allocated). FIX (bug): the field's yaw, as MW3's.
- The save stamps PM's build: "Last updated Nov 14 1999 at 14:36:03"; the
  rest of its text, the remove-then-rename of location.bak (still into the
  working directory) and the error box without location.zrd are MW3's.
- The player is moved by vt+0xe8, PM's set_place (0x00419d50, the port's
  `move_mech`).

Ported on pm in `mech3/teleport` (the +0x4d byte as `kept`). Tests in
mw3pmtests:
- `teleport_spots_from_pm_c3_and_the_saved_text`: PM's c3 places (Ruins,
  Pirate_landing), a nav point not kept, PM's header, the text read back
  without the nav point, a short place left out and a word carried.
- `teleport_dialog_handlers`: as main's, plus the typed name's yaw.
- `pm_teleport_moves_the_player_and_saves_pms_text` (a mission in a child,
  1,1,3 on c2): the list with Able, Baker, Charlie and Start; OK at Portal
  Wall moves the player there and writes PM's text with c2's four places and
  no nav point; Cancel moves nothing and writes nothing.
Breaks caught: the nav points kept, MW3's date, no carry, the typed yaw
left out.

## The editor's help (ours, 2026-11-09)

Owen asked (through the lead) for help in the game and on GitHub, from one
source so the two cannot drift.
- **The tables** (`upgrades/editor/help.h`) sit beside the forms' models:
  help_menus.cpp (both menus' rows, Teleport, Save as's box), help_weapons.cpp
  (New Weapon), help_nets.cpp (the net dialog, Node
  Params, Add vehicle, Delete's question, the net window and its Open list,
  the Tree View, the Help window) and help_vehicles.cpp (Mech AI, its pages
  and tabs, its message). Each entry is keyed by its form's title (up to "##"
  or ':') and the item's label (a tab as "tab <title>").
- **In the game** the ImGui view shows an item's tip on hover. A row of the
  editor's panel (the game's own menu) shows its tip as a tooltip while the
  mouse is over it.
- **HELP rows**, ours, sit in a blank row of each menu (main menu row 4, net
  menu row 10). They open "Editor help", the quick start.
- **The guide**, docs/editor-guide.md, is generated whole from the same
  tables: the quick start, the walk-through from a campaign to the player's
  own changed copy, and a section per form. `build/mw3 --editor-guide
  docs/editor-guide.md` writes it.
- **Tests:**
  - `editor_guide_is_current` fails when the guide is stale. It also checks
    every tip is filled, each item once, the Help window and the lookup.
  - `mission_c1m1_editor_help_covers_every_form_headless` opens all thirteen
    forms (Save as's box by a stand-in session, cancelled, nothing made) and
    reads both menus: every field, button, tab and row has a tip (0
    without), and every tip names an item its form has (0 stale).
  - Break: a tip renamed ("Send hom") fails both.
- **Brought up to mods by campaign (2026-11-11).** The quick start, the
  walk-through and every row that says where a save goes follow
  docs/modding.md's steps 3 and 4: MODS, a campaign at the left, EDITOR, a
  mission, PLAY; the editor only saves as (the first save names a copy; the
  campaign opened, the game's files and imported mods never written); EXIT
  GAME EDIT into the mission in play, the pause's QUIT back to MODS on the
  copy; TEST, PLAY, REPORT, EXPORT, DELETE and
  RESET there; IMPORT's warning count and REPORT's three groups and file.
  `save_as_form()` gives the box to the view's tips and the check. Break:
  Save as's "Name" tip renamed fails both tests.
- Valve's "Target Distance" check box is labelled "Target Distance##valve"
  now: the form already had Zig Zag's field of that name.

## Dear ImGui for the editor's dialogs (lead, 2026-11-09)

Owen: the editor's dialogs need not imitate MFC. The lead's call is Dear
ImGui (MIT, source only, with an SDL3 platform backend and an SDL_GPU
renderer backend). It is used for the Battlesport editor's dialogs and for
our modding dialogs. The game's own menus stay zTwoD.

**What it replaces:** the panel forms that stand in for Windows dialogs:
- the net pick (139);
- the Ai Network Editor window (181);
- Node Params (180);
- Add veh to node (141);
- the tree view (155);
- Delete Node's second-press confirm, which stood in for a Windows message box;
- the Mech AI dialog and its twelve pages.

These get real text fields, combo boxes, lists, trees and tabs, with no
paging rows. The editor's own 25-row menus (0x0040e7b0, 0x0040e7d0, the AI
net editor's 13 rows) are the original's in-game panel, not MFC: they stay
as they are, drawn through zTwoD's FontText.

**Code layout:**
- Each form's state and actions are model code with no ImGui types: the
  ported functions in `mech3/game_editor_*`, and ours in
  `src/upgrades/editor/`.
- A thin view, `src/frontend/sdl/editor_imgui.cpp`, draws each model and
  calls its actions.
- Headless tests drive the models, as today's tests do, never the widgets.
- The vendored copy is `third_party/imgui`, one tagged release pinned (not
  the docking branch). It holds the core files, `imgui_impl_sdl3`,
  `imgui_impl_sdlgpu3` and LICENSE.txt.
- The build target `mw3imgui` links into the programs (mw3, mw3pm, through
  mw3sdl), never into mw3core or the probes.
- THIRD-PARTY.txt gets an entry in `tools/macos/` and
  `tools/windows/dist/`; the AppImage copies the Windows list.

**Input hand-off** (`SdlPump::handle`):
- While the editor is up, each event goes to `ImGui_ImplSDL3_ProcessEvent`
  first. Mouse events then go on to zInput (`input_->on_event`) only when
  `io.WantCaptureMouse` is false, and key and text events only when
  `io.WantCaptureKeyboard` and `io.WantTextInput` are false. So the stealth
  flies and the reticle picks whenever no window is under the mouse and no
  field has the focus.
- Text input (`SDL_StartTextInput`) is on only while an ImGui field has the
  focus.
- Our Alt+Enter keeps coming first, as now.
- With the editor down, ImGui gets no events and nothing changes.

**Render order** (`SdlRender3D::present`):
1. The game draws its frame into `frame_` as now, the editor's zTwoD panel
   included.
2. The frame is blitted to the swapchain, fitted with its bars.
3. While the editor is up and ImGui has draw data:
   `ImGui_ImplSDLGPU3_PrepareDrawData` on the same command buffer, then a
   render pass on the swapchain with LOADOP_LOAD, `RenderDrawData`, the end
   of the pass and the submit. ImGui draws in window pixels over the whole
   window, bars included, scaled by the window's pixel density.
4. A hidden window, which the headless tests use, makes no ImGui frame.
   `--shot` reads `frame_`, so shots show the game without ImGui; a
   swapchain read-back for editor shots can come later if wanted.

`NewFrame` and `Render` run once per presented frame, from the editor's
frame hook, after the game's update and before present.

**Built (S7a, 2026-11-09).**
- **The copy:** Dear ImGui v1.93.0 (2026-10-14), commit c1aaac65, in
  `third_party/imgui`; `VERSION.txt` gives the tag, the commit and what was
  copied. It is not named VERSION because the folder is on the include
  path, and on a case-insensitive disk that file stands in for `<version>`.
- **The build:** target `mw3imgui`, linked through mw3sdl.
- **The code:** `src/frontend/sdl/editor_imgui.{h,cpp}`. `EditorUi` is made
  at the program's start but starts ImGui only at the editor's first event
  or frame, so a run that never opens the editor has no ImGui context.
  `SdlPump::overlay_event` and `SdlRender3D::overlay` are its two hooks;
  the overlay runs only on the swapchain path, never in a capture.
- **The window:** an empty "Game editor" window stands in until S7b.
  `io.IniFilename` is null, so nothing is written beside the game.
- **The test:** `sdl_editor_imgui_routes_input` (mw3sdltests), on a hidden
  window. With the editor down, ImGui is never made and the key reaches
  zInput. With it up, a click over the ImGui window is ImGui's and one off
  it is the game's; a key goes to the game with no text field focused and
  not with one. Break: the pump passing every event on, and the field's
  key reaches zInput (2 events, not 0).
- **The check:** `MW3_EDITOR_AT=N` with `--debug-camera` brings the editor
  up at the pump's Nth pass in a mission, as Shift+F12 does. With
  `--shot`, c1m1 shows the game alone; in a visible window the log says
  "the editor's dialogs drawn over the game".
- **Packages:** both THIRD-PARTY.txt lists carry the licence. The Windows
  package copies it to `licenses\imgui`, and `check_dist.ps1` requires it
  there.

**Built (S7b, 2026-11-09).**
- **The model:** `upgrades/editor/form.{h,cpp}`. An `EditorForm` has a
  title, fields (a label, int, float with whether it is given, flag,
  three-state, text, choice list, tree) bound to the dialog's own state, its
  buttons, and whether it is modal. `form_show` and `form_hide` keep the
  list of shown forms.
- **The view:** `draw_editor_forms` in `frontend/sdl/editor_imgui.cpp`
  draws every shown form as an ImGui window, or a modal popup. A list
  longer than 8 is a list box, a shorter one a combo. A field's edit
  writes the form's copy at once. Its `changed` and the buttons' presses
  are posted and run at the editor's next frame (`form_run_posted`, from
  `editor_frame`), in the game's update, not while the renderer presents.
- **Moved to forms:** the net pick (139), the Ai Network Editor window (181)
  with Open's file list, Node Params, Add veh to node, the tree view, and
  Delete Node's question (modal, OK and Cancel, as the exe's box).
- **The panel:** keeps the menus. The forms float over the mission, and the
  panel's paging rows and typed-text rows are gone.
- **Tests:** they drive the forms (fields set, `press("OK")`) where they
  pressed rows; the logic under them is unchanged.

## The vehicle editor and ImGui, in sittings (planned 2026-11-09)

1. **S7a ImGui in.**
   - Vendored and pinned, with its licence and both THIRD-PARTY.txt entries.
   - Built into mw3 and mw3pm on macOS; the mingw cross build and the
     Windows and Linux packages checked.
   - The input hand-off and the render pass above, and an empty window while
     the editor is up.
   - Tests: a unit test of the routing (an event the editor's ImGui takes
     never reaches zInput; with the editor down, every event does);
     c1m1's Shift+F12 mission test unchanged.
2. **S7b The forms built so far, moved to ImGui:** the net pick, window
   181, Node Params, Add vehicle, the tree view and Delete's confirm (a
   modal with Yes and No). The model/view split comes first. Their logic
   and tests stay, and the tests drive the models; the panel's paging rows
   go. editor.md and CHANGES.md say what changed. This is its own sitting,
   so nothing else moves at the same time.
3. **S7c The Mech AI dialog's shell** (ImGui for MFC: FIX (A)):
   - The entry, object and OnInitDialog with its lists.
   - The vehicle pick, load 0x004bc140, store 0x004bb7e0 with its compares,
     the aiv cache's edit 0x004defc0, Apply, OK, Pause, Refresh, the close.
     The tabs are there, with the pages empty.
   - Differential tests of the store's compare chain and of the cache's edit
     on random records, against the exe.
   - A mission test on c1m1: open the editor, pick a vehicle, change a value
     and Apply; the brain's settings change and the cache holds the record.
   - Pirate's Moon's differences counted.
4. **S7d The pages, part 1** (done, above): p2, the vehicle, with its
   handlers (team, home net and Net Edit into window 181, Send home,
   configuration, chassis), and p1. Each page's DDX runs on an MFC stand-in,
   checked against the exe's DDX in Unicorn with MFC's calls modelled. Part
   2: Show InOut's drawing (0x004c4730, 0x004c41b0).
5. **S7e The pages, part 2** (part 1 done above; Valve next): Attack and its strategy tab (Circle, Direct,
   Jump, Zig Zag, Random Move on Circle's DDX), Retreat, p5, Fire and Valve,
   with the valve combo.
6. **S7f The save (ours)** (done, above):
   - Apply's records into the editor mod's aiv.zrd for the mission
     (`aiv_with_record`, as the nets' save), with MODS EDIT saving into the
     mod itself.
   - A restart read-back mission test and a mech3ax check of the written
     archive.
7. Then PM's differences on `pm`, and the modding dialogs (above) on the
   same ImGui views.

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
6. **S7** The vehicle editor: mapped 2026-11-09 ("The vehicle editor, the
   map"), its sittings S7a..S7f in "The vehicle editor and ImGui, in
   sittings".
7. **S8** The "GameZ Interpreter" console 0x5960a0, with its log callback
   0x596130, its line runner 0x596190 and its toggle 0x4810d0 (`interp.md`,
   Hidden debug console). The exe's only way in, action 0xa3, is likely
   unreachable, so the key is ours, under a debug flag as the camera's is.

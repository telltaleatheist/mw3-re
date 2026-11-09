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

## The way in (S5 sitting 3a, 2026-11-09)

Ported in `mech3/game_editor_net` (EDIT AI NETS on the main menu calls it):

- **The dialog** (template 139; FIX (A), ours): in the editor's panel, not an
  MFC dialog, and not modal (the mission runs on, as under the editor's
  menus). Its rows are the template's controls: the title, "Select a network
  to edit:", the list, OK and Cancel. The list is znet's registry by name,
  sorted case-insensitively (the drop-down's CBS_SORT; Windows sorts with
  CompareString, which may order punctuation otherwise, but no shipped net
  name differs by it), "New Net" at the top and selected; the selected row
  marked `>`; a longer list pages with "(up)" and "(down)" rows (ours: the
  panel has 25 rows). A list row selects; OK and Cancel both open the
  selection, as the exe's do.
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
  round). No net: "Select a node or line first". FIX (A): the exe asks with
  an OK/Cancel box, "Confirm node deletion" / "Confirm line deletion"; here
  the status line asks and a second Delete Node on the same one confirms.
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
  is a list of every net_ and reg_ file the open archives hold, paged 18 to
  a page (ours: the panel's 25 rows), Cancel at the end.
- **Each frame** 0x00404290 steps every editor on the list (0x00403080) but
  the main one while the editor's menus are up (their own step runs it): so
  the window's net is drawn and picked like the edited one, beside it, with
  no menu of its own. With the menus down, the main editor is stepped there
  too, as in the exe.
- **OK and Cancel** close it (DestroyWindow on a modeless dialog,
  0x004d5610 / 0x004d55f0 -> 0x004bf6f0); its dtor (0x004bf5d0 ->
  0x00404420) frees its editor and takes the slot off the list.
- FIX (A): the window's controls are the panel's rows (with ours, a line
  naming the net it shows), the net editor's step going on under them.

Tests: `net_window_open_and_hosted_editor` (the rows, Open's list and
Cancel, a file opened as a second editor that the frame steps apart from the
main one, OK freeing it; with the menus down, the main one stepped);
`mission_c1m1_net_window_shows_a_second_net_headless` (c1m1: Open lists the
archives' net files, net_01.zrd among them, pages down and up, net_02.zrd
opened with its 19 nodes as marks, stepped, OK closing). Breaks: the window's
editor not stepped, Open opening nothing.

## Node Params (S5 sitting 4c; ours, 2026-11-09)

In the release Node Params (0x00403710 -> the node's vt+0x90, 0x004c7190 ->
0x004c7840) opens template 180, "Node Parameters", modeless, with OK and
Cancel and nothing else, and never hands it the node: 0x004c7820 calls
0x004bf620, an empty function. Owen's call (2026-11-09): build it as it was
meant to work, as ours (`upgrades/editor/node_params`).

The first pick's record is a form in the editor's panel, one row a field:
placed (kind 0, its x y z given), x, y, z, speed, wait interval, wait time
(each "off" until a value is typed), the two can-leave words, jump,
arrange nodes' spacing, hidden, fixed, autonext and visible (each "as the
default", yes or no, cycling on a press), backup and shunt (yes or no), name
and gate. A press on a value row takes typed text through zInput's character
callback, replacing the value (Enter keeps it, Esc drops it, Backspace;
nothing typed or a number that does not read leaves the value; "off" turns
a switchable value off, "-" clears a name). OK puts
the record on the node field by field (the node keeps its own auxname), its
place through set_pos and the links stored again, so the fold
(0x004befc0) takes it to the cache and Save Changes keeps it; the editor is
made anew. Cancel leaves the node as it was. Without a pick: "Select a node
first".

Tests: `node_params_form_edits_the_picked_node` (no pick refused; a value
typed and kept, another dropped by Esc, the tri-state cycle, a name; OK
puts them on the node, Cancel does not); `mission_c1m1_node_params_saved_headless`
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

The form: the type (a press lists them, paged), x, y, z, yaw and the team id
typed, the side cycling Friend, Foe, Other (the team following it: 1, 2, 0).

Tests: `add_vehicle_form_and_record` (no pick refused; the form's fields
from the node, the side cycling the team, a typed yaw, Cancel; the record
helpers on an aiv.zrd of ours); `mission_c1m1_add_vehicle_made_and_saved_headless`
(net_01's node_00, a ttruck as a foe: ttruck_03 made at the node, one
vehicle more, and read back from the editor mod's archive through our aiv
reader with net 1, team 2, the node's place and ttruck_01's settings);
test_aiv_add_mech3ax.py (in every MW3 reader archive with an aiv.zrd, a
record of each type its records have added as Add vehicle adds it: 240 in 48
archives read by mech3ax's unzbd exactly as written, after every shipped
record, every other member the same). Breaks: the tags dropped, a wrong team.

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
  editor came from it (not to the mission).
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
`.bak`, no "editor edits" folder, EXIT GAME EDIT back on MODS with the
original pilot). Breaks: the save into the side folder, no editor on the
first frame, MODS not opened again.

## Our modding dialogs (planned; a sketch for Owen, 2026-11-09)

Owen's direction: modders never have to edit a file. Each major modding
feature gets a basic dialog in the Battlesport editor, in its style (the
panel's rows, as the net editor's forms are), marked ours, in
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

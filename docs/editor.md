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
  0x00402fd0 does), the camera's far clip kept and put back (0 for none:
  the hooks give none yet), dirty and rebuild. The selection base 0x0058bf50
  (a display object for each node and link, the selection box) is 3b's.
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

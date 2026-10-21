# The campaign flow: from the pilot screen to a mission and back

Mech3.exe 1.2. The states are ported (`mech3/app_states`, game-loop.md);
the screens they make are hooks. This maps the screens and the campaign's
progress between them, and gives a porting order.

## 1. The path

```
main menu NEW CAMPAIGN -> Campaign state (0x0048b000 resume, 0x0048af20 frame)
  -> pilot screen ACCEPT (pop 2)
  -> [movies on, the newest save is Op 1 Mission 1] fmv.zrd "campaign"
  -> pop (0), Replace with Load (state 6)
Load state (0x00409440): briefing movie fmv.zrd "c<op>m<n>" (when +0x1c1),
  sounds, zbd;zbd\<dir>, briefing screen 0x00405230 ("brief_<dir><n>"),
  the mission load thread 0x00490880 (mission-load.md)
briefing LAUNCH (0x00404f70) -> the launch 0x00491150 (mech-build.md 8)
  -> Replace with Core (5); a failed launch unloads and Replaces with Main
Core (the mission) ... the end screen 0x00491bf0 (mission-frame.md):
  campaign / Instant Action fade out 2 s, Replace with the Win State at
  0x006099a0; training with 0x006098c8
Win State 0x006099a0 (enter 0x0047ac20): results kept, the next mission
  saved (0x00478f10(1)), the debriefing screen 0x0047b430
debriefing CONTINUE (0x0047d030(0)):
  campaign won (Op 4 Mission 6)  -> Win Movie State 0x00608be8 (fmv "c4win"),
                                    then Main with the credits flag [0x00608e4e]
  mission passed                 -> unload(0), Replace with Load: the next briefing
  mission failed / cheated       -> lancemates reset, unload, Replace with Load:
                                    the same mission again
debriefing ABORT (0x0047d030(2)) -> pilot saved, unload(1), Replace with Main
```

There are two Win States (0x00608e70 in the state table, 0x006099a0 the
end screen's fade target, the same class 0x0047ab10) and two Salvage
States (0x00608ee8, 0x00608bd8 pushed by the briefing's SALVAGE; class
0x0047f220). Instant Action's debriefing goes on to the "IA Dummy State"
(0x00608bd0, 0x0047ce80: the kill tallies, then Main).

## 2. The briefing screen (0x00405230, new(0x12e6c), briefing.cpp)

The dialog is briefing.zrd's BRIEFINGDIALOG (mpinfo.zrd's for multiplayer);
the briefing's own section (`brief_c1m1`, else "default") gives the map
(0x00406650: game_type, scenario_name, scenario_desc, bitmap from
`..\data\common\images\maps`, waypoint, circle, triangle, label, line,
text) and a list of sounds (+0x11220) played one after another. Buttons
(id, press 0x00404c30):

| Id | Part | Press |
|---|---|---|
| 0 | ABORT | the sounds stopped; 0x00405050: when the mission is loaded, the pilot saved (0x00478f10(0)), a campaign's salvage merged (0x00434cb0), unload(1), Replace with Main |
| 1 | LAUNCH | 0x00404f70: a campaign with unclaimed salvage (+0x11214 clear, the store +0x3c not empty) asks first (0x004ac150(1, 0x1e0, 0x1e1)); then 0x00491150 |
| 2 | MECHLAB | the sounds stopped, the mech lab (0x00448830); back with resume(4) the loadout is refreshed |
| 3 | REPLAY | the sound list restarted |
| 4 | SALVAGE | pushes the Salvage State 0x00608bd8 (screen 0x0047d3b0), +0x11214 = 1 |
| 5 | LOADING | shown while the load runs |
| 7 | (from the load) | 0x00404ae0: the buttons disabled |

Built (item 2 corrected the map): the briefing's own section of
briefing.zrd (`brief_c11` from "brief_%s%d"; multiplayer's `briefing` of
mpinfo.zrd when the game's vt+0x4c, +0x94, is over 2), else `default`, is the
dialog (images, fonts, TRANSPORT_PROGRESS, CURSOR); the buttons always come
from BRIEFINGDIALOG. Besides the buttons: a debug text box of the load's
lines (+0x113bc at (300, 12), `debug_font`, shown only with the app's
+0x1ba; 0x00405120 writes it), the version line "<message 7> <version>" at
the bottom right, REALLOC_OK (7) and REALLOC_LIST (the mech lab's store
list 0x0048d880). The six buttons are the mech lab's (0x0044aaf0): under the
mouse they blink between the normal and ACTIVATE images every 0.05 s and
show their HELP labels. Built, the screen is drawn once at once.

Each frame (0x00406440): the script's current entry steps (item 3);
the load polled (0x00490930) into 0x00408ef0, which on a change sets the
buttons by the game:

| Game | ABORT | MECHLAB | SALVAGE | REPLAY | LAUNCH |
|---|---|---|---|---|---|
| 0 Mech Lab / dev | loaded | loaded | loaded | loaded | loaded |
| 1 campaign | loaded | loaded, not Op 1 Mission 1 | as MECHLAB | loaded | loaded |
| 2 Instant Action | loaded | loaded | hidden | hidden | loaded |
| 3 multiplayer | hidden | hidden | hidden | hidden | loaded, and launches itself |
| 4 training | loaded | hidden | hidden | hidden | loaded |

Loaded, the cursor shows, LOADING hides and the lancemates' loadouts are
made (0x00408c30); a campaign forgets that SALVAGE was pressed. The bar
TRANSPORT_PROGRESS (the shared slider 0x005bbc90: a click sets it from the
mouse) creeps 0.005 a frame towards the load's fraction (0x004050d0; FIX
(B) per_second: 0.15 a second), 1.0 once loaded. Keys: L (LAUNCH) and M
(MECHLAB) press the buttons once loaded, Space or S stop the voice
(0x0051a300 clears the keys).

Ported (item 2, `mech3/briefing`): the screen 0x00405230 with its
destructor, the frame, the enabling, the presses, ABORT, LAUNCH, the
fraction and the lines; the mech lab's button (`HelpButton`), the shared
slider (`ShellSlider`) and the store list's making (`StoreList`) in
`menu_widgets`. The calls of later items are Engine hooks: the map's image
0x00405160, the reallocation list 0x00404ae0, the loadouts 0x00408c30, the
mech lab 0x00408b40/0x00448830, the Salvage State 0x0047d2f0. The Load state
still builds the original's screen by address (item 5 wires the port).

### The script (item 3)

SOUND in the briefing's section is `("briefing_c1m1" MARKERS (step0 step1
...))`, each step a list of map objects `(circle (..) text (..) ...)`. The
script (+0x11220, a list with its current entry +0x0c and playing +0x10)
is: the voice (vtable 0x005bbc50: zsnd's sound by name, played at 1.0 with
its cue markers calling 0x00405ed0, which sets the step reached [0x005e043c];
no such sound sets 999, every step at once), then for each step a marker
(0x005bbc40, done once [0x005e043c] >= its index) and its objects
(0x005bbc30, shown when reached; 0x00406650 reads them, item 4), then the
end marker (-1, done once the voice has stopped). The Load state starts it
(+0x10 = 1, +0x0c = the first entry); a frame steps the current entry and
moves on when it is done. REPLAY stops the voice, hides the objects (+0x08)
and starts again; ABORT, MECHLAB, SALVAGE, Space and S stop it.
Ported: `BriefingVoice`, `BriefingMarker`, `BriefingShow`, `read_script`,
`briefing_cue`; the voice's calls are Engine hooks (`voice_play`,
`voice_status`, `sound_cue_briefing`), the objects' reader
`briefing_map_object` (null until item 4).

### The map's objects (item 4)

0x00406650 reads a MARKERS entry `(kind (keys ...))`; FONT (else
brief_grey) and COLOR (else white) are read for every kind, AT defaults to
(320, 240):

| Kind | Object | Keys |
|---|---|---|
| circle | 0x005bbfb0: zTwoD's circle with a label (+0x54) and SOUND (+0x504) | at, radius (first, last; 10 3) |
| text | 0x005bc030: a FontText (its move erases its old place) | at (else none), string (a message name) |
| game_type, db_name, scenario_name, scenario_desc | the same, shown at once | at; the text is the game's vt+0x04 .. +0x10 (multiplayer's) |
| bitmap | an image (0x005bbf30) | at, image |
| waypoint | 0x005bbeb0: a circle on a nav point of the mission ([0x00609980], made by the Load state's 0x00490720), placed on the mission's map (0x00491c80, 0x004586e0), labelled with message 0x200 | label (the nav point's name and the label: NAME is not read), radius, color |
| triangle | 0x005bbe30: zTwoD's triangle | at, height (size; the second value is kept, unused), sound |
| line | 0x005bbdb0: a polyline drawn every frame | points |

Anything else is reported ("Failed to create locator", an empty function in
this build) and skipped. The objects start hidden; the script shows them.
A circle shrinks from its first radius to its last by max(20 x dt, 1) pixels
a frame, so a pixel a frame above 20 fps (FIX (B) per_second: 30 a second).
A waypoint follows its nav point while a world is up. The objects are kept
(+0x12e60) and deleted with the screen.

## 3. The debriefing (0x0047b430, new(0x13bf4))

dlgWin.zrd's WINMENU: WIN_BACKGROUND, ICON_ABORT, ICON_REPLAY,
ICON_CONTINUE, ICON_PLAYER0..3 (the lance), WIN_TITLE, SALVAGE_LIST,
ICON_RANK. The title is message 0xdc0 with the operation and mission; the
lance's kills and damage, the objectives and the salvage are drawn as text
columns at fixed places (the globals 0x005e5ae4..0x005e5af8). The press
0x0047d030: 0 CONTINUE (section 1), 1 REPLAY plays the `c<op>m<n>win`
voice again (0x0047abe0), 2 ABORT.

The Win State's enter: the kept results (0x0047ae10(0, ..): every mech of
the player's team, or only the player's, put in the salvage store
0x00472f00) and the record +0xb4/+0xc4 from the screen; a failed mission
0x0047ae10(1, ..) merges the salvage only.

## 4. The campaign's progress

- **The counts** 0x00484840: operations 1 and 2 have 4 missions, 3 and 4
  have 6.
- **The advance** 0x00484870 (ECX &operation, EDX &mission): the mission
  goes up; past the operation's count the operation goes up and the
  mission is 1. True when the operation reaches 5: the campaign is over.
- **The next mission's game** (campaign vt+0x24, 0x00434e00(1)): unless
  the campaign is over, a new campaign at the next operation and mission
  (0x004345a0) takes over the stores (the player's +0x08 and the salvage
  +0x64), the record +0xb4 (four words), the player's slot, +0xa8 (3 after
  Op 1 Mission 1 or Op 4 Mission 4, else kept), +0xc4, the lancemates;
  it is saved (vt+0x14: `pilots\games\<pilot><op><n>.mw3`) and becomes
  the one with +0xb0 set. With 0 the campaign settles in place (the
  ammo and mechs of each slot back to the store, the lancemates' list
  emptied).
- **The pilot's save after a mission** 0x00478f10(next): for a campaign
  game, the game from vt+0x24(next); when the pilot already has a
  campaign at that operation and mission, "overwrite?" (message 0xfb3,
  caption 7): yes replaces it, no keeps the old one; else it is added.
  It becomes the current game (+0x6c), then the pilot file is written.
  Called by the Win State (1, a mission passed), the launch (0) and the
  briefing's ABORT (0).

Ported (item 1): `mech3::Pilot::record_mission`, `CampaignGame::next_mission`,
`campaign_advance` and the copies they make (pilot.md section 7), with the
loadout's copy and free-slot count (`Config::copy_of`, `Config::recount`,
mechdef-configs.md). Found: the next mission's saved game is written (zSave
0x005a8a70, CREATE_ALWAYS) before the "overwrite?" question, so No keeps the
old entry but its file already holds the new world (fixes.md, to fix when
zSave is wired).

## 5. What is ported, what is a hook

Ported: the main menu and the pilot screen (menus.md 10); the pilot and
its games, the campaign's read and salvage merge 0x00434cb0 (pilot.md);
the states Load, Core, Win, Salvage, Fade (game-loop.md); the mission
load and unload (mission-load.md), the launch 0x00491150 (mech-build.md
8), the mission start (mission-start.md).

Hooks: the briefing screen 0x00405230 and its calls (0x0040a820,
0x00409ec0, 0x00409eb0, the movie 0x00409320), the debriefing 0x0047b430,
the salvage screen 0x0047d3b0, 0x0047ae10 (`mission_results`), the Win
Movie and IA Dummy states, the end screen's fade target, the movies
(zFMV), the mech lab. The program's `--mission` start queues Core in
place of LAUNCH (program.md).

## 6. 640x480

BRIEFINGDIALOG, WINMENU and the salvage screen are 640x480 dialogs like
the other menus (menus.md 7). The briefing map is drawn into the MAP
part's rectangle from the briefing's coordinates; the debriefing's text
columns are fixed pixel places (0x005e5ae4..). At any size the screens
need the menus' one scale or a centred 640x480 frame.

## 7. Porting order

1. (Done) The campaign's progress: 0x00484840, 0x00484870, 0x00434e00 (both
   ways), 0x00435240, 0x00478f10 with its question, on the pilot module
   (test_pilot.py's harness). The launch's and the Win State's hooks
   then call the port.
2. (Done) The briefing screen's shell: 0x00405230 without the map, the
   buttons and their enabling 0x00408ef0, the frame 0x00406440, ABORT
   0x00405050 and LAUNCH 0x00404f70.
3. (Done) The briefing's script and REPLAY (+0x11220): the SOUND block,
   the voice 0x00405e10 with its cue markers 0x00405ed0, the map steps
   0x00405db0 and the objects' show/hide 0x00405d50/0x00405d90.
4. (Done) The briefing map: 0x00406650's reader and its objects.
5. (Done) The program: the Campaign state's path wired (movies skipped while
   zFMV is not ported), the Load state's hooks to the port, LAUNCH to
   the real launch: `mw3` start -> New Campaign -> pilot -> briefing ->
   Launch -> c1m1 (program.md, A campaign from the menus;
   `test_program.cpp` campaign_menus_to_c1m1_through_the_briefing_headless).
6. The debriefing screen 0x0047b430 and its press 0x0047d030.
7. The Win State's results 0x0047ae10 and the end screen's fade to it:
   c1m1 -> debriefing -> c1m2's briefing.
8. The salvage screen 0x0047d3b0 and its state; with it the briefing's
   loadouts (0x00408b40, 0x00408bd0, 0x00408c30) and the reallocation
   list (REALLOC_LIST, the store list 0x0048db30.., 0x00404ae0).
9. The Win Movie and IA Dummy states.

## 8. Testing

Item 2: `test_briefing.py` (40/40 runs; the original skips the
REALLOC_LIST bind with a code hook, stubs the later items' calls; since
item 3 it reads SOUND too and the plan starts the script, sends cue markers
and replays; since item 4 the map objects are built for real, with the nav
points, the mission's map, its projection and the game's texts stubbed, and
the synthetic file adds objects of every kind);
`test_mainmenu.cpp` checks the per_second bar. Breaks: scratch/briefing/
breaks.py.

Item 1: `test_pilot.py` (pilot.md section 7; 100/100) and
`test_mechsave.py` (the loadout copy, 553 copies). In the program the
Win State's 0x00478f10(1) and the launch's 0x00478f10(0) run the port on
the pilot the pilot screen chose (a debug start's stand-in pilot is still
logged); `test_program.cpp` checks the save after a mission on a pilot
made at the pilot screen. Breaking the port showed the tests catch it, 33
of 33 (`scratch/campaign/breaks.py`, 30 runs): the advance at the count,
the end at operation 4, the +0xa8 rule, the +0x64 store, the record +0xb4,
the salvage into +0x50, +0xc4, the +0xb0 flags, the merge after, the world
save's kind, the settled mechs and +0xac, the slot count's minimum, the
slot's +0x0c, the modified loadout's recount, another mech's loadout
refused, the lancemates' reset field, the game filed first, matched by
mission alone, No taken as yes, the result, the store's mech merge and
0x13; the loadout's item merging, its group, a weapon's place, the XL
engine's slots, the head's fixed slots, unplaced items counted, a weapon
without a firepoint, the modified and player-made flags, the six-slot
parts.

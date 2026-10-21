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

dlgWin.zrd's WINMENU: WIN_BACKGROUND, ICON_ABORT (2), ICON_REPLAY (1),
ICON_CONTINUE (0) (the mech lab's help buttons), ICON_PLAYER0..3 (lance cards:
the mech lab's mech button 0x0044a770 with a mech display at HTAL's place,
0x0047e840; no rollover, no press), WIN_TITLE (labels: [1] the result,
[2] "Operation %1 Mission %2" 0x177, [3] the mission's time), ICON_RANK,
SALVAGE_LIST (the mech lab's store list over the mission's salvage
[0x00609990]). By the game:

- A campaign: the result 0x181, or 0x180 when it failed ([0x00609926]);
  CONTINUE's help 0x174 when won without a cheat ([0x00609994]), else 0x173,
  0xfb8 when the campaign is over (the Win State's +0x10); won: the salvage
  list shown and filled, REPLAY shown unless a cheat was on; failed: both
  hidden.
- Instant Action: the title is message 0xdc0 formatted with the operation
  and the game's name (or the name alone), ABORT's help 0xdbf, the result
  0x176, CONTINUE's help 0x173, the salvage list shown, no REPLAY.

The mission's time is `%02d:%02d:%02d` of [0x0060991c]; the seconds are the
time less the minutes only, so past an hour 01:01:3640 (FIX (bug), as the
HUD's timer). Then the results.

### The results (0x0047baf0..0x0047c84b, item 6b)

Inlined at the end of 0x0047b430 (`DebriefScreen::results`):

- **The lance.** zEntity's list in its order ([0x0073655c..]), each
  entity's vehicle (vt+0x2c) and mech (vt+0x50): the player's mech
  ([0x0060861c] +0x14) or one of its allies (vt+0x28's +0xc0 against the
  player's +0xc4), with a type (+0x684 +0xc not -1). The player takes card
  0, the others cards 1, 2, 3 in order; the loop stops when card 3 is
  used, so a fourth ally before the player in the list would leave the
  player off. A card: shown, its vehicle set (0x0047b210), labels [0] the
  pilot's name (vt+0xa4: the player's name, else the brain's 0x004aee70),
  [4] hidden, [5] mechs and vehicles destroyed (+0x3f4 + +0x3f8, the kill
  credit 0x0041b420's counts), [6] the mechs, [7] the vehicles, and a white
  line (0xffff) from x 100 to 5 left of [5]'s text,
  6 under it. The health (vt+0x98 x 100, floored) is read and dropped.
  The cards left over are emptied and hidden; the count goes to
  [0x00608bf8] (read nowhere else). Without a cheat the lance's points for
  mechs (+0x400), vehicles (+0x404) and turrets (+0x408) are summed (the
  kill credit adds them; +0x3fc counts the turrets).
- **The campaign's records.** Each card's pilot is matched to a record by
  name: the player 0, Dominic Paine (0xe14) 1, Epona Rhi (0xe15) 2, Alan
  Mattila (0xe01) 3, anyone else none.
- **The lines.** Columns at x 0xe1 (an indent of 0xf) and the numbers at
  0xe1 + 0xbd; rows of 14, headers of 16 ([0x005e5ae4..0x005e5b04], never
  written). From y 100: a campaign's primary objectives (header 0x16d, when
  [0x0060994c] has any), 2 px, the secondary header 0x16e and its
  objectives ([0x00609950]). Each objective shown (+0x30) with a line
  (0x0045e080(0)): green and 2500 when done (+0x08), red and 0 (message
  0x183 "Failed") when not; a line taller than a row pushes the next down.
  Instant Action has the header 0x16d "Primary Objectives" and nothing
  under it. Then the lance's points, green: "Mechs Defeated" 0xfab (any
  game), "Vehicles Destroyed" 0xfac and "Turrets Destroyed" 0xfad (a
  campaign only).
- **The points.** Instant Action: 1000, or in modes 1 and 2 (the game's
  +0x44) 1000 a wave (+0x70) plus a hundred for each round left
  (+0x6c, one less unless the mission ended won: [0x00609925] and not
  [0x00609926]) at each wave: waves x 1000 + sum over the waves of
  (rounds - i) x 100. A campaign: the objectives and the lance's three.
- **A campaign's totals.** The score (+0x13be0) is the campaign's +0xc4,
  plus the mission's points when it did not fail and no cheat was on. At y
  0xf0: "Campaign Total" 0x171 (large) and the score, a grey line (0xdc
  0xdc 0xdc) under it, 5 px, "Mission Total" 0x170 and the points, 10 px,
  the header "'Mechs Defeated" 0x169 and each record's line: the pilot's
  name and the mechs that pilot has destroyed in the campaign (+0xb4 of
  that record, plus this mission's +0x3f4; into +0x13be4). The Win State copies these
  into the campaign (0x0047ac20, `Engine::win_keep_scores`) when the
  mission counts, by card: card i's number into +0xb4 + 4i, though it was
  read from the record its name matched. They agree while the lancemates
  stand in the list in record order; otherwise one pilot's kills land on
  another's record (a quirk, kept).
- **The top.** At y 0x46: "Mission Total" 0x170 (large) with the mission's points
  (Instant Action's, or the campaign's total) and a grey line, then every
  text joins the dialog.
- **The rank** 0x0047b2f0: the campaign's score (Instant Action: its
  points) / 100,000, 0 to 4: the icon `icon_rank%d_green` (the old one
  released) and the label message 0x1d8 + rank: Sergeant, Master
  Sergeant, Captain, Major, Colonel.

### The lance cards' armor display (item 6c)

Each card's HTAL key builds a display (0x0047e840, new(0x2fb8), vtable
0x005c3de8, a Line2D): eleven bars (0x290 bytes each: two polygons, the
standing armor green over the lost red; the rear torso's three dimmer,
0xaa), the labels "H" "T" "A" "L" (messages 0xf3b..0xf3e, font hud_htal),
a line (1, 0xa2, 0x1f) under the bars. `set_vehicle` 0x0047b210 lays it out
for the vehicle (0x0047eba0), fills it (0x0047ef10) and puts the mech's
icon on the card (0x0047b260 with 0x0044a9b0: `icon_<name>_green`).

- **The layout.** The bars stand at the display's place, `height` tall at
  the most: the scale is height / the most armor any section had (kept
  between vehicles on the same card; each card has its own). Head (width
  wide), then the torso's six half-width bars (right, right rear, centre,
  centre rear, left, left rear: armor 1, 4, 0, 3, 2, 5) a step apart (half
  of the half width when over 4, else 2), the arms, the legs (width wide).
  The labels sit over the head, the centre torso, between the arms and
  between the legs; the line runs under the tops from the head to the last
  leg's end.
- **The fill.** Each bar's green part is ⌊its height x the part's armor
  left⌋ (part vt+0x44 by section: head; torso sections 1, 4, 0, 3, 2, 5;
  the arms; the legs), the rest red below it.

The products and the scale are single precision, as the x87 runs in play
(CLAUDE.md, FPU precision); at 53 bits a bar at exactly its section's most
would come out a pixel shorter.

Ported (debriefing.cpp): `LanceDisplay` with `Bar::place` 0x0047e520,
`Bar::fill` 0x0047e680, `Bar::update` 0x0047e810, `layout` 0x0047eba0,
`fill` 0x0047ef10, `update` 0x0047f0e0; `LanceCard::set_vehicle`,
`set_mech_image`; `mech_icon` (0x0044a9b0, the mech lab's). Engine hook:
`vehicle_armor` (the program reads a ported Mech's loadout armor and parts).
Test: `test_debriefing.py` builds the displays natively, the lance's fake
mechs carry a loadout's armor and parts whose vt+0x44 answers from a
table; the polygon fills (0x0055d010) are recorded and every display is
dumped (bars' points and colours, labels, line); out-of-range fractions
try the clamps. `test_program.cpp`'s loop checks the player's card display
reads the ported mech. Breaks 20 of 23 (`scratch/briefing/breaks13.py`, 12
runs); the three left cannot show: the fill always rewrites the points the
placing sets, and a card shows one vehicle per screen (the kept "most").

The texts are new Texts (0x0047cab0: 0x005a15f0 sets the text, the font,
then x and y; kept at +0x13bd0, deleted by the destructor), numbers through
0x0047cce0 ("%d") and 0x0047cd70 (0 shows message 0x183).

In the program the lance is the ported mechs (`mech_debrief_lance`), the
score and records the pilot's ported campaign (`CampaignGame::c4`, `b4`),
and the Win State's copy goes into the same campaign. Not yet: a
lancemate's name is now its brain's (0x004aee70, AiParams +0xd4), so its
record matches by name (the Win State's copy still keeps a card's +0xb4
when the card had no record line, where the exe writes 0: a decision for
the lead, now that the names are wired); the objective lists ([0x0060994c],
[0x00609950]: objectives.zrd's objects are not ported) are empty; Instant
Action's +0x44, +0x6c, +0x70 are not mapped on the ported `ActionGame`
(scored as mode 0, 1000).

The press 0x0047d030: CONTINUE: a campaign over goes to the Win Movie
State 0x00608be8; passed (and clean): cleanup(0), the Load state (the next
briefing: the Win State's save made it current); failed or cheated: the
lancemates reset (0x00435240(0)), cleanup (the world unloaded only at a
mission 1 or Op 4 Mission 4), the animations reset (0x0050ad60), the campaign's +0xb0 cleared, the Load state
(the same mission). Instant Action: its next round (+0x6c, +0x70), cleanup,
the Load state. The Mech Lab game: cleanup, Load. ABORT: a campaign saves
the pilot (failed or cheated: the lancemates reset first), cleanup(1),
the main menu; Instant Action fades to the IA Dummy State (0x0048ef00:
ECX 0x00608bd0, EDX 0, then 1, 2, [0x005e7274] (2 s), 0, 0, 0). REPLAY: the mission's voice again (0x0047abe0).

Ported (6a, 6b, `mech3/debriefing`): the screen, its texts and buttons,
the press, the results and the rank, the cards' and the salvage list's
shells (`MechButton`, `LanceCard`, `StoreList`'s reader, layout and
update); hooks: the cards' mech display (6c), the store list's rows (the
mech lab), the Win Movie and IA Dummy states.

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
old entry but its file already holds the new world. FIX (bug), behind
`zfix` bugs: next_mission leaves the save to record_mission, which writes it
once the game is filed and not on No (fixes.md; `fix_campaign_save_after_
the_question` in test_pilot_names.cpp; test_pilot.py keeps the exe's order).

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
6. The debriefing screen 0x0047b430 and its press 0x0047d030, in three:
   6a (Done) the shell and the press; 6b (Done) the results
   0x0047baf0..0x0047c84b and the rank 0x0047b2f0, the scores into the
   ported campaign; 6c (Done) the lance cards' armor display (0x0047e840,
   0x0047b210, the icon 0x0044a9b0).
7. (Done) The Win State's results 0x0047ae10 and the end screen's fade to it:
   c1m1 -> debriefing -> c1m2's briefing. The end screen's fade
   (0x0048ef00 on 0x006099a0) is the ported fade_replace to the second Win
   State (`States::end_win`, the missions object's +0xd0); the Win State
   builds the ported debriefing; 0x0047ae10's failed path merges the salvage
   back (0x00434cb0), its won path (the lance into the salvage store,
   0x00472f00 for each of zEntity's list) is item 8's. The launch's game
   answers vt+0x48 (the operation) and vt+0x38 (the campaign's slots), and
   0x00479440 gives it; since 6b the Win State's copy of the scores goes
   into the pilot's ported campaign (`GameEngine::win_keep_scores`).
8. Salvage, in three: 8a (Done) a won mission's salvage, 0x0047ae10's
   won path with 0x00472f00, 0x00472da0, 0x00472ec0 and the store's adds
   0x00471840, 0x00471a20 (section 9); 8b the store list's rows 0x0048dbe0
   (the debriefing's salvage list, the briefing's REALLOC_LIST with
   0x00404ae0, 0x00408bd0, 0x00408b40); 8c the lancemates' loadouts
   0x00408c30 (Done, all three). The salvage screen 0x0047d3b0 and its
   state after.
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

Items 6a, 6b: `test_debriefing.py` (40/40 runs): dlgWin.zrd or a
synthetic one, every game kind, failed, cheated, won, the time; since 6b
the results run in the original on a fake zEntity list (the player's mech,
allies by team, other mechs, typeless ones and entities without a mech;
up to seven, so every card count from 1 to 4 shows), fake objective lists
(none, empty, done, shown, without a line), the campaign's score and
records and Instant Action's mode, rounds and waves; the dump has the
dialog (the texts and the grey lines, a `N` kind for Line2D in
test_twod.py and probe_twod.h), the score, the records' numbers, the card
count and the texts. `test_program.cpp`'s loop checks the score carried
from the campaign through the results into the next mission's game, the
player's card, the lines and the rank. Breaking the results showed the
test catches it, 35 of 35 (`scratch/briefing/breaks8.py`, 12 runs; the
type filter, the clean-only score and the grey line's colour need 40):
the lance's team and type filters, the stop at four cards, the player's
card, the clean-only bonuses, the records' names, the card's labels and
line, the spare cards, the count, Instant Action's points (flat, rounds,
waves), the primary header and the gap, shown and done objectives, their
points, fonts and tall lines, "Failed" for 0, the mechs' points in any
game, the totals' place, the records' numbers and names, the top number,
the rank's argument, step, release and label, the texts' place.

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

## 9. Salvage (item 8a, `mech3/mech_salvage`)

The missions object's constructor 0x0048f290 makes a store (flag 1) at its
+0xc0, [0x00609990]: the mission's salvage. The launch 0x00491150 empties
it; the kill credit 0x0041b420 adds each mech killed (0x00472f00) and each
other vehicle (0x00472ec0); salvage objectives add loadouts (0x00472cf0 at
0x0045e2a6, 0x0045ec82); the debriefing's and the salvage screen's store
lists show it; the next mission's game takes it as its +0x3c.

**0x0047ae10, won** (CL 0; DL from the Win State: 1, or 0 at Op 4
Mission 4): over zEntity's list, the player's mission mech goes into the
campaign's +0x64 store (which the next mission's game adds to its own
store, section 4), and with DL every mech of the player's allies (vt+0x28
+0xc0 & the player's +0xc4) into the salvage store. Failed (CL 1): the
campaign's salvage merged back (0x00434cb0).

**0x00472f00, a mech's salvage** (on a store; none for a mech without its
data +0x684, or an Elemental, type 7):

- the mech itself (0x00471c10) when its torso stands or its head is gone
  (a head shot leaves a whole mech); with a leg off, only on a coin (rand
  & 1);
- its weapons (0x00472da0 with the definition's tech, 0x0041df10): each
  slot of the weapon manager (+0xd4: +0x24 count, +0x40 slots) but TAG
  (0x20), in its loadout item's tech (+0x90 +0x18) or the mech's; a tech
  the weapon is not made in (the table 0x005e6500 through 0x00484940) is
  swapped (anything but Clan becomes Clan, Clan becomes Inner Sphere), and
  a weapon made in neither (TAG) is left. Added through the store's vt+0x10
  (0x00471840: one, its place cleared). A weapon that is not a beam with a
  bin holding rounds (not 0, not infinite) adds those rounds as ammo
  (vt+0x08, 0x00471940) and empties the bin (0x004b4d10(0)); bins are
  shared by type, so a type's rounds come once;
- its loadout's (+0x3ec) equipment (kind 3) but heat sinks (9, 10), each
  by its count (0x00472630), unless it sits on a part (0x0041d510 by its
  place) that is destroyed;
- the heat sinks past the engine's ten (+0x65c - 10), single or double
  (+0x660), in the sinks' tech (+0x658), through vt+0x0c (0x00471a20: kind
  1's tech is always 3).

**0x00472ec0, another vehicle**: on a coin its weapons, Clan tech first.

Ported: `add_salvage`, `add_weapons`, `add_vehicle_salvage` and
`lance_salvage` over a `SalvageMech` (what 0x00472f00 reads; the program
fills it from a ported Mech in app_mech.cpp, the probe from the test's
setup); `Store::add_weapon` and `Store::add_equipment`; `weapon_has_tech`
(mech_names). In the program 0x0047ae10's won path runs on the ported
mechs into the pilot's campaign and `mission_salvage()`, which the launch
empties and the next mission's game takes. The kill credit's calls wait
for the kill credit's port.

Test: `test_salvage.py` (200/200): test_pilot.py's emulator, the store
filled through its own adds and add_copy, 0x0047ae10 over fake mechs
(parts standing, destroyed or missing, weapons of every type and tech,
beams, shared, empty and infinite bins, loadouts, sinks), the campaign's
+0x64 store real; compared: both stores' items, the bins, rand's calls.
`test_program.cpp`'s loop checks the player's mech reaching the next
mission's store. Breaks 19 of 21 (`scratch/briefing/breaks9.py`, 80 runs,
200 for three); the two left cannot show: TAG's skip and the skip after
the tech swap (only TAG is made in neither tech).


### The store list's rows (item 8b, first half)

`StoreList::refill` 0x0048dbe0 (the mech lab's list, the debriefing's
SALVAGE_LIST, the briefing's REALLOC_LIST): the old rows freed, then for
each kind listed (+0x204: mechs, weapons, ammo, equipment) a header
(messages 0x294..0x297, with +0x209) and a row for each of the store's
items of that kind (0x0048d9a0): one the list's +0x200 filter (a function,
set by nothing ported) takes, a weapon or equipment whose tech is in
+0x210. A row's text is the item's label (vt+0x1c): "name (n)" unless one
or the store's own, ammo "name (n rounds)" (0x1c4) or "name (unlimited)"
for the store's own (0x1c3), armor "name (2.5 tons)" (half a ton each).
Names (vt+0x20) are Mech3Msg.dll's by table (mechs 0xd82.., weapons
0x2d7.. with LB-10X AC at 0xe6 and 0x2ee skipped, equipment 0x1c6..),
" (CLAN)" after a Clan weapon or piece of equipment. The row's value is the
item; the selection stays on the item it was on, else on its index. With
+0x20d the tonnage (each item's weight summed as floats, "%.2f") goes into
the title, message 0x299.

Ported: `StoreList::refill`, `add_row`, `selected_value`; `item_name`,
`item_label` (mech_salvage.cpp, the original's file for 0x00473700..);
`mech_name_message`, `weapon_name_message`, `equip_name_message`
(mech_names). Engine hooks: `store_of`, `item_handle`, `store_list_filter`,
`message_format_sd`. Test: `test_debriefing.py` refills its salvage list
from a real store in the emulator (items with the exe's vtables; sprintf
on c_format, which now has %f); the dump lists the rows, their values and
the title; a plan step sets the list's kinds, header, tonnage and techs,
selects a row and refills. Breaks 16 of 18 (`scratch/briefing/breaks10.py`,
40 runs); the two left cannot show: refill passes only the kind it
lists, so the kinds check in 0x0048d9a0 always holds, and ammo rows use
their own messages, not the label.

### The briefing's reallocation (item 8b, second half)

A campaign's mech lab works on one store built by 0x00408b40: the game's
store (+0x08) and the campaign's salvage (+0x3c) together (any other
game's lab opens on the game's store). Back from the lab the Load state
calls 0x00408bd0 and then 0x00404ae0(1):

- **0x00408bd0 / 0x00408850, the split.** The room is the campaign's +0xa8
  (3 in practice, section 4) times 300 t (0x00470cc0) and 2 mechs
  (0x00470cd0). Out of the lab's store (0x00408690, "take"): first each of
  the game's old items, as many as the lab still has, while they fit the
  room (a mech by count, anything else by its weight for n, n - 1, ...);
  then each of the salvage's old items, as many as the lab has, with no
  limit. Those become the game's store and the salvage. What is left in the
  lab's store (new combinations, merged items) goes item by item to the
  game while it fits, the rest to the salvage. What went to the salvage is
  also the briefing's +0x111ec, the reallocation list's store.
- **0x00404ae0, the list shown** only when that store has something: the
  six buttons off, REALLOC_LIST (no headers, +0x20a set) and REALLOC_OK on;
  OK (button 7) hides them again (0x00404ae0(0)).

`store_take` copies an item the lab's store refuses to give up (the
store's own, b10) all the same, as the exe does. A mech short of room
counts its shortfall as left over: with +0xa8 at 0 the first take has no
limit, the game keeps every mech, and the salvage then gains the game's
excess on top of the lab's mechs (more mechs than there were). +0xa8 is
never 0 in play (section 4); kept, and the test runs it.

Ported (briefing.cpp): `store_tons` 0x00409fd0, `store_mechs` 0x00472970,
`store_count_like` 0x00473510, `store_take` 0x00408690, `store_reallocate`
0x00408850, `lab_store_build` (0x00408b40, with 0x00408660),
`BriefingScreen::build_lab_store`, `refresh`, `show_realloc`; REALLOC_LIST
bound. Engine hooks: `game_store`, `campaign_salvage`, `campaign_units`,
`store_handle` (stores and items get handles from a table by default). The
program's Load state calls the ported refresh and show on the briefing.

Tests: `test_salvage.py` runs 0x00408b40, the lab's edits (adds and
removals through the exe's store functions) and 0x00408bd0 on a fake
briefing over a campaign or another game (weapons' tons from weapons.zrd's
MASS, as test_mechsave.py), compared on the game's store, the salvage,
the reallocation store and the lab's (200/200; about a third leave
something over). Breaks 16 of 18 (`scratch/briefing/breaks11.py`, 40
runs): the two left cannot show (the take's two limits always come from
one count; +0x20a is read by nothing ported). `test_briefing.py` binds REALLOC_LIST for real now (no
skip), runs 0x00408b40 on the mech lab's button over a campaign with
salvage, and a plan step for the lab's return (0x00408bd0, 0x00404ae0(1)).

### The lance's loadouts (item 8c, 0x00408c30)

When the briefing's load ends (`set_loaded`), each slot of the game (vt+0x3c
the count, vt+0x34 one: the player's first, then the lancemates) is fitted
to the mech lab's store (0x00408b40), then the split runs (0x00408bd0):

- The slot's mech must be in the store (0x00409f00); if not, the next mech
  by roster number (round the 19) that is; none: the slot is emptied (mech
  -1, no loadout). A slot whose mech has no definition is emptied too.
- A loadout of another mech goes back into the store (0x00433df0 with the
  store: 0x00473400 gives the old one's items back) and the slot has none.
- No loadout: the definition's first (its ConfigList's [0]), copied out of
  the store (0x00424010 with the store and `take`: the copy cut to the
  items the store holds when it lacks some (0x004735f0, each item cut, the
  emptied dropped, marked modified, the sinks recounted), then taken out
  of it (0x00473440)). A loadout: copied once cut to the store, then that
  copy copied out of the store into the slot.
- The mech itself is taken out of the store (0x00472a30).
- At Operation 4 Mission 6 the lancemates (not the player) keep their mechs
  and loadouts as they are, the store untouched.

Ported: `lance_slot` and `BriefingScreen::lance_loadouts` (briefing.cpp);
`config_copy_store` (0x00424010 whole), `GameSlot::set_config` with a
store (0x00433df0 whole), `Store::has_config` 0x004735f0, `take_config`
0x00473440, `give_config` 0x00473400, `remove_config` 0x00472d40,
`has_mech` 0x00409f00, `count_like` 0x00473510 (pilot_games.cpp);
`Config::copy_of` takes a step before the recount. Engine hooks:
`game_slot_count`, `game_slot`. Test: `test_salvage.py`'s lance part, on
stand-in definitions (firepoints as test_mechsave's, a first loadout or
none, built with the exe's 0x00423d00 and 0x004723c0), up to four slots
with or without loadouts (some of another mech), the stores filled, Op 4
Mission 6 or not; compared: each slot's mech and loadout (items, modified,
sinks, free slots) and the four stores (200/200; the probe runs the
port's own loop, `lance_fit` with `lance_keeps`). test_briefing.py runs
0x00408c30 for real with no slots. Breaks 17 of 19
(`scratch/briefing/breaks12.py`, 60 runs); the two left cannot show here:
0x004735f0's second store and 0x00473400's mech are passed only by the
mech lab. test_salvage.py keeps test_pilot.py's emulator but lets the
loadouts' copy, recount and destructor run as they are, with sprintf and
_strdup stubbed.

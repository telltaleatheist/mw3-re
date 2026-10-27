# The front end: census and plan (2026-10-25)

What a player meets outside a mission, screen by screen. For each: the
original's object, what is ported, what runs in the program now, what is
missing and its size, then a porting order in small items. Built from
campaign.md, menus.md, mainmenu.md, pilot.md, mechlab.md and game-loop.md,
the headless tests, and live runs of `build/mw3` through the menus (the
menu driver below). Function counts come from the address ranges the docs
give (`analysis-output/decomp`, with the port's `Mech3.exe 1.2 @` comments
for "ported"); ranges take in neighbouring helpers and small destructors,
so they are estimates.

## 0. How the front end is checked live

`build/mw3 -nofmv --write-dir DIR` starts at the main menu. For checking
(ours): `--click-at X,Y@N` clicks at a game pixel (640 x 480) after frame
N (the cursor put there through zInput's 0x0051b440, then the press and
the release a frame apart), `--tap DIK@N[:M]` presses a key, `--shot F.bmp
--shot-frame N [--shot-series K]` writes frames. The walk used here:

```
build/mw3 -nofmv --write-dir DIR \
  --click-at 70,60@40                      # CAMPAIGN
  --tap 0x14@70 --tap 0x12@74 --tap 0x1f@78 --tap 0x14@82   # a pilot "TEST"
  --click-at 470,320@100                   # ACCEPT
  --shot brief.bmp --shot-frame 2500       # the briefing, loaded
```

Seen: the main menu (all seven buttons), the pilot screen (entry, list,
Operation panel, RENAME / DELETE, ACCEPT / CANCEL), c1m1's briefing (its
map, the script's Start marker, the loading bar, the five buttons; MECHLAB
and SALVAGE grey on Op 1 Mission 1, as the original's 0x00408ef0 has it).
Past LAUNCH the mission runs; the debriefing needs a won mission, so the
rest of the loop is checked headless
(`campaign_menus_to_c1m1_through_the_briefing_headless`: c1m1 played, the
debriefing, CONTINUE, c1m2's briefing, MECHLAB into the ported lab and
ACCEPT back).

## 1. The campaign loop

| Screen | Original | Ported | In the program now | Missing | Size (fns, ported) |
|---|---|---|---|---|---|
| Main menu | 0x00486260, MenuScreen 0x005bc190; Main state | yes (mainmenu.md) | draws, every button opens its state | the ripple's draw 0x00558dd0 | 17, 6 (the rest the states') |
| Pilot screen | 0x00486f40, vtable 0x005c5060 | yes (menus.md 10) | new pilot, list, rename, delete (the game's message box) | the Action screen after it (IA) | 35, 25 |
| Campaign state / Operation | 0x0048b000, 0x0048af20 | yes | ACCEPT goes on to the Load state | the movie "campaign" (zFMV) | 12, 6 |
| Briefing | 0x00405230 (BRIEFINGDIALOG, brief_c<op><n>) | shell, script, map, REPLAY, LAUNCH, ABORT, the reallocation list, the loadouts (campaign.md 2, 9) | yes: map, voice, markers, buttons by the game | its movie (c<op>m<n>.avi, zFMV); SALVAGE's screen (below) | 109, 51 (+52, 14 movie and load glue) |
| Mission | Core state | yes | yes | (the HUD and mission work) | |
| End screen, Win State | 0x00491bf0, 0x0047ab10 | yes: the fade, the results 0x0047ae10, the won salvage 0x00472f00 | yes | | 18, 11 |
| Debriefing | 0x0047b430, new(0x13bf4) | yes: shell, press, results and rank, the lance cards (campaign.md 3) | yes (headless; live needs a won mission) | its salvage list's rows are 8b's (done) | 17, 8 |
| Salvage screen | 0x0047d3b0 (dlgSalvage.zrd), Salvage State 0x0047f220 (0x00608ee8, 0x00608bd8) | yes (item 1, campaign.md 10) | moves items both ways, ACCEPT back | | 28, 8 + 22, 1 |
| Mech lab | Lab State 0x00448830, screen 0x0044b920 | steps 1-4, 5a, 5b1 (mechlab.md 5): the state, the shell, the chassis and loadouts pages, the item lists' base, the unassigned list's shell | from c1m2's briefing MECHLAB opens it, ACCEPT returns | 5b2 the item rules, 6 the criticals page, 7 the weapon groups, 8 engine, sinks, armour and the data panels, 9 the 3D view, 10 the loadout reaching the launch | 159, 66 (state and screen), 71, 43 (item lists), 9, 1 (3D world) |
| Next mission | debriefing CONTINUE (0x0047d030) | yes | c1m1 -> c1m2's briefing; c1m4 -> c2m1's (the opener asked for) | the movies themselves (zFMV) | |
| Save / load | the pilot's games (pilot.md), 0x00478f10 after a mission, "Replace existing game?" | yes (the files and the question) | saves after each mission; the pilot screen lists the games | done: zSave, the anim registrants, the headless and live round trips, "Replace existing game?" (save.md) | 119, 44 + 33, 24 |
| Win Movie, IA Dummy | 0x0047a960 (0x00608be8), 0x0047ce80 (0x00608bd0) | Win Movie yes (no movie until zFMV) | Op 4 Mission 6 won -> the credits | IA's tallies (the IA Dummy State) | small |

## 2. Instant Action, training, multiplayer

- **Instant Action**: main menu -> pilot screen -> the Action screen
  (0x00488bc0, its state 0x0048aac0; the line-up, the mode, the wave) ->
  Load (the briefing's IA layout: no SALVAGE or REPLAY) -> mission
  (`ia<n>` objectives, ported) -> debriefing -> IA Dummy State (0x0047ce80).
  Ported end to end (items 12a and 12b): the Action screen and its
  ActionGame, the store, the launch, the header, the IA Dummy State.
- **Training**: main menu -> pilot -> the training game (type 4, t1) ->
  Load (training's briefing: no MECHLAB, SALVAGE, REPLAY) -> the lessons
  (objectives.md, Training's types and lessons) -> the end screen's fade to
  the TR Dummy State (campaign.md 12) -> Main, training again. Ported
  (item 13).
- **Multiplayer**: out of scope (the network work).

## 3. The movies (zFMV, Indeo 5)

fmv.zrd's script (reader.zbd): INTRO (intro.avi, then the SPLASH logos
mech3splash / mech3unload as images), ATTRACT, `campaign` (campaign.avi
after a new campaign's first pilot), one per mission played before its
briefing (`c<op>m<n>`, each operation's first with its opener c<op>.avi),
`c<op>win`, the demo blocks, UNLOAD (unplayed). Its verbs: PLAYAVI, PLAYMCI,
LOADIMAGE, SHOWIMAGE, BLITIMAGE, FADEIN, FADEOUT, BLUR, BLURH, BLURV, WAIT,
PLAYSOUND. The install has
19 of the AVIs (`video/`; c3m3, c1win..c3win are disc-only). zFMV
(0x00598d80..0x0059b2f0, about 56 functions): the scripts are ported
(item 10, fmv.md) and the AVI player too (item 11), on the importer's
files: without them (mw3import not run) PLAYAVI ends at once, as the
original's without the file. With `-nofmv` the Intro state skips INTRO
(game-loop.md); SPLASH plays either way.
Indeo 5 (IV50) has no system decoder on macOS; FFmpeg's libavcodec decodes
it. Decided (Owen, 2026-10-26): the first-run importer converts the AVIs
once (libavcodec decodes the Indeo 5, only in the importer), and the game
plays the converted files with a small built-in decoder: MPEG-1 video
through pl_mpeg (one header, MIT), at 30 fps with every frame twice (MPEG-1
has no 15 fps); the sound stays the AVI's own PCM (the lead, 2026-10-27: the
refills then read the original's bytes). fmv.md section 6. Assets stay out
of the repo.

## 4. Porting order

Each item is one sitting, with its test plan; the campaign loop first, so
Owen can play c1m1 -> c1m2 with salvage and the lab.

0. **(Done) The menu driver** (`--click-at`, `--tap`, `--shot` from the
   plain start) **and `--briefing c1m2`** (program.md): a pilot whose newest
   saved campaign stands at the mission, then the player's clicks (CAMPAIGN,
   the pilot, ACCEPT), so the real Campaign and Load states show the
   mission's own briefing. (Stopping a debug launch at its briefing instead
   needs the pilot's game the Load state reads; going through the menus does
   not.) Checked: c1m2's briefing with MECHLAB and SALVAGE lit; MECHLAB opens
   the lab (its bay, the chassis page), whose stockpile is empty: a new
   campaign's store fill (0x00470e30, its mechs 0x004733b0; pilot.md Left)
   is not ported, so this start's store has nothing. Added as item 1a.
1a. **(Done) A new campaign's store**: already ported. A new campaign
   (0x004345a0, Op 1 Mission 1) gets its stock through `store_add_mech`
   (0x004733b0) and 0x00434e00 carries the stores, the slots and the field
   bases (+0xa8, 3 at each operation's first mission) on. The empty lab was
   `--briefing`'s: it built the game at the mission itself (no stock, and no
   field bases, which would have made the salvage screen's capacity 0); it
   now steps the pilot's new campaign on with 0x00434e00 and the lab shows
   the Bushwhacker (Primary, GUNMETAL) at 55 t. In the real loop c1m2's
   store holds no spare mech, weapons, the three ammo stocks and equipment
   (now checked in campaign_menus_to_c1m1_through_the_briefing_headless;
   break: no stock, a spare mech appears). 0x00470e30 is the other games'
   "everything" store (lab, Instant Action, multiplayer, training): with
   item 12.
1. **(Done) The salvage screen** (campaign.md section 10; test_salvage_screen.py,
   test_salvage.py's moves, the campaign test). (0x0047d3b0, dlgSalvage.zrd) and the Salvage
   State it runs in (0x0047f220, the briefing's SALVAGE 0x0047d2f0): its
   two lists (the salvage store and the campaign store, the ported
   StoreList rows), moving items, ACCEPT back to the briefing. Test: a
   differential of the screen's making and presses against the exe (as
   test_debriefing.py does), break tests; headless: c1m2's briefing,
   SALVAGE, an item moved, ACCEPT, the store changed; a screenshot.
2. **Lab 5b2, the item rules** (0x00453840 may it go on, 0x00454600 put
   it on, with the Config helpers they ask). Test: test_lab_screen.py's
   sessions with adds against the exe; breaks.
3. **Lab 6, the criticals page** (ASSIGNED_LIST 0x005c1fe0, the part
   buttons, items into parts' slots). Test: test_lab_screen.py criticals
   sessions; breaks.
4. **Lab 7, the weapon groups** (WEAPONS_UNGROUPED / WEAPONS_GROUPED).
   Test: as 3, plus the groups reaching the mech's weapon manager at the
   launch (headless).
5. **Lab 8, engine, sinks, armour and the data panels** (ENGINE_TABLE, ids
   14..30's presses, MECH_DATA, CHASSIS_DATA, ARMOR_TITLES, LOADOUT_LIST).
   Test: as 3; the weights and the tonnage limit against test_mechsave's
   table.
6. **(Done) The lab to the launch** (mechlab.md 10): a mech and loadout changed in
   the lab reach c1m2's launch. Test: headless campaign loop through the
   lab (a weapon swapped, the launched mech carries it); a screenshot of
   each lab page from `--briefing c1m2`.
7. **(Done) Save and load, end to end**: c1m1 won, the game saved, the app quit;
   a new app, the pilot's saved game chosen, c1m2's briefing with the
   salvage and the lab's loadout kept. Test: headless across two app runs
   in one process (reset_process_state between), and the "Replace existing
   game?" path. In three parts (save.md): save 1 (done) zSave, the
   campaign's read-back and the headless round trip in the campaign test;
   save 2 (done) the anim registrants (what a saved game holds in MW3); save 3 (done) the
   live round trip and the "Replace existing game?" path (`tools/analysis/savecheck.py`).
8. **(Done) An operation's end and the campaign's** (the Win Movie State
   0x0047a960, the credits flag, the next operation's first mission and
   its opener). Test: headless: c1m4 won leads to c2m1's briefing; c4m7
   (the last) to the Win Movie State and the main menu's credits.
9. **(Done) The lab's 3D view** (labworld, labwindow, labcam: 0x0044ffc0,
   0x00450390, 0x00450690; the armour section's highlight 0x004504d0 /
   0x00450470: the parts of the section chosen blended on the model; the
   exe has no mouse picking, 0x004507c0 is the chassis lookup): with the
   render port's pieces it needs. In two: 9a (done) the world, camera and the
   turning model (a screenshot per chassis, the turn and camera headless);
   9b (done) the section highlight (headless, against the exe).
10. **(Done) zFMV's script without video** (fmv.md): fmv.zrd's reader and
    every verb but PLAYAVI (a stub that ends at once) and PLAYMCI (unused),
    the blur passes (the pause screen's too). Test: test_fmv.py, the calls
    of every shipped block and random ones against the exe; the SPLASH
    logos in a shot (`--shot-any --shot-frame 3`).
11. **(Done) The movies' video** (PLAYAVI, fmv.md sections 5 and 6): the
    player (fmv_stream.cpp) ported on Video for Windows calls answered from
    the importer's files (`mw3import`: the picture MPEG-1 at 30 fps doubled
    for pl_mpeg, the sound the AVI's own PCM, the headers in a sidecar).
    Test: test_fmv.py with movies (every call against the exe); `mw3 --movie`
    plays each block to its AVIs' length; the intro's first frames in shots.
12. **Instant Action**, in two:
    - 12a. **(Done) The Action screen** 0x00488bc0 and its state, the
      ActionGame filled by ACCEPT (menus.md 13; test_actionscreen.py).
    - 12b. **(Done) The launch from it**: the 0x00470e30 "everything"
      store (pilot.md; test_store_fill.py), the Load state's Instant Action
      mission (the world, the ia mission), the 0x231 header's arguments
      (hud.md 24), the IA Dummy State and CONTINUE's round (campaign.md
      11; test_actionscreen.py), the kill tallies; headless:
      instant_action_menus_to_wave1_tallies_headless.
13. **(Done) Training**, in two: 13a the fourteen training condition
    types (objectives.md, Training's types; test_objectives.py's t1 blocks,
    test_objective_check.py); 13b the lessons (objectives.md, Training's
    lessons; test_training_lessons.py), the "training sequence's fade",
    which is the TR Dummy State (campaign.md 12), the program's answers for
    the checks; headless training_menus_to_lesson3_headless (m11, m12, m13).

Multiplayer stays with the network work.

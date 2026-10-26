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
| Next mission | debriefing CONTINUE (0x0047d030) | yes | c1m1 -> c1m2's briefing | an operation's last mission: the op's win movie and the next op's opener | |
| Save / load | the pilot's games (pilot.md), 0x00478f10 after a mission, "Replace existing game?" | yes (the files and the question) | saves after each mission; the pilot screen lists the games | an end-to-end check: quit after c1m1, restart, the saved game resumes at c1m2 | 119, 44 + 33, 24 |
| Win Movie, IA Dummy | 0x0047a960 (0x00608be8), 0x0047ce80 (0x00608bd0) | no | | the campaign's end (c4win, the credits flag [0x00608e4e]) and IA's tallies | small |

## 2. Instant Action, training, multiplayer

- **Instant Action**: main menu -> pilot screen -> the Action screen
  (0x00488bc0, its state 0x0048aac0; the line-up, the mode, the wave) ->
  Load (the briefing's IA layout: no SALVAGE or REPLAY) -> mission
  (`ia<n>` objectives, ported) -> debriefing -> IA Dummy State (0x0047ce80).
  Not ported: the Action screen and its ActionGame filling (a debug start
  makes the game itself), the IA Dummy State; the Instant Action header of
  the objectives box (hud.md 24's open note). About 31 functions, 3 ported.
- **Training**: main menu -> pilot -> the training game (type 4, t1) ->
  Load (training's briefing: no MECHLAB, SALVAGE, REPLAY) -> the lessons:
  objectives.zrd's fourteen training condition types are unported
  (objectives.md: "piece 5"), so the lessons never complete; the training
  sequence's fade (0x006098c8) is unported.
- **Multiplayer**: out of scope (the network work).

## 3. The movies (zFMV, Indeo 5)

fmv.zrd's script (reader.zbd): INTRO (intro.avi, then the SPLASH logos
mech3splash / mech3unload as images), ATTRACT, `campaign` (campaign.avi
after a new campaign's first pilot), one per mission played before its
briefing (`c<op>m<n>`, each operation's first with its opener c<op>.avi),
`c<op>win`, the demo blocks. Its verbs: PLAYAVI, LOADIMAGE, SHOWIMAGE,
BLITIMAGE, FADEIN, FADEOUT, BLUR, WAIT, PLAYSOUND, UNLOAD. The install has
19 of the AVIs (`video/`; c3m3, c1win..c3win are disc-only). zFMV
(0x00598d80..0x0059b2f0, about 56 functions) is not ported. With
`-nofmv` the states skip the movies (game-loop.md); without it the Intro
state asks zFMV (0x00598cc0, 0x00598d80 with "fmv.zrd" "INTRO",
0x005996d0), which answers 0 as unported, and goes on to the main menu; the
Campaign and Load states skip theirs. Nothing plays and the splash logos
are not shown.
Indeo 5 (IV50) has no system decoder on macOS; FFmpeg's libavcodec decodes
it. A decision for Owen: decode at run time with libavcodec, or convert
the AVIs in the first-run importer (assets stay out of the repo either way).

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
6. **The lab to the launch** (mechlab.md 10): a mech and loadout changed in
   the lab reach c1m2's launch. Test: headless campaign loop through the
   lab (a weapon swapped, the launched mech carries it); a screenshot of
   each lab page from `--briefing c1m2`.
7. **Save and load, end to end**: c1m1 won, the game saved, the app quit;
   a new app, the pilot's saved game chosen, c1m2's briefing with the
   salvage and the lab's loadout kept. Test: headless across two app runs
   in one process (reset_process_state between), and the "Replace existing
   game?" path.
8. **An operation's end and the campaign's** (the Win Movie State
   0x0047a960, the credits flag, the next operation's first mission and
   its opener). Test: headless: c1m4 won leads to c2m1's briefing; c4m7
   (the last) to the Win Movie State and the main menu's credits.
9. **The lab's 3D view** (labworld, labwindow, Blabcam: 0x0044ffc0,
   0x00450390, 0x00450690, the part picking 0x004507c0..): with the render
   port's pieces it needs. Test: a screenshot per chassis; the picking
   headless.
10. **zFMV's script without video**: fmv.zrd's reader and verbs other
    than PLAYAVI (the SPLASH logos, fades, waits, sounds). Test: a
    differential of the script's run (its calls) against the exe; the
    intro's logos in screenshots.
11. **The movies' video** (PLAYAVI): Indeo 5 by Owen's choice (libavcodec
    at run time, or the importer's conversion), drawn as zFMV draws. Test:
    the intro's first frames in screenshots; a movie's length in frames.
12. **Instant Action**: the Action screen 0x00488bc0 and its state, the
    ActionGame from it, the IA Dummy State. Test: differential of the
    screen; headless IA wave 1 played to the tallies.
13. **Training**: the fourteen training condition types (objectives.md
    piece 5) and the training sequence's fade. Test: test_objectives.py's
    shipped t1 blocks; headless t1 lesson 1 completed.

Multiplayer stays with the network work.

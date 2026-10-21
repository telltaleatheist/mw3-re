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

Each frame (0x00406440): the load polled (0x00490930) into 0x00408ef0,
which on the change enables the buttons by game type (a campaign: all
but two at Op 1 Mission 1; Instant Action: two fewer; training: three
fewer; multiplayer: none, and it launches by itself as soon as the load
ends; which flag reaches which part is checked in item 2); the progress bar TRANSPORT_PROGRESS creeps up by 0.005 a
frame towards the load's fraction (1.0 once loaded; a per-frame step, so
a per_second candidate under Owen's rule); mouse buttons stop the sounds.

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

## 5. What is ported, what is a hook

Ported: the main menu and the pilot screen (menus.md 10); the pilot and
its games, the campaign's read and salvage merge 0x00434cb0 (pilot.md);
the states Load, Core, Win, Salvage, Fade (game-loop.md); the mission
load and unload (mission-load.md), the launch 0x00491150 (mech-build.md
8), the mission start (mission-start.md).

Hooks: the briefing screen 0x00405230 and its calls (0x0040a820,
0x00409ec0, 0x00409eb0, the movie 0x00409320), the debriefing 0x0047b430,
the salvage screen 0x0047d3b0, 0x00478f10 (`pilot_record_mission`,
`LaunchHooks::pilot_478f10`), 0x0047ae10 (`mission_results`), the Win
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

1. The campaign's progress: 0x00484840, 0x00484870, 0x00434e00 (both
   ways), 0x00435240, 0x00478f10 with its question, on the pilot module
   (test_pilot.py's harness). The launch's and the Win State's hooks
   then call the port.
2. The briefing screen's shell: 0x00405230 without the map, the buttons
   and their enabling (0x00408ef0, 0x00404ae0), the frame 0x00406440,
   ABORT 0x00405050 and LAUNCH 0x00404f70.
3. The briefing's sound list and REPLAY (+0x11220).
4. The briefing map: 0x00406650's reader and its drawing.
5. The program: the Campaign state's path wired (movies skipped while
   zFMV is not ported), the Load state's hooks to the port, LAUNCH to
   the real launch: `mw3` start -> New Campaign -> pilot -> briefing ->
   Launch -> c1m1.
6. The debriefing screen 0x0047b430 and its press 0x0047d030.
7. The Win State's results 0x0047ae10 and the end screen's fade to it:
   c1m1 -> debriefing -> c1m2's briefing.
8. The salvage screen 0x0047d3b0 and its state.
9. The Win Movie and IA Dummy states.

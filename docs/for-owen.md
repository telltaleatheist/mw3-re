# For Owen: what waits on you

Collected 2026-10-28 (mission-plan item 23) from `playtest.md` and the
Candidates in `fixes.md`: each item with what was found, the evidence and a
recommendation. Nothing here is changed in the port; each item is a
decision or a check in play. The sources keep the full detail.

## 1. The playtest rows

### To check in play (fixed, waiting on your look)

From the 2026-10-21 and 2026-10-25 runs. Each was fixed and tested
headless; only you can say whether it now looks and feels right.

| # | What to look at |
|---|---|
| 1 | The main menu buttons ripple on hover |
| 2 | "Ok to delete pilot?" is the game's own box, not an OS alert |
| 3 | Escape in a mission shows the pause menu (no purple screen) |
| 7, 20 | Keypad 0 shows the zoom window, one click sound per change |
| 8 | The HUD's weapon lines show each weapon's recharge |
| 13 | Z, keypad 0, F12, Q, T, R, Shift+R, D, Ctrl+D and Alt+R reach the HUD |
| 21 | Quitting through the pause menu: the quit box's prompt and answers |
| 24 | A short laser click on the move: the beam leaves from the arm now, not a metre behind (behind `bugs`) |
| 29 | The MFD (D): your mech from the front, the parts diagram, the armour bars; TARGET's view |
| 30 | The radar's mission boundaries (yellow inside, red outside) |

Recommendation: one c1m1 run through these. Mark each row in
`playtest.md` closed when it looks right.

### Done since the table was written

These rows still read "waiting" in `playtest.md`, but they have been
ported since:

- **#11, trucks, APCs and mobile HQs**: built and on the radar
  (vehicles.md, steps 1a and 1b; 2026-10-23 to 10-25).
- **#18, the lance orders (F6..F11) and Order MFB**: the brain's orders
  and the radio ported (ai.md section 6, step 7; keybindings.md rows
  0x4f..0x56).
- **#26, F12's objectives text**: the box fills from the mission's
  objectives (hud.md section 24), and Training's lessons show in it
  (mission-plan item 21's shot: "TRAINING: Starting your 'Mech").

Recommendation: try each in play (c1m1 has trucks, APCs and the mobile HQs; any campaign mission
from c2 has a lancemate), then close the rows.

### Needs your repro

- **#22, ground textures blink.** Not reproduced: c1m1 still and walking
  in the cockpit, c1m1 still and turning outside, c1m2 and c2m1 still, all
  without a blink (`tools/analysis/blinks.py` on shot series). Found while
  looking: a parked vehicle's edges on c1m2's start view change for one
  frame (render.md section 6c), likely two faces of one surface fighting
  in depth, not traced. Needed: the mission, where you stood, what you were
  doing, and whether the blink is a hole, a flash or a wrong texture. Also
  whether the original shows c1m2's vehicle edge flicker at the start.
- **#27, the missile turrets don't die.** Not reproduced: every c1m1
  turret dies to hits on it (`mission_c1m1_shots_destroy_the_srm_turrets_headless`,
  vehicles.md "Shots"). One trap: tur_srm02 sits in a trench whose walls
  take shots from across it. Needed: which turret, which weapon, and from
  where.

### Reopened: firing lag (#23)

You saw a fully charged weapon lag, with the button needing a hold.

- **Found**: every short click reaches the weapons, mouse or key, five in
  a frame (`mission_c1m1_short_clicks_fire_headless`). A beam fires only
  from a full charge (0x4b3fc0), and the trigger is an edge, so a click
  while the bar fills is dropped. A fully charged beam fires on the click's
  frame (`sdl_program_mouse_click_fires_the_laser`: pressed for frame N,
  fired on N).
- **Not yet known**: whether the lag you felt on a full charge is the
  port's frame pacing, or the original's too.
- **Recommendation**: on your PC, in the original, take a medium laser
  at full charge and give it one short click. Note whether it fires at
  once, then do the same in the port. If the original fires at once and
  the port doesn't, it's a port bug and I'll chase it. If both lag, it's
  the design. The dropped click during a recharge is Candidate A1 below.

## 2. The Candidates

Each looks like a bug or a limit, but the fix would be a guess at intent,
or it changes balance. "Keep" means the original as played.

### A. Weapons and firing

- **A1. A click during a beam's recharge is forgotten** (playtest #23;
  weapons.md, Beams). Buffering it, so the beam fires when full, would
  change the feel of every beam weapon. *Recommendation*: decide after
  #23's check. If the original drops these clicks too, keep it.
- **A2. The AI never alpha-strikes** (ai.md 17.7; 0x4b17a9 doubles a
  sum that starts at 0). A fix makes AI pilots fire everything at once
  when their heat allows: a big jump in how hard enemies hit.
  *Recommendation*: keep.
- **A3. The AI fires whole groups nearly always** (the 0.8 constant mixed
  up, ai.md 17.7). The likely intent was an 80% share; a fix makes them
  fire single weapons more often (softer). *Recommendation*: keep; it is
  how the game has always played.
- **A4. A missile launcher in a part with no scattering firepoint** fires
  from another firepoint (mechlab.md, The item rules). It cannot happen
  with the shipped data, only with modded mechs. *Recommendation*: keep.
- **A5. Lasers' heat glow on vehicles lights nothing** (vehicles.md, The
  glow): the light takes IMPACT HEAT's colour but keeps the pool's 0.1 to
  0.2 m range. *Recommendation*: change it, behind `bugs`. It's visual
  only, the colour shows the glow was meant, and the rendering rule
  allows better than the original.

### B. Vehicles and the AI

- **B1. Vehicles pass through walls and buildings** except when sliding
  (vehicles.md, The collision count: the model path never writes the 12
  collision points' count). The fix is one number, but AI vehicles might
  then get stuck on things the designers never saw them hit.
  *Recommendation*: let me try it on a branch, then watch c1m2 and c2m1's
  vehicles with you before deciding.
- **B2. Vehicles never slow in quicksand or lava** (vehicles.md, The
  suspension). The data's slowdowns (0.9 and 0.8) are counted by a water
  flag that is only 0 or 1. Counting by the ground's soil, as the mechs
  do, is the clear intent, and the balance effect is small.
  *Recommendation*: change, behind `bugs`.
- **B3. The vehicle net's mode 3 gets no behaviour** (ai.md 17.2). Nothing
  is found that reads it, so the effect is unknown. *Recommendation*:
  keep; nothing to decide until something reads it.
- **B4. The AI backup node's turn** reads a stack value the code never
  sets. It is fixed behind `bugs` with the likely intent (the way back to
  the node), and no shipped net has a backup node. *Recommendation*:
  keep the fix. It changes nothing in the shipped missions.
- **B5. Enemy mechs go silent while you move** (movement.md, Level of
  detail): with your throttle up, no other mech's footsteps, engine or
  eject is heard; standing still, only the first within 250 m. It reads as
  a 1999 sound or CPU budget. *Recommendation*: lift it (an (B)-like
  fix): fights would sound as busy as they look. Hear it in a firefight
  first, since it changes how a battle sounds.

### C. Missions and the campaign

- **C1. Objective rewards that never arrive** (objectives.md, Quirks):
  c1m2's 120 XL engine and c2m2's and c3m3's weapons are written in a form
  the salvage reader skips. *Recommendation*: change. The missions say
  you earn them. It gives the player more gear in Op 1 and Op 2, so it
  would go in CHANGES.md as a play change.
- **C2. c4b m4's boundary is inverted** (fixes.md, Decided): entering its
  "outside" region fails the mission. Possibly meant. *Recommendation*:
  check in the original, by walking out of c4bm4's area both ways. Keep
  unless the original plays differently.
- **C3. A lost field base's last cut moves nothing** (vehicles.md, A
  mobile field base lost). It is reached only when the random picks give
  up on weightless items. *Recommendation*: keep.
- **C4. Quicksand does nothing for mechs** (craters.md section 3). The
  name is there, but no effect was ever wired. *Recommendation*: keep.
  Any effect would be invented.

### D. The lab, the HUD and the front end

- **D1. The lab's heat sink list** (mechlab.md, The unassigned list): the
  check for engine-mounted sinks never holds, which changes only the
  slot counts shown. *Recommendation*: keep until the fitting rule (5b2)
  is traced, then decide both together.
- **D2. Ctrl+I imports into the shown mech's list**, whatever mech the
  file is for. *Recommendation*: keep.
- **D3. Ctrl+X / Ctrl+I use fixed folders** (`export\`, `import\`) with
  no file dialog. *Recommendation*: use SDL3's file dialogs, closer to
  the original's pickers. It's front end only.
- **D4. The HUD's missing images** (playtest #10, hud.md section 16): a
  destroyed part's silhouette and the "other" target arrows were never
  shipped. *Recommendation*: keep. Fill-in art would be your taste, not
  the designers' (a dark red tint, `hud_navarrow_*`).
- **D5. The eject sound is never heard** (damage.md, The death): Die stops
  it in the same call that starts it. *Recommendation*: change, behind
  `bugs`. A sound made and played was meant to be heard.
- **D6. Radio subtitles** (sound.md): 40 radio lines have a SUBTITLE that
  never shows. *Recommendation*: an option, off by default as in the
  exe.
- **D7. The menus' motion step** (fixes.md table, Menu motion): it now
  runs at 30 steps a second at any frame rate. The 1999 menus ran at the
  monitor's refresh. *Recommendation*: 60 a second, closer to how the
  menus looked on the CRTs of the time. Compare the main menu's scroll
  and ripple against the original on your PC.

### Decided already (for reference)

- Allies wait for orders unless an enemy is in reach: kept (2026-10-22).
- Lit mechs' sun shading at full brightness: kept (2026-10-21).
- Lens flares in Direct3D: withdrawn, as the exe draws them (2026-10-25).
- Resolution: wanted, 16:9 either way, up to 1920x1080 if larger sizes are
  a problem (2026-10-20). In progress (resolution.md).

## 3. What I'd do first

1. The c1m1 check run (section 1), with #23's comparison on the original.
2. B1 on a branch, to watch together.
3. A5, B2, C1, D5 as small behind-`bugs` changes, if you agree.

## 4. Questions from the night of 2026-10-29/30

The night's fixes are in playtest.md (W1-W10, S1-S14). These need you:

1. **W8, the field bases waiting at the op point**: the port and the exe's
   logic both let the bases finish with you standing on the point or a slot.
   Which mission was it (c1m1's Baker or c1m3's Charlie)? Were you powered
   down or just standing? On the centre marker or where a base parks? What
   did "step off" mean, and what happened then?
2. **#23/#24, a full weapon that won't fire on a click**: likely chain fire
   (the default: a press fires only the group's current weapon, the one with
   the mark). Which fire mode (`\` cycles chain, single, group), which group
   and weapons, fullscreen or windowed, and does it go away in group fire?
3. **The first start on Windows** (only you can see these): start
   `mw3.exe --write-dir C:\temp\x` (a fresh settings folder) and try the
   install dialog (Browse, a wrong folder, the right one, Cancel) and the
   movie conversion window. Alt+Enter both ways; Alt+Tab with the trigger
   held (nothing should stay held, the sound mutes while away); mouse speed
   windowed against fullscreen (it now matches the original, so it may feel
   faster).
4. **16:9 by default?** Fullscreen opens at the original's 640x480 (bars at
   the sides). Should a first start pick your desktop's size (16:9 on most
   screens) instead? Already selectable in the graphics options.
5. **The HUD at 1280x720 (S12)**: the HUD art scales by whole steps, so at
   720p it is drawn at 1x and looks smaller than at 640x480 or 1080p. With
   the HUD art scale question above (section 3).
6. **The movie importer's FFmpeg**: the package ships a ready-made FFmpeg
   (LGPL v3, ~60 MB, many libraries listed in its notices). A minimal build
   of only what the importer uses (LGPL 2.1, a few MB, no other notices)
   means building FFmpeg from its source on the PC (MSYS2). Worth it?
7. **The fog's start**: the software renderer kept the nearest 500 m clear
   (its range 500..750); the Direct3D game fogs from 32 m. An option for the
   software game's clearer near view?
8. **To compare with the original when you play it**: S3 (t1's hall seen
   from outside), S4 (c1m1's dark sky from the outside view), S5 (the weapon
   list's green text on bright skies), S6 (c1m2's blue glint by a smoke
   column).
9. **The importer applying the 1.2 patch** for a 1.0 player (from the patch
   file they supply): wanted?

Owen's answers (2026-10-30): 1 W8 dropped (long ago, not reproduced);
2 #23/#24 closed (working now); 3 pending his PC tries; 4 16:9 (the
desktop's size) by default; 5 the HUD scaled to look as big as at 640x480
at any size; 6 no FFmpeg in the package unless absolutely necessary (a
32-bit helper on the install's own Ir50_32.dll is being looked at); 7 go
with intent: the worlds' `hwrange` (32..750, the lava's 64..350) was
written for the hardware game on purpose, separate from the software
`range`, so the port keeps it; 9 the importer upgrades 1.0 data from the
player's own 1.2 patch file.

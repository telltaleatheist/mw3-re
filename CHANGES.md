# Changes from the original MechWarrior 3

This rebuild runs the original game's own logic, ported function by function
from `Mech3.exe` 1.2. Wherever it does something different from the 1999
game, the change is listed here in plain words. The technical record (the
original address, the evidence, how each fix is tested) is
[`docs/fixes.md`](docs/fixes.md). In the code each change is marked `FIX`
next to the function it touches.

The rule: keep everything as the original did it, quirks that players know
and plan around included. Change only what stops the game running on today's
machines, what makes it behave differently on a fast computer, and bugs where
it is clear what the designers meant.

## Bug fixes that change play

- **"Sort by name" in the mech lab sorts by name.** The original looked the names up and then sorted by the game's own mech numbers.
- **Lancemates keep their career kill counts.** After a won mission the original copied each lance card's "'Mechs Defeated" number into the campaign's records by card position, writing 0 for a card with no matching pilot, which wiped a lancemate's whole tally when they were not on that card. Now such a card leaves the record as it was.
- **Answering No to "Replace existing game?" keeps your old save.** The original wrote the new mission's world into the saved game's file before asking, so No kept the old entry in the list but its file was already overwritten. The world is now saved only once you answer Yes (or when there was nothing to replace).
- **Lava heats the same at any frame rate.** The original added heat on
  every frame a foot touched lava, so a faster computer cooked you faster
  (1.5 heat a second at 30 fps, over 7 at 144). It is now 1.5 a second
  always, what lava did at 30 fps.
- **Smoke drifts the same at any frame rate.** Above 60 fps the original's
  smoke stopped slowing down and ignored the wind, and the wind's gusts
  changed faster the faster the game ran. Both now go by time, at the 30 fps
  pace.
- **Mech movement plays the same at any frame rate.** Falls, the shoves from
  slopes and ledges, the blend into a walk, jump-jet thrust and the damage
  done to buildings you push into all went by frames; they now play as they
  did at 30 fps.
- **The throttle keys open the throttle at the same speed at any frame
  rate.** Each frame a key was held added a fixed step, so on a fast computer
  the throttle ran up several times faster. It now moves as it did at
  30 fps: from stop to full in about 1.3 seconds.
- **Lasers and TAG recharge at the same speed at any frame rate.** The
  original dropped the fraction of each frame's recharge, so the faster the
  game ran the slower beams came back: TAG took 67 seconds at 30 fps, 83 at
  60, and above about 166 fps (or at 144 fps once the mech ran a little hot)
  it never recharged at all after its first shot. Beams now recharge at
  their 30 fps pace whatever the frame rate.
- **Alan talks in every mission.** His name is misspelt in two missions'
  data (the first Instant Action map of the first world and operation 3's
  mission 6), so the game did not give him his radio voice there. Both
  spellings now get it.
- **Two enemies follow their orders.** On the fourth world's Instant Action
  maps, two enemies had their scripted orders written without the label that
  marks them, so they were ignored. They are read now.
- **Feet use the ground under their middle.** Each foot reads the ground
  at its front, middle and back, but the setting that picks the middle was
  never filled in, so the game used leftover memory: usually the front or
  back reading, sometimes any. The middle is used now when it is nearest.
  This touches how feet sit on slopes and riverbanks and when a foot counts
  as in water.
- **Both feet ease into a walk.** For the first two thirds of a second of
  walking, the right foot snapped straight to its walking angle and the left
  one swung toward the right foot's old angle. Each foot now eases in from
  its own position.
- **Deep water no longer stops a mech dead.** Wading in water nearly as deep
  as the mech is tall slowed it almost to a standstill, then it sped back up
  to 70% once the water was deeper still. It now slows smoothly down to 70%.
- **Wingmen call out low and empty ammo.** Dom, Alan and Epona have lines
  for it that never played because of a mistake in the sound data. They now
  play, each line at most twice a mission.
- **The ERPPC's HUD static lasts as long as its data says** (2 seconds); the
  original read the first number twice and held it half a second.
- **The HUD no longer reads a stray layout** when the screen mode has none of
  its own in hud.zrd: the original could use whatever memory held; the
  rebuild keeps the 640x480 layout, as later set-ups did.
- **Pilot names ignore capitals, as their files do.** Making "BOB" when
  "bob" exists picks bob instead of a second pilot sharing (and
  overwriting) bob's file, and renaming onto another pilot's name is refused.
- **The plain PPC is in the mech lab.** The game defines and names it, but it
  was left out of the mech lab's weapon list. It can now be mounted (Inner
  Sphere, 7 tons, 3 slots, as the data says).
- **The Mad Cat and Supernova show their battle damage.** Their damage
  texture lists were shifted out of line, so the wrong parts got bullet holes
  or none. They now line up.
- **Unpausing resumes the sounds that were playing,** from where they
  stopped, instead of restarting every copy of them, including ones that had
  already finished.
- **Volume fades finish.** A sound fading to a partial volume used to wobble
  around it forever; it now settles.
- **3D sounds pan left and right.** The game asked the sound card for mono
  even though its own settings say stereo, so on 1990s Windows the panning
  was lost. The rebuild reads the setting. Medium sound quality also gets its
  own settings now (the code misspelled their name).
- **Heat sink critical hits are reported.** Losing a heat sink to a critical
  hit now gives the "heat sink" callout and sound, which the original's check
  never allowed.
- **A broken animation condition no longer cancels the others.** When one
  of a mission animation's conditions names an object missing from the
  level, the original dropped all its conditions and fired it at once; now
  only the broken one is ignored. No shipped mission has one; it matters
  for modded missions.
- **The last animation in a mission can be waited for.** A search stopped one
  short of the end of the list. No shipped mission was affected.
- **A smoke fade-in typo is fixed:** smoke set to fade in near the camera
  vanished instead.
- The debriefing's mission time keeps its seconds under 60 past the first hour, as the HUD's timer.
- The HUD's mission timer keeps its seconds under 60 past the first hour (the original showed an hour and 40 seconds as 01:01:3640).
- The credits' "Other Voices" heading sits where the other headings do (a typo in the credits file gave it one number of its two, so it hung indented under the line above).
- TARGET's distance past 1 km reads " (1.23km)" like the " (500m)" below it; the original ran the kilometres straight on after the loadout's name.
- **The message panel slides up at any frame rate.** The original moved it a whole number of pixels a frame, which above 60 fps is none: on a fast computer the in-mission messages never came into view.
- The circles on the briefing map shrink at the same speed at any frame rate (the original shrank them a pixel a frame above 20 fps).
- The briefing's loading bar creeps at the same speed at any frame rate (the original moved it 0.005 a frame).
- The coolant gauge glides at the same speed at any frame rate (the original eased it a tenth of the way each frame).
- **Accepting the game options keeps your mouse sensitivity.** The original reset the vertical mouse sensitivity to 1.3 (or -1.3 with Invert Mouse Y) every time; now the invert box only flips its direction.
- **TAG is the same at every frame rate, and its airstrike follows the target.** The one-second hold is a true second, and the beam lasts what it did at 30 fps, so you always have about 67 ms to slip (the original gave two frames at 30 fps and one at 60 or 144). The four missiles now home on the mech you marked instead of the spot it stood on seven seconds earlier, so a moving mech is hit too. TAG still fires only from a full charge.
- **A mech put back on the ground looks 10 m above where it stands**, as everything else in the game does. The original looked y + 10 above a mech at height y, so below sea level it found nothing and dropped to the lowest surface (under a bridge), and high up it could land on a roof overhead. No mission's starting place changes: all 195 placements in the 24 campaign missions and all 768 records in every campaign, Instant Action and multiplayer file land exactly where they did. What changes is a mech re-placed where it stands (swapping mechs mid-mission or in multiplayer), which now stays put.
- **Cut ground keeps its texture lined up.** When a crater cut a ground polygon and a corner of a cut piece lay on a straight edge, the original dropped that corner from the piece's points but not from its texture coordinates and colours, so every corner after it took the next one's: the texture and shading on that piece slid by one corner (about 1 in 200 of the polygons a crater adds). The extra corner's texture coordinates and colours now go with it.
- **Craters next to an earlier crater are judged on the earlier one.** The original refuses a crater that would come within 5 m of an earlier crater in the same patch of ground, but it checked against the earlier crater's record after freeing it, so the answer depended on what the memory held by then. The rebuild keeps the earlier craters' outlines for the test.

## Crashes and broken data that no longer break the game

None of these change normal play; they stop a crash or a hang when something
unusual happens.

- A picture the menus or the HUD let go of twice is no longer freed twice (which could crash); the rebuild reports it instead.
- Keypad 0 (the zoom lock) no longer crashes when the mech you are looking at has no HUD; it does nothing then.
- A script that cycles the textures of a model with no material, or turns a scroll off, no longer crashes (neither happens with the shipped scripts).
- An edited debriefing dialog missing its buttons' help, its title's, its lance cards' or its rank's lines no longer crashes.
- An edited mech lab dialog missing the supply list's slot titles (ALIST_TITLES) no longer crashes.
- A briefing whose voice block has no map markers plays the voice instead of crashing.
- Salvaging a mech with a weapon that has no definition skips that weapon instead of crashing.
- A hand-edited objectives file with a number or a list where a name belongs, or an objective list without its `list`, no longer crashes the mission load; the odd entry is skipped.
- An Instant Action game in an unknown mode no longer crashes when its "destroy the enemies" objective is built; the objective is left out.
- An objective description with a `%` in its second or later lines is shown as written instead of being read as a format.
- A line of the briefing's debug log with a `%` in it is printed as it is (the original read it as a printf format).
- A soils file missing one of the game's ground types no longer corrupts the
  word in memory before the game's soil table; the type is just left out.
- A shot passing through more than 32 objects no longer writes past the end
  of the list of what it hit; the original had room for 32.
- Asking for the ground under a point where more than 32 objects' surfaces
  overlap no longer writes past the end of the answer; the original had
  room for 32 and stopped only some of the time. No shipped mission comes
  close.

- A cockpit camera given a distance in a mech file no longer reads leftover
  memory for its offset (no shipped mech does it).
- On a computer without MMX, the software renderer's screen flashes no longer
  write past the end of the screen's memory on its last line.
- A mech file with a bad version no longer crashes the game when it unloads.
- A mech type whose file fails to load no longer crashes the next mission's
  load (the original kept the broken type and read through it there).
- The option screens no longer read past their background picture on a
  screen taller than 960 lines or wider than 1280, the likely cause of the
  crash when opening Options from the pause menu at 1280x1024 or 1600x1200.
- A video card whose name has a `%` in it, or is very long, no longer
  garbles or crashes the graphics options; it is shown as written.
- Three checks the original menus skipped no longer crash when missed (a
  credits line over 255 characters, a dialog without a mouse pointer, the
  credits opened without the main menu); nothing shipped reaches them.
- A scripted sequence with no frames in it no longer reads past its data (no
  shipped mission makes one).
- The game starts without a mouse (the original crashed), and pressing a
  newer key such as Power, Sleep or a web key while holding Ctrl, Alt and
  Shift no longer corrupts the game's input memory.
- A damaged or hand-edited saved loadout (`configs\<mech>.mw3`) can no longer
  hang the load or crash it.
- A controls profile with a `%` in its file name shows its name in the load
  and save list instead of garbage (or a crash), and an empty list's
  clickable area is empty instead of whatever was left in memory.
- A damaged archive or script file is stopped at its end instead of making
  the game read or allocate gigabytes.
- Rare random rolls in equipment critical hits no longer crash.
- Clearing every shot in flight no longer writes past a firing laser's
  memory (the original treated beams as missiles there).
- Very large polygons, long script lines and oversized weapon sound or
  damage lists no longer write past their buffers.
- A null texture, a missing weapon group and a few malformed script commands
  are checked instead of crashing.
- A mech whose right foot had fewer toes than its left would have animated
  one toe too many on that foot, writing past its data; it now stops at the
  last toe.
- A mech animation sampled at the very start of its cycle, before any other
  moment of it, no longer reads leftover data past its frames (it could show
  one wrong pose; for the longest motions, garbage).
- Undefined values the original left in a few places (a matrix row, a new
  sound copy's position, an air burst's surface direction) are zeroed.
- A damaged saved controls-file name (no end marker) no longer makes the game
  read past it.
- A hand-edited controls profile (`keys\<name>.zrd`) with an axis of an
  unknown name, or a number or an empty entry where a command belongs, no
  longer crashes the game; the bad line is skipped.
- The controls screen no longer overruns its file name with a very long
  current-profile setting, and an empty command group (none ships) shows an
  empty list instead of crashing.
- A text data file with a word of 256 letters or more, a quote that is never
  closed, or an `#ifdef` without its `#endif` no longer crashes or hangs the
  game.
- A vehicle with a name of 20 letters or more (or with a `%` in it) in a
  mission's vehicle file keeps its name; the original cut it and, from 28
  letters, overwrote the vehicle's settings. A made-up vehicle whose name
  ends in `_mech...` without an AI path no longer gets the file's own record
  as its driver. No shipped mission has either.
- A mech model missing a node its definition names (a firepoint or a foot),
  or a definition without camos, no longer crashes the game as the mech is
  built at launch, nor does a launch with the view on another vehicle (the
  game never does that). No shipped mech has either.
- When the only delayed trigger waiting fires and starts more delayed
  triggers, they are kept; the original dropped them, which can silence
  three radio lines in operation 3 mission 2.
- A hand-edited mission trigger file (`valves.zrd`) with a number where a
  list or a name belongs, or a trigger that teleports or switches the scan
  of a pilot that is not there, no longer crashes the game.
- An AI vehicle that fails to be built no longer crashes the mission start
  as its driver is thrown away, and a made-up radio voice name of 15
  letters or more in a mission's vehicle file no longer overruns memory.
  The shipped missions have neither.
- A big C3 network with mission targets in range no longer writes past
  the memory set aside for its shared contacts, and a lancemate without a
  pilot name in a hand-made mission file no longer crashes the start.
- A hand-made AI region file without a name, or an AI path linking past its
  last point or to points without a position, no longer crashes the
  mission load or the AI's route finding. One shipped path (c2 mission 3,
  path 13) links to a point that does not exist; the original read
  leftover memory there, the rebuild takes no link.

- A damaged pilot file no longer hangs the game while it is read (a count
  read past the end of the file made up to 4 billion empty campaigns or
  lancemates).
- A file in `pilots\` with a name of 64 characters or more no longer
  crashes the pilot screen; it is not listed.
- DELETE on the pilot screen before any pilot exists no longer crashes the
  game; it does nothing.
- Copying a saved loadout that holds an engine item (only a hand-made file
  can) no longer crashes the game after a mission; the item is left out. A
  damaged loadout with an item in a part that does not exist no longer
  corrupts the loadout when its free slots are counted.
- Deleting the pilot you had chosen before no longer leaves the game holding
  the deleted pilot (it could be taken for the player's in a lobby game).
- A weapon group with no weapons in it (or none selected) reads as having no ammo; the original's answer depended on where its weapons list sat in memory.
- An AI pilot with exactly 13 (or 39, ...) orders queued no longer reads past its order list (a possible crash) when the game checks whether it is on a path it is not on.
- A target behind the zoom window's camera no longer picks TARGET's arrow from stray memory bits (the original read an unset value there).
- A mech with a targeting computer and no weapon group selected no longer crashes the game when it has a target on screen (the lead circle needs a group).
- An AI vehicle record with a broken attack range no longer crashes the game when its pilot has no target.
- An AI path with a step that leads nowhere no longer crashes the game when a pilot weighs the ways on from a waypoint; that way counts as closed. No shipped path has one.
- An AI "backup" waypoint (none in the shipped game) turns the pilot toward the way it came, before backing in and again at the spot, instead of toward whatever direction happened to be left in memory.
- An AI "deploy" waypoint without its switches named no longer crashes the game; the missing switches read as off.
- An AI vehicle without weapons no longer crashes the game when it is hit (its pilot's firing stands down); neither does an AI weapon without a definition when it fires.
- An AI pilot whose target disappears while it is lining up a shot no longer crashes the game; it gives the shot up.
- An AI mech with no weapon group selected no longer crashes the game when it turns its torso toward a target.
- An AI mech whose chosen weapon belongs to no weapon group no longer crashes the game when it picks what to fire.
- An AI pilot given an order before it has a vehicle no longer crashes the game when the order first runs.
- An AI pilot that finds no step of its old route to walk back to after a fight stops there instead of crashing the game.
- A world's `declient.zrd` with a one-frame quicksand texture, or a `QUICK_SAND` block without `DEFAULT_TEXTURE`, no longer crashes the mission load; the one frame is shown, or the quicksand is left untextured. No shipped file has either.
- Cutting a crater into large stretches of ground no longer risks reading freed memory or writing past the clipper's tables.
- A crater whose outline passes exactly through a corner of the ground's polygons no longer freezes the game; that piece of ground is left uncut.
- A crater whose outline runs exactly along an edge of the ground's polygons and touches one at a corner no longer freezes or crashes the game; that piece of ground is left uncut.
- A crater dug inside one large ground polygon can no longer overrun the memory used to cut the ground around it into triangles.
- A crater without its texture flags no longer crashes the game (the original wrote the crater floor's texture position through a null pointer; every shipped crater has the flags).
- Clearing a mission's ejection seats can no longer hang if one of them will not come off what holds it (none does in the shipped game).
- A modded mission script that sets a surface-mark type above 2 no longer overwrites other settings (no shipped script uses surface marks).


## Copy protection removed

- No CD check at startup; the importer copies the movies from the disc.
- No self-check of the game's files, which made a modified game quit after
  the intro.
- Hand-edited saved loadouts load if they follow the mech lab's rules
  (weight, slots, jump jets, armor). They still get the original's
  checksum, so the original game can read them. The same goes for a
  loadout imported in the mech lab (Ctrl+I).

Details: [`docs/protection.md`](docs/protection.md).

## Running on modern machines

These change how the game talks to the computer, not how it plays.

- The pause menu's background is the battlefield as you left it. The original copied the screen; the rebuild draws the 3D world on the graphics card, so it reads the last frame back from it.

- The game's question boxes ("Ok to delete pilot?", "Replace existing game?" and the rest) keep reading the mouse and keyboard while they wait: the original's box read DirectInput directly, the rebuild's also handles the window's events while it waits.

- The mech lab's hidden loadout export and import keys (Ctrl+X, Ctrl+I) opened Windows file dialogs. Ctrl+X now writes the loadout to the `export` folder (named as the dialog suggested, `<mech>_<loadout>.m3c`), and Ctrl+I reads the next `.m3c` file from the `import` folder, then marks it done.

- The main menu's water ripple around a button builds its ring in a buffer
  as large as the screen; the original's held 1024x768, enough for its
  640x480 menus but not for menus at today's sizes.

- More screen sizes: besides the original 640x480 to 1600x1200 (all six
  now offered on a 3D card, not only the first three), 1280x720, 1600x900,
  1920x1080, 2560x1440, 3840x2160 and a size of your own (ScreenWidth and
  ScreenHeight in the settings). On a wide screen the view widens to the
  sides instead of stretching, keeping the original's look; 640x480 is
  exactly as before. The window, the HUD and the menus at those sizes come
  in later steps (docs/resolution.md).

- The craters a host sends to a player joining a multiplayer game name their ground and texture by material number instead of by the host's memory addresses, which meant nothing on the joining machine (and do not fit a 64-bit program).

- A developer's leftover debug message ("haha", whenever an object named
  blackhawk_01 has its bounds rebuilt) is not printed. The 1999 game had no
  console to show it in; the rebuild would print it to the terminal.
- The controls' load and save list shows the profiles in your save folder
  (and any in the game folders) instead of asking Windows for the game
  folder's `keys` files, in alphabetical order as Windows lists an NTFS
  folder.

- The main menu's crawling background, sliding MECH3 logo and the ripples
  around its buttons move at the same speed at any frame rate (the 30 fps
  pace). The original moved them a step every frame, so on a fast computer
  the menu raced and the ripples vanished almost at once.

- Keyboard, mouse and joystick through SDL3 instead of DirectInput, read the
  same way: the mouse is captured while the window has the focus (as
  DirectInput's exclusive mouse was) and moves at the same speed whatever the
  window's size; a joystick gets the game's own ranges and dead zones.
- Graphics through SDL3 (Metal, Direct3D 12 or Vulkan) instead of DirectX 6;
  the game's own drawing code is unchanged.
- The fades (a mission's start, leaving it) take their still picture from
  the program's own copy of the screen instead of from DirectDraw; the colour
  over it is drawn as the original drew it. They look and last the same.
- The HUD and the other 2D drawing in a mission are laid over the 3D picture
  instead of being drawn into a locked DirectDraw back buffer; they look the
  same.
- The fade at a mission's start reads the picture back from the graphics card
  to fade in from it, as the original's DirectDraw grab did.
- Sound through SDL3 instead of DirectSound, with the same volumes and
  panning.
- Settings are saved in `settings.reg` in the user's folder instead of the
  Windows registry, with the same names and values (regedit's format: a
  Windows export of the game's keys can be dropped in). They are read even
  without an installer's key, which the original needed.
- The detail settings are picked for a modern machine described by the
  program (a fast processor with MMX, a 3D card, the real memory) instead of
  the original's processor timing and DirectDraw queries.
- A mission loads before its briefing appears, instead of in the
  background while the briefing plays (the graphics card wants the world's
  textures loaded from the game's own thread).
- Music from `music/trackNN.ogg` (or .flac, .wav, .mp3) instead of the CD;
  a ripped disc maps track for track.
- The frame clock counts from game start, not from when the computer booted,
  so the game no longer stutters on a machine that has been on for days.
- The software renderer's stack-pointer trick (drawing pixels with PUSH) is
  replaced by ordinary writes, pixel for pixel the same.
- Files are found through search folders, so replacement files can sit in
  front of the original install, or stand in for it.
- Controls profiles (`keys\<name>.zrd`) are saved in the user's folder
  instead of the game folder, in the same text format (a profile from the
  original game can be copied in).
- The game is its own program, `mw3`, in an ordinary window (SDL3) instead
  of a Windows program; it starts the way the original did, step for step.
  Saves and settings go to the user's application-data folder, not into the
  game folder.
- The keyboard, mouse and joystick are read through SDL3 instead of
  DirectInput, with every key reported under the same code the original's
  key tables use.
- The game's texts come from its own `Mech3Msg.dll`, read as data, and the
  menus' writing is drawn with the game's own fonts (Arial, Impact, Lucida
  Console, shipped with it) instead of Windows' text drawing.
- Message boxes are the computer's own instead of Windows'.
- The game's random numbers are generated exactly as on the original's
  Windows, so everything random behaves the same.
- Some rendering depends on the processor's math precision. In the emulator,
  at the 24-bit precision DirectX 6 sets, mech shadows, coloured lights on
  textures and the moving cloud shadows come out blank or black. On Windows
  11 the original shows mech shadows, so the rebuild draws what that runtime
  shows: mech shadows, coloured explosion and muzzle-flash light on textured
  surfaces, the ground lit by weapon fire, and moving cloud shadows. (On a
  1999 Windows 98 machine these may never have shown; that is unconfirmed.)

- Pilot files (and every saved file) are written in the rebuild's own
  folder; pilots from an imported install are copied there the first time
  they are opened, so their progress continues. Accented pilot names
  (Windows' own character set, as the original typed them) are stored as
  ordinary file names on macOS.
- In Direct3D the HUD's see-through images (compass, reticle) blend with the 3D picture behind them, as on the original; the rebuild's overlay layer had shown them with magenta fringes.


## Kept as the original, on purpose

Known quirks the rebuild deliberately keeps:

- Wading in any depth of water gives the full x8 cooling, and only the left
  foot counts.
- Elementals have one point of armor everywhere and die to a single hit.
- Missiles that miss vanish at the end of their range without exploding.
- Radio chatter plays the newest line first.
- Every mech's engine tops out at 400.

# Changes from the original MechWarrior 3

This rebuild runs the original game's own logic, ported function by function
from `Mech3.exe` 1.2. Wherever it does something different from the 1999
game, the change is listed here in plain words. The technical record (the
original address, the evidence, how each fix is tested) is
[`docs/fixes.md`](docs/fixes.md). In the code each change is marked `FIX`
next to the function it touches.

**To play the game as it shipped:** the options' UPGRADES page has five
switches, all on to start: WATER MOVEMENT, MFB REGROUP, IMPROVED AI, ARTEMIS
TRACKING and SMALL FIXES (the page lists what the small fixes are). Turn one
off and that part plays as in 1999; `mw3 --original` turns all five off for
one run. Crash fixes, the fixes that let the game run today, and the
restored sounds, pictures, voice lines and text stay on (Owen's rule,
2026-11-10). In a network game everyone plays on the host's switches. Which
change sits under which switch: [`docs/upgrades-sort.md`](docs/upgrades-sort.md).
A 1.0 `mw3.cfg`'s `bugfixes=` line becomes the five switches the first time.

The rule: keep everything as the original did it, quirks that players know
and plan around included. Change only what stops the game running on today's
machines, what makes it behave differently on a fast computer, and bugs where
it is clear what the designers meant.

## Bug fixes that change play

- **The Sunder's Alternate B loadout carries all its LRM 20 missiles.** A misplaced word in its file left 240 of the 360 unloaded. Under SMALL FIXES.
- **The Strider has its recoil slide.** Its file gave the number as text ("1.5.0"), so it had none; it now has the 1.5 of the mechs beside it. Under SMALL FIXES.
- **The Firefly's fifth camo is named SLATE.** It was labelled WASP like the fourth. Always on.
- **All of mission 1's ocean scrolls.** One water surface was left still by a misspelt word in the world's script. Always on.
- **The Small Pulse Laser has its muzzle flash.** Its weapon data named a flash no mission has, so it fired with none; it now shows the Small Laser's blue flash, as every other laser uses its size's colour.
- **Typing in the cheat box or the chat no longer moves your view.** A key you type into a box still counted for the game when you let it go after pressing Enter, as you do when typing fast; L is the hold-to-look key, so "operation cwal" left the mouse turning the torso instead of moving the reticle. Keys typed into a box are the box's now.
- **Artemis works in a side torso.** The rebuild looked for Artemis on the torso as one piece, so a launcher and its Artemis in the same side torso (some stock mechs have them there) never got it. Now, as in the original, a locked shot from that launcher tracks at double rate and hugs the ground.

- **Difficulty sets the damage your side takes again.** The rebuild read the setting but never passed that part on, so every level played like High: you took full damage even on Medium. Now, as in the original, you and your allies take 40% on Low, 70% on Medium and 100% on High (not in multiplayer). This is not behind the bug-fix switch, because it restores the original. On Low the screen's heat haze is back to the original's rule too, from the same setting.

- **A joystick's throttle lever reaches reverse.** The original's lever ran from stop to full ahead; reverse took the Toggle throttle direction key (Backspace). Now the lever covers the whole range, full reverse at the bottom to full ahead at the top, with stop a little over a third of the way up (reverse is slower, so each step of the lever is the same change of speed both ways) and a small band around it that holds stop. A lever left at the bottom does not send the mech backwards at the start of a mission. Toggle throttle direction (Backspace, stick button 10) flips the keys' throttle but not the lever, so nothing can leave the lever backwards for the rest of a mission; a lever that runs backwards on your stick is cured by unticking Invert in Assign Axis. `throttle_reverse=0` in `mw3.cfg` gives the original's lever, which Backspace flips. Gamepad triggers are unchanged. The controls screen's Assign Axis also offers the stick's sliders, where many throttle levers are.
- **A destroyed weapon does not stay selected.** When your selected weapon is destroyed, the game moves the selection to the next weapon on the list, but it did not check that one was still working: if it had been destroyed already, the selection stayed on a dead weapon. It now goes on to the next weapon that still works.
- **A laser's beam leaves from the arm after a quick click.** A short laser keeps firing for a moment after you let go of the button, and the original left it at the point where the arm was when you let go, so on a walking mech the beam hung back a metre or more behind the arm (and fired from there). It now follows the arm until it ends, as it does while the button is held.
- **Every nav point you pass on your objectives' list is ticked off.** The game marks a nav point an objective watches as reached (and says its line) when you come within 25 m, but once its check passed the currently selected nav point it skipped the rest of the list, so those were only ticked off once selected. Now all of them are checked.
- **Turrets do not slow down mission after mission.** Each turret looks for a target, aims and fires on only one frame in so many, and the original counted that over every turret the game had made since it started: the more missions you had played in one sitting, the less often each mission's turrets acted. The count now starts again with each mission's turrets.
- **Falling over is felt on a force feedback stick.** Patch 1.2 added a force feedback effect for a fall, but the game asked for it by another name than the effect file gives it, so it never played. The rebuild finds it.
- **The mobile field bases always come when called.** Called back to the point where they last repaired you, the bases were already standing in place and never "arrived", so the dock waited for the rest of the mission and refused every new call; a base that got stuck on its way did the same. They now count as arrived, and a base that cannot move on for 30 seconds is left behind so the repair goes ahead.
- **Artemis IV FCS makes missiles track twice as hard.** Locking with an Artemis fire control system was meant to double your missiles' turn rate, but the game reset the setting before any missile read it. Each missile now keeps it, so FCS-guided missiles turn (and climb over obstacles) twice as fast.
- **A turret's charging effect goes out when it dies.** The original's test for putting it out was the wrong way round (no shipped turret shows one, so nothing visible changes).
- **"Sort by name" in the mech lab sorts by name.** The original looked the names up and then sorted by the game's own mech numbers.
- **Lancemates keep their career kill counts.** After a won mission the original copied each lance card's "'Mechs Defeated" number into the campaign's records by card position, writing 0 for a card with no matching pilot, which wiped a lancemate's whole tally when they were not on that card. Now such a card leaves the record as it was.
- **Answering No to "Replace existing game?" keeps your old save.** The original wrote the new mission's world into the saved game's file before asking, so No kept the old entry in the list but its file was already overwritten. The world is now saved only once you answer Yes (or when there was nothing to replace).
- **A vehicle driving over burning ground no longer crashes the game.** When a tank's collision check touched a floor marked as fire, the original wrote into memory it should not have and could crash.
- **A vehicle gun's pitch limit is worked out with the right length.** The original's function for putting an aim on a vehicle gun's top or bottom angle used the wrong length (a slip of one letter), so the result missed the limit. Nothing in the game uses that result (the AI aims with the unclipped direction), so this changes nothing you can see; it matters only if a mod or later code relies on it.
- **A vehicle with chassis pitch settings and no roll settings keeps its pitch.** The original zeroed the pitch values when the roll values were missing (a copy slip). No vehicle in the game has either setting, so this only matters for modded vehicles.
- **Lava heats the same at any frame rate.** The original added heat on
  every frame a foot touched lava, so a faster computer cooked you faster
  (1.5 heat a second at 30 fps, over 7 at 144). It is now 1.5 a second
  always, what lava did at 30 fps.
- **The HUD's damage static builds at the same speed at any frame rate.**
  With the torso or head badly hurt the panels break up with static; the
  original added to it every frame, so a faster computer showed more.
- **Rain and snow fall the same at any frame rate.** The original moved
  them by a fixed step a frame for the fall and the wind but by the real
  frame's move for your mech, so above 30 fps the rain looked steeper while
  you walked and the snow fell and blew faster; now both look as they did
  at 30 fps.
- **Smoke drifts the same at any frame rate.** Above 60 fps the original's
  smoke stopped slowing down and ignored the wind, and the wind's gusts
  changed faster the faster the game ran. Both now go by time, at the 30 fps
  pace.
- **Turrets and mech arms turn at any frame rate.** The original eased
  them toward where they aim, and eased the arms' recoil, by a table that
  rounds a frame's step down to 1/51: above about 150 fps the turrets did
  not turn at all, and the missile platforms that start facing straight up
  (Op 1 Mission 2) were drawn flat, invisible but alive, and the objective
  looked impossible. These now ease at the original's 30 fps pace at any
  frame rate, and exactly as before at 30 fps. A turret aimed straight up
  or down also keeps its shape. Vehicles keep the original's damping.
- **Mech movement plays the same at any frame rate.** Falls, the shoves from
  slopes and ledges, the blend into a walk and jump-jet thrust all went by
  frames; they now play as they did at 30 fps.
- **The throttle keys open the throttle at the same speed at any frame
  rate.** Each frame a key was held added a fixed step, so on a fast computer
  the throttle ran up several times faster. It now moves as it did at
  30 fps: from stop to full in about 1.3 seconds (with the lance command
  panel open too, which has its own copy of the keys).
  The mech's speed then follows at the same pace too: above about 83 fps an
  earlier version of this fix let it skip its slow build-up and pull away
  nearly 2 seconds sooner.
- **Lasers and TAG recharge at the same speed at any frame rate.** The
  original dropped the fraction of each frame's recharge, so the faster the
  game ran the slower beams came back: TAG took 67 seconds at 30 fps, 83 at
  60, and above about 166 fps (or at 144 fps once the mech ran a little hot)
  it never recharged at all after its first shot. Beams now recharge at
  their 30 fps pace whatever the frame rate.
- **Turrets act at the same pace at any frame rate.** A turret looks for a
  target, turns and fires only on its turn, one frame in as many as there
  are turrets, so on a faster computer turrets reacted and shot more often.
  Turns now come by time, at the 30 fps pace: with 14 turrets each acts
  about twice a second.
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
  back reading, sometimes any. The middle is now used when the foot is close
  to it, by the value the original team's own Pirate's Moon sets.
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
- From high up, no dark band between the far ground and the sky: what nothing draws takes the fog's colour (bug fixes on).
- A light carried by something that moves now shines from where it is, not from the world's origin.

## Crashes and broken data that no longer break the game

- A rare crash when a crater cut the ground in an awkward place is gone: the cut is skipped on that piece of ground.
- **More than 64 lights at once no longer corrupt the shadows' memory.** Every explosion and weapon impact brings its own light. With more than 64 lights on at once, the original wrote the brightness of the 65th and later ones, wherever they reached a model, past the end of its table and over the shadows' data. Heavily modded weapons with lots of firing could get there. Those lights now keep their values apart and light the model as the others do.
- **Ctrl+I in the mech lab loads a loadout again.** In the rebuild so far it read
  the file from `import` and added nothing, and still renamed the file
  `.m3c.done`, so the loadout seemed to vanish. The file itself is intact: take
  `.done` off its name and press Ctrl+I again.

- **Finishing a mission while commanding your lance from the pocket view no
  longer crashes.** The original threw the commander's view away at the
  mission's end while your mech was still using it, then had the mech let
  go of it again; the rebuild gives the mech its own controls back first.

None of these change normal play; they stop a crash or a hang when something
unusual happens.

- A movie script line with too few values reads the missing ones as 0 instead of reading past the line (no shipped script has one).
- c2 mission 3's two Firefly objectives are real: the data named the wrong Fireflies, so the original ticked them off before the mission began.
- The HUD no longer crashes drawing static on the TARGET display in a mode with nothing to show.
- The rain's and snow's very first frame no longer draws its streaks from unset sizes (it used the screen's size from the second frame on anyway).
- A picture the menus or the HUD let go of twice is no longer freed twice (which could crash); the rebuild reports it instead.
- Keypad 0 (the zoom lock) no longer crashes when the mech you are looking at has no HUD; it does nothing then.
- A script that cycles the textures of a model with no material, or turns a scroll off, no longer crashes (neither happens with the shipped scripts).
- A mission script command that changes an object's model (its colour, priority, facade, back faces, morph or world textures) after the object was not found now does nothing. The original went on with the last object's model, which after a second mission could be one already freed.
- An edited debriefing dialog missing its buttons' help, its title's, its lance cards' or its rank's lines no longer crashes.
- An edited mech lab dialog missing the supply list's slot titles (ALIST_TITLES) no longer crashes.
- An edited mech lab dialog missing the armour panel's texts (ARMOR_TITLES, ARMOR_REMOVE) no longer crashes.
- A briefing whose voice block has no map markers plays the voice instead of crashing.
- Salvaging a mech with a weapon that has no definition skips that weapon instead of crashing.
- Losing a field base while carrying 64 or more spare mechs no longer overruns the game's list of them (which could crash).
- Cycling the field bases' dock points (MFB, then N or Shift+N) onto the list's empty first entry no longer reads through nothing on the radar and the edge arrow (which could crash); no marker is shown there.
- A nav point file linking to a node that is not there no longer reads or writes past the game's lists (no shipped file does).
- The waypoint arrows' images are all released at the end of a mission (two of the four sides were kept, a small leak each mission).
- The waypoint arrow no longer keeps an image the last mission let go: the next mission's first arrow at the screen's right or bottom edge read that freed image's size, which on today's systems can crash the game.
- In Instant Action, a wreck taken away at 180 seconds no longer stays the "last attacker" of the mechs it hit: one of them dying later (a fall, its own ammo) credited the kill to the removed wreck, which could crash.
- A hand-edited turret without parts, or without a gun, no longer crashes in play; it shuts down, or does not aim.
- A mission animation that sends the unused "move the player through viaNode" message, in a world without that node, no longer throws the player's mech to a random place (no shipped animation or world does this).
- A hand-made mission animation whose object is missing no longer crashes the mission load when it flips a switch on a part above it (no shipped animation does this).
- A hand-edited turret file whose turret names a weapon that does not exist, without a detection range, no longer crashes the mission load; the turret sees 200 m. A turret file entry shorter than the game reads is taken as missing (no shipped file has either).
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
- A damaged saved game (`pilots\games\*.mw3`) no longer crashes the
  mission it is loaded into; the damaged parts are passed over.
- Rare random rolls in equipment critical hits no longer crash.
- Clearing every shot in flight no longer writes past a firing laser's
  memory (the original treated beams as missiles there).
- Ending a mission with a laser still firing no longer crashes the game when you
  press Replay or leave (our port had missed the fix above in one place).
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
- Leaving the mission area now fails operation 4b's missions 5 and 6, as
  their objectives say: their boundary triggers named a region and a
  switch that did not exist (behind the bug-fix switch).
- Ending a mission while the lance command mode is open no longer frees
  the mode twice (a possible crash in the original).
- The lance command mode no longer reads past a missing HUD when the view
  is not on your own mech (a possible crash in the original).
- Starting a mission after another no longer runs the previous mission's
  player triggers against switches that were already thrown away.
- A mission trigger that updates itself, directly or round a loop of
  triggers, no longer crashes the game: each trigger's actions run once
  for each change. No shipped mission has such a loop.
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
- A vehicle definition without its `platform` or `collision` points no longer crashes the mission start.
- A vehicle whose weapon list (`vehicle.zrd`) names a weapon the game does not know no longer crashes the mission start; that weapon slot stays empty.
- A modded mission with more than three mobile field bases no longer overwrites the first base's valve; the extra bases are put out of play as the original does, without the stray write. No shipped mission has more than three.
- A mobile field base hit while there is no player's mech no longer crashes the game (it cannot happen in the shipped game).
- A modded mission with fewer than three mobile field bases no longer crashes when a repair starts.
- An AI "deploy", "mcbwait" or "valve_assign" waypoint without its switches named no longer crashes the game; the missing switches read as off and are not set.
- An AI jump waypoint that does not lead to a path node no longer crashes the game; the pilot jumps without turning.
- An AI vehicle without weapons no longer crashes the game when it is hit (its pilot's firing stands down); neither does an AI weapon without a definition when it fires.
- An AI pilot whose target disappears while it is lining up a shot no longer crashes the game; it gives the shot up.
- An AI mech with no weapon group selected no longer crashes the game when it turns its torso toward a target.
- An AI mech whose chosen weapon belongs to no weapon group no longer crashes the game when it picks what to fire.
- An AI pilot given an order before it has a vehicle no longer crashes the game when the order first runs.
- An AI pilot that finds no step of its old route to walk back to after a fight stops there instead of crashing the game.
- An AI pilot that starts keeping station during a fight no longer keeps a stale home point after the fight ends (the original could measure its leash from freed memory).
- A lancemate AI pilot that would keep station on a vehicle with no driver no longer crashes the game.
- A campaign lancemate slot left empty no longer risks crashing the launch when its mission entry cannot be read or there is no player mech to take over its duties (no shipped mission does either), and a lancemate's name with a `%` in it is no longer read as a format.
- Instant Action's line-up no longer risks a crash on a mission entry it cannot read, a wave number below 1 or a very long entry name (none happens with the shipped files).
- Two training objectives (looking at a target, choosing a weapon group) no longer read through a missing mech or weapon list; they simply wait (never the case in the shipped lessons).
- The Instant Action screen, opened after all 20 progression waves are beaten, no longer reads past its list of waves for a moment (it read text as mechs before going back to Wave 1; nothing showed).
- A vehicle hit by a laser and still alive when the mission ends no longer leaves the laser's light behind (the game could not free it).
- A line-of-sight or shot query toward something at an undefined place no longer freezes the game.
- An effect started on an object, after the same effect was used in mid-air, no longer takes that object's model down with it when the mission ends.
- An AI pilot with a negative personal-space setting no longer hangs the game when another mech comes close (no shipped mission has one).
- A world's `declient.zrd` with a one-frame quicksand texture, or a `QUICK_SAND` block without `DEFAULT_TEXTURE`, no longer crashes the mission load; the one frame is shown, or the quicksand is left untextured. No shipped file has either.
- Cutting a crater into large stretches of ground no longer risks reading freed memory or writing past the clipper's tables.
- A crater whose outline passes exactly through a corner of the ground's polygons no longer freezes the game; that piece of ground is left uncut.
- A crater whose outline runs exactly along an edge of the ground's polygons and touches one at a corner no longer freezes or crashes the game; that piece of ground is left uncut.
- A crater dug inside one large ground polygon can no longer overrun the memory used to cut the ground around it into triangles.
- A crater without its texture flags no longer crashes the game (the original wrote the crater floor's texture position through a null pointer; every shipped crater has the flags).
- Clearing a mission's ejection seats can no longer hang if one of them will not come off what holds it (none does in the shipped game).
- A modded mission script that sets a surface-mark type above 2 no longer overwrites other settings (no shipped script uses surface marks).
- A mission without an AI boundary (a multiplayer game, or a map without one) played after a campaign mission no longer leaves the AI using the previous mission's freed boundary.
- A modded map whose boundary names a mission past its operation's last no longer writes past the game's list of boundaries; that one is ignored.
- An AI pilot at a broken (NaN) position no longer reads leftover memory when the boundary looks for the way out.
- An objective's text can no longer garble the objectives box: the original used each objective's text as a formatting pattern, so a '%' in it would have printed garbage or crashed. No text in the game has one, so nothing you see changes.
- A file name the game reads from its data, a mod or a script (a texture, a sound, a movie, a script's `source` or file delete, a world folder) can no longer reach outside the game's folders: a name with `..`, a drive or a full path opens, writes and deletes nothing, and the log says so. The original opened whatever it was given. Nothing shipped uses such a name. Multiplayer sends worlds as numbers, which only ever name your own worlds.

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

- On Windows the game can be installed, and keep its data, in a folder whose name has any letters, and it works for a Windows user name with accents (Windows 10 version 1903 or later; before that, it says so and asks for plain English letters). A pilot or file name with accents from an earlier version of the rebuild is put right at the next start.
- The graphics options no longer offer the software renderer, which the rebuild does not have: the video device list shows your graphics card only, and a setting saved by the original or an older rebuild that chose software starts on the card.
- **Gamepads work as they did on Windows.** A modern pad's sticks, triggers
  and buttons arrive in the order the game knew from Windows: the left stick
  steers and walks, the triggers are the throttle, the right stick looks.
  Read the raw way, a pad's resting triggers held the torso twisted.
- **Flight sticks' axes land where the game expects them.** A stick with a
  twist and a throttle lever (a Thrustmaster T.16000M, say) gave its twist
  as the throttle and turned no torso, so the reticle could not aim. Each
  axis is put where Windows put it, read from the stick itself; a player
  can set the map by hand with `joystick_axes=` in `mw3.cfg` (the log
  shows the line to start from).
- **The joystick is chosen sensibly.** The original took the first device
  that called itself a joystick; headsets and RGB keyboards often do now.
  The game takes a gamepad or a real stick first, or the one named by
  `joystick=` in `mw3.cfg`. Unplugged during a mission, the stick lets go
  of its turn and fire instead of holding them, and works again when
  plugged back in.
- **Force feedback becomes rumble.** The original drove force-feedback
  joysticks through Immersion's driver. The rebuild reads the same effect
  file and plays the effects on your pad's or stick's rumble motors: weapon
  recoil, laser buzz, hits, footsteps, landings, the jump jets, wading and
  your death. The stick-centring springs have no rumble version.

- The game finds your MechWarrior 3 installation by itself: where its installer put it (the registry, then Program Files), or it asks you once to point at Mech3.exe, then remembers the folder in `mw3.cfg`. The original simply ran from its own folder.
- The game installs straight from its disc, with no Windows installer: `mw3 --install-from` takes the CD drive, a disc image (.iso) or a .cue/.bin, copies the game into a folder of its own and remembers it, so Mac and Linux players need no Windows to install it. Given the 1.2 patch too (`--patch`), it is installed as 1.2 in the same step, and a .cue's CD audio becomes the music.
- Alt+Enter switches between fullscreen and a window while you play (the original had no window). Your saved choice (`windowed=1` in `mw3.cfg`) is what the next start uses.
- The mouse moves the menu cursor and turns the view at your desktop's speed at any screen size, as the original does on Windows 11 with DDrawCompat. Before, fullscreen on a big screen (4K) made the mouse three to four times too fast, and a bigger window made it slower.
- In a window, the menus leave the mouse free: the menu cursor follows the system's, and the mouse can leave the window (to resize it, or to use another program).
- In a window during a mission, pressing Ctrl+Alt together (and letting go) frees the mouse; the game ignores it until you click inside the window again, and that click does not fire. The pause menu frees it too, and switching to another program frees it while you are away. Coming back to a mission (Cmd+Tab, Alt+Tab or a click) takes the mouse again at once, cursor hidden, and the click that brought the window back does not fire; a mouse you freed with Ctrl+Alt before leaving stays free until you click. Fullscreen holds the mouse as before.
- While the game holds the mouse (a mission, or fullscreen), the system's cursor stays hidden even if Windows or SDL let go of the mouse for a moment (it showed in the middle of the screen).
- The click on LAUNCH (or RESUME) no longer fires your weapons as the mission starts.
- A yes/no question over the mech lab or a mission (saving over a config, quitting) keeps the picture behind it sharp instead of dropping to low resolution until you answer.
- Switching away from the game (Alt+Tab) mutes its sounds and pauses it, as the original did, and nothing you were holding (a key, a mouse button) stays held when you come back.
- The game opens fullscreen at your desktop's resolution, the picture fitted with bars at the sides if its shape differs, instead of switching the monitor to 640x480. `--windowed` (or `windowed=1` in `mw3.cfg`) plays in a window.
- The movies play from files the importer makes from the game's own (the picture as MPEG-1, the sound as it was), since Indeo 5 video does not play outside old Windows; a movie that is not in the game's folder is not looked for on a CD drive. Timing, skipping and the end are the original's: the picture still follows the sound.

- The pause menu's background is the battlefield as you left it. The original copied the screen; the rebuild draws the 3D world on the graphics card, so it reads the last frame back from it.
- At 16:9, coming back from the pause menu no longer shows the top-left corner of the picture blown up over the whole screen (the menu's picture size stayed on the 3D view).
- At 16:9 the pause menu fills the screen with its buttons at the left edge, and stays that way after visiting the options.
- Changing the resolution in the options during a mission no longer leaves part of the HUD undrawn (the weapon list, the speed gauge's marks).

- The game's question boxes ("Ok to delete pilot?", "Replace existing game?" and the rest) keep reading the mouse and keyboard while they wait: the original's box read DirectInput directly, the rebuild's also handles the window's events while it waits.

- Changing a mech's skin in the lab no longer blanks the lab's screen for a frame. The original's skin change briefly unlocked the screen while it was being drawn, which DirectDraw tolerated; the rebuild keeps the screen locked until it is finished.

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
- The picture keeps its shape in a window of any size: black bars at the
  sides (or top and bottom) when the window's shape differs, and the mouse
  moves the same distance whatever the window's size. A window opened
  without `--window` takes the picture's shape.
- The in-mission screen size (InGameVMode) is used: a mission opens at the
  size chosen in the graphics options, 16:9 sizes included, and the menus
  go back to 640x480. Shift+V steps through every size the screen offers.
- At the new sizes the HUD is laid out for the screen: the MFD, the weapons
  list, the timer and the gauges keep to the right and bottom edges, as the
  original's larger layouts put them. It grows with the screen's height,
  covering the same share of the screen as at 640x480 (1.5 times at 720p,
  2.25 at 1080p, 4.5 at 4K), and stays sharp.
- The first time the game starts, missions play at the desktop's own size
  (16:9 on most screens) instead of 640x480; a size chosen in the graphics
  options is kept from then on. The menus stay 640x480.
- On screens wider than 16:9 (21:9, 32:9) the game plays 16:9 at the
  screen's full height, with black bars at the sides: the cockpit was never
  made wider than that.
- Changing the resolution in the middle of a mission gives the view the new shape (the original never needed to: all its sizes were 4:3), and the options reached from a mission stay in their places at any resolution.
- The pause screen and the quit box over a mission at the new sizes are
  centred and drawn at the same scale as the HUD.
- On screens refreshing at 120 Hz or more, the main menu's buttons' ripple spreads as it should (it stayed a dot: each frame's growth rounded down to nothing).
- 16:9 menus: at a widescreen in-mission size the menus open at that size
  too, their buttons spread to the sides of the screen with their labels and
  lines, and the backgrounds filled out to the edges from the player's own
  art as the game runs (nothing on disk changes). On by default; `widemenus=0`
  in mw3.cfg or `--no-wide-menus` keeps the original's 4:3 menus. A size
  picked in the menus takes effect for the menus at the next start. The
  multiplayer connection screen and lobby spread the same way, the lobby's
  middle column widening between the other two.
- With the 16:9 menus, the credits (from the main menu and at the campaign's end) scroll inside their box, not to the left of it.

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
- A message box over the 3D picture (the mech lab's "Overwrite?", the
  mission's quit) shows that picture behind it at full size, as the
  original did; the rebuild's v1.0 showed the lab's box over a purple
  screen.
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
- Mechs use the full-resolution textures and show bullet holes, as a full
  install on a good card did. The rebuild answers the installer's question
  "what was installed" with everything (every texture set and sound
  fidelity); before, it said only the smallest texture set, so the mechs
  wore the reduced textures, took no holes and the audio screen offered no
  sound quality.

- The rebuild runs on any MechWarrior 3 data, 1.0 as well as 1.2, and says
  at start which it found. On 1.0's files it makes the 1.2 patch's changes
  itself as the data loads, in its own words and numbers, without the patch's
  files: so far the messages 1.2 added (target names, the new resolutions,
  the jump-jet limit), the HUD layouts for 1152x864, 1280x1024 and
  1600x1200, 1.2's weapon balance, the mechs' torso twist, heat sinks and
  loadouts, its force feedback effects (a fall and a death shake), the
  mission fixes (objectives, boundaries, a jump-jet ceiling, patrol points),
  the named targets, the footstep and engine sounds (the engine loop
  without its click, made from 1.0's own sound), and the AI paths and
  regions 1.0 left out of Op 4b Mission 5 (taken from your own Mission 6
  and 4 files, as 1.2 did), the other new AI paths, and 1.2's fixes to Op
  3's world (the dome's gaps, the helipads' stray wall, the terrain holes).
  Op 3's two new multiplayer maps, which 1.2 added as files of their own,
  are copied into your settings folder on the first start, so a 1.0 player
  has every 1.2 map too. Your files are not changed, and data that is neither 1.0's nor
  1.2's (a mod) is left as it is. Details in `docs/patch12.md`.
- A 1.0 install can be brought to 1.2 with the official patch file you
  already have (`--patch`, its ZIP or folder): the rebuild reads the patch
  without running its installer, checks every file against 1.2's, and
  keeps the result in its own folder, leaving the install as it was.
- On the first start the rebuild converts the game's movies itself (about a
  minute, with a progress window), when its importer is beside it; the
  original played them from the disc.
- On Windows the movies play through the game's own Indeo 5 codec from your
  install, in a small 32-bit helper beside the rebuild, so they look exactly
  as they did and need no conversion; the converted copies are only the
  fall-back. It can also turn a disc image's CD
  music into files the game plays.
- Each start writes everything the rebuild reports to `mw3.log` in the saves
  folder (the start before kept as `mw3.old.log`), for bug reports. It also
  says which version of the game's data it found, and on the first start
  shows a note when no music was found or the game files are of an unknown
  version, with what to do (1.0's need nothing: they play as 1.2).

- Setting the game up takes whatever you have (ours): one POINT TO YOUR GAME button takes your installed game, the disc, a disc image (.iso, .cue/.bin with its music) or the file you downloaded (.zip, .7z, .rar, even a zip inside a zip) and installs straight from it, with nothing to unpack by hand; it tells MechWarrior 3 from Pirate's Moon by the files themselves. Started directly, the game goes straight in when it finds your install, and shows only this setup screen when it does not.
- A launcher (ours; its own program, the main icon, which starts the game and steps aside while it runs; the game also starts directly) finds your MechWarrior 3 by itself and, when it cannot, sets it up from buttons instead of command lines: point to your game (with a progress bar and Cancel), apply the official 1.2 patch (offered only for game files of an unknown version), add the music from a disc image.

## Kept as the original, on purpose

Known quirks the rebuild deliberately keeps:

- Wading in any depth of water gives the full x8 cooling, and only the left
  foot counts.
- Elementals have one point of armor everywhere and die to a single hit.
- Missiles that miss vanish at the end of their range without exploding.
- Radio chatter plays the newest line first.
- A lancemate told to follow you or attack walks round a wall in the way instead of standing behind it (bug fixes on).
- A lancemate following you takes the way you walked, round corners and buildings and along canyons, instead of stopping against a wall or short of you (bug fixes on).
- The mobile field bases stay where you called them after a repair: the next mech (a lancemate told to repair, or you powering down on the spot again) is repaired without calling them again; the third MFB press still sends them away (bug fixes on).
- Every mech's engine tops out at 400.
- Jump jets lift you when you fire them walking uphill at more than 30 fps (the original could skim the ground with its jets burning).
- The reticle turns yellow over a target out to your selected weapons' range, past 800 m (the ER large laser, ER PPC, Gauss rifle and others reach further); the original stopped looking at 800 m (bug fixes on).
- A targeted building such as the Clan HQ in mission c3m1 shows on the radar and the map where it stands; the original pointed it at the map's zero point instead (bug fixes on).
- The movies play smoothly with their sound: each frame shows once, when the sound reaches it; the original repeated one frame and skipped one every three or so, because it looked at the sound only when its own clock ticked (bug fixes on).
- The movies no longer flash a frame from two seconds earlier every two seconds when they play through Windows' own Indeo codec: at each turn of the sound buffer the original read the sound's position as a whole buffer back (bug fixes on).
- Multiplayer runs over UDP with ENet instead of DirectPlay, which modern systems no longer have: games are found on the local network or joined by address (`IP:port`, the host's UDP port forwarded for internet play). IPX, modem and serial connections are gone.
- The multiplayer connection screen searches the local network as soon as it opens: games there show up without ticking Connect or pressing Find Game. Typing a host's address and pressing Find Game (or Enter) while it searches asks that host instead of stopping the search. The modem's Answer, Dial and Call buttons are gone, and the connection list has its one entry.
- Hosting a multiplayer game opens its UDP port (27930) on your router by UPnP when the router allows it, and closes it again when you stop. The connection screen shows the address to give your friends, with a click to copy it, and your LAN address; when the internet cannot reach you it says why (no router answered, the router refused, another router in front of yours, or your provider's carrier-grade NAT) and what to do.
- Joining a game with your player name already in use there is refused right after joining (the original checked before; our connection learns the names only on joining), with the same message.
- Joining a host who is playing the other game (Pirate's Moon from MechWarrior 3, or the other way round) is refused with a message naming that game. The original refused it too, but only said the host's version was older or newer.
- Another player's loadout that arrives cut short is read only as far as it goes, instead of past its end.
- Another player's record that arrives cut short makes no mech instead of being read past its end.
- A multiplayer update or shot aimed at something that is not another player's mech names no player, instead of whatever memory followed that object.
- A multiplayer update that arrives cut short is dropped, and its shots and damage are read only as far as it goes.
- With bug fixes on, other players' aim is carried forward by the measured lag as intended (the original mixed seconds and milliseconds, so it barely moved).
- With bug fixes on, a player whose mech is shoved to death by another player's collision credits that player with the kill.
- A multiplayer clock message that would end the game outside a network game is ignored instead of crashing.
- Messages from other players are checked before they are used, as anyone on the internet can send anything: a crater on ground that does not exist, an effect on objects that do not exist, more shots than a message holds, or a move the mech does not have are dropped instead of reading or writing past the game's tables.
- The multiplayer host no longer re-checks its locked game rules every 51 seconds (a guard against memory editing that ended the game).
- The multiplayer lobby no longer sends the scores to an online game service (the MSN Gaming Zone's score reporting, long gone).
- The multiplayer lobby shows the host its internet and LAN addresses, with a click to copy, as the connection screen does.
- The multiplayer lobby takes game settings only from the host, and checks them before use.
- The multiplayer lobby takes the start of a game only from the host, and reads the scores between games only as far as the message goes.
- The multiplayer lobby's chat reads each message only as far as it goes.
- In-game multiplayer chat lines are read only as far as the message goes, and control characters in them show as '?'.
- With bug fixes on, another player's 'Mech no longer throws its arms about on your screen after a hiccup in the connection (a delay longer than the ping's round trip was taken as a lag of 24 days).
- In a multiplayer match Escape shows the game's "Are you sure you want to quit?" box, which the original never drew; with bug fixes on it does not stop play (move and fire as normal), Y leaves the match for the main menu and N or Escape closes it. Before, an Escape in a match left the mouse unable to fire.
- The controls are saved only when you set them in the controls menus (ACCEPT, or loading, saving or resetting a set), not from whatever they are at the moment the game closes or the options are accepted. Before, closing the game while a multiplayer match's quit box was up saved the mouse's fire button unbound, and the mouse never fired again until the controls were reset. Controls already saved that way by an older build are mended when the game starts: if the fire command has no mouse button and the left button is bound to nothing, the left button goes back on fire (a fire button moved on purpose is left alone).
- A player who leaves a multiplayer game disappears for the others at once instead of after about half a minute.
- A multiplayer game keeps running while its window is minimised or in the background, so your 'Mech is not a frozen target for the others (the original paused it).
- On a Mac the numeric keypad types digits in text fields (an IP address on the connection screen); a Mac has no Num Lock, which the game needed on.
- A locked multiplayer game turns away players who try to join it, instead of only greying out its Join button.
- With bug fixes on, locking a multiplayer game with "Use stock 'Mechs" keeps the stock loadout a player chose instead of resetting it to the mech's first.
- The game editor the original carries but never lets you reach (its menu over the mission, the view on the invisible "stealth" camera flown with the movement keys), for mods only: the MODS screen's EDITOR opens a mod's or custom campaign's mission with it up, and a custom campaign played from MODS has it in every mission, by EDITOR in the pause menu or Shift+F12 (Shift+F12 again or EXIT GAME EDIT closes it and puts the view back on your mech; the original left it on the stealth). The original's campaign, Pirate Missions, Instant Action, training and multiplayer have no editor; `--debug-camera`, a developer's command-line flag, opens it anywhere (mw3.cfg's `debugcamera=1` is no longer read). Its EDIT AI NETS opens the AI net editor: a net picked from the original's list, its nodes and links drawn over the view and picked with the mouse as in the original, linking picked nodes, Move Node, Add veh to node (in the original its OK did nothing; here it makes the vehicle at the node on the net and writes it into the mission's vehicle list in the editor's mod), Node Params (in the original an empty window; here a form of the picked node's settings, saved with the rest), Change Net Params (the original's window that shows a second net file beside the edited one; its file dialog a list of the mission's net files), Tree View (the world's node tree), Delete Node (a node or a picked line, asked first with OK and Cancel, as the original did), New Net, GOTO Net, and Save Changes, which saves those edits, a new net included, and says on screen where; the other editing rows are not there yet. EDIT VEHICLES opens the original's Mech AI dialog: a vehicle of the mission picked, Apply putting its AI settings on it; its first two pages work as the original's (p2, the vehicle: its place, team, pilot name, chassis, loadout, skin and home net, Show to draw that net, Show InOut to draw the mission's boundary as walls (the original's way of drawing it could hang on a very large boundary piece and leave a stale drawing behind after the mission; here it does neither), Send home to put the vehicle back at its place on its net; p1, when it wakes and how far it leaves its net to follow), the number boxes reading and refusing what the original's Windows boxes did; Attack (its strategy and the strategy's own settings), Retreat, p5 (the scan), Fire and Valve (which valve the vehicle sets, and when) work too; a valve or region name typed into its list is sent with Enter. Its Save button (the original had none: Apply only changed the running mission) writes the vehicles Applied into the mission's vehicle list in the editor's mod, or into the mod itself from the MODS screen's EDIT, the previous save kept as a .bak. On the MODS screen, EDITOR lists a mod's or custom campaign's missions and ENTER opens one with the editor already up; its saves go into that mod itself (the replaced file kept as a .bak), and EXIT GAME EDIT goes back to MODS. The MODS screen offers no way in to edit the original campaign. The editor's dialogs are windows of the rebuild's own over the mission (text fields, lists, a tree), where the original used Windows dialogs; its menus are the original's. Saving an AI net the mission is running on rebuilds it at once, and the AI mechs and vehicles on it carry on along the new one (the original's version of this was for a net format no shipped file uses). A link is drawn blue whenever its two nodes see each other (the original could draw some of those red, from leftover memory). TELEPORT (ours, in a blank row) opens the developers' teleport, which the original carries but never lets you reach: pick your own place, a nav point or one of the places each world ships (or type a name to add one), and OK moves your mech there and saves the places into the mod as the original did (its backup now kept beside the file, and a place list without a name or with nothing picked no longer crashing it, and Cancel no longer moving you and saving as OK did). EDIT WEAPONS, an empty row in the original, opens New Weapon in a mission opened from the MODS screen's EDITOR: a new weapon copied from one the game knows, its name, ammo, tech and title, its damage, heat, range, fire rate, velocity, weight, slots and rounds a ton, and its muzzle flash, sounds, projectile, trail and impact (picked from those the game's weapons use, each shown where the view points when picked) set, written into that mod (its mod.txt and its weapons file, or the mod's own weapons.zrd text file with its text kept, each earlier copy kept as a .bak) so the mech lab offers it from the next start; the original game reads the same mod files without trouble. The editor has help of its own: a tip on every field, button and menu row, and a HELP row (in a blank row of each menu) with a quick start; docs/editor-guide.md gathers the same text.
- The game editor's Save Changes, which in the original never wrote anything to disk, saves an edited AI net (once the net editor is there) into the mission's archive in an editor mod of its own, `mods/<mod> edits` (`editor edits` for the game's own data): never the installed files or a mod the editor did not make, the previous save kept as a `.bak`, and the mod turned on just before its source so the edits show from the next start. Unedited parts are kept byte for byte, edited ones with their exact values.
- The launcher closes once MechWarrior 3 is on screen, instead of waiting hidden for the game to end; if the game fails to start it stays and says so.
- The launcher is installed as "MechWarrior 3" (the Windows Start menu and desktop, the Mac's "MechWarrior 3.app", the Linux desktop entry); the game itself (mw3.exe) is installed too and starts directly, without a shortcut of its own.
- The launcher shows the games, GAME FILES and Quit (Escape too). GAME FILES opens the "find your game" screen at any time, to pick another copy or refresh it, and to add what it lacks from the disc; a new copy replaces the old only once it is complete. The launcher works with the arrow keys and a gamepad's d-pad, A and B as well as the mouse.
- Choosing a game whose files are not found opens a short "find your game" screen: FIND THE GAME searches the computer and the disc drives and uses what it finds; if nothing is there, one picker takes a disc image or download, or Mech3.exe (any file) of an installed copy. Once the game is found or installed it starts at once. The separate Setup, "a folder instead", "search again" and "apply the 1.2 patch" buttons are gone (1.0 plays as 1.2 by itself).
- On the Mac and Linux the movies are converted by the launcher right after the game is set up, with a progress bar and Cancel, instead of the game's first start waiting on a status line. The file picker no longer takes long to open on the Mac.
- An install whose movies stayed on the CD, or with no music, is told so before the first game, with ADD FROM MY DISC: it takes the movies from your disc in a drive, a disc image or the disc's folder, and the music from a .cue disc image.
- On a wide screen the movies keep their shape: the intro and the ending movie, whose black bars are part of the picture, fill the screen's width; the 4:3 movies and the logo screens are centred with black sides (they were stretched a third too wide). The shipped movies' shapes come with the game (measured by us from the pictures), so nothing is measured when they play; a movie the game does not know (a mod's) is measured after it has played, in the background, and kept in movie-shapes.txt in the settings folder. Every movie's first play on Windows no longer freezes for 10 to 30 seconds while this was measured.
- Mods: folders of game data in the settings folder's mods\ folder, enabled in mw3.cfg (mod=NAME, in order), sit in front of the game's files; a mod's loose .zrd replaces one entry of a reader archive. A mod can never replace a player's pilots, loadouts, controls or settings, and with no mods enabled nothing changes (docs/modding.md).
- A mod's loose .zrd gets the same fixes and additions as the archive's copy (the official 1.2 patch's edits, the MODS button), so a mod's edited menus keep the MODS screen; one that does not read (a '(' never closed, a stray ')', an open quote) is refused with its line named in the log and by `--check-mod`, and the archive's copy is used.
- A mod can hold a reader entry as mech3ax's JSON, as its unzbd tool writes it: zbd\reader\weapons.zrd.json, or unzbd's reader.zip itself beside the archive. The game reads it as the entry, numbers exact; a loose .zrd wins over it. One that does not read is refused with its line and place named, in the log and by `--check-mod`, and the archive's copy is used.
- A mod can hold a mech's motion (a walk, a fall) as mech3ax's JSON, as unzbd writes it: zbd\motion\madcat_walk.json, or unzbd's motion.zip. The game reads it as the archive's motion, to the same bytes. One that does not read is refused with its line and place named, in the log and by `--check-mod`, and the game's motion is used.
- A mod can hold mech models and the mech materials as mech3ax's JSON, as its unzbd tool writes them: zbd\mechlib\mech_thor.flt.json, or unzbd's mechlib.zip. The game reads them to the same bytes as the archive's, so a new mech needs no repacked mechlib.zbd. One that does not read is refused with its place named, in the log and by `--check-mod`.
- A mod can hold a world's animations as mech3ax's JSON, as its unzbd tool writes them: zbd\c1\anim\, or unzbd's anim.zip. The game builds the world's anim.zbd from it and reads it as the shipped one. One that does not read is refused with its file, line and place named, in the log and by `--check-mod`, and the world's own animations are used.
- A mod can hold a whole world as mech3ax's JSON, as its unzbd tool writes it: zbd\c1\gamez\, or unzbd's gamez.zip; a new world a mod adds can be its JSON too. The game builds the world's gamez.zbd from it, byte for byte as the shipped one would be. One that does not read is refused with its file, line and place named, in the log and by `--check-mod`, and the world's own file is used.
- Saving no longer fails where Windows refuses its own temporary file (a very long temp folder path): the save uses a temporary file of its own instead.
- A multiplayer game needs every player's gameplay mods to match: a player joining with other mods is told which mods the host uses and which they use, and LOCK OPTIONS names the player whose mods differ. Text, image, sound and music mods never count.
- Mods can add new worlds (world= in mod.txt) and Instant Action maps on any world (ia_world=), and change the game's text (strings.txt, over Mech3Msg.dll's messages); a pilot whose Instant Action world came from a mod since removed plays on the first world.
- A mod's new world can have multiplayer maps (readermp3.zbd and on), listed in the lobby with the others.
- A broken entry in a reader archive (a mod's, say) is refused and named in the log instead of being read into whatever follows it; `mw3 --check-mod DIR` reads a mod as the game would and reports what it would refuse.
- Hand-edited mod files are made to work where they can. A map whose settings entry lists one item more than it holds loads as it does in the original, which reads on into the next entry (many community multiplayer and Instant Action maps are made that way; until 1.1.0 the rebuild refused such a mod whole). Where an entry goes wrong part way (a value that is no kind of item, a count no file could hold, the file ending early), what is good before that point is read and the rest left out, where the original read on into whatever followed and could crash. The check and the report say each one in plain words: which file and entry, what is wrong and how many bytes in, what the rebuild did, and what the original game does with it. IMPORT no longer refuses a mod for such a file; one that cannot be used at all is left out by name and the rest of the mod is added.
- A broken texture or image library, world texture directory, sound or script file (a mod's, say) is refused and named in the log instead of being read past its end; a refused texture or sound is a missing one, a refused script line ends its script. `mw3 --check-mod` looks inside these files too.
- `mw3 --check-mod` no longer calls a good world's gamez.zbd, anim.zbd or software texture sets broken, and reads a mod's animations from a folder of any length.
- Mods can add new mechs: a `mech=` line in mod.txt names the mech, its tech, tonnage and weight class, and optionally the mech whose models, motions and sounds it uses and the name shown. The original's 19 keep their numbers; a mod's mechs come after them. Their rows count toward multiplayer's check that every player has the same gameplay mods.
- A Pirate's Moon mod (`for=mw3pm` in its mod.txt) is left out of MechWarrior 3; only `for=mw3` (a version may follow) or none is taken.
- Mods can add new weapons: a `weapon=` line in mod.txt names a weapon the mod's weapons.zrd defines, whether it takes ammo, the techs it comes in and the name shown. The game's weapons keep their numbers; a mod's come after the plain PPC's, with bug fixes on or off.
- A pilot or loadout that uses a mod's mech or weapon names it in a `.rows` file beside it, so it loads with the same mechs and weapons when mods are reordered, and is refused with a message naming the missing mech or weapon (not loaded, and left as it was) when that mod is off. The mods' mechs appear in Instant Action, the lobby and the mech lab's lists; a mod's mech whose definition or model the game would not find is left out (said in the log and by `--check-mod`), and if a mech still cannot be built at launch, the game's own "Failed to create player" box now shows (the rebuild only logged it).
- Starter mods: `--make-starter mech`, `weapon`, `translation` or `mission` makes a small example mod of that kind from your own game (a Thor variant, an ER PPC variant, the main menu in German, a one-mission campaign), each with a short guide (docs/starter-mods); it only ever makes a new folder. A mod's new mech definition as a loose file beside reader.zbd is now read (the checks passed it, the game did not).
- The MODS screen's TEST plays any mission straight away with your mods on, the chosen mod's own missions first and then the game's, so a weapon or mech mod can be tried without a pilot or a campaign; when the mission ends you are back on MODS, and edits to the mod's weapon and mech files since the last TEST are in the next one without a restart.
- Mods can list custom campaigns (`campaign=` in mod.txt, several per mod): a text file of missions in play order, each on any world the game or an enabled mod has. The game reads and checks them when the mods load and with `--check-mod`; a campaign with a line it cannot read, or a mission on a world nobody has, is left out whole and the log names the line. Entering one comes with the in-game MODS screen; the original campaign is never touched. A custom campaign's pilots and saved games live in their own folder (`pilots\mods\<mod>\<campaign>\`), apart from the original's; the game only ever removes that folder when you say yes, and only one it made itself. A custom campaign plays its missions in its list's order, with the field bases, briefings and movies its lines give, and its last mission won ends it with its own closing movie. A MODS button on the main menu opens a screen in the game's own style listing the custom campaigns, entered with ENTER, and the other mods, turned on and off and reordered there, with the files two mods both replace pointed out; the changes take hold at the next start. ADD MOD on that screen takes a mod's .zip, .7z or .rar (or its folder), finds its files whatever the archive's layout, keeps only what differs when it is a whole copy of the game, checks it, and puts it in the mods folder under a name no folder there has; an archive with a file that would land outside its folder, or with a name Windows cannot hold, is refused whole. The launcher's MODS list has the same ADD MOD.
- ADD MOD is called IMPORT now, and EXPORT beside it packs the selected mod into one .zip (named after the mod and its version) that IMPORT takes back as the same mod: its own files only, with a checksums file that IMPORT checks, so a zip changed or cut short on the way is refused. In the game's MODS screen and the launcher's.
- The mouse settings, like the controls, are saved only as you set them in the menus.
- The controls screen's KEY A and KEY B columns take a key again: click the box, press the key (v1.0 never saw it; the mouse and joystick columns were not affected).
- A MOUSE SENSITIVITY slider on the Game Options screen, under Invert Mouse (0.5 to 3.0): it sets how far the mouse turns the mech in a mission. It starts at the original's 1.3, and Invert Mouse keeps it.
- An UPGRADES page, one press from the options (in the menus and in a mission), saved with your settings: five switches for the rebuild's play changes (WATER MOVEMENT, MFB REGROUP, IMPROVED AI, ARTEMIS TRACKING, SMALL FIXES; the top of this file says what each covers) and WIDE MENUS. It began with one bug fixes switch, which the five replaced (2026-11-10).
- The music pauses while a movie plays and comes back after (the campaign's opening movie no longer has the menu music under it), as when the game read its movies off the CD.
- On a wide screen the arm weapons reach the reticle across the whole width: an arm weapon still fires straight ahead when aimed far off to the side, but the limit now widens with the screen, so it starts at the same place on screen as on a 4:3 one (bug fixes on).
- Cheat codes in single player (ours): Shift+Enter, Keypad * or Keypad − in a mission opens a box for six codes, from a radar that shows everything to an instant win (README, Cheat codes).
- Shooting off an arm with a weapon in its hand no longer reads memory the game has just freed (harmless on Windows 98, a possible crash today).
- A crash late in a fight (seen in Operation 4's last fight): the graphics' matrix stack no longer writes through a bad entry when it is pushed or popped out of step; the slip is refused and logged instead, and the stack is cleared each frame. Its cause was the rebuild's own (a shortcut that took a moving mech's position on the shared stack instead of on its own), and that is fixed too.
- A lancemate no longer gets thrown back by his own feet when you are in another part of a mission (bug fixes on): he used to lose sight of the floor under him and read the land above as a wall. In the mission where you meet Epona Rhi he now follows you through the tunnel instead of standing at its mouth for ever.
- Lancemates find their way when their straight walk fails (bug fixes on, single player): round walls and parked vehicles, up ramps and along narrow walkways, and across a river at its bridge or ford, for follow, attack and repair orders. A lancemate will now come to you inside the areas the original told the AI to keep out of, if there is a way. They notice doors opening and buildings falling. On open ground they walk exactly as before.
- With no joystick attached, a saved controls file with axes on the stick no longer makes your mech walk or its torso turn by itself (bug fixes on). The original moved the axes off the stick only for a file exactly like the defaults; a slider, or just a changed deadzone, kept them. The binding is kept for when the stick is back.
- On fast machines, small bumps throw vehicles much less: the suspension's reaction to the ground is capped at what it is at 30 fps. On rough ground they still tip more than at 30 fps, and they still jump off steep banks at full speed now and then, as in the original.
- Linux: the game no longer closes at the end of the intro movies. The launcher closed when the game appeared, which unmounted the AppImage the game was still running from; it now waits, out of sight, until the game ends.
- A crash now says so, in the log and in a message box that names the log, as does a start that cannot open its window or graphics. It used to just close.
- Linux: an install whose files are in another letter case (all lower case, all upper case, mixed) now works. Windows ignores case, so the game's names never had to match; on Linux it could not find them.
- A softer message when files match neither 1.0 nor 1.2: they look modded or from another version, data mods are read but may not play right, and a mod that patches the old Mech3.exe won't apply. It never stopped the game, and still doesn't.
- Your data folder (settings, pilots, saves, converted movies) can live on another drive: GAME FILES, MY DATA FOLDER in the launcher copies it, checks the copy, then switches over, and removes the old copy only if you say so. If the drive is missing at start the game says so instead of starting afresh.
- An install without the game's fonts (arial.ttf, impact.ttf, lucon.ttf) shows its text with the system's fonts, and says so in the log, where it used to show blank labels.
- Restarting a mission after a mech lost a limb can no longer crash in the pathfinding's start-up, which followed the limb to its freed mech.
- The launcher brings its own font (Saira Condensed Bold, free under the SIL Open Font License) on every system, in place of Bahnschrift, DIN Condensed or a Linux face, so a Linux without the expected fonts no longer shows empty buttons. Without it the launcher searches the system's font folders.
- An install without the game's fonts, on a system without them either, now shows its text in free stand-ins the package carries (Liberation Sans, Anton, Liberation Mono), sized to keep the original's line heights.
- A mech can no longer be given more than 16 weapons by a mod or by another player's loadout in multiplayer: the extra one is left off and named in the log, where it used to overwrite memory.
- A modding reference (`docs/modding-reference.md`, also `mw3 --reference FILE`): every key mod.txt, a custom campaign, weapons.zrd, the mech definitions and a mission's AI, targets, paths, regions, switches (valves) and objectives files, the turrets, vehicles, worlds' weather, landmines, the HUD's layout, the sounds and the dialogs take, every script command with its arguments, and the members each of mech3ax's JSON forms takes, with its type, what the game uses when it is left out, and its limits; checked against the game's own readers so it stays right. More files follow.
- The launcher finds Pirate's Moon by itself, as it does MechWarrior 3: its registry key, its usual folders, Wine, Proton and Lutris prefixes, and on the Mac CrossOver and Whisky bottles (searched for MechWarrior 3 too now).
- Pirate's Moon installs from its disc, a disc image (.iso or .cue/.bin) or a zip or 7z of one, without running its Windows installer, its CD track as the music, as MechWarrior 3 does.
- Before Pirate's Moon starts, the launcher checks its files and says in plain words what is wrong: it refuses only when the game cannot start at all, names the operations whose missions are missing, and notes an empty high-quality sounds file (the game then uses its low-quality sounds), which the disc's copy can replace in the settings folder without touching the install.
- The launcher's GAME FILES works for Pirate's Moon as for MechWarrior 3: find or install it (into its own settings folder), add its music and sounds from the disc, and move its data folder. MechWarrior 3's files and settings are never touched by it.
- A mission whose files are missing (an operation's world, or the mission scripts) no longer crashes or drops you back at the briefing without a word: the load stops and a box says what is missing and to install the game files again.
- Before MechWarrior 3 starts, the launcher checks its files too and says, once, which missions are missing (an operation, training) or that its sounds file is empty, as it does for Pirate's Moon.
- The launcher has an UPDATES button: it asks GitHub for the newest release of the package and, when there is a newer one, shows its notes and asks whether to download and install it.
- The launcher shows the package's version at the bottom of its main screen.
- Each game in the launcher is labelled "official" or "modified", saying what is changed (its data, its program), and how many mods are on. It is only a label: modified games start as before.
- Each game in the launcher shows its data's version and its program's version (read from the program's file, without starting it).
- A mod can now change or add a world's animations one at a time: a single animation's file (as mech3ax's unzbd writes it) in the mod's `zbd\<world>\anim\` or `zbd\anim\` folder replaces that animation or adds a new one, while every other animation stays as the game (or another mod) has it, so two animation mods can be on together (the MODS screen says when two of them change the same animation, and the mod check reads each animation file and names the line of a broken one); `mw3 --build-anim <mod>` builds the same animations into whole files for the original game in the mod's `original\` folder (which this game never reads), and `anim=defs` in mod.txt says the mod's own whole animation files are for the original only; an animation can bring its own model (a node file beside it), and an animation file that names a model the mission does not have is left out with a note instead of stopping every animation after it; a saved game made under other animation mods no longer restarts a running animation on the wrong one; a campaign saved with gameplay mods notes them in the save (an entry the original game skips), and loading it with other mods, or none, says so on the loading screen.
- A mod's report in plain sentences: REPORT on the game's MODS screen says what the mod replaces and adds, the mechs, weapons, worlds and campaigns it brings, what in it is ignored, and each thing the game would refuse or left out at this start and why. `mw3 --mod-report <folder>` prints the same.
- A mod can say what it needs in its mod.txt: another mod (`needs=`, with the least version if it matters), a mod it does not work with (`conflicts=`), and the least version of this rebuild (`game_version=`). The MODS screen and the launcher warn when a mod is turned on without what it needs, or with a mod it conflicts with, and its report lists them; the mod is still turned on.
- EXPORT checks a mod before packing it and says how many things in it the game would refuse (the zip is made all the same), and a mod folder without a mod.txt gets one in its zip, so it imports as a mod straight away.
- The enemy and lancemate mechs steer, walk their paths, jump, pick targets and aim exactly as the original does on today's Windows, down to the last bit of each sum. Before, a rounding of the rebuild's own could now and then turn a mech the other way for a frame, pick a different link on its path, or aim a hair off.
- Where a mech's feet meet the ground, where shots and sight lines hit walls, buildings and terrain, and what lies inside an area of effect are now worked out exactly as the original does on today's Windows, down to the last bit. Before, the rebuild's own rounding could put a foot a hair higher or lower, or, at the very edge of one of the world's invisible grid squares, miss the ground under a point that the original finds.
- The launcher no longer has a MODS button or screen: mods are managed in each game, on its own MODS screen (IMPORT, EXPORT, REPORT, TEST, the list). The launcher's main screen keeps GAME FILES, UPDATES and QUIT, and each game's line still says how many mods it has on.
- The launcher no longer says "N mods on" beside a game: a game started from the launcher is always the original, and mods are played from its own MODS screen.
- The launcher says "Opening the file picker..." before it asks the system for the picker. The system can take a second to open it (far longer the first time on some Macs), and until now the launcher sat frozen on the old screen meanwhile.
- A resolution picked in GRAPHICS & AUDIO changes the menus' shape as soon as you press ACCEPT (4:3 to 16:9 or back), and so does WIDE MENUS on the UPGRADES page: you are put on the main menu in the new shape. Before, the menus kept their old shape until the game was started again.
- Mods by campaign: the main menu always plays the original game. Starting the game no longer applies any mod, so CAMPAIGN, INSTANT ACTION and multiplayer on the main menu use the game's own files, and nobody's campaign can be changed by a mod. Mods are played from MODS: ENTER on a custom campaign closes the game and starts it again inside that campaign, with its mods, its own pilots, saves and saved mech loadouts; ENTER with the mods list chosen does the same for the game's own campaign with the mods turned on in that list. CANCEL on the campaign's pilot screen brings you back to MODS, on the original again. The mods you had turned on are kept: they became the campaign's ticks. (The new CAMPAIGNS list with a checkbox per mod, Instant Action's own MODS button, and TEST and EDITOR from MODS follow; until then TEST and EDITOR say they are not available there.)
- Instant Action has its own MODS button. It opens a list of every mod you have imported, each with a checkbox, in the order they win; ACCEPT closes the game and starts it again in Instant Action with the ticked mods (their maps, mechs and weapons), with its own pilots and saved mech loadouts kept apart from the main game's. The screen always says what is on ("MODS: 2 on"), and ALL OFF goes back to the original Instant Action. With nothing ticked, Instant Action is the original and nothing restarts. The ticks are remembered: with mods ticked, INSTANT ACTION on the main menu goes straight into the modded one. Your pilot comes along (see the next line); if a ticked mod has been removed, Instant Action opens as the original and says "MODS: 1 missing".
- The MODS screen lists campaigns: the game's own campaign first, then every campaign you imported and every copy. Choosing one shows its mods at the right, each with a checkbox: every campaign keeps its own ticks, so a weapons mod can be on for one campaign and off for another. A campaign's own mods are shown as "[on]" and cannot be turned off. PLAY starts the chosen campaign with its mods. A campaign that cannot be played (a mod it needs was removed) is marked "!" and says which. Importing a mod no longer turns it on: tick it for the campaigns you want it in.
- Pilots in a campaign from MODS and in modded Instant Action: the pilot screen there lists that campaign's own pilots and your main-game pilots together. Pick one of yours and it is copied in at that moment, with your saved mechs, and nothing is asked; your main-game pilot is never changed. In the game's own campaign (with mods ticked) the copy carries on from where you are; in a different campaign it starts at that campaign's first mission. From then on the copy goes its own way: to bring newer main-game progress across, delete the campaign's copy of the pilot there and pick the name again.
- TEST on the MODS screen works again, by campaign: choose a campaign, TEST, pick a mission and PLAY, and the game starts again in that campaign with its mods and goes straight into the mission. When the mission ends you are on a short list, "TEST in <campaign>", where PLAY starts another mission at once (no restart, the mechs read again, so a modder edits and tests in a loop) and BACK returns to MODS.
- A mod made for the original game that is laid over the game's own files (changed missions, weapons, Instant Action scenarios, all in one) is an ordinary mod with a checkbox: tick it for the campaign you want it in, for a copy, or for Instant Action (its MODS button), so every part of it can be reached. Only a mod that brings a campaign of its own gets a row in CAMPAIGNS.
- EDITOR on the MODS screen works again, by campaign, and it only ever saves as a copy: choose a campaign, EDITOR, pick a mission and PLAY, and the game starts again in that campaign with the mission open in the editor. The first time you save, it asks for a name ("<campaign>, my edit") and makes a new campaign of your own that holds just what you changed, on top of the one you opened. The campaign you opened, the game's own files and mods you imported are never changed by the editor.
- DELETE and RESET on the MODS screen. DELETE removes what is chosen and says first what goes: a campaign you made in the editor (with its pilots and saved games), a mod you imported (a campaign that had it ticked then shows "!" until you untick it or import it again; an imported campaign's pilots and saved games are kept for when you import it again), or what is left of a campaign whose mod is gone. On the game's own campaign the button reads RESET: its ticked mods are cleared and its pilots and saved games on the MODS screen go, and your pilots in the main game are not touched. The game never deletes its own files or a mod folder you put in the mods folder by hand.
- A mod's texture file whose entries name a palette the file does not have (a slip in hand-edited files: two community texture mods have it) loads: such a texture is used without a palette when it needs none, and left out (shown as missing) when it cannot be drawn without it. The original game crashes when it loads one. IMPORT and `--check-mod` say which textures, as warnings, and no longer refuse the mod.
- A mod with a faulty file is no longer refused whole. IMPORT leaves out the one file the game cannot use (or the one sound, script or line of mod.txt), adds the rest, and says how many warnings there are ("Added <name>: 9 warnings: REPORT lists them. Tick it for a campaign, then PLAY."). Only a mod that is not a mod at all is refused: a mod.txt that cannot be used, one made for the other game, or nothing usable in it.
- IMPORT takes a mod the way it is shipped. Given the mod's installer program itself, it reads the files out of it without running it (the community's mods come as such installers), compares each with your own install and keeps the ones that differ or are new as the mod. Given a game folder a mod was installed over, it does the same with that folder. The program files and dlls in either are never part of a mod.
- IMPORT takes a mod's texts. A mod made for the original game changes briefings, objectives and names in Mech3Msg.dll, a program file that was left out, so those texts showed as the game's own or as bare ids. IMPORT now reads that file's texts, keeps the ones that differ from yours or are new in the mod's strings.txt, and says how many; the dll itself is still not part of a mod, and one that cannot be read is a warning, not a refusal.
- Two mods that change the same game file can both be on. Mods made for the original bring whole copies of the game's record files (the missions' and the main reader archive), so the first one ticked hid everything the second changed in the same file. IMPORT now keeps only the records a mod actually changes, and the game lays every ticked mod's records over its own file when it starts; where two mods change the same record the first ticked wins. EXPORT still writes the whole files, so an exported mod works in the original game too. Mods already added are not changed: import them again to get this.
- IMPORT takes a mech loadout. A loadout exported from the mech lab (a .m3c file, Ctrl+X) picked with IMPORT on the MODS screen goes into your import folder, and Ctrl+I in any mech lab then takes it; a file already there is never replaced.
- The debriefing after a mission played with TEST on the MODS screen names the mission ("Operation 2, Mission 3"); its title showed the unfilled text "Operation !d!, Mission !d!".
- REPORT on the MODS screen sorts a mod's warnings for the person who made it: what stops part of the mod working, what works here but may crash the original game, and slips that do no harm, each saying where it is, what is wrong, what the rebuild does, what the original game does, and where it can, what to change in the file. The whole report is also saved as a text file in `mod-reports` in the game's data folder (the screen says where, and MODS FOLDER opens it while the report is up).
- Pirate's Moon: a mod without a mod.txt (files laid over the game's, as the community's installers ship them) can be added. IMPORT wrote the wrong game word into the mod.txt it makes, and the game then refused its own mod as "for pm, not Pirate's Moon".
- In a campaign from MODS, coming back from a mission keeps your pilot chosen on the pilot screen. The game read the campaign's pilots again from disk after each mission and then looked once at the old, deleted one (a read of freed memory that could, rarely, crash or show the wrong saved games), and left no pilot chosen.
- The MODS screen's text stays inside its boxes at 4:3 and wide: a long mod name ends "..." in the list (DETAILS shows it whole), a report's rows are wrapped to the box, and the left box's heading says what it shows (CAMPAIGNS, REPORT, TEST, EDITOR). After IMPORT the screen says how many warnings there are and to press REPORT; an import from an installer showed only how many files it took. A mod you put in the mods folder by hand says so and how to remove it. Instant Action's MODS no longer lists a mod that is only a campaign.
- DELETE on the MODS screen asks once. After you confirmed deleting a mod, a second box asked at once whether to reset the game's own campaign (the RESET tab appears where DELETE was, and the same click reached it).
- Multiplayer can be played with mods when every player has the same ones. The multiplayer screen has a MODS button: tick the mods, ACCEPT, and the game starts again with them on, your pilot kept; the host's game uses the host's mods. A joiner whose mods are not the same is turned back and told which mod is missing, differs or is extra. The check compares what the mods change in the game, so a mod added with IMPORT and the same mod copied in by hand count as the same. The original had no mods.
- The Battlesport editor has a pointer. With the editor up the mouse is let go and the system's cursor shows, in a window and fullscreen, and the row under it lights and takes the click (EDIT AI NETS, EDIT VEHICLES and the rest). Before, as in the original, no pointer was drawn: only the row under the unseen mouse lit up.
- One switch between playing and editing a mission. In a mission opened from the MODS screen, the pause menu's button reads EDIT MISSION while you play and PLAY MISSION while the editor is up, and Shift+F12 does the same; either way you stay in the mission. Before, the button was called EDITOR, and pressing it (or EXIT GAME EDIT) with the editor up left the mission for the MODS screen. Now only quitting or finishing the mission goes back to MODS.
- The MODS screen has six buttons: PLAY, MISSIONS, IMPORT, INFO, a delete tab and BACK. A click on a mod's box turns it on or off for the chosen campaign (ON/OFF is gone); a click on its name chooses it. MISSIONS replaces TEST and EDITOR: pick a mission and PLAY, and in the mission the pause menu or Shift+F12 switches between playing and editing. INFO replaces REPORT, says what a campaign is as well as what a mod changes, and holds EXPORT and OPEN FOLDER. The delete tab is labelled by what it removes (DELETE MOD, DELETE COPY, RESET). IMPORT's one file box takes a folder mod by a file inside it (ADD FOLDER is gone). UP and DOWN show only for a ticked mod that shares something with another ticked mod.
- Every button on the MODS screen says what a press does when the mouse is over it, for example "Pick a mod's .zip, its installer (.exe) or a file in its folder. Don't run the installer: IMPORT reads it."
- A mission where the editor can be opened says so for its first seconds: "PAUSE: EDIT MISSION, or SHIFT+F12".
- Instant Action is unmodded when you open the game. Mods ticked for it last time are remembered as ticks on its MODS screen ("Ticked last time. ACCEPT starts the game again with them."), not applied by themselves; before, INSTANT ACTION on the main menu closed the game and opened it again with them. With no mods at all, that screen has no ALL OFF and says where to get one.
- Windows: the game no longer starts now and then with the cursor stuck in the middle of the screen (seen mostly with Pirate's Moon, started from the launcher). The launcher now hands the keyboard and mouse over to the game, the game takes the mouse only once its window really is in front, and if the mouse still gives no motion the next key or button press takes it again. The log says what state the mouse was taken in.
- The launcher's BEFORE YOU PLAY screen fits its text: with long notes (a long settings folder, or movies, music and sounds all missing) the orange status line was drawn on top of the notes' last lines, and the line under ADD FROM MY DISC sat on the button's bottom edge. The notes now use the panel's width, the status line comes under them, and the button's second line is inside it.
- Wide menus with the original's FastMenu option (-seasick): the screens were drawn once at their 4:3 layout and then spread, so the main menu showed CREDITS and QUIT twice and Instant Action's map box stood in the middle with its texts at the right. The screen is now laid out wide before it is drawn into its one picture.
- Instant Action's MODS button says what it does when the mouse is over it: "Play Instant Action with mods. The game starts again to load them."
- A mission started from the MODS screen's MISSIONS can be left when it ends. On its debriefing neither the tick (continue) nor the list (abort) did anything, because the stand-in pilot has no campaign to go on with, so you stood on the debriefing; both now go back to the missions' list.
- A world the game does not have is found when a mod is imported. A mod with no mod.txt (a folder, a zip, or a game folder it was laid over, as a zStudio export is) holding `zbd\<name>\gamez.zbd` for a name none of the game's worlds has gets the lines it needs written into the mod.txt the game makes: `world=`, `ia_world=` when it has Instant Action maps, and a campaign of its missions in number order, so PLAY and MISSIONS on MODS work with no hand edit; its multiplayer maps are seen by the lobby. INFO says what the world has and what it lacks, and whether its script takes another world's scenery. A world's script left loose at the top of a modded game folder is taken.
- A mod's campaign that could not load is left out at the start and said, instead of failing at the launch: one whose world's build script is nowhere the game reads (the load stopped, the next one failed and the launch crashed), and one with a mission whose own archive (readerm<n>.zbd) is missing (the game went back to the main menu with nothing said). An Instant Action world with no script is left out the same way.

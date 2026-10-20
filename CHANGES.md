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

## Crashes and broken data that no longer break the game

None of these change normal play; they stop a crash or a hang when something
unusual happens.

- A mech file with a bad version no longer crashes the game when it unloads.
- The game starts without a mouse (the original crashed), and pressing a
  newer key such as Power, Sleep or a web key while holding Ctrl, Alt and
  Shift no longer corrupts the game's input memory.
- A damaged or hand-edited saved loadout (`configs\<mech>.mw3`) can no longer
  hang the load or crash it.
- A damaged archive or script file is stopped at its end instead of making
  the game read or allocate gigabytes.
- Rare random rolls in equipment critical hits no longer crash.
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

## Copy protection removed

- No CD check at startup; the importer copies the movies from the disc.
- No self-check of the game's files, which made a modified game quit after
  the intro.
- Hand-edited saved loadouts load if they follow the mech lab's rules
  (weight, slots, jump jets, armor). They still get the original's
  checksum, so the original game can read them.

Details: [`docs/protection.md`](docs/protection.md).

## Running on modern machines

These change how the game talks to the computer, not how it plays.

- Keyboard, mouse and joystick through SDL3 instead of DirectInput, read the
  same way: the mouse is captured while the window has the focus (as
  DirectInput's exclusive mouse was) and moves at the same speed whatever the
  window's size; a joystick gets the game's own ranges and dead zones.
- Graphics through SDL3 (Metal, Direct3D 12 or Vulkan) instead of DirectX 6;
  the game's own drawing code is unchanged.
- Sound through SDL3 instead of DirectSound, with the same volumes and
  panning.
- Music from `music/trackNN.ogg` (or .flac, .wav, .mp3) instead of the CD;
  a ripped disc maps track for track.
- The frame clock counts from game start, not from when the computer booted,
  so the game no longer stutters on a machine that has been on for days.
- The software renderer's stack-pointer trick (drawing pixels with PUSH) is
  replaced by ordinary writes, pixel for pixel the same.
- Files are found through search folders, so replacement files can sit in
  front of the original install, or stand in for it.
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

## Kept as the original, on purpose

Known quirks the rebuild deliberately keeps:

- Wading in any depth of water gives the full x8 cooling, and only the left
  foot counts.
- Elementals have one point of armor everywhere and die to a single hit.
- Missiles that miss vanish at the end of their range without exploding.
- Radio chatter plays the newest line first.
- Every mech's engine tops out at 400.

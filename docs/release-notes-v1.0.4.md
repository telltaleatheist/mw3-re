**Linux launcher text fixed, difficulty does what it says again, rain stays outside, and a long list of smaller fixes.** Install it over any earlier 1.0.x: your pilots, saves and settings are kept.

## Worth knowing first

- **Low and Medium get noticeably easier.** Every release so far gave your side the High damage figure on every difficulty. The game's own table now applies: you take 40% on Low, 70% on Medium and 100% on High. Low also loses a heat haze the original doesn't have there.
- **The launcher has a new font on every system.** It now brings its own, Saira Condensed, where it used to borrow one from your system.

## Linux

- **The launcher's buttons have their text.** It looked for a font at fixed places and drew empty buttons when none was there. We reproduced that with every font hidden, and the package's own font fixes it. That was on Ubuntu, not Arch.
- **Menus with no fonts.** An install without Arial, Impact and Lucida Console now shows its text in free stand-ins the package brings, at the original's line heights. Windows and Mac still try your system's copies first.
- **Stuck at "Downloading Mission Parameters": not fixed yet.** That label stays up for the whole mission load, and we couldn't make it stick here, with or without fonts. The log now says which load step went quiet and for how long, so if it happens to you, `mw3.log` is what we need.

## Fixed in play

- **Rain and snow stop under roofs and in tunnels.** The check was ported but never connected.
- **Artemis in a side torso works.** The game looked for it in the wrong place, so launchers there never got its better tracking. Stock loadouts with Artemis in a side torso now do.
- **A locked Artemis missile no longer crashes the game.** One flying under 10 m over the ground with a clear line did.
- **A possible crash after a blown-off limb,** when a mission restarted, is gone.
- **The same acceleration above 83 fps.** Holding a throttle key above about 83 fps skipped the mech's slow build-up of speed, so it pulled away nearly 2 seconds sooner than at 30 or 60 fps.
- **Walking into a building does its full damage at any frame rate.** An earlier fix scaled the 20 damage of each touch by the frame time, so a higher frame rate meant less damage (about a fifth at 144 fps). Each touch does 20 again, as in the original.
- **Where a foot reads the ground.** The original never filled in the setting that decides when a foot uses the ground under its middle. Our fix guessed at it; it now uses the value the original team's own Pirate's Moon sets. This changes slightly how feet sit on slopes and when a foot counts as in water. This is part of bug fixes.
- **The target keys tell the HUD again.** Choosing a target with E, W or their Ctrl and Shift forms now does what Z and Q already did: the target triangle pulses, the target's mech sound plays, and a missile lock starts over on the new target.
- **A stick's throttle lever stays the right way round.** Tapping throttle-down with the lever below stop, or pressing Toggle throttle direction (Backspace or stick button 10) with it above stop, could leave the lever backwards until the mission ended. That toggle no longer affects a lever. If yours runs backwards from the start, use the Invert box (Options, Controls, Assign Axis). We tested this with a stand-in stick, not real hardware, and we couldn't make the Invert box fail. If it does nothing for you, please tell us your stick's model and send `mw3.log`.
- **Op 2 Mission 2's gas kills.** After the Air Refinery goes down, standing in its gas now destroys you, or Dominic, as the original does.

## Menus and files

- **The debriefing's scores.** A campaign debriefing now lists the mission's objectives, each worth 2500 when done, and counts them in the totals and the rank. Instant Action's Battle Waves and Progression now score 1000 a wave plus a bonus for how far you got, as the original does. Scores you already have stay as saved; new missions add to them.
- **The debrief at 16:9.** The three columns now sit together in the middle, and the extra width stays open at the sides.
- **Ctrl+I in the mech lab keeps your loadout.** Importing a loadout read the file, added nothing and renamed the file `.done`, so it seemed to vanish. If you lost one, take `.done` off its name in the `import` folder and press Ctrl+I again.
- **Accented and non-English letters on Windows.** A folder like `D:\Café`, or a Windows user name with an accent, stopped the game from starting. Windows 10 version 1903 or later now handles them, and pilots saved under a garbled name get their right name back.
- **`--check-mod` passes a mod with a new world.** It refused every good `gamez.zbd`, `anim.zbd` and `texture*.zbd` as "not an archive". Only the check was wrong. It also no longer stops at a long folder path.

Everything the rebuild changes from the original is in `CHANGES.md`.

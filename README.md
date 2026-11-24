# MechWarrior 3 Rebuild

> **This is the download page; the code now lives in its own repos.** Get the
> game from [Releases](../../releases) as always, and report bugs in
> [Issues](../../issues). On 2026-11-21 the project was split into one repo
> per game: see [Where everything lives](#where-everything-lives) below.

![The BattleTech launcher](docs/screenshots/launcher.png)

An unofficial fan rebuild of **MechWarrior 3** (Zipper Interactive and
MicroProse, 1999) for today's computers: Windows 10 and 11, macOS and Linux,
with no Wine, DirectX wrappers, DDrawCompat or dgVoodoo.

It is not a remake. The original `Mech3.exe` (patch 1.2) was taken apart and
its code rewritten function by function in C++, most of it checked against
the original running in an emulator. The game plays the way it always did,
minus the bugs and the problems it has on modern machines. It is a personal
learning project: free, not sold, and not affiliated with or endorsed by
Microsoft or MicroProse.

The download is one launcher, named **BattleTech**, which installs each game
for you:

- **MechWarrior 3**, rebuilt as above;
- **Pirate's Moon**, the expansion, rebuilt the same way from its own exe;
- **MechWarrior 1 Remake** (formerly MW1: Critical Hit), a fan remake of MechWarrior (1989)'s mercenary
  campaign on MechWarrior 3's combat, worlds and 'Mechs;
- **The Crescent Hawk's Inception** (1988), rebuilt; plays from your own
  DOS 1.03 or Amiga 2.3 copy.

Each game also starts on its own and comes as a separate download too.

**No game files are included.** You need your own copy of each game you play.

> Found a bug? Please [report it](#reporting-a-bug).

## Where everything lives

On 2026-11-21 the project was split. Until then this one repo held
everything: MechWarrior 3, the launcher, and Pirate's Moon and MechWarrior 1
Remake on branches of their own. Now each game has its own repo, and this
one is the front door.

| What | Where | Releases |
|---|---|---|
| **The download** (the launcher, which installs the games), bug reports, and this page | **mw3-re** (here) | [Releases](../../releases): the launcher for Windows, Mac and Linux, and MechWarrior 3, Pirate's Moon and MechWarrior 1 Remake alone |
| MechWarrior 3, the launcher, and the packaging that builds the download | [mech3-re](https://github.com/telltaleatheist/mech3-re) | [MechWarrior 3 alone](https://github.com/telltaleatheist/mech3-re/releases) |
| Pirate's Moon | [mech3-pm-re](https://github.com/telltaleatheist/mech3-pm-re) | [Pirate's Moon alone](https://github.com/telltaleatheist/mech3-pm-re/releases) |
| MechWarrior 1 Remake | [mech1-remake-re](https://github.com/telltaleatheist/mech1-remake-re) | [its releases](https://github.com/telltaleatheist/mech1-remake-re/releases) |
| The shared "find your game files" setup | [re-setup-kit](https://github.com/telltaleatheist/re-setup-kit) | (a library the games build in) |
| MechWarrior (1989) rebuilt as C++ | [mech1-re](https://github.com/telltaleatheist/mech1-re) | not released yet (a first preview once it runs from its own files) |
| BattleTech: The Crescent Hawk's Inception rebuilt | [inception-re](https://github.com/telltaleatheist/inception-re) | [its releases](https://github.com/telltaleatheist/inception-re/releases) |

**Why it was split:**

- **Each game stands on its own.** Every game has its own repo, its own
  version numbers and its own releases, and plays without the launcher.
  Pirate's Moon and MechWarrior 1 Remake used to be long-lived branches
  here; as repos of their own, their history, changes and downloads are
  easy to find.
- **MechWarrior 3 stays as it shipped.** Pirate's Moon is a complete copy of
  the game with the expansion's changes in place, and those changes never
  flow back. Fixes flow down only: mech3-re into mech3-pm-re into
  mech1-remake-re.
- **The family is growing.** MechWarrior 1 Remake, a MechWarrior (1989)
  rebuild and an Inception rebuild share the setup kit and the `-re` name
  ("reverse engineered"). The names now say what each one is ("mw3" was
  often read as Modern Warfare 3).

**Why this repo stays:** it's the address people know. Articles and links
point here, and every copy of the game already installed checks this page's
Releases for updates. So new releases keep coming out here, and the launcher
keeps finding them. Its code is frozen as it was at the split, so old clones
and links keep working. The game repos carry this repo's full history, and
pull requests opened here are merged there.

**If you want to:**

- **Play:** download from [Releases](../../releases). One package holds the
  launcher and the games; you need your own copies of the games.
- **Report a bug** in any of the games: open an issue
  [here](../../issues) (more [below](#reporting-a-bug)).
- **Change the code:** fork the game's own repo and open your pull request
  there.
- **Understand how the repos fit together**, how the launcher brings in each
  game and how updates and releases work: [DEPLOY.md](DEPLOY.md).

## What's new

Every release's full notes are on the [Releases](../../releases) page, and
[CHANGES.md](https://github.com/telltaleatheist/mech3-re/blob/main/CHANGES.md) lists everything the rebuild changes from the
original.

### Coming in the next release

Nothing yet.

**1.2.3 (2026-11-21)** (the full notes are in
[docs/release-notes-1.2.3.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/release-notes-1.2.3.md)): setting up your
games is simpler, and always your choice.

- **Your files are copied in**, so the copy, zip or disc image you set a game
  up from can be deleted afterwards; a game folder put next to the program is
  found whatever it is called.
- **Always your choice**: what a game finds is only offered ("USE THESE GAME
  FILES?"), and holding Shift at a game's start lets you choose again.
- Pirate's Moon and MechWarrior (1989) are set up the same way.
- MechWarrior 1 Remake 1.0.6: its new name on the window and save folder,
  reach contracts that can be reached, the arctic defence contract back.

**1.2.2 (2026-11-21)** (the full notes are in
[docs/release-notes-1.2.2.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/release-notes-1.2.2.md)): fixes from your
bug reports, for all the games.

- **Raw mouse**: the mouse moves exactly as far as you move it, without the
  system's acceleration (RAW MOUSE on the Upgrades page); MOUSE SENSITIVITY
  goes to 5.0.
- New key bindings work at once, rumble pads keep their bindings, free look
  on the middle mouse button, no Linux crash at a mission's start.
- MechWarrior 1 Remake 1.0.5: varied 'Mech prices and out-of-bounds lines on
  every map.

**1.2.1 (2026-11-21)** (the full notes are in
[docs/release-notes-1.2.1.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/release-notes-1.2.1.md)): a fix release
for MechWarrior 1 Remake, now 1.0.4.

- **Every contract can be won**: Desolation Ridge's "destroy the pirate base"
  put its target beside your start, so the mission never ended; every
  contract map is now checked.
- No freeze switching the damage display (D) at high resolution; the arctic
  maps cool your 'Mech faster; smoother HUD shapes.

**1.2.0 (2026-11-20)** (the full notes are in
[docs/release-notes-1.2.0.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/release-notes-1.2.0.md)):

- **Ultrawide screens** (21:9, 32:9) filled without stretching, with
  2560x1080, 3440x1440, 3840x1080 and 5120x1440 in the graphics options.
- **Joysticks**: buttons 11 to 15 can be bound, and a stick plugged in while
  the game runs works without a restart.
- **A new launcher**: a card for each game, named BattleTech, in a window (on
  a Mac its file pickers now open in a second, not 7 to 12); MW1: Critical
  Hit 1.0.3.

**1.1.3 (2026-11-20)** (the full notes are in
[docs/release-notes-1.1.3.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/release-notes-1.1.3.md)):

- **Three games on one launcher**: MechWarrior 1 Remake 1.0.1, a fan remake of
  MechWarrior (1989) on the rebuild's engine, joins the combined package. The
  launcher finds your MechWarrior (1989), or takes its folder or the .zip it
  came in.
- **Music that should repeat now repeats**, and a disc plays on to the next
  track.
- Fixes: a machine gun's sound stuck on after firing (a small fix), memory
  growing mission after mission, Pirate's Moon's lance orders going to the
  first lancemate.

**1.1.2 (2026-11-15)** (the full notes are in
[docs/release-notes-1.1.2.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/release-notes-1.1.2.md)):

- **Both games are ported whole**, the developers' tools and unused code
  included, kept as the original has them.
- **Pirate's Moon behaves like its own** in its menus, briefings, Instant
  Action line-ups, mission look, sensors, field bases and multiplayer; Ctrl+P
  saves a screenshot.
- **Distant ground and walls no longer shimmer** (mipmaps), in both games.
- Fixes: the pause's Escape silencing the mission, a missile locked on a mech
  that left the game, damaged model files, the mod check with a world loaded.

**1.1.1 (2026-11-13)** (the full notes are in
[docs/release-notes-1.1.1.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/release-notes-1.1.1.md)):

- **Pirate's Moon plays by its own rules** where 1.1.0 still used
  MechWarrior 3's: multiplayer salvage, field base repairs, which weapons can
  bear, weapon lights, group fire, and the AI's firing and lance orders.
- **MechWarrior 3:** the radar's view wedge on the mission map, the ER PPC's
  HUD static, equipment and engine-hit callouts, and the zoom window's range
  when aimed at the sky.
- Two crash fixes: a laser turret firing as a mission ends, and a lance order
  on something with no vehicle behind it (Pirate's Moon).

**1.1.0 (2026-11-12)** (the full notes are in
[docs/release-notes-1.1.0.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/release-notes-1.1.0.md)):

- **One download holds both games**, MechWarrior 3 and Pirate's Moon, with a
  launcher to pick between them.
- **Mods are played by campaign** from a new MODS screen, and community mods
  are imported straight from their installers, without running them.
- **Instant Action and multiplayer with mods**: each has its own MODS tab;
  in multiplayer the host decides and everyone needs the same mods.
- **An UPGRADES page**: the one bug-fixes switch becomes five (water
  movement, MFB regroup, improved AI, Artemis tracking, small fixes).
- Fixes from the playtests: the Windows cursor stuck at the centre, a slow
  mouse not moving the aim, the game's boxes in its own look, and more.

<details>
<summary>1.0.1 to 1.0.5 (2026-11-03 to 11-07)</summary>

**1.0.5 (2026-11-07)**

- **No crash ending a mission with a laser firing.** If a beam was still firing when
  a mission ended, the cleanup treated it as a shot and could crash, for example on
  REPLAY at the debriefing.
- **A loadout past 16 weapons can't damage the game.** The game took a 17th weapon
  unchecked and overwrote what came after the list. A mod's mech could do that,
  and so could a modified player's loadout in multiplayer. The extra weapon is now
  refused and named in the log.
- **Walls and footing match the original more closely.** Mech collision and the
  feet's ground probes now do their sums at the precision the original game really
  runs at. In tests they agree with it exactly; in play the difference is rare and
  small.

**1.0.4 (2026-11-07)**

- **The launcher's buttons have their text on Linux.** It looked for a font at
  fixed places and drew empty buttons if none was there. It now brings its own,
  and on all systems the launcher wears that font.
- **No game fonts is no longer blank menus.** An install without Arial, Impact
  and Lucida Console uses your system's on Windows and Mac. With none there, or on
  Linux, it shows its text in free stand-ins the package brings, at the original's
  line heights.
- **Rain and snow stop under roofs and in tunnels.** The check was ported but
  never connected, so they kept falling indoors.
- **`--check-mod` passes a mod with a new world.** It refused every good
  `gamez.zbd`, `anim.zbd` and `texture*.zbd` as "not an archive". Only the check
  was wrong. It also no longer stops at a long folder path.
- **Artemis in a side torso works.** The game looked for it in the wrong
  place, so launchers there never got its better tracking. Stock loadouts with
  Artemis in a side torso now do.
- **Difficulty changes how much damage you take again.** Every release so far
  gave your side the High figure on every difficulty. The game's own table now
  applies: 40% on Low, 70% on Medium, 100% on High. **Low and Medium get
  noticeably easier.** Low also loses a heat haze the original doesn't have
  there.
- **A locked Artemis missile no longer crashes the game.** One flying under 10 m
  over the ground with a clear line did.
- **Ctrl+I in the mech lab keeps your loadout.** Importing a loadout read the
  file, added nothing and renamed the file `.done`, so it seemed to vanish. It
  now adds the loadout. If you lost one, take `.done` off its name in the
  `import` folder and press Ctrl+I again.
- **Accented and non-English letters on Windows.** A folder like `D:\Café`, or
  a Windows user name with an accent, stopped the game from starting. Windows
  10 version 1903 or later now handles them, and pilots saved under a garbled
  name get their right name back.
- **The debrief at 16:9.** The screen's three columns now sit together in the
  middle, and the extra width stays open at the sides.
- **A possible crash after a blown-off limb,** when a mission restarted, is
  gone.
- **The same acceleration above 83 fps.** Holding a throttle key above about
  83 fps skipped the mech's slow build-up of speed, so it pulled away nearly
  2 seconds sooner than at 30 or 60 fps. It now builds up the same at any
  frame rate.
- **Walking into a building does its full damage at any frame rate.** An
  earlier fix scaled the 20 damage of each touch by the frame time, so the
  higher your frame rate the less a building took (about a fifth at 144 fps).
  Each touch does 20 again, as in the original.
- **Where a foot reads the ground.** The original never filled in the setting
  that decides when a foot uses the ground under its middle. Our fix guessed
  at it; it now uses the value the original team's own Pirate's Moon sets. This
  changes slightly how feet sit on slopes and when a foot counts as in water.
  This is part of bug fixes.
- **The target keys tell the HUD again.** Choosing a target with E, W or their
  Ctrl and Shift forms now does what Z and Q already did: the target triangle
  pulses, the target's mech sound plays, and a missile lock starts over on the new
  target instead of carrying across.
- **A stick's throttle lever stays the right way round.** Tapping throttle-down
  with the lever below stop, or pressing Toggle throttle direction (Backspace or
  stick button 10) with it above stop, could leave the lever backwards until the
  mission ended. That toggle no longer affects a lever. If yours runs backwards
  from the start, use the Invert box (Options, Controls, Assign Axis).
- **Op 2 Mission 2's gas kills.** After the Air Refinery goes down, standing in its
  gas now destroys you, or Dominic, as the original does; before, nothing happened.
- **The debriefing's scores.** A campaign debriefing now lists the mission's
  objectives, each worth 2500 when done, and counts them in the mission and
  campaign totals and the rank; before, it listed none and scored none. Instant
  Action's Battle Waves and Progression now score 1000 a wave plus a bonus for
  how far you got, as the original does, in place of a flat 1000, so a round that
  clears no wave scores nothing but its kills. Scores you already have stay as
  saved; new missions add to them.
- **A mission load that stops says where.** The log now names the load step that
  has gone quiet and for how long.

**1.0.3 (2026-11-05)**

- **Your data folder can live on another drive:** GAME FILES, MY DATA FOLDER in the
  launcher copies it, checks the copy, then switches over, and removes the old
  copy only if you say so. If the drive is missing at start the game says so
  instead of starting afresh.
- **Linux:** the game no longer closes at the end of the intro movies.
- **Linux:** an install in any letter case now works (all lower case, all upper
  case, or mixed).
- **A softer message for mods:** when files match neither 1.0 nor 1.2, it now says
  data mods are read, though they may not play right, and that a mod which
  patches the old Mech3.exe won't apply. It never stopped the game.
- **Crashes say why:** a crash or a failed start now leaves a line in the log
  and shows a message box, on every platform.
- **Windows:** the installer asks for a folder every time, so you can install
  somewhere other than C.
- **Vehicles on fast machines:** small bumps throw them much less. On rough
  ground they still tip more than at 30 fps, and they still jump off steep
  banks at full speed, as in the original.
- **A band under the sky:** where nothing is drawn between the far ground and
  the sky, the game now shows the fog's colour.
- Smaller fixes: a memory leak in the zoom views, a light's position, and the
  pitch of moving sounds.

**1.0.2 (2026-11-04)**

- **Lancemates walk round things.** When a straight walk fails, they find a
  way round walls, up ramps and across rivers. Part of bug fixes; not for
  enemies or in multiplayer.
- **Lights made during play work.** Jump jets and gunfire light your mech.
- **A mech no longer walks by itself** when saved stick controls are used with
  no stick attached.

**1.0.1 (2026-11-03)**

- **Op 1, Mission 2's missile platforms** no longer vanish at high frame rates,
  which could make the mission impossible to win.
- **Custom key bindings** work on the Controls screen.
- **Flight sticks:** twist and throttle reach the right axes, and the throttle
  lever reaches reverse.
- **A crash late in a fight** is fixed, along with the end credits on wide
  screens and the mech lab's "Overwrite?" box.

</details>

## What you need

- **Your own MechWarrior 3**, any of these:
  - an installed copy, version 1.2 or 1.0 (the rebuild makes the 1.2
    patch's changes itself as 1.0's data loads, so it plays as 1.2; your
    files are not changed);
  - the original disc;
  - a disc image: an `.iso`, or a `.cue` with its `.bin` files (a `.cue`
    also gives you the music).
- **A computer** with a graphics card that does Direct3D 12 (Windows), Metal
  (Mac) or Vulkan (Linux):
  - Windows 10 or 11, 64-bit;
  - macOS 12 or later, Apple silicon or Intel;
  - Linux, x86-64, with your distribution's Vulkan driver.

Pirate's Moon, the expansion, is in the same download as its own program, and
needs your own Pirate's Moon the same way (an installed copy, its disc or a
disc image). Each game works without the other.

MechWarrior 1 Remake needs your own MechWarrior (1989), MechWarrior 3 and
Pirate's Moon: it is fought on their worlds with their 'Mechs. The launcher
looks for MechWarrior (1989) in the usual places; if it isn't there, a click
takes its `BTECH.EXE`, its folder or the `.zip` it came in.

## Download

From the [Releases](../../releases) page:

| | File | |
|---|---|---|
| Windows | `MW3-Rebuild-Setup-<version>.exe` | An installer. A `.zip` of the same files is there too. |
| Mac | `MW3-Rebuild-<version>-mac.dmg` | Drag the app to Applications. |
| Linux | `MW3-Rebuild-<version>-x86_64.AppImage` | Make it executable (`chmod +x`) and run it. |

**On a Mac, the first time:** the app is not signed by Apple, so macOS stops
it the first time you open it ("BattleTech Not Opened"; click **Done**,
not Move to Trash).

- **macOS 15 Sequoia and later:** open **System Settings > Privacy &
  Security**, scroll down to the line about BattleTech, click **Open
  Anyway**, then **Open** (and your password if asked). Right-click > Open
  no longer gets past it on these versions.
- **macOS 12 to 14:** right-click (or Control-click) the app, choose
  **Open**, then **Open** again. Open Anyway in Privacy & Security works
  there too.

macOS remembers your choice after that.

**On Windows**, SmartScreen may warn about an unknown publisher: click
**More info**, then **Run anyway**.

## The first start

Start **BattleTech** (the launcher; **MechWarrior 3** in the MechWarrior 3
alone download) and click a game. MechWarrior 3 goes straight into the game
when it finds your install: where the original installer put it, `Program Files`,
`C:\Games`, a Wine, Proton or Lutris copy on Linux and the Mac, or a copy it
installed itself earlier. It says which version it found in its log.

**If it finds none**, choosing MechWarrior 3 opens a screen that finds
your game. From the launcher, once the game is there you are back on the
launcher's games, so you can set up Pirate's Moon too, then click the game
to play; a plain `mw3` start (that screen only) goes straight on into the
game:

- **Find the game** searches this computer and your disc drives first and
  uses what it finds, with nothing to pick. If there is nothing, one
  picker opens: choose a disc image or download (an `.iso`, a `.cue` with
  its `.bin` files, whose CD audio becomes the music, or a `.zip`, `.7z` or
  `.rar` holding any of those, even a zip inside a zip; nothing needs
  unpacking by hand), or `Mech3.exe` (or any file) in an installed copy,
  which means that whole folder.
  It looks inside first and says what it found ("The MechWarrior 3 disc
  version 1.0, with its music (2 CD tracks)"), then copies what the game
  needs into a folder of the rebuild's own, with a progress bar and
  **Cancel**. An installed copy is used where it is. No Windows and no
  original installer needed. Versions 1.0 and 1.2 both work as they are:
  1.0 plays as 1.2. Pirate's Moon is found or installed the same way, from the
  launcher's GAME FILES.

If something is wrong (not enough space, a damaged download, a file that
is not MechWarrior 3) it says so in plain words and changes nothing.

- **The launcher**, installed as "BattleTech" (the Start-menu and desktop
  shortcut on Windows, the Mac's app, the Linux desktop entry; a search for
  "MechWarrior 3" finds it too), is the rebuild's own front screen, in a
  window: a card for each game. It starts
  the game and closes as soon as the game's first frame is on screen (if
  the game fails to start, it stays and says so). **Game files** there
  opens the same find-your-game screen at any time: to use another copy,
  install again from your disc, or add movies and music it lacks. `mw3`
  (`mw3.exe`) also starts the game directly. The arrow keys and a gamepad
  work in the launcher as well as the mouse.
- Before the first game it tells you anything worth knowing about your
  copy: no movies (a CD-type install leaves them on the disc), no music, or
  game files of an unknown version. **Add from my disc** there takes what
  is missing from your disc in a drive, a disc image or the disc's folder.
- **The movies** play from your own copy. Where they cannot be played
  directly, the rebuild converts them once, the first time (about a minute).

Your MechWarrior 3 files are only ever read, never changed.

## Music

The original played its music from the CD's audio tracks, which an
installed copy does not have. Two ways to get it back:

- With a disc image (a `.cue` and its `.bin`): **Add from my disc** on the
  notes before your first game (installing from the `.cue` takes it at once).
- Your own rip: put the tracks in the rebuild's `music` folder (below) as
  `track02.ogg`, `track03.ogg` and so on (or `.flac`, `.wav`, `.mp3`),
  numbered as on the CD.

## Playing

- The game runs fullscreen at your desktop's resolution, widescreen
  included. **Alt+Enter** switches between fullscreen and a window.
- Ultrawide screens (21:9, 32:9) play at their full width: the view widens
  without stretching, and RESOLUTION offers 2560x1080, 3440x1440, 3840x1080
  and 5120x1440 (contributed by roswellien).
- In a window, press and release **Ctrl+Alt** to free the mouse; click in
  the window to take it back. Switching away frees it too, and coming back
  to a mission (Cmd+Tab, Alt+Tab or a click) takes it again at once; that
  click does not fire.
- **Options > Mouse sensitivity** (ours, under Invert Mouse) sets how fast the mouse aims in a mission; it starts at the original's setting. Click the bar where you want it.
- Gamepads work as they did on Windows 98, and force feedback becomes rumble.
- Flight sticks' axes (twist, throttle lever) go where the game expects
  them. With several devices, `joystick=Part of its name` in `mw3.cfg`
  picks one; an odd stick's axes can be set with `joystick_axes=X,Y,Rz,S0`
  (one word a stick axis: X, Y, Z, Rx, Ry, Rz, S0, S1, or - for none).
  `mw3.log` lists every device found and the line in use.
- A stick's throttle lever covers reverse too: full reverse at the bottom,
  stop a little over a third of the way up. If yours runs backwards, untick
  Invert in Options, Controls, Assign Axis on Forward Throttle, and save
  when Accept asks. Toggle throttle direction (Backspace) is for the keys
  and does not flip a lever. `throttle_reverse=0` in `mw3.cfg` gives the original's
  stop at the bottom (reverse is then Backspace).
- On a widescreen every screen uses the whole width: the missions, the
  menus, the mech lab, the briefings and the multiplayer screens. For the
  original's 4:3 menus, put `widemenus=0` in `mw3.cfg` (or start with
  `--no-wide-menus`).
- The movies keep their shape on a wide screen: the intro and the ending,
  whose black bars were part of the picture, fill the screen's width; the
  4:3 movies are centred with black sides.

### Multiplayer

**Multiplayer** on the main menu plays the original's network games over
the internet or a local network, up to 8 players. DirectPlay, which the
original used, is gone from modern systems, so the rebuild has its own
connection (UDP); IPX, modem and serial play are gone with it. Everyone
plays the rebuild, on any mix of Windows, Mac and Linux.

- **On a local network**, games show up on the connection screen by
  themselves as soon as it opens.
- **Joining by address**: type the host's address (`203.0.113.7`, or
  `203.0.113.7:27930` for another port) and press **Find Game** or
  Enter.
- **Hosting**: **Host Setup**, then **Host**. The game opens its UDP port,
  27930, on your router by UPnP when the router allows it, and closes it
  again when you stop. The connection screen and the lobby show the
  address to give your friends (click it to copy) and your LAN address.
  When the internet cannot reach you, the screen says why and what to do:
  - "forward UDP 27930 on your router to host": no router answered, so set
    up the port forward by hand in your router's settings;
  - "Could not open the port": the router refused UPnP; forward the port
    by hand;
  - "Another router stands in front of yours": a second router (often the
    provider's modem) sits between yours and the internet; forward UDP
    27930 on that one too, or use a VPN such as Tailscale;
  - "Your internet provider blocks hosting": your provider shares one
    internet address between many customers (carrier-grade NAT), so no
    one can reach you from outside. Play on a LAN, or over a VPN such as
    Tailscale; joining other people's games still works.
- **Leaving a match**: Escape shows "Are you sure you want to quit?". With
  bug fixes on (below) the match goes on while it is up, so you can still
  move and fire; **Y** leaves
  for the main menu, **N** or Escape closes the box. The others see you
  leave at once. A match also keeps running when its window is in the
  background, so your 'Mech is never a frozen target.
- **Mods**: the multiplayer screen's MODS tab ticks the mods to play with
  (the game starts again with them). The host decides; a player whose mods
  are not the same is turned back and told which mod is missing or
  different. Players on 1.0 and 1.2 game files play together.
- **Pilot names**: two players can't use the same name (the original refuses
  the second). Change NET NAME on the multiplayer screen, or click PILOT
  ENTRY on the pilot screen and type a new pilot's name.

### Bug fixes, and playing exactly like the original

The rebuild fixes the original's bugs where the intent is clear: things that
ran faster on a fast computer, the mobile field bases that would not come
back, lancemates walking into walls, and more. Quirks that players know and
plan around are kept. Every change is listed in plain words in
[CHANGES.md](https://github.com/telltaleatheist/mech3-re/blob/main/CHANGES.md).

The fixes that change play are grouped under five switches on the
**UPGRADES** page in the options (in the menus and in a mission), all on
to start: WATER MOVEMENT, MFB REGROUP, IMPROVED AI, ARTEMIS TRACKING and
SMALL FIXES (the page lists what the small fixes are). Turn one off and that
part plays as in 1999. To play exactly as the original did, turn all five
off, or start with `--original` for one run. In a network game everyone
plays on the host's switches. The changes the game needs to run on a modern
computer, and crash fixes, always stay on.

### Cheat codes

The rebuild adds cheat codes (ours, not in the original) for the campaign,
Instant Action and training; a network game refuses them. In a mission,
press **Shift+Enter** (or **Keypad \*** or **Keypad −**) to open a one-line
box, type a code and press **Enter**; **Esc** closes it. The mission keeps
running while you type. Capitals and extra spaces don't matter. A code
lasts until the mission ends; the ones that switch something on switch it
off when typed again. The message panel says what was turned on or off.

| Code | What it does |
|---|---|
| `black sheep wall` | Every mech and vehicle on the radar, at any range |
| `power overwhelming` | Your mech takes no damage |
| `operation cwal` | Your weapons reload and your mech cools ten times as fast |
| `food for thought` | Maximum salvage: every chance in the salvage goes your way |
| `there is no cow level` | Wins the mission: every enemy mech and vehicle destroyed by you and salvaged whole, every objective done |
| `game over man` | Loses the mission |

A mission won with a code counts as won. In a network game the keypad keys
stay the chat's. More in [docs/cheats.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/cheats.md).

## Mods

Mods are turned on per campaign, inside the game, on the **MODS** screen
(main menu). A fresh start always plays the original game: nothing is modded
until you turn it on.

1. **MODS** on the main menu. The left box lists the campaigns (the game's
   own first), the right box the mods you have.
2. **IMPORT** adds a mod: pick the file you downloaded.
3. Click a mod's **box** to turn it on for the campaign chosen at the left.
4. **PLAY** starts that campaign with its ticked mods. Each campaign keeps
   its own ticks, pilots and saved games, apart from the main menu's.

**MISSIONS** plays any one mission of the chosen campaign at once (nothing
is kept), **INFO** says what a mod changes, and each button says what it
does when the mouse is over it. A mod never touches your install or your
main-menu pilots.

### Importing a community mod (the weapons mods, map packs)

Community mods were made for the original game and mostly come as an
installer program.

1. Download it. **Do not run its installer**: it is made for the original
   game and would overwrite that game's files.
2. In the rebuild: MODS, IMPORT, and pick the downloaded file (the .exe
   installer itself, a .zip/.7z/.rar, or a file inside an unpacked folder).
   The rebuild reads the mod out of it without running it.
3. Click its box for a campaign, then PLAY.

IMPORT may say "N warnings": files in the mod the game cannot use as they
are (often slips that also trouble the original). The rest of the mod is
added; INFO lists each warning and what was done about it.

Pirate's Moon has its own MODS screen. A Pirate's Moon mod imported in
MechWarrior 3 (or the other way round) is handed to the other game, which
adds it at its next start, and a box says so; with the game started without
the launcher, the box says to IMPORT it in the other game. In multiplayer
every player needs the same mods.

Mods live as folders in a `mods` folder inside the settings folder (below),
laid out like the game's own files, each with a short `mod.txt`. `mw3
--mods` lists what it found, and `mw3 --check-mod FOLDER` reads a mod as
the game would and says what it would refuse. How to make one:
[docs/modding.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/modding.md).

To build or change a world, use [zStudio](https://github.com/Vorta/zStudio) by Vorta
(Windows): it unpacks the game's files into the editable source tree Zipper's own team
worked from, and builds them back into game files you can ship as a mod.

The game editor itself (the original's own, opened to players: the AI's paths, the
vehicles' AI, new weapons, saved into your own copy of a campaign) is reached from
MODS: MISSIONS, a mission, PLAY, then EDIT MISSION on the pause menu (or Shift+F12).
It has a guide: [docs/editor-guide.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/editor-guide.md).
In the game every field, button and menu row has a tip, and the menu's HELP row gives a
quick start.

The original game has a tool of its own here: [tools/weapon-patch](https://github.com/telltaleatheist/mech3-re/tree/main/tools/weapon-patch)
adds weapons to the original `Mech3.exe`'s mech lab. It isn't part of the rebuild.

## Where your saves and settings go

| | Folder |
|---|---|
| Windows | `%APPDATA%\MicroProse\MechWarrior 3\` |
| Mac | `~/Library/Application Support/MicroProse/MechWarrior 3/` |
| Linux | `~/.local/share/MicroProse/MechWarrior 3/` |

It holds `mw3.cfg`, your pilots and saves, the settings, your mods, the
converted movies and the music, and `mw3.log`. Pirate's Moon keeps its own
beside it, in `MicroProse/Pirate's Moon`, and MechWarrior 1 Remake in
`MicroProse/MechWarrior 1 Remake` (moved there by itself from the old
`MicroProse/MechWarrior 1 Critical Hit` on its first start). Uninstalling the rebuild keeps
them; deleting one starts that game over. The launcher's GAME FILES, MY DATA
FOLDER moves a game's folder to another drive.

## Known issues

- **macOS: window tools can delay the mouse button by about a second.**
  Tools that filter mouse clicks to snap or move windows (Magnet and
  similar) can hold each mouse-button press back, so in a mission the mouse
  fires only when you let go. The keyboard is not affected. Quit the tool
  while you play, or add BattleTech (MechWarrior 3 in the MechWarrior 3
  alone download) to its excluded apps if it has that setting.

## Reporting a bug

Open an [issue](../../issues) and include:

- what you did and what happened (a screenshot helps);
- the mission, and whether it happens with `--original` too;
- your system (Windows, Mac or Linux, and the version);
- **`mw3.log`** from the folder above. It is written new at every start and
  the one before is kept as `mw3.old.log`, so if you started the game again
  after a crash, send `mw3.old.log`.

## Hidden mechanics

Taking the game apart turned up hundreds of hidden mechanics, quirks and
bugs, collected for players in [docs/discoveries.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/discoveries.md).
A few:

- Lasers never cause critical hits, and criticals only happen once a part's
  armor is down to 15%.
- Ammo criticals don't explode: they just empty a bin. CASE in a side torso
  does nothing at all, and the stock Mad Cat carries both of its CASE there.
- Ultra autocannons never jam; they simply fire twice as fast.
- TAG works, but the missiles home on the spot you marked, not the target,
  and take 7 seconds to arrive.
- Pilot files track hidden Instant Action and multiplayer stats that the
  game never shows.

**For the curious:** start with `--debug-camera` (or put `debugcamera=1`
in `mw3.cfg`) and press **Shift+F12** in a mission to open the game editor
the original carries but never lets you reach: its menu over the mission,
and the view on the invisible "stealth" camera, flown with the movement
keys. Shift+F12 again puts the view back on your 'Mech. It is a developer's
tool, as it was in 1999: expect rough edges.

## Building from source

The code is in the game repos (see [Where everything lives](#where-everything-lives)).
For MechWarrior 3 and the launcher, see [docs/building.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/building.md). How the code is organised (the
shared engine, MechWarrior 3, Pirate's Moon and the launcher):
[docs/games-layout.md](https://github.com/telltaleatheist/mech3-re/blob/main/docs/games-layout.md).

## Credits and licence

- **MechWarrior 3** and **Pirate's Moon** were made by Zipper Interactive
  and published by MicroProse in 1999. **MechWarrior** (1989) was made by
  Dynamix and published by Activision. All the games' content is theirs and
  comes from your own copies.
- The rebuild is under the [MIT licence](LICENSE). The
  original's code was studied with Ghidra and
  [REA](https://github.com/morluto/rea), the data formats cross-checked with
  [mech3ax](https://github.com/TerranMechworks/mech3ax), and the port checked
  against the original in the [Unicorn](https://www.unicorn-engine.org/)
  emulator.
- It runs on [SDL3](https://www.libsdl.org/) (with SDL3_ttf, FreeType and
  SDL3_sound), and converts the movies with a minimal LGPL build of
  [FFmpeg](https://ffmpeg.org/). Each download lists them in
  `THIRD-PARTY.txt`, with their own licence files.
- The MW1 card's PLAY ORIGINAL (MechWarrior as released in 1989) is coming
  later.

MechWarrior and BattleTech are trademarks of their owners. This project is not
affiliated with or endorsed by Microsoft, MicroProse, Zipper Interactive,
Dynamix or Activision.

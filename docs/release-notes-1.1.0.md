# MechWarrior 3 Rebuild 1.1.0

**One download now holds both games, MechWarrior 3 and Pirate's Moon, with a launcher to pick between them.** Mods are played by campaign from a new MODS screen, community mods are imported straight from their installers without running them, and multiplayer works with mods. Install it over any 1.0.x: your pilots, saves, settings and mods are kept.

## Downloads

The combined package is the one to get. It holds the launcher, MechWarrior 3 and Pirate's Moon, and it's the one the launcher's UPDATES button looks for. Each game alone is there too.

| | Both games and the launcher | MechWarrior 3 alone | Pirate's Moon alone |
|---|---|---|---|
| Windows | `MW3-Rebuild-Setup-v1.1.0.exe` (or `MW3-Rebuild-v1.1.0-win64.zip`) | `MW3-alone-Setup-v1.1.0.exe` (or `MW3-alone-v1.1.0-win64.zip`) | `PM-Rebuild-Setup-pm-v1.1.0.exe` (or `mw3pm-pm-v1.1.0-win64.zip`) |
| Mac | `MW3-Rebuild-v1.1.0-mac.dmg` | `MW3-alone-v1.1.0-mac.dmg` | `PM-Rebuild-pm-v1.1.0-mac.dmg` |
| Linux | `MW3-Rebuild-v1.1.0-x86_64.AppImage` | `MW3-alone-v1.1.0-x86_64.AppImage` | `PM-Rebuild-pm-v1.1.0-x86_64.AppImage` |

- **Windows:** both installers upgrade a 1.0 install in place.
- **Mac:** drag the app to Applications. "MechWarrior 3.app" replaces 1.0's; Pirate's Moon has its own app. The first time, macOS stops it because it isn't signed: System Settings > Privacy & Security > Open Anyway (the README has the steps).
- **Linux:** make the AppImage executable and run it.

## Before you start

- **Bring your own games.** None of the original games' files are in these downloads. You need your own MechWarrior 3 (an installed copy, 1.0 or 1.2, the disc, or a disc image) and, for Pirate's Moon, your own Pirate's Moon the same way. Each game works without the other. Your installed games are only ever read, never changed.
- **The launcher sets both games up.** GAME FILES finds or installs either game from what you have. After an install the launcher comes back to its games screen, so you can set up the other game too, then click the one to play.
- **A start is always the original game.** Nothing is modded until you turn it on, on a game's MODS screen.
- **Mods are imported, not run.** A community mod for the original game usually comes as an installer program. Don't run it (it would overwrite the original game's files): pick it with IMPORT on the MODS screen and the game reads the mod out of it.
- **Where your things are.** Each game keeps its settings, pilots, saves, mods and `mw3.log` in its own folder: on Windows `%APPDATA%\MicroProse\MechWarrior 3\` and `%APPDATA%\MicroProse\Pirate's Moon\`, on the Mac under `~/Library/Application Support/MicroProse/`, on Linux under `~/.local/share/MicroProse/`. Uninstalling keeps them, and the launcher's GAME FILES, MY DATA FOLDER can move them to another drive.
- **Leave the main menu alone for a minute** and it plays the original's attract movie, as the 1999 game did. It isn't the game starting again.

## What's new

### Pirate's Moon

**Pirate's Moon is supported, as its own program in the same package** (and alone, above). The launcher finds your copy by itself (its usual folders, and Wine, Proton, Lutris, CrossOver and Whisky copies), or installs it from the disc, a disc image or a zip of one, without running its Windows installer. The disc's CD track becomes the music.

- Its own campaign and its Pirate Missions, Instant Action and multiplayer, with its own settings, pilots, saves and mods, kept apart from MechWarrior 3's.
- The same MODS screen, IMPORT, Instant Action and multiplayer with mods, custom campaigns and game editor for mods as MechWarrior 3 (below).
- Its own UPGRADES page. Its WATER MOVEMENT slows a wading mech gradually as the water deepens; off, it drops to 60% in one step, as the original does.
- **Fixed:** Instant Action going black a few seconds into a mission (the HUD stayed). A mission whose files are missing stops with a box saying what is missing, where it crashed. The cursor stuck in the middle of the screen on Windows, mostly when started from the launcher.

### Mods by campaign: the MODS screen

- **MODS on the main menu** lists campaigns at the left (the game's own first, then every campaign you imported and every copy you made) and your mods at the right, each with a box. Click a mod's box to turn it on for the chosen campaign. Every campaign keeps its own ticks, so a weapons mod can be on for one and off for another.
- **PLAY** starts the game again in that campaign with its mods. A campaign from MODS has its own pilots, saved games and saved mechs. Its pilot screen also lists your main-game pilots: pick one and it is copied in, and your main-game pilot is never changed.
- **MISSIONS** plays any one mission of a campaign at once, nothing kept, and lets you play another straight after.
- **INFO** says in plain sentences what a mod replaces and adds: the mechs, weapons, worlds and campaigns it brings. For the mod's maker it sorts any warnings into what stops part of the mod working, what works here but may crash the original game, and harmless slips, each with what to change. EXPORT (in INFO) packs a mod into one checked `.zip`.
- **The delete tab** says what it acts on: DELETE MOD, DELETE COPY (a campaign you made, with its pilots and saves) or RESET (the game's own campaign's ticks and pilots). It asks first. The game never deletes its own files, your main-game pilots, or a folder you put in the mods folder yourself.
- Every button says what it does when the mouse is over it.

### IMPORT: community mods as they are shipped

- **IMPORT takes the mod's installer program itself** and reads the files out of it without running it, then keeps the ones that differ from your own install as the mod. It also takes a `.zip`, `.7z` or `.rar`, a file inside the mod's folder, or a game folder the mod was installed over.
- **The mod's texts come along.** Briefings, objectives and names that a mod changed in the original's message file are kept with the mod.
- **Two mods that change the same game file can both be on.** IMPORT keeps only the records a mod changes, and the game builds each file from the ticked mods.
- **A mod with a faulty file is still added.** The one file the game can't use is left out, and IMPORT says how many warnings there are; INFO lists them.
- **A Pirate's Moon mod imported in MechWarrior 3 goes to Pirate's Moon** (and the other way round). The game tells the two games' mods apart by their files, hands the mod to the other game, which adds it at its next start, and says so. Started without the launcher, it says where to IMPORT it instead.
- IMPORT also takes a mech loadout exported from the mech lab (`.m3c`), for Ctrl+I in any mech lab.

### Instant Action and multiplayer with mods

- **Instant Action has its own MODS tab.** Tick the mods to play it with and ACCEPT; the game starts again with them. A fresh start always plays the original Instant Action: last time's ticks are kept on the tab, ready to ACCEPT.
- **Multiplayer has a MODS tab too.** The host decides the mods, and everyone who joins needs the same ones. A player who doesn't have them, or has a different copy, is turned back and told which mod. Nothing is downloaded from the host: get the mod, IMPORT it, tick it. Your pilot comes with you through the restart.
- **Players on 1.0 and 1.2 game files play together.** The rebuild plays 1.2 on either, so the version of the original you own never keeps two players apart.
- Joining a host playing the other game (Pirate's Moon from MechWarrior 3, or the other way round) is refused with a message naming that game.

### Making mods

- **Custom campaigns** (`campaign=` in a mod's mod.txt): missions in order on any world, with their own row on MODS. A mod can also set how often a kill's salvage is offered (`salvage_chance=`, `mission_salvage=`).
- **New worlds.** A world the game doesn't have is found at IMPORT (from a zStudio export, say) and played whole: its campaign from MODS, Instant Action on it, and its multiplayer maps in the lobby.
- **New mechs and weapons** from mod.txt (`mech=`, `weapon=`), in Instant Action, the lobby and the mech lab.
- **mech3ax's files read as they are:** worlds, animations (one at a time, so two animation mods can be on together), mech models, motions and reader entries can be unzbd's JSON in the mod, with no repacking.
- **The game editor the original hid**, for mods: in a mission from MODS, the pause menu's EDIT MISSION (or Shift+F12) opens it and PLAY MISSION goes back, without leaving the mission. It edits AI paths, vehicles and new weapons, has a mouse pointer and a tip on every field, and only ever saves as a copy: a campaign of your own holding just what you changed.
- `needs=`, `conflicts=` and `game_version=` in mod.txt: MODS warns when something a mod needs is missing.
- Starter mods (`--make-starter mech`, `weapon`, `translation` or `mission`), a modding reference (`docs/modding-reference.md`) and one page for tools that export for the rebuild (`docs/import-spec.md`).

### The UPGRADES page

The one bug-fixes switch is now five, on a page of their own in the options (in the menus and in a mission), all on to start: **WATER MOVEMENT, MFB REGROUP, IMPROVED AI, ARTEMIS TRACKING and SMALL FIXES**. The page lists what the small fixes are. Turn one off and that part plays as in 1999; `--original` turns all five off for one run. Crash fixes and the fixes the game needs to run today always stay on. In a network game everyone plays on the host's switches. WIDE MENUS is on the same page. A 1.0 `bugfixes=` setting becomes the five switches the first time.

### The launcher

- **One launcher for both games.** Click MECHWARRIOR 3 or PIRATE'S MOON. Each game shows its data's and its program's version, and whether it is "official" or "modified" (only a label: a modified game starts as before).
- **Checks before a game starts**, in plain words (an operation's missions missing, an empty sounds file). It refuses only when a game can't start at all.
- **GAME FILES** finds or installs either game, adds music and sounds from the disc, and moves a game's data folder. One game's files are never touched by the other's.
- **UPDATES** asks GitHub for the newest release. If there is one, it shows the notes and asks before downloading and installing it. Your mods, pilots, saves and settings are kept.
- Mods are managed in each game, on its own MODS screen; a game started from the launcher is always the original.

## Fixed

From Owen's playtests of the 1.1.0 builds:

- **Windows: the cursor no longer sticks in the middle of the screen** at a start (mostly Pirate's Moon from the launcher). The game takes the mouse only once its window really is in front.
- **A slow mouse moves the reticle.** With MOUSE SENSITIVITY below its middle, a slow mouse could cross the pad with the aim not moving, then jump.
- **MOUSE SENSITIVITY's bar moves where you click it**, and a click on its right end no longer presses ACCEPT and leaves the screen.
- **The game's boxes are the game's own.** Once the main menu is up, the boxes the original showed as Windows message boxes, the first start's notes and our own questions use the game's dialog, in its look. Every box's words also go into `mw3.log`.
- **Wide menus:** Instant Action's boxes stay in place after its MODS tab, and the menus are right with the original's FastMenu option (`-seasick`). The UPGRADES switches' right arrows sit in their boxes.
- **A new resolution reshapes the menus at once.** Picking one in GRAPHICS & AUDIO (4:3 to 16:9 or back), or WIDE MENUS, no longer waits for the next start.
- The launcher's notes screen fits its text, and it says when the system's file picker is opening.
- A mission started from MISSIONS can be left from its debriefing.
- **After installing a game, the launcher waits on its games screen** instead of starting the game, so you can set up the other game first.
- **A mod for the other game goes to that game.** A Pirate's Moon mod imported in MechWarrior 3 (or the other way round) is recognised by its files and handed to the right game, which adds it at its next start. A box says so.
- **A mod that renames a mech's main loadout no longer drops that mech** from a campaign's start (seen with a Pirate's Moon mod: only the Puma was left). The nearest loadout is given, and the log says what the mod's maker could change.
- **Each world's sun takes its script's settings**, as in the original (the script's light commands did nothing in the rebuild). Nothing you can see changes in the missions checked.

And:

- **Typing in the cheat box or the chat no longer moves your view.**
- **A destroyed weapon doesn't stay selected.** The selection moves to the next weapon that still works.
- **Lots of lights at once no longer corrupt the shadows** (more than 64, from explosions and impacts or heavy weapon mods).
- **The mouse speed matches your desktop at any screen size.** Fullscreen on a 4K screen made it three to four times too fast.
- **A mission whose files are missing no longer crashes.** The load stops and a box says what is missing.
- **Saving works where Windows refuses its own temporary file** (a very long temp folder path).
- **Slips in the game's own data are put right.** The Sunder's Alternate B carries all its LRM 20 missiles, the Strider has its recoil, the Small Pulse Laser has a muzzle flash, the Firefly's fifth camo is named SLATE, and all of mission 1's ocean moves.
- **Enemy and lancemate mechs behave exactly as in the original.** How they steer, pick targets and aim, and where feet, shots and sight lines meet the ground and walls, now come out the same as the original's sums, not just close to them.
- A mission script that changes an object it didn't find now does nothing, where the original could reach a freed one after a second mission.
- **Linux: the movies are converted again.** The 1.0.5 AppImage went out without its movie importer.

Everything the rebuild changes from the original is in `CHANGES.md`.

## Still known

- **macOS: window tools can delay the mouse button by about a second** (Magnet and similar). Quit the tool while you play, or exclude MechWarrior 3 in it.
- In multiplayer two players can't use the same pilot name: the original refuses the second ("Your name is already in use"). Click PILOT ENTRY on the pilot screen and type another name, or change NET NAME on the multiplayer screen.
- **Jump jets lighting up water (Op 3, Mission 5):** one report, not reproduced. A screenshot and `mw3.log` help.
- **Linux: stuck at "Downloading Mission Parameters":** one report, not reproduced. `mw3.log` from such a run names the loading step that went quiet.
- Mech speed in shallow water and on hills is the original's.

## Reporting bugs

Open an issue on the project's GitHub page and attach `mw3.log` from the game's folder (above; each game has its own). If you started the game again since, send `mw3.old.log`.

## Checksums

Each package's SHA256 is in `SHA256SUMS` on the release, which the launcher's UPDATES also checks.

```
(filled in from SHA256SUMS when the packages are built)
```

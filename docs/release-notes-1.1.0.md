# MechWarrior 3 Rebuild 1.1.0

<!-- DRAFT (dev 1, 2026-11-10) for the lead's review and Owen's go: not tagged, not published.
     The Pirate's Moon version (pm-v1.0.2 below) and the checksums are filled in when the packages are built. -->

**One download now holds both games: MechWarrior 3, Pirate's Moon, and a launcher to pick between them.** Mods get a whole set of tools: a MODS screen in the game, custom campaigns, new mechs and weapons, and mods made from mech3ax's files without repacking. Install it over any 1.0.x: your pilots, saves, settings and mods are kept.

## Downloads

The combined package is the one to get. It holds the launcher, MechWarrior 3 and Pirate's Moon, and it's the one the launcher's UPDATES button looks for. Each game alone is there too.

| | Both games and the launcher | MechWarrior 3 alone | Pirate's Moon alone |
|---|---|---|---|
| Windows | `MW3-Rebuild-Setup-v1.1.0.exe` (or `MW3-Rebuild-v1.1.0-win64.zip`) | `MW3-alone-Setup-v1.1.0.exe` (or `MW3-alone-v1.1.0-win64.zip`) | `PM-Rebuild-Setup-pm-v1.0.2.exe` (or `mw3pm-pm-v1.0.2-win64.zip`) |
| Mac | `MW3-Rebuild-v1.1.0-mac.dmg` | `MW3-alone-v1.1.0-mac.dmg` | `PM-Rebuild-pm-v1.0.2-mac.dmg` |
| Linux | `MW3-Rebuild-v1.1.0-x86_64.AppImage` | `MW3-alone-v1.1.0-x86_64.AppImage` | `PM-Rebuild-pm-v1.0.2-x86_64.AppImage` |

- **Windows:** both installers upgrade a 1.0 install in place.
- **Mac:** drag the app to Applications. "MechWarrior 3.app" replaces 1.0's; Pirate's Moon has its own app.
- **Linux:** make the AppImage executable and run it.

You need your own copy of each game you play (an installed copy, the disc, or a disc image). None of the original game's files are in these downloads.

## MechWarrior 3

### Fixed

- **Typing in the cheat box or the chat no longer moves your view.** A key typed into a box could still count for the game after Enter, so L (hold to look) could leave the mouse turning the torso.
- **A destroyed weapon doesn't stay selected.** When your selected weapon is destroyed, the selection now moves to the next weapon that still works, never to one that's already gone.
- **Lots of lights at once no longer corrupt the shadows.** Past 64 lights (explosions and impacts, heavy weapon mods), the original wrote into the shadows' memory. Those lights are now handled properly.
- **The mouse speed matches your desktop at any screen size.** Fullscreen on a 4K screen made the mouse three to four times too fast, and a bigger window made it slower. It now moves as the original does on Windows 11.
- **A mission whose files are missing no longer crashes.** If an operation's world or the mission scripts are gone, the load stops and a box says what is missing.
- **Saving works where Windows refuses its own temporary file** (a very long temp folder path).
- **Slips in the game's own data are put right.** The Sunder's Alternate B loadout carries all its LRM 20 missiles (240 of 360 were left unloaded), the Strider has its recoil, the Small Pulse Laser has a muzzle flash, the Firefly's fifth camo is named SLATE, and all of mission 1's ocean moves.
- **Enemy and lancemate mechs behave exactly as in the original.** How they steer, pick targets and aim, and where feet, shots and sight lines meet the ground and walls, now come out the same as the original's sums, not just close to them.
- **A new resolution reshapes the menus at once.** Picking one in GRAPHICS & AUDIO (4:3 to 16:9 or back) no longer waits for the next start.
- **Linux: the movies are converted again.** The 1.0.5 AppImage went out without its movie importer; this one has it.

### New

- **MOUSE SENSITIVITY** on the Game Options screen, under Invert Mouse (0.5 to 3.0, starting at the original's 1.3).
- **An UPGRADES page** in the options, in the menus and in a mission. The one bug-fixes switch is now five, all on to start: WATER MOVEMENT, MFB REGROUP, IMPROVED AI, ARTEMIS TRACKING and SMALL FIXES. Turn one off and that part plays as in 1999. Crash fixes always stay on. In a network game the host's switches apply to everyone. WIDE MENUS is on the same page.
- **Joining a host playing the other game** (Pirate's Moon from MechWarrior 3, or the other way round) is refused with a message naming that game.

### Mods

- **The main menu always plays the original game.** Starting the game applies no mod, so CAMPAIGN, INSTANT ACTION and multiplayer on the main menu use the game's own files. Nobody can lose their campaign to a mod or need to restore game files.
- **Mods are played by campaign, from MODS.** The MODS screen lists campaigns at the left: the game's own first, then every campaign you imported and every copy you made. Choose one and its mods are at the right, each with a box: click the box to turn that mod on for that campaign. Every campaign keeps its own ticks, so a weapons mod can be on for one and off for another. PLAY closes the game and starts it again in that campaign with its mods. Every button says what it does when the mouse is over it.
- **Each campaign from MODS has its own pilots, saved games and saved mechs.** Its pilot screen also lists your main-game pilots: pick one and it is copied in at that moment, and your main-game pilot is never changed. In the game's own campaign the copy carries on from where you are; in a different campaign it starts at that campaign's first mission.
- **Instant Action has its own MODS button.** Tick the mods to play it with and ACCEPT; the ticks are remembered, the screen says what is on, and ALL OFF goes back to the original.
- **Multiplayer with mods.** The multiplayer screen has a MODS button too. The host picks the mods; everyone who joins needs the same ones, and a player who doesn't is turned back and told which mod is missing or different. Nothing is downloaded from the host: get the mod, IMPORT it, tick it.
- **Instant Action has its own MODS button.** A fresh start always plays the original Instant Action. Its MODS screen keeps last time's ticks ticked: ACCEPT starts the game again with them, the screen says what is on, and ALL OFF goes back to the original.
- **A mod made for the original game works as a tick.** One that is laid over the game's files (missions, weapons and Instant Action scenarios in one) can be ticked for the campaign, for a copy, or for Instant Action. A mod that ships as an installer is imported from the installer itself: IMPORT reads the files out of it without running it, and what differs from your own install becomes the mod. A game folder the mod was installed over works the same way.
- **Custom campaigns.** A mod can bring its own campaign (`campaign=` in its mod.txt): missions in order on any world. It gets its own row in CAMPAIGNS.
- **New mechs and weapons from mod.txt** (`mech=`, `weapon=`). They come after the game's own, appear in Instant Action, the lobby and the mech lab, and count toward multiplayer's check that everyone has the same mods. A pilot that uses one remembers it, so reordering mods doesn't mix it up.
- **mech3ax's files read as they are.** Worlds, animations, mech models, motions and reader entries can be unzbd's JSON in the mod, so there's no repacking. A loose `.zrd` works too. Anything that doesn't read is named with its line in the log.
- **Animations one at a time.** A single animation file replaces that animation or adds a new one, so two animation mods can be on together.
- **IMPORT and EXPORT.** IMPORT takes a mod's .zip, .7z or .rar, its installer program, or a file inside its folder, whatever its layout, and brings the mod's changed texts (briefings, objectives, names) with it. A mod with one faulty file is not refused: that file is left out, the rest is added, and IMPORT says how many warnings there are. EXPORT (in INFO) packs a mod into one .zip with checksums and checks it first.
- **INFO.** It says in plain sentences what a mod replaces and adds, and the mechs, weapons, worlds and campaigns it brings; for a campaign, what it is and which mods are on for it. For the mod's maker it sorts the warnings into three groups: what stops part of the mod working, what works here but may crash the original game, and harmless slips, each with what to change. It is also saved as a text file (`mod-reports` in the game's data folder).
- **MISSIONS.** Choose a campaign, MISSIONS, pick a mission and PLAY: the game starts again in that campaign and goes straight into the mission, with nothing kept. Afterwards the list lets you play another at once, with edits to a mod's weapons and mechs read again, no restart.
- **A mod can say what it needs** (`needs=`, `conflicts=`, `game_version=`). MODS warns when something is missing; the mod is still ticked.
- **The game editor the original hid**, for mods. In a mission started from MISSIONS, or in a campaign played from MODS, the pause menu has one switch (EDIT MISSION, PLAY MISSION) and Shift+F12 does the same: you go between playing and editing without leaving the mission, and the editor has a mouse pointer. It edits AI nets, vehicles and new weapons, and it only ever saves as a copy: the first save asks for a name and makes a campaign of your own holding just what you changed. The campaign you opened, the game's files and mods you imported are never changed.
- **DELETE and RESET.** One tab, labelled by what it acts on: DELETE MOD (a mod you imported), DELETE COPY (a campaign you made, with its pilots and saved games), or RESET on the game's own campaign (its ticks and its pilots on the MODS screen cleared). It asks first and says what goes. The game never deletes its own files, your main-game pilots, or a folder you put in the mods folder by hand.
- **Starter mods.** `--make-starter mech`, `weapon`, `translation` or `mission` makes a small example mod from your own game, each with a short guide.
- **A modding reference** (`docs/modding-reference.md`): every key the game's and the mods' files take.

## Pirate's Moon

**Pirate's Moon is supported, as its own program in the same package** (and alone, above). It finds your copy by itself, or installs from the disc, a disc image or a zip of one, without running its Windows installer. The disc's CD track plays as the music.

- **Its own mods, settings and saves**, kept apart from MechWarrior 3's. MODS, IMPORT, EXPORT, INFO, MISSIONS, custom campaigns, mech3ax's files and the game editor for mods work there as in MechWarrior 3. A mod made for one game is left out of the other.
- **A mission whose files are missing no longer crashes.** Pirate's Moon's scripts make an empty world when the file is gone, so the load now stops on the failed read with a box saying what is missing.
- **Saving works where Windows refuses its own temporary file**, as in MechWarrior 3.
- **Its own UPGRADES page**, with the same five switches. Its WATER MOVEMENT slows a wading mech gradually as the water deepens; off, it drops to 60% in one step as the original does.
- **Mods' animations:** Pirate's Moon reads a world's animations from its anim.zbd only. A mod's other animation files are said to be unused instead of being listed as if they worked.
<!-- Lead: the "needs" warnings and the checked EXPORT reach pm with the mods-pm merge (next); add them here if they land before the tag.
     The typing-in-boxes and mouse-speed fixes are not in pm's CHANGES: say them here only if pm gets them. -->

## The launcher

- **One launcher for both games.** Click MECHWARRIOR 3 or PIRATE'S MOON. Each game shows its data's and its program's version, and whether it is "official" or "modified" (just a label: modified games start as before).
- **Checks before a game starts.** It says in plain words what is missing (an operation's missions, an empty sounds file). It refuses only when a game can't start at all.
- **GAME FILES** finds, installs, or adds music and sounds from the disc for either game, and moves its data folder. One game's files are never touched by the other's.
- **Mods are managed in each game**, on its own MODS screen. A game started from the launcher is always the original.
- **UPDATES** asks GitHub for the newest release. If there is one, it shows the notes and asks before downloading and installing it. Your mods, pilots, saves and settings are kept.
- The package's version is at the bottom of its main screen.

Everything the rebuild changes from the original is in `CHANGES.md`.

## Still known

- **macOS: window tools can delay the mouse button by about a second** (Magnet and similar). Quit the tool while you play, or exclude MechWarrior 3 in it.
- **Jump jets lighting up water (Op 3, Mission 5):** one report, not reproduced. A screenshot and `mw3.log` help.
- **Linux: stuck at "Downloading Mission Parameters":** one report, not reproduced. `mw3.log` from such a run names the loading step that went quiet.
- Mech speed in shallow water and on hills is the original's.

## Reporting bugs

Open an issue on the project's GitHub page and attach `mw3.log` from the game's settings folder (the README says where it is; each game has its own).

## Checksums

Each package's SHA256 is in `SHA256SUMS` on the release, which the launcher's UPDATES also checks.

```
(filled in from SHA256SUMS when the packages are built)
```

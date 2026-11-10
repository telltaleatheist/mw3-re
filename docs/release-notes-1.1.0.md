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

### New

- **MOUSE SENSITIVITY** on the Game Options screen, under Invert Mouse (0.5 to 3.0, starting at the original's 1.3).
- **An UPGRADES page** in the options, in the menus and in a mission: the bug-fix and wide-menu switches.
- **Joining a host playing the other game** (Pirate's Moon from MechWarrior 3, or the other way round) is refused with a message naming that game.

### Mods

- **A MODS button on the main menu.** Its screen lists custom campaigns, which you enter with ENTER, and your other mods, which you turn on and off and reorder there. When two mods change the same file, it says which one wins.
- **Custom campaigns.** A mod can bring its own campaign (`campaign=` in its mod.txt): missions in order on any world, with their own pilots and saves kept apart from the original campaign's.
- **New mechs and weapons from mod.txt** (`mech=`, `weapon=`). They come after the game's own, appear in Instant Action, the lobby and the mech lab, and count toward multiplayer's check that everyone has the same mods. A pilot that uses one remembers it, so reordering mods doesn't mix it up.
- **mech3ax's files read as they are.** Worlds, animations, mech models, motions and reader entries can be unzbd's JSON in the mod, so there's no repacking. A loose `.zrd` works too. Anything that doesn't read is named with its line in the log.
- **Animations one at a time.** A single animation file replaces that animation or adds a new one, so two animation mods can be on together.
- **IMPORT and EXPORT.** IMPORT takes a mod's .zip, .7z or .rar (or its folder) whatever its layout. EXPORT packs a mod into one .zip with checksums, checks it first, and says what the game would refuse.
- **REPORT.** It says in plain sentences what a mod replaces and adds, the mechs, weapons, worlds and campaigns it brings, and what the game refuses in it and why.
- **TEST.** Play any mission straight away with your mods on, then come back to MODS. Edits to a mod's weapons and mechs are in the next TEST without a restart.
- **A mod can say what it needs** (`needs=`, `conflicts=`, `game_version=`). MODS warns when something is missing; the mod is still turned on.
- **The game editor the original hid**, for mods: from MODS (EDITOR), or Shift+F12 in a custom campaign. It edits AI nets, vehicles and new weapons, saved into your mod, never into the game's files.
- **Starter mods.** `--make-starter mech`, `weapon`, `translation` or `mission` makes a small example mod from your own game, each with a short guide.
- **A modding reference** (`docs/modding-reference.md`): every key the game's and the mods' files take.

## Pirate's Moon

**Pirate's Moon is supported, as its own program in the same package** (and alone, above). It finds your copy by itself, or installs from the disc, a disc image or a zip of one, without running its Windows installer. The disc's CD track plays as the music.

- **Its own mods, settings and saves**, kept apart from MechWarrior 3's. MODS, IMPORT, EXPORT, REPORT, TEST, custom campaigns, mech3ax's files and the game editor for mods work there as in MechWarrior 3. A mod made for one game is left out of the other.
- **A mission whose files are missing no longer crashes.** Pirate's Moon's scripts make an empty world when the file is gone, so the load now stops on the failed read with a box saying what is missing.
- **Saving works where Windows refuses its own temporary file**, as in MechWarrior 3.
- **Mods' animations:** Pirate's Moon reads a world's animations from its anim.zbd only. A mod's other animation files are said to be unused instead of being listed as if they worked.
<!-- Lead: the "needs" warnings and the checked EXPORT reach pm with the mods-pm merge (next); add them here if they land before the tag.
     The typing-in-boxes and mouse-speed fixes are not in pm's CHANGES: say them here only if pm gets them. -->

## The launcher

- **One launcher for both games.** Click MECHWARRIOR 3 or PIRATE'S MOON. Each game shows its data's and its program's version, and whether it is "official" or "modified" (just a label: modified games start as before).
- **Checks before a game starts.** It says in plain words what is missing (an operation's missions, an empty sounds file). It refuses only when a game can't start at all.
- **GAME FILES** finds, installs, or adds music and sounds from the disc for either game, and moves its data folder. One game's files are never touched by the other's.
- **Mods are managed in each game**, on its own MODS screen. The launcher says how many mods each game has on.
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

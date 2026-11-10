# MechWarrior 3 Rebuild 1.1.0

<!-- DRAFT (2026-11-10, brought up to date the same evening) for the lead's review and Owen's go: not tagged, not published.
     The Pirate's Moon version (pm-v1.0.2 below) and the checksums are filled in when the packages are built.
     Written ahead of the code in two places, to check before the tag:
     - UPGRADES' five switches (docs/upgrades-sort.md): today the page still holds BUG FIXES and WIDE MENUS.
     - Mods by campaign (docs/mods-campaigns-plan.md): the marked gap under Mods. -->

**One download now holds both games: MechWarrior 3, Pirate's Moon, and a launcher to pick between them.** Mods get a whole set of tools: a MODS screen in the game where each campaign has its own mods, custom campaigns, new mechs and weapons, and mods made from mech3ax's files without repacking. The game's own campaign always plays the original files. Install it over any 1.0.x: your pilots, saves, settings and mods are kept.

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
- **An UPGRADES page** in the options, in the menus and in a mission. The rebuild's changes to how the game plays are switches there, all on to begin with:
  - **MFB regroup after repair:** the field bases come back to you as soon as a repair is done, and a lancemate can use them too.
  - **Improved AI:** lancemates find their way round walls, rivers and corners.
  - **Water movement:** deep water slows a mech smoothly instead of stopping it.
  - **Artemis tracking:** Artemis FCS missiles get their extra tracking.
  - **Small fixes:** the rest, one switch, with a button that shows its list (a turret's glow going out when it dies, TAG's air strike landing where the target is, the reticle locking past 800 m, and the like).
  - **Wide menus** is there too (a display choice, not a play change).
- **The game exactly as it was.** Turn the switches off, or start with `--original`, and it plays as it shipped. What stays on whatever you choose: crashes and lost data fixed, running on today's systems, and what the game's own files meant to show and say (missing voice lines, corrected graphics, sound, HUD and text).
- **A resolution you pick shows in the menus at once.** ACCEPT in GRAPHICS & AUDIO with a new shape (4:3 to 16:9, or back) puts you on the main menu in that shape; before, the menus kept the old one until the next start. WIDE MENUS on the UPGRADES page does the same.
- **Joining a host playing the other game** (Pirate's Moon from MechWarrior 3, or the other way round) is refused with a message naming that game.

### Mods

<!-- GAP: mods by campaign (docs/mods-campaigns-plan.md). Fill as the steps land; the first three points are the plan's, not yet
     in the build. Check each against the build before the tag:
     - PLAY starts the game again into the campaign (and leaving starts it again as the original); on Windows in fullscreen the
       desktop shows for about a second between the two.
     - EDIT's Save as, DELETE of a copy (its pilots and saves go with it, said in the confirm), REPORT on a copy.
     - A ticked mod that has gone missing: the campaign does not start until it is unticked or imported again.
     - Copying a pilot over from the main menu's campaign at the first PLAY. -->

- **The main menu is always the original.** Its CAMPAIGN plays the game's own files, and no button anywhere applies a mod to it. You can't lose your campaign to a mod, and there are no game files to restore.
- **Mods are played from MODS**, a button on the main menu. On the left are the campaigns: the game's own, Instant Action, every campaign a mod brings, and every copy you saved. On the right are your other mods (mechs, weapons and their values), each with a tick. Each campaign keeps its own ticks, so a weapons mod can be on for one campaign and off for another. When two ticked mods change the same thing, the list says which one wins, and you can reorder them. PLAY starts the chosen campaign with its ticked mods.
- **Each campaign keeps its own pilots and saves**, apart from the main menu's. Starting one can copy a pilot over from the main menu's campaign; the original pilot is never written.
- **Multiplayer takes no mods for now.**
- **Translations and purely visual mods are not managed here.** They are installed the old way, by replacing game files, and nothing stops that.
- **Custom campaigns.** A mod can bring its own campaign (`campaign=` in its mod.txt): missions in order on any world. A mod made for the original game that replaces the campaign's files becomes a campaign of its own, on top of the game's.
- **New mechs and weapons from mod.txt** (`mech=`, `weapon=`). They come after the game's own and appear in Instant Action and the mech lab of the campaigns they are ticked for. A pilot that uses one remembers it, so reordering mods doesn't mix it up.
- **mech3ax's files read as they are.** Worlds, animations, mech models, motions and reader entries can be unzbd's JSON in the mod, so there's no repacking. A loose `.zrd` works too. Anything that doesn't read is named with its line in the log.
- **Animations one at a time.** A single animation file replaces that animation or adds a new one, so two animation mods can be ticked together.
- **IMPORT and EXPORT.** IMPORT takes a mod's .zip, .7z or .rar (or its folder) whatever its layout, and puts a campaign mod among the campaigns and any other mod in the list of ticks. EXPORT packs a mod into one .zip with checksums, checks it first, and says what the game would refuse.
- **REPORT.** It says in plain sentences what a mod replaces and adds, the mechs, weapons, worlds and campaigns it brings, and what the game refuses in it and why.
- **TEST.** Play one mission of the chosen campaign straight away with its ticks, then come back to MODS.
- **A mod can say what it needs** (`needs=`, `conflicts=`, `game_version=`). A campaign shows the mods it needs ticked and locked.
- **The game editor the original hid**, for mods: EDIT on the chosen campaign. It edits AI nets, vehicles and new weapons. Saving is always a new copy of the campaign that holds only what you changed; the game's own files and the campaign you started from are never written.
- **Starter mods.** `--make-starter mech`, `weapon`, `translation` or `mission` makes a small example mod from your own game, each with a short guide.
- **A modding reference** (`docs/modding-reference.md`): every key the game's and the mods' files take.

## Pirate's Moon

**Pirate's Moon is supported, as its own program in the same package** (and alone, above). It finds your copy by itself, or installs from the disc, a disc image or a zip of one, without running its Windows installer. The disc's CD track plays as the music.

- **Its own mods, settings and saves**, kept apart from MechWarrior 3's. MODS works there as in MechWarrior 3, with both of its campaigns (the Campaign and the Pirate Missions) and Instant Action as entries, each with its own ticks, pilots and saves. A mod made for one game is left out of the other.
- **The UPGRADES page and `--original`**, as in MechWarrior 3, with Pirate's Moon's own small fixes in its list.
- **A resolution you pick shows in the menus at once**, as in MechWarrior 3.
- **A mission whose files are missing no longer crashes.** Pirate's Moon's scripts make an empty world when the file is gone, so the load now stops on the failed read with a box saying what is missing.
- **Saving works where Windows refuses its own temporary file**, as in MechWarrior 3.
- **Mods' animations:** Pirate's Moon reads a world's animations from its anim.zbd only. A mod's other animation files are said to be unused instead of being listed as if they worked.
<!-- Lead: the "needs" warnings and the checked EXPORT reach pm with the mods-pm merge (next); add them here if they land before the tag.
     The typing-in-boxes and mouse-speed fixes are not in pm's CHANGES: say them here only if pm gets them. -->

## The launcher

- **One launcher for both games.** Click MECHWARRIOR 3 or PIRATE'S MOON. Each game shows its data's and its program's version, and whether it is "official" or "modified" (just a label: modified games start as before).
- **Checks before a game starts.** It says in plain words what is missing (an operation's missions, an empty sounds file). It refuses only when a game can't start at all.
- **GAME FILES** finds, installs, or adds music and sounds from the disc for either game, and moves its data folder. One game's files are never touched by the other's.
- **No MODS button in the launcher.** Mods are managed in each game, on its own MODS screen.
- **"Opening the file picker..."** is shown before the system's file picker comes up. The system can take a second to open it (longer the first time on some Macs), and the launcher used to sit frozen meanwhile.
- **UPDATES** asks GitHub for the newest release of the combined package. If there is one, it shows the notes and asks before downloading it; the download is checked against the release's checksums before anything is installed. Your mods, pilots, saves and settings are kept. Nothing is downloaded or changed unless you press the button and say yes.
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

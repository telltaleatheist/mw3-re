# Mods by campaign: the plan (Owen, 2026-11-10)

Owen's model for how players use mods, replacing "mods turned on apply to the
whole game". Both games; Pirate's Moon has two official campaigns (Campaign and
Pirate Missions), MechWarrior 3 one.

## The rule

- **The main menu is the original.** Its CAMPAIGN (and Pirate's Moon's Pirate
  Missions) always plays the game's own files: no button anywhere applies a mod
  to it. Nobody can lose their campaign or need to restore game files.
- **Game mods are played through MODS.** Game mods are worlds and campaigns
  (built in zStudio and imported whole) and assets (new or changed mechs,
  weapons and their values).
- **Instant Action takes mods, from its own screen** (Owen, 2026-11-10,
  replacing "an entry in the CAMPAIGNS list"): see Instant Action below.
- **Multiplayer takes no mods for now**; perhaps later, if players ask.
- **Translations and purely visual mods are not managed.** Players do them the
  old way, with an installer that replaces game files; nothing stops that, and
  modders may still edit any file directly. The MODS screen is for game mods.

## The MODS screen

- **CAMPAIGNS (left):** the official campaign(s), there from the first start;
  every campaign imported from a mod; every copy the player saved.
- **The chosen campaign's mods (right):** every imported asset mod, with a
  checkbox. Each campaign keeps its own ticks: Freako's weapons can be on for
  one campaign and off for another. A campaign that ships its own assets, or
  names them with `needs=`, shows those ticked and locked. Where two ticked
  mods change the same thing, the list shows which one wins, and the player
  can reorder them (as the MODS list does today).
- **PLAY:** the chosen campaign with its ticked mods.
- **EDIT:** the chosen campaign in the Battlesport editor. Saving is always
  **Save as**: a new campaign entry, a copy that holds only the files changed,
  on top of the campaign it came from. The official entry is never written; a
  copy can be deleted (only folders we made and marked), and its original is
  still there.
- **IMPORT / EXPORT, REPORT, TEST:** IMPORT puts an asset mod in the
  right-hand list, unticked, and a mod that brings a campaign of its own in
  CAMPAIGNS. **A mod made for the original game that is laid over its files**
  (new weapons, harder missions, changed scenarios, in any of the game's
  worlds) **is an asset mod** (Owen, 2026-11-10, after a real one was tried):
  a tick for each campaign and for Instant Action, since one such mod often
  changes the campaign, the Pirate Missions and Instant Action at once. Only
  a mod with a campaign of its own gets a row at the left. **IMPORT also
  takes a modded game folder**: a mod that ships as an installer is installed
  onto a copy of the game, and IMPORT keeps the files that differ from the
  player's own install as the mod. REPORT on a copy says what it changed
  from the campaign it was saved from. TEST plays one mission of the chosen
  campaign with its ticks.
- **Pilots and saves:** each campaign entry keeps its own, so modded progress
  never mixes with the main menu's campaign. **The player picks their own
  pilot** (Owen, 2026-11-10): an entry's pilot screen lists its own pilots and
  the main game's; picking a main-game pilot the entry does not have brings it
  and its saved mechs across quietly, then and there, and it goes on as the
  entry's own. No question is asked. The main game's pilot is never written,
  so what the pilot earns in an entry (kills, progress) stays with the entry's
  copy. In the game's own campaign the pilot carries on from where it is; in a
  different campaign it starts at that campaign's first mission, and its saved
  games are not brought.

Freako's weapons, start to end: MODS, IMPORT his files, choose any campaign
(the official one or a copy), tick his mod, PLAY.

## What changes in the code

- The one list of mods turned on (`mod=` lines in mw3.cfg, mounted at start for
  the whole run) gives way to one set per campaign entry, kept beside that
  entry's pilots and saves.
- A start mounts no mods: the main menu, its campaign and multiplayer play the
  original. Mods are mounted only for a campaign (or Instant Action) entered
  from MODS.
- Mods are read when the game starts, and caches (mech definitions, the
  mechlib, textures, sounds, the menus' reader entries) are filled then. So
  the first version enters a campaign by **starting the game again into it**
  with its set (the program's own start path, behind the loading screen), and
  goes back to the original the same way. Mounting in place, without the
  restart, can come later, once every cache it would leave stale is known.
- "Save as" makes a new campaign mod: a folder of ours (with our marker) whose
  `mod.txt` names the campaign it is based on and holds the edited files.
- Multiplayer's lobby check for matching mods stays, finding none on both
  sides until multiplayer takes mods.
- Nothing is lost on the way: the `mod=` lines a player has today become the
  official campaign entry's ticks the first time this build starts.

## Instant Action (Owen, 2026-11-10)

- **A MODS button on Instant Action's own screen** opens the tick list a
  campaign has: every imported mod with a checkbox, order and conflicts shown.
- **ACCEPT starts the game again into Instant Action with those mods** and
  comes back to the same screen. With nothing ticked, Instant Action is the
  original and nothing starts again.
- **The ticks are remembered.** With mods ticked, INSTANT ACTION on the main
  menu goes straight into the modded one. The screen always says what is on
  ("MODS: 2 on"), and one button turns them all off (back to the original).
- **Custom maps:** a ticked mod's Instant Action maps (`ia_world=`) show in
  the map list beside the game's own, so a map a player built and imported as
  a mod is played here.
- **Saved mech loadouts with mods on are kept apart** from the main game's (a
  loadout with a modded weapon would not load in the original); the copy-over
  offer brings the player's existing ones across.
- **MODS, CAMPAIGNS lists campaigns only**: Instant Action has no row there.
  Underneath it is still the entry `ia` (its folder, ticks and loadouts).

## Deleting and resetting (Owen, 2026-11-10)

- **Edits are saved as a copy.** The editor never writes the game's own
  campaign or a mod someone else made: the first save makes a copy of the
  player's own, and later saves go into it.
- **DELETE on the MODS screen**, by what is chosen, each with a confirm that
  says what goes:
  - a copy of the player's own: the copy, with its pilots and saved games;
  - a mod the game imported: the mod. An asset mod's ticks are left, so a
    campaign that had it ticked shows "!" until it is turned off or imported
    again. An imported campaign's pilots and saved games are kept, so importing
    it again goes on where it stopped;
  - what is left of a campaign whose mod is gone: its pilots and saved games.
- **RESET on the game's own campaign** (it cannot be deleted): its ticked mods
  are cleared and its pilots and saved games on the MODS screen go; the row
  stays, as on a first start. The main game's pilots are not changed.
- **Not deleted by the game:** the game's own files, and a mod folder put in
  the mods folder by hand (no marker of ours). MODS FOLDER opens the folder so
  the player can remove it.
- The standing rule holds: code removes only folders it made and marked.

## Settled in the lead's review (2026-11-10)

- **Inside a campaign entry** the player gets that entry's own screen only:
  its pilot screen, Instant Action's screen, the mission (TEST) or the editor.
  Multiplayer is not offered there. Leaving goes back to MODS on a plain run.
- **Entering and leaving restart the program** (the first version). On Windows
  in fullscreen the desktop shows for about a second between the two.
- **One set of Instant Action ticks per game**, on both games (Instant Action below).
- **The Upgrades settings are the player's, not the entry's**: one set for the
  whole game, whichever campaign is played.
- **A ticked mod that has gone missing** (removed from the mods folder) shows
  as missing, and the entry does not start until it is unticked or imported
  again. The entry's pilots and saves are kept.
- **Deleting a copy** removes its pilots and saves with it, and the confirm
  says so. Official entries and imported mods' files are never deleted this way.
- **A saved game belongs to its entry**: it lives in the entry's own pilots
  folder and records the entry and its ticks, so loading it after the ticks
  changed says so.
- **No release ever read `mod=` lines** (mods came after 1.0.5), so the move of
  those lines into the official entry only meets test folders.

## State (2026-11-10): built

Every step is built and on main or in review; how each works, with its
tests, is in `docs/modding.md` from "Mods by campaign: how it works" on.

| Step | What | modding.md |
|---|---|---|
| 1 | the entries, their folders and ticks, the first start's move | Mods by campaign: the entries |
| 2a | starting again into an entry and back; an entry's own screen and saved mechs | Starting again into an entry |
| 2b | a plain start mounts nothing; the run's entry takes what wrote mw3.cfg | A plain start is the original |
| 3a, 3b | the CAMPAIGNS screen, each campaign's ticks, PLAY; REPORT on a copy | The CAMPAIGNS screen |
| | Instant Action's own MODS button, the tick list, straight in | Instant Action's own MODS |
| | pilots: the main menu's listed in an entry and brought when picked | Pilots inside an entry |
| | a mod laid over the game's files is a tick, not a campaign | the entries (A mod's kind) |
| 4a | TEST through entries, the test mode | TEST through entries |
| 4b | the editor only saves as a copy | EDITOR through entries: Save as only |
| 4c | DELETE and RESET, the one remover | DELETE and RESET |
| | dev 3: the pilot's copy, REPORT on a copy, a save's entry, the restart on Windows | their own sections after Pilots inside an entry |
| | dev 2: IMPORT from a modded game folder | IMPORT from a modded game folder |
| 6 | Pirate's Moon: by merge on `pm` (dev 3) | |

## Open

- 1.1.0 waits for this (Owen, 2026-11-10), so mods change behaviour once.
- Owen's playtest of what was only seen headless or through a checks-only
  switch: `docs/playtest-1.1.0.md`, "To see with eyes: mods by campaign".
- Mounting in place, without the restart, once every cache it would leave
  stale is known (What changes in the code, above).
- EXPORT cannot yet split a copy's weapons out into an asset mod.
- Choosing and ordering a campaign's missions in a dialog (Owen's Campaign
  Missions pick): after the release.
- The starter mods' translation kind: kept as it is (it costs nothing), but not
  featured, since translations are not managed.

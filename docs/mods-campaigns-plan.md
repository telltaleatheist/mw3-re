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
- **Instant Action takes mods**: it is an entry in the same CAMPAIGNS list,
  with the same checkboxes; PLAY opens Instant Action's own screen with them.
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
- **IMPORT / EXPORT, REPORT, TEST:** as now. IMPORT puts an asset mod in the
  right-hand list and a campaign mod in CAMPAIGNS. A mod made for the original
  game that replaces its campaign's files (Freako's campaign rewrite) becomes a
  campaign entry of its own, on top of the official one. REPORT on a copy says
  what it changed from the campaign it was saved from. TEST plays one mission
  of the chosen campaign with its ticks.
- **Pilots and saves:** each campaign entry keeps its own, so modded progress
  never mixes with the main menu's campaign. Starting a campaign entry can
  copy a pilot over from the main menu's campaign; the copy goes on alone and
  the original pilot is never written.

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

## Open

- 1.1.0 waits for this (Owen, 2026-11-10), so mods change behaviour once.
- The starter mods' translation kind: kept as it is (it costs nothing), but not
  featured, since translations are not managed.

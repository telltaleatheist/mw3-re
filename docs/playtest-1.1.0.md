# Playtest list for 1.1.0

Owen's reports from the combined build (main de859a4a, pm 111a468f, Mac),
2026-11-10, one item per row, worked through in order. Each item says what
was seen, what is known so far, who has it and its state. A fix names its
branch and commit when it lands.

| # | Area | Seen | Known so far | Who | State |
|---|---|---|---|---|---|
| 1 | Launcher | The MODS button isn't needed any more: mods are done in the games' own MODS screens. | Both games' MODS screens do everything the launcher's did (IMPORT, EXPORT, REPORT, TEST, the list). | dev 1 | in progress |
| 2 | Launcher (Mac) | FIND THE GAME's file picker took a very long time to open, then stayed blank for a while. | mw3launcher.log: "the picker took the focus 9531 ms after it was asked for". The picker opens with no start folder, so macOS reopens the last one (Downloads, which holds two disc zips of 300-600 MB). To measure: the first picker of a fresh process against a second one, Downloads against an empty folder, and a start folder of our own (the Desktop or /Volumes for discs). | - | open |
| 3 | Pirate's Moon | Started from the launcher it plays 4:3. Changing the resolution to 1920x1080 doesn't change the shape. | PM's usual Mac folder (~/Library/Application Support/MicroProse/Pirate's Moon) holds settings from earlier tests (InGameVMode 13, 1920x1080, GameOptions 9); a first start with no InGameVMode plays at the desktop's size. Its log shows the menus at "frame 640 x 480, canvas 640 x 480", where MW3 with wide menus draws 853x480: check whether PM's menus take wide menus, and what the mission's view is after a resolution change in PM. | - | open |
| 4 | MODS, both games | Pirate's Moon's MODS screen has no Campaign or Pirate Missions to choose. | Owen's design (2026-11-10): the MODS screen holds a copy of each game's own campaigns as permanent entries (MW3: Campaign; PM: Campaign and Pirate Missions), played and modded there without touching the original's files, whether or not a mod is downloaded. The main menu's CAMPAIGN stays the original. Not built in either game: CAMPAIGNS lists only mods' campaigns today. | dev 1 | open |
| 5 | Pirate's Moon (Mac) | Its usual folder's mw3.cfg says `install=` a test worktree (lead-pm's game/pm, from 11-07). | The launcher passes `--install` to its new install, so play is not affected; a start of mw3pm without the launcher would read the stale line. Clean the folder's leftovers from tests, and check the launcher writes the install it made into mw3pm's own cfg. | lead | open |

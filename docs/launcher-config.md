# The launcher's settings file: an inventory (step 1)

Every number or setting compiled into the launcher (2026-11-27, on v1.9.0), as
input for a settings file that ships with it: what is there, what each does, a
safe range, and who a bad value could hurt. Step 2 (approved 2026-11-27, the
layout of the notes at the end) is built: see Step 2, built, at the end.

"Hurts" means who pays for a bad value:
- **player**: only the person who edited the file (a long wait, a download
  given up too early, a window that looks wrong);
- **servers**: our catalog server and release mirrors, or GitHub (its API
  limits unauthenticated asks per address per hour);
- **nobody**: cosmetic, or the log.

The **File** column is the proposal: **yes** (in the file, clamped to its
range), **no** (stays compiled in), **never** (trust: compiled in or in signed
data, see section F).

Paths are `src/frontend/sdl/` unless given in full.

## A. Asking our servers and GitHub

No ask is ever retried and nothing backs off: a failed ask is simply asked
again at its next trigger (below). So the number of asks is set by the
triggers, not by timeouts; the timeouts only decide how long the player waits.

| Setting | Where | Now | What it does | Range | Hurts | File |
|---|---|---|---|---|---|---|
| Catalog fetch while open | `launcher.cpp` `catalog_every_ms` | every 4 h | Fetches the games catalog again while the launcher stays open (also at the start, on CHECK FOR UPDATES, after an install or update) | 4 h to 7 days, or off | servers (below 4 h) | yes, floor 4 h |
| Catalog fetch at the start | `launcher.cpp` `start_catalog_fetch("the start")` | on | The start's fetch (the list on screen meanwhile) | on / off | player (off: new games only on CHECK FOR UPDATES) | maybe |
| Catalog and its .sig, time allowed | `sdl_catalog.cpp:274, 278` | 15 s each | How long one fetch may take | 5 to 120 s | player | yes |
| Catalog art, time allowed | `sdl_catalog.cpp:146` | 15 s | A badge or card PNG the catalog names | 5 to 120 s | player | yes |
| Games' release lists, time allowed | `sdl_games.cpp:391` | 15 s | Each game's releases at each home, at the start and on CHECK FOR UPDATES | 5 to 120 s | player | yes |
| Games' release lists, count | `sdl_games.cpp:391` | 20 | Releases asked per list (`{count}` in the home's address) | 10 to 40 | servers (bytes per ask), player (too few can miss a release behind prereleases) | yes |
| Launcher's own update check, time allowed | `sdl_update.cpp:565, 568` | 20 s | The launcher's releases, both repos, each home | 5 to 120 s | player | yes |
| Launcher's own update check, count | `sdl_update.cpp:565, 568` | 40 | Releases asked per list | 20 to 100 | servers (bytes), player (too few) | yes |
| SHA256SUMS, time allowed | `sdl_update.cpp:278` | 60 s | The sums file of a package about to be installed | 15 to 300 s | player | yes |
| SHA256SUMS.sig, time allowed | `sdl_update.cpp:283` | 30 s | Its signature | 10 to 300 s | player | yes |
| Our own wait past curl's | `sdl_update.cpp:142` | time allowed + 5 s | Ends a curl that does not stop at its own `--max-time` | as is | nobody | no |
| Download: connect timeout | `sdl_update.cpp:189` | 20 s | A package or original download gives up if it cannot connect | 5 to 120 s | player | yes |
| Download: too slow | `sdl_update.cpp:190` | under 1024 B/s for 60 s | Gives up a download that has all but stopped | 1 to 65536 B/s; 15 to 600 s | player | yes |
| Release checks: when | `launcher.cpp:4157-4158`, `recheck()` | the start, CHECK FOR UPDATES, a catalog with new games | No periodic release check today | as today; any periodic check only at 4 h or longer | servers | no (a new trigger is a design change) |
| User agent | `sdl_update.cpp:121, 191` | `MW3-Rebuild-launcher` | Names the launcher to the servers | none | servers (our logs) | no |

## B. Waiting on programs (the games, installers, the system)

| Setting | Where | Now | What it does | Range | Hurts | File |
|---|---|---|---|---|---|---|
| Game's first frame | `launcher.cpp` `ready_wait_ms` | 20 s | After PLAY, how long before the status line says the game is slow to start | 5 to 300 s | player | yes |
| Status reads while a game starts | `launcher.cpp:2364` | every 250 ms | Reads the game's own status line before its first frame | 100 to 2000 ms | player (CPU) | no |
| Kit game status, at the start | `launcher.cpp:1771, 1776` (`status_seconds(10)`) | 10 s | A kit game's `--status` | 3 to 120 s | player | yes |
| Kit game status, on a press | `launcher.cpp:1820` (`status_seconds(30)`) | 30 s | The same, asked again when PLAY finds it unknown | 5 to 300 s | player | yes |
| Kit setup `--describe` | `launcher.cpp:1985` | 10 s | Asks a kit game what its setup offers | 3 to 60 s | player | yes |
| Kit setup `--search` | `launcher.cpp:2011, 3586` | 120 s | A kit game's search for its files | 30 to 600 s | player | yes |
| Kit setup `--look`, `--install` | `launcher.cpp:2014`, `sdl_games.cpp:682, 691` | no limit | A pick's look, the copy in | none | player | no (a limit could cut a copy in half) |
| Original's setup, stopping | `sdl_games.cpp:604` | 30 s | After CANCEL, how long the setup may take to stop at its next step | 10 to 120 s | player | no |
| Mac: mounting a DMG | `sdl_update.cpp:488`, `sdl_games.cpp:187` | 120 s | `hdiutil attach` for an update or a game | 30 to 600 s | player | yes |
| Linux: unpacking an AppImage | `sdl_games.cpp:218` | 600 s | `--appimage-extract` | 60 to 3600 s | player | yes |
| Windows: an uninstaller | `sdl_games.cpp:978-979` | 300 s, then 30 s more | Waits for a game's own uninstaller (and the copy Inno runs from) | 60 to 1800 s | player | yes |

## C. The progress bar (One bar for a press)

| Setting | Where | Now | What it does | Range | Hurts | File |
|---|---|---|---|---|---|---|
| A setup's share of the bar | `launcher_job.h:45` `kSetupShare` | 15% | The original game's setup's part of one bar | 0 to 50% | nobody | yes |
| Speed taken over | `launcher_job.h:46` `kWindow` | 5 s | The speed shown is the average over this long | 1 to 30 s | nobody | yes |
| "starting..." for | `launcher_job.h:47` `kSettle` | 2 s | Before a speed and time left are shown | 0 to 10 s | nobody | yes |
| "waiting for the server..." after | `launcher_job.h:48` `kStall` | 10 s | Without a byte this long | 3 to 120 s | nobody | yes |
| The original's setup starts at | `sdl_games.h:54` `kOriginalSetupFrom` | 0.6 | Inside get_original's own progress | as is | nobody | no |
| Progress line in the log | `launcher.cpp:1328` | each second, and on a part change | How often a running job is logged | 1 to 60 s | nobody (log size) | yes |

## D. The window, pacing and sounds

| Setting | Where | Now | What it does | Range | Hurts | File |
|---|---|---|---|---|---|---|
| Window size at the start | `launcher.h:28` | 1280 x 720 | Already `--window WxH` | 640 x 360 to 7680 x 4320 | player | yes |
| Layout canvas | `launcher.cpp:45`, `launcher_ui.cpp:19` | 854 x 480 | The units the screens are laid out in, scaled to the window | none | player (a broken layout) | no |
| Vsync | `launcher.cpp:4275` | on | Paces frames to the display | on / off | player | yes |
| Frames without vsync | `launcher.cpp:4441` | 60 a second | The timed sleep when there is no vsync | 30 to 240 | player (CPU) | yes |
| Covered, minimized, or a game in front | `launcher.cpp:4448` | wakes every 100 ms | Few frames while not seen; any event at once | 50 to 500 ms | player | yes |
| Pacing with vsync | `launcher.cpp:4441` (`display_frame`) | never faster than a little over the display | Stops macOS piling up drawables behind a full-screen game (docs/launcher.md, Pacing) | none | player (memory: tens of GB seen 2026-11-25) | never off |
| Sound gains | `launcher.cpp` `kGain` | hover 0.7, select 1.0 | The main menu's sounds | 0 to 1 | nobody | yes |
| Copies shown on GAME FILES | `launcher.cpp:52` `kCopyRows` | 4 | The rest through PICK A FILE | none | player (layout) | no |
| Text sizes and widths | `launcher.cpp` (`kTries`, `kNotesTop`, `kUpdateWidth`, row widths) | fixed | The layout | none | player | no |
| Font search, title | `launcher.cpp:57, 78-95` | fixed | The lettering and the BATTLETECH name | none | nobody | no |

## E. Sizes the code is built around (no)

`kMaxGames` 10 (the per-game button ids leave room for ten), `kMaxHomes` 4,
`kMaxCatalogSources` 6, `kMaxNextKeys` 4, `kMaxRevokedKeys` 8, `kMaxOriginals` 4
(`src/upgrades/update/games.h`), `kMaxSourceAddresses` 6, `kMaxKeys` 8
(`sources.h`). These size arrays and the catalog's reader; raising them is a
code change and a release, not a setting.

## F. Never configurable (trust)

These stay compiled in or come only from signed data. A file anyone can edit
must not reach them, even to "fix" something:

- **Keys**: the catalog's signing keys, its next and revoked keys, the release
  signing keys (`signing_keys()`), the sources' keys (`sources.h`).
- **https only**: `--proto =https --proto-redir =https` on every ask; `file://`
  only in checks.
- **Checksums**: every download checked against its SHA-256 (packages, the
  Internet Archive's originals, the catalog's art by `badge_sha256` and the
  rest); SHA256SUMS checked against its signature; which homes may serve
  unsigned sums (`takes_unsigned_sums`).
- **The catalog**: its signature, the rule that only a newer revision than the
  one in use and the built-in is taken (no going back), `min_launcher`, the
  owner (`kCatalogOwner`).
- **Where the launcher asks**: the catalog's addresses and the release homes
  come from the sources list and the signed catalog. A file that could point
  them elsewhere could send every player's asks to someone else's server.
- **The folder marks** (`rebuild-games.txt`, `.rebuild-staging`, `.incoming`,
  `rebuild-game.txt`, `rebuild-original-download.txt`, `mw3-install.txt`): they
  are the guard that keeps the launcher from removing a folder it did not
  make.
- **The checks' environment variables** (`MW3_*`): they stay as they are, for
  the checks, and are not mirrored in the file.

Settings the player already makes in the launcher (the games folder, the data
folder, linked copies, unlinked ones) stay in the launcher's own saved
settings, not in this file.

## Step 2: comments on the design

The proposed design: plain JSON beside the program; defaults equal today, so a
missing or broken file changes nothing; each value clamped to its range, with
a log line for a bad one; floors so the servers are never asked more often
than today. Agreed, with these points:

1. **Not beside the program.** The Mac app is signed and notarized: a file
   edited inside the bundle breaks its signature, and the app may then refuse
   to open. Program Files on Windows needs an administrator to edit, an
   AppImage is read-only, and every self-update replaces the program's folder,
   wiping the player's edits. Proposal: ship a read-only
   `launcher-settings.json` with the program, every key at its default as
   documentation, and read the player's copy from the launcher's data folder
   (beside its saved settings). The launcher writes nothing there; a player
   copies the shipped file to start.
2. **Clamping, per key.** Out of range: the nearest end of the range, with a
   log line naming the key, the value and what was used. Wrong type: the
   default, logged. Unknown key: ignored, logged once (a typo is visible).
   A file that does not parse: every default, one log line. Read once at the
   start; no reload.
3. **Floors for the servers.** Only the items marked servers in section A
   need them: the catalog's 4 h minimum, the release counts, and no new
   periodic release check below 4 h. Room to tighten them without a
   release: the signed catalog could carry floors that only raise the file's
   minimums, never lower them.
4. **A version key** (`"version": 1`) so a later launcher can rename or retire
   keys and say so in the log, instead of guessing.
5. **One table in code**: each key's name, default, minimum, maximum and unit
   in one list, read by the loader, by a check that every key here is in it,
   and by the shipped file's generator, so the doc, the code and the shipped
   file cannot drift.
6. **Checks**: a missing file and a file of defaults give the same log as
   today; each key below, above and of the wrong type is clamped or falls
   back with its line; a broken file gives the defaults; nothing in section F
   is read from it (a file naming keys, https or a catalog address changes
   nothing).

## Step 2, built (2026-11-27)

The layout as approved: `assets/launcher/launcher-settings.json` ships with
the launcher (every package copies it beside `sources.json`; Windows'
`check_dist.ps1` checks it is there) and holds every key at its default, a
`"_<key>"` line beside each giving its range, unit and use (JSON has no
comments; keys starting with `_` are skipped). The player's own
`launcher-settings.json` in the launcher's data folder is read over it. Both
are read once at the start (`run_launcher`), and the log lists each file
read, each value clamped or ignored, and every value not at its default.

One table, `kKeys` in `src/frontend/sdl/launcher_settings.cpp`, gives each
key's name, default, range, unit and line. The loader clamps by it, the
shipped file is its text (`shipped_text()`), and
`launcher_settings_shipped_file_is_the_table` fails when the file and the
table differ (`MW3_WRITE_SETTINGS=1` rewrites the file from the table). The
defaults are today's values.

The rules: out of range, the nearest end, with a line; a whole-number key
given a fraction is rounded, with a line; a wrong type, the default, with a
line; an unknown key, ignored, with a line (so a typo shows); a file that does
not read, nothing in it used, with one line; a `"version"` newer than 1, its
known keys read, with a line. `catalog_every_hours` takes 0 (off) or 4 to 168,
so the servers are never asked more often than today. Section F has no key,
and `launcher_settings_clamp_and_fall_back` checks that a file naming an
address, keys or https changes nothing. The checks' `MW3_*` variables still
win where they existed (`MW3_LAUNCHER_READY_MS`, `MW3_STATUS_SECONDS`,
`MW3_CATALOG_EVERY_S`, `MW3_LAUNCHER_NOVSYNC`), and `--window WxH` wins over
the window keys.

The keys, by the sections above: A `catalog_every_hours`, `catalog_at_start`,
`catalog_seconds` (the catalog and its .sig), `art_seconds`,
`releases_seconds`, `releases_count`, `update_check_seconds`,
`update_check_count`, `sums_seconds`, `sums_signature_seconds`,
`download_connect_seconds`, `download_slow_bytes_per_second`,
`download_slow_seconds`; B `game_first_frame_seconds`, `game_status_seconds`,
`game_status_press_seconds`, `game_setup_describe_seconds`,
`game_setup_search_seconds`, `mac_disk_image_seconds` (an update's and a
game's), `linux_appimage_seconds`, `windows_uninstaller_seconds` (the wait for
the uninstaller; the 30 s for Inno's copy after it stays); C `bar_setup_share`,
`bar_speed_seconds`, `bar_starting_seconds`, `bar_waiting_seconds`,
`log_progress_seconds`; D `window_width`, `window_height`, `vsync`,
`frames_without_vsync`, `hidden_wake_ms`, `sound_hover_volume`,
`sound_select_volume`. Everything marked no or never above stays compiled in.

Checked: `launcher_settings_shipped_file_is_the_table`,
`launcher_settings_clamp_and_fall_back`, `sdl_program_launcher_reads_its_settings`
(the program with a player's file: vsync off at 30 frames a second, a first
frame wait clamped to 300 s, an address ignored, each in its log).

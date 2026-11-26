# The launcher on its own (plan, 2026-11-25)

The aim: the BattleTech launcher lives on mw3-re's main (the hub), built from
its own code. Each game works on its own: MechWarrior 3, Pirate's Moon, the
MW1 Remake, MW1 Rebuild and Inception's two. MW1 Rebuild and Inception already
work that way: their own setup finds the player's files and answers through
re-setup-kit's contract (`rekit/status.h`, `rekit/sdl/setup_serve.h`;
launcher.md, The setup kit's contract). MW3 and Pirate's Moon do not. The
launcher sets them up itself with MW3's code, so `mw3launcher` links `mw3core`.

The order, each step shippable on its own:

- **(a)** MW3 and Pirate's Moon serve their own setup through the kit's
  contract. The launcher keeps its copy meanwhile.
- **(b)** The launcher uses the contract for every game and drops the game
  code.
- **(c)** The launcher's code moves to mw3-re's main, with a bridge release if
  one is needed.
- **(d)** The old copy comes out of mech3-re.

This is a survey and a plan. It was surveyed on mech3-re main at fbe9dcfd8.
Done since: a1 (re-setup-kit main 78e1a76) and a2 (mech3-re branch
mac5/mw3-recipe). The decisions of 2026-11-25 are in Decided, at the end.

## 1. Where the launcher reaches into game code

**Measured, not read from the includes.** The links were followed from the
built objects (`nm` over `CMakeFiles/mw3launcher.dir` and the static
libraries, the linker's own walk). `mw3launcher` takes 356 objects from
`mw3core`, `mw3sdl` and `mw3text`. That is nearly the whole game: the AI, the
mech lab, mods, campaigns and the editor. The program is 27 MB against mw3's
31 MB (a development build).

The door is one chain. `mech3::setup::apply_patch` (game_setup.cpp) calls
`mech3::mod_note` (data_version.cpp). That reaches `entry_archives`, then
`campaign_entries`, then `mods`, and from there the rest. The launcher calls
only seven of `mw3core`'s objects directly. Each one is below.

**mw3 compiles the launcher too.** `mw3` builds `launcher.cpp`, `launcher_ui`,
`sdl_update` and `sdl_games` into itself. A plain start with no game files,
`--setup`, and Shift held at the start all show the launcher's find screen in
mw3's own window (`LauncherOptions::setup_only`). mw3pm does the same with
`pm`. So today one source file serves two programs. Step (d) has to give mw3
and mw3pm a setup screen of their own (section 3, d1).

### What each piece is for, and where it goes

"Game" means the game's program, answering through the contract. "Kit" means
re-setup-kit, as a general optional piece that names no game. "Own" means a
small copy that is the launcher's own.

**MW3's and Pirate's Moon's setup recipe: to the game.**

| Piece | The launcher uses it for | Goes to |
|---|---|---|
| `mech3/game_setup` | `check_install`: each copy's state and data version (the start's quiet search, GAME FILES' list, Pirate's Moon's notes). `start_check`: BEFORE YOU PLAY's notes (`mw3_ready`). `install_from_source`: INSTALL. `add_from_disc` and `add_pm_from_disc`: ADD FROM MY DISC (music, movies, Pirate's Moon's full sounds) for the files in use. `find_discs`: a disc in a drive, when FIND THE GAME's search finds no install. `apply_patch`: the 1.2 patch's zip (`kPatch`), unreachable since the patch is built in. Its types `Progress`, `Result` and `ProgressFn` are also the launcher's `Work` type. | Game: the served verbs (section 2). `apply_patch`'s button: removed. The `Work` types: own, a few lines. |
| `mech3/source_map` | `map_source`: what a pick holds (`look_at`), and `is_pm_install` (find_pm, GAME FILES, a Pirate's Moon pick). | Game: `--setup --look`. |
| `mech3/data_version` | Reached through `game_setup`, and through `launcher_notes`' words. | Game: status `data`. |
| `mech3/patch_import.h` | Included by sdl_update and sdl_games for `mech3::sha256_file_hex`, an alias of the kit's. | Kit: `rekit::sha256_file_hex` called directly. |
| `frontend/sdl/sdl_install` | `install_candidates`: MW3's and Pirate's Moon's registry keys and folders (the search). `install_valid`. `pick_source`: what the one picker's pick means. `media_dir`. `save_install` and `save_config_value`: mw3.cfg's lines. `kGameFolderMark`. Its header also pulls `upgrades/settings/upgrades.h`. | The search and `install_valid`: game (`--search`). `pick_source`'s game-free part (a .bin means its .cue): own. `media_dir` and the mark: kit (`rekit/game_folder.h` has both, same file name). The line editor: own copy, for the launcher's own keys (below). |
| `frontend/sdl/movie_import` (mw3text) | `importer_path`, `run_importer`, `movies_missing`: the launcher converts the movies with mw3import before the game starts (`convert_movies`). | Game: a served first-run step (section 2). |
| `frontend/sdl/launcher_notes` (mw3text) | `have_music`, `have_movies`, `launcher_notes`: MW3's notes, in MW3's words. | Game: status `notes`. |
| `gamez/zrender/zrndr_tint.h` | Included; nothing in it is used. | Remove the include. |

**Launcher features that read MW3's files.**

| Piece | Used for | Goes to |
|---|---|---|
| `gamez/zutil/zutl_zar` | The main menu's two sounds (`Sounds::load`): sfx_button2.wav and sfx_button6.wav from the player's soundsL.zbd or soundsH.zbd. Also `crc32`, for BEFORE YOU PLAY's seen-once check (`notes_once`). | `crc32`: own. The sounds: the launcher's own, made for it (Decided, 3); nothing of zutl_zar stays. |
| `gamez/zutil/zutl_path` | `any_case` (music/, zbd/ in any case), `path_from_config` (mw3.cfg's paths), `narrow_paths_utf8` (Windows before 1903), `repair_old_names` (launcher_main). | Own (small). The kit's `rekit/files.h` has none of these. |
| `mech3/data_folder` (and `data_folder_start.h`) | Its own write dir at the start (`resolve`, through the pointer). `family_move`: MicroProse to BattleTech, once. `adopt_nested`: Pirate's Moon's old folder inside MW3's. MY DATA FOLDER, for MW3 and Pirate's Moon (`destination`, `plan`, `move_data`, `remove_old`, `moved_path`). `retarget_config`: the games folder's move. | Kit: `rekit/data_folder`, a general piece that keeps the file names (`data-folder.txt`, `mw3-data-folder.txt`, `mw3-data-copying.txt`) so every copy still reads the same folders. mw3, mw3pm and the launcher each use it. |

**MechWarrior 1.**

| Piece | Used for | Goes to |
|---|---|---|
| `frontend/sdl/mw1_files` (through `mech3/source_archive.h`, which is the kit's archive reader now) | MechWarrior (1989)'s files for the Remake: `search`, `game_of_pick`, `copy_in` into `<write dir>/mw1`, `game_valid`, `has_main`, `in_unpacked`, `unpacked_dir`. Also `original_supported`, for Elthial's port (PLAY ORIGINAL, `kMw1Original`) when it sits beside the launcher. | The Remake: its own copy, `mw1/src/mw1_game_files` (namespace `mw1::files`, without `original_supported`), behind its served setup (b5). Deleted from mech3-re in b5 with `test_mw1_files` (its checks are the Remake's `mw1_game_files_*`) and `tests/mw1_btech_stand_in.h`. The raw port's own path: out in b5 (Decided, 6). |
| The Remake's start | The launcher passes `--install <PM> --mw3-install <MW3> --mw1-game <MW1> --no-launcher`. The Remake's status already says `needs: ["mech3-pm", "mech3", "mech1"]` and keeps the three in mw1.cfg. | Kit: an optional way to hand a needed game's folder (section 2, Needs). Done in b5: a Remake with `"setup": 1` is given `--with mech3=<MW3> --with mech3-pm=<PM>` for its status and its start; its needs are `["mech3-pm", "mech3"]`, never in its `missing`; an older one keeps the three flags. |

**The launcher's own, free of game code: they move with it to the hub.**

- **Updates and downloads:** `upgrades/update` (update.cpp, games.cpp),
  `frontend/sdl/sdl_update` and `sdl_games` (self-update, games on demand,
  links, records). mw3 needs only `record_self` and the version stamp
  (`MW3-PROGRAM-VERSION:`): `record_self` goes to the kit (each game writes
  its own `rebuild-program.txt`), and the stamp stays in each game.
- **Screen and system helpers:** `launcher_ui`, `text_fit`, `font_search`,
  `vulkan_check`, `run_log`, `crash_report`, `log_start`, `sdl_focus`,
  `sdl_child`'s `log_to_file`, and `platform/pointer_hands_off.h`. They are
  small and game-free: the hub keeps copies. `system_pickers.h` and the child
  runner are already the kit's (`rekit/sdl/pickers.h`, `child.h`).
- **Libraries and art:** RmlUi (`third_party/rmlui`), FreeType, SDL3,
  `assets/launcher` (the art, fonts and `.rml`/`.rcss`), and the kit's
  `archive`, `hash` and `status`. SDL3_ttf comes in today only through
  `mw3text`. Check at b6 whether anything still needs it.

**The launcher's settings live in MW3's folder.** The launcher's write dir is
MW3's user folder (`usual_folder()`: BattleTech/MechWarrior 3). Its log
`mw3launcher.log` sits there, and its settings are lines in MW3's own
`mw3.cfg`:

- **It reads:** `install=`, `pm_install=`, `mw1_game=`, `mw3_notes=`,
  `pm_notes=`.
- **It writes:** those, plus `link_<key>=`, `unlinked_<key>=` and
  `games_<key>=`.
- **In Pirate's Moon's folder:** it also writes `install=` in Pirate's Moon's
  own `mw3.cfg` (`pm_wd()`).

**Plan for those settings:**

- **The same folder and file:** the launcher keeps them, through its own line
  editor. An upgrade then moves nothing.
- **The games' own lines:** after (b), `install=` (MW3's and Pirate's Moon's)
  and the Remake's paths are written only by the games, through their
  `--install` or an in-place look.
- **`pm_install=` and `mw1_game=`:** they stop being the source of truth, and
  are read once as a fallback for a copy that has not answered yet.

## 2. What the contract gives, and what MW3 and Pirate's Moon need from it

**What it gives MW1 Rebuild and Inception today:**

- **Status:** `--status` prints one JSON line: `game`, `version`, the files'
  `state` (`ready`, `missing` or `incomplete`), `where`, `data`, `missing[]`,
  `first_run.pending[]`, `needs[]`, `note`, and `setup` (1 when the setup is
  served).
- **Served setup:** `--setup --describe | --search | --look <pick> |
  --install <pick> [--folder <dir>]`. Each prints JSON lines ending with
  `done`. Progress comes as `fraction`, `stage` and `message`. A `cancel` line
  on stdin stops a look or an install and leaves nothing behind. Failures
  come back as a `summary`, with exit 0, 1 or 2.
- **Shown on the launcher's screens:** `open_kit_files`, `kit_look`,
  `kit_looked`, `kit_install`, `kit_ready`.
- **PLAY:** starts the game plain. The launcher hides until the game exits.

**Where MW3 and Pirate's Moon stand:** `mw3 --status` answers (`"mech3"`,
`check_install`, `pending: ["movies"]`) but without `"setup": 1`. `--setup`
opens launcher.cpp's find screen inside mw3. mw3pm answers the same as
`"mech3-pm"`. Neither answers the served verbs. The launcher could not use
them yet anyway: `kit_game(i)` is `i >= 3`.

Each need, and what fills it:

- **Discs, images, archives, an installed copy.** `--look` and `--install` fit
  as they are:
  - `map_source` and `install_from_source` become MW3's recipe
    (`rekit::sdl::Game`: `find`, `describe`, `recognise`, `install`).
  - `in_place` covers a copy already set up.
  - `--search` takes MW3's search and then `find_discs`, so a disc in a drive
    is still found.
  - A Pirate's Moon pick shown under MW3, and the reverse, keeps its sentence
    ("This is MechWarrior 3, not Pirate's Moon...") through `recognise`.
- **GAME FILES' list of every copy** (`find_copies`: each copy's folder, data
  version, whether it is in use, and what it lacks) is missing. Wanted: a
  served `--setup --copies` verb, one line per copy (`path`, `data`,
  `in_use`, `missing[]`), then `done`. It needs new optional fields in
  `SetupMessage` and an optional `copies` hook in `rekit::sdl::Game`.
- **The 1.2 patch, and 1.0 data:** nothing new is needed. The patch is built
  into the game (patch12.h) and applied at install and start. Status `data`
  already says "1.0", "1.2" or "1.0 with 1.2 files". A 1.0 copy's "missing
  the 1.2 patch" goes in `--copies`' `missing`.
- **Music, movies and full sounds from a disc for the files in use** (ADD
  FROM MY DISC on BEFORE YOU PLAY) are missing. Wanted: `--setup --add
  <pick>`, with progress lines, as an optional `add` hook.
- **Movies before the first game** are missing. `pending: ["movies"]` is there,
  but nothing runs the step. Wanted: `--setup --first-run <step>`, with
  progress lines, as an optional `first_run` hook (DESIGN.md's first-run
  steps). The launcher's screen stays as now. A game started alone keeps its
  own first-start import (first_run_import.cpp).
- **BEFORE YOU PLAY's notes** are missing. Wanted: status `notes[]`, plain
  sentences: no music, no movies, the 1.0 data, Pirate's Moon's low sounds.
  The seen-once check (a CRC kept as `mw3_notes=` or `pm_notes=`) stays the
  launcher's.
- **The game's user folder** is missing. MY DATA FOLDER needs it, and so do
  the cross-game mod arguments (`--other-write-dir`, `--other-install`: W15)
  and the media. Wanted: status `user`.
- **Start options** are missing. MW3 and Pirate's Moon are started with:
  - `--install <dir>`, and `--write-dir` when a named or pointed folder is in
    use;
  - `--windowed`, `--window WxH`, `--nosound`, `--hidden` and `--no-import`;
  - `--other-*`;
  - `--ready-file F`, so the launcher closes at the game's first frame, with
    `F.status` lines shown while it loads.

  Kit games are started plain, with the launcher hidden until they exit.
  Wanted: status `play: 1` for a game that takes the shared options and
  `--ready-file`. With it, PLAY passes them and closes at the first frame;
  without it, PLAY is as now. A game finds its own files, so `--install`
  is no longer passed.
- **Needs (the Remake)** is missing. The launcher passes the other games'
  folders. Wanted: an optional `--with <id>=<where>` that the launcher fills
  from each needed game's status `where`. A game started alone does without
  it and uses its own search and mw1.cfg.
- **Progress, errors and cancel:** covered (`SetupMessage`, `kSetupFailed`
  with its JSON reason).
- **Choosing again and the copy in use:** covered (`in_place`, `same_words`).
- **UNINSTALL with the game files:** covered. The game folder and its mark are
  the kit's (`rekit/game_folder.h`), and `where` comes from the status.

Everything above is optional and additive. A launcher that does not know a
field ignores it. A game that sets no hook answers as today. `kStatusVersion`
stays 1, since no field changes meaning.

## 3. The steps, one sitting each

"Touched tests" means the tests of the code the item changes, run under
`run_capped.py`.

### (a) MW3 and Pirate's Moon serve their setup

The launcher is unchanged, so each item ships unseen: the launcher still asks
only kit games (`i >= 3`).

- **a1, kit.** Add the optional fields and hooks from section 2:
  - status `notes`, `user` and `play`;
  - `SetupMessage` copy rows;
  - `rekit::sdl::Game`'s `copies`, `add` and `first_run`;
  - `serve_setup` dispatching `--copies`, `--add` and `--first-run`;
  - `rekit/data_folder` and `record_self` moved in, game-free.

  Tests: the kit's `test_status`, `test_setup_serve`, a new
  `test_data_folder` (mech3-re's 21 checks moved), and mech3-re's
  `test_data_folder` again through the kit.
- **a2, MW3's recipe (mech3-re).** A `rekit::sdl::Game` for MechWarrior 3 over
  `map_source`, `install_from_source`, `install_candidates` and `find_discs`:
  - mw3 answers `--setup --describe | --search | --look | --install`;
  - the status says `"setup": 1`;
  - `install=` is saved after an install or an in-place look.

  Tests: `sdl_program_status_and_setup_answer_the_launcher`, extended to
  each verb on a copy of MW3_GAME_DIR (copied into `<wd>/install`, as the
  launcher's own find does today), and `test_setup_sources`.
- **a3, MW3's extras.** Add `--copies`, `--add <disc>` and `--first-run
  movies`, plus status `notes`, `user` and `play`. Tests: the a2 test with
  these verbs; a cue-sheet disc stand-in for `--add` (as
  `sdl_program_launcher_lists_the_copies` and the notes tests build theirs);
  the importer stand-in (`MW3_IMPORTER`) for `--first-run`.
- **a4, Pirate's Moon (mech3-pm-re).** The same on `pm`, after main merges
  in. Pirate's Moon's differences go in its recipe: `is_pm_install`,
  `add_pm_from_disc`, the full sounds, its words, game `"mech3-pm"`. Tests:
  the same checks in mw3pmsdltests, with PM_AI not involved (no mission
  runs).
- **a5, the Remake (mech1-remake-re).** MechWarrior (1989)'s files as its
  served setup: mw1_files' finder becomes its recipe, keeping the copy into
  `<write dir>/mw1`. Its status gets `"setup": 1`, and `--with` is read for
  its needs. Tests: mw1's ctest `mw1mousesettings` pattern for a new
  `mw1setup` (stand-in BTECH.EXE folders, as `test_mw1_files` has them), and
  `test_mw1_files`' six checks moved there.
  Done with b5 (mech1-remake-re 285cf63ac): `mw1/src/mw1_game_files`,
  `mw1setup`, `test_mw1_game_files`; an empty user folder refused in words.

### (b) The launcher on the contract for every game

- **b0, the games catalog (built in).** A JSON data file lists the families
  and the games, and replaces today's hard-coded tables: games.cpp's table,
  and launcher.cpp's panel rows, badges and descriptions. Each game's entry:
  `key`, `family`, `name`, `description`, `order`; `state` (`coming_soon`,
  `released` or `removed`); `repo`, `tag_prefix` and the asset name for each
  system; `badge` and `card` art; `needs[]`; and `min_launcher`, the oldest
  launcher version that understands the entry. A game's options and notes are
  not in it: they stay in the game's own `--status`. Here the catalog is a
  copy built into the launcher only, with the same entries as today's tables,
  so nothing changes for players. Tests: the built-in catalog read alone
  (every family and game as today), and every `--launcher-shot` screen
  compared byte for byte with the shots from before.
- **b1.** The launcher's own small copies: the `Work` types, the mw3.cfg line
  editor, `crc32`, `any_case` and `path_from_config`. No change in
  behaviour; launcher.cpp stops naming `mech3::setup` types. Tests: the
  `launcher_*` unit tests in mw3sdltests, and every `--launcher-shot` screen
  compared byte for byte with the shots from before.
- **b2.** MW3's and Pirate's Moon's panels from their `--status`: `find_install`,
  `check_install`, `pm_refresh` and `start_check` are replaced, and the notes
  come from `notes`. `kit_game(i)` becomes every game with `"setup": 1`.
  Tests: `run_launcher_with`'s stand-in `mw3` gains a STATUS answer (as the
  kit game's stand-in in `sdl_program_launcher_sets_a_kit_game_up` does), and
  a stand-in `mw3pm` likewise. Then the panel tests:
  - `sdl_program_launcher_shows_the_games_status`;
  - `shows_each_games_versions`;
  - `checks_mechwarrior3`;
  - `checks_pirates_moon`;
  - `notes_fit_above_the_buttons`;
  - `launcher_first_start_notes`.
- **b3.** GAME FILES for MW3 and Pirate's Moon through the served verbs (`look`,
  `install`, `copies`, `add`). The screens are the same code paths as the kit
  games'. Tests:
  - `sdl_program_launcher_finds_the_game_then_plays`;
  - `lists_the_copies`;
  - `game_files_repicks`;
  - `game_files_pirates_moon`;
  - `finds_pirates_moon`;
  - `takes_the_game_folders_own_files_first`;
  - `says_the_picker_is_opening_before_it_asks`;
  - `a_found_install_is_copied_in_first`;
  - `shift_at_start_chooses_the_files_again`.

  Each stand-in answers from a script. The real copy-in is checked by the game
  (a2), not here. Shots: setup, work, notes and found, unchanged.
- **b4.** PLAY: `--first-run movies` before the game, and `play: 1`'s options
  and ready file. Tests:
  - `sdl_program_launcher_converts_the_movies_before_the_game`;
  - `starts_mw3_as_its_own_program`;
  - `waits_for_the_game_in_an_appimage`;
  - `stays_when_the_game_fails_or_is_slow`;
  - `window_goes_on_to_the_game`;
  - `starts_pirates_moon`;
  - `never_runs_away`.
- **b5.** The Remake through the contract, with `--with` for its needs. The
  raw port's own path (Elthial's port as a launcher entry, PLAY ORIGINAL)
  comes out (Decided, 6), with its BTECH.EXE finder. Tests:
  `sdl_program_launcher_starts_mechwarrior_1`, `starts_mechwarrior_1989`,
  `family_cards`.
  Done (2026-11-26). The launcher's MW1 finder and copy (`find_mw1_game`,
  `offer_mw1`, `copy_mw1`, `kMw1Files`, `Work::kMw1Copy`, `mw1_files.{h,cpp}`)
  and the port's path (`port_exe_`, `run_mw1`, `kMw1Original`, PLAY
  ORIGINAL's confirm, the BTECH.EXE check, the shots `mw1found` and
  `mw1origfound`) are out; `starts_mechwarrior_1989` is retired,
  `starts_mechwarrior_1` rewritten for a served and an older Remake, and
  `offers_an_older_remake_its_update` added (an older Remake with no
  `mw1_game=` gets its UPDATE, not a dead PLAY). The port's opt-in packaging
  scripts are still in the tree (docs/launcher.md, MechWarrior 1's files and
  Elthial's port).
- **b6.** The game code leaves the launcher's target. `mw3launcher` links
  rekit, rekit_sdl, RmlUi and its own sources, with no `mw3core`, `mw3sdl` or
  `mw3text`. MY DATA FOLDER runs on the kit's `rekit/data_folder` with the
  game's `user`. The older programs' path from b2 (a status without
  `"setup": 1` gets the launcher's own checks, through `mech3::setup`)
  cannot stay: it collapses to mw3.cfg's `install=` / `pm_install=` read
  unchecked, as the fallback for a copy that answers without `setup: 1`.
  The panels' words go with it: b2's `mw3setup::found_words` calls
  `mech3::setup::game_version_words` (game code), to be replaced by the
  launcher's own copy or the status's own words.
  The launcher's removal of its earlier copy (`use_install` / `use_pm`:
  `<user folder>/install` once GAME FILES moves to another copy) moves into
  the game's `--install`, which saves `install=` and so knows the old copy,
  with the same guard: only a folder carrying our mark (`mw3-install.txt`)
  is ever removed, never anything else `install=` pointed at.
  **Open at b6: the copy PLAY passes.** Through b4 the launcher still starts
  MW3 with the copy in use (the status's `where`) and Pirate's Moon with
  `--install <where> --no-launcher`. It cannot drop the copy until this is
  settled: mw3's and mw3pm's own find at a start (own game files, then
  mw3.cfg's `install=` and the installer's places) is not the status's
  order (`mw3_status_find`: `games_mw3=` place, own game files, mw3.cfg,
  `<user folder>/install`, the search, the development copy), so with a
  `games_mw3=` place set, or a copy only in `<user folder>/install`, a start
  without the copy opens another copy than the panel shows. Two ways, for
  the owner to pick (the first changes a plain start):
  - the start's find uses `mw3_status_find` when no copy is given, so a
    plain start and the launcher agree (recommended);
  - the contract carries the copy back generically: an optional "pass
    where" in `play: 1`, so the launcher hands any kit game its status's
    `where` in the same form.
  Nothing changes in a plain start before then.
  **Decided 2026-11-26: the first.** mw3's start with no copy given takes
  `mw3_status_find`'s (`find_install`, and the player's plain start: a copy
  the status offers goes to the find screen to be copied in, an incomplete
  one straight in so the game's own box says what is missing), and PLAY
  passes no copy to a game whose status serves it (`setup: 1`): MW3 gets
  no install and mw3.cfg's `install=` is not rewritten; Pirate's Moon gets
  `--no-launcher` without `--install`. An older program is still handed its
  copy. (b6 piece 2.)
  By then MW3 and Pirate's Moon releases that answer `setup: 1` have
  shipped with the new launcher (one release), so only a player who has
  not updated a game sees the thinner panel, and it offers UPDATE.
  Checks:
  - the link walk above shows no game object;
  - the size is noted;
  - the launcher's tests once, as one batch: the 50 named `*launcher*`,
    `package_*`, `plain_start`, `status_and_setup`, `data_folder_*`,
    `finds_music_*` and `names_stay_*`;
  - `test_update`;
  - the mingw cross-check of the touched files.

  The menu's sounds are the launcher's own (Decided, 3): a soft click and a
  hover sound, two WAV files in `assets/launcher`, made with the sound-effect
  generator on the Mac (Crucible, its audio generation's small-SFX model;
  check it is not busy first). They are committed with a line saying how
  they were made, prompt included. zutl_zar's sound read leaves the launcher;
  only `crc32` stays, as the launcher's own.
  `sdl_program_launcher_plays_the_menus_sounds` then plays those, with no
  game files. MW3 alone's Linux AppImage keeps passing `--status`, `--setup`
  and `--ready-file` to the mw3 beside it.
- **b6's plan (2026-11-26, after b5).** First, the owner's question (above): with
  no copy given, does a plain start find the copy the panel shows
  (`mw3_status_find`, recommended), or does `play: 1` carry `where` back? Until
  it is answered PLAY keeps passing the copy, and b6 can go ahead without it.
  Answered 2026-11-26: the plain start finds it (piece 2, above).
  Every piece of game code still in `mw3launcher`'s target, as of b5:
  - **Links and build:** `mw3core`, `mw3sdl`, `mw3text` (and `mw3rmlui`, SDL3);
    `add_dependencies(mw3launcher mw3)`; `MW3_GAME_DIR` in development builds.
    Go: links become rekit, rekit_sdl, RmlUi and its own sources. The
    dependency and the define go, and the tests' stand-ins take their place.
  - **`launcher_mw3_setup.{h,cpp}` → `mech3/game_setup`** (`check_install`,
    `start_check`, `found_words` / `start_found_words` through
    `game_version_words`, `find_discs`, `add_from_disc`, `add_pm_from_disc`,
    `apply_patch`, `install_from_source`): the older programs' path from b2
    and b3. Collapses: for a status without `setup: 1`, `install=` /
    `pm_install=` are read unchecked and the panel offers UPDATE. Discs,
    patches and sources go only through the game's served `--setup`. Both
    files are deleted.
  - **`mech3/source_map`** (`map_source`, `Found`): `is_pm_install` in
    `find_pm`'s search, the pick's check and the files search. Collapses:
    Pirate's Moon's `where` from its status (b2). A pick is classified by the
    game's `--setup --look`, and an older PM's `pm_install=` is read
    unchecked.
  - **`mech3/data_folder` through `data_folder_start.h`** (`usual_folder`,
    `data_folder_at_start`, `resolve`, `plan`, `move_data`, `destination`,
    `remove_old`, `moved_path`; `retarget_config` in sdl_games): MY DATA
    FOLDER and the start's family move. Goes to the kit's `rekit/data_folder`
    with the status's `user` (as above).
  - **`mech3/patch_import`'s `sha256_file_hex`** (sdl_games, sdl_update: a
    download's checksum). Goes to the kit's `rekit/hash.h`.
  - **`gamez/zutil/zutl_zar`** (`zar::open/find/read`: the menu's sounds,
    read from the game's own archive; `crc32`). Goes: the launcher's own two
    WAVs (Decided, 3). `crc32` is the launcher's own copy (b1).
  - **`gamez/zutil/zutl_path`** (`repair_old_names` in launcher_main; the
    rest has had its own copies since b1). Goes to the launcher's own copy.
  - **`gamez/zrender/zrndr_tint`**: only included now (the cards dropped the
    ripple). The include and launcher.h's comment go.
  - **`upgrades/update`** (`games.cpp`, `update.cpp`: the catalog, `judge`,
    `swap_in`, the versions): not game code, but built in `mw3core`. Goes to
    the launcher's own sources, and to the hub with them in (c).
  - **mw3sdl's `sdl_install`** (`find_install`, `install_candidates`,
    `install_valid`, `pick_source`, `save_install`, `media_dir`): MW3's own
    find, for the older path. Collapses with `launcher_mw3_setup`.
    `save_config_value` is already b1's line editor. `media_dir` comes from
    the status's `data`.
  - **mw3text's `launcher_notes`** (`have_music`, `have_movies`,
    `launcher_notes`; it includes `mech3/data_version.h`): the older path's
    notes. Collapses: the notes come from the status's `notes` (b2), and an
    older game has none.
  - **mw3text's `movie_import`** (`importer_path`, `movies_missing`,
    `run_importer`): the launcher's own conversion for an older MW3.
    Collapses: the served `--first-run movies` (b4). An older game converts
    on its own start.
  - **No game code, but built in the game's libraries:** `log_start`,
    `sdl_child`, `sdl_focus` (mw3sdl); `font_search`, `text_fit`,
    `vulkan_check` (mw3text); `crash_report.cpp` (built into mw3 too). They
    go to the launcher's own sources, or to rekit_sdl where another game
    wants them.
  - **Checks**, as in b6 above: the link walk, the size, the launcher's
    tests as one batch, `test_update` and mingw.
  - **Shots** change where an older game's panel loses its found words.
    They get listed when b6 is done.
  - **Piece 1 done (2026-11-26): the older path behind `MW3_GAME_FIND_SCREEN`.**
    mw3's (and mw3pm's) own find screen is still launcher.cpp's, so the older
    path cannot be deleted before d1 replaces that screen with the kit's
    `run_setup`. Until then it is compiled only where `MW3_GAME_FIND_SCREEN`
    is defined: the `mw3` target (and `mw3pm` on `pm`), never `mw3launcher`,
    which no longer builds `launcher_mw3_setup.cpp`. Each block is a whole
    function or a whole branch, with the launcher's collapsed version in its
    `#else`: an older MW3 or Pirate's Moon (no `setup: 1`) gets its status's
    ready `where`, else mw3.cfg's `install=` / `pm_install=`, unchecked, with
    no notes and no movie conversion; GAME FILES, and PLAY with no copy, say to
    update it, or start its UPDATE when one is known (`older_game`).
    `found_words`, `start_found_words` and the version words are the
    launcher's own (`launcher_own`). The find screen's checks run through the
    game's program (`mw3 --launcher-shot F --launcher-screen S`; mw3pm's on
    `pm`, `MW3PM_BIN`), and `takes_older_games_unchecked` runs untagged
    stand-ins through mw3launcher. What the launcher checked for Pirate's
    Moon before is mw3pm's now, tested against it on `pm` (`MW3PM_BIN`,
    outside the PM suite's MW3-only regions): `sdl_pm_status_checks_its_copy`
    (its `--status`), `sdl_pm_find_screen_game_files` (install, an MW3 pick,
    ADD FROM MY DISC, no music), `sdl_pm_find_screen_lists_its_copy` and
    `sdl_pm_find_screen_notes_fit_above_the_buttons`.
    Shots (tools/launcher/shots.sh, both builds in one folder): 8 of 62
    change, all setup A's Pirate's Moon, an untagged stand-in, so an older
    program: its panel line loses "data 1.0" (main, main_big, chifiles,
    updatebox), its GAME FILES row its version and lacks (game_pm), its copies
    list its copy (pmcopies), its notes screens their note (pmdisc, pmnotes).
    game_mw3 in both setups differs only in the program path drawn on it.
    **d1: delete every `MW3_GAME_FIND_SCREEN` block** (the `#if` side, keeping
    the `#else`), the define in CMakeLists.txt, `launcher_mw3_setup.{h,cpp}`,
    and mw3's `--launcher-shot` once the find screen is the kit's.
  - **Piece 2 done (2026-11-26): PLAY passes a served game no copy.** mw3's
    plain start finds the copy its status shows (`mw3_status_find`: the
    game's own folders, mw3.cfg's `install=`, the search), saving only a
    searched one to mw3.cfg, never the game folder's own files; mw3pm is
    given no `--install`. A copy only offered (an older copy beside own
    folders) opens the find screen with it in view. An older program still
    gets its copy passed. The launcher's checks of the start read the copy
    in use from its log (`started_on`), and
    `sdl_program_plain_start_takes_the_copy_the_status_shows` checks the
    plain start against `--status`.
  - **Piece 3 done (2026-11-26): `mech3/source_map`** is included, and the
    last look's `found_` kept, only under `MW3_GAME_FIND_SCREEN`.
  - **Piece 4 done (2026-11-26): the data folder on the kit.** The launcher's
    start and MY DATA FOLDER call `rekit::data_folder` directly
    (`launcher_folder.{h,cpp}`, the launcher's own: `usual_folder`,
    `data_folder_at_start` and MechWarrior 3's `Game` record, the same one
    `mech3::data_folder` gives the kit); sdl_games' games-folder move calls
    `rekit::data_folder::retarget_config`. The launcher no longer adopts
    Pirate's Moon's old nested folder (`adopt_nested`, a Windows launcher's
    path up to 1.3.1): mw3pm does at its every start, its `--status`
    included, before the launcher reads where its data is. mw3 and mw3import
    keep `data_folder_start.h`.
  - **Piece 6 done (2026-11-26): zar, zutl_path and zrndr_tint leave.** The
    menu's sounds are `assets/launcher/hover.wav` and `click.wav` (Stable
    Audio 3.0 Small SFX on the Mac's Crucible; prompts, seed and processing
    in assets/launcher/README.md), loaded from the first screen with no
    install; every package carries them (make_app.sh, make-appimage.sh,
    package.cmd), and check_dist.ps1 allows those two `.wav` and no other.
    `repair_old_names` is the launcher's own copy (`launcher_own`), and the
    `zrndr_tint` include and launcher.h's ripple line are gone.
  - **Piece 7 done (2026-11-26): `upgrades/update` is its own library**,
    `mw3update` (update.cpp, games.cpp and the built-in games.json), with no
    game code: mw3core links it for the mods' needs (`mod_needs.cpp`), and
    the launcher links it directly from piece 8.
  - **Piece 8 done (2026-11-26): the links.** `mw3launcher` links
    `rekit_sdl` (and rekit with it), `mw3rmlui`, `mw3update` and SDL, plus
    Foundation on the Mac (sdl_games' Dock and Finder calls), and none of
    `mw3core`, `mw3sdl` or `mw3text`. The game libraries' files with no game
    code that it used are compiled into it too: `log_start`, `sdl_child`,
    `sdl_focus` (mw3sdl), `text_fit`, `vulkan_check` (mw3text). sdl_install's
    `media_dir` (the notes' music folder, shortened to fit) is the
    launcher's own (`launcher_own`), and `sdl_install.h` is included only
    under the switch. No `add_dependencies(mw3launcher mw3)` and no
    `MW3_GAME_DIR` for it (its dev copy was read only by the older path).
    A static libarchive's `bcrypt` (Windows) is linked onto the kit's target,
    as mw3core had it. The link walk: no `mech3::`, `zutl`, `zrndr` or other
    game symbol in the binary; 10.8 MB on the Mac, 27.5 MB before b6.
- **b7, the games catalog (fetched).** After b0, and it needs nothing else
  from (b), so it may move earlier. A newer catalog is fetched from mw3-re
  (its main, or a release asset) the same way as the update check: the
  mirror's first, then GitHub's. The last one fetched is cached, so the
  launcher works offline. It is fetched without asking the player: it is
  data, and the request sends nothing about them. Where (2026-11-26): the
  file `catalog/games.json` on mw3-re's main, read raw (the mirror's copy,
  then `raw.githubusercontent.com/telltaleatheist/mw3-re/main/catalog/games.json`),
  its art in `catalog/art/`. The file goes onto mw3-re's main with (c);
  until then both answer "not found" and the built-in catalog is used. A
  change to it is a `revision` one higher than the last. How entries act:
  - `removed` hides the card for new players. An installed game stays and
    keeps working: no catalog change ever touches installed files.
  - A new game, a description change or a release needs no launcher build.
  - Trust: an entry whose `repo` is not under the owner's GitHub account is
    ignored, and so is one whose `min_launcher` is newer than this launcher.
    Downloads are still checked against SHA256SUMS. The art comes from the
    same repo and is cached beside the catalog. No signing for now.

  Tests: a fetched catalog that adds a game, flips one from `coming_soon` to
  `released` and removes one (an installed copy stays and still plays); a
  bad entry (another owner's repo, a too-new `min_launcher`) ignored while
  the rest is read; offline with a cache; offline with none (the built-in
  copy). The fetch goes to a `file://` stand-in, as `test_update`'s releases
  do (`MW3_GAMES_API`).

**The tests that start mw3 beside the launcher.** Today they run the real
setup in the launcher, on MW3_GAME_DIR, behind a shell stand-in for mw3
(`run_launcher_with`). After (b), the launcher's tests use stand-ins only:

- **The stand-ins:** the `mw3` and `mw3pm` scripts answer `--status` and the
  served verbs from fixed JSON, as the kit game's stand-in already does.
- **What is checked:** what the launcher shows and passes on.
- **What moves to the games:** what is copied, patched or converted moves to
  mech3-re's and mech3-pm-re's own `--setup` tests.
- **Still needing game files:** none. The menu-sounds test plays the
  launcher's own sounds (b6), so the launcher's tests run where no game
  files are.

### (c) The launcher's code to mw3-re main

- **c1, the frozen tree to an archive (Decided, 1).** The hub's main still
  holds the whole tree as it was frozen on 2026-11-21. That tree goes to an
  archive branch and a tag, and main becomes the launcher only. The README's
  promise ("Its code is frozen as it was at the split, so old clones and
  links keep working") is reworded to name the archive branch and tag, so
  old links still have somewhere to land.
- **c2, the tree.**
  - **Code:** the launcher's sources and its tests (a `tests/launcher` suite,
    stand-ins only), `assets/launcher`, `third_party/rmlui`, and the kit as a
    submodule.
  - **Packaging:** the launcher-only package scripts (combined.cmd and .sh,
    installer.iss, make_app.sh with `MW3_PKG_LAUNCHER_ONLY`, make-appimage.sh,
    check_dist).
  - **Version:** `git describe --tags --match "v[0-9]*"` on the hub. The hub
    already carries tags v1.5.0 to v1.5.2, so its next tag (v1.6.0) sorts
    above every installed launcher.

  Tests: the launcher suite on the Mac, then on the PC and in WSL (hidden
  starts only), and check_dist for each package.
- **c1 and c2's code, done (2026-11-26, on the hub).** The branch
  `archive/frozen-2026-11-21` and the tag `frozen-2026-11-21` hold the tree
  as it was (bb2107585); main is the launcher only. From mech3-re e077a60b9
  (and 6866e21a6's owner compare): the launcher's sources, `upgrades/update`,
  RmlUi, `assets/launcher` and the kit at f3d3a5b. Changes from mech3-re's
  copy:
  - launcher.cpp without its `MW3_GAME_FIND_SCREEN` blocks (`unifdef`, the
    games' own find screen stays theirs until d1);
  - sdl_games.cpp takes the mark from the kit (`rekit::kGameFolderMark`, the
    same file name) instead of sdl_install.h;
  - font_search.cpp in the launcher on Linux, with SDL3_ttf: launcher.cpp's
    `font_path` calls it there, and mech3-re's `mw3launcher` has lacked it
    since b6 piece 8 took mw3text away.
  The tests are `launchertests`: test_update, test_log_start and 30 of
  test_sdl_frontend's launcher tests (`tests/launcher/test_launcher.cpp`).
  The stand-in `mw3` (`kAsksAsMw3`) answers `--status` and
  `--setup --describe` from fixed text, as mw3 answers them (a ready copy,
  1.2 data, the music note until the write dir has a track), where it ran
  the real mw3 before. Two update tests' `install=` names a folder of the
  test's own, and the hashes are the kit's names.
  **Left in mech3-re for now:** the launcher tests that read MW3_GAME_DIR or
  run the real mw3. They move in d2, each rewritten to stand-ins or its
  copying and checking moved to the games' `--setup` tests:
  `launcher_own_copies_do_as_the_games`,
  `launcher_one_picker_takes_a_file_for_its_folder` (sdl_install's
  `pick_source`), `sdl_menu_clicks_land_on_the_same_buttons_at_any_window`,
  `sdl_program_launcher_notes_fit_above_the_buttons`,
  `sdl_program_launcher_says_its_version`,
  `sdl_program_mouse_settings_stay_across_starts`,
  `sdl_program_data_folder_pointer_and_move`,
  `sdl_program_launcher_window_goes_on_to_the_game`,
  `sdl_program_launcher_takes_the_game_folders_own_files_first`,
  `sdl_program_launcher_finds_the_game_then_plays`,
  `sdl_program_launcher_lists_the_copies`,
  `sdl_program_launcher_takes_older_games_unchecked`,
  `sdl_program_launcher_links_and_uninstalls`,
  `sdl_program_launcher_converts_the_movies_before_the_game`,
  `sdl_program_a_found_install_is_copied_in_first`,
  `sdl_program_shift_at_start_chooses_the_files_again`,
  `sdl_program_launcher_game_files_repicks`,
  `sdl_program_launcher_starts_mechwarrior_1`,
  `sdl_program_launcher_offers_an_older_remake_its_update`,
  `sdl_program_launcher_starts_pirates_moon`,
  `sdl_program_launcher_finds_pirates_moon`,
  `sdl_program_launcher_checks_mechwarrior3`,
  `sdl_program_launcher_offers_its_own_update_at_start`,
  `sdl_program_package_layouts_find_both_games`,
  `sdl_program_package_upgrade_keeps_the_players_files`,
  `sdl_program_launcher_shows_each_games_versions`,
  `sdl_program_launcher_game_files_pirates_moon`,
  `sdl_program_launcher_checks_pirates_moon`,
  `sdl_program_launcher_panels_from_the_status`,
  `sdl_program_launcher_game_files_through_the_game`,
  `sdl_program_launcher_follows_a_moved_folder`,
  `sdl_program_splash_keeps_its_shape_on_a_wide_screen` and
  `sdl_pm_find_screen_lists_its_copy`.
  `sdl_program_plain_start_goes_straight_in_or_to_setup` stays mw3's (d2).
- **c3, an upgrade over 1.5.2 on each system,** hidden: the Setup over an
  installed 1.5.2 on the PC, the DMG's swap-in on the Mac, and the AppImage's
  swap on Linux. Each keeps the BattleTech folder, `mw3.cfg`'s lines and the
  linked and installed games.
  `sdl_program_package_upgrade_keeps_the_players_files` and
  `package_layouts_find_both_games` run on the hub.
- **c4, the release.** With every name and ID kept (section 4), no bridge is
  needed. Every launcher ever installed already asks mw3-re (all copies) or
  mw3-re and mech3-re (the copies since the split). So the first hub-built release is an
  ordinary one, published on mw3-re. A bridge is needed only if a name
  changes (for instance BattleTech-Setup-*). It would be one last
  `MW3-Rebuild-*` release whose launcher takes both names. From c4 on,
  mech3-re's releases carry no `MW3-Rebuild-*` asset; otherwise a launcher
  built from mech3-re's copy with a higher tag would win the update check.
  **The new launcher and the new Remake ship together** (b5): the launcher's
  MW1 finder is gone, so an older Remake whose player has no `mw1_game=` yet
  can only be offered its UPDATE. Publish the Remake with `"setup": 1`
  (mech1-remake-re 285cf63ac or later) in the same release as the first
  launcher with b5.

### (d) The old copy out of mech3-re

- **d1, mw3's and mw3pm's own find screen (Decided, 2).** Today it is
  launcher.cpp's (`setup_only`). It becomes the kit's `run_setup` with MW3's
  look (the launcher's art and font), as MW1 Rebuild and Inception use, over
  the recipe a2 made. Its screens are redrawn without RmlUi, so they are
  close, not identical. Before d1 lands: shots of the old screen against the
  new, side by side. A game started alone without its files uses this
  screen itself. Two kit hooks it wants (small, optional, general):
  - recognise a folder the player picks as a copy of the game without a full
    look, so the launcher can ask a game about a pick cheaply;
  - words for a find of another game the rules know (Pirate's Moon picked
    under MechWarrior 3), given the Found, not only the path. a2 carries that
    sentence through a flag set by the rules' judge, which holds for one
    served ask a process, not for a screen's several looks.
- **d2.** The `mw3launcher` target comes out of mech3-re, with launcher.cpp,
  launcher_ui, sdl_update, sdl_games, upgrades/update, and their tests (moved
  to the hub in c2). mw3 keeps `record_self` (now the kit's) and its version
  stamp. Only the art the theme uses stays. Tests: mw3sdltests and mw3tests'
  touched tests, the d1 screen's shots, and
  `sdl_program_plain_start_goes_straight_in_or_to_setup`.
- **d3, MW3 alone's packages without the launcher.**
  - **Windows:** the zip and Setup start `mw3.exe`.
  - **Linux:** the AppImage's entry is `mw3`, and the exec trick goes.
  - **Mac:** the app's executable is `mw3`.
  - **IDs:** MW3 alone needs IDs of its own: today it shares the launcher's
    AppId and bundle ID (section 4).
  - **The launcher's download:** check that games on demand still finds the
    program in MW3 alone's dmg and AppImage (`sdl_games`' unpack, and
    `identify` by the stamp) before d3 ships.

  Tests: check_dist and a hidden start of each package.
- **d4, Pirate's Moon (mech3-pm-re).** The same as d1 to d3 on `pm`
  (`LauncherOptions::pm`), after main merges in.

## 4. The update chain and the IDs, as the scripts have them

**The launcher's self-update** (`sdl_update.cpp` `check_for_updates`,
`update.cpp`):

- **Where it asks:** the releases of `mw3-re` and `mech3-re`, each first at
  the mirror's releases API, then GitHub's (40 releases a list).
  Copies up to 1.2.1 ask mw3-re only. The user agent is
  `MW3-Rebuild-launcher`.
- **What it takes:** the newest release that is not a draft or a prerelease,
  with a bare `vX.Y.Z` tag (`package_tag`) and this system's asset
  (`platform_asset`):
  - Windows: a name starting `MW3-Rebuild-Setup-` and ending `.exe`;
  - Mac: starting `MW3-Rebuild-` and ending `-mac.dmg`;
  - Linux: starting `MW3-Rebuild-` and ending `-x86_64.AppImage`.

  Its line in the release's SHA256SUMS is checked (`sums_line`). A game's own
  download (MW3-alone, PM-Rebuild and the rest) is never taken.
- **Compared with:** the program's `MW3_VERSION`, which is CMake's `git
  describe --tags --match "v[0-9]*" --always --dirty`.

**How it installs itself:**

- **Windows:** it starts the new Setup (Inno Setup), and the launcher quits.
  - The ID and names: `AppId={{920C98A6-550D-41A3-839E-540B93B8F40D}`,
    `AppName=MechWarrior 3 Rebuild`, and
    `DefaultDirName={autopf}\MechWarrior 3 Rebuild`.
  - The program: `UninstallDisplayIcon={app}\mw3launcher.exe`, and the last
    page's `[Run]` is `{app}\mw3launcher.exe`.
  - The output: `OutputBaseFilename={#Stem}-Setup-{#AppVersion}`, where
    `Stem` is `MW3-Rebuild` (the combined package; the shortcut is
    "BattleTech") or `MW3-alone` (the shortcut is "MechWarrior 3"). **Both
    use the same AppId**, and MW3 alone's `[InstallDelete]` removes mw3pm.exe
    and mw1.exe.
- **Mac:** it mounts the DMG and takes the app holding
  `Contents/MacOS/mw3launcher` (`is_our_app`). That app replaces the running
  one at its path (`swap_in`), and it restarts
  `<app>/Contents/MacOS/mw3launcher`.
  - The app is `MechWarrior 3.app` (`NAME`; the folder is "the updater's
    path").
  - `CFBundleIdentifier` is `com.owenmorgan.mw3rebuild`, and
    `CFBundleExecutable` is `mw3launcher`.
  - The name shown is "BattleTech" through `en.lproj/InfoPlist.strings` for
    the combined and launcher-only packages.
  - `install_app.sh` refuses an app with another bundle ID. MW3 alone's app
    has the same ID and executable today.
- **Linux:** the downloaded AppImage replaces `$APPIMAGE` in place, and starts
  again.

**What must stay for an in-place upgrade:** the three asset name patterns, the
`v` tag form, the AppId and install folder, `mw3launcher(.exe)` as the
program's name, `MechWarrior 3.app` with its bundle ID, and the folders
below.

**The folders:**

- **The launcher's and MW3's user folder:** `SDL_GetPrefPath("BattleTech",
  "MechWarrior 3")`. That is `%APPDATA%\BattleTech\MechWarrior 3`,
  `~/Library/Application Support/BattleTech/MechWarrior 3`, or
  `~/.local/share/BattleTech/MechWarrior 3`. Its pointer is `data-folder.txt`
  and its marker `mw3-data-folder.txt`. It holds `mw3launcher.log` and the
  launcher's lines in `mw3.cfg` (section 1). A player's older
  `MicroProse/<Game>` is moved once (`family_move`).
- **Pirate's Moon's:** `BattleTech/Pirate's Moon`, and the Remake's is
  `BattleTech/MechWarrior 1 Remake` (mw1.cfg).
- **The games folder** (games on demand): `%LOCALAPPDATA%\BattleTech\games`,
  `~/Library/Application Support/BattleTech/games`, or
  `$XDG_DATA_HOME/BattleTech/games`. The 1.3.x
  `MicroProse/BattleTech/games` is moved once. It can be moved by the player
  (`mw3.cfg` `games_<key>=`).
- **The marks and records:**
  - `rebuild-game.txt`: a game the launcher installed;
  - `rebuild-game-folder.txt`: a game's own folder (`game=<key>`);
  - `rebuild-program.txt`: each game's record in its user folder, read by
    path;
  - the program's stamp `MW3-PROGRAM-VERSION:<tag>`, read and never run.

**Each game's download** (`games.cpp`, `game_asset_name`):

| Game | Repo | Tag | Windows | Mac | Linux |
|---|---|---|---|---|---|
| MechWarrior 3 | mech3-re | v | MW3-alone-<tag>-win64.zip | MW3-alone-<tag>-mac.dmg | MW3-alone-<tag>-x86_64.AppImage |
| Pirate's Moon | mech3-pm-re | pm-v | mw3pm-<tag>-win64.zip | PM-Rebuild-<tag>-mac.dmg | PM-Rebuild-<tag>-x86_64.AppImage |
| MW1 Remake | mech1-remake-re | mw1-v | MW1-Remake-<tag>-win64.zip | MW1-Remake-<tag>-mac.dmg | MW1-Remake-<tag>-x86_64.AppImage |
| MW1 Rebuild | mech1-re | mw1r-v | MW1-Rebuild-<tag>-win64.zip | ...-mac.dmg | ...-x86_64.AppImage |
| Inception Rebuild | inception-re | chi-v | CHI-Rebuild-<tag>-win64.zip | ...-mac.dmg | ...-x86_64.AppImage |
| Inception Remake | inception-remake-re | chirm-v | CHI-Remake-<tag>-win64.zip | ...-mac.dmg | ...-x86_64.AppImage |

MechWarrior 3's tags share the launcher's `v` form. That is safe while only
the launcher's releases carry `MW3-Rebuild-*` assets (c4). The games check
reads only mech3-re for MechWarrior 3, and takes only `MW3-alone-*`.

## Open questions (before the steps they block)

1. **Before c4:** do the names stay (no bridge), or does the launcher get its
   own names (one bridge release)? Recommended: they stay.

## Decided (2026-11-25)

1. **The hub's frozen tree:** main becomes the launcher only; the frozen tree
   goes to an archive branch and tag (c1).
2. **mw3's and mw3pm's own setup screens:** the kit's `run_setup` with MW3's
   look, shots of old against new before d1.
3. **The menu's sounds:** MW3's sounds go from the launcher (they were a
   jarring sound for it). It gets its own soft click and hover sound, made
   with the sound-effect generator on the Mac and committed with how they
   were made, prompt included; no third-party files (b6).
4. **The shape:** the kit covers every game, MW3 and Pirate's Moon included.
   Each game's own extras live in its repo. The launcher drives a game's setup
   by asking the game's program. A game started alone without its files uses
   the kit's setup itself.
5. **A games catalog:** a data file of the families and games, built in and
   fetched, so a new game or a release needs no launcher build (b0, b7).
6. **The raw port (2026-11-25):** its own launcher path comes out in b5. The
   game players see is "MechWarrior 1: Reverse Engineered", "MW1 Rebuild" for
   short (-re stands for reverse engineered). The port's credit stays in its
   repo, not in player text. b0's built-in entry carries the same name and
   blurb as the panel row on main.

# Mods

Ours (Owen, 2026-11-01: "players and hobbyists can add their own game data
without any change to the exe later"; small, "nothing big and dramatic").
With no mods enabled nothing is mounted and nothing in play changes; nothing
is ever written into the install or into a mod.

Every key each file a mod can change takes, with its type, default and
limits: `docs/modding-reference.md`, generated from `src/upgrades/reference/`
(`mw3 --reference docs/modding-reference.md`; the test `reference_*` fails
when a reader and its registry disagree or the document is stale). The
check runs every campaign mission and t1 for 30 frames with zrdr's
observers on: each file loaded is mapped node by node (until freed), and
each lookup counts for the file whose node it searched, so every module
that reads a file is covered, not only its reader; a file's own names (a
weapon's, a record's) are declared per file and left out. A key the code
reads but no shipped mission reaches is marked unchecked in the document.
Covered so far: mod.txt, custom campaigns, weapons.zrd, the mech
definitions, aiv.zrd, targets.zrd, the AI nets and regions, valves.zrd,
objectives.zrd, aiTurrets.zrd, vehicle.zrd, world.zrd, landmines.zrd,
hud.zrd, sounds.zrd, the dialogs (the check also opens every main menu
screen and the options' pages for them), the script commands, and mech3ax's
JSON forms (motion, mechlib, anim, gamez: the members our readers take,
recorded while they read unzbd's export of every shipped file, listed
compactly since their meanings are mech3ax's schema; skipped without
mech3ax beside the checkout). A file's own names are declared per file (`names_at`: a path,
`@top` for a record asked of the root by name, `m<n>` for the missions'
blocks, `any:KEY` for the list after KEY wherever it stands); a key the
registry lists is a key wherever it stands.

## A mod (M1, 2026-11-01)

A folder in the settings folder's `mods\` (the write dir: on Windows
`%APPDATA%\MicroProse\MechWarrior 3\mods`, on the Mac `~/Library/Application
Support/MicroProse/MechWarrior 3/mods`), laid out like the install
(`zbd\...`, `video\...`), with a `mod.txt`:

```
name=Hard Rain
version=1.0
author=Someone
for=mw3 1.2
description=Rain on every world.
```

`name` is required; `for`, when given, must have `mw3` as its first word, a
version after it if any (a Pirate's Moon mod, `for=mw3pm`, is left out of MW3). Other keys are ignored, `#` starts a comment.

**Enabling.** `mw3.cfg` in the settings folder, one line a mod, the folder's
name, in order (the first wins where two hold the same file):

```
mod=hardrain
mod=newmaps
```

`mw3 --mod NAME` (repeatable) uses that list for one run instead,
`--no-mods` none, `mw3 --mods` lists the mods found and which are enabled.
At start the log names each enabled mod and every file it replaces or adds;
a name that is no usable mod (no folder, no `mod.txt`, no `name`, another
game) is said and left out.

**Where it sits.** The settings folder's own files first (what the game
saved: a player's files always win), then the enabled mods in order, then
the imported 1.2 patch (`patch12\`), then the install (`zutl::mount_mod`,
assets-layer.md). The world folders a mission adds (`zbd\<world>\`) come
from every mount, the mods' too.

**Game data only.** A mod never serves the player's own files: `pilots\`,
`configs\` (saved loadouts), `keys\` (controls), `saves\`, `mw3.cfg`,
`settings.reg`, the logs and `movie-shapes.txt` (`zutl::mod_may_serve`).
Such files in a mod are listed as ignored.

**Replacing.**

- A whole file: the mod's copy of any game file (an archive such as
  `zbd\c1\gamez.zbd`, `zbd\rmechtex.zbd`, `zbd\reader.zbd`) is read instead.
- One entry of a reader archive, as text (ours, mods only): a loose `.zrd` in
  the mod, in the archive's own folder, is read instead of that archive's
  entry (`zbd\fmv.zrd` for `zbd\reader.zbd`'s fmv.zrd; `zbd\c1\world.zrd` for
  the c1 world's reader). The game's own rule (zreader.md) reads a loose file
  only when no archive has the name; inside a mod the loose copy comes first.
  Each such file is named in the log; one that does not parse leaves the
  archive's entry in use (Readable files read directly, below).
- mech3ax (TerranMechworks, `unzbd` / `rezbd`) unpacks the archives to JSON
  and PNG and packs them again byte for byte: reader, image and texture,
  sound, interp, mechlib, motion, gamez and anim archives. It reads
  Mech3Msg.dll but cannot rebuild it (M2's `strings.txt` is the way to change
  text).

**Multiplayer.** Every player's gameplay data must match. A number over the
enabled mods' gameplay files (`mods_gameplay_hash`) travels in the lobby's
player data, with the names of the mods that count (`mods_identity`, set at
start-up). A joiner whose number differs from the host's is sent back with
"Your mods don't match the host's. The host uses: X. You use: Y.", and LOCK
refuses while any player's differs, naming theirs and ours. Strings,
images, sounds, music and movies do not count, so a translation or a
texture pack never blocks a game; with none on either side nothing changes.
The block's layout: multiplayer-port.md, the mods check.

Tests: `test_mods.cpp` (mod.txt; the folders found; the mounts in order,
the first winning, unusable names left out, nothing served for pilots or
mw3.cfg, writes to the settings folder; a loose fmv.zrd over reader.zbd's,
the archive's own without the mod or with a broken copy; the gameplay
number ignoring textures, sounds, strings and music).

## New worlds and words (M2, 2026-11-01)

**A new world.** `world=<folder>` in mod.txt (as many as the mod has):

- the world's files in the mod's `zbd\<folder>\`, as an install's world
  folder holds them (`gamez.zbd`, `anim.zbd`, `reader.zbd`, `readerm<n>.zbd`
  for missions, `readeria<n>.zbd` for Instant Action, `readermp<n>.zbd` for
  multiplayer);
- its build script `<folder>.gs` at the mod's root, a text script read
  where `interp.zbd` has none (the game's own fallback, interp.md). It can be
  a whole script or `source` another world's (`source c1.gs`).

The game's worlds are the nine folders of 0x00484c50 (c1 c2 c3 c4 c4b t1 lab
dev1 dev2, by number); a mod's are numbered on from 9 in mod order
(`mission_dir`, `mod_worlds()`). A folder name that is already a world (any
case), has a slash, dot or space, or has no `zbd\<folder>\gamez.zbd` in the
mod is said and left out.

**Instant Action.** `ia_world=<folder> <name>` puts a world on Instant
Action's list after the original's four, named `<name>` (UTF-8, shown in the
game's code page; the folder when none). The world can be the mod's own or
any other's. Its maps are its `readeria<n>.zbd`: the screen's scenarios
pick `n` and step down to the nearest the world has, as for the game's own
worlds. A pilot keeps the chosen world by number; with that mod gone the
Action screen shows the first world and a launch asks for c1, said in the
log (ours: the original would load a world named "none").

**Multiplayer.** The lobby's add-on scan (every `readermp<n>.zbd`, `n` over
2, named by its `mpinfo.zrd`'s `db_name`) looks in the first six folders
and, ours, in every mod world (9 on); t1, lab and dev1/dev2 stay left out.
A mod world's map is an add-on world like c3's: listed in the player's
lobby data, `**` beside a player who lacks it, and LOCK refused for it.
Test: `net_lobby_lists_a_mod_worlds_maps_headless`.

**Words.** `strings.txt` at the mod's root: `id=text` a line, over
Mech3Msg.dll's messages (the first enabled mod's line wins). The id is a
number (decimal or `0x` hex, as mech3ax lists them) or a message's name;
the text is UTF-8, shown in the game's code page (Windows-1252: a character
it lacks leaves the line out, said in the log), `\n` a line break, `\\` a
backslash, `#` a comment line. Cut to the original's 511 characters.

Tests: `test_mods.cpp` (world= and ia_world= read, a good world numbered 9,
a name taken, without files, with `..` and an unknown Instant Action world
left out; strings.txt: the first mod's line, hex ids and names, UTF-8 to
Windows-1252, `\n` and `\\`, a character the game lacks and lines without
an id left out, nothing without mods) and `mod_world_plays_in_instant_action_headless`
(a mod world of c1's files linked, `source c1.gs`, Instant Action on world
9: its readers read from `zbd\mymap\`, the player's mech in play).

## New mechs and weapons (after PM, items 1 and 2, 2026-11-08)

**A new mech.** `mech=` in mod.txt, one line a mech:

```
mech=thor_b tech=clan mass=70 class=heavy model=thor title=Thor B
```

- the name first (letters, digits, `_` and `-`, up to 40): its definition is
  `dfn_<name>.zrd`, in the mod's `reader.zbd` (a mech3ax repack of the
  game's with the entry added; a loose `.zrd` is read only as text) and its
  saved loadouts `configs\<name>.mw3`;
- `tech=clan`, `sphere` or `any`; `mass=` the most tons, 10 to 100 in steps
  of 5 (the internal structure's table has a row each 5 t); `class=light`,
  `medium`, `heavy` or `assault` (the roster's class, as the exe's own table:
  not always the tonnage's, discoveries.md);
- `model=` the mech whose models (`mech_<model>.flt`, its `_hud` and
  variants), motions, step sounds and voice it uses, its own name when not
  given (then they come from the mod's own mechlib.zbd and motion.zbd);
- `title=` the name shown (the rest of the line), else the name. The
  original's names come from Mech3Msg.dll (0xd82 on); a mod's row has no
  message id.

The exe's 19 keep their numbers (`mech_names.cpp`, 0x005e6078); a mod's rows
follow from 20 in the mods' order (`mech_count()`, `mech_row_add`): 19 (0x13)
stays the exe's "no mech", which saved pilots, the debriefing and the lab's
waiting mech hold, so it is never a mod's. A name
already a mech (any case, the exe's or an earlier mod's), an unknown key, a
value out of range, or files the game would not find (`dfn_<name>.zrd` in the
first `reader.zbd` of the enabled mods, the patch and the install, or loose
beside it; `mech_<model>.flt` in the first `mechlib.zbd`: a mod's archive is
the whole archive, so its own hides a later mod's) is said in the log and the
row left out, so no list offers it; `--check-mod` says the same (the files
against the mod and the install given). A row whose files fail anyway
reaches the launch's error box, "Failed to create player[ <name> ]" (0xf30;
the build's boxes, 0xf25 and 0xf30, are shown now, where they were only
logged). The rows are gameplay data: `mods_gameplay_hash` mixes each
parsed row (name, model, tech, mass, class; not the title or the spacing),
so a mod of rows alone changes the lobby's check (Multiplayer, above). A row
that parses but is refused as taken is mixed in too: every player computes
the same. Each mount starts from the exe's tables (`mods_mount` clears the
rows and the definitions cached for them), so mounting again, as the
launcher's MODS list will, adds the rows afresh.

The original's quirks stay with their numbers: the Mad Cat's loadouts weigh
half a ton less (mech 9 only, `Config::weight`), so the Mad Cat's own
loadouts copied to a new 75 t row are half a ton over and refused; the
Elemental's (7) are never weighed, and the other checks by number
(`mech_damage.cpp`, `mech_build.cpp`) stay the Elemental's alone.

**A new weapon.** `weapon=` in mod.txt, one line a weapon (the editor's
New Weapon writes both the line and the definition: MODS, EDITOR, a mission,
EDIT WEAPONS; editor.md, New Weapon):

```
weapon=ERPPC_B ammo=no tech=any title=ER PPC B
```

- the name first (as a mech's, up to 31): the weapon itself is the entry of
  that name in `weapons.zrd` (the mod's reader.zbd), which zWeapon reads as
  it reads the game's; a mech's loadout names it as any weapon;
- `ammo=yes` or `no` (whether the lab sells rounds for it and its bin
  empties); `tech=clan`, `sphere`, `any` or `none` (the techs the lab offers
  it in; TAG's is none);
- `title=` the name shown, else the name.

The exe's 37 keep their numbers, the plain PPC (FIX (bug), offered with the
bug fixes on) keeps 37, and a mod's rows follow from 38 whether the fixes
are on or off, so a pilot's saved weapon keeps its number (`weapon_count()`
is 38 and the rows once there are rows; 37 itself then still reads as no
weapon with the fixes off). A name taken (the PPC's too) is refused. The
rows are gameplay data in the hash as the mechs' are (name, ammo, tech).

**Both ways (Owen, 2026-11-09).** Mods must load in ours as they are, and a
mod made for ours must not break the original where it ships files the
original reads too. A mech or weapon row adds only entries the original never
asks for, read by name into tables sized from the data:

- **weapons.zrd** (a weapon row's definition): zWeapon's reader 0x583880
  counts BALLISTICS' pairs, `(items - 1) / 2`, and allocates that many
  definitions, `calloc(1, count * 500)`, each read from the file; the game
  finds a definition by name (0x581ed0) for the 37 weapons it knows. An extra
  entry is read into its own slot and never asked for. Run on the original
  (`test_weapons_zrd.py`, `mod_weapon_appended`): the shipped file with ERPPC
  copied as ERPPC_B at the end of BALLISTICS reads 50 definitions with no
  error, the port byte for byte the same, and the original's mech lab view of
  its 37 weapons unchanged. (Put a new weapon at the end: a definition's
  index, +0x10, is its place in the list.)
- **reader.zbd's dfn_<name>.zrd** (a mech row's definition): the archive's
  table is read whole whatever its count (0x5767b0, grown by 0x576690's
  `realloc(count * 0x94)`), and the mech
  reader 0x46ba40 opens `dfn_<name>.zrd` only for the 19 names of its own
  table (0x5e6078). An extra entry is never opened.
- **mechlib.zbd's mech_<model>.flt and motion.zbd's motions**: models and
  motions are opened by name (0x4fc8f0, 0x5a9ad0); mechlib's materials table
  is sized from its own count (0x4fc9e0, `operator new(count * 4)`), so a
  mod's mechlib with more materials reads in the original as in ours.
- **configs\<name>.mw3**: only read for a mech the original lists.

So nothing a row adds can overflow or break the original, and no separate
file is needed. Files only ours reads (mod.txt, strings.txt, campaign files,
mech3ax JSON beside an archive) the original never opens.

Equipment has no rows: each of the 15 kinds (AMS, jump jets, ECM, the heat
sinks...) is code as well as a name (its weight and slots by kind,
`equip_weight`, `equip_slots`, and what the mech does with it), so a new
kind would need new code, not data.

Tests: `mods_mech_rows_after_the_exes_and_in_the_gameplay_hash` (the rows'
keys and refusals, two mods' rows in order, a taken name, files missing, the
hash as data), `mods_mech_row_with_a_file_missing_is_refused_c1m1_headless`
(no dfn, no model: left out at the mount and by `--check-mod`; a row added
past that check: no mech, the launch's box naming it),
`mods_mech_row_launches_as_the_player_c1m1_headless` (a Thor copy as mech
20 from the mod's repacked reader.zbd, launched as the player's mech in c1m1
with the Thor's model, loadout and motions), `mods_weapon_rows_after_the_ppcs_place`
(the keys, numbers 38 on with the fixes on and off, the PPC's place, taken
names, a remount, the hash) and `mods_weapon_row_mounts_and_fires_c1m1_headless`
(ERPPC's definition copied as ERPPC_B in the mod's weapons.zrd, the Thor
copy carrying it: mounted in its group in c1m1, and two shots fired).

**Mod rows in the lists.** The lists that walked the exe's 19 go on to
`mech_count()`, skipping 19 (none): Instant Action's mech lists (a mod's mech
chosen as the commander selects its own row; the original's quirk, by
number, stays for the exe's), the lobby's pick by tonnage, the lab's whole
stock and the lab view's models. The plain PPC (37) is never offered with
the fixes off: every loop over `weapon_count()` reads it through
`weapon_has_tech` (false) or finds no definition for "noweapon" (the lab's
stock list shows only what the stock holds; `reset_ready_times`). Test:
`mods_weapon_rows_after_the_ppcs_place`.

Instant Action keeps its chosen commander, allies and enemies by their place
in its lists (the original's setup), so adding or reordering mods' mechs can
move those saved picks to other mechs. A mod's mech whose definition fails
to load after the mount's check (a broken one) leaves the lab empty for it
and says so once in the log (`mw3: mech <name>: dfn_<name>.zrd did not load`).

**Fixed sizes (audit, 2026-11-08).** No array or table outside
`mech_names.cpp` is sized by the mech or weapon count: the weapon tables
there are read only below `kBuiltinWeapons` (a row's own fields otherwise),
the dfn cache grows with `mech_count()`, the lab's models are found by the
mech stored in each (`model_index`), the loadouts' items pack a weapon in 16
bits (`mech_config.cpp`), the network sends the roster as 32 bits. A
sanitizer run (ASan and UBSan) of the launch tests is clean.

**Saves that hold a mod's mechs and weapons.** A pilot's file
(`pilots\<name>.mw3`), a mech's loadouts (`configs\<mech>.mw3`) and an
exported loadout (`export\*.m3c`) keep the original's numbers; beside one
holding a mod's row (a mech from 20, a weapon from 38), `<file>.rows` names
each (`mod_rows_file.cpp`):

```
# The mods' mechs and weapons this file's numbers name (docs/modding.md)
mech 21 beta
weapon 39 Gun_B
```

Loading translates each number to that row's number now, so swapping mods
round is safe. A name no enabled mod adds refuses the file, said in the log
(`mw3: pilots\Rows.mw3 not loaded: it uses the mod mech beta, which no
enabled mod adds`), and the file is left as it was: a refused pilot is not
listed and never written, a refused loadout file is not read and never
written over (`ConfigList::keep_file`), a refused import stays for later.
With no sidecar, every number is read as it is, as the exe reads it, past
the table too, and the sidecar names only rows a mod adds, so a damaged
file's number past the table is written and read back as it is (refusing
it, or naming it, departed from the exe: test_pilot.py's damaged files,
fixed 2026-11-09). A failed load other than a refusal still saves a dirty
pilot, as the exe's destructor does. A sidecar line that does not read refuses the
file (never the raw number). A file with no row numbers ignores a sidecar,
and writing one with none deletes it, so a stale sidecar never remaps a
clean file. The pilot's rename and delete and the import's `.done` carry or
remove it (only a file named exactly `<file>.rows`). The campaigns' saved
games hold no mech or weapon numbers (zSave keeps the animations' state).

The original exe and builds before this read the file without the sidecar:
they see the raw numbers, 20 on or 38 on, which name nothing there. A pilot
that used a mod's mech or weapon is ours only.

Test: `mods_saves_name_their_rows_beside_them` (a pilot with a mod's mech,
loadout, stock weapon and mech, and a loadout file, saved with the mods in
one order and loaded in the other: the same mechs and weapons; a mod off:
refused by name, the files untouched and not written over; no sidecar and a
number past the table: read as it is; a line that does not read: refused; rename
and delete carry it; a rewrite with no rows deletes it; a stray sidecar
beside a file with no rows ignored).

## The launcher's MODS list (after PM, item 5, begun 2026-11-08)

The list's model (`mods_list`, `mods_enabled`, `mods_save_enabled` in
`mods.cpp`), shared with the screen to come: every folder in the write
dir's `mods\` once, the enabled ones first in mw3.cfg's order, then the rest
by folder name; a folder that is no usable mod (no `mod.txt`, no `name=`,
another game's `for=`) listed with why, as the mount says it. For each
enabled mod, what it shares with an enabled one before it: the files that
one wins (any case) and the `mech=` and `weapon=` names it already adds
(this mod's rows then refused as taken), with the mods it shares them with.
On request, `--check-mod`'s count of what the game would refuse. Turning a
mod on or off or moving it rewrites mw3.cfg's `mod=` lines in the new order
at the first old one's place, every other line kept. Test:
`mods_list_for_the_launcher`. The screen itself waits.

## Custom campaigns (after PM, modding step 1, begun 2026-11-09)

The plan and what the original takes from code: `custom-campaigns.md`. A
`campaign=<file>` line in mod.txt names a text file in the mod (several
lines, several campaigns); the file's name without `.txt` is the
campaign's id and its pilots' folder, `pilots\mods\<mod>\<id>\`, so no
two campaigns share pilots (letters, digits, `_` and `-`, up to 32; two
with the same id in one mod, any case: the second left out). The file:

```
name=Hard Rain
stock=default
movie_start=campaign
op=Operation 1 bases=3
mission=c1 1 brief_c11 c1m1
mission=mymod_w1 1
op=Operation 2 bases=3
mission=c4 4 - c4m4
mission=c4b 4 bases=3
movie_end=c4win
```

- `name=` (required) the campaign's name; `stock=default` the original's
  new-campaign stock (the only one for now); `movie_start=`, `movie_end=`
  fmv.zrd's names, each optional.
- `op=<heading> [bases=N]` a heading over the missions after it; with
  `bases=` the field bases (0 to 3) are set to N at its first mission.
- `mission=<world> <n> [briefing] [movie] [bases=N]` one mission in play
  order: its world (a folder, the game's or a mod's `world=`), its `m<n>`
  block and start animation, briefing.zrd's block (else
  `brief_<world><n>`, `-` for that), fmv.zrd's movie before the briefing
  (`-` or none for none), and its own reset of the field bases.
- `#` starts a comment line. Every line must read: a misspelt key or a
  missing number refuses the whole campaign, naming the line (a dropped
  mission would change the campaign silently).
- At mount, once every enabled mod's worlds are added, each mission's world
  must have its `zbd\<world>\gamez.zbd` in an enabled mod, the patch or the
  install; a line naming one that does not is refused, named in the log, and
  the whole campaign is left out (a mod that needs another mod's world plays
  only with that mod enabled). `--check-mod` reports each campaign, refuses
  a line that does not read, and warns about a world neither the mod's nor
  the install's.
- The game never deletes a campaign's pilots' folder on its own: removing it
  is the player's choice, behind a confirm, and only for a folder with our
  marker (custom-campaigns.md).

Tests: `mods_campaign_file`, `mods_campaigns_mounted`,
`mods_check_mod_reads_campaigns`.

Step 2 (2026-11-09), the game and its pilots (`mod_campaign.cpp`,
`ModCampaignGame` in `pilot.h`):

- Entering a campaign (`campaign_enter`, for the MODS screen to call) lets
  the original's pilots go (saved first), makes the campaign's folder in the
  write dir with our marker `mw3-campaign.txt`, and lists only the pilots
  there; leaving (`campaign_leave`) brings the original's back. A folder of
  that name holding files the game did not put there is never entered.
  Everything the pilot module names goes through `pilots_folder()`: the
  pilot files and their saved games in its `games\`.
- While entered, every campaign game a pilot makes or reads is a
  `ModCampaignGame`: the original's campaign game (the kind the original's
  code and the scripts see is still the campaign's, 1, so every screen
  between missions is the original's) whose `mission` is its place in the
  list and `campaign` 1, so its saved games are `<pilot>01<place>.mw3`. The
  mission sees the line as the original's would (vt+0x48, +0x4c, +0x54,
  virtual in the exe too): the world's operation (c1..c3 1..3, c4 and c4b 4,
  any other world 4, the six-mission count the AI's in/out names take), the
  line's `m<n>`, its folder. Its name is the `op=` heading and the mission's
  place under it ("Op A, Mission 2"). A saved game's world is restored except
  at a world's start: the first mission and any whose world differs from the
  one before (the original's Op N Mission 1 and Op 4 Mission 4).
- Removing a campaign's pilots (`campaign_pilots_remove`) happens only on
  the player's yes, never for the campaign entered, and only for a folder
  holding our marker. The pilot screen's DELETE of one pilot asks as the
  original does ("Ok to delete pilot?") and removes its files only.

Test: `mod_campaign_pilots_and_games`.

Step 3 (2026-11-09), the advance and the movies by line:

- A mission won goes on to the list's next line, whatever its world (the
  original's walk by operations, `next_mission`'s `campaign_advance`, is a
  virtual `advance` the mod game replaces). The last line won ends the
  campaign as Op 4 Mission 6 does: the Win Movie state, then the credits.
  A failed or cheated mission is played again with its world loaded afresh
  at a world's start on the list (the first line, or a world other than the
  line before's: the original's Op N Mission 1 and Op 4 Mission 4), the same
  rule as a saved game's restore. The Win State and the debriefing ask the
  engine (`campaign_ends_at`, `campaign_world_start`), which answers the
  original's way for the original's campaign. What a mission itself decides
  by its operation and number (Op 4 Mission 4's salvage of allies' mechs,
  the voices) follows the line's world and `m<n>`, as the mission sees them.
- The field bases: a line's `bases=`, or for an `op=` heading's first line
  the heading's, sets them there (the first line's at the campaign's start);
  everywhere else they carry on, one lost each time a field base dies. The
  original's resets at each operation's first mission and at Op 4 Mission 4
  are not applied to a mod's list: write `bases=` where you want them.
- The movies: `movie_start=` before the first line's briefing, in place of
  the original's "campaign" (which plays only before Op 1 Mission 1); a
  line's movie before its briefing in place of `c<op>m<n>` (none when the
  line names none); `movie_end=` in place of `c4win` (none when not set). A
  line's briefing names briefing.zrd's block (and its voice) in place of
  `brief_<world><n>`.

Tests: `mod_campaign_advance_over_the_list`,
`mod_campaign_advance_and_end_headless` (the real screens: the pilot screen,
the briefing, a won mission, the debriefing, the next briefing, the ending).

### The MODS screen (begun 2026-11-09)

Ours, from the game's own pieces (`mods_menu`, `mods_tables.h`): a MODS
button on the main menu and its screen, both our records in dialog.zrd
(`patch12::apply_ours`, as the options' UPGRADES): the button after QUIT in
the original's look (`circlebtn_small`, a red label, OPTIONS' line), placed
for Owen to choose (at the bottom continuing the left column, or on the
right under the circle); the screen, MODSDIALOG, on the pilot screen's art
and widgets, with the pilot screen's groups at 16:9. Bound only where the
program turns it on (`mods_menu_page`), so the probes see the original's
menu. The screen is made when MODS is pressed and reads dialog.zrd itself
(the main menu's tree is not the file's by then).

- CAMPAIGNS (Owen, 2026-11-09: a plain list as MODS, not the pilots' six
  bars): the games' list in a copy of the games' box at the left, lined up
  with MODS (`campaign_lines`: the pilot screen's lines, the six bars' box
  taken out, the same size and place so 16:9 stretches it the same); a row
  for each `campaign=` of every mod in the write dir's `mods\`, by its
  `name=`, ` !` after one whose mod is off in this run or that cannot be
  played (the note line says which); DETAILS shows its author and version. A mod of campaigns only is not in MODS; one that also adds
  mechs, weapons or Instant Action worlds is in both.
- MODS (the games' box): every other mod, `[x]` on or `[ ]` off, the
  enabled ones first in mw3.cfg's order (`mods_list`, the launcher's), a
  ` !` after one with a conflict or that cannot be used; chosen, the line
  under the tabs says why it cannot be used, or what it shares with which
  earlier mod ("Shares 1 file with Green HUD, which wins").
- ON/OFF (an off mod goes to the end of the order), UP and DOWN (earlier
  wins a shared file) write mw3.cfg's `mod=` lines at once
  (`mods_save_enabled`) and take hold at the next start, which the screen
  says while mw3.cfg differs from the run.
- ENTER on a campaign whose mod is on in this run: `campaign_enter` (its own
  pilots), MODS popped and the Campaign State pushed on the main menu, so
  the stack is CAMPAIGN's: once a pilot is chosen the Campaign State pops
  itself and replaces the main menu with the mission's state, nothing left
  under the mission. (With MODS left under it, that pop resumed MODS and the
  replace swapped MODS out, keeping the main menu under the mission; found
  in review, 2026-11-09.) The campaign's pilots are let go when the main
  menu updates again (`MainState::update`: after QUIT, the campaign's end,
  or CANCEL on its pilot screen), never on a resume, which the Campaign
  State's own pop makes on its way to the mission. A campaign whose mod is
  off is never started half-mounted: ENTER turns the mod on for the next
  start and says so ("its mod is turned on; restart to play it").
- EDITOR: EDIT (editor.md, The way in from MODS): the selected campaign's or mod's missions in the campaigns' box, ENTER opening one in the game editor, saves going into the mod itself.
- TEST (Quick testing, below): the same list with the game's own missions after the mod's, ENTER playing one with the mods on.
- BACK: the main menu.
- ENTER and BACK hit where they are drawn (`ModsSlideButton`, lead 2026-11-10):
  the ring and knob of their 82x82 image (alpha 64 and up) and their label's
  box. SlideButton's own test (0x004855f0) takes the whole image with the
  label's height added below and the label's end beside, which reached over
  the tab row: a click on MODS FOLDER's right edge, or a tab placed past
  x 452, left the screen too. `sdl_program_mods_back_hits_where_drawn`.

Tests: `mods_screen_tables_order_and_conflicts` (the tables from five mods:
the campaign rows with a broken file, the rows' marks and notes, UP moving
the conflict to the other mod, ON/OFF and the save) and
`mods_screen_enter_campaign_and_back_headless` (MODS, ENTER, a pilot in the
campaign's folder, the first mission, the pause's options and QUIT, the main
menu, MODS, BACK: the stack at each step, nothing under the mission, the
pilots kept until the main menu). The button's place: the bottom left, as
shown (Owen, 2026-11-09). Left: a live remount.

### Plain errors on the MODS screen (modder step 3, 2026-11-10)

Ours (`upgrades/mods/mod_report`): a mod's `--check-mod` and what this
start's mount left out of it, as sentences for a player.
- `mod_check` fills a `ModFindings` when asked: the files it replaces and
  adds (counted only against an install), the mechs, weapons, worlds,
  Instant Action worlds and campaigns mod.txt adds, the player's own files
  ignored, the animation files Pirate's Moon does not read, each error
  (refused) and warning (a note) as `<what>: <why>`.
- `mods_mount` records what it left out per mod folder (`mods_left_out`):
  a mod not found or unusable, a world, campaign, mech, weapon or Instant
  Action world, with why.
- `mod_sentences` says them in order: "It replaces 2 of the game's files and
  adds 1 new file.", "It adds the mech Thor B and the campaign "Long
  Night".", what is ignored or unread, "The mech Scout is refused: there is
  a mech Scout already.", "Note: ...", "At this start the game left out the
  world x: ...", else "Nothing in it is refused."
- In the game: REPORT, a tab under CAMPAIGNS' box (28,336; the bottom row
  past x 452 is inside BACK's hit box, so a tab there also leaves the
  screen), shows the chosen mod's or campaign's mod's sentences in that box,
  wrapped to 34 characters, with the count refused under DETAILS; REPORT or
  BACK goes back to the tables, ENTER does nothing there.
- In the launcher: CHECK with a mod chosen opens its report on a page of its
  own (`<`, `>`, BACK to MODS). A Pirate's Moon mod's comes from
  `mw3pm --mod-report <folder> --install <PM's install>`, PM's own readers;
  `--mod-report` is a file-only flag (no SDL, no write dir, no log), so the
  player's folder and log are untouched.

Tests: `mods_report_says_what_a_mod_does_in_sentences` (the sentences, the
wrap, a mod checked and mounted), `mods_screen_report_shows_a_mods_sentences_headless`
(REPORT, ENTER ignored, BACK to the tables, then out) and
`sdl_program_launcher_mod_report_in_sentences` (MW3's from the check, PM's
from mw3pm's answer, BACK to MODS).

### ADD MOD (2026-11-09; IMPORT since the evening)

Ours (`mod_install`, the MODS screen's IMPORT, EXPORT, ADD FOLDER and MODS
FOLDER tabs beside EDITOR; IMPORT was ADD MOD, EXPORT is the next section).
IMPORT opens the system's file picker for an archive,
ADD FOLDER its folder picker (SDL's dialogs: one dialog cannot offer both);
the answer comes back on the screen's next update (`mods_menu_picked`, as it
may come on another thread). MODS FOLDER opens the write dir's `mods\` in the
system's file browser (`SDL_OpenURL` on its file:// URL). `--pick PATH`
answers the picker for scripted runs and shots.

- Archives: whatever the build's libarchive reads (the SETUP layer,
  `source_archive`): .zip, .7z and .rar now, not a later step. A build without
  libarchive adds folders only and says so.
- Names Windows cannot hold are refused on every system, so a mod added on
  the Mac still works on the PC: a device name in any part (CON, PRN, AUX,
  NUL, COM1-9, LPT1-9, with any extension, any case), a trailing dot or
  space, < > " | ? *; two files whose names differ only in case (one would
  overwrite the other on Windows and the Mac). A mod's folder is never given
  a device's name (`con` becomes `con_`). (2026-11-09, second sitting.)
- Unpacked into a staging folder of ours (`<write dir>\mod-staging\<n>\`, the
  marker `mw3-mod-staging.txt` in it), never into `mods\` directly. Every
  entry is checked first: an absolute path, a drive or any ':' (an alternate
  stream), a ".." part, a link (symbolic or hard) or another special file
  refuses the archive whole, as does going past the caps (20,000 files, 4 GB
  unpacked); a folder with a link in it too. On any refusal the staging
  folder is removed (only with its marker; staging folders left by a run that
  stopped halfway are cleared the same way at the next ADD).
- The root: the shallowest folder with a mod.txt, else with the game's own
  folders (zbd, video, music) or Mech3.exe, else a lone folder followed down;
  what sits beside it (a pack's readme) is left behind.
- A whole install (Mech3.exe in its root) is cut to the files that differ
  from the player's install (byte for byte, names in any case); programs
  (.exe, .dll: an exe patch cannot apply) and the player's own kind of file
  (pilots, configs, saves) are left out. All the same: nothing to add.
- An archive holding only a program (a mod shipped as its own installer):
  refused, saying to run it and add the folder it makes.
- No mod.txt: one is written, its name the archive's or folder's and for=
  from the names only one game ships (MW3's c4b and t1 worlds and its own
  mission readers, Pirate's Moon's readersa and readeria4); when they cannot
  tell, a message box asks MechWarrior 3 or Pirate's Moon (Cancel: not added).
- Checked as `--check-mod` does (`mod_check`): anything the game would refuse
  and it is not added, the first such line said.
- Moved into `mods\<name>\` (mod.txt's name, else the archive's) or, when a
  folder of that name exists in any case, `<name> (2)` and on: nothing in
  `mods\` is ever replaced or removed. The marker `mw3-mod-added.txt` in it
  says the game made it (the only kind it may ever remove; never served from
  the mod, nor counted as a shared file).
- The screen says "Adding <name>..." and draws it before the add runs (on
  its next update): a big archive takes a while, and a frame without a word
  looked frozen. No worker thread yet.
- On the screen: a mod that is more than a campaign is turned on for the
  next start; a campaign-only one is listed, ENTER turning it on for the next
  start. Said in two lines under MODS ("Added Desert Pack: on from the next
  start.", "Not added: it holds a file that would land outside its folder
  (../../outside.txt).").

Tests: `mod_install_entry_paths`, `mod_install_refuses_escapes_and_limits`
(zip slip, absolute, drive, link, the caps, empty, an installer only, a
folder with a link, the game's own folders), `mod_install_finds_each_layout`
(flat, nested with MOD.TXT, one folder down without mod.txt, a lone folder,
a folder source; listed with no conflict from the markers),
`mod_install_full_install_reduced`, `mod_install_name_clash_and_foreign_folders`
(a foreign `mods\My Mod` and a foreign staging folder untouched, a case-only
clash, a mod the check refuses), `mods_screen_add_mod_headless` (the screen
through the hooks), and `mod_install_real_mod_env` (MW3_PM13=<a mod on this
machine>, reported only).

The launcher's MODS screen has the same three (ADD MOD, ADD FOLDER, MODS
FOLDER, a row above UP and DOWN; five mods a page now), its own picker and
its status line, through the same add (`mech3::mod_add_run`, which both
call: prepared, the game asked, finished, turned on, the line to say);
`--launcher-pick PATH` with `--launcher-screen mods` runs it for a shot.
The SDL pieces both use (the folder opened, the game asked) are
`frontend/sdl/sdl_mods`. In a scripted run (`--pick`, `--launcher-pick`)
the game question answers MechWarrior 3 without a box. Test:
`launcher_add_mod_by_pick`.

### IMPORT and EXPORT (Owen, 2026-11-09; modder step 8 brought forward)

IMPORT is ADD MOD renamed. EXPORT packs the selected mod (a MODS row, or a
campaign's row: its mod) into one .zip that IMPORT takes back as the same
mod: `src/upgrades/mods/mod_export` (ours, the first of the MODS screen's
code in `src/upgrades/`).

- **Where:** the system's save box (`SDL_ShowSaveFileDialog`) in the
  player's documents folder, "<name> <version>.zip" offered (mod.txt's name,
  else the folder's, made a name every system can hold). `.zip` added when
  the box returns a name without one. Written as `<zip>.saving` and renamed
  over the name when whole; never into the mod's own folder. "Exporting to
  <zip>..." is drawn before the work, as IMPORT's "Adding".
- **In it:** the mod's files under one folder named as the mod (so unzipping
  it into `mods\` by hand makes a mod folder too), mod.txt with them, each
  with its own time, deflated; and `mw3-checksums.txt` beside mod.txt:
  sha256sum's lines (`<64 hex>  <path>`), one per file, so a player can check
  it with `sha256sum -c` (or `shasum -a 256 -c`).
- **Left out:** our markers (`mw3-mod-added.txt`, the staging marker), an old
  checksums file (made anew), the player's own kind of file
  (`zutl::mod_may_serve`: pilots\, configs\, saves\, mw3.cfg, logs and the
  rest), `*.bak`, `*.saving`, and the systems' folder notes (.DS_Store,
  Thumbs.db, desktop.ini, `._` files).
- **Refused whole,** as IMPORT would refuse the zip: a link, a special file,
  a name Windows cannot hold, two names differing only in case, more than
  IMPORT's caps (20,000 files, 4 GB), nothing of its own.
- **IMPORT's check:** a root with `mw3-checksums.txt` (any case) has every
  file listed with its sum and nothing else beside it (the systems' folder
  notes apart: a Finder re-zip adds them); a file changed, added or missing,
  a damaged line or a path that leaves the folder refuses the zip, named.
  Matched, the list is removed (a mod folder keeps no list that goes stale
  as the modder edits it). A zip without one is taken as before. sha256sum's
  binary mark (` *path`), CRLF lines and `#` comments are read.
- **The launcher's MODS list:** IMPORT, EXPORT, ADD FOLDER, MODS FOLDER in
  one row, EXPORT on the selected mod through the same `mech3::mod_export`
  and its own save box. `--save-pick PATH` (the game) and
  `--launcher-save-pick PATH` answer the save box in scripted runs.
- **Both ways:** the zip holds only the mod's own files and our checksums
  text, which neither game reads, so the original takes an unzipped export
  as it took the mod.

Tests: `mod_export_round_trip` (a mod with JSON folders and every kind of
left-out file, and files beside it in mods\: the zip holds only the mod's,
the sums are sha256sum's, IMPORT takes it back byte for byte with the same
mod.txt, and exporting that again gives the same files),
`mod_export_checksums_refuse_a_changed_zip` (changed, added, gone, mod.txt
changed, a damaged list, a list naming outside: refused; a Finder re-zip, a
hand-made list in binary form with CRLF: taken; no list: taken),
`mod_export_refusals` (into its own folder, nothing of its own, gone, a name
Windows cannot hold, a link, a case clash, a zip replaced whole),
`mod_export_sha256_in_pieces`, and `mods_screen_add_mod_headless` (EXPORT
through the screen's save hook, and the box cancelled).

Open: whether SDL's native pickers and the game question show above the
game's fullscreen window on Windows (the PC, a night check). Left: archives
inside an archive.

## Broken files and checking a mod (M3, begun 2026-11-01)

**Names stay inside the game's folders** (2026-11-02): a name a mod's files
give (a texture, a sound, a movie, a script's `source`, a world folder) is
looked up only inside the mounts; one with `..`, a drive or a full path
reads, writes and deletes nothing, and the log says so
(docs/assets-layer.md, Confined).

**The reader archives' entries, bounded.** Every `reader*.zbd` entry (a
mission's objectives, a mech's definition, the worlds' settings) is read
through `zrdr::load` with the entry's own length known: a list whose count
the bytes left cannot hold (every node is at least 8 bytes), a node of no
type, or lists nested past 64 deep mark the entry broken. It is then read as
no entry at all, and the log names the archive and the entry. The original
read on into the next entries or allocated what the count said; the shipped
entries read exactly as before (`test_zreader.py`, 178 files identical;
`test_zreader_text.py`, 540; `test_ai_readers.py`). Already bounded before
M3: the archives' tables (`zutl_zar`: a count or a length the file cannot
hold), strings (to the file's end), the text `.zrd` parser (an unclosed
list), `gamez.zbd` (a bad signature, version or a cut file), the AI files,
the mech definitions and the saved loadouts.

**`mechlib.zbd`, bounded** (2026-11-01): the materials table and each
model tree are read with the entry's end known (`cls_mechlib.cpp`): a count
the bytes left cannot hold (materials, vertices, normals, morphs, polygons,
a polygon's arrays, a node's children), a node of another class than
object3d (the exe dereferenced null), a tree deeper than 64, or a polygon's
vertex, normal or material index past its array refuse the tree (or the
materials), named in the log; a refused tree is a mech without its model.
`test_mechlib.py` still reads all 770 models, 442 materials and 57 trees as
the original; `bounded_mechlib_reads_the_shipped_trees_whole` (57 of 57)
and `bounded_mechlib_fuzz_never_crashes` (210 trees cut short, bytes flipped
or words made huge, and 30 broken material tables: none crash, every one
cut short refused; without the bound it crashes at once).

**`motion.zbd`, bounded** (2026-11-01): a motion is read with its entry's
end known (`zsis::Script::read`, `load_motion`): a frame count of 0 (the
loop time divides by it), a frame or part count the bytes left cannot hold,
a loop time not over 0, or a part's keys past the end refuse the motion
(its parts freed, named in the log); a refused motion is a missing one.
`test_motion.py` still reads all 258 as the original, each to its exact
end; `bounded_motion_reads_the_shipped_motions_whole` (258 of 258) and
`bounded_motion_fuzz_never_crashes` (300 copies: none crash, every one cut
short refused; without the bound none is refused).
A refused or missing motion is then taken from the next mount's copy of
`motion.zbd` (normally the install's: `load_motion` over `zutl::find_all`),
said in the log with the mod's archive and the motion's name; the rest of
the mod's motions stay its own. A gait a mod broke, with no whole copy
anywhere, refuses the mech (`load_animations`, `motion_broken`; the log
names the mech and the motion), and so does a step or walk missing from
every copy (`load_animations` reads both right away, a crash in the exe
too): the mech's set-up fails without the data
box 0xf2d, so an AI mech is only a log line, and the player's mech gets the
launch's error box (0xf30) and the briefing's Launch does not go on. Any other gait
missing from every copy is not refused: the exe ran with it null, as the
shipped Elemental does (there is no `elemental_limp`; 2026-11-02, the first
rule refused it and the campaign's Elementals with it). A mech without its
gaits can no longer crash (`pose_jump`, `load_animations`).
Tests: `bounded_motion_falls_back_to_the_next_copy` (madcat_walk cut short
in a mod: the install's frames, refused with no other copy, the mod's other
motions its own) and `mod_broken_motion_falls_back_or_refuses_the_mech_headless`
(Instant Action in a Mad Cat: it plays on the install's walk; with an install
of links lacking motion.zbd it is refused, no player mech, no crash; without
the check the refusal case segfaults).

**The worlds' `anim.zbd`, bounded** (2026-11-01; a whole file, not an
archive): every count and size it holds (its source list, each def's
lists, blobs and sequences, the messages) is checked against the bytes left
in the file before anything is allocated (`zeff_anim_init.cpp`); a broken
copy, or one cut short, is refused whole, named in the log, and the world
has no animations (the original left what it had read in place, half
bound). A def that cannot be bound to the scene (a lookup) still fails the
original's way. `test_anim.py` (6 worlds and the 10 mismatched and failing
runs), `test_anim_reset.py` and `test_anim_binder.py` as the original;
`bounded_anim_fuzz_never_crashes` (150 copies of c1's: none crash, every one
cut short refused; a source count of 0xfffffff0 refused before any
allocation, where unbounded the run passes 4 GB and is stopped). What the
anim run later makes of a garbled event stream inside a well-sized blob is
not checked here.

**The texture and image libraries, bounded** (2026-11-02; `rimage.zbd`,
`rtexture*.zbd`, `rmechtex*.zbd`, one format, not zar archives:
`zimg_library.cpp`). The table's counts must fit the file (the exe
malloc'd and read them as they were); an entry's name ends inside its 32
bytes; an entry's offset lies past the table and palettes and inside the
file, its global palette one the library has (the exe indexed the shared
palette table with it unchecked); a texture's bytes run to the next
texture's offset or the file's end, and a width, height or palette count
below 0, or pixels, an alpha plane or a palette past them, refuse the
texture (null, named in the log: the exe returned a half-read texture). A
refused library is a missing one; a refused texture is one the library
lacks. `test_texload.py` and `test_shadow_image.py` as the original;
`bounded_textures_read_the_shipped_libraries_whole` (22 libraries, 17,644
textures, each the same pixels as the exe's read) and
`bounded_textures_survive_broken_copies` (300 copies of a 12-texture
library: no crash, every cut copy keeps exactly the textures whose bytes it
still has; table counts past the file or below 0 and an entry's offset or
palette out of range refused).

**`gamez.zbd`'s texture directory, bounded** (2026-11-02,
`zimg_texture.cpp`): each entry's mip index must be -1 or a row of the
table; one outside refuses the directory (the table as it was, named in the
log), and the world fails to load as for a short read ("Error reading GameZ
texture data."). The exe made a pointer of any index and followed it when
the texture loaded. `test_gamez.py` and `test_texload.py` as the original;
`bounded_gamez_texture_directory_mips` (300 garbled copies of c1's 553
entries: every one with an index outside refused, every one read with each
mip inside the table; without the bound none is refused).

**The sounds, bounded** (2026-11-02; `soundsL.zbd`, `soundsH.zbd` and loose
WAVs: `zsnd_create.cpp`). After the chunk walk, a WAV whose samples or cue
points run past its bytes, or whose format no output can play (0 or more
than 8 channels, 0 or more than 32 bits, a rate of 0 or over 384 kHz, a
block or byte rate of 0), is refused, named in the log, and is a sound not
loaded (the exe copied the samples as the data chunk said, from past the
buffer). `bounded_sounds_read_the_shipped_archives_whole` (1,998 sounds and
10 loose WAVs, each buffer its own samples) and
`bounded_sounds_survive_broken_copies` (300 cut, flipped or garbled sounds:
no crash, all 99 cut inside their samples refused; without the bound all
300 are made).

**`interp.zbd`'s scripts, bounded** (2026-11-02, `zinterp.cpp`). An entry's
name ends inside its 0x78 bytes; a script whose offset lies outside the
file is not the archive's (the loose file's turn, as for a script the
archive lacks; the exe seeked there); a script runs to the next one's start
or the file's end, and a line past that ends the script there, said in the
log (the exe read on into the next script). Each include keeps its own end.
`test_interp.py` as the original; `bounded_interp_reads_the_shipped_scripts_whole`
(44 scripts, 2,504 lines, as unbounded) and `bounded_interp_survives_broken_copies`
(300 copies of a 10-script archive: no crash, every cut copy reads its whole
scripts as before; a first line reaching into the next script ends the
script there, the others unchanged; an offset past the file refused).

**Every format a mod can replace is now read with its end known** (M3 complete, 2026-11-02): a broken file or entry is refused and named in the log, never read past its end.

**Larger than shipped** (after PM, item 4, 2026-11-08). A repack that adds
to the game's archives (mech3ax's, say) reads whole: no reader caps a count
below what its file holds. Test `bounded_readers_take_more_than_shipped`,
on our own repacks (no mech3ax, no real mods): mechlib.zbd with its 57 trees
four times over (228, 3,080 models), motion.zbd's 258 motions four times
over, reader.zbd's 178 entries four times over, c1's anim.zbd with its 529
sources and 1,770 defs twice over, c1's gamez.zbd with 2,500 free node slots
more (a second pool block, chained into its free list): each read whole, the
world's same 8,167 nodes live. The limits that are left, each refused and
named, no crash: a count the bytes left cannot hold (a tree or motion a byte
short), reader lists past 64 deep (64 read, 65 refused), anim.zbd's def count
(an int16 in the format: 32,767 at most; 0x7fff with too few bytes refused),
a node array past its file.

One limit is not a reader's: the model pool. Before a world is read it holds
1,750 models (0x6d6, `reset_pool`), and a world's gamez.zbd sets it from its
own count; a mission that loads more models than its pool holds (many new
mechs' trees, say) stops the game with "GameZ model resource failure", as the
original did. Not raised yet: it is the first entry of the fixed limits a mission
can reach (Planned, Whole campaigns).

**`mw3 --check-mod DIR`** reads a mod as the game would without mounting it
into play, and exits 1 when anything would be refused, else 0. It reports:
mod.txt (a folder without one is checked as an install overlay, files laid
over the game's folder, which play as an install: no error); each `world=` (its `zbd\<folder>\gamez.zbd` and `<folder>.gs`)
and `ia_world=`; every file and what it replaces in the install (mw3.cfg's
`install=`) or adds; the player's own kind of file, ignored; every archive's
table; every reader archive's entries through the bounded reader; a
`mechlib.zbd`'s materials and every tree, a `motion.zbd`'s every motion, an
`anim.zbd`'s read, every texture and image library's textures, every sounds
archive's sounds and `interp.zbd`'s scripts through theirs; every loose
`.zrd` through the text parser; `strings.txt`'s lines the game would
leave out (warnings). Test: `mods_check_mod_reports_what_the_game_would_refuse`
(a good mod with a copied reader, a loose fmv.zrd and strings: no errors; a
bad one: no name, a world without files, an unknown Instant Action world, a
non-archive `.zbd`, a reader entry claiming a billion nodes and an unclosed
`.zrd`: six errors) and `mods_broken_reader_entries_are_refused` (a list of
a billion, a type 9, 100 lists deep, a list running into the next entry:
each refused, the entries around them read), and
`bounded_check_mod_reads_textures_sounds_and_scripts` (shipped copies of a
library, interp.zbd and six sounds pass; a library cut by 5,000 bytes names
its last four textures, a script offset past the file and a sound cut in
half are named: six errors).

`gamez.zbd`, `anim.zbd` and the `texture*.zbd` sets are not zar archives, and
the check took them for one, so a shipped copy came back an error and the
animation read was never reached (found 2026-11-06). Now `anim.zbd` is read
(by `AnimState::host_path`, ours, as a mod can sit deeper than the 127
characters 0x005090f0 holds), the texture sets go through the library read,
and `gamez.zbd` gets its signature and version only: the rest needs the world
up. Every shipped kind passes on c1, c4b and t1. Test:
`mods_check_mod_reads_animations_at_any_depth`.

**Pitfall: an edited `reader.zbd` for the original Pirate's Moon needs its
CRC.** PM's archives (zArchive version 2) end with a CRC of every entry's
data, in table order. The original PM checks only `zbd\reader.zbd`'s, after
the intro (0x004876e0; protection.md section 6 on the `pm` branch), and on a
mismatch closes with no message. So a `reader.zbd` edited for the original
needs its CRC set again: `tools/analysis/zcrc.py --fix reader.zbd` (on the
`pm` branch), or rebuild it with `tools/analysis/zarc.py`, which writes it.
The other archives hold 0 there and are never checked. It doesn't matter
for our port: the check is a protection trap we leave out (FIX (trap)), and
a mod's archive loads with any CRC. MechWarrior 3's archives (version 1)
have none.

## Quick testing (modder-friendly step 4, begun 2026-11-10)

**TEST on the MODS screen** (`ModsScreen::test`, `mods_test_rows`,
`mods_menu_test_start`): a tab under the CAMPAIGNS box, at (108 336) beside
REPORT (BACK's hit box covers the bottom row from x 452, so neither fits
there). With a mod or a custom campaign chosen:
- The campaigns' box lists missions, as EDITOR's does: the mod's own first
  (its campaigns' lines, else its worlds' `readerm<N>.zbd`), then every
  campaign mission of the install's nine worlds (`zbd\<world>\readerm<N>.zbd`
  in the game's world order: c1 1..4, c2 1..4, c3 1..6, c4 1..3, c4b 4..6, t1
  1..4 on MW3 1.2), each world and number once. So a mod of weapons or mechs
  alone, which has no missions, is tested on the game's.
- ENTER plays the chosen one with the mods on in this run, started as EDIT's
  is (a debug start: a stand-in pilot in the Mad Cat, no briefing), the editor
  left closed (Shift+F12 and the pause's EDITOR still reach it, saving into
  the mod, as in any mission MODS starts). When the main menu comes back
  (the pause's QUIT is the tested way), MODS is on top again and the chosen
  pilot and the launch are put back (`mods_edit_begin` with `editor`
  false).
- A mod not on in this run is turned on for the next start instead, as EDIT
  and ENTER do. TEST again leaves the list; EDITOR while it is up shows
  EDIT's list of the same mod (and TEST while EDIT's is up, TEST's); BACK
  leaves the list first.

Test: `mods_screen_test_plays_a_mission_and_comes_back_headless` (a mod with
no missions: TEST's 24 rows from c1 1 to t1 4; EDITOR then says it has none
to edit; ENTER on c2 mission 1 plays it with the editor closed; the pause's
QUIT lands on MODS, the pilot back). Breaks caught: the game's missions left
out, the editor opened, MODS not reopened. A `--click-at` shot (MODS, the
mod, the tab at 130,344) shows TEST's list with only TEST fired.

**Values read again between TESTs** (part 2, 2026-11-10): edit a mod's
files, QUIT to MODS, TEST again; no restart.
- Weapons: weapons.zrd (a mod's loose text or its archive) is read with the
  world (0x00583880 from the world load 0x0048faf0), which keeps a world
  already up and loads any other afresh. The pause's QUIT unloads the world
  (0x00490480: its `current` is -1 back on MODS, checked in the headless
  test's run), so every TEST loads it, and the file as it is then, again.
- Mechs: a mech's dfn is read once per run (0x004846a0's cache). TEST's
  start lets every cached definition go (`mod_test_reload`, 0x004844b0's
  loop: each one's saved loadouts written first, as at the game's exit), so
  the mission reads them again. Nothing keeps a definition across a mission:
  every user asks `mech_dfn` (or its handle) when it needs one, and the
  `Config`s that come from a definition's list (a mission's mechs, the
  launch's first loadout) live only in that mission; a pilot's slots hold
  copies.
- Not reloaded: mod.txt's `mech=` and `weapon=` rows (the row tables are
  made at the start), models, textures and sounds. In a running mission
  nothing is reloaded: the weapons' definitions are held by pointer with
  their effects and shots in flight (one ported reader makes both).

Test: `mods_test_reads_edited_weapons_and_mechs_again` (a mod's loose
weapons.zrd and dfn_madcat.zrd: the first TEST reads 1234.5 and 0.77; after
the edit a load without TEST's reload has the new velocity and the old
accel_rate; TEST again has both new), and the headless TEST test checks the
start calls it. Breaks caught: the definitions not let go, the start not
calling it.

## Starter mods (modder-friendly step 5, 2026-11-10)

`mw3 --make-starter mech|weapon|translation|mission` (`mw3pm` the same,
`upgrades/mods/starter_mods`) makes one small example mod in the write dir's
`mods\starter_<kind>`, from the player's own install: the recipe is in the
code, the game's file is read from the install and the edit applied, so
nothing of the game's is in the repo or the download. The guides, in plain
words for modders: `docs/starter-mods/` (one per kind, and a README).
- mech: Thor B, `mech=thor_b ... model=thor` and `zbd\dfn_thor_b.zrd`, the
  Thor's definition as text with `max_turn_rate` x1.25 and `accel_rate` x1.5.
- weapon: ER PPC B, `weapon=ERPPC_B ammo=no tech=any` and `zbd\weapons.zrd`,
  the game's as text with ERPPC copied after the last (DAMAGE x1.25, HEAT
  x1.2; New Weapon's `weapons_with_definition`).
- translation: `strings.txt`, the main menu's seven buttons in German, by
  the install's own message numbers (read from its Mech3Msg.dll, each name in
  a comment: the games number them apart, and `--check-mod` has no names).
- mission: `campaign=one_mission.txt`, a campaign of the game's c1 mission 1.
Each also has `about_this_mod.txt` (not `README.txt`, which the install has:
the check would count it as replacing the game's). Each passes `--check-mod`
with no warning and its report says nothing is refused.

Only ever a new folder: when `starter_<kind>` is there (a folder or a file,
whatever is in it), nothing is made and the message names it; the made
files are built in memory first and the folder is made last-moment with
`create_directory`, so there is no replace, no delete and no `--force`.

Found with it, fixed: a mech row's loose `zbd\dfn_<name>.zrd` passed the
mount's and `--check-mod`'s checks (archive_has: "loose beside it") but the
reader never opened it. zrdr's load looked a name no archive has up only at
the mounts' roots. Now a mod's file in an archive's own folder (loose, or
mech3ax's JSON) is that archive's new entry (`mod_copy(..., added)`, said as
"added to zbd\reader.zbd"); with no mods nothing changes.

Test: `mods_starter_mods_made_checked_and_loaded` (each made, checked and
reported clean; all four mounted: Thor B's loadouts with 1.5 times the Thor's
accel_rate, ERPPC_B 1.25 times ERPPC's damage and after it, the menu's
messages German, the campaign of c1 1; made again over a folder with a stray
file and over a file of that name: refused by name, both left as they were;
an unknown kind: nothing). Without the reader fix Thor B's definition does
not load.

## Readable files read directly (modder-friendly step 1, begun 2026-11-09)

A modder edits a reader entry as text and runs, without repacking reader.zbd.

**Which lookup.** Ours, `mod_copy` in `zreader.cpp`: when an archive has the
entry, `zutl::find_in_mods` is asked for the archive's own folder plus the
entry's name (`zbd\weapons.zrd` for `zbd\reader.zbd`, `zbd\c1\world.zrd` for
the c1 world's reader), in the enabled mods only, first mod first. The
original has a loose-file lookup, but not one that can do this: `zrdr::load`
(0x00556740) asks the archives first and takes the text branch (the search
path 0x00556240, then the text reader 0x005563d0) only for a name no archive
has. Our text branch keeps that rule for the install; the mod's copy is the one
case where a loose file wins over an entry, and only inside a mod (an
install's stray `.zrd` changes nothing).

**The same reader.** The loose file goes through the original's text reader
(0x005563d0, `parse_text`), as the game's own loose files do, and then through
the load hook as an archive entry does, with the archive's name: the official
1.2 patch's edits (`patch12`) and the rebuild's own records (the MODS button,
MODSDIALOG; `patch12_edits.cpp`) are applied to it, so a mod's edited
dialog.zrd keeps the MODS screen.

**Errors.** Before it is parsed, `zrdr::text_error` reads the file's brackets
and quotes, skipping `;` comments: "line N: a '(' never closed" (the outermost
one), "line N: a ')' with no '(' before it", "line N: a quote never closed".
A file with one is refused, said once in the log ("mw3: mod: FILE: line 3: a
'(' never closed; zbd\reader.zbd's entry read instead"), and the archive's entry
is read: a broken edit never leaves the game without the entry. `--check-mod`
gives the same line as an error ("zbd/weapons.zrd: line 3: a '(' never closed
(not read; an archive's entry of that name is)"). A loose copy that reads is
named in the log once ("FILE instead of ARCHIVE's entry").

Tests (`test_mods.cpp`): `mods_loose_weapons_zrd_read_directly` (the PPC's
VELOCITY as zwep::init reads it: 900 shipped, 1234.5 with the loose copy, also
when the mod has its own reader.zbd beside it, 900 again with the copy broken
on line 3, which `--check-mod` names; each error's line),
`mods_loose_dialog_zrd_keeps_our_records` (MODSDIALOG through the hook, for the
entry and the loose copy, none without the hook),
`mods_loose_zrd_beats_the_archives_entry` (no mod: the archive's). Breaks
caught: the line count, the outer '(' (a case with two open), the hook skipped,
the fallback dropped, the check's line dropped, the loose lookup off.

**mech3ax's reader JSON (built 2026-11-09).** A mod may hold a reader entry
as unzbd writes it: `zbd\reader\weapons.zrd.json` (a folder named as the
archive without `.zbd`, the entry's name plus `.json`, as unzbd's
`replace_ext` names it), or unzbd's zip itself beside the archive,
`zbd\reader.zip`, holding `weapons.zrd.json` (its name matched in any case,
as the archive's are; `manifest.json` and anything else in it ignored). Per
mod, first match wins: the loose `.zrd`, then the folder's JSON, then the zip,
then the mod's own archive; mods first to last; never the install's. The JSON
(`zrdr::parse_json`, `zreader_json.cpp`) is the tree one to one: an array a
list, `null` an empty one, a string (UTF-8, kept only if Windows-1252 holds
it, as the game's text), a number with '.' or an exponent a float, else an
int. Objects and `true`/`false` are refused: mech3ax writes neither.

*Exact numbers.* An int must fit 32 bits and is read as written. A float is
read straight to a float (`strtof`, rounded once): mech3ax writes each f32 as
the shortest text that reads back to it (serde's), so its bits come back; a
hand-written 0.1 rounds to the nearest float as the text reader does. (Read
as a double and then narrowed, a rare value would round twice and miss by
one bit.) `to_json` writes the shortest text that reads back to the same
float. `mods_reader_json_round_trips_every_shipped_entry` writes every reader
entry MW3 ships (55 archives, 1,268 entries) as JSON and reads it back to the
same tree, every float to the bit; with mech3ax's unzbd beside the checkout
(`../mech3ax/target/release`, or `MECH3AX_UNZBD`; the part skipped and said
without it) it exports each archive and reads unzbd's own files too: all
1,268 the same trees.

*Errors* name the line and the value's place, as a JSON path: "line 2 (at
$[1][1]): true or false: the reader has none (write 1 or 0)", "line 1 (at
$[0]): 2147483648: past the game's 32-bit integers", "a '[' never closed",
lists past 64 deep. A refused copy is said in the log ("mw3: mod: FILE: line
2 (at $[1][1]): ...; zbd\reader.zbd's entry read instead") and the archive's
entry is read; a zip that does not open, the same. A good copy goes through
the load hook, as the archive's entry does. `--check-mod` reads every
`*.zrd.json` and every entry of a `reader*.zip` the same way and gives the
same lines as errors ("zbd/reader.zip:fmv.zrd.json: line 1 (at $): a '['
never closed (not read; an archive's entry of that name is)"). Tests:
`mods_reader_json_errors_name_the_line_and_place`,
`mods_reader_json_entry_in_a_folder_or_a_zip` (the order, the refusals, the
hook), `mods_check_mod_reports_what_the_game_would_refuse`.

**mech3ax's motion JSON (built 2026-11-09).** A mod may hold a motion as
unzbd writes it: `zbd\motion\madcat_walk.json` (a folder named as the
archive, the motion's name plus `.json`), or unzbd's `zbd\motion.zip` itself
(matched in any case; `manifest.json` ignored). `load_motion` asks the enabled
mods first to last: the folder's JSON, then the zip's, stopping at the first
mod with its own `zbd\motion.zbd` (that archive is then read as before, its
broken motions from the next copy). The JSON (`zsis::motion_from_json`,
`si_script_json.cpp`; documents through `zutl_json`) becomes the entry's own
bytes: version 4, `loop_time`, `frame_count`, the part count, then -1.0 and
1.0 (stride and multiplier, the same in every motion; unzbd checks them and
leaves them out), each part's name, flags 12, its `frame_count` + 1
translations and rotations (the last key the first's again, which unzbd leaves
out); unzbd's quaternion `x, y, z, w` is the file's order. Those bytes go
through `Script::read`, the archive's bounded reader. Floats are read with
`strtof`, so unzbd's shortest texts come back to the bits.

`mods_motion_json_round_trips_every_shipped_motion` writes each of MW3's 258
motions as JSON (`motion_to_json`) and reads it back to the entry's own bytes,
each read by `Script::read`; with unzbd (as above) it reads unzbd's own 258
files too, all the same bytes. Errors name the line and place: "line 2 (at
$.parts[0].frames[1].rotation): no \"w\"", "(at $.parts[0].frames): 2
frames, where frame_count says 3", "(at $.loop_time): not above 0", syntax
errors too ("line 2 (at $.parts[0].frames[1]): the file ends where a value
should be"). A refused motion is said in the log ("mw3: mod: FILE: ...;
zbd/motion.zbd's motion read instead") and the archives' is read, not a later
mod's JSON. `--check-mod` reads every `zbd/motion/*.json` and `motion.zip`
entry the same way. Tests: those two, `mods_motion_json_errors_name_the_line_and_place`,
`bounded_motion_json_in_a_folder_or_a_zip` (`test_bounded_readers.cpp`: the
order, the refusals, the own-archive stop),
`mods_check_mod_reports_what_the_game_would_refuse`.

**mech3ax's mechlib JSON (built 2026-11-09).** A mod may hold mechlib's
entries as unzbd writes them: `zbd\mechlib\mech_thor.flt.json` (a model's
node tree) and `zbd\mechlib\materials.json` (the shared materials), or
unzbd's `zbd\mechlib.zip` itself. `mechlib_load` and the materials' read ask
the enabled mods first to last, the folder then the zip, stopping at a mod
with its own `zbd\mechlib.zbd`; a model the archive lacks is served too, so a
new mech's model needs no repacked mechlib.zbd, and a `mech=` row finds its
`dfn_<name>.zrd` and `mech_<model>.flt` as JSON (a folder's or a zip's) as it
finds them in an archive. The JSON (`zcls::mechlib_model_from_json`,
`mechlib_materials_from_json`, `cls_mechlib_json.cpp`) becomes the entry's
bytes as mech3ax's writer lays them out: from node 0 down, each node's 0xd0
record (its name over "Default_node_name", flags from the named booleans,
the node's `index` in the model slot, as unzbd keeps it), its object3d (0x90:
`Initial`, a `Matrix`, or `RotateTranslateScale` built as mech3ax builds it,
unzbd's `original` matrix where the sums differ, the zero signs), its model
(0x5c, then vertices, normals, morphs, the polygons' 0x24 records, then each
polygon's indices, UVs and colours), then its children. Those bytes go
through the archive's own bounded readers (`read_tree`, `read_material`)
from a `ScratchFile`. Lights in a mechlib model are refused (none ship).

*The shipped bits.* `mods_mechlib_json_is_every_entrys_bytes`: with
unzbd found beside the checkout (`../mech3ax`, or `MECH3AX_UNZBD`; skipped and said without it) and run on the install, every one of MW3's 57 models and the
materials come to the archive entry's own bytes. The matrix's sines and
cosines are rounded once from double: mech3ax's f32 `sin_cos` on the Mac is
Apple's `__sincosf`, and `cosf` can miss it by one bit (the Blackhawk's
lcalf: cos 0.855211 is 0x3f27f37b in the file and in truth, 0x3f27f37c from
`cosf`); the true value, rounded, is the file's on every system. Errors name
the place: "(at $.nodes[0].model_index): no model 3", "(at
$.models[0].polygons[0].priority): 51: past -50..50", "(at $.nodes[0]):
reached twice (a loop in child_indices)", an unknown flag's name.
`--check-mod` reads `zbd/mechlib/*.json` and `mechlib.zip`. Tests:
`bounded_mechlib_json_in_a_folder_or_a_zip` (a model the archive lacks, the
order, a refusal, the own-archive stop), `bounded_mechlib_json_errors_name_the_place`,
`mods_mech_row_files_as_json`, the check test. A self-contained round trip
needs a writer of mechlib JSON as well (not written; the unzbd run stands for
it).

**mech3ax's anim JSON (built 2026-11-09).** A mod may hold a world's
animations as unzbd writes them: the folder `zbd\c1\anim\` (its
`metadata.json`, a `<def>.json` per anim def, a `si-script-NNN.zan.json` per
motion script), or unzbd's `zbd\c1\anim.zip`. The file is read whole, as
the game reads anim.zbd: the enabled mods first to last, the folder then the
zip, stopping at a mod with its own `zbd\c1\anim.zbd`. The JSON
(`zeff::anim_from_json`, `zeff_anim_json.cpp`) becomes the anim.zbd it
stands for, laid out as mech3ax's writer lays it (header, the source list,
the globals with mech3ax's per-world pointers, def 0, then each def's 0x13c
record, its lists, prerequisites, anim refs, reset state and sequences, each
sequence a blob of events), read through a `ScratchFile` by the anim reader
0x0050a0e0 and bound by the post-load pass as the shipped file is.

*Names past their NUL.* The shipped files keep, after many names' NULs, what
the original tools' buffers held. mech3ax restores those bytes from tables of
its own built from the shipped files (its anim-names crate); we keep no
game-derived tables (Owen's rule), so our names are zero after their NUL.
Those bytes are never read: the reader and the runtime take names to their
NUL. Our file therefore differs from the shipped one only past names' NULs.
Where unzbd keeps a name's trailing bytes in the JSON (the source list's
`garbage`, the static sounds' and anim refs' `pad`), they are written back
exactly, and node names keep mech3ax's "Default_node_name" padding.

`mods_anim_json_is_every_worlds_file` runs unzbd on all six worlds and
compares our file with the shipped one, every name masked past its NUL: c1
5,228,852 bytes (24,975 names), c2, c3, c4, c4b and t1 all the same. Events:
all 39 of MechWarrior 3's types. The six no shipped world uses but the
runtime runs (Effect, ObjectDeleteChild, CameraState, CameraFromTo,
FbfxCsinwaveFromTo, AnimVerbose) are checked against mech3ax's own writer
(`rezbd`) on the same JSON (`mods_anim_json_unused_events_as_mech3ax_writes_them`);
Effect, which mech3ax refuses for MechWarrior 3, needs a def's `effects`
list (written after the static sounds, as the reader takes it) and is checked
by its layout. Errors name the file, line and place ("box.json: line 2 (at
$.sequences[0].events[0].data.ObjectActiveState.node): no node \"lid\" in
the def's nodes", an unknown event's name, a world mech3ax does not know). A
refused world is said in the log and its anim.zbd read; `--check-mod` reads
an `anim\` folder or `anim.zip` the same way. Tests: those two,
`mods_anim_json_in_a_folder_or_a_zip` (the order, a refusal, the own-archive
stop), `mods_anim_json_errors_name_the_place`, the check test.

**mech3ax's other JSON, mapped (not built).** `unzbd` writes one zip per archive,
with `manifest.json` (the entries' names, order and the bytes needed to pack
them again) and one JSON per entry, or per file for gamez:

| Archive | unzbd's files | Our reader that would take them |
|---|---|---|
| `reader*.zbd` | `<entry>.zrd.json` | built, above |
| `mechlib.zbd` | `materials.json`, `<model>.flt.json` (`format`, `version` not needed) | built, above |
| `motion.zbd` | `<motion>.json` per motion | built, above |
| `zbd\<world>\anim.zbd` | `metadata.json`, `<def>.json` per anim def, `<script>.zan.json` per script | built, above |
| `zbd\<world>\gamez.zbd` | `metadata.json`, `textures.json`, `materials.json`, `models.json`, `nodes.json` | built, "gamez JSON" |

Proposed naming: a mod holds unzbd's output unzipped in a folder named for the
archive without `.zbd` (`zbd\mechlib\`, `zbd\motion\`, `zbd\c1\anim\`,
`zbd\c1\gamez\`; the reader's `zbd\reader\weapons.zrd.json`, built), or the zip itself beside it
(`zbd\mechlib.zip`, as unzbd wrote it). For reader, mechlib and motion each
entry stands alone: a folder with one model's JSON replaces that model and
the archive gives the rest, and `manifest.json` is not needed. Anim and gamez
are read whole, so their folder needs all its files or is refused (said by
`--check-mod`), and the archive is used. Within one mod: a loose `.zrd`, then
the JSON folder, then the zip, then the mod's archive; mods first to last as
now. JSON is read only from a mod, never the install.

Order, smallest and most edited first: (1) reader JSON, (2) motion, (3)
mechlib, (4) anim, (5) gamez: all done. Each with a test against unzbd's own output on
every shipped entry, run by itself when mech3ax's unzbd is beside the
checkout. PM's archives the same where its formats match (mech3ax's `pm`
readers), on `pm` after pm-v1.0.0.

## Animations a def at a time (design 2026-11-09; step 1 built 2026-11-10)

Pirate's Moon (branch pm) reads a world's animations from its anim.zbd only
(its own reader, 0x00523140): a mod's anim.zbd, yes; unzbd's JSON
(`anim.zip`, `anim/`), def files and node files, no. For a Pirate's Moon mod
(`for=mw3pm`: mw3pm's own check, and the launcher's MODS for Pirate's Moon)
`--check-mod` says so for each such file, and the MODS list counts them on the mod's line
("N animation files Pirate's Moon does not read") instead of reporting
shared animations (`anim_file_unread`, mods.cpp).

**Built (step 1): the merge, the def files and the save's name check.**
- `upgrades/mods/anim_defs` (`mod_def_files`, `with_defs`, `def_spans`)
  runs in the anim load (`zeff::anim_read_zbd`), on the file chosen (the
  install's, or the first enabled mod's whole anim.zbd or JSON).
- A def file is read alone by `zeff::anim_def_from_json`. Its key is
  unzbd's file name for it, from its own names (`anim_def_key`: node-anim,
  or node-anim-root when the root is another node; the same as unzbd's names
  on every def of the six worlds).
- An ObjectMotionSiScript event in a def file names its script's file
  (`"script": "si-script-<name>.zan"`, read from `<name>.json` beside it)
  in place of metadata's index.
- Each file that does not read, or is passed over by the rules, is said once
  in the log (`mw3: mod: ...`).
- A saved running animation whose def at the saved index has another name
  is dropped and said (`zeff_anim_save.cpp`, running_load). The save also
  records its mods (below, The save's mods). In test_anim_save.py's saves
  (six missions, plain, cross-loaded from the original's, and damaged), the
  only runs dropped are the damaged phase's records cut short before their
  name ends (all 28 shorter than 0x28 bytes, the name read as the zeros
  past the cut). No plain or cross-loaded save loses one.

Tests: `mods_anim_def_files_merge_on_every_world` (all six worlds: the keys,
one def replaced, a scripted one where the world has one, and one added,
every other def's bytes, the header and what follows as shipped) and
`mods_anim_def_files_by_the_mods_order` (t1: the order rules, the notes, the
world as the game reads it, the save's drop). Breaks caught: no replacing in
place, the root left out of the key, a later mod replacing the base's def,
the name check left out.

**Built (step 2, 2026-11-10): the MODS screen and `--check-mod`.**
- The list (`mods_list`) reads each enabled mod's def files by world and key
  (their own names, `anim_defs::def_file_key`) and its whole anim files.
- `anims_taken` holds a def an earlier enabled mod's def files hold for the
  same world: a world-less one meets every world.
- `anim_bases` holds a world where an earlier enabled mod's whole anim file
  is the base, so this mod's def files only add new animations there.
- Both mark the mod as sharing with that mod ("Shares 1 animation with a,
  which wins; its animations only add to t1", or "Its animations only add to
  t1: c has the whole world's").
- `--check-mod` reads each def file alone (`zeff::anim_def_from_json`, scripts
  beside it) and names the line and place of one that does not read ("this
  animation left out"), else says which animation it is and for which
  world.

Test: `mods_anim_def_files_on_the_list_and_checked`. Breaks caught: a
world-less def not meeting a world's; the check not reading def files.

**Built (step 3, 2026-11-10): `--build-anim` and `anim=defs`.**
- `mw3 --build-anim <mod> [--install DIR]`
  (`anim_defs::build_for_original`): for each world of the install that the
  mod has def files for, it writes the install's anim.zbd with the mod's
  def files merged in (its world folder before its world-less one, the first
  file of a key) to `<mod>\original\zbd\<world>\anim.zbd`, keeping a .bak of
  one there before. Errors are named and counted, and the exit code is 1 on
  any.
- mod.txt's `anim=defs` tells ours that the mod's whole anim files (at
  `zbd\<world>\` too) are the original's copy:
  - the load passes over them, for the next holder's (a mod's, else the
    install's): `anim_from_mod`'s skip and `whole_file_past_defs_only`;
  - the def merge's base passes over them as well;
  - the MODS list takes no base from them;
  - its def files are merged as any mod's.
- `original\` is ours never to read: `mod_own_file` leaves it out of the
  list's sharing, the lobby's hash and `--check-mod`. It is never served at
  a game path anyway.

Tests:
- `test_anim_build.py`: def files from unzbd, one replaced (a scripted one
  on c1, its script by file) and one added, built by the real
  `mw3 --build-anim`; the original's reader 0x50a0e0, its post-load
  0x50ae60 and frames of random calls (test_anim_run's run_mission) run on
  each built file as the port does, on t1 and c1.
- `mods_anim_defs_only_and_build_anim`: with anim=defs the install's base
  (gravity) and the def added; without it the mod's own file; no base and
  no sharing from original\; the built file walks, with its .bak.

Breaks caught: the count left as it was; the anim=defs skip left out;
original\ counted as the mod's.

**Step 4's check (2026-11-10): what the shipped effect defs need from the
world.** These are the 64 animation names weapons.zrd uses (every key holding
ANIMATION). On each of the six worlds (unzbd's anim and gamez JSON), the 60
defs that have one name 221 nodes (their own node, their node list and their
object list).
- Every one of those nodes is an Object3d tree (220).
- The one exception is `impact_ppc_mech`'s camera1, the world's own camera,
  which every world has.
- No effect def names a LOD, a light or any other node class from the world.
  Their lights (32 defs), puffers (17) and static sounds (5) come from their
  own lists, made at load.

So a node file holding object3d trees in mechlib's model form is enough for
any effect. A def naming a camera can name the world's. The four names with
no def in any world are in fixes.md, Candidates for Owen.

**Built (step 4, 2026-11-10): the node files.**
- A node file is `<node>.flt.json` beside the def files (`zbd\<world>\anim\`
  or `zbd\anim\`). It is a tree in mechlib's model form as unzbd writes one
  (`{"nodes": [...], "models": [...]}`), plus `"materials"`: its own list in
  mechlib's materials.json form, which its polygons' `material_index`
  indexes. Copy an unzbd mechlib model and its materials.json into one to
  start.
- With the world loaded, before the fixup (`anim_read_zbd`), the enabled
  mods' node files are read (`load_mod_node_files`). Per node name the first
  mod's counts, its world folder before its world-less one, and only a name
  the world lacks is read: a node already there is the world's, said once.
- Each tree is read as mechlib's are (read_tree on its bytes, every
  polygon's material within its own list, resolved onto the world's
  materials) and left parentless and as its flags say, as the shipped
  effects' nodes are (gaus_muzl.flt and the rest have no parent). The
  fixup's search by name (list_find_by_name over every node) finds it.
- Then each def file's nodes (its own, its root, its object and node lists)
  must be in the world or among its own lights and sounds
  (`def_nodes_found`). A def naming one that is not is left out, said in
  the log. The original's fixup stops at a def it cannot bind and leaves
  every later def unbound, so one typo would otherwise break the world's
  animations from there on.
- `--check-mod` reads each node file's two halves without a world.
  `--build-anim` says, for each node file, that the original reads nodes
  only from the world's gamez.zbd.

Test: `mods_anim_node_files_bind_their_defs`, on c1's world. A node file
(mechlib's smallest model renamed mod_muz.flt, mechlib's materials as its
own) and a one-node effect def on it:
- the read runs to its end, every one of the 1,770 defs bound, mod_blast on
  mod_muz.flt;
- a def naming a node no world has is left out;
- --check-mod passes the node file and names one with no materials list;
- --build-anim notes the node file.

Breaks caught: no node files read (the def then left out); no node check
(the read stops at the bad def, -1, later defs unbound).


Owen's ask: animations moddable, and easy, with every old mod working
exactly as before. Today a mod replaces a world's whole anim.zbd (or its
whole JSON, above). Adding one weapon effect, say a heavy gauss rifle's
`hgaus_fire` with its trail, muzzle flash and flyby loops, means editing all
six worlds' anim.zbd and gamez.zbd, and two mods that touch animations
cannot both be on. This is the plan to fix that. Nothing here is built yet.

**What the engine already allows (from the code).**
- Defs find each other by name. CallAnimation keeps the target's name and
  looks it up the first time it runs (0x506640: a search over all defs by
  name, the index cached in the event at +0x30). Stop, reset and invalidate
  find their def by name too (0x506d60..0x506ea0), as do `anim_find`
  (0x50b450) and the prerequisites' type 1 (0x508950). So a def added under a
  new name is reachable by any def that names it, and a def replaced under its
  own name is what its callers get.
- An anim ref (a def's own list, 0x48 bytes) is a slot the calling def fills
  at run time. It holds no index into the set, so merging does not touch it.
- Within a def, events index only that def's own lists: nodes, lights,
  puffers, sounds, effects, scripts. A def's JSON stands alone, apart from the
  .zan scripts it names by index. Those come with it (below).
- The fixup (0x50a0e0) binds each def to a world node by name: +0x20, with a
  run of defs of the same node name taking that name's successive nodes.
  The post-load pass (0x50ae60) then works on each def alone (loops 1-4).
  Neither cares where in the list a def sits, except for those runs, so
  appended defs keep the shipped defs' bindings.
- Saved games are the one place the index matters. A running animation is
  saved as "Running%03d" with its def's index (0x50be30), and read back by
  index (0x50c250). Activation records are saved by name.

**1. A def at a time.** A mod may hold:
- `zbd\c1\anim\<node>-<anim>.json`: one def for one world, the file unzbd
  writes for it;
- `zbd\anim\<node>-<anim>.json`: one def for every world;
- a script it names, `si-script-<name>.zan.json`, beside it. The def's
  `index` in ObjectMotionSiScript then names its own file
  (`"script": "si-script-<name>.zan"`, our addition), so no global script
  numbering is shared between mods.

No `metadata.json` means "merge into the world's set", not "the whole file"
(the whole-file form, above, keeps metadata.json). The world's set is built
from three things:
- the base: the world's own anim.zbd, or the first enabled mod's whole file
  (below);
- each def file: it replaces the base def of the same key (the file's
  `<node>-<anim>` name, which unzbd makes from the def's node name and anim
  name), or it is added;
- per key, the first enabled mod that has it wins, as for every other file
  (mods first to last; the first wins). The lead's brief said "later
  winning"; this keeps our one rule. Within one mod, the world's own folder
  beats the world-less one.

Order and indices: replaced defs keep their base index. Added defs go after
the base's last def, in the order of the mods and then by name, so the
shipped defs keep their indices and their node runs. Two fixes come with it:
- a saved running animation is checked by name as well as index on load,
  where today only the index is used. A save made under another mod set then
  drops the anims that moved, said in the log, instead of starting the wrong
  def;
- the save records the mods' identity (`mods_identity`, which today only the
  multiplayer lobby compares), and the load tells the player when it differs.

**The save's mods (built 2026-11-10).** `upgrades/mods/save_mods`, a zSave
registrant of ours, "Mods" (priority 1, saved and read ahead of the
animations' 0x32..0x34):
- A campaign game saved while a gameplay mod is on (`mods_identity`'s hash
  not 0, the lobby's rule: textures, sounds, strings, music and movies do
  not count) holds one more entry, `Mods/identity`, as text:
  `hash <16 hex digits>`, then `mod <name>` for each such mod. Saved
  without one, the file is the original's, entry for entry.
- The original's load passes over an entry whose owner it has not
  registered (0x005a8e40; `test_zsave.py` runs loads with registrants
  missing), so the original exe and earlier builds read ours as before.
- Each saved-game read (`PilotHooks::load_world`) starts as "made without
  mods"; the entry, when there, gives the save's. After the mission load's
  "Saved Game Loaded", a save whose hash is not the running one's adds a line
  to the loading screen and the log: "Saved with other mods: A; now B",
  "Saved with mods: A; none now" or "Saved without mods; now with B". The game
  still loads: the animations are matched by name (above), so what moved is
  dropped rather than run as the wrong one.

Tests: `mods_saved_game_records_its_mods` (a world mod, two of them, a
textures mod: the entry only with gameplay; each pairing said or not, once;
the text read back), and `campaign_menus_to_c1m1_through_the_briefing_headless`
(c1m1 won with a mod's identity, c1m2's load with none: the loading screen's
line). Breaks caught: the line left out, the entry written without mods, the
load's reset left out.

The bytes: each def is written with the anim JSON's def writer
(`zeff_anim_json.cpp`'s, unchanged), and each base def's bytes are spliced in
from the base file. A def's bytes are self-contained: its record, its lists,
its reset state and its sequences. Then the header's def count is set and the
file is read by 0x50a0e0 as now.

**2. The nodes a def needs.** A weapon effect's def drives its own nodes:
`hgaus_trc.flt` and its tracer, a muzzle-flash model. Today they live in each
world's gamez.zbd, which is why an effect means six gamez edits. Proposal: a
def may come with its node tree as a mechlib-style model,
`zbd\anim\<node>.flt.json` (the format of `mechlib_model_from_json`, above).
- It is loaded the way a mech's parts are: read_tree, mechlib_resolve onto the
  world's materials, its textures by name from the texture archives.
- It is put into the world inactive before the fixup, under its node name, so
  0x50a0e0's search finds it. Effects are made active and placed by their own
  defs (CallAnimation's at-node and the motion events), as the shipped ones
  are.
- Its materials: a mechlib model indexes mechlib's shared list, so the file
  carries a `materials` list of its own (our addition), appended to the
  world's list as mechlib's are resolved.
- A texture of its own needs a texture library, which this does not cover;
  for now the shipped textures, or a mod's whole rtexture file.
- Open: whether any shipped effect def relies on a gamez-only node class
  (lights, LODs). Lights come from the def's own light list, made at load.
  To check per shipped effect def before building.

**3. Conflicts on the MODS screen.** The list's model (`mods_list`, above)
already reports the files an enabled mod shares with one before it. Def
files get the same treatment, keyed by world and def: "Heavy Gauss's
hgaus_fire (all worlds) is Other Mod's, which comes first". It also reports
when a whole-file mod sets the base for a world in which another mod adds
defs, so a modder sees which defs take effect. `--check-mod` reads each def
file alone through the def writer and names its errors by file and place, as
it does for the whole JSON.

**4. Old mods, and the two forms together.**
- A mod's whole anim.zbd (or whole JSON folder or zip) is still the base,
  exactly as today: with no def files anywhere, nothing changes.
- With both kinds enabled: the base is the first enabled mod's whole file for
  that world (else the install's). Def files from mods listed before that mod
  replace or add defs over it. Def files from mods after it only add defs the
  base lacks, and never replace that mod's own. So a whole-file mod keeps what
  it shipped against anything listed after it, as it does now, and a
  def-at-a-time mod listed ahead of it can still put its defs in.
- Two whole-file mods for one world cannot be merged, as now; the first
  wins, and the screen says so.

**5. Both ways.** The original reads only a world's anim.zbd (and gamez.zbd);
it never sees def files or node files. A modder targeting both games ships:
- the def files, for ours;
- a whole anim.zbd per world built from the install's plus those defs, for
  the original. We would give this a tool: `--build-anim <mod>` writes each
  world's merged file into the mod's `original\` folder, with the same
  splicing as the loader;
- the effect's nodes in each world's gamez.zbd, for the original only (it has
  no other way to get them).

In such a mod, mod.txt's `anim=defs` tells ours to use the def files and
ignore that mod's own whole files, so two both-ways mods still combine in
ours. Without it a whole file wins as in point 4, and old mods are unchanged.
The `original\` folder is the modder's to copy over an original install.
Ours would ignore it, added to what `mod_own_file` skips as mod.txt is.

Order of work, when Owen agrees, one sitting each:
1. the merge and the def files, with a test that adding and replacing defs on
   all six worlds leaves every other def's bytes as shipped, plus the save's
   name check;
2. the MODS screen's conflicts and `--check-mod`;
3. `--build-anim` and `anim=defs`;
4. the node files, after the per-def check of the shipped effect defs.

## gamez JSON (built 2026-11-09)

A mod may hold a world as unzbd writes it: the folder `zbd\c1\gamez\`
(`metadata.json`, `textures.json`, `materials.json`, `models.json`,
`nodes.json`) or unzbd's `zbd\c1\gamez.zip`. The game opens it as the
world's gamez.zbd (`open_zbd` 0x4ff6a0, used by the load 0x4ff520 and the
re-read alike, the 1.2 patch's open hook still applied): the enabled mods
first to last, the folder then the zip, stopping at a mod with its own
`zbd\c1\gamez.zbd`. A world a mod declares (`world=`) may be its JSON too.
The JSON (`zcls::gamez_from_json`, `src/upgrades/json/gamez_json.cpp`)
becomes the file it stands for, laid out as mech3ax's writer lays it: the
header, the texture directory (each name's last '.' a NUL, its suffix kept),
the materials with their free slots and cycles, the model array (each
model's record and its data's offset, the free slots chained, then the
models' data, through mechlib's model writer with lights), then the node
array (each node's 0xd0 record and its data's offset, an empty node's its
one parent's index, the free slots, then each node's class data, parents and
children). Every class: empty, camera, world (fog, area, the virtual
partition's constants, lights, sounds and every partition), window, display,
object3d, lod and light. The file is read through a `ScratchFile`, kept
until the next open since the reader reads on from it. Fields unzbd leaves
out take its defaults.

*Size.* models.json is 41 MB for c1, too big for one tree:
`zutl_json::Pull` walks the models and nodes lists and reads each item as
its own small tree, with the same line-and-path errors.

*Byte for byte.* `mods_gamez_json_is_every_worlds_file` runs unzbd on all
six worlds: every file is the shipped one byte for byte, with no masks (c1
11,412,064 bytes). mech3ax keeps a table of about a hundred c3 partitions
whose middle height its float sum misses; its partitions' pointers are not
in the 1.2 c3 file (another release's, it seems), and on all six 1.2 worlds a
float sum and a double one give the same bits, so neither the table nor the
way of summing matters for these files. Ours sums in double and rounds once,
as the original's tools likely did (unverified: no file here tells them
apart). Errors name the file, line and place ("nodes.json:
line 2 (at $[0].data): not a node class this format has: Portal", "an empty
node has one parent", an unknown flag's name). Tests: that one,
`mods_gamez_json_in_a_folder_or_a_zip` (the order, a refusal, the
own-archive stop), `mods_gamez_json_errors_name_the_place`,
`mods_json_pull_walks_and_names_the_place`, the check test,
`mods_worlds_declared`.

For mods, gamez is whole-world only: no per-node merging. New nodes for
animations come from point 2 of "Animations a def at a time".

## Planned
- M3 (v1.0), the rest: bounded readers for mechlib, motion and anim
  (above), each with `--check-mod` looking inside.
- After v1.0: a custom campaign, new mechs and weapons (done: New mechs and
  weapons, above), loose `.zan` motions, the launcher's MODS list.
- Existing data mods load (Owen, 2026-11-07): a file the old engine loaded isn't
  refused by a check stricter than it was. An exe patch can't apply; what it
  changed is rebuilt as data. Tested on real mods, kept out of the repo.
- Whole campaigns (Owen, 2026-11-07): the goal is fully moddable, including a
  rewritten campaign with more enemies. Every fixed limit a mission can reach
  (units, AI, objects, scripts) found, then raised, or refused with a message
  naming it, where the original crashed.
  The fixed limits found so far:
  1. The model pool: 1,750 models (0x6d6, `reset_pool`) before a world is
     read, the world's gamez.zbd count after; past it the game stops with
     "GameZ model resource failure: restarting your game usually solves
     this." (`model_alloc`, 0x00538fe0). Found by item 4's mechlib four times
     over (3,080 models); to be sized from the mods' rows.
- After Pirate's Moon (Owen, 2026-11-07), in one list: custom weapons with their own
  effects and animations, custom mechs, custom maps and campaigns, all as data;
  existing data mods (mech3ax and zStudio output) loading as they are; quicksand as a
  part a mod can use, the pits the engine already makes plus, behind a mod's own
  setting, the sinking Recoil had (slower, lower gravity, settling in); the game
  editor reachable from the launcher; and installing a mod made simple: the
  launcher takes a mod's .zip, .7z or folder (picked, or dropped on its window),
  finds the game files in it whatever its layout, checks them with `--check-mod`,
  asks which game, writes `mods/<name>/` with its `mod.txt`, and lists mods to turn
  on, off and reorder, with their conflicts shown. A mod made by overwriting an
  install is taken as the files that differ from the player's own install. Nothing is
  ever written into the install.
- The in-game MODS screen and custom campaigns (Owen, 2026-11-08): mods are turned on
  and off, and custom campaigns entered, from the game itself, so a mod never replaces
  the original campaign. Owen: "i just want people to be able to enable/disable mods at
  will, and to enter custom campaigns/games". In order:
  1. **Campaigns as data.** A mod names a campaign in mod.txt (`campaign=<file>`), a
     text file listing its operations and missions in order: each mission's world (the
     game's or the mod's own, `world=`), its mission files, briefing (text, and a movie
     if any), and what comes between missions (debriefing, salvage, the mech lab as the
     original campaign has them). Single missions the same way, a campaign of one.
     The original campaign is untouched; a custom campaign's pilots and saves are its
     own, beside the original's.
  2. **A MODS button on the main menu**, set apart from the others (there is little room
     under CAMPAIGN: at the bottom or on the right side, placed per game, MW3's and PM's
     menus each their own; a shot of each choice for Owen first), in each game's own style
     (its menu art, fonts, list boxes and buttons; zTwoD's widgets, as the other
     screens). It opens a screen with two tables: CUSTOM CAMPAIGNS (each installed
     campaign or mission, its author and version, an entry point that starts it or
     continues its pilot) and MODS (every other mod: mechs, weapons, worlds, asset
     packs, text; a check box each, an order, conflicts shown and why a mod isn't
     usable). The same choices as the launcher's MODS list, which stays; both write
     mw3.cfg. A change to the MODS table applies at the next start, said on the
     screen (a live remount is a later step); entering a custom campaign mounts its
     own mod for that game.
  3. **Installing made simple**, as planned above: an ADD MOD button on the MODS screen
     (and the launcher) opens the system's file picker; the picked .zip, .7z or folder
     (or one dropped on the launcher) is unpacked into `mods/<name>/`, checked, sorted
     by what it holds (a campaign, or mechs, weapons, worlds, assets, text), and shown
     in its table at once: a campaign can be entered straight away, a content mod is
     turned on for the next start (until the live remount). An "Open mods folder"
     button beside it.
  Both games: main first, pm by the crossing (PM's own campaign format where it
  differs). Each step with its tests and a section here.
- Modder-friendly, after the MODS screen and custom campaigns (Owen, 2026-11-08), in
  this order:
  1. **Readable files read directly**: a mod's loose `.zrd` as text and mech3ax's JSON
     (models, motions, animations, worlds) taken as they are, ahead of the archives, so
     a modder edits and runs without repacking with rezbd.
  2. **A reference generated from the readers**: every key each reader takes
     (weapons.zrd, the dfn files, aiv, targets, the mission scripts' commands, mod.txt),
     with its type, default and limits, built from the code so it stays right.
  3. **Plain errors on the MODS screen**: `--check-mod`'s findings and the mount log as
     sentences per mod (what it replaces or adds, what is refused and why), instead of
     a crash or silence. Built 2026-11-10 (above, Plain errors on the MODS screen).
  4. **Quick testing**: a "test this mod" start straight into a chosen mission with the
     mod on; later, weapon and mech values reloaded while the game runs.
  5. **Starter mods and guides**: one small example of each kind (a mech variant, a
     weapon, a translation, a single mission) that names the game's files instead of
     shipping them, and a short guide for each.
  6. **A modder mode**: the developer tools behind one switch (free camera, spawning a
     unit, AI paths and nav points, hit boxes), with the Battlesport editor below.
  7. **Requirements in mod.txt**: needs another mod, conflicts with one, the game
     version it needs; the MODS screen warns before a mod is turned on.
  8. **A pack button**: a mod folder to a shareable zip with its mod.txt and checksums.
- The Battlesport editor as the mission editor (Owen, 2026-11-08, the after-PM order's
  step 4): the original developers' in-game editor (editor.md: the AI net and vehicle
  editors behind its menu, the stealth object's view) ported whole and wired up so
  modders make missions with it as Zipper did, saved as a mod. It is the "game editor"
  of Making missions below: worlds come from zStudio (Vorta's own program, maintained
  apart), missions and their units, nets and objectives from the editor. Built in tandem
  with the MODS screen: the screen gets an EDITOR entry from the start, a stub until
  the editor is built, then wired to it. The editor's dialogs and screens are built out
  to work exactly as the original's; what the exe doesn't carry but the editor needs is
  made by hand, said as ours. After pm-v1.0.0, in the after-PM order; not to be dropped
  (Owen: "i want to do all of this so lets not forget about it").
- Quicksand's art (2026-11-07): every MW3 world's `declient.zrd` asks for seven frames,
  `qsnd01` to `qsnd07`, that no MW3 texture file has; Recoil ships them under those names.
  Ours will be new art in MW3's style (Owen: made with the local image generator), one
  seamless texture turned into the seven looping frames by a scripted drift, so the loop
  stays smooth; in the rebuild's own assets with its licence, never Recoil's.
- Making missions (Owen, 2026-11-07): worlds are zStudio's, missions are ours.
  [zStudio](https://github.com/Vorta/zStudio) (Vorta, MIT, Windows) rebuilds Zipper's own
  source tree from the game files and compiles worlds back to `gamez.zbd`; the rebuild
  loads what it exports, and we don't duplicate it (what we learn goes back as pull
  requests: knowledge and fixes in its own C#, since it takes no decompiled sources).
  On top of a world, built in or from zStudio, the game editor gets "New mission": the
  mission files with a player start, then units, nets, nav points and objectives placed
  in the running game, saved as a mod. The launcher's mod section links zStudio (opens
  it when installed on Windows) and starts the editor on a game and mission. A guide,
  `docs/making-missions.md`, written once those exist: a world in zStudio, a mission in
  the editor, packed and installed as a mod.
- Our additions in their own folder (Owen, 2026-11-07): whole features that aren't the
  original's (the mission designer, the mod installer, Recoil-style quicksand, and the
  load watch, hit log and font search now beside ported code) go in `src/rebuild/`, so
  `src/gamez` and `src/mech3` stay the original with its marked fixes, and
  `src/rebuild` and `src/frontend` are ours.
- Exe patches aren't supported: the code is open, so a change to the engine is a
  change to the source. What exe patches were used for (weapon rows, limits, the
  resolution) becomes data or a setting instead, so mods stay combinable and
  multiplayer can compare them.

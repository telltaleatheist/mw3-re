# Mods

Ours (Owen, 2026-11-01: "players and hobbyists can add their own game data
without any change to the exe later"; small, "nothing big and dramatic").
With no mods enabled nothing is mounted and nothing in play changes; nothing
is ever written into the install or into a mod.

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
  archive's entry in use.
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

**A new weapon.** `weapon=` in mod.txt, one line a weapon:

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

Step 2 (2026-11-10), the game and its pilots (`mod_campaign.cpp`,
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

Test: `mod_campaign_pilots_and_games`. Next: the advance over the list (the
original's walk by operations is still what `next_mission` does), the field
bases and the movies by line, then the MODS screen's CUSTOM CAMPAIGNS table.

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
     a crash or silence.
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

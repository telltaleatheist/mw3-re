# What the rebuild reads: a spec for tools that export to it

For authors of tools that make MechWarrior 3 content (zStudio in the first place; Owen, 2026-11-11:
making things is zStudio's, the rebuild imports and plays it). This page is the contract in one place;
each rule links to where it is worked out and tested. Field-by-field keys: `docs/modding-reference.md`
(generated from the readers, so it cannot drift). How it all works: `docs/modding.md`.

The short version: **export the original game's own files, laid out as an install lays them out.** The
rebuild reads the original's formats with ported readers; anything the original exe loads, the rebuild
loads. Everything below is either a place to put those files or one of a few small text files of ours
that say what the original exe hard-codes.

## 1. What a player imports

MODS, IMPORT takes any one of: a folder, a `.zip`/`.7z`/`.rar` (archives inside archives too), an
installer `.exe` (read, never run), or a game folder the mod was laid over (only files that differ from
the player's install, or are new, are taken). The import is copied into `mods\<name>\` in the player's
data folder. The player's install is never written.

A tool's export can be a plain folder or zip with the install's layout:

```
mod.txt                  optional; written by IMPORT when absent (section 3)
strings.txt              optional; words (section 6)
<world>.gs               optional; a world's build script as text (section 2)
<campaign>.txt           optional; a campaign's mission list (section 4)
zbd\interp.zbd           optional; the prepared scripts, as the original's
zbd\reader.zbd           optional; the game's records (weapons.zrd, dfn_*.zrd ...)
zbd\<world>\gamez.zbd    a world: its scenery (version 27, as the original's)
zbd\<world>\anim.zbd     its animations (version 39)
zbd\<world>\reader.zbd   its world records (world.zrd: nav points, sky)
zbd\<world>\readerm<n>.zbd    mission n (the campaign's missions)
zbd\<world>\readeria<n>.zbd   Instant Action map n
zbd\<world>\readermp<n>.zbd   multiplayer map n (named by its mpinfo.zrd db_name)
zbd\<world>\rtexture*.zbd     its textures
```

Archives that the player's install also has (reader archives) are cut at IMPORT into the entries that
differ: `<archive>.entries`. So a tool may export whole archives; two mods that change different records
of one archive then both apply. (modding.md, Archives by their changed entries.)

## 2. A new world

- **The folder name**: not one of the game's nine (`c1 c2 c3 c4 c4b t1 lab dev1 dev2`, any case); no
  slash, dot or space. A mod's worlds are numbered on from 9.
- **Required**: `zbd\<world>\gamez.zbd` (the world's mark: without it the folder is not a world), and a
  build script the load can find: `<world>.gs` inside the mod's `zbd\interp.zbd`, or as a text file at
  the mod's root. Without a script the world's campaign and Instant Action entries are left out at the
  mount and INFO says "nothing on it can load".
- **The script decides the scenery**: `set DB <world>` reads `gamez.zbd` and `anim.zbd` from the world's
  own folder; `source c1.gs` (or `set DB c1`) takes c1's. INFO says which.
- **Always from the world's own folder**: `reader.zbd` (`world.zrd`; without it the mission loads but has
  no nav points and cannot be won), each `readerm<n>.zbd` (without it the launch gives up), the textures.
- (modding.md, A world the game does not have, found at IMPORT; measured table of what each missing file
  does.)

## 3. mod.txt (optional for an export)

When an import has no `mod.txt`, IMPORT writes one from what it finds: for each new world `world=<w>`;
`ia_world=<w> <w>` when it has `readeria<n>` maps; and a campaign of its `readerm<n>` missions in number
order (section 4). A tool that wants other names writes its own `mod.txt`; then IMPORT never changes it,
and a world folder it does not name is warned about. Keys: modding-reference.md, mod.txt. The ones a
world tool needs:

```
name=Iron Coast
version=1.0
author=...
for=mw3                      (or mw3pm; Pirate's Moon takes only mw3pm mods)
world=ironcoast
ia_world=ironcoast Iron Coast
campaign=ironcoast.txt
```

## 4. A campaign

The original exe hard-codes its campaign's order; the rebuild reads it from a text file, named by
`mod.txt`'s `campaign=` (modding-reference.md, A custom campaign's file). Every unknown key refuses the
file, its line named.

```
name=Iron Coast
op=Operation One bases=3
mission=ironcoast 1 brief_ironcoast1 -
mission=ironcoast 2 brief_ironcoast2 -
mission=c1 3                 (any world's mission may appear)
movie_end=...
```

A mission line names a world folder and its `readerm<n>`; a line whose world has no script, or whose
`readerm<n>.zbd` is nowhere in the mounts, leaves the campaign out (said on MODS and in the log). A
campaign plays as the original's does: pilots, mech lab, salvage, saved games, its own save folder.

## 5. Instant Action and multiplayer

- **Instant Action**: `ia_world=<world> <name>` puts the world on the list; its maps are
  `readeria<n>.zbd`.
- **Multiplayer**: every `readermp<n>.zbd` of a mod world is listed (from `readermp1`), named by its
  `mpinfo.zrd` `db_name`, else "<world> <n>". Players play together only with the same mods: compared
  over the files the game plays (reader archives by changed entry), not by folder, so the same export
  imported by two players matches. Strings, textures, sounds and music do not count.
- (modding.md, Mods in multiplayer; New worlds and words, Multiplayer.)

## 6. Words, mechs, weapons, rules

- **Words**: `strings.txt`, `id=text` (or `NAME@id=text` to give a message a name the game's table lacks,
  for `MSG_...` references in a mission's records). A mod's own `Mech3Msg.dll` is read at IMPORT into
  such lines. (modding.md, New worlds and words.)
- **New mechs and weapons**: `mech=` and `weapon=` lines in mod.txt, their definitions the original's
  formats (`dfn_<name>.zrd`, weapons.zrd's entry). (modding.md, New mechs and weapons.)
- **Salvage**: `salvage_chance=N` (0 to 100, every mission of the campaign) and
  `mission_salvage=<world> <n> <N>` (one mission). The original's is 1 in 2.
- Requirements between mods: `needs=`, `conflicts=`, `game_version=`.

## 7. Reading it back (EXPORT)

EXPORT zips a mod with every archive whole (changed entries laid over the player's) and the loose
files (`mod.txt`, `<world>.gs`, `mw3-checksums.txt`) at the folder's top, never inside `zbd\`. It
changes no byte of an archive it packs. Checked: the original's own readers, run in an emulator, read
every archive of an exported world as the rebuild does (`tests/differential/test_export_readers.py`).
Known: zStudio's Initialize refuses a world whose `reader.zbd` lacks `anim.zrd` (its own exports'), so
a zStudio world exported back by us is refused the same way. (modding.md, Exported.)

## 8. Checking an export without playing it

`mw3 --check-mod <folder or zip>` (or INFO on the MODS screen) reads every file the way the game will and
says, in plain words, what it found and what it cannot use: worlds and their missions, maps, a missing
script or world record, an archive whose table does not read. A malformed file is warned about and left
out; it never stops the rest of the mod.

## Open

- A real zStudio export through IMPORT (Z1 in docs/playtest-1.1.0.md) has not been run yet; everything
  above is tested on worlds made from the game's own files.
- Pirate's Moon reads the same layout with `for=mw3pm`; its new-world rules are being checked against
  its own world folders.

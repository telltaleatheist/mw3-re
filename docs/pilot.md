# The pilot and its files (ported)

Mech3.exe 1.2, 2026-10-20. The pilot is the player's record: a name, five
games (the campaign saves, Instant Action, multiplayer, training and the mech
lab's), and in each a store of mechs and parts and the player's own mech. The
current pilot is [0x0060861c]; every pilot listed is in [0x0060860c] (an STL
list, its count at [0x00608610]). Each lives in `pilots\<name>.mw3`; a
campaign's saved game is `pilots//games//<name><op><mission>.mw3` (two
digits each, the doubled slashes the exe's own). The pilot screen picks one
(menus.md, The pilot screen); gametypes.md maps the game types and where the
game reads them.

Code: `src/mech3/pilot.h`, `pilot.cpp` (the pilot, the list, the files),
`pilot_games.cpp` (the games, the slot, the store). Checked by
`tests/differential/test_pilot.py` (probe `tests/probe_pilot.cpp`).
Original files unknown: the pilot's functions sit between
mech3\mech_skin.cpp and mech3\mech_waynet.cpp (0x004779f0..0x004794f0), the
games and the slot after the option screens (0x00433cc0..0x004379e0), the
store between mech3\mech_salvage.cpp and mech3\mech_skin.cpp
(0x00470ce0..0x00473400).

## 1. The pilot (0x00477b20, vtable 0x005c37a0), 0x84 bytes

| Offset | Field | Set by the constructor |
|---|---|---|
| +0x00 | vtable: +0x00 read (0x004790d0), +0x04 write (0x00479390); the base's (0x005c37a8) only closes the file | |
| +0x04 | 1, the file's first word (checked) | 1 |
| +0x08 | the version: 0x00423cf0 (the loadouts' 15) + 0x20 = 0x2f (checked) | 0x2f |
| +0x0c | 0, the header's third word (written, never checked) | 0 |
| +0x10 | the open file (INVALID_HANDLE_VALUE when closed) | -1 |
| +0x14 | the pilot's mech in a mission (0x004782c0 sets it) | 0 |
| +0x18 | the name, 64 bytes (copied with strcpy) | the argument |
| +0x58 | dirty: the destructor saves the pilot while set; cleared by a write and by deleting the files | 1 |
| +0x59 | a byte, read by nothing here | 0 |
| +0x60 | vector of campaigns: the new one first, then the saved ones read from the file | a new campaign (op 1, mission 1) |
| +0x6c | the current game (0x00478250 picks it) | the new campaign |
| +0x70 | Instant Action (0x00436650, 0x17c bytes) | |
| +0x74 | multiplayer (0x004353a0, 0xb0 bytes) | |
| +0x78 | training (0x00436e30, 0x8c bytes) | |
| +0x7c | the mech lab's (0x004373e0, 0x84 bytes) | |
| +0x80 | the mech's +0xd4 +0x1c, kept across a respawn (0x004782c0, 0x004782f0, 0x004783c0) | 0 |

The games are made in the order campaign, lab, Instant Action, multiplayer,
training; the destructor (0x00477e10) saves first while dirty, then deletes
the campaigns, then the lab, Instant Action, multiplayer and training.

0x00478250 (select the game by its type): 0 the lab, 1 the *last* campaign
(the newest save, or the new one), 2 Instant Action, 3 multiplayer, 4
training. 0x00478ed0 picks campaign i (the pilot screen's GAMELIST). The
getters 0x00479440 / 0x00479460 / 0x00479480 / 0x004794a0 answer the current
game when it is a campaign / Instant Action / multiplayer / training, else 0;
0x00479430 the current game's store (+0x08); 0x00479420 its vt+0x54.

## 2. The games (vtable base 0x005beca0)

Every game starts with: +0x04 the pilot, +0x08 its store, +0x1c a CString
(the campaign's name or file, below), +0x20 the player's slot (mech 3). The
shared save (vt+0x18, 0x00434560) writes the slot, then the store; the read
(vt+0x1c, 0x00434580) reads them back.

**The slot** (0x00433cc0, vtable 0x005bec38), 0x18 bytes: +0x04 the mech
(roster index), +0x08 its configuration (a Config, mech_config.h; deleted with
the slot), +0x0c 0, +0x10 the skin, +0x14 a word (0 at first). Saved
(0x00433ef0): mech, +0x14, skin, then 1 and the configuration (Config::Save
0x00424480) when it has one, else 0. Read (0x00433f80) the same; a
configuration read (0x004242a0, named "temp") replaces the one held without
deleting it (a leak; deleted in the port).

**The store** (0x00470ce0, vtable 0x005c36e0), 0x14 bytes: the loadouts'
item list (mechdef-configs.md: base 0, weapon 1, ammo 2, equipment 3, engine
4 items) with a flag at +0x10 that makes it save as an empty list. The mech
lab's, Instant Action's, multiplayer's and training's stores are filled with
everything the lab offers (0x00470e30, flag set: not saved); the campaign's
hold what the player owns. vt+0x00 (0x00472630) adds n of an item (merged
with one like it, matched by vt+0x24: kind and id, the tech too for
weapons, 0x00473480's rule for equipment; the merged one's place reset, 0 for
a weapon, -1 for the rest), vt+0x08 (0x00471940) n rounds of a weapon's ammo,
vt+0x14 (0x00471d20) takes n away (never the store's own, +0x10 of the item)
and deletes the item at 0; 0x00472a30 takes one of a mech (a base item).
Counts never go below 0.

| Game | Type | Ctor | Fields (offset: default) | In the file, after the slot and the store |
|---|---|---|---|---|
| Mech lab | 0 | 0x004373e0 | +0x38 the level: 7 (vt+0x50 sets 6 for the lab, 7 and 8 for the debug test levels); +0x3c, +0x54, +0x6c three slots, mechs 3, 8, 13 | nothing |
| Campaign | 1 | 0x004345a0 (new), 0x00434820 (read) | +0x38 byte 0; four more stores +0x3c, +0x50, +0x64, +0x78; +0x90 vector of lancemates (slots); +0xa0 mission, +0xa4 operation; +0xa8 3, +0xac 3; +0xb0 byte 0; +0xb4 four words 0; +0xc4 0 | +0xa4, +0xa0, +0x38 (1 byte), +0xa8, +0xac, +0xc4, +0xb4 (16 bytes), the stores +0x3c, +0x64, +0x50, +0x78, the lancemates' count, each lancemate |
| Instant Action | 2 | 0x00436650 | +0x38..+0x40 three malloc'd pointers (the Action screen's, freed here); +0x4c player kills, +0x50 lancemate kills, +0x54 enemy kills, +0x58 attack wins, +0x5c attack losses, +0x60 defend wins, +0x64 defend losses, +0x68 progression waves; +0x6c 1, +0x74 -1, +0x78, +0x7c 0; +0x80 ten slots; +0x170, +0x174, +0x178 (short) 0 | the eight stats, then the Action screen's settings kept outside the game: [0x00605874], [0x00605878], [0x0060587c], [0x00605820..0x00605833] (5 words), then 6 pairs [0x00605838 + 4i], [0x00605858 + 4i] |
| Multiplayer | 3 | 0x004353a0 | +0x74 10.0, +0x78 90, +0x8c byte 1, +0x8f byte 1, +0x94 1, the rest 0 (+0x70 is never set: heap garbage, saved); +0x98 kills, +0x9c deaths | +0x98, +0x9c, +0x70, +0x74, the bytes +0x8c, +0x8d, +0x8e, +0xa8, then +0x90, +0x94, the byte +0x8f, +0x78 |
| Training | 4 | 0x00436e30 | +0x38 0; +0x3c four bytes, mission n passed at +0x3b + n; +0x40 the mission chosen, 1 | +0x40, +0x3c (4 bytes) |

A **new campaign** (0x004345a0 with operation 1 and mission 1, every new
pilot's first game) starts with a store: two mechs 3 (the Bushwacker,
mech_names.h's roster) with their "Primary" loadout (message 0xf20, through
0x004733b0), one mech 3 taken back (0x00472a30), and ammunition: 750 for
weapon 0x11, 4000 for 0x15, 1000 for 2 (the mech lab's weapon list: LRM-5,
machine gun, AC/10).

A **campaign read** (0x00434820 runs the read, 0x00434ae0) ends with
0x00434cb0: the stores +0x50 and +0x78 are added to the campaign's own store
(each item by its count), then those two and +0x64 emptied, and +0xa8 set to
+0xac. The campaign's name (vt+0x0c, 0x00434900) is message 0x21d
"Op %1!d!, Mission %2!d!" with the operation and mission; its saved game
(vt+0x14, 0x00434980 -> 0x00434950) "pilots//games//%s%02d%02d.mw3" with the
pilot's name; vt+0x20 (0x00435330) is whether that file exists (_access).
Both are formatted into the same CString at +0x1c.

## 3. The file

pilots\<name>.mw3, little-endian, written in one pass:

| | Bytes |
|---|---|
| header | 1, 0x2f, 0 (three words) |
| training | slot, store, mission (4), passed (4) |
| multiplayer | slot, store, 33 bytes as above |
| Instant Action | slot, store, the 8 stats, 23 words of the Action screen's settings |
| mech lab | slot, store |
| campaigns | count - 1 (the new one is never saved), then each saved campaign |

A slot is 16 bytes (+ its configuration); a store is a count and the items
(kind, count, id, part, group, + the tech for weapons and equipment, + the XL
byte for engines), 0 for the full stores.

**Opening** (0x00478ce0, false if already open): CreateFileA OPEN_EXISTING,
the header read; when that fails or the first two words differ, the file is
opened again OPEN_ALWAYS (made when missing, *not* truncated) and the header
written over the old one. So a file of another version is not refused: its
header is replaced and the rest read as if it were this version's. Writes
never truncate either: a shorter pilot leaves the old file's tail behind it.

**Loading** (0x00478700, from the listing only): a pilot already listed is
kept as it is; else a new one is made, opened, the header read again and
compared (it always matches now), and the record read (0x004790d0): training,
multiplayer, Instant Action, the lab, then the campaigns' count and each
campaign (read by its constructor), *kept only when its saved game exists*
(the others leaked). A pilot whose read fails is destroyed, which writes it
(it is dirty from its constructor). A pilot read is still dirty: the
destructor saves it again.

**Listing** (0x00478560): `_mkdir("pilots")`, `_mkdir("pilots\games")`, then
every `pilots\*.mw3` loaded by its name without the extension. The search
handle is closed with CloseHandle, not FindClose.

**Creating** (0x004788b0): no name, nothing; a pilot already listed is
returned; else a new one is made, its file written at once (over whatever a
file of that name held: open, the header, the record; then clean), and
listed. **Deleting** (0x00477f50): unlisted, each campaign's saved game
deleted (the new one's name too, 0x00433ca0), then its own file, then
destroyed (clean, so not saved). **Renaming** (0x00477fe0): nothing for the
same name; else each campaign's saved game moved to the new name
(MoveFileA), the name copied, the file moved and written again. Nothing
checks the new name is free. **Deleting the others** (0x004785f0): every
pilot but the current one destroyed (so saved while dirty); the pilot
screen's destructor and the app's stop (0x004808a0, current cleared first)
call it.

The debug menu's campaign entries (0x00478a40, gametypes.md) take a pilot
"DefaultPlayer" (0x005e596c; the startup's Mech Lab one is "Default Player",
0x00480b30), made and saved when missing, select its newest campaign and set
its operation (1..4) and mission (1..the operation's count, 0x00484840).

## 4. Who reads it

The mission code reads the current pilot's game through its vt+0x2c (the
type: heat in multiplayer, kill credit in a campaign, the AI's targeting;
gametypes.md, Where the game type is read), vt+0x48 / +0x4c (operation and
mission: the briefing, mission.cpp), vt+0x34 (the player's slot: the mech
and its loadout, player.cpp), the campaign's lancemates (0x004351c0,
0x00435230) and 0x00478f10 (after a mission: the campaign saved as a new
save or over one, "Saved game already exists. Replace existing game?",
0xfb3). The mech lab edits the current game's store and slot. The pilot
screen lists the pilots, their saves and records (menus.md).

## 5. The port

`mech3::Pilot`, the games and the store keep the original's fields and
order, the five game classes as C++ classes (only the slots the pilot and its
screen use are virtual). The original's Win32 handle is a `std::FILE*`.
Calls out are `mech3::PilotHooks` (the store fill 0x00470e30 and a new
campaign's mechs 0x004733b0 are not ported: the mech lab's tables; the slot's
configuration is the ported Config by default; messages and their
formatting).

Changes (docs/fixes.md):
- FIX (A): files. CreateFileA, ReadFile, WriteFile, SetFilePointer,
  DeleteFileA, MoveFileA, _access and _mkdir on the install's folder become C
  stdio in the write dir (zutl): a pilot found only in a read mount (an
  imported install's pilots\) is copied to the write dir when first opened,
  so its progress stays where the rebuild writes; OPEN_EXISTING and
  OPEN_ALWAYS keep the original's "never truncate"; MoveFileA fails when the
  new name is taken (a new case of the same name moves). FindFirstFileA is
  `zutl::list` (the write dir's and the mounts', each name once, NTFS's
  order). Names are Windows-1252 in the game and UTF-8 on the host
  (`zutl::host`, `zutl::list`).
- FIX (bug): a damaged campaign count, lancemate count (or item count, as in
  the loadouts) read on past the end of the file, making a campaign or slot
  each time, up to 4 billion: a hang. The reading stops at the end.
- FIX (bug): a name of 64 bytes or more (a long pilots\ file name) overran
  the record into the games' vector: a crash at the pilot screen. Such files
  are not listed; names are cut to 63 characters.
- FIX (bug): deleting the current pilot left [0x0060861c] pointing at the
  freed one (the pilot screen's DELETE on the pilot chosen before; a lobby
  launch then used it); it is cleared.
- Not changed: the header written over a mismatched one; the tail left by a
  shorter write; a read pilot staying dirty; the uninitialised +0x70 of
  multiplayer saved (0 here); the campaign saves kept only while their file
  exists; renames onto a taken name.

## 6. Testing

`test_pilot.py` (40 runs: 40/40 identical, about 6,300 calls and 260 files
compared; 500 runs also identical): the original's pilot module and games in
the emulator over a fake file system (CreateFileA, ReadFile, WriteFile,
SetFilePointer, CloseHandle, DeleteFileA, MoveFileA, FindFirstFileA /
FindNextFileA, _access and _splitpath, case-insensitive and case-preserving
as NTFS), MFC's CString and the heap as stubs; the port on a real folder.
The store fill, a new campaign's mechs, messages and the slot's
configuration load and save are stubs on both sides (the configuration a
marked word in the file). The plan: crafted files (random bytes after a good
header, bad headers, short and empty files), the listing, new pilots (names
differing only in case among them), changes to every game's fields and the
Action settings, items added, merged, taken and marked as the store's own in
every store, ammunition, lancemates, slot configurations, saved campaigns
with and without their file, renames, deletions, selections, saves, the
debug start, deleting the others, everything saved and read back; then every
file damaged (cut short, a count set to 0, 1, 2, 5, 0x40, 0x7fffffff,
0x80000000 or 0xffffffff, an item's kind changed, a byte flipped, the header
broken) and the listing, a few changes and saves again. Compared: the calls
out, a dump of every pilot, game, store, item, lancemate and slot and of the
Action settings, and every file's name and bytes. The port's stops at the end
are applied to the original by code hooks (0x004715af, 0x00434244,
0x00479366), an item id cut off by the end pinned to INT32_MAX (0x00472b21),
and a header shorter than 12 bytes reads as zeros (the exe's stack garbage).

Breaking the port showed the test catches it, 33 of 34 (40 runs each): the
games written in another order, the campaign's byte field as a word, a
merged weapon's place, the store's own items taken, the +0x64 store kept,
+0xa8 not reset, the header not rewritten, the name compared without case,
the saves kept on deleting or not moved on renaming, every campaign kept,
the new campaign saved, the Action settings' pairs swapped, multiplayer's
byte late, training's fields swapped, the slot's fields swapped, ammunition
below 0, no save on destroying, the first campaign selected, the starting
store for every new campaign, the current pilot deleted with the others, the
extension kept, the +0x64 store merged, the stores' order, the stats'
order, a second pilot made over a listed one, the lab's line-up, a default,
the campaign index's range, a mech taken by identity, the item's group not
copied, the debug clamp, a rename to the same name saving. Missed, and
unobservable: a slot's configuration kept on reading (the slots read are
always new, with none).

## 7. Left

- 0x00478f10 (the campaign saved after a mission) and 0x004782c0 /
  0x004782f0 / 0x004783c0 (the pilot's mech in a mission), with the mission.
- The store fill 0x00470e30 and a new campaign's mechs 0x004733b0 (the mech
  lab's tables and the dfn loadouts); the games' other virtuals (the
  campaign's next mission 0x00434e00, the training's mission names are
  ported as ids, 0x00437060 / 0x004370d0).
- The bridge to the mission side: the mission code still reaches the pilot
  through [0x0060861c] as a module-memory handle (GameEngine sets it to the
  pilot's handle when the screen chooses one), not through `mech3::Pilot`.

# zSave: the saved game's world (ported)

A campaign's saved game is two files: the pilot's (`pilots\<name>.mw3`,
pilot.md: the games, their stores, slots and lancemates) and, for each saved
campaign, `pilots\games\<pilot><op><mission>.mw3`, written by zSave. The
pilot keeps a saved campaign only while that second file exists (vt+0x20,
0x00435330), so without zSave a pilot read back loses every saved
campaign.

zSave is 0x005a8a20..0x005a9970 (the source file is not named: it sits
between `zentity\zent_init.cpp` and `zsiscript\si_script.cpp`; its header
entry is "zSaveHeader"). Ported: `gamez/zsave`.

## 1. The manager

`new(0x34)` at [0x00736580], made by InitInstance (0x005a8b30, from
0x00480020) and freed by ExitInstance (0x005a8bb0, from 0x0047fa10). Every
entry point does nothing (or answers 0) before it is made.

| Offset | |
|---|---|
| +0x04, +0x08 | the registrants: a `std::list` (0x1c-byte nodes) and its count |
| +0x0c | the archive (zutl_zar's 0x18-byte object) |
| +0x24, +0x28 | the load's buffer: its size and pointer, grown, never shrunk |
| +0x30 | stop: a registrant's load sets it to skip the rest of the file |

A registrant (the node's data, 0x14 bytes): its name (the pointer kept, not
copied), the save call, the load call, a priority and a user word.

- **0x005a8a20** (ECX name, EDX save; load, priority, user): added at the
  list's end unless one of that name (strcmp) is there already.
- **0x005a8a70** (ECX path, EDX kind), 0x005a8da0: the archive made afresh
  (zutl_zar's create: truncated), the entry "zSaveHeader" written (8 bytes:
  8, the kind), the list sorted by priority (0x005a92e0: `std::list::sort`,
  a stable merge, lowest first), each registrant's save called (ECX a
  context {manager, registrant}, EDX its user word), the archive closed
  (the table written). True when the file was made.
- **0x005a8a50** (ECX context, EDX sub-name; data, size), 0x005a9140: an
  entry named `"<owner>/<sub>"` ("%s/%s" into 80 bytes).
- **0x005a8a90** (ECX path, EDX kind), 0x005a8e40: unless the kind is -1,
  the header's kind (0x005a90d0) must be that one, else false. The archive
  opened (false when it does not open); +0x30 cleared; then each entry but
  the header: its name split by strtok into the owner (to the first "/")
  and the sub-name (from there to a space), the registrant of that owner
  found (strcmp: case counts), the entry read into the buffer and the
  registrant's load called (ECX the context, EDX the sub-name; data, size,
  user). An entry whose owner is not registered is passed over. After each
  registrant's load, +0x30 set ends the loop. The archive closed; true.
- **0x005a90d0** (ECX manager; path): the header entry read on the
  manager's archive; its kind when the entry is 8 bytes and its first word
  8, else -1.
- **Temporary files** (for the anim registrants, save 2): 0x005a8ab0
  `tmpfile()`; 0x005a8ad0 (ECX file, EDX context; sub) the file's bytes as an
  entry, then `_rmtmp` (every temporary file closed and removed);
  0x005a8af0 (ECX data, EDX size) a temporary file holding them, rewound;
  0x005a8b10 `_rmtmp`.

## 2. Who registers, and what MW3 turns on

| Owner | Save, load | Priority | Registered by | Its switch | In MW3 1.2 |
|---|---|---|---|---|---|
| GWWorld | 0x004f31a0, 0x004f3230 | 1000 | zClass's reset 0x004f8f70 | [0x005ea82c], 0x004f8f60 | off (0) |
| zDEClient | 0x00500b50, 0x00500c20 (craters and quicksand, ported: zdec_crater) | 1000 | the decals' load 0x00501ab0 | [0x005eba1c], 0x00501f90 | off (0) |
| AnimActivation | 0x0050b710, 0x0050b970 | 0x32 | zEffect's start 0x00508fc0 | [0x005ebcc0], 0x005090d0 | **on (1)** |
| RunningAnim | 0x0050c190, 0x0050c250 | 0x33 | the same | the same | **on** |
| Anim | 0x0050c640, 0x0050c890 | 0x34 | the same | the same | **on** |
| Weapons | 0x00583830, 0x00583850 (one word, "WeaponData": [0x009a17c0]) | 1000 | zWeapon's start 0x00583750 | [0x005f1e5c], 0x00583740 | off (0) |

InitInstance (0x00480020) sets the switches right after making the manager:
anim on, the rest off. **So a saved game holds only the header and the
mission animations' state**: what the world's objects were doing and which
animations had run. The world's own state (GWWorld), the craters and the
weapons in flight are not saved; their code is there, switched off.

## 3. When it runs

- **Saved**: at a won mission's end, the pilot's save (0x00478f10(1)) makes
  the next mission's game (0x00434e00) and zSaves into that game's file
  (kind 2), while the mission just won is still loaded. So `bob0102.mw3`
  holds Op 1 Mission 1's animation state at its end. (With `zfix` bugs the
  write waits until the game is filed, campaign.md section 4.)
- **Read back**: the mission load (0x00490ab0, after the mission's vehicles)
  asks the pilot's game (vt+0x28). A campaign's (0x004352d0) reads its file
  (kind 2) when the game is not the one the last mission's end made (+0xb0,
  which the pilot file does not keep, so every game read from the file
  qualifies), is not Op 1 Mission 1 or Op 4 Mission 4, and its file exists;
  then "Saved Game Loaded" (0x00405120). Every other game's vt+0x28
  (0x004b3d30) answers true without reading anything, so Instant Action,
  training and multiplayer loads all say "Saved Game Loaded".

What the read gives the new mission is the last mission's animation state,
by def name and index and by node index. That is sound because a campaign
operation's missions share one world (c1..c4; c4b from Op 4 Mission 4), and
the two missions the restore leaves out, Op 1 Mission 1 and Op 4 Mission 4,
are exactly where the world changes. So a reloaded campaign starts the next
mission with the last one's animations as they ended: what had been
destroyed, opened or triggered. Going straight on from the debriefing reads
nothing (+0xb0), so the same mission then starts fresh.

### The anim registrants (zeff_anim_save, ported)

All three are registered by zEffect's start (0x00508fc0) when [0x005ebcc0]
(0x005090d0, 1) is set, in the priority order 0x32, 0x33, 0x34. The win's
save runs with the mission as it is: the Core state is only suspended under
the end screen, its fade and the Win State, so its CoreState::leave and the
mission's end there (0x0050ace0(1), every def and instance stopped) come
later. The save holds the runs going on when the mission was won (a live
c1m1 win: 80 RunningAnim entries among 1,849).

- **AnimActivation** (0x0050b710 / 0x0050b970): the animation call log
  (anim-postload.md, the loggers) replayed. Each record is an entry
  `Activation%04d`: the 0x50-byte record, then (+0x54) the def's status,
  (+0x55) its object count and the objects' node states, 0x44 each (the
  node's live index, -1 for none; active; whether it keeps a matrix; the
  matrix or translation, rotation and scale; the model's flag 8 and its
  +0x24). The save binds each def to the record's node first (0x00509eb0),
  so saving moves defs. On the load the first entry (`Activation0000`)
  resets every def the new mission has logged so far and empties the log;
  every record then makes its call again (0x0050ca90: start at, start,
  connector, connector at; a reset restarts), the def is marked restored
  (+0x94 0x4000) and brought to the saved status: a reset (1) resets it and
  logs the record again, executed (3) and invalid (4) stop it and log it, a
  run (2) starts it again, a resting run (6) only from invalid. The objects
  are put back as they were; the model's flag is only ever set back, never
  cleared.
- **RunningAnim** (0x0050c190 / 0x0050c250, the writer 0x0050be30): each def
  running (2) or resting (6), and its running instances (only when the def
  itself is saved), as `Running%03d` (the def's index: a def and its
  instances share the name). Made in a temporary file: a 0x68-byte head (the
  index, whether it is the def's own record, the name, its nodes as live
  indices and their offsets, the status, the clocks +0xa8 / +0xb0, the start
  vector, the three counts), each sequence (0x40, the current event as an
  offset) with its events, the lights (0x38) and sounds (0x3c) with their
  positions and parents. The load puts it back without starting anything:
  the sequences' events replaced, the lights and sounds made then but not
  now made where they were, the def's object ticking it again (0x00507de0).
  A def's own record goes on the instance at the saved node; with none, a
  running def is given a new instance that is left idle and the state goes
  on the def. An instance's record goes on the def itself, so a def with
  running copies ends with the last copy's run.
- **Anim** (0x0050c640 / 0x0050c890): every def not in error (5), as
  `Anim%04d` numbered after the activations (the def's index plus the log's
  count): its index, status and objects' node states. Loaded only for a def
  the activations did not restore: reset if it was reset (1), its running
  copies reset, its objects put back.

A def's own setting (+0x94 0x1000 with 0x2000 off) keeps it out of
RunningAnim and Anim, as it keeps it out of the log; [0x005ebf0c] off keeps
all of them out.

**What the win's save holds**: the activations, every def's state and the
runs going on at the win (a live c1m1 win: 123 activations, 80 runs, 1,646
Anim entries).

**Stack leftovers.** The exe builds each record on its stack and does not
clear it: what a record does not set (the activations' +0x50, an object's
tail when it has no node, a light or sound without its node, the Anim
entries' +0x54 when the def has no objects, which keeps the last def's)
holds whatever was there, and a running sequence's events are written as
their address and the current event as cur - events (garbage for one never
started). The port writes zeros, 0 for the address and -1 for an offset
outside the events. The loads never read those bytes (a light or sound is
remade only when its saved +0x24 says it was made, and then its fields were
written), so a save written by the original loads in the port the same way
(the test cross-loads every one), and the port's in the original.

## 4. Damaged and odd files

What the original does with each (`test_zsave.py --nofix` shows it), and the
port:

- **The kind differs** (or the header is gone, not 8 bytes, or its first
  word is not 8): the load answers false before opening the rest.
- **Cut short**: the last 8 bytes are no longer the version (1) and count,
  so the archive does not open (or a file under 8 bytes): false.
- **The table's count does not fit the file** (larger than the bytes before
  the tail, or negative): the archive's read (0x005767b0) seeks before the
  file's start (refused, the position kept), reads nothing into a buffer of
  that many entries (or fails to allocate one) and keeps the count; zSave
  then reads the names from that buffer and crashes (or, with a negative
  count, loads nothing and answers true). **FIX (bug)**: such an archive
  does not open (zutl_zar's read_table, shared by every archive).
- **An entry's name has no "/<sub>"** (no slash, a trailing one, an empty
  name): the second strtok gives null and strncpy reads address 0: a crash.
  **FIX (bug)**: the entry is skipped.
- **An entry longer than the whole file**: the exe allocates that length
  (MFC's new throws when it cannot: a crash). **FIX (bug)**: skipped. One
  that only runs past the end is read short, as the exe does; the rest of
  the buffer is whatever it held (zeros when it was just made, here and in
  the test).
- **An entry's start at 2 GB or more**: SetFilePointer takes a signed
  distance, so the seek fails and the read takes what follows the last
  one. zutl_zar's seek does the same (FIX (A) kept faithful).
- **A registrant missing** (its owner not registered at the load): its
  entries are passed over silently; the load answers true.
- **The same name registered twice**: the second is refused.
- **A sub-name with a space**: it ends there ("a b" gives "a"); one with a
  "/" keeps it ("x/y").
- **An anim entry that does not fit** (shorter than its head, a def index
  outside the defs, an object count larger than the entry, more sequences
  than the def has, a node index past the node pool): the exe indexes and
  reads past them (a light or sound is added under whatever lies past the
  pool: a crash). **FIX (bug)**:
  such an entry is passed over, or only what it holds is read.

## 5. The port

`zsave::Manager` keeps the fields; the list is a `std::list` sorted with
its own stable `sort`; strtok is a local tokenizer with the CRT's two-call
behaviour; `_rmtmp` closes the temporary files zSave made. The pilot's
hooks `save_world` and `load_world` are zSave by default; GameEngine's
`save_init` / `save_shutdown` (the hooks InitInstance and ExitInstance call;
they were misnamed `entity_init` / `entity_shutdown`) make and free the
manager; the mission load's `game_saved` asks the pilot's game's
`restore()` (vt+0x28) when the launch came from the menus. The anim
registrants are `zeff_anim_save`'s (activation_save .. anim_load), added
by `anim_init`; `GameEngine::anim_enable` is 0x005090d0.

## 6. Testing

`test_zsave.py` (200 runs: 200/200 identical): the original's zSave and
zutl_zar in the emulator over a fake file system, the port on a real
folder. Fake registrants (eight names, one differing from another only in
case) write their entries from an x86 routine through 0x005a8a50 and log
each load call; one may set +0x30. The plan: registrations at random
priorities (the same name twice too), saves of several kinds, the header's
kind, loads with the right kind, any and another, loads after a new
manager with registrants missing, then each file damaged (cut, the table's
count, an entry's start, length or name, the header's entry and first
word, the version, random bytes) and loaded again. Compared: every
result, each load call (registrant, sub-name, size, bytes) and every file's
bytes (the exe's entries carry stack leftovers in the flags, comment and
time, zeroed after each save; the port writes zeros). The three FIXes are
applied to the original by code hooks; `--nofix` leaves them out.

Breaking the port showed the test catches it, 18 of 18 (40 runs each): the
name check on adding, the sort reversed or left out, the header's kind or
first word, the entries named with a dot, the header read without its size
or word checks, the load's kind not checked, the sub-name split at "/", the
owner matched without case, +0x30 ignored or not cleared, empty entries
skipped, an entry one byte short, the seek past 2 GB made, and the two
damaged-table fixes left out (the port then runs out of memory or crashes,
as the exe does). Two breaks are invisible and left out: the last of two
registrants of a name (the second is refused) and the header entry passed
on (it has no sub-name, so it is skipped either way).

The round trip (`campaign_menus_to_c1m1_through_the_briefing_headless`,
both `zfix` passes): a test registrant writes an entry at c1m1's win; after
c1m2's launch the app quits, the pilots are freed (saved), and a new app
reads them from the write dir; the pilot screen's pilot chosen, the newest
save is Op 1 Mission 2 with its file there; at c1m2's briefing the mission
load has given the registrant its entry back, and the slot, its loadout,
the lancemates, the record and what the player holds (the store and the
mission's salvage together: the briefing reallocates between them, and the
load merges the last launch's settled lance back) are as they were. Breaking
it, 4 of 4: no world save (the pilot read back drops the save: c1m1 comes
up), no manager (the same), the campaign's vt+0x28 never reading, the
mission load not asking it.

`test_anim_save.py` (the six worlds): the original's anim registrants in
the emulator on test_anim_run's worlds after frames of random calls, zSave's
calls caught (the entries kept as they are written; the temporary files over
fwrite and fread), against the port's real saved game. Two saves per world:
the win's (the runs as they are) and one after the mission's end
(0x0050ace0(1), every run stopped; MW3 makes none there). Compared: every entry's name and words (floats within
1e-4: the rotations come from the matrices), the state after each save;
then the world read again on both sides, each save loaded (the original's
entries given to its loads in the file's order), the whole state compared,
the original's entries written as a file and loaded by the port in a third
world (the cross-load) and compared, and frames of random calls run on to
show the restored runs go on alike. The savers' stack frames are zeroed on
entry (the port writes zeros), and the event addresses normalized as above.
A third world is played first and every def's object nodes hidden or shown
and moved (through 0x004eb170 / 0x004fc0b0 on both sides) before the win's
save is loaded into it; entries cut short or given a def index, object
count or sequence count that does not fit are loaded by the port, which
must come through. A fourth world has the states random play does not
reach at a save set directly on both sides (a def in error, a running def
with a running copy at another node, with the def's last record moved to
its own node so the activations' save does not bind it onto the copy's,
one with a resting copy, and a def logged only without a node set to reset),
is saved, and the save is loaded into a world where the reset def is invalid
(4). On the six worlds (60 frames, 20 after): everything
identical (the win's saves held 407 runs, the stopped ones none).

Breaking the port, on c1: 23 of 27 caught. Caught: the save's bind left
out, a reset's objects saved, every object restore left out, the first
entry's reset, the 0x4000 mark, an executed record not logged again, a
reset record on an invalid def left invalid, resting copies saved, a def's
setting ignored in RunningAnim, the current event's offset, the clocks, the
main flag inverted on the load, the made test, the tick, the status, the
Anim numbering, defs in error saved, the restored defs not skipped and
running copies not reset in Anim's load, the registration order; and the
three damaged-entry fixes left out (the port then crashes). The node-pool
check is the one damaged c4's test found (a light added under memory past
the pool: a crash); left out, the port crashes only on some runs, as what
lies past the pool is undefined. Not seen, with
the reason: the status read after the bind (the bind changes it only when a
root is missing: no world has one), the model's morph flag (no saved object
morphs), the matrix case (no saved object keeps a matrix), and the active
flag alone (the replays put every object's flag back themselves).

In the program (`tools/analysis/savecheck.py`, four runs of build/mw3 with
`--nosound --hidden` on a scratch write dir): c1m1 won with `--win 60`
(the end screen, the fade, the debriefing; the Win State's save writes
`pilots\games\Briefing0102.mw3`: 1,849 entries, 123 activations, 80 runs,
1,646 Anim), then straight on to c1m2's briefing; in a second dir the same
win, the app quit at the debriefing, and a new run from the plain start
(CAMPAIGN, the pilot, ACCEPT on the newest save): "Saved Game Loaded" at
c1m2's load, and its briefing holds what the straight-on one did (the store
and salvage by item, the mech and its loadout, the lancemates), its
animation log the replayed one (140 records against 143 straight on). Then
Op 1 Mission 1 chosen again and won: "Saved game already exists. Replace
existing game?" answered No keeps the game's file as it was, OK rewrites it
(the rebuild's order; the original's, which writes it first, is
`test_pilot.py`'s and the campaign test's). Breaking the port, 5 of 5: the
campaign's vt+0x28 never reading, no world save, the world written before
the answer, No filing the new game, the anim registrants not registered.

## 7. Left

- The original's order for "Replace existing game?" is not reachable in the
  program (no switch for `zfix` bugs there); the headless tests cover it.

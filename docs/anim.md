# Mission animations (`anim.zbd`)

Mech3.exe 1.2, 2026-10-19. Ported: `zeff::anim_read_zbd` 0x50a0e0 with
0x5090e0, 0x5090f0, 0x509150 and the lookups 0x509640, 0x5096d0, 0x5091e0,
0x509160 (`src/gamez/zeffect/zeff_anim_init.*`), on the ported zClass: the
list walk 0x4f8b20/0x4f8bd0, 0x4f7fe0, 0x4f9d60, the light and sound
creators 0x4fa720 / 0x4fd280 / 0x4fd480, 0x4eb300, 0x4eb170. Still hooked
(`zeff::AnimHooks`): puffers 0x54a710, zSound's lookup 0x56f6f0, the effect
table 0x50d9e0, the dead tree copy 0x4f9b50. `test_anim.py` reads each
mission's world with 0x4ff520 in both, then its anim.zbd: the hooked calls,
every def, list, blob and sequence (nodes as pool slots) and the whole node
pool and lists after are identical on all six; so are anim files read
against the wrong world (lookups fail for real) and runs where one hooked
call fails. The data never tells apart child order in 0x4f9d60 or light
entry 0 in 0x509640 (deliberate breaks there pass). `zbd/<db>/anim.zbd`
(c1, c2, c3, c4, c4b, t1) is the compiled form of the mission's animation
definitions (`.zrd` text in the authoring build). It is not a zArchive. It is
a header, a list of the source files it was compiled from, a 0x44-byte global
block, then one variable-length record per animation definition, then an
optional message table. Everything is read with CRT `fopen`/`fread` in one
pass and no seeking, by **0x50a0e0** in `zeffect\zeff_anim_init.c`.

`tests/differential/explore/anim_walk.py` walks every shipped file in the
exe's read order and ends exactly at the file size on all six. With `--emu` it
also runs the original 0x50a0e0 in Unicorn (CRT and node lookups stubbed); on
all six files it returns 0 having read every byte of the file.

| File | Bytes | Source list | Defs (incl. empty def 0) | Messages |
|---|---|---|---|---|
| c1 | 5,228,852 | 529 | 1770 | 0 |
| c2 | 3,947,704 | 476 | 1422 | 0 |
| c3 | 3,879,480 | 352 | 1472 | 0 |
| c4 | 4,780,384 | 445 | 1422 | 0 |
| c4b | 1,791,044 | 160 | 1071 | 0 |
| t1 | 1,473,220 | 135 | 1097 | 0 |

Totals over the six: 21,007 object refs, 29,581 node refs, 855 lights, 655
puffers, 304 dynamic sounds, 3,721 static sounds, 0 effects, 79 activation
prerequisites, 3,700 anim refs. Every def has status 0.

## Where it is opened and by whom

All in the mission load, 0x48faf0 (`mission.cpp`):

- 0x48fb70: `0x509150(1)` sets 0x5ebcbc = 1, the "loading allowed" gate.
  This is the only caller; the gate is on for every mission.
- The mission script (`support\compile.gw`, see `docs/interp.md`) runs
  `AnimSetZBDFile zbd\<db>\anim.zbd`. The command (name 0x5f3f64, compared at
  0x591462) calls **0x5090f0 `anim_set_zbd_path(path ecx)`**: a path of 0x80
  characters or more logs `Animation ZBD filename too long: %s` and is
  ignored (the old path stays); otherwise it is copied to **0x60c570**.
- After the world, 0x490121: `0x5090e0(world? edi)` stores a node (probably
  the world, unverified) in 0x73f738. Then 0x49012b: **0x50ae60
  `anim_load(ecx = "anim.zrd")`**. The ECX string is not used: a leftover of
  the text path. The progress bar reads `Loading Animations` (0x5e732c).
- 0x50ae60: if 0x73f720 (loaded flag) is already set, returns 0. Else
  0x549410 (resets the puffer tick node, `gmod_puff`), then **0x50a0e0**,
  **ignoring its return value**, then the post-load pass below, then
  0x73f720 = 1.

`zerr_report` (0x49bfd0) is an empty stub in this build, so the two messages
the loader can log are silent. The loader returns 0 or -1; nobody checks it.

Reset/free: 0x508fc0 (anim system init, reached from `zeff_init` 0x50d2a9)
calls 0x50b380 if loaded, clears 0x60c570 and the globals, sets gravity
0x73f73c = -9.8 and 0x73f75c = 1, fills a 200-float random table at
0x73f400..0x73f720 (`rand()` scaled by about 1/32768), and registers three savegame
savers. 0x50b380 frees 0x73f724, every def (0x50b0e0: events, seq defs,
every ref list), 0x73f72c, 0x73f734.

## 0x50a0e0 `anim_read_zbd()` (no args; returns 0 or -1)

1. 0x5ebcbc == 0 -> -1. Empty 0x60c570 -> -1. `fopen(0x60c570, "rb")`
   (IAT 0x5bb628); null -> -1.
2. `fread` 0x0c bytes (header). Short read, signature != 0x08170616 or
   version != 39 (0x27): `fclose`, -1.
3. Source list: count = header +0x08 -> 0x60c5f8; `malloc(count * 0x54)` ->
   0x60c5fc; `fread(buf, 0x54, count)`; then the buffer is **freed at once**
   and both globals zeroed. Only a short read matters (-1).
4. Global block: 0x73f738 is saved, `fread(0x73f720, 0x44, 1)` straight over
   the globals, then 0x73f734 = 0 and 0x73f738 restored. Short read -> -1.
5. `defs = malloc(def_count * 0x13c)` -> 0x73f72c, zeroed. def_count is the
   **signed** 16-bit 0x73f72a.
6. For each def, in file order (def 0 included, read the same way):
   - `fread(def, 0x13c, 1)`.
   - Clear the pointers: +0x114..+0x137 (the nine list pointers; +0x138 is
     not cleared), +0xc4 (seq defs), +0x100 (reset events).
   - For each list whose count byte is non-zero, `malloc(count * size)`, store
     the pointer, `fread(ptr, size, count)`:

     | Order | Count byte | Pointer | Record | List |
     |---|---|---|---|---|
     | 1 | +0x109 | +0x114 | 0x60 | object refs |
     | 2 | +0x10a | +0x118 | 0x28 | node refs |
     | 3 | +0x10b | +0x11c | 0x2c | light refs |
     | 4 | +0x10c | +0x120 | 0x2c | puffer refs |
     | 5 | +0x10d | +0x124 | 0x2c | dynamic sound refs |
     | 6 | +0x10e | +0x128 | 0x24 | static sound refs |
     | 7 | +0x10f | +0x12c | 0x24 | effect refs |
     | 8 | +0x110 | +0x130 | 0x30 | activation prerequisites |
     | 9 | +0x112 | +0x134 | 0x48 | anim refs; then each +0x44 = 0 |

   - If +0x108 (seq def count) != 0: `malloc(count * 0x40)` -> +0xc4.
   - Reset sequence: `fread(def + 0xc8, 0x40, 1)`, over the copy that came in
     the 0x13c record (identical in every shipped def). If its size (+0x104,
     signed) > 0: `malloc(size)` -> +0x100, `fread(+0x100, size, 1)`.
   - For each seq def: `fread(&seq[i], 0x40, 1)`; if its size (+0x3c, signed)
     > 0: `malloc` -> +0x38, `fread(+0x38, size, 1)`.
   - Any short read: `fclose`, -1 (what was allocated stays allocated).
7. Fixup pass, below (defs 1..n-1).
8. Messages: if 0x73f730 (global +0x10) > 0: `malloc(count * 0x24)` ->
   0x73f734, `fread(…, 0x24, count)`, then for each `+0x20 = 0x575f90(msg)`
   (calls the hook in 0x72c670 if set, else 0): the message-id lookup
   `zutl::msg_id`, also used for the camo names. Shipped count is 0.
9. `fclose`, 0.

The event blobs are never looked into at load. The runtime walks them in
`zeff_anim_run.c` (0x507c9d: `cur += [cur+4]` until `cur >= +0x38 + +0x3c`;
`Invalid Sequence Event` at 0x507cee).

### Header (0x0c)

| Offset | Field | Shipped | Exe use |
|---|---|---|---|
| +0x00 | signature | 0x08170616 | checked |
| +0x04 | version | 39 | checked |
| +0x08 | source list count | 135..529 | records to skip |

### Source list (count x 0x54)

`name[0x50]` (`..\data\c1\zrdr\anim.zrd`, `..\data\common\zrdr\weapons\…`;
garbage after the NUL), +0x50 timestamp (mech3ax: Unix time). Read and thrown
away.

### Global block (0x44, read over 0x73f720)

| Offset | Global | Field | Shipped | Exe use |
|---|---|---|---|---|
| +0x00 | 0x73f720 | loaded flag | 0 | overwritten by the file, set to 1 by 0x50ae60 |
| +0x04 | 0x73f724 | pointer | 0 | kept; **freed by 0x50b380 if non-zero** |
| +0x08 | 0x73f728 | u16 | 0 | kept; zeroed by 0x50b380; use unknown |
| +0x0a | 0x73f72a | def_count (s16) | 1071..1770 | loop bound everywhere |
| +0x0c | 0x73f72c | defs pointer | authoring pointer | replaced by `malloc` |
| +0x10 | 0x73f730 | message count | 0 | messages read after the defs |
| +0x14 | 0x73f734 | messages pointer | 0 | zeroed, then `malloc` |
| +0x18 | 0x73f738 | world pointer | authoring pointer | **file value discarded**, the 0x5090e0 value is restored |
| +0x1c | 0x73f73c | gravity (f32) | -9.8 | taken from the file |
| +0x20..+0x38 | | | 0 | taken from the file; not used by the loader |
| +0x3c | 0x73f75c | | 1 | taken from the file; init also sets 1; use unknown |
| +0x40 | 0x73f760 | | 0 | |

## Def record (0x13c)

Field names from mech3ax where the loader does not use them.

| Offset | Size | Field | Loader use |
|---|---|---|---|
| +0x00 | 0x20 | anim name (`ams_blast`, `gaus_muzl.flt`) | key for animation prerequisites; post-load names an object `_<anim name>` |
| +0x20 | 0x20 | node name (`ams_muz.flt`) | the node this def drives -> +0x40 |
| +0x40 | 4 | anim node | **lookup** (below) |
| +0x44 | 0x20 | anim root name | -> +0x64 |
| +0x64 | 4 | anim root node | **lookup** via 0x509640 |
| +0x68 | 4 | | post-load: new object3d `_<anim name>` (0x50ae60) |
| +0x6c..+0x93 | | zero in the file | |
| +0x94 | 4 | flags | 0x80000 = local nodes only (0x509640); 0x20000 set when the node tree is copied (dead path); post-load reads 0x20, 0x8000, 0x100000, sets 0x200 |
| +0x98 | 1 | status | 5 = corrupt: skipped by the fixup and post-load; 4 also skipped by post-load |
| +0x99 | 1 | activation | post-load: 0 weapon hit, 1 collide hit, 2 both, 4 on startup (mech3ax names); shipped 0:729, 1:68, 2:881, 3:6391, 4:185 |
| +0x9a..+0xc3 | | priority, range, reset time, health (mech3ax) | not used by the loader |
| +0xc4 | 4 | seq defs | `malloc(count * 0x40)` |
| +0xc8 | 0x40 | reset sequence (a seq def named `RESET_SEQUENCE`) | re-read from the file after the lists |
| +0x100 | 4 | (= reset +0x38) reset events | `malloc` |
| +0x104 | 4 | (= reset +0x3c) reset events size | |
| +0x108 | 1 | seq def count | |
| +0x109..+0x110 | 1 each | list counts (table above) | |
| +0x111 | 1 | prerequisites min to satisfy | not used by the loader |
| +0x112 | 1 | anim ref count | |
| +0x113 | 1 | pad | |
| +0x114..+0x134 | 4 each | list pointers | `malloc` |
| +0x138 | 4 | zero | not cleared |

### Seq def (0x40)

+0x00 name[0x20]; +0x20 state (u8; runtime state byte); +0x21 reset state
(runtime: 3 = ends after the last event); +0x24..+0x33 times and loop count;
+0x34 current event (runtime); +0x38 events pointer (`malloc`); +0x3c events
size in bytes. A size <= 0 reads nothing (none shipped).

### Events (inside a blob, runtime layout)

12-byte header: +0x00 type (u8), +0x01 start offset (u8), +0x02 pad, +0x04
total size including the header, +0x08 start time (f32); then the data. The
walk checks the sizes tile every blob exactly. Counts by type (mech3ax names):
1 sound 2669, 2 sound node 156, 4 light state 1221, 5 light animation 215,
6 object active 21001, 7 translate 902, 8 scale 1452, 9 rotate 390, 10 motion
10082, 11 motion from-to 2000, 12 motion SI script 1141, 13 opacity 1460,
14 opacity from-to 2085, 15 add child 82, 17 cycle texture 420, 18 connector
48, 19 call connector 12, 22 call sequence 5279, 23 stop sequence 4837,
24 call animation 19065, 25 stop animation 534, 26 reset 3, 27 invalidate
808, 28 fog 7, 30 loop 2611, 31 if 4245, 32 else 2774, 33 elseif 1493,
34 endif 4245, 35 callback 198, 36 fbfx colour 198, 41 detonate weapon 82,
42 puffer state 842. The per-event layouts are for the runtime trace
(0x5079b0.., one handler per type), not this one.

### Ref records

Entry 0 of every list except prerequisites and anim refs is a blank
placeholder; the fixups start at index 1.

| List | Size | Layout | Fixup (index >= 1) -> field |
|---|---|---|---|
| object | 0x60 | name[0x20], +0x20 0, +0x24 node, +0x28 flags, +0x2c flags merged, +0x30 affine 3x4 f32 | `0x509640(def, name)` -> +0x24; 0 = fail |
| node | 0x28 | name[0x20], +0x20 0, +0x24 node | same |
| light | 0x2c | name[0x20], +0x20 flags, +0x24 node, +0x28 in world | **new** light 0x4fa720 -> +0x24 (0 = fail), named `0x4eb300(light, name)` |
| puffer | 0x2c | same as light | **new** puffer object 0x54a710 (0xe4 bytes, put on the puffer list 0x72be04; not named here) -> +0x24 |
| dynamic sound | 0x2c | same as light | **new** sound node 0x4fd280 -> +0x24 (0 = fail), named 0x4eb300, then `0x4fd480(node, name)` sets its sound (copies the name, `zsnd_find`) |
| static sound | 0x24 | name[0x20], +0x20 handle | `zsnd_find(name)` 0x56f6f0 -> +0x20; 0 is accepted |
| effect | 0x24 | name[0x20], +0x20 index | `0x50d9e0(name)`: index in the effect table (0x73f3d4, stride 0x4c, count 0x73f3c8) -> +0x20; -1 = fail |
| prerequisite | 0x30 | +0x00 optional (u32), +0x04 type (u32; byte read), then type 1: +0x08 anim name[0x20], +0x28 def; types 2/3: +0x08 active (u32), +0x0c node name[0x20], +0x2c node | below; all entries, from 0 |
| anim ref | 0x48 | name[0x40] (two 0x20 names for a call to an object connector), +0x40 type, +0x44 pointer | +0x44 = 0 at read; not resolved by the loader |
| message | 0x24 | +0x20 result | `0x575f90` |

## Fixup pass (inside 0x50a0e0)

For each def i (offset `i * 0x13c`), with `prev_name` = 0 at the start:

1. Save the light, puffer and dynamic sound counts and set all three to 0, so
   the lookups below cannot return the authoring pointers still in those
   lists. Def 0 and status-5 defs stop here, **with the three counts left at
   0**.
2. +0x40 = 0. If `prev_name` != 0 and `strcmp(prev_name, def+0x20) == 0`:
   +0x40 = `0x4f8bd0(0, 0)`, the **next** node of that name. If +0x40 is
   still 0: `0x4f8bd0(def+0x20, 6)` starts a name search over list 6 (every
   node, `docs/zclass-world.md`), `0x4f8bd0(0, 0)` returns the first match,
   `prev_name = def+0x20`. 0 -> fail.
   So a run of defs with the same node name binds to successive nodes of
   that name; shipped files have 2,712 such follow-on defs, in runs up to 58
   long (c1). Running out restarts at the first node.
3. If 0x60c5f0 != 0 and `0x4ed8b0(node)` (root) has type other than 1 or 2:
   +0x40 = `0x4f9b50(node, 0x5ebcb0, 0x5ebcb4, 0x5ebcb8)` (a copy of the
   tree) and flags |= 0x20000. **0x60c5f0 is never written** (bss, 0): dead
   in this build.
4. +0x64 = 0, then +0x64 = `0x509640(def, def+0x44)`. 0 -> fail.
5. Restore the light count; lights 1..n-1 as in the table. Then puffers,
   then dynamic sounds, then objects, nodes, static sounds, effects.
6. Prerequisites, for each entry in order, with `last` = 0:
   - type 1: +0x28 = 0, then the first def k in **0..def_count-2** (the rebuild searches all, `fixes.md`) whose
     anim name (+0x00) equals +0x08. Not found leaves 0, no error.
   - type 2 or 3: `node = last ? 0x4f9d60(last, name) : 0x509640(def, name)`.
     0x4f9d60 searches `last` and its children, depth first. Not found: logs
     `ACTIVATION_PREREQUISITE error; couldn't find node.` (line 0x33a0) and
     **sets the prerequisite count to 0** (the def loses all of them; the
     load goes on). Found: +0x2c = node, `last = node`, and type 2 resets
     `last` to 0. So a type-3 (parent) entry is looked up under the node found
     by the entry before it.
   - other types: skipped.

### 0x509640 `anim_find_node(def ecx, name edx)` -> node or 0

In order, first hit wins:

1. 0x5096d0(def+0x64 root, name): the node itself or any descendant by
   `strcmp`, depth first (children at +0x5c count, recursion).
2. 0x5096d0(def+0x40 anim node, name).
3. 0x5091e0(def, name): index of the light ref whose created node's name
   (node +0x00) matches; index > 0 -> that ref's +0x24.
4. 0x509160: the same over the dynamic sound refs.
5. Unless flags & 0x80000: 0x4f7fe0(list 6, name), any node in the world.

### Failure

Any lookup failure above is `fclose` and -1, ending the load in the middle
of the defs. Later defs keep the authoring-time pointers from the file in
+0x40 and the ref lists, and 0x50ae60 runs on them anyway (likely a crash;
not tested). Shipped data never fails in the stubbed emulation, which says
nothing about the real lookups.

## Post-load, 0x50ae60

For defs 1..n-1 with status not 4 or 5:

- `0x4f9e00(node, cb)` walks the tree; a hit sets flags |= 0x200 (callback
  not identified).
- +0x40 == 0 (cannot happen after a successful load): a new object3d
  (0x4fb740) goes in +0x40 and +0x64.
- flags & 0x8000: 0x509750 (uses 0x5ebcb0..b8; not traced).
- +0x68 = new object3d named `_<anim name>` (`"_%s"`, 0x5ebca8), 0x4eb5b0.
- activation 1: `0x5833f0(0x502df0)`; 0: `0x5833b0(flags>>20 & 1,
  0x502e60)`; 2: both. Probably the collide and weapon damage callbacks
  (strings `zEffAnimApplyCollideDamage`, `zEffAnimApplyDestroyDamage`;
  which is which not checked);
  0x100000 is mech3ax's proximity damage flag.

Then: defs with flags & 0x20 and status 0 or 1 -> 0x508310(0); every def ->
0x507fc0; status 5 logs `Corrupt animation loaded:` (0x5ebee0); status != 2
and activation 4 -> 0x508ae0(0, 0, 0) (start now). None of this was traced
further.

## mech3ax cross-check

mech3ax (`crates/anim/src/mw`, `common`) reads the same bytes in the same
order. Differences:

- Header: mech3ax reads 8 bytes, then the list count; the exe reads all 12
  at once. Same bytes.
- Effects and messages: mech3ax asserts both counts are 0; the exe reads and
  resolves them (0x24 each). Shipped files agree with mech3ax.
- Seq defs of size 0: mech3ax rejects them; the exe accepts them.
- `LOCAL_NODES_ONLY` (0x80000) is marked "PM only" in mech3ax; the MW 1.2 exe
  honours it in 0x509640. No shipped MW def sets it.
- The `ActivPrereqObjC` comment puts the name at 32; mech3ax's code (and the
  exe, +0x0c) read it at 4 into the body.
- Fields mech3ax asserts on (anim root pointer equal to the anim pointer, the
  world pointer non-zero, anim ref pointer null) are pointers the exe
  replaces or ignores.

## Porting order (leaves first)

1. The read phase of 0x50a0e0 as a pure parser (header, list skip, global
   block, defs, lists, blobs, messages). Check: `anim_walk.py --emu` already
   runs the original on all six files; compare the C++ structures against
   the emulator's heap.
2. 0x5096d0, 0x5091e0, 0x509160, 0x509640, 0x4f9d60 (pure on the node
   graph; need the zClass port in `docs/zclass-world.md`), and the list 6
   iterator 0x4f8b20/0x4f8bd0 with 0x4f7fe0.
3. 0x50d9e0 (effect index), then the creators 0x4fa720 (light), 0x4fd280 and
   0x4fd480 (sound node), 0x54a710 (puffer), with 0x4eb300 and `zsnd_find`.
4. The fixup pass, checked against the emulator with a loaded `gamez.zbd`
   world (the real lookups instead of stubs).
5. 0x5090f0, 0x509150, 0x5090e0, 0x508fc0, 0x50b380/0x50b0e0, 0x50ae60.
6. The event runtime (`zeff_anim_run.c`), its own trace.

## Open questions

- Global +0x08 (u16) and +0x3c (1): what reads them.
- The message hook 0x72c670 and the message layout (no shipped messages).
- The callbacks for 0x4f9e00 (flag 0x200) and what 0x509750, 0x508310,
  0x507fc0, 0x508ae0 do.
- Which node 0x5090e0 receives (the world is the guess) and what the anim
  system uses 0x73f738 for.
- The order list 6 holds same-named nodes in, which decides which def binds
  to which instance (not checked; see `zclass-world.md` for the list code).
- Whether a failed load really crashes in 0x50ae60.

## Discoveries

Listed in `discoveries.md` (Mission animations).

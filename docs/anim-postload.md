# Mission animations: the post-load pass 0x50ae60

Mech3.exe 1.2, 2026-10-19 (subagent draft); ported 2026-10-20: every piece
below (`zeff_anim_init.cpp` anim_load, `zeff_anim_run.*`, `zeff_anim_events.*`,
`zweapon/zwep_damage.*`, the zClass helpers) and checked with the frames
after it in `tests/differential/test_anim_run.py`; the runtime is
`anim-run.md`. Follows `docs/anim.md`: the def
record (0x13c) offsets below are the ones in its table; "def+X" is an offset
into one def. `zeffect\zeff_anim_init.c` per `func2file.json`; the helpers
are in `zeff_anim_run.c`, `zclass\class.c`, `object3d.c`, and an unnamed
file between `zvideo\zvid_ddd3d.c` and `zweapon\zwep_init.c` (the damage
callbacks; called "zwep damage" below).

Evidence: decomp + `mech3.asm` (calling conventions from the asm), the
file-only survey `tests/differential/explore/anim_postload_stats.py`, and
`tests/differential/explore/anim_postload_emu.py`, which loads each
mission's world and anim.zbd through `test_anim.Emulated`, then runs the
**original 0x50ae60** in Unicorn (0x549410 and 0x50a0e0 stubbed, data
already loaded; the fake puffer/sound ids from test_anim mapped as memory;
0x73f738 set to the first world node on list 13, see Open). It returns on all
six missions with no `zerr_report` calls. Counts below marked (emu) come from
it; (file) from the survey; (unverified) is reading only.

mech3ax names for the def fields this pass touches (their MW `AnimDefC`
offsets are the exe's): +0x9a execution priority (always 4), +0x9b (always
2), +0x9c/+0xa0 range min/max, +0xa4 reset time, +0xa8 (zero168; the
runtime uses it as the anim's elapsed time), +0xac max health, +0xb0 current
health, +0xb4..+0xc3 zero (runtime: +0xb4 damage threshold, +0xb8..+0xc0
start vector). Flags (mech3ax `AnimDefFlags`): 0x2 EXECUTION_BY_RANGE, 0x8
EXECUTION_BY_ZONE, 0x10 HAS_CALLBACKS, 0x20 RESET_TIME, 0x400/0x800 network
log set/on, 0x1000/0x2000 save log set/on, 0x10000 AUTO_RESET_NODE_STATES,
0x80000 LOCAL_NODES_ONLY, 0x100000 PROXIMITY_DAMAGE. Runtime-only bits seen
here: 0x40, 0x80, 0x100, 0x200, 0x8000, 0x20000, 0x40000.

## 0x50ae60 `anim_load()` (ECX = "anim.zrd", unused) -> 0

```
50ae60  if [0x73f720] (loaded) != 0: return 0
50ae76  0x549410()            puffer tick node (below)
50ae7b  0x50a0e0()            the reader; result ignored
        four loops over defs 1..count-1 (count = signed s16 0x73f72a,
        re-read every iteration; def = [0x73f72c] + i*0x13c)
50b0c6  [0x73f720] = 1; return 0
```

The first compare is `cmp ax, di` on the 16-bit count; later ones `movsx`.
A count <= 1 skips every loop.

### Loop 1 (0x50aeaa), status (+0x98) not 5 and not 4

1. `0x4f9e00(ecx = def+0x40, edx = 0x4f9de0)`: depth-first walk of the anim
   node's tree; returns 1 at the first node the callback accepts. Hit:
   **flags (+0x94) |= 0x200**.
2. If +0x40 == 0: `0x4fb740()` (new object3d) into **+0x40 and +0x64**.
   Dead in practice: step 1 already dereferenced +0x40 (0x4f9de0 reads
   [node+0x3c]), so a null here would have crashed first. A successful load
   never leaves 0.
3. If flags & 0x8000: `0x509750(ecx = def, edx = def+0x40)`. No shipped def
   has 0x8000 (file), so dead for shipped data.
4. **+0x68 = 0x4fb740()** (new object3d; unchecked), `sprintf(buf[0x24],
   "_%s" @0x5ebca8, def)` (the anim name +0x00), `0x4eb300(+0x68, buf)`
   (name), `0x4eb5b0(+0x68, edx = byte +0x9a)` (action priority, 4).
5. Activation (+0x99, a byte re-read for each test):
   - 1: `0x5833f0(ecx = def, edx = +0x64, push 0x502df0)`: collide callback.
   - 0: `0x5833b0(ecx = def, edx = +0x64, push (flags >> 20) & 1, push
     0x502e60)`: weapon callback; the flag is PROXIMITY_DAMAGE.
   - 2: both, collide first.
   Return values ignored. Note the target is the anim **root** (+0x64), not
   the anim node.

### Loop 2 (0x50afda): reset

Status not 5/4, flags & 0x20 (RESET_TIME), and status 0 or 1:
`0x508310(ecx = def, edx = 0, push 0)`. (emu) 8,129 calls over the six
missions = every RESET_TIME def; every one ends in 0x508170 and leaves
**status 1**.

### Loop 3 (0x50b02d): save object states

Status not 5/4: `0x507fc0(ecx = def)`. Runs after loop 2, so the "initial"
state saved is the state *after* the reset sequence ran (deliberate, it
seems: the reset sequence defines the baseline that AUTO_RESET 0x508090
restores).

### Loop 4 (0x50b06c): report and start

- status 5: `zerr_report(0x400, file 0x5ebd08, line 0x3595, "Corrupt
  animation loaded: %s" 0x5ebee0, def)`; silent (stub). None shipped.
- else status != 2 and activation == 4 (on startup):
  `0x508ae0(ecx = def, edx = 0, push 0, 0, 0)` (start now, zero vector).
  (emu) 185 calls, every one leaves **status 2** (running).

Status 4 defs reach this loop too, but 0x508760 refuses status 4 (and 5,
6), so they are not started. None shipped (every def has status 0).

## Fields written

Def (offsets into the 0x13c record):

| Field | By | Value |
|---|---|---|
| +0x40, +0x64 | loop 1 (dead) | new object3d if +0x40 was 0 |
| +0x68 | loop 1 | new object3d `_<anim name>`, priority +0x9a |
| +0x94 | loop 1 | \|= 0x200 (a facade billboard in the anim node's tree) |
| +0x94 | 0x508310 | &= ~0x40 (arg 0) |
| +0x94 | 0x508170 | &= ~0x100 after 0x4f2c30 if it was set |
| +0x94 | 0x5084f0 (start) | \|= 0x100 if the anim node's root is not a world/camera (added to the world), else &= ~0x100 |
| +0x94 | 0x508ae0 | 0x80 = any \|start vector\| > 0.01 (cleared here) |
| +0x98 | 0x508170 | status: 1 (or 4 if it was 6); 4/5 kept |
| +0x98 | 0x508760 | 2 when started |
| +0xa8 | 0x5084f0 | 0 |
| +0xb0 | 0x508170 | = +0xac (health refilled) |
| +0xb8, +0xbc, +0xc0 | 0x508ae0 | start vector (0 here) |
| +0x74..+0x90 | 0x508ae0 | 0 (eight dwords) |
| reset seq +0xc8: +0xe8 state, +0xec, +0xf0, +0xfc | 0x506f40, 0x5079b0 | state = reset state, times 0, current event = events; then the run |
| each seq def (+0xc4): +0x2c, +0x30 (u16), +0x20, +0x24, +0x28, +0x34 | 0x5084f0 (start) | 0 / via 0x506f40 |
| object ref +0x28 | 0x507fc0 | node ACTIVE (flags bit 2) |
| object ref +0x2c | 0x507fc0 | object3d data flag 0x10 (USE_MATRIX), object3d nodes only |
| object ref +0x30..+0x5f | 0x507fc0 | USE_MATRIX: the 12-float transform (data+0x30); else translation (data+0x54..5c) at +0x30, rotation (data+0x18..) at +0x3c, scale (data+0x24..) at +0x48 |
| light / dyn sound ref +0x28 | 0x5069b0 / 0x506a70 | 0 after removing from the world (if set) |
| anim ref +0x44 | 0x508170 | 0 (all 0 already) |

(emu) changed bytes per def after the pass: always +0x68; +0x98 for every
reset or started def; +0xe8/+0xfc for the 2,861 with reset events; +0x94
for the 0x200 and 0x100 cases. +0xb0 never changes (current health equals
max in every file).

Other state:

- New nodes: one object3d per live def (+0x68) and one puffer tick node
  (0x549410, first load only); all go through node_alloc (list 6) and
  object3d_init (list 7).
- Damage records (node+0xbc, below) on each damage def's root and its
  subtree; node flag 0x40 on the root for weapon-hit defs with
  PROXIMITY_DAMAGE.
- Whatever the reset events and started anims do to nodes, lights, sounds,
  puffers (below).
- Startup anims: +0x68 node +0x40 = def, action callback 0x507de0 appended
  to action list +0x44 (4) via 0x4eb500; the anim log at 0x60c708 gets one
  record each (if logging applies).
- 0x73f720 = 1.

## Callees

"Ported" = an address comment in `src/`.

| Address | What | Ported |
|---|---|---|
| 0x549410 | puffer tick node: once, `0x72be58 = 0x4fb740()`, null -> zerr line 0x1e2 "Error allocating puffer tickNode"; else `0x4eb5b0(node, 5)`, `0x4eb450(node, 0x5490e0)`. Every call: copy vec3 0x63d870 to 0x72be28 and 0x72be38, zero 0x72be4c/50/54 (`gmod_puff.cpp`) | yes |
| 0x50a0e0 | the reader | yes |
| 0x4f9e00 | `tree_any(node ecx, cb edx)`: cb(node) == 1 -> 1; else children **from the last to the first** (+0x5c count, +0x60 array), recursive | yes |
| 0x4f9de0 | callback: node+0x3c (model) non-null, model+0x00 (type) == 1 Facade and model+0x04 (facade mode) == 0 CylindricalY -> 1 | yes |
| 0x4fb740 | object3d_create | yes |
| 0x509750 | (flag 0x8000) `def+0x40 = 0x4f9b50(…, [0x5ebcb4], [0x5ebcb8])` (the tree copy), 0 -> return 0; flags \|= 0x20000; `0x509eb0`; flags &= ~0x8000. Dead for shipped data | yes (0x4f9b50 hooked) |
| 0x4eb300 | node_set_name | yes |
| 0x4eb5b0 | `node_set_priority(node, p)`: null -> zerr, 5. If a callback (+0x48) is set: off the old action list (+0x44 in 0..5) via 0x4f8220, onto list p via 0x4f8080 (if 0..5). +0x44 = p. Here +0x48 is 0, so only +0x44 = 4 | yes |
| 0x5833f0 | `damage_on_collide(def, node, cb)` = 0x5832f0(…, slot 1) | yes |
| 0x5833b0 | `damage_on_weapon(def, node, flag, cb)` = 0x5832f0(…, slot 0); success and flag -> `0x4eb790(node, 1)` | yes |
| 0x5832f0 | register a damage callback, below | yes |
| 0x583220 | `owns_record(node, rec)`: 1 if every parent (+0x54/+0x58) is non-null and has a different +0xbc | yes |
| 0x583270 | fill slot {def, cb} of a new record, then 0x5832a0 | yes |
| 0x5832a0 | `spread(node, new, old)`: if node+0xbc == old, recurse into the children, then node+0xbc = new | yes |
| 0x4eb790 | node flags +0x24: set/clear 0x40 (mech3ax PROXIMITY, "never set in GameZ, only after Anim is loaded") | yes |
| 0x502df0, 0x502e60 | the collide / weapon damage callbacks (run at hit time, not here; below) | yes |
| 0x508310 | `anim_reset(def, node, keep)`, below | yes |
| 0x507fc0 | `anim_save_states(def)`: object refs from **0** (entry 0's node is 0 in every file); node 0 skipped; +0x28 = ACTIVE; object3d only: +0x2c = USE_MATRIX, then the matrix copy (0x4fc3f0) or 0x4fc020 / 0x4fbe90 / 0x4fbd20 | yes |
| 0x4fc3f0 | object3d transform pointer (data+0x30); checks node, data, class 5 | yes |
| 0x4fc020, 0x4fbe90, 0x4fbd20 | object3d get translation (data+0x54), rotation (+0x18), scale (+0x24); 0x4fc020 checks class 5, the other two do not | yes |
| 0x508ae0 | `anim_start(def, node, x, y, z)`, below | yes |
| 0x49bfd0 | zerr_report, empty stub | (symbols.csv) |

### 0x5832f0 `damage_register(def ecx, node edx, cb, slot)` -> 1 / 0

Node+0xbc points at a 0x10-byte record `{def0, cb0, def1, cb1}`: slot 0 =
weapon hit, slot 1 = collide. It is shared down the subtree.

- node+0xbc == 0: `calloc(1, 0x10)` (null -> 0), slot = {def, cb},
  `spread(node, rec, 0)` (node and every descendant still at 0) -> 1.
- Else if `owns_record(node, rec)`: slot already has a cb -> **0, nothing
  changes**; else fill the slot -> 1.
- Else (inherited from a parent): a **new** record with only this slot, spread
  over the subtree that had the parent's record -> 1. The other slot of the
  parent's record is not copied.

(emu) Over the six missions: 1,610 weapon and 1,048 collide registrations
(= activation 0+2 and 1+2 counts), all return 1; the "already has a record"
path is taken exactly once per activation-2 def (its second call); the
inherited path never. So no two shipped damage anims share a root node and
slot, and none sits under another's root.

### The damage callbacks (hit time; reading only, unverified)

- 0x502df0 collide (thiscall def, stack f32 damage, ret 4): if activation 1
  or 2: `d = damage - def+0xb4`; d > 0 -> health (+0xb0) -= d; then if
  health <= 0: `0x5085e0(def, 0, nine zeros)` (start the anim). Returns
  health (x87).
- 0x502e60 weapon (ecx def, edx weapon/projectile?, stack: an unknown
  dword, then f32 damage; ret 8): skipped if [edx+0x6c] & 0x400; activation 0 or 2;
  same subtraction; health <= 0 -> 0x5085e0 then `0x583710(1, 0)` (sets
  0x9a1788 = 1), returns 0; else `0x583730(health / max health)` (0x9a178c).
- Both test health <= 0 even when the hit did no damage, so a def with max
  health 0 fires on any hit (94 damage defs have health 0, file).

### 0x508310 `anim_reset(def ecx, node edx, keep)` (ret 4) -> 0 / -1

Here node = 0, keep = 0, which skips the instance part (edx != 0: compare
with +0x40, `0x506ca0` / `0x509eb0` rebinding, the instance chain at +0x138
and `0x5097c0` cloning; runtime only).

1. Null def or status 5 -> -1.
2. flags: keep ? |= 0x40 : &= ~0x40.
3. Status 1 -> return 0 (nothing else). At load every status is 0.
4. flags & 0x40 -> `0x508090` (restore saved object states, needs
   0x10000 AUTO_RESET). Not here (keep = 0).
5. If reset events (+0x100): `0x506f40(seq = def+0xc8)` (state +0x20 =
   reset state +0x21, +0x24 = +0x28 = 0, current +0x34 = events +0x38),
   `0x5079b0(def, seq)` (run the events due now). If the seq state is now 0
   or 1 (still running): `0x4eb500(+0x68, 0x508260)` (tick callback) and
   return 0. (emu) Never happens at load: all 2,861 reset sequences finish
   in one go.
6. `0x508170(def)` -> 0.

### 0x508170 `anim_reset_finish(def)` -> 0 / -1

Null or status 5 -> -1. flags & 0x100: `0x4f2c30(world [0x73f738], +0x40)`
(take the node out of the world), nonzero -> -1; flags &= ~0x100. Then
`0x5069b0` (lights: ACTIVE -> `0x4eb170(node, 0)`; +0x28 -> `0x4f2e00(world,
node)`, +0x28 = 0), `0x506a20` (puffers: `0x54ab40(puffer, 0)`: byte +0 = 0,
+0x80 = 0), `0x506a70` (dynamic sounds: off, +0x28 -> `0x4f3040(world,
node)`). Anim refs: +0x44 and +0x40 set -> `0x506ca0`; +0x44 = 0. +0x68 ->
`0x4eb450(+0x68, 0)` (clear its callback). Status: 5/4 kept, 6 -> 4, else 1.
+0xb0 = +0xac. All three list helpers start at entry 0 (whose pointer is 0
in every file).

(emu, t1) 63 light and 17 sound nodes switched off, 58 puffers stopped by
this at load.

### 0x5079b0 `anim_run_events(def ecx, seq edx)` (runtime core)

The event interpreter (a switch on the event type, one handler each). Not
mapped here: it belongs to `zeff_anim_run.c`'s own trace (`anim.md` step
6). What load needs: the reset sequences hold only types (file) 6 object
active 5,995, 8 scale 324, 17 cycle texture 192, 7 translate 134, 13
opacity 126, 35 callback 17, 9 rotate 12; handlers 0x5044d0, 0x504350,
0x505e20, 0x504380, 0x505bf0, 0x507410, 0x504180. Startup anims run their
first events through 0x508ae0 too (more types; not counted).

### 0x508ae0 `anim_start(def ecx, node edx, f32 x, y, z)` (ret 0xc) -> def / 0

1. `0x508760(def, node)` (the call proper): status 6/4/5 -> 0; status 2/3
   handle instances (+0x138 chain, 0x5097c0) and the "-99" reset time;
   node -> `0x509eb0`; `0x508950` (activation prerequisites, unverified
   reading: type 1 with +0x28 still 0 searches the def by name over **all**
   defs, unlike the loader's 0..n-2, and caches it; not found fails. A
   required entry (+0x00 == 0) fails while its target is in status 1; an
   optional one counts when it is not. Type 2 compares the node's ACTIVE bit
   with +0x08 the same way. **Type 3 entries are not checked at all.** Fewer
   counted than +0x111 fails) -> 0 fails; RESET_TIME -> `0x508310(def, 0,
   keep)` (again); `0x5084f0` (root of +0x40 not world/camera ->
   `0x4f2a40(world, +0x40)`, flags 0x100; +0xa8 = 0; unless 0x40000
   `0x507fc0`; every seq def reset); +0x68 made if missing; +0x68+0x40 =
   def; `0x4eb500(+0x68, 0x507de0)`; status = 2.
2. flags & 0x100 and the node is a camera (1): `0x4f3870(0,0,0)`, and unless
   0x200 `0x4f36b0(0,0,0)`; object3d (5): `0x4fc0b0(0,0,0)` (translation),
   unless 0x200 `0x4fbd80(0,0,0)` (rotation).
3. flags 0x80 from the vector, +0xb8/+0xbc/+0xc0 = it, +0x74..+0x90 = 0.
4. `0x50cce0(def, x, y, z)`: anim log record type 2 (name, 0x4fe180 count,
   vector) in the 0x50-byte-record buffer 0x60c708 (0x50bd50: 1000 records,
   doubles), kept if save logging applies (0x1000/0x2000, default 0x5ebf0c),
   sent to the hook 0x60c714 if network logging applies (0x400/0x800,
   default 0x5ebf10); a free-standing node (0x100) never logs by default.

(emu) one def per mission gets 0x100 at startup: `waypoint`.

## Quirks and bugs

For players (candidates for `discoveries.md`, plain words):

- Every animation that has a "reset" script runs it once while the mission
  loads, and the positions, sizes and on/off states it leaves are what the
  game remembers as "how things start". Lights, sounds and smoke puffers
  tied to animations are all switched off at load until an animation turns
  them on (t1: 63 lights, 17 sounds, 58 puffers).
- An object can carry at most one "when shot" and one "when rammed"
  animation. A second one on the same object would be silently ignored (the
  shipped missions never do this).
- An animation with 0 health fires on any hit, even one doing no damage
  (94 such damage animations).
- Billboards (flat sprites that turn to face you, "cylindrical facades")
  are flagged at load so that starting their animation does not reset their
  rotation.
- A free-floating animation (one not attached to the world or a camera,
  such as `waypoint` in every mission) is added to the world when started
  and snapped to the world origin (position and rotation 0) first.

For the port (`fixes.md` candidates only if they ever matter):

- Loop 1 checks +0x40 for null after already walking it: the safety net
  cannot catch anything (a failed load would crash in 0x4f9de0 first).
- A failed load (0x50a0e0 returns -1) is ignored; the pass then runs over
  defs that still hold authoring pointers (likely crash; `anim.md`).
- 0x5832f0's inherited path drops the parent record's other slot: a
  weapon-hit anim on a child would stop the parent's collide anim reaching
  that child, and the reverse. Never happens in shipped data (emu).
- `0x4eb450`/`0x4eb500` `free()` a pool node if the list push returns
  nonzero; the pushes always return 0, so dead (it would be a heap bug).
- 0x507fc0 runs again for every startup anim (via 0x5084f0): harmless.
- The puffer tick node is made once and never re-made after a reset of the
  node pool (0x72be58 is only tested for 0; unverified who clears it).

## Porting order (small pieces, each with an emulator check)

All checks extend `test_anim.py`: load world + anim.zbd in both, then run
the piece; compare like `dump()` plus the new fields. The probe side needs
the post-load entry in `tests/probe.cpp`.

1. **Facade walk** 0x4f9e00 + 0x4f9de0 (zclass, pure). Check: call the
   original on every node of each world and compare 0/1 (also a hand-made
   tree where only the last child matches, for the reversed order).
2. **Node action helpers** 0x4eb5b0, 0x4eb450, 0x4eb500, 0x4eb790 with
   list_remove 0x4f8220 (list.c, `zclass-world.md`). Check: synthetic nodes
   in the zclass emulator (`test_zclass.py`): priority change with and
   without a callback, callback set/clear on lists 0..5, bad priority (error
   1), and the list state after.
3. **Damage records** 0x583220, 0x583270, 0x5832a0, 0x5832f0, 0x5833b0,
   0x5833f0 (new file next to zweapon; name unknown). Check: after loop 1
   on all six missions, every node's record (def index, callback id per
   slot) and flag 0x40; plus synthetic: slot taken (0), inherited path,
   null parent, calloc failure.
4. **Object state save** 0x507fc0 with 0x4fc3f0, 0x4fc020, 0x4fbe90,
   0x4fbd20 (object3d.c). Check: every object ref +0x28..+0x5f after loop 3
   with loop 2 hooked out (so the world-file state is saved), on all six.
5. **Puffer tick node** 0x549410 (gmod_puff): node, priority 5, callback id,
   the globals; check on first and second call.
6. **0x50ae60 itself** with 0x508310, 0x508ae0 and 0x509750 hooked
   (recorded in a trace like `AnimHooks`). Check: the trace (def index per
   call, in order), +0x68 nodes (names, priority), +0x94, the records, the
   whole node pool and lists, on all six; and a run with a count <= 1.
7. **Reset machinery** 0x508310 (load path: node 0), 0x506f40, 0x508170,
   0x5069b0, 0x506a20 (needs the puffer object, still hooked: record the
   call), 0x506a70, with 0x5079b0 and the world removals 0x4f2c30,
   0x4f2e00, 0x4f3040 hooked. Check: status, flags, seq fields, light/sound
   refs, node ACTIVE bits on all six; the hooks' traces.
8. **The reset event handlers** (types 6, 7, 8, 9, 13, 17, 35) and the
   0x5079b0 loop, i.e. the start of the event runtime trace. Check: unhook
   0x5079b0 in step 7, compare the node pool after (transforms, opacity,
   texture cycle state). Floats within tolerance.
9. **Start path** 0x508ae0, 0x508760 (load path: node 0, status 1),
   0x508950, 0x5084f0, 0x50cce0, 0x50bd50, with the camera/object3d setters
   and 0x4f2a40 (cls_world); instance cloning 0x5097c0 / 0x509eb0 hooked
   (not reached at load). Check: the 185 startup anims, status 2, +0x68
   callbacks on action list 4, the log buffer, `waypoint`'s world link.

Then 0x509750 (needs the tree copy 0x4f9b50) only if Pirate's Moon data
ever sets 0x8000.

## Open

- What node 0x5090e0 stores in 0x73f738: the code uses it as the world
  (0x4f2a40, 0x4f2c30, 0x4f2e00, 0x4f3040 are `cls_world.c`); both test
  sides set the first list-13 node. Still to trace in 0x48faf0 ([esp+0x30],
  also passed to 0x50d2c0).
- Answered (`anim-run.md`): the ticks 0x507de0 / 0x508260 / 0x5085b0; the
  dispatcher is 0x4f7d80 over lists 0..5 (0x4f7d20 each, then 0x4f7f70),
  calling the action with ECX = node for active nodes on non-removable items;
  0x502e60 is ECX def, EDX the hit (its +0x6c 0x400 skips), a stack dword
  not used, the damage; 0x50bd50's +4 is `[0x60c718] & index & 0xffffff`.
- Who calls the records' callbacks: the weapon hit code 0x583480 (slot 0,
  with globals for the hit position) and 0x5836b0 (slot 1); not ported.
- Who sets 0x5ebf0c / 0x5ebf10 (log defaults, 1) and 0x60c714 (network hook).
- `functions reached` in the emu script comes from a block hook and misses
  some (0x4eb170 is called but not listed); use the per-address counters.

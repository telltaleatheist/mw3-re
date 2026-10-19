# Mech definition: firepoints and part slots

Mech3.exe 1.2, 2026-10-19. Ported: `src/mech3/mech_reader.*` (the MechDfn
constructor 0x46b890, 0x46ba20, `read_firepoints`, `read_part_slots`) and
`mech_names.*` (0x484e40, 0x484cc0). `test_mechdef.py` runs the original
blocks after the original constructor on all 19 files: the MechDfn image,
part slots and firepoints match, and the part id/name helpers agree on every
name and id tried. Only the Elemental tells radius from share (every other
mech has 0.5 for both), and it caught a deliberate swap. Follows
`docs/mechdef-skins.md`: the mech definition reader 0x46ba40
(`mech3\mech_reader.cpp`) after the skins block. Everything is from the asm
(`analysis-output/mech3.asm`); Ghidra's pseudocode drops ECX/EDX on all the
callees below.

## Piece boundaries

| Range | Block | Lands in |
|---|---|---|
| 0x46be07..0x46bfd8 | skins (done, `mechdef-skins.md`) | +0x1c |
| **0x46bfd8..0x46c13a** | **`firepoints`** | **+0x24** |
| **0x46c13a..0x46c1f0** | **`parts`: the part-slot table** | **+0x28..+0x147** |
| 0x46c1f0..0x46cb44 | `configurations` (next piece: 0x424a90, 0x423d00, 0x49da10, 0x484e40 for ammo parts, weapons at 0x46c6bc, `weapons.md`) | +0x20 |
| 0x46cb3c | 0x424b70 on +0x20, then +4 = 1, return true | |

Neither block carries a register into the next; only `ebp` (the `MechDfn`) and
the stack frame are live across the boundaries, so each range can run alone.
The next piece starts at **0x46c1f0** (`find(tree, "configurations")`).

## The data

`firepoints` is a top-level key in all 19 `dfn_*.zrd` (checked: the depth-first
`find` lands on the root list's own entry in every file; no string value
`firepoints` comes earlier). Its value is a flat list of `name (body)` pairs;
each body is a flat key list. From `dfn_madcat.zrd` (`tools/analysis/zrd.py`):

```
"firepoints" (
  "firepoint01" ( "radius" (0.5) "share" (0.5) "part" ("rarm") )
  "firepoint02" ( "radius" (0.5) "share" (0.5) "part" ("rarm") )
  "firepoint03" ( "radius" (0.5) "share" (0.5) "part" ("larm") )
  ...                                       12 firepoints: rarm×2 larm×2 rtorso/ltorso×2 torso×2 head×2
)
```

`dfn_elemental.zrd`, the only file that differs in shape:

```
"firepoint01" ( "part" ("rarm") )
"firepoint02" ( "part" ("larm") )
"firepoint03" ( "radius" (0.2) "part" ("rtorso") )
"firepoint04" ( "radius" (0.2) "part" ("ltorso") )
```

`dfn_firefly.zrd`: firepoint01/02 (rarm, larm) have radius 0.3 and share 0.3;
the rest 0.5/0.5.

Survey of all 19 files (181 firepoints, 4 to 12 per mech, always named
`firepoint01`.. in order, no duplicates, even item counts): every value is a
float (no ints), radius 0.5 ×175, 0.3 ×2, 0.2 ×2, missing ×2; share 0.5 ×175,
0.3 ×2, missing ×4 (all elemental). Parts used: rarm 38, larm 38, torso 32,
ltorso 28, rtorso 25, head 20; nothing else. Every weapon in every
configuration (327) names a firepoint that exists, so the weapons block's
"unknown firepoint" path (0x46c828) is never taken with shipped data.

`parts`: **no dfn file has this key** (no string `parts` anywhere in any tree),
so `find` returns 0 in every file. The value would be ignored anyway (below).

## The objects

**FirepointList**, 0x10 bytes, `MechDfn` +0x24: MSVC 6 `std::vector<Firepoint*>`
`{allocator byte, first, last, end}` as in the skins. The allocator byte is the
stack byte `[esp+0x13]` (garbage); skip it in a test. Always allocated, empty
when `firepoints` is missing. Freed by 0x46cca0 (deletes each record, then the
buffer) from the `MechDfn` dtor 0x46b9b0.

**Firepoint**, `operator new(0x20)`; the reader writes only the first 0x10:

| Off | Type | What |
|---|---|---|
| +0x00 | char* | name, **borrowed from the reader tree** (no `_strdup`); the tree lives at `MechDfn` +8 until the dtor frees it (0x556820) after the firepoints |
| +0x04 | float | `radius`, 0.0 when missing |
| +0x08 | float | `share`, 0.0 when missing |
| +0x0c | int | part id of `part` (0x484e40), -1 when missing or unknown |
| +0x10 | int | uninitialized here; part id of the model node's MechPart (or -1), set by 0x46cb80 |
| +0x14..+0x1c | Vec3 | uninitialized here; the model node's translation (0x4fc020), set by 0x46cb80 |

**Correction for `docs/weapons.md`**: it gives +4 `share` and +8 `radius`. The
x87 order says the opposite (0x46c0c9: `fld share; fld radius; fstp [+4];
fstp [+8]`), and the users agree: the shared-firepoint offset (0x4b2530,
0x4b2682) scales by +8 = share, the salvo scatter (0x4b36e0, 0x4b376d) by
+4 = radius. Also the unknown-firepoint record at 0x46c828 is radius 0,
share -1.0, part -1 (weapons.md says "radius -1").

**Part slots**, `MechDfn` +0x28, 18 × 0x10 bytes (+0x28..+0x147), slot *i* at
+0x28 + 0x10·*i*, *i* = part id. The `MechDfn` ctor 0x46b890 fills all 18 with
`{0, 1.0f, -1, -1}`; the reader then sets +0 and +0xc of 13 of them:

| Off | Type | What |
|---|---|---|
| +0x00 | const char* | part name, a static string from 0x484cc0 (`torso`, ...) |
| +0x04 | float | 1.0 (ctor); not touched by the reader. Meaning unknown |
| +0x08 | int | -1 (ctor); not touched by the reader. Meaning unknown |
| +0x0c | int | part id (1..15), -1 (ctor) when not set |

Slots set, in call order: 15 hip (+0x118), 1 torso (+0x38), 2 larm (+0x48),
3 rarm (+0x58), 9 lhand (+0xb8), 10 rhand (+0xc8), 4 lthigh (+0x68), 5 rthigh
(+0x78), 11 lcalf (+0xd8), 12 rcalf (+0xe8), 13 lfoot (+0xf8), 14 rfoot
(+0x108), 6 head (+0x88). **Never set: 7 ltorso (+0x98), 8 rtorso (+0xa8)**, and
the unused 0, 16, 17. No reader of the slots was found by a quick scan (indexed
or via 0x4846a0 results); open.

For `types.json` (`MechDfn`): +0x4 u8 `loaded` (1 on success, 0x46cb44),
+0x20 configurations (next piece; dtor 0x424f60 then 0x425110), +0x24
`FirepointList*`, +0x28 `PartSlot[18]`. +0x0 is the mech index the ctor gets
(0x46b7a0 passes its ECX, the index 0x4846a0 caches at 0x5e60b0 + 0x3c·i).

## The firepoints block, 0x46bfd8..0x46c13a

```
fp = zrdr::find(dfn->reader, "firepoints")             // 0x5569e0, ECX node, EDX key
if (!fp):                                              // 0x46c10b
    dfn->firepoints = new(0x10) ? empty vector : 0     // +0x24
    goto parts
list  = fp->list                                       // [fp+4]
pairs = (list[0].i - 1) / 2                            // cdq/sub/sar: toward 0
vec   = new(0x10) ? {alloc=[esp+0x13], 0, 0, 0} : 0
if (vec && pairs != 0) vec->reserve(pairs)             // 0x46cc00, thiscall, ret 4
dfn->firepoints = vec                                  // +0x24, before filling
for i in 0..pairs-1:                                   // signed: pairs <= 0 skips
    name = list[1 + 2i].value                          // no type check, not copied
    body = &list[2 + 2i]                               // the node, passed as the tree
    radius = 0; share = 0
    get_float(body, "radius", &radius)                 // 0x556a60, ret 4
    get_float(body, "share",  &share)
    part = part_id(get_string(body, "part"))           // 0x5569f0 then 0x484e40
    rec = new(0x20)
    if (rec) { rec->name = name; rec->radius = radius;
               rec->share = share; rec->part = part }  // +0x10..+0x1c left as is
    dfn->firepoints->insert(last, 1, &rec)             // 0x49ced0 = push_back, ret 0xc
```

`dfn->firepoints` is not checked for 0 inside the loop (a failed `new` would
crash at `[ecx+8]`); neither is the reserve's result.

Callees:

- **0x5569e0** `find(ECX = node, EDX = key)` → node after the key, depth-first
  (`docs/zreader.md`). Ported.
- **0x556a60** `get_float(ECX = node, EDX = key, [esp+4] = float*)`, ret 4:
  find, then a float node copies its bits, an int is `fild`-converted, a list
  takes its first item the same way; else the output is left alone. Returns 1
  on a write (0 otherwise). Ported (zReader keyed getters).
- **0x5569f0** `get_string(ECX = node, EDX = key)` → `char*` of a string node or
  of a list whose first item is a string, else 0. Ported.
- **0x484e40** `part_id(ECX = name)` → id, no stack args. Inline case-sensitive
  `strcmp` against, in order: hip 15, torso 1, larm 2, rarm 3, lhand 9, rhand 10,
  lthigh 4, rthigh 5, **lleg 4, rleg 5** (aliases), lcalf 11, rcalf 12, lfoot 13,
  rfoot 14, head 6, ltorso 7, rtorso 8; anything else and a null pointer give -1.
  Pure; port.
- **0x46cc00** `vector<T*>::reserve(n)` (thiscall, ret 4): if capacity < n
  (unsigned): `new(4·max(n,0))`, copy, `delete(old)` (also when old is 0),
  end = new + 4n, last = new + size. Same template as the skins' reserve.
- **0x49ced0** `vector<T*>::insert(pos, n, const T* x)` (thiscall, ret 0xc),
  with pos = last, n = 1: push_back. Grows to `size + max(size, n)` when full;
  never full here (reserved exactly). `std::vector` in the port.
- 0x5abd26 / 0x5abd1a: `operator new` / `delete` thunks (IAT 0x5bb4bc / 0x5bb4c8).

Stack slots used: `[esp+0x13]` (allocator byte, read only), +0x14 radius,
+0x20 loop counter, +0x28 share, +0x2c rec, +0x38 fp node, +0x48 vec,
+0xb8 the EH state (writes 2, 3, -1).

## The parts block, 0x46c13a..0x46c1f0

```
node = zrdr::find(dfn->reader, "parts")                // always 0 in shipped files
for (id, slot) in [(15,+0x118), (1,+0x38), (2,+0x48), (3,+0x58), (9,+0xb8),
                   (10,+0xc8), (4,+0x68), (5,+0x78), (11,+0xd8), (12,+0xe8),
                   (13,+0xf8), (14,+0x108), (6,+0x88)]:
    PartSlot::Set(dfn + slot, id, node)                // 0x46ba20
```

- **0x46ba20** `bool PartSlot::Set(int id, node*)` (thiscall, ret 8):
  `this->name = part_name(id)` (0x484cc0), `this->id = id`, returns 1 (ignored).
  **The node argument is never read.**
- **0x484cc0** `part_name(ECX = id)` → static string: 1 torso, 2 larm, 3 rarm,
  4 lthigh, 5 rthigh, 6 head, 7 ltorso, 8 rtorso, 9 lhand, 10 rhand, 11 lcalf,
  12 rcalf, 13 lfoot, 14 rfoot, 15 hip, else `none` (0x5e6f04). Jump table at
  0x484d30 (objdump shows it as code). Pure; port.

So the block's whole effect is constant: the same 13 names and ids in every
mech, whatever the file says.

## Later users (not part of the reader)

- **0x46cb80** `MechDfn::BindFirepoints(model)` (thiscall, ret 4; called once
  at 0x4160b2 in the mech setup 0x415e40): for each record, the model node
  named `rec->name` (0x4f9d60 on `[model+4]`), its MechPart's id (0x40ef30 →
  +8 → +0x28, or -1) into +0x10, and its translation into +0x14..+0x1c
  (0x4fc020, Object3DGetTranslate). The weapons use +0x10, not +0xc, to find
  the firing part (0x4b26f8, 0x4b2c71 → 0x41d510).
- **0x473220**, vt+8 of the weapon config item (vtable 0x5c0fc0): the dfn part
  id (+0xc) of the item's firepoint, -1 when out of range. Its callers were not
  traced.
- **0x414679**: the TAG equipment mounts weapon 0x20 on **`firepoints[0]`'s
  name** (0x4b0460, group arg 4).
- 0x4b2530 (weapon ctor): with no SALVO truecount (WeaponType +0x44 <= 0), if a
  weapon already on the manager uses the same record, this one gets a fixed
  offset `share · (rand() − 16383.5) / 16383.5` in x and y (+0x84/+0x88, z 0).
- 0x4b36e0 (aim): while salvo shots remain (+0x68), each shot instead gets a new
  random offset `radius · (rand() − 16383.5) / 16383.5` in x and y.

## Porting order (leaves first) and checks

1. **0x484cc0 part_name and 0x484e40 part_id**, pure, with a standalone check
   over every name in both tables, `lleg`/`rleg`, case variants, junk and null.
2. **Firepoint record and FirepointList** (`std::vector<Firepoint*>`; 0x46cc00
   and 0x49ced0 are its reserve and push_back). Keep the name as a pointer
   into, or a view of, the reader tree; the tree outlives the records. Leave
   +0x10..+0x1c as fields defaulted to -1/0 in the port, filled by
   BindFirepoints later (note that the original leaves them uninitialized).
3. **`read_firepoints`**, the block 0x46bfd8..0x46c13a.
4. **PartSlot defaults** (the 18-slot loop in the ctor 0x46b890), **0x46ba20**,
   and **`read_part_slots`**, the block 0x46c13a..0x46c1f0. A tiny function in
   the port; keep the call order and the two skipped torsos.
5. Later, with the zClass node lookup: 0x46cb80 BindFirepoints, and 0x46cca0
   (the list's destructor).

Emulator check, extending `tests/differential/test_mechdef.py`:

- Same stubs as the skins (operator new / delete IATs). No `_strdup`, no
  localizer, no globals: the two blocks touch no global state.
- Build the dfn with the real ctor **0x46b890** (thiscall, ret 8, ECX = 0x1a8
  zeroed bytes, args `(mech index, tree)`; it calls nothing) so the part
  slots hold their `{0, 1.0, -1, -1}` defaults, rather than a zeroed dfn.
- Run 0x46bfd8..0x46c1f0 in one go (or the two ranges separately) with
  `ebp` = dfn and the same scratch frame as the skins (>= 0xc0 bytes). Calls it
  makes, all run natively: 0x5569e0, 0x556a60, 0x5569f0, 0x484e40, 0x5abd26,
  0x5abd1a, 0x46cc00, 0x49ced0, 0x46ba20, 0x484cc0.
- Compare: vector size; for each record the name's bytes (and that the
  pointer points into the tree, at the name string's data), radius and share
  as raw bits (no arithmetic, so they must be identical), part id. Skip the
  allocator byte and +0x10..+0x1c. For the slots, all 18 × 4 fields (the name
  as a string, or 0).
- All 19 `dfn_*.zrd`, plus crafted trees built as bytes and loaded with the
  original reader: no `firepoints` (empty vector, not 0), an odd item count
  (last name dropped), an int radius (converted), a missing `part` and an
  unknown part (-1), `lleg`, a body that is a string instead of a list, and a
  `parts` key (must change nothing).

## Discoveries

Listed in `discoveries.md` (Mechs: firepoints).

## Unverified

- Nothing here has been run in the emulator.
- The meaning of part-slot +4 (1.0) and +8 (-1), and who reads the slots.
- Who calls 0x473220 (the dfn part of a weapon item), e.g. the mech lab or
  the crit code, and whether a mismatch with the model's part matters.
- Whether the scatter in 0x4b36e0 is applied to the aim point or the muzzle
  (it adds to `[edi]`, `[edi+4]`; weapons.md calls it a muzzle offset).
- The TAG group argument (4, zero-based group 5 per weapons.md).

# Mech definition: configurations (loadouts)

Mech3.exe 1.2, 2026-10-19. Mapped from the asm, checked by running the original
block in the emulator on all 19 files. Ported so far: the porting order's
step 1, the table helpers (`src/mech3/mech_names.*`), checked over every
index, name, rating and armor tonnage in `test_mechdef.py`. From the asm
(`analysis-output/mech3.asm`); Ghidra's pseudocode drops ECX/EDX on every
callee below and the return value of the constructors. Follows
`docs/mechdef-skins.md` and `docs/mechdef-parts.md`: the mech definition
reader 0x46ba40 (`mech3\mech_reader.cpp`, the path is in its error messages)
after the part slots. Nothing ported yet. A scratch run (not a test) of the
original 0x46ba95..0x46cb3c in Unicorn on all 19 `dfn_*.zrd` ran clean and
gave the values quoted below as "emulator".

## Piece boundaries

| Range | Block | Lands in |
|---|---|---|
| 0x46c1f0..0x46c264 | `configurations`: the list | `MechDfn` +0x20 |
| 0x46c264..0x46c320 | per config: name, `Config` ctor, push, movement defaults, `train` flag | |
| 0x46c320..0x46c44d | `equipment` | config item list |
| 0x46c44d..0x46c484 | `internal` (endo steel) | +0x20 |
| 0x46c484..0x46c4e2 | `engine` | +0x24, +0x28, +0x74 |
| 0x46c4e2..0x46c6bc | `armor`, with the allocation check | item list, +0x2c..+0x60 |
| 0x46c6bc..0x46cb28 | `weapons` (and their `ammo`) | item list |
| 0x46cb28..0x46cb3c | next config | |
| 0x46cb3c..0x46cb44 | 0x424b70: the player's saved loadouts, `configs\<mech>.mw3` | +0x20 |
| 0x46cb44..0x46cb5f | `loaded` = 1, return true | +0x4 |

This piece runs to the **end of 0x46ba40**. When `configurations` is missing
(never, in the shipped files), the code jumps straight to 0x46cb44: +0x20
stays 0, no saved loadouts are read, and the reader still returns true.

Unlike the earlier blocks, this one is **not self-contained**: it reads six
stack slots that the head of the function (0x46ba95..0x46bb6f) fills, see
"Stack slots". Registers carried in: only `ebp` (the `MechDfn`).

## The data

`configurations` is top level in all 19 files: `name body` pairs, 62 configs
in total (1 to 5 per mech). From `dfn_madcat.zrd`:

```
"configurations" (
  "MSG_CONFIG_PRIMARY" (
    "internal" ("endosteel")
    "engine" ( "rating" (375) "xl" ("true") "max_speed" (86.4) )
    "weapons" (
      ( "MGUN" "firepoint09" "group" (1) "ammo" ( 200 "part" ("rtorso") ) )
      ( "MGUN" "firepoint05" "group" (1) )
      ...
      ( "LRM20" "firepoint07" "group" (5) "ammo" ( 120 "part" ("rtorso") ) )
    )
    "equipment" ( ("dsink" "rarm") ("dsink" "rleg") ... ("case" "rtorso") ... )
    "armor" ( "ferro-fibrous" ("true") "mass" (12.0) "head" (9) "torso" (36 9)
              "torsolr" (25 7) "arms" (24) "legs" (32) )
  )
  "MSG_CONFIG_ALTERNATE_A" ( ... )  ... "MSG_CONFIG_ALTERNATE_D" ( ... )
)
```

Survey of all 62 (scratch script over the trees):

- Names: `MSG_CONFIG_PRIMARY` ×19, `_ALTERNATE_A` ×12, `_B` ×12, `_C` ×9,
  `_D` ×6, and **`training1`..`training4` in `dfn_bushwacker.zrd`**. The
  `MSG_CONFIG_*` keys localize (Mech3Msg.dll, 0xf20.. `Primary`,
  `Alternate A`, ...); `trainingN` are not in the DLL and stay as they are.
- Keys per body: always `engine`, `weapons`, `equipment`, `armor`; `internal`
  (always `("endosteel")`) in 22. No other keys.
- `engine`: `rating`, `xl`, and `max_speed` in 55 of 62. **`max_speed` is not
  read here** (the only code that asks for a `max_speed` key is the mission
  reader's `global_wind`, 0x49201b).
- `armor`: always the seven keys above; `torso` and `torsolr` are (front rear).
- `equipment`: 372 two-string lists `(kind part)`; never a third item (the
  explicit tech, below). Kinds: dsink 170, case 82, jjets 62, fcs 15, bap 12,
  c3slave 8, ams 7, sink 6, tc 3, masc 3, c3 2, tag 1, ecm 1. No config mixes
  `sink` and `dsink`.
- `weapons`: 4 to 12 per config (never over 16). 194 entries without ammo, 132
  with `ammo (N "part" (P) ["extra" (...)])`, 2 with a bare `ammo (N)`
  (elemental: no part), and one malformed: **`dfn_sunder.zrd`
  MSG_CONFIG_ALTERNATE_B**, `("LRM20" "firepoint03" "group" (3) "ammo" (120
  "part" ("larm")) "extra" (120 120))`, with `extra` outside the `ammo` list.
  `extra` holds 1 or 2 ints (37 entries).
- Armor check (below) fails only for `dfn_elemental.zrd` (1 t standard = 16
  points, 11 assigned). Confirmed in the emulator: one zerr call, line 362.

## The objects

### ConfigList, 0x18 bytes, `MechDfn` +0x20 (ctor 0x424a90)

| Off | Type | What |
|---|---|---|
| +0x00 | int | count passed to the ctor (the number of pairs) |
| +0x04 | int | mech index (`MechDfn` +0) |
| +0x08 | u8 | vector allocator (low byte of the count; garbage) |
| +0x0c/+0x10/+0x14 | Config** | `std::vector<Config*>` first/last/end |

0x424a90 `ConfigList(int mech, int count)`, thiscall ret 8 (args pushed
`count`, then `mech`): fields as above, then `reserve(count)` when count != 0
(same template as the firepoint vector). The reader pushes with **0x49da10**,
`vector<T*>::insert(pos, n, const T*)` (thiscall ret 0xc, ECX = list + 8,
pos = last, n = 1: push_back; reserved exactly, so it never grows). Freed by
0x424f60 / 0x425110 from the `MechDfn` dtor (`mechdef-parts.md`).

### Config, 0xd8 bytes (ctor 0x423d00), vtable 0x5bd3c0

0x423d00 `Config(int mech, const char* name)`, thiscall ret 8. It first runs
the base ctor **0x470ce0(0)** (ret 4: +4 = arg byte, +8 = a new 0xc-byte list
sentinel linked to itself, +0xc = 0, +0x10 = (arg != 0), vtable 0x5c36e0).

| Off | Type | Ctor | Reader |
|---|---|---|---|
| +0x00 | vtable | 0x5bd3c0 | |
| +0x04 | u8 | 0 | list allocator |
| +0x08 | node* | sentinel | `std::list<Item*>`, nodes `{next, prev, item}` |
| +0x0c | int | 0 | item count |
| +0x10 | u8 | 0 | base flag (meaning unknown) |
| +0x14 | int | mech index | |
| +0x18 | char* | `_strdup(name)` | the localized name |
| +0x1c, +0x1d | u8 | 0 | not touched; +0x1c is the "player-made" flag (only those are saved), +0x1d is saved (`mechdef-head.md`) |
| +0x1e | u8 | 0 | 1 when the name starts with `train` (`strncmp`, 5) |
| +0x1f | u8 | **not set** | "modified": 1 after any AddItem/AddEquipment (always, since every config has armor) |
| +0x20 | u8 | 0 | 1 when `internal` is `endosteel` (`_stricmp`) |
| +0x24 | int | 0 (or [0x5e3304] if < 0; it is 79) | engine table index of `rating` (0x432ea0) |
| +0x28 | u8 | 0 | `xl` |
| +0x2c | float | 0 | armor tons (half-ton count · 0.5), by AddEquipment |
| +0x30 | int | the mech's tech (0x484720) | armor tech: the mech's for ferro, 3 for standard |
| +0x34 | u8 | 0 | ferro-fibrous |
| +0x38..+0x60 | int[11] | 0 | armor points, see below |
| +0x64 | int | mech tech | tech of the last sink added |
| +0x68 | int | 10 | heat sinks: 10 + one per `sink`/`dsink` item |
| +0x6c | u8 | 0 | 1 when the last sink added was a `dsink` |
| +0x70, +0x78 | dword | copies of [0x63d870], [0x63d878] (BSS, 0 in the emulator) | not touched |
| +0x74 | float | 1.2566 ([0x63d874] first, overwritten) | top-level `max_turn_rate` · 0.5; then `engine`'s `max_turn_rate` if present (never) |
| +0x7c | float | 0.5 | top-level `accel_rate` |
| +0x80 | float | 1.3 | top-level `decel_rate` |
| +0x84 | float | 3.0 | top-level `turn_accel_factor` (default 1.5 in the head) |
| +0x88..+0xd7 | | not written | +0x88 is the saved loadout's CRC-32 checksum (`mechdef-head.md`); the rest not traced |

Armor points, from the reader's stores: +0x38 `torso`[0] (centre front),
+0x44 `torso`[1] (centre rear), +0x3c and +0x40 both `torsolr`[0], +0x48 and
+0x4c both `torsolr`[1], +0x50 `head`, +0x54 and +0x58 `arms`, +0x5c and
+0x60 `legs`. Which of each pair is left and right is not known from the
reader.

The engine table at 0x5e2f50: 79 entries of 12 bytes, `{int rating, int
name message id, float weight in tons}`, ratings 10..400 in steps of 5 (375
→ 38.5 t). **0x432e30** clamps the rating to 10..400 and returns the index of
the last entry whose rating is <= it (so 377 → the 375 entry). **0x432ea0**
`Engine::Set(rating)` (thiscall, ECX = config + 0x24, ret 4) stores it.

### Config items (one list per config, in file order)

All share `{+0 vtable, +4 count, +8 group, +0xc part/slot, +0x10 u8}`;
vt+4 = 0x409f50 `SetPart(int)` writes +0xc, vt+0xc returns the kind.

| Kind (vt+0xc) | Vtable | Size | Fields |
|---|---|---|---|
| 1 weapon | 0x5c0fc0 | 0x1c | +4 = 1, +8 group 0..4, **+0xc firepoint index** in `MechDfn` +0x24, +0x10 = 0, +0x14 mech-lab weapon index, +0x18 the mech's tech |
| 2 ammo | 0x5bd150 | 0x18 | +4 rounds, +8 = 0, +0xc part id (`SetPart`), +0x10 = 0, +0x14 weapon index |
| 3 equipment | 0x5bd180 | 0x20 | +4 count, +8 = 0, +0xc part id (`SetPart`), +0x10 = 0, +0x14 not written, +0x18 equipment kind (0x4849a0), +0x1c tech |

Armor is stored as an equipment item: kind 1 (`armor`) or 2 (`armorff`), count =
half tons, part -1 (madcat: `armorff` ×24).

Emulator, `dfn_madcat.zrd` primary, 23 items: 9 equipment (7 dsink, 2 case,
parts 3 5 4 8 7 8 7 3 2), then `armorff` n24, then 10 weapons with their ammo
items right after each (`MGUN` fp 8 group 0, ammo 200 part 8; ...; `LRM20` fp
6 group 4, ammo 120 part 8). +0x68 = 17, +0x6c = 1, +0x74 = 1.395 (2.79 / 2).

## The block, 0x46c1f0..0x46cb44

```
cfgs = find(dfn->reader, "configurations")                // 0x5569e0
if (!cfgs) goto done_loaded                               // 0x46cb44
list  = cfgs->list
pairs = (list[0].i - 1) / 2                               // toward 0, -> [esp+0x48]
dfn->configs = new(0x18) ? ConfigList(dfn->index, pairs) : 0   // +0x20; not checked later
for i in 0..pairs-1:                                      // signed; 0 pairs skips
    name = localize(list[1 + 2i].value)                   // 0x575fb0, no type check
    body = &list[2 + 2i]                                  // [esp+0x14]
    c = new(0xd8) ? Config(dfn->index, name) : 0          // c not checked: 0 crashes below
    dfn->configs->vec.push_back(c)                        // 0x49da10
    c->accel = s.accel; c->decel = s.decel                // +0x7c, +0x80   (stack, head)
    c->turn_rate = s.turn_rate_half                       // +0x74
    c->turn_accel = s.turn_accel                          // +0x84
    if (strncmp(name, "train", 5) == 0) c->training = 1   // +0x1e, the localized name

    // equipment, 0x46c320
    eq = find(body, "equipment"); tech = 0                // tech is NOT reset per item
    for each item e of eq (items 1..count-1):
        kname = 0; part = -1
        if e.type == 3: kname = e.str                     // a bare string: part -1
        elif e.type == 4:
            kname = e.list[1].value; part = part_id(e.list[2].value)   // 0x484e40, no type checks
            if e.list.count > 3: tech = tech_id(e.list[3].value)       // 0x4852b0
        kind = equip_kind(kname)                          // 0x4849a0; kname 0 would crash
        if tech == 0: tech = s.mech_tech                  // sticks for the later items
        if kind != -1 && !restricted(dfn->index, kind, tech):          // 0x4d6dc0: always 0
            tmp = {vt 0x5bd180, 1, 0, -1, 0, ?, kind, tech}            // on the stack
            c->AddItem(&tmp, 1)->SetPart(part)            // vt+0 0x4723c0, then vt+4

    // internal, 0x46c44d
    v = find(body, "internal")
    if v && v->list[1].type == 3 && _stricmp(v->list[1].str, "endosteel") == 0:
        c->endo = 1                                       // +0x20

    // engine, 0x46c484
    rating = 200; xl = 0
    if (e = find(body, "engine")):
        get_int(e, "rating", &rating); get_bool(e, "xl", &xl)
        get_float(e, "max_turn_rate", &c->turn_rate)      // +0x74, raw, NOT halved
    c->engine.Set(rating)                                 // 0x432ea0 on +0x24
    c->xl = xl                                            // +0x28

    // armor, 0x46c4e2
    if (a = find(body, "armor")):
        ff = 0; mass = 0
        get_bool(a, "ferro-fibrous", &ff); get_float(a, "mass", &mass)
        c->AddEquipment(ff ? 2 : 1, s.mech_tech, ftol(floor(2*mass + 0.5)))  // vt+0xc 0x471fb0
        legs = arms = head = 0 (get_int each); torso = lr = {0, 0} (get_ivec2, 0x557110)
        +0x60 = +0x5c = legs; +0x58 = +0x54 = arms; +0x50 = head
        +0x38 = torso[0]; +0x44 = torso[1]; +0x40 = +0x3c = lr[0]; +0x4c = +0x48 = lr[1]
        computed = armor_points(c->armor_tech, c->ferro, c->armor_tons)    // 0x424760
        assigned = sum of the 11 ints +0x38..+0x60
        if (!(mass > 0) || computed != assigned):
            zerr(0x400, file, 362, "***ARMOR ALLOCATION ERROR*** ...", mech name,
                 c->name, (double)mass, ff ? "ferro-fibrous" : "standard",
                 computed, assigned, head, ct, ct rear, lr, lr rear, arms, legs)
    // no armor key: nothing, no armor item

    // weapons, 0x46c6bc
    w = find(body, "weapons")
    if w:
        n = w->list.count - 1
        if n > 16: zerr(0x400, file, 392, "Too Many Weapons (%d>%d) in (%s:%s)",
                        n, 16, mech_name(c->mech), c->name); n = 16
        for each entry E of the first n items:            // no type check on E
            idx = weapon_index(E.list[1].value)           // 0x4848a0, _stricmp
            if idx == -1: zerr(0x200, file, 400, "Unrecognized Weapon (%s)", name); continue
            if restricted(c->mech, idx, mech_tech(c->mech)): continue      // never
            fpname = E.list[2].value                      // no type check
            fp = index of the first dfn->firepoints[k] with strcmp(name) == 0
            if none: fp = size; push_back(new(0x20) {fpname, 0.0, -1.0, -1})  // unknown firepoint
            N = 0; apart = -1; extra = {0, 0, 0}
            if (am = find(E, "ammo")):
                N = am->list[1].value                     // no type check
                apart = part_id(get_string(am, "part"))   // missing: -1
                if (x = find(am, "extra")):               // only inside "ammo"
                    for k in 0..min(x.count-1, 3)-1:
                        if x.list[1+k].type == 1: extra[k] = x.list[1+k].i
            g = 0; get_int(E, "group", &g); g = clamp(g - 1, 0, 4)   // missing group -> 0
            wi = new(0x1c) {vt 0x5c0fc0, 1, g, fp, 0, idx, s.mech_tech}  // 0 crashes ([0xc] write)
            c->items.push_back(wi); c->count++            // inline list insert, not AddItem
            if N == 0: continue                           // no ammo item, extras dropped too
            ai = new(0x18) {vt 0x5bd150, N, 0, -1, 0, idx}
            if ai: push_back(ai); count++; ai->SetPart(apart)
            for k in 0..2: if extra[k] != 0:
                same as ai with extra[k] rounds, same part
done:
dfn->configs->LoadSaved()                                 // 0x424b70, 0x46cb3c
done_loaded:
dfn->loaded = 1; return true                              // +0x4
```

`s.*` are the head's stack slots. The whole block runs once per `MechDfn`;
the reader returns early when +0x4 is already set (0x46ba76).

## Callees

ECX/EDX confirmed in the asm for each.

- **0x424a90** ConfigList ctor, above. **0x49da10** vector insert, above.
- **0x423d00** Config ctor, above; calls 0x470ce0, 0x484720 (ECX = mech),
  `_strdup` (IAT 0x5bb670).
- **0x575fb0** localize (ECX = key) → text, or the key itself when there is
  no DLL hook ([0x72c670] = 0). Ported (`zutl_msg`).
- **strncmp** (IAT 0x5bb654), **_stricmp** (0x5bb580), **floor** (0x5bb66c),
  **0x5ac340** `_ftol` (in the exe).
- **0x484720** `mech_tech(ECX = index)` → [0x5e60a8 + 0x3c·i] for 0..18,
  else 3. 1 clan, 2 Inner Sphere, 3 "any". Clan: blackhawk, cauldron, daishi,
  elemental, madcat, puma, shadowcat, supernova, thor, vulture; the other nine 2.
- **0x484650** `mech_name(ECX = index)` → 0x5e6078 + 0x3c·i, else `notmech`.
- **0x4852b0** `tech_id(ECX = str)`: `clan` 1, `sphere` or null 2, anything
  else 3. Pure.
- **0x4849a0** `equip_kind(ECX = str)`: case-sensitive walk of the 5-dword
  table at 0x5e6868 (`{kind, name, ...}`), -1 when absent. Crashes on null.
- **0x4848a0** `weapon_index(ECX = str)`: `_stricmp` over 0x5e64f0
  (`weapons.md`); null or "" → -1.
- **0x4d6dc0** `restricted(ECX = mech, EDX = id, [esp+4] = tech)`, ret 4:
  `xor al, al`. A disabled check; port as `false`.
- **0x4fd1c0** vector size (ECX = vector). 0x5569e0 find, 0x5569f0
  get_string, 0x556a60 get_float, 0x556ae0 get_int, 0x557150 get_bool,
  0x557110 get_ivec2 (needs 2 items, else both stay 0): ported zReader.
- **0x4723c0** `Config::AddItem(Item* it, int n)`, vt+0, ret 8. Sets +0x1f.
  0x471460 looks for `it` itself (pointer identity) in the list; never found
  for the reader's stack item. By `it`'s kind: 0 → new 0x18 (0x44ec90 base,
  vtable 0x5bc160); 1 → a copy of a weapon item; 2 → a copy of an ammo item;
  each copy keeps `it`'s group (+8) and is appended. **3 (the reader's case)
  → `return this->AddEquipment(it->kind, it->tech, n)`**.
- **0x471fb0** `Config::AddEquipment(int kind, int tech, int n)`, vt+0xc,
  ret 0xc, returns the item (or 0):
  - kind 1 or 2 (armor): standard forces tech = 3. 0x4714c0 looks for an
    existing item matching a stack template through each item's vt+0x24
    (0x473480: same item kind, same equipment kind, tech equal or either 3).
    Found: count += n (floored at 0). Else a new item `{n, 0, -1, 0, kind,
    tech}` appended (0x45b800, list insert before the sentinel). Then +0x34 =
    ferro, +0x2c = count · 0.5, +0x30 = ferro ? tech : 3, +0x1f = 1.
  - kind 9 or 10 (sink, dsink): n items of count 1 appended; +0x68 = 10 +
    (0x425f30: number of kind-9/10 items), +0x64 = tech, +0x6c = (kind == 10).
    Returns the last item made, **0 when n <= 0** (the reader would then call
    SetPart on 0).
  - any other kind: one new item `{n, 0, -1, 0, kind, tech}` appended.
- **0x424760** `armor_points(ECX = tech, DL = ferro, [esp+4] = float tons)`,
  ret 4: `ftol(floor(tons · k))`, k = 16.0001 (float 0x5bd3e4) standard,
  ferro 1.2 · 16.0001 (clan, tech 1) or 1.12 · 16.0001 (otherwise), in double.
  So 16, 19.2 and 17.92 points per ton; the 0.0001 keeps exact products from
  flooring down.
- **0x49bfd0** `zerr` (cdecl: level, file, line, format, ...): an empty stub in
  this build (`docs/gamez.md`), so neither message is ever seen.
- **0x424b70** `ConfigList::LoadSaved()` (ECX = list, returns 1 when the file
  opened): `sprintf("configs\%s.mw3", mech_name(+4))`, opens it as a zArchive
  (0x5764e0, 0x576520, `zutl_zar.cpp`); when entry `version` holds 15
  (0x423cf0), entries 1..count-1 are each parsed by 0x4242a0 into a new
  `Config` and pushed onto the same vector (grows as needed). So the mech lab's
  saved loadouts follow the built-in ones. **Its own piece**; not mapped further.

## Stack slots (frame after the four register pushes)

Read from the head (must be seeded for a standalone run): +0x3c `MechDfn`,
+0x44 `max_turn_rate` · 0.5 (default 1.2566 before halving), +0x54 the mech's
tech (0x484720), +0x58 `accel_rate` (0.5), +0x5c `decel_rate` (1.3), +0x60
`turn_accel_factor` (1.5).

Written here: +0x13 xl, +0x14 body node / ammo rounds, +0x1b ferro, +0x1c
equipment tech / weapon entry, +0x20 armor mass, +0x24 equipment counter /
weapon index, +0x28 ammo part, +0x2c weapons left, +0x30 int temp, +0x34
group, +0x38 `configurations` node, +0x40 firepoint name / extra counter,
+0x48 pairs, +0x4c/+0x50 ivec2, +0x64 rating, +0x68 config counter, +0x6c new
firepoint record, +0x70..+0x78 extra[3], +0x90..+0xaf the equipment template,
+0xb8 EH state (6, 7, -1). Beware the pushes: several reads sit between a
`push` and its `call`, so their offsets look 4 or 8 higher in the listing.

## Corrections and additions to `docs/weapons.md` (applied there)

- **Weapon item +0xc is the firepoint's index** in `MechDfn` +0x24, not a
  record pointer ("+0xc firepoint"). An unknown firepoint appends a record and
  uses its new index. +0x18 is the mech's tech (0x484720), +0x10 a zero byte.
- **`extra` is only read inside the `ammo` list**, only int items count (a
  float leaves its slot 0), zeros are skipped, and **an `ammo` of 0 drops the
  extras too** (0x46c9c9). Otherwise "one more ammo item per extra, same part,
  up to three" is right.
- **A weapon without `group`** gets group 0 (get_int fails, 0 − 1 clamps to 0).
- Equipment items also carry +0x1c tech; **armor is stored as equipment** kind
  1 / 2 with count = half tons, and sinks as one item each. Equipment goes
  through `Config::AddItem` → `AddEquipment`; weapons and ammo are appended
  directly.
- Both error messages carry the mech and config names; 0x5e51f4's full text is
  "Too Many Weapons (%d>%d) in (%s:%s)" (level 0x400, line 392); "Unrecognized
  Weapon" is level 0x200, line 400. All are silent (zerr is a stub).
- The firepoint record for an unknown name (radius 0, share −1, part −1,
  0x46c828) and "only the first group number counts" are confirmed.

## Porting order (leaves first) and checks

1. Pure helpers: 0x484720, 0x484650, 0x4852b0, 0x4849a0, 0x4848a0 (with their
   tables), 0x432e30 with the engine table, 0x424760, 0x4d6dc0 (`false`).
   Standalone emulator checks over every name in each table, case variants,
   junk, and ratings 0..500; 0x424760 over tons 0..100 in 0.5 steps × tech
   1..3 × ferro.
2. The item types (weapon, ammo, equipment; only their data and SetPart/kind
   for now) and **Config** with the base 0x470ce0, the ctor 0x423d00,
   AddEquipment 0x471fb0 (with 0x4714c0/0x473480 matching, 0x425f30), and the
   kind-3 path of AddItem 0x4723c0 (the other kinds are mech-lab copies; port
   them with the mech lab). `std::list<Item*>` is fine; keep the order.
3. **ConfigList** 0x424a90 (`std::vector<Config*>`).
4. **`read_configurations`**, the block above, taking the head's six values as
   arguments (or a struct) until the head is ported. Stub 0x424b70 behind a
   callback (no saved loadouts) until its piece is done; zerr as a no-op or a
   log hook (note in `fixes.md` if it logs).

Emulator check, extending `tests/differential/test_mechdef.py`:

- Stubs as before (operator new/delete, `_strdup`, `_stricmp` from the reader
  harness), plus `strncmp` (0x5bb654), `floor` (0x5bb66c, as x87 code:
  `fld qword [esp+4]`, round-down control word, `frndint`), and 0x49bfd0
  stubbed to record (level, line). Localizer: [0x72c670] = 0 (names stay
  keys) and a second run with the hook (names `Primary`, `Alternate A`..);
  `training1..4` must keep the flag both ways.
- Build the dfn with the ctor 0x46b890 (mech index = the file's position in
  the 0x5e6078 table, so the tech is right). Either seed the six head slots
  and run **0x46c1f0..0x46cb3c**, or run **0x46ba95..0x46cb3c** (head +
  skins + firepoints + parts + configs) with `[esp+0x3c]` = dfn and
  [0x6061cc] = 0, the static list init 0x473ae0 called first; the second is
  what the scratch run did and needs no seeding. Stop at 0x46cb3c (before
  0x424b70, which opens files).
- Compare: ConfigList +0, +4, size; per config +0x14 and the name, bytes
  +0x1c..+0x1e, +0x20, +0x24, +0x28, +0x2c (bits), +0x30, +0x34, the 11 armor
  ints, +0x64, +0x68, +0x6c, +0x74..+0x84 (bits; +0x74 is the only arithmetic,
  `x · 0.5`, exact); skip +0x04, +0x10, +0x1f (uninitialised in the ctor),
  +0x70, +0x78, +0x88..+0xd7. Items in list order: kind, +4, +8, +0xc, the
  +0x10 byte, +0x14 (not for equipment), +0x18, +0x1c. The firepoint vector
  size afterwards (it grows on an unknown name). The zerr calls (elemental: one,
  line 362).
- Crafted trees through the original reader: no `configurations` (+0x20 = 0,
  `loaded` = 1), zero pairs, an odd pair count, a missing `armor` (no armor
  item, no check), mass 0 (error even if the points match), a 17-weapon list
  (error, 16 read), an unknown weapon and equipment name, an unknown
  firepoint, a missing `group`, `group (9)`, `ammo (0 "extra" (5))`, `extra`
  with a float and with four ints, a bare-string equipment entry, a
  three-item equipment entry with `sphere` followed by a two-item one (the
  sticky tech), `sink` then `dsink`, an `engine` with `max_turn_rate` and a
  rating of 3 and 900, a config named `trainer`.

## What remains of 0x46ba40

Only the head. Every other block is mapped (skins, firepoints, parts, this
piece).

| Range | Block | Keys (→ `MechDfn` offset) |
|---|---|---|
| 0x46ba40..0x46ba95 | prologue: no tree (+8 = 0) → return false; already loaded (+4) → return true | |
| 0x46ba95..0x46bad4 | mech tech → stack +0x54; version check; chicken | `version` (+0xc; must be an int equal to 2, else return false), `chicken` (+0x18, bool) |
| 0x46bad4..0x46bb6f | movement, kept on the stack for the configs; crouch; engine limits | `max_turn_rate` (s+0x44, ×0.5), `accel_rate` (s+0x58), `decel_rate` (s+0x5c), `turn_accel_factor` (s+0x60), `crouch_height` (+0x148), `engine_minmax` (+0x10/+0x14, ivec2; **+0x14 then forced to 400**) |
| 0x46bb6f..0x46bc72 | torso | `torso_limits` (vec3, degrees → radians: [0] +0x174, [1] +0x17c, [2] +0x178), `torso_shake` sub-list: `pitch_amp`, `yaw_amp`, `roll_amp` (radians, +0x160..+0x168), `freq` (+0x16c), `damp_freq` (+0x170) |
| 0x46bc72..0x46bd0d | LOD, lean, recoil bend | `lod_range` (vec2, +0x14c/+0x150), `lean_angle` (rad, +0x154), `lean_factor` (+0x158), `lean_speed` (+0x15c), `recoil_bend` (**sin** of the radians, +0x180) |
| 0x46bd0d..0x46be07 | recoil, aim | `recoil_slide` (+0x184), `recoil_twist` (rad, +0x188), `aim_slerp_freq` (+0x18c), `aim_min_pitch`/`aim_max_pitch`/`aim_min_yaw`/`aim_max_yaw` (rad, +0x190..+0x19c), `aim_arm_pitch_factor` (+0x1a0), `aim_ignore_arms` (+0x1a4, bool) |

Radians: `x · 0.01745329251994` (double 0x5c3398), only when the getter wrote.
About 0xd0 instructions, 25 getter calls and one helper: one sitting. Not
read by 0x46ba40 at all, though present in the files: `omnimech`, `clan`,
`internal_mass`, `max_mass`, `weight_class`, `hud`, `camera`, `animations`
(read elsewhere, not traced). The head's slot map above is from a first read;
check it when mapping the head.

## Discoveries

Listed in `discoveries.md` (Mechs: loadouts).

## Unverified

- Who reads +0x1e (training), +0x1c/+0x1d, the base flag +0x10, and +0x88..
  +0xd7; units of +0x74 (radians per second is likely).
- Left/right order inside the armor pairs (+0x3c/+0x40, +0x48/+0x4c, ...).
- 0x424b70 (saved loadouts) and the mech-lab copy paths of 0x4723c0.
- That nothing else reads `max_speed` from a dfn (only one code reference to
  the string; a key built at run time would be missed).
- Whether a failed armor check has any effect besides the message (none
  seen in the reader; the loadout is kept as is).
- How the equipment counts (armor items included) feed the mech at run time
  (`weapons.md`, 0x414501..0x414732, says every non-jump-jet kind adds 1 to a
  per-kind count; armor items with count 24 would only add 1 there).

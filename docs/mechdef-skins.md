# Mech definition: skins

Mech3.exe 1.2, 2026-10-19. Ported: `src/mech3/mech_skin.*` (the five
functions and the reader's skins block as `read_skins`) and the localizer
0x575f90 / 0x575fb0 (`src/gamez/zutil/zutl_msg.*`, the DLL calls as FIX (A)
hooks). `tests/differential/test_mechdef.py` runs the original block on all
19 `dfn_*.zrd`, with and without the camo names and the damage mode: every
skin, entry, the global entry list and the hole textures match. Not ported:
the texture loading and damage linking (Next layer, below). The mech
definition reader 0x46ba40 (`mech3\mech_reader.cpp`) reads the `skins` list of
each `dfn_<mech>.zrd` into a `SkinSet` at `MechDfn` +0x1c. The skin code lives
between `mech_salvage.cpp` and `mech_waynet.cpp`, around the one function
func2file pins to `mech3\mech_skin.cpp` (0x474740, whose error messages name
`C:\zipper\Mech3\mech_skin.cpp`), so the source file is very likely
`mech_skin.cpp`.

Everything below is from the asm (`analysis-output/mech3.asm`); Ghidra's
pseudocode drops `this` (ECX) and the return value of all three constructors.
All five functions are `__thiscall` with callee-cleaned stack arguments.

## The data

`skins` is not at the top level of the file but found depth-first like every
key (`docs/zreader.md`). Its value is a flat list of `name value` pairs: the
first pair is the full skin, then seven variants that give only a suffix, then
`damage`. From `dfn_madcat.zrd` (`tools/analysis/zrd.py`):

```
"skins" (
  "MSG_SKIN_PRIMARY" ( "mcat01" "mcat02" "mcat03" ("mcat04" "mcat26") "mcat05"
                       ("mcat06" "mcat25") "mcat07" ... "mcat23"
                       ("mcat_d01" "mcat16") ("mcat_d02" "mcat16")
                       ("mcat_d03" "mcat_24") ... ("mcat_d06" "mcat08") )   23 items
  "MSG_SKIN_VARIANTB" ("_b")
  "MSG_SKIN_VARIANTC" ("_c")  ... "MSG_SKIN_VARIANTH" ("_h")
  "damage" ( 0 0 ... 0 "mech_damage128x128" "mech_damage128x128"
             "mech_damage64x64" ... )                                      25 items
)
```

and `dfn_elemental.zrd`, the simplest:

```
"MSG_SKIN_PRIMARY" ("elemental02" "elemental03" ... "elemental14")   12 strings
"MSG_SKIN_VARIANTB" ("_b") ... "MSG_SKIN_VARIANTH" ("_h")
"damage" (0 0 0 0 0 0 0 0 0 0 0 0)
```

All 19 `dfn_*.zrd` have this shape: nine pairs, the primary list mixes strings
and two-string lists, each variant is a one-string list, `damage` mixes the int
0 and strings. Primary item counts run 12 (elemental) to 29 (firefly). The
damage list has the same length as the primary in 17 files; madcat has 25 vs
23, supernova 20 vs 19.

The skin names are message keys. 0x575fb0 passes each one to
`ZLocGetID` in `Mech3Msg.dll` (case-sensitive lookup in a sorted `{name, id}`
table, 0 when absent) and, for a non-zero id, returns the message text
(0x576040: FormatMessage into one of 16 rotating 0x200-byte buffers, trailing
`\r\n` cut). So at run time the names are:

| Key | Id | Name |
|---|---|---|
| MSG_SKIN_PRIMARY | 0xf10 | GUNMETAL |
| MSG_SKIN_VARIANTB | 0xf11 | SUNFIRE |
| MSG_SKIN_VARIANTC | 0xf12 | DESERT |
| MSG_SKIN_VARIANTD | 0xf13 | WASP |
| MSG_SKIN_VARIANTE | 0xf14 | SLATE |
| MSG_SKIN_VARIANTF | 0xf15 | JUNGLE |
| MSG_SKIN_VARIANTG | 0xf16 | ARCTIC |
| MSG_SKIN_VARIANTH | 0xf17 | SPECTRE |

`damage` is not in the DLL's table, so it stays `damage`, and the code
compares the result with `"damage"` (0x5e5454).

## The objects

MSVC 6 `std::vector<T*>` is `{allocator byte, first, last, end}`; the
allocator byte is never used. The constructors copy into it the low byte of
their first stack argument (garbage, since the allocator is empty). A
differential test should skip it.

**SkinSet**, 0x14 bytes, `MechDfn` +0x1c:

| Off | Type | What |
|---|---|---|
| +0x00 | u8 | allocator (low byte of the ctor's count) |
| +0x04/+0x08/+0x0c | Skin** | `skins` first/last/end: every skin except `damage`, in file order |
| +0x10 | Skin* | the `damage` skin, or 0 |

**Skin**, 0x18 bytes:

| Off | Type | What |
|---|---|---|
| +0x00 | char* | name, `_strdup` of the localized name |
| +0x04 | u8 | allocator (low byte of the name pointer) |
| +0x08/+0x0c/+0x10 | SkinEntry** | `entries` first/last/end, one per texture slot |
| +0x14 | u8 | textures loaded (set by 0x474430, cleared by 0x4744a0); 0 after reading |

**SkinEntry**, 0x1c bytes, one texture slot:

| Off | Type | What |
|---|---|---|
| +0x00 | void* | the texture, 0 until loaded (0x473d40) |
| +0x04 | u8 | 1 when +0 came from the zImage cache 0x5179d0 (decides how it is released) |
| +0x08 | char* | name, `_strdup`, or 0 |
| +0x0c | char* | alternate name, `_strdup`; **the same pointer as +8** when none was given |
| +0x10 | SkinEntry* | the linked damage entry (0x474740), 0 after reading |
| +0x14 | u8 | 0 |
| +0x18 | int | reference count, 1 |

The alternate name: the loader 0x473d40(by_name, use_alt) loads +0xc instead
of +8 when `use_alt` is set or when +8 is exactly `vult_d03_c` (0x5e5790).

## Globals

| Addr | What |
|---|---|
| 0x6061bc | `std::list<SkinEntry*>` of every live entry: sentinel node `{next, prev, value}`, made by the static initializer 0x473ae0 (0x473ad0), freed at exit by 0x473b20 |
| 0x6061c0 | number of entries in that list |
| 0x6061b8 | its allocator byte |
| 0x6061a8 (+0x6061ac first, +0x6061b0 last, +0x6061b4 end) | `std::vector<texture*>` of the eight bullet-hole textures |
| 0x6061c8 | number of SkinSets ever built (never decremented here) |
| 0x6061cc | u8 flag set at mission load (0x48fc69, `mission.cpp`); when set, skins work in the damage-texture mode. Exact meaning unverified |

## The call site, 0x46be07-0x46bfd8

`ebp` is the `MechDfn`, `[ebp+8]` the reader tree. Runs after the aim keys
(+0x1a4) and before `firepoints`.

```
skins = zrdr::find(dfn->reader, "skins")           // 0x5569e0
if (!skins) skip                                   // dfn+0x1c stays 0 (ctor 0x46b890)
list  = skins->list                                // [skins+4]
pairs = (list[0].i - 1) / 2                        // cdq/sub/sar: rounds toward 0
set   = new(0x14) ? SkinSet(pairs) : 0             // 0x474f60
dfn->skins = set                                   // +0x1c
for i in 0..pairs-1:
    name  = localize(list[1 + 2i].value)           // 0x575fb0, no type check
    items = list[2 + 2i].value                     // used as a list, no type check
    if (i == 0 || strcmp(name, "damage") == 0):
        skin = new(0x18) ? Skin(name, items[0].i - 1) : 0      // 0x473e50
        set->Add(skin)                                         // 0x4752e0, before filling
        for each item k of items:                              // items[1..]
            type 1 (int):    skin->AddEntry(0, 0)              // 0x474000
            type 3 (string): skin->AddEntry(str, 0)
            type 4 (list):   skin->AddEntry(l[1].value, l[2].value)   // no type checks
            type 2 (float):  nothing
    else:
        base = set->skins.size() > 0 ? set->skins[0] : 0
        skin = base->Derive(name, items[1].value)              // 0x474500, the suffix
        if (skin) set->Add(skin)
```

The first pair always becomes a full skin whatever its name; every later pair
that is not `damage` is a variant of `skins[0]`, using only its first item as a
suffix. An int item makes an empty slot (no names), which keeps the damage list
aligned with the primary list by index.

## The five functions

### 0x474f60 `SkinSet::SkinSet(int count)`, ret 4, returns `this`

- +0 = low byte of `count`; +4, +8, +0xc, +0x10 = 0.
- If `count != 0` (unsigned): `reserve(count)`: `operator new(4 * max(count, 0))`,
  copy the (empty) old range with 0x475bf0, `operator delete` the old buffer,
  end = new + 4·count, first = last = new.
- If `byte [0x6061cc]` and the hole vector 0x6061a8 is empty: for each of
  `hole_8x8`, `hole_8x8a`, `hole_8x8b`, `hole_8x8c`, `hole_16x16`,
  `hole_16x16a`, `hole_16x16b`, `hole_16x16c` (0x5e58a0 ... 0x5e584c, in that
  order), `tex = 0x5179d0(ECX = name)`; if non-zero, insert it at the end of
  the hole vector (0x4aa780, ECX = 0x6061a8, args `(last, 1, &tex)`).
- `[0x6061c8]++`.

So the hole textures are fetched once, by the first SkinSet built while the
flag is set. 0x5179d0 (zImage, between `zimg_winfonts.cpp` and
`zimg_library.cpp`) looks a texture up by name in a reference-counted cache at
0x6384ec, loading it on a miss (0x5177a0 find, 0x5176b0 load).

### 0x473e50 `Skin::Skin(const char* name, unsigned count)`, ret 8, returns `this`

- +0 = 0, +4 = low byte of `name`, +8, +0xc, +0x10 = 0, +0x14 = 0.
- +0 = `_strdup(name)` (IAT 0x5bb670).
- If capacity < `count` (unsigned): `reserve(count)` as above.

No entries are made here.

### 0x4752e0 `bool SkinSet::Add(Skin* skin)`, ret 4

- `skin == 0` or `skin->name == 0`: return false (al = 0).
- `strcmp(skin->name, "damage") == 0` (case-sensitive): `this->damage = skin`
  (+0x10), return true. It does not go in the vector; a second `damage`
  would replace the first without freeing it.
- Otherwise `push_back` onto +4..+0xc, return true.

`push_back` (inlined, here and in 0x474000): if `end - last >= 1`, store and
`last += 4` (via 0x475bf0/0x475c20 with zero counts). Otherwise grow to
`size + max(size, 1)` (`size <= 1` gives +1), copy with 0x4d5110, store the new
one with 0x475c20, delete the old buffer. The reader reserves enough up front,
so with the shipped files growth never happens (the set reserves 9 and gets 8,
each skin reserves exactly its item count).

### 0x474000 `void Skin::AddEntry(const char* name, const char* alt)`, ret 8

- `e = operator new(0x1c)`, then 0x473a30(name, alt) on it (`SkinEntry` ctor,
  ret 8): +0 = 0, +4 = 0, +8 = 0, +0xc = 0, +0x10 = 0, +0x14 = 0, +0x18 = 1;
  +8 = `_strdup(name)` if `name`; +0xc = `_strdup(alt)` if `alt`, else +0xc =
  +8 (the same pointer, or 0). Then appends `e` to the global list 0x6061bc
  (new 0xc-byte node `{sentinel, sentinel->prev, e}` linked before the
  sentinel) and `[0x6061c0]++`.
- `push_back(e)` onto `this` +8..+0x10.

If `operator new` fails it pushes 0.

### 0x474500 `Skin* Skin::Derive(const char* name, const char* suffix)`, ret 8

`this` is the base skin (`set->skins[0]`).

- Base has no entries: return 0, no skin made.
- `s = new(0x18) ? Skin(name, base->entries.size()) : 0`.
- For each base entry `e`, in order:
  - `buf1[64]` = `strcpy(e->name) + strcat(suffix)` (inline `rep movs`, no
    bounds check).
  - If `e->alt != e->name` (pointer compare): `buf2[64]` = `e->alt + suffix`;
    `s->AddEntry(buf1, buf2)`. Else `s->AddEntry(buf1, 0)`, so the derived entry
    again shares one pointer.
- Return `s`.

`mcat04`/`mcat26` with `_b` gives `mcat04_b`/`mcat26_b`; a plain `mcat01`
gives `mcat01_b` with no alternate.

## Counts to check

After one file: `set->skins.size()` = 8 (primary + 7 variants), every variant
has the primary's entry count, `set->damage` has the damage list's length, and
`[0x6061c0]` grows by 8·primary + damage (madcat: 8·23 + 25 = 209), all with
refcount 1, texture 0, link 0.

## Porting order (leaves first) and checks

1. **SkinEntry ctor 0x473a30** with the global entry list (0x6061bc/0x6061c0).
   In the port the list can be a `std::list<SkinEntry*>` in a struct with the
   addresses in comments; keep the "alt = same pointer as name" rule as a flag
   or by sharing the string, since 0x474500 and the loader depend on it.
2. **Skin ctor 0x473e50**, **Skin::AddEntry 0x474000**.
3. **SkinSet ctor 0x474f60** and **SkinSet::Add 0x4752e0**. The hole-texture
   block needs zImage's 0x5179d0; until zImage's cache is ported, pass it in as
   a callback (or skip it when the flag is clear, which is what the reader test
   will exercise).
4. **Skin::Derive 0x474500**.
5. **The skins block of 0x46ba40**, taking the reader tree and a localize
   callback (0x575fb0). Localization is `Mech3Msg.dll`'s table; the port needs
   the eight `MSG_SKIN_*` names above, or the table read from the DLL by the
   importer.

`std::vector` is fine in the port: capacity is never observable with the
shipped files. Keep the vectors' order and contents identical.

Emulator check (harness `tests/differential/original.py`):

- Stub the imports: `operator new` MFC42 #823 at IAT 0x5bb4bc and
  `operator delete` #825 at 0x5bb4c8 (the thunks are 0x5abd26 and 0x5abd1a),
  `_strdup` at 0x5bb670 (allocate and copy in Python), as `test_gamez.py` does
  for malloc/free.
- **Run the static initializer first** or build the list sentinel by hand:
  0x6061bc must point to a node whose next and prev point to itself, or
  0x473a30 writes through 0. Calling 0x473ae0 does it (it uses operator new).
- Leave `[0x6061cc] = 0` so 0x5179d0 is never called; for a second run, set it
  to 1 and stub 0x5179d0 (thiscall, ECX = name) to return a fake pointer per
  name, then check the hole vector at 0x6061a8 has the eight in order.
- Stub 0x575fb0 (ECX = string, returns string) to return a Python-allocated
  localized name, or leave `[0x72c670] = 0` so it returns its argument (then
  the names stay `MSG_SKIN_*`). Both are worth a run; the port must give the
  same as the stub.
- Build the reader tree in emulator memory with the original reader (as
  `test_zreader.py` does with 0x557180) or copy the port's tree in, then run
  the original code range 0x46be07..0x46bfd8 directly with `ebp` = a fake
  0x1a8-byte `MechDfn` (+8 = tree) and a scratch stack frame of at least 0xc0
  bytes (it uses `[esp+0x14]`, `+0x20`, `+0x28`, `+0x38`, `+0x48`, `+0xb8`).
  That avoids running the rest of 0x46ba40. Calls it makes: 0x5569e0 (find),
  0x575fb0, 0x5abd26, 0x474f60, 0x473e50, 0x4752e0, 0x474000, 0x474500,
  0x4fd1c0 (vector size, ECX = set).
- Walk the SkinSet from `[dfn+0x1c]` and compare with the port: skin names,
  entry names (+8, +0xc, and whether +0xc == +8), refcounts, damage skin,
  `[0x6061c0]`, `[0x6061c8]`. Do it for all 19 `dfn_*.zrd`.
- Each function also stands alone for small cases: Derive with a pair entry
  and a plain entry, Add with `damage`, with 0, with a nameless skin.

## Next layer (not mapped in detail)

Used after reading, not by the reader: 0x474430 `Skin::LoadTextures(by_name,
use_alt)` (0x473d40 on every entry, then +0x14 = 1), 0x4744a0 `Skin::Unload`
(0x473e10 on each entry: 0x517b10 or 0x518b30 by the +4 flag), 0x473d40
`SkinEntry::Load` (0x5179d0 by name, or 0x518220 then 0x518d10(name, 3, 0)),
0x473c60 `SkinEntry::Release` (refcount, 0x473ca0 dtor removes it from the
global list), 0x473bb0 copy ctor, 0x473f40 Skin dtor, 0x4751a0 SkinSet dtor
(from the `MechDfn` dtor 0x46b9b0), 0x475520 `SkinSet::LinkDamage` (for every
skin, entry j gets damage entry j through 0x474740, which refuses a size
mismatch: "Damage Skin size error" / "Failed to link damage to texture ... not
loaded", `zerr` 0x49bfd0 level 0x200), and 0x4748b0 / 0x4749e0, which walk a
model's node tree and swap material textures for a skin's by index (0x474bd0
finds a texture's slot, 0x474b70 its index).

## The skins in play (ported 2026-10-21)

`mech3/mech_skin.cpp`; checked by `test_skins.py` and
`mission_c1m1_mech_wears_its_skin_headless`.

- **The type's model** (0x00475780, from the mech data's load 0x00430ad0):
  the damage skin linked (0x00475520), then the first skin's walk
  (0x004748b0) over every polygon's material of the model tree. Each
  material gets the soil "mech" (0x00535f20). Its texture, by name, finds the
  first skin's entry. An entry without a texture takes the model's own
  texture (with HOLES, or when it has no alternate name). Otherwise
  (NOHOLES and an alternate name) the entry loads its texture
  (0x00473d40(1, 1)), and the material takes it. With HOLES, a material
  whose entry has a damage link is marked flag 2: each copy of the model
  then owns that material (0x00536ab0).
- **The mech's own skin** (0x00475a80, from the build 0x00415b00): skin 0
  copied for this mech (0x00475870) and swapped onto its model tree
  (0x004749e0). The copy shares the entries (one more reference each),
  but with HOLES an entry that has a damage partner is copied
  (0x00474210, 0x00473bb0), so its bullet holes can be painted into its
  own texture (0x00474d90, not ported). The mech keeps the copy at +0x3e8.
- **A skin chosen later** (0x004757f0: the launch's slot skin, aiv.zrd's
  `skin`, 0x00414390): the chosen skin copied as above, swapped in on
  the model (+0x694) and its low-detail copy (+0x698), from the old copy's
  textures to the new one's by index, and the old copy deleted.
- **The swap** (0x004749e0): a textured material's texture is found in the
  old skin by name, else by alternate name (0x00474bd0); the same index in
  the new skin (0x00474b70) gives the new texture, loaded first when it has
  none (0x00473d40(1, NOHOLES)).
- **The damage link** (0x00475520, once per set, with HOLES only): the
  damage skin's textures from zImage's cache (0x005179d0, not ported, so
  in the program every link reports "not loaded" and links nothing), then
  every skin's entry j linked to damage entry j (0x00474740). Mismatched
  image sizes refuse the link, but at the first link no skin entry has a
  texture yet, so that check never runs in play.
- **An entry's texture** (0x00473d40): by its alternate name with
  `use_alt`, or always for `vult_d03_c` (0x005e5790). By name, a table entry
  is found (0x00518220, only when the alternate name is used) or a new one
  loaded in group 3 (0x00518d10). Not by name, zImage's cached image
  (+0x04 set).

The program sets HOLES from the mission load's decision ([0x006061cc],
`set_holes`) into `skin_globals().damage_mode` too.

Without HOLES the materials of every mech of a type are shared, so a skin
swapped onto one mech is worn by all of them (discoveries.md).

## Discoveries

Listed in `discoveries.md` (Mechs: camos).

## Unverified

- The meaning of 0x6061cc and of the alternate name (second item of a pair);
  which name the game loads in which mode.
- Whether the Mad Cat / Supernova mismatches really fail at run time.
- 0x575fb0's behaviour when the DLL is missing (`[0x72c670]` = 0): names stay
  `MSG_SKIN_*`, read from the asm, not run.
- Nothing here has been run in the emulator yet.

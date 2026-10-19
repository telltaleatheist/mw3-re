# Mech definition: the head, the entry, and the saved loadouts

Mech3.exe 1.2, 2026-10-19. Mapped from the asm
(`analysis-output/mech3.asm`), checked in the emulator. Ported: the head
(`mech3::read_head`, `src/mech3/mech_reader.*`), identical on all 19 files in
`test_mechdef.py` (the image and the five values it leaves on the stack).
Not ported yet: the entry 0x4846a0 / 0x46b7a0, 0x424b70 and the dtor.
Completes 0x46ba40 (`mech3\mech_reader.cpp`) after `mechdef-skins.md`,
`mechdef-parts.md` and `mechdef-configs.md`, and maps 0x424b70 (saved
loadouts, `configs\<mech>.mw3`) with the Config file I/O it uses.

Emulator (`tests/differential/explore/mechdef_head_emu.py`, scratch, not a
test):

1. The original head, 0x46ba95..0x46be07, after the real ctor 0x46b890 on all
   19 `dfn_*.zrd`, against a Python model of this doc: every field and the
   five stack slots match (0 mismatches; floats within 2e-7 relative).
2. Madcat with `version` patched to 3: 0x46ba40 returns 0, +0x20 = 0,
   `loaded` = 0. The dtor 0x46b9b0 on that dfn then **faults at 0x424f9b**
   (reads `[0 + 4]`).
3. All 62 built-in configs (built by 0x46ba95..0x46cb3c), with +0x1c set,
   written by the original Config::Save 0x424480 and read back by 0x4242a0
   (ctor + Load + Validate): 62 accepted, every saved field equal, item lists
   equal; each with one armor byte flipped: rejected, except the elemental's.

Survey: `tests/differential/explore/mechdef_head_survey.py` (per file) and
`mechdef_head_table.py` (one row per mech). No `configs/*.mw3` exist in
`game/mw3-1.2/` (the install never ran the game), so the file format below
comes from the code and the emulator's own saves.

## How a MechDfn is made

```
0x4846a0 GetMechDfn(ECX = index)        // 48 call sites
    if index not in 0..18: return 0
    e = &mech_table[index]              // 0x5e6078, 0x3c bytes per mech
    if !e->dfn: e->dfn = 0x46b7a0(index)   // +0x38 (0x5e60b0); a 0 is not cached
    return e->dfn
```

Mech table entry (0x3c bytes, 19 entries, alphabetical: 0 annihilator,
1 avatar, 2 blackhawk, 3 bushwacker, 4 cauldron, 5 champion, 6 daishi,
7 elemental, 8 firefly, 9 madcat, 10 orion, 11 owens, 12 puma, 13 shadowcat,
14 strider, 15 sunder, 16 supernova, 17 thor, 18 vulture): +0 name,
+0x28 float max mass (0x4846e0; 10.0 out of range), +0x30 tech (0x484720),
+0x38 the cached `MechDfn*`.

```
0x46b7a0 LoadMechDfn(ECX = index)       // __thiscall-like, no stack args, plain ret
    CString path; path.Format("dfn_%s.zrd", mech_name(index))   // MFC42 #540, #2818
    tree = zrdr::load(ECX = path, EDX = "..\data\common\zrdr\mechs", 0)   // 0x556740
    if !tree: return 0                  // CString freed (#800)
    dfn = new(0x1a8) ? MechDfn(index, tree) : 0                  // 0x46b890, ret 8
    if dfn && !dfn->Read():             // 0x46ba40
        dfn->~MechDfn(); delete dfn     // 0x46b9b0 -> crashes, below
        dfn = 0
    return dfn
```

`load` ignores the directory for archives (`zreader.md`): the name is looked
up in the open archives, i.e. `reader.zbd`.

**0x46ba40 `MechDfn::Read()`**: `__thiscall`, ECX = the `MechDfn`, no stack
arguments, plain `ret`, returns `al` (1 ok, 0 failed). Frame: `ebp` = this,
`[esp+0x3c]` = this, an SEH frame (state at `[esp+0xb8]`), 0xa0 bytes of
locals after `and esp, -8`.

## The head, 0x46ba40..0x46be07

| Range | What |
|---|---|
| 0x46ba40..0x46ba95 | prologue; `+8` (tree) == 0 → return 0 (0x46cb60); `+4` (loaded) set → return 1 |
| 0x46ba95..0x46bad4 | tech → s+0x54; `version`; `chicken` |
| 0x46bad4..0x46bb6f | movement defaults → stack; four getters; s+0x44 halved; `crouch_height`; `engine_minmax` |
| 0x46bb6f..0x46bbd2 | `+0x14` = 400; `torso_limits` |
| 0x46bbd2..0x46bc72 | `torso_shake` sub-list |
| 0x46bc72..0x46bd0d | `lod_range`, `lean_*`, `recoil_bend` |
| 0x46bd0d..0x46be07 | `recoil_slide`, `recoil_twist`, `aim_*` |

All getters take ECX = the node (`+8` for top-level keys) and EDX = the key;
the out pointer is the stack argument (ported zReader: 0x556ae0 get_int,
0x556a60 get_float, 0x557150 get_bool, 0x557110 get_ivec2, 0x5570b0
get_vec3, 0x5570d0 get_vec2, 0x5569e0 find). Each writes only on success.
`rad(x)` = `x · 0.01745329251994` (double 0x5c3398, x87, stored as float),
applied only when the getter returned true.

```
s54 = mech_tech(this->index)                         // 0x484720, ECX = +0
if !get_int(tree, "version", &+0xc) || +0xc != 2: return 0     // 0x46cb60
get_bool(tree, "chicken", &+0x18)
s44 = 1.2566 (0x3fa0d97a, 72 deg); s58 = 0.5; s5c = 1.3; s60 = 1.5
get_float(tree, "max_turn_rate", &s44)
get_float(tree, "accel_rate", &s58)
get_float(tree, "decel_rate", &s5c)
get_float(tree, "turn_accel_factor", &s60)
s44 = s44 * 0.5f                                     // float 0x5c338c; always, default too
get_float(tree, "crouch_height", &+0x148)
get_ivec2(tree, "engine_minmax", &+0x10)             // writes +0x10, +0x14
+0x14 = 400                                          // the file's maximum is discarded
if get_vec3(tree, "torso_limits", tmp3):             // tmp3 = stack +0x84, uninitialised
    +0x174 = rad(tmp3[0]); +0x17c = rad(tmp3[1]); +0x178 = rad(tmp3[2])
if (sh = find(tree, "torso_shake")):
    if get_float(sh, "pitch_amp", &+0x160): +0x160 = rad(+0x160)
    if get_float(sh, "yaw_amp",   &+0x164): +0x164 = rad(+0x164)
    if get_float(sh, "roll_amp",  &+0x168): +0x168 = rad(+0x168)
    get_float(sh, "freq", &+0x16c); get_float(sh, "damp_freq", &+0x170)
if get_vec2(tree, "lod_range", tmp2):                // stack +0x7c; raw copy
    +0x14c = tmp2[0]; +0x150 = tmp2[1]
if get_float(tree, "lean_angle", &+0x154): +0x154 = rad(+0x154)
get_float(tree, "lean_factor", &+0x158)
get_float(tree, "lean_speed", &+0x15c)
if get_float(tree, "recoil_bend", &+0x180): +0x180 = sin(rad(+0x180))   // fsin, extended
get_float(tree, "recoil_slide", &+0x184)
if get_float(tree, "recoil_twist", &+0x188): +0x188 = rad(+0x188)
get_float(tree, "aim_slerp_freq", &+0x18c)
for (off, key) in (0x190 aim_min_pitch, 0x194 aim_max_pitch, 0x198 aim_min_yaw, 0x19c aim_max_yaw):
    if get_float(tree, key, &+off): +off = rad(+off)
get_float(tree, "aim_arm_pitch_factor", &+0x1a0)
get_bool(tree, "aim_ignore_arms", &+0x1a4)
// 0x46be07: skins (mechdef-skins.md)
```

Quirks kept: the `torso_limits` and `lod_range` temporaries are uninitialised
stack, so a component of the wrong type (the getter keeps "its old value")
copies stack garbage into the field; nothing in the shipped files does this.
`version` stays in +0xc even when it is not 2.

### Stack slots the configurations block reads (confirmed)

| Slot | Default | Key | Config field |
|---|---|---|---|
| +0x44 | 1.2566 · 0.5 = 0.6283 | `max_turn_rate` · 0.5 | +0x74 |
| +0x54 | | mech tech (0x484720) | tech of items, armor |
| +0x58 | 0.5 | `accel_rate` | +0x7c |
| +0x5c | 1.3 | `decel_rate` | +0x80 |
| +0x60 | 1.5 | `turn_accel_factor` (in no file) | +0x84 |

Plus +0x3c = this. The table in `mechdef-configs.md` is right.

### The tail, 0x46cb3c..0x46cb77

```
0x46cb3c: this->configs->LoadSaved()   // 0x424b70, ECX = +0x20; result ignored
0x46cb44: +4 = 1; return 1
0x46cb60: return 0                     // tree missing, bad version
```

No `configurations` jumps to 0x46cb44 (no LoadSaved, +0x20 stays 0).

## MechDfn fields the head writes

Ctor defaults from 0x46b890 (already ported). "Files" is how many of the 19
have the key (depth-first `find`).

| Off | Type | Name | Ctor | Key | Files |
|---|---|---|---|---|---|
| +0x0c | int | version | 0 | `version` (must be 2) | 19 |
| +0x10 | int | engine_min | 10 | `engine_minmax`[0] | 18 (not elemental) |
| +0x14 | int | engine_max | 400 | `engine_minmax`[1], **then forced to 400** | |
| +0x18 | u8 | chicken | 0 | `chicken` | 19 |
| +0x148 | float | crouch_height | 1.0 | `crouch_height` | 18 (not elemental) |
| +0x14c, +0x150 | float[2] | lod_range | 95, 85 | `lod_range` | 0 |
| +0x154 | float | lean_angle, rad | 0.4363 (25 deg) | `lean_angle` | 19 |
| +0x158 | float | lean_factor | 1.0 | `lean_factor` | 19 |
| +0x15c | float | lean_speed | 1.0 | `lean_speed` | 0 |
| +0x160 | float | torso_shake pitch_amp, rad | 0.4887 (28 deg) | | 19 |
| +0x164 | float | yaw_amp, rad | 0.4363 (25 deg) | | 19 |
| +0x168 | float | roll_amp, rad | 0.2199 (12.6 deg) | | 19 |
| +0x16c | float | freq | 14.0 | | 19 |
| +0x170 | float | damp_freq | 2.9 | | 19 |
| +0x174 | float | torso max yaw, rad | **not set** | `torso_limits`[0] | 19 |
| +0x178 | float | torso max pitch, rad | **not set** | `torso_limits`[2] | 19 |
| +0x17c | float | torso min pitch, rad | **not set** | `torso_limits`[1] | 19 |
| +0x180 | float | recoil_bend, **sine** of the angle | 0.5 | `recoil_bend` | 19 |
| +0x184 | float | recoil_slide | 0 | `recoil_slide` | 19 (strider's is a string) |
| +0x188 | float | recoil_twist, rad | 0.3491 (20 deg) | | 19 |
| +0x18c | float | aim_slerp_freq | 3.0 | | 19 |
| +0x190 | float | aim_min_pitch, rad | -1.3963 (-80 deg) | | 19 |
| +0x194 | float | aim_max_pitch, rad | 1.2217 (70 deg) | | 19 |
| +0x198 | float | aim_min_yaw, rad | -1.3963 | | 19 |
| +0x19c | float | aim_max_yaw, rad | 1.3963 | | 19 |
| +0x1a0 | float | aim_arm_pitch_factor | 0.3 | | 19 |
| +0x1a4 | u8 | aim_ignore_arms | 0 | `aim_ignore_arms` | 12 |

The torso names come from the files themselves: 17 of 19 carry a comment the
text compiler kept as stray top-level strings, `"//" "torso" "limits:"
"maxYaw" "minPitch" "maxPitch" ("degrees")`, right after `torso_limits`
(e.g. daishi `(100.0 -18.0 18.0)`). Harmless: no key the reader asks for is
among them (the configs' `torso` lookups start at the config body).

Ranges in the files: max_turn_rate 1.8 (annihilator) .. 7 (elemental);
torso max yaw 5 deg (firefly) .. 110; recoil_slide 0, 0.75, 1.0 or 1.5.

**For `types.json` (MechDfn)**: add +0x0 int `index`, +0x4 u8 `loaded`,
+0x14c float[2] `lod_range`, +0x20 `ConfigList*` `configs`, +0x24
`FirepointList*` `firepoints`, +0x28 `PartSlot[18]` `parts`; rename +0x10 to
int `engine_min` (not `int[2]`), +0x14 `unk_14` to `engine_max` ("forced to
400 after engine_minmax"), +0x174 `torso_max_yaw`, +0x178 `torso_max_pitch`,
+0x17c `torso_min_pitch`; note +0x180 is a sine.

Readers seen in a quick scan of the 48 GetMechDfn sites (not traced):
+0x18 → Mech +0x585 (0x415e72, next to building the hip from slot +0x118);
+0x148 → the hip ctor 0x4620a0 (0x46219c); +0x14c/+0x150 squared ×k at
0x430840 (LOD distances); +0x160.. → the torso ctor (0x4642b6); +0x180..
+0x18c → the arm ctor (0x466409). Nothing found reading +0x10/+0x14.

## The dtor crash (0x46b9b0)

The dtor first calls **0x424f60 `ConfigList::SaveAll`** on +0x20 with no null
check, and SaveAll reads `[this + 4]` after making `configs\`. So when Read
fails *after* the tree loaded (a missing or wrong `version`, the only such
exits), +0x20 is still 0 and the game faults (emulator: 0x424f9b). A missing
file (no tree) returns 0 cleanly. Port note: a FIX (B)-style guard is not
needed for shipped data; keep the behaviour or log and fail.

The dtor is also called for every cached dfn at shutdown (0x4844b0 walks the
19 entries), so **player loadouts are written when the mech data is freed**.

Ported (`MechDfn::~MechDfn`, with the skin, loadout and firepoint dtors
0x4751a0, 0x473f40, 0x473ca0, 0x473c60, 0x473e10, 0x4744a0, 0x425110,
0x424350); the crash is fixed by skipping the save without loadouts
(`fixes.md`).

## 0x424b70 `ConfigList::LoadSaved()`

`__thiscall`, ECX = the ConfigList (`MechDfn` +0x20), returns `al`: 1 when the
file opened (even if nothing was read), 0 when not. The reader ignores it.

```
sprintf(path, "configs\%s.mw3", mech_name(this->mech))   // +4; sprintf 0x5bb658
Archive a; init(a)                                       // 0x5764e0, zutl_zar
if !open(a, path): destroy(a); return 0                  // 0x576520, relative to the cwd
ver = 0; size = 4
if read(a, find(a, "version"), &ver, &size) == 0 && ver == 15:   // 0x576a80, 0x576b30, 0x423cf0
    for i in 1 .. a.count-1:                             // entry 0 skipped, whatever it is
        name = a.entries[i].name                         // +8 of the 0x94-byte entry
        c = Config::FromFile(ECX = name, EDX = seek(a, i, 0))   // 0x4242a0, 0x576ae0
        if c: this->vec.push_back(c)                     // inline vector insert, grows
close(a); destroy(a)                                     // 0x576650, 0x576500
return 1
```

- A `version` entry that is missing, longer than 4 bytes (0x10002), or not
  15 skips the whole file silently. Shorter than 4 leaves the high bytes 0.
- The loop does not compare the config's mech index with the list's: a
  config is appended to whichever mech's file it is in.
- Saved configs follow the built-in ones in the same vector, so their
  indexes are 1..5 + n. Nothing removes duplicates.

**0x4242a0 `Config::FromFile(ECX = name, EDX = HANDLE)`**: name 0 or handle
-1 → 0. `new(0xd8)`, `Config(mech = -1, name)` (0x423d00), then
`Load(handle)`; false → dtor 0x424350, delete, return 0.

**0x424600 `Config::Load(HANDLE h)`** (thiscall, ret 4). Reads with `ReadFile`
(0x5bb0c4); byte counts are never checked, so a short file leaves fields as
the ctor set them:

```
if h == -1: return 0
free(+0x18); +0x18 = read_string(h)          // 0x576460: u32 len, bytes; malloc(len+1)
ItemList::Load(h)                            // 0x471540, below
read +0x14 (4)      mech index (the ctor's -1 is replaced)
read +0x70 (12)     the three movement dwords +0x70, +0x74 (turn rate), +0x78
read +0x7c, +0x80, +0x84 (4 each)   accel, decel, turn_accel
read +0x20 (1)      endo steel
read +0x88 (4)      checksum
read +0x1d (1), +0x1f (1)
read +0x2c (4) armor tons, +0x30 (4) armor tech, +0x34 (1) ferro, +0x38 (0x2c) 11 armor ints
Engine::Load(h) on +0x24                     // 0x432ff0: +0x24 index (4), +0x28 xl (1)
Sinks::Load(h) on +0x64                      // 0x4243c0: +0x64 tech (4), +0x68 count (4), +0x6c dsink (1)
+0x1c = 1
return Validate(true)                        // 0x4254d0
```

**0x471540 `ItemList::Load(h)`** (thiscall on the Config, ret 4): u32 n, then n
records `u32 kind` + the item's own load; each item goes to the list's end
(inline insert, +0xc count += 1). Kinds (jump table 0x471778):

| Kind | Object | Load | Record after `kind` |
|---|---|---|---|
| 0 | base item 0x18, vt 0x5bc160 | 0x472b20 | count, id → +0x14, part, group |
| 1 | weapon 0x1c, vt 0x5c0fc0 | 0x472b90 | base + tech (+0x18) |
| 2 | ammo 0x18, vt 0x5bd150 | 0x472b20 | base (id = weapon index) |
| 3 | equipment 0x20, vt 0x5bd180 | 0x472c00 | base (id → +0x18 kind, 0x41ec40) + tech (+0x1c); **kind 1 (standard armor) forces tech 3** |
| 4 | 0x1c, vt 0x5c3700 | 0x472c80 | base + one byte (+0x18). Never made by the dfn reader; unknown |

Base record (0x472b20): `+4 count`, `id` (passed to vt+0x10, which stores
+0x14, or +0x18 for equipment), `+0xc part`, `+8 group`, 4 bytes each; +0x10
is 0. **A kind above 4 makes no item and reads nothing more**, so the rest
of the file is read out of step (no error).

**0x4254d0 `Config::Validate(bool crc)`** (ret 4):

1. mech index (+0x14) == 7 (**elemental**) → true, nothing checked.
2. if `crc`: 0x4255b0 must equal +0x88. The checksum is CRC-32 (table at
   0x72eec0, 0x576c10 `crc = table[(crc >> 24) ^ b] ^ (crc << 8)`, start 0)
   over: the name (strlen bytes), a packed copy of the config made by
   0x425650 (malloc'd, `0x34 + 12 · items` bytes, its length in the low 16
   bits of its first dword; holds the mech index, +0x1c, +0x20, the engine
   index (7 bits) and more, not mapped bit by bit), then +0x70 (12), +0x7c,
   +0x80, +0x84.
3. total weight 0x425300(-1) − 0.24 (float 0x5bd418) <= max mass (0x4846e0).
4. 0x424220 false: no item whose part is -1 has a non-zero vt+0x18 (an
   unplaced item that needs a slot; reading, not confirmed).
5. jump jets (equipment kind 11, counted by 0x425ff0) <= 0x425fb0: 2 when
   max mass <= 40 t, 3 when <= 65 t, else 4.
6. sum of the 11 armor ints <= `armor_points(+0x30, +0x34, +0x2c)` (0x424760).

The other Validate caller, 0x41445e (mech setup), and a second loader at
0x4260f0.. (ctor, Load, then its own CRC compare) are not traced.

Ported (`mech3/mech_config`: `Config::load`, `load_items`, `packed`,
`checksum`, `weight`, `validate`, `from_file`, `ConfigList::load_saved`;
`mech_names`: the engine, equipment and weapon weight and slot tables;
`zutl_zar::crc32`), checked by `tests/differential/test_mechsave.py`: 1,111
configs saved by the original (built-in, changed in memory, damaged, cut short,
with kind 0/4/5 items spliced in) load identically, and 152 whole files.
Weapon weights and slots come from zWeapon's run-time copy of `weapons.zrd`
(the block at +0x1f0, `weapons.md`), not ported: `set_weapon_lab` hooks it.

Corrections from the port:

- Kind 4 is an **engine item** (vt 0x5c3700): +0x14 the engine index, +0x18
  XL; it weighs `count x` the engine's tons, clamped to index 0..79 (79 is one
  past the table), and needs no slots. Kind 0 (vt 0x5bc160) weighs 0 and needs
  no slots.
- vt+0x14 is the item's id (equipment: its kind), vt+0x18 the slots it needs:
  weapons from the run-time block (+0x20 Clan, +0x2c otherwise), ammo 1,
  equipment from 0x484a20 except **ferro-fibrous, which returns the item's
  group field**. Check 4 of Validate is "an item with part -1 that needs
  slots".
- The packed form, word 1: bit 0 +0x1c, bit 1 endo, bits 2-8 engine index,
  bit 9 XL, bit 10 ferro, bits 11-29 `ftol(armor tons x 2)`, bits 30-31 armor
  tech; words 2-12 the armor; then per weapon, ammo or equipment item: kind
  (bits 0-2), id (3-18), tech (19-20, not ammo), count (21-31), group, part.
  Items of kinds 0 and 4 are skipped and not counted in the length.
- Weight(-1) = the sum of categories 0..7 (stored as a float each step) + 3 t
  (0x5e24f0) + `ceil(rating x 0.009999)`, minus 0.5 t for the Mad Cat (mech
  9). Category 1 is `equip_weight(dsink ? 10 : 9, sink tech) x (sinks - 10)`,
  never below 0; category 3 is `max mass x 0.1`, with endo steel `floor(x +
  0.5) x 0.5`.

### The writer, for the format

0x424f60 `ConfigList::SaveAll()` (also from 0x44a5aa, 0x44a653, 0x44d69a: the
mech lab, not traced): if `.\configs` does not exist (0x576080), `_mkdir
("configs")`; create `configs\<mech>.mw3` (0x5765b0, truncates); add entry
`version` = 15 (0x576990, 4 bytes); for each config **with +0x1c set**
(0x424f30), add an entry named after the config (+0x18) whose bytes come from
the callback 0x426590 → `Config::Save` 0x424480; close (writes the table).

`Config::Save` first stores the checksum (0x4255a0: +0x88 = 0x4255b0, so it
covers +0x1c = 1), then writes in Load's order. ItemList::Save (0x4717a0)
writes n = 0 when the list's +0x10 flag is set, else +0xc, then each item's
vt+0x28 (`kind` from vt+0xc first, then the base fields). **Save returns a
byte count 22 to 35 short** of what it wrote (emulator, 62 configs: 35 ×39,
31 ×19, 22 ×4; e.g. the four armor fields at 0x424586..0x4245d2 drop their
counts), and the archive stores that as the entry's length. Harmless in the
original, since Load reads the handle sequentially from the entry's start;
**a port must not bound the read by the entry length**.

### The `.mw3` file

A normal zArchive (`zutl_zar.h`): entries back to back, then the 0x94-byte
table, `u32 1`, `u32 count`. Entry 0 `version` = u32 15. Entries 1.. are
configs, little-endian, no padding:

```
u32 len; char name[len]            // e.g. 18 "MSG_CONFIG_PRIMARY", or the player's name
u32 n; n × { u32 kind; u32 count; u32 id; i32 part; u32 group; [u32 tech | u8] }
i32 mech; u32 m70; f32 turn_rate; u32 m78; f32 accel; f32 decel; f32 turn_accel
u8 endo; u32 crc; u8 b1d; u8 modified
f32 armor_tons; i32 armor_tech; u8 ferro; i32 armor[11]
i32 engine_index; u8 xl
i32 sink_tech; i32 sinks; u8 dsink
```

Madcat primary: 668 bytes, starting `12000000 "MSG_CONFIG_PRIMARY" 17000000`
(23 items) `03000000 01000000 0a000000 03000000 00000000` (equipment, count
1, dsink, part 3, group 0) `01000000` (its tech) ...

## Corrections to the existing docs

- `mechdef-configs.md`, Config: **+0x1c is the "player's own" flag**: set by
  Load, required by SaveAll, inside the checksum. **+0x1d** and **+0x1f** are
  saved and loaded. **+0x88 is the checksum**, not unused garbage
  (+0x8c..+0xd7 still untraced). +0x70..+0x84 are all saved. The base flag
  **+0x10** makes ItemList::Save write an empty list. The training flag
  +0x1e is not saved.
- `mechdef-configs.md`, 0x424b70: "returns 1 when the file opened" is right
  (also when the version is wrong); add that entry 0 is skipped by position,
  and that `Config` objects from files start with mech -1 and take +0x14
  from the file.
- `types.json` / `structs.md`: the MechDfn field list above. `structs.md`'s
  "built at 0x46b7a0" should read "loaded by 0x46b7a0 (cached by 0x4846a0),
  constructed by 0x46b890".

## Porting order and emulator plan

1. `GetMechDfn` 0x4846a0 and `LoadMechDfn` 0x46b7a0 over the ported zReader;
   the table fields (+0x28 max mass, 0x4846e0).
2. `read_head(MechDfn&)` = 0x46ba95..0x46be07 returning the five movement
   values (a small struct) for `read_configurations`. Check: the scratch
   run above as a test, port vs original image +0xc..+0x1a8 and the stack
   slots, all 19 files; plus crafted trees through the original: no
   `version`, `version (3)`, `version (2.0)` (a float fails get_int),
   `chicken ("on")`, `torso_limits` with two items (fields stay unset) and
   with a string component (garbage: seed the stack slot, compare), every
   angle key as an int.
3. `MechDfn::Read` = head + the four ported blocks + tail; the dtor's crash on
   a bad version kept (or turned into a logged failure, `fixes.md`).
4. Config file I/O: `read_string` (FIX (A): HANDLE → FILE*), ItemList::Load
   with the five kinds, Engine/Sinks load, Config::Load, the packed form
   0x425650 and CRC 0x4255b0, Validate's five checks (weight needs 0x425300
   and the weapon/equipment weights, its own piece), 0x4242a0, then
   LoadSaved. Check: the round trip above (original Save → port Load and
   original Load, compare), tamper cases per field, a kind-5 item, a
   truncated file, `version` 14 and 8 bytes long, a config with mech index
   of another mech, and jump-jet counts at 2/3/4 around 40 and 65 t.
5. The writer (SaveAll, Save, archive writer) with the mech lab; needed for
   the files to exist at all.

## Discoveries

Listed in `discoveries.md` (Mechs: the definition file and saved loadouts).

## Unverified

- What reads +0x10/+0x14 (engine min/max), +0x14c/+0x150, +0x15c, and the
  meanings of the readers in the scan above.
- Who sets Config +0x1c outside Load (the mech lab's "save as"), what +0x1d
  is, and kind-4 items (vt 0x5c3700, one byte at +0x18).
- Validate's weight in the emulator lacks run-time tables (the Daishi primary
  weighs 56.5 t there), so check 3 was only exercised loosely; the packed
  form 0x425650 is not mapped field by field.
- Whether the 7-bit engine index in the packed form lets higher bits of +0x24
  change without failing the CRC.
- The second loader near 0x4260f0 and the Validate call at 0x41445e.
- Behaviour with a real saved file from the PC (none in this install).

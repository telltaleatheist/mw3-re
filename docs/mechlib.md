# Mech model library (`mechlib.zbd`), draft

Mech3.exe 1.2, 2026-10-19. Research draft: nothing ported yet. `mechlib.zbd` is
a zArchive (`zreader.md`) of 60 entries: `version` (u32 27), `format` (u32 1),
57 `.flt` model trees (`mech_<name>.flt`, `mech_<name>_1.flt`,
`mech_<name>_hud.flt` (no `_hud` for elemental), `lab.flt`) and, last,
`materials`. The reading code sits between `zclass\object3d.c` and
`zclass\switch.c` (0x4fc470..0x4fd1c0, file name unknown; the debug strings
`read node: %s type=%s` and `read model: ...` suggest a GameZ model-library
file, likely `zclass\...`). The MW3 side that names the files is in
`mech3\` between `mech_waynet.cpp` and `mission.cpp` (0x484270..0x4847e0).

A walk of the file that follows the exe's reads below (scratch script, not in
the repo) reads every one of the 57 `.flt` entries and `materials` and ends
exactly at each entry's end: 770 models, 10,589 polygons, 442 materials (434
textured). The layout agrees with mech3ax (`crates/gamez/src/mechlib/mw`,
`nodes/node/mw`, `nodes/object3d`, `model/mw`, `materials/read_single.rs`).

## The library object

One object, `operator new(0x40)`, pointer at 0x609844. Constructor 0x4fc5d0
(thiscall):

| Offset | Type | What |
|---|---|---|
| +0x00 | zutl_zar `Archive` (0x18) | `init` 0x5764e0 |
| +0x18 | u8 | "materials changed": close writes them back. Only the constructor writes it (0), so the writer never runs |
| +0x1c | u8 | allocator byte (MSVC `std::vector`) |
| +0x20/+0x24/+0x28 | `Material*` begin/end/capacity | `std::vector<Material*>`, the shared material list |
| +0x2c | u8 | allocator byte (`std::list`) |
| +0x30 | list head | `std::list<Node*>` sentinel, `new(0xc)` linked to itself: the loaded trees (cache) |
| +0x34 | int | list size |
| +0x38 | int | 0, not seen used |
| +0x3c | int | nesting depth while reading a tree (only for the debug log) |

`Material` (0xc bytes, `new(0xc)`, constructor 0x4fc470): +0 pointer to the raw
0x28-byte file material (malloc) or, once resolved, the world material; +4 u8
"resolved"; +8 texture name (malloc, or 0).

## Opening (mission load)

- 0x48faf0 (mission.cpp) calls 0x484270 at 0x48fb5f (`cl = 1`, unused).
- 0x484270: if 0x609844 is null, `new(0x40)` + 0x4fc5d0. Then
  0x4fc6d0(`"zbd\\mechlib.zbd"` 0x5e6eb4). The rest of 0x484270 fills two
  tables (0x608fc0.., 0x609400..) with 0x56f6f0 (zsnd_parm area), unrelated to
  the models (likely sound parameters).
- **0x4fc6d0 `open(path)`** (thiscall, ret 4): `ok = (zar open 0x576520 == 1)`;
  then, whatever `ok` is, reads entry `version` (0x5e24f4) and entry `format`
  (0x5eb1f8) with zar `read` 0x576b30 (buffer size 4 each, zeroed first). Checks
  `version == [0x5eb1f0]` (27) and `format == [0x5eb1f4]` (1); on mismatch it
  closes the archive (0x576650) and returns false. If `ok`, reads the materials
  (0x4fc9e0, result ignored). Returns `ok`.
- **0x4fc9e0 `read_materials`**: find `materials` (0x5eb200), seek (0x576ae0,
  no length); a missing entry (handle -1) returns 0xffffff00 (al = 0). Reads
  u32 `count` with `ReadFile`, reserves the vector to `count`, then `count`
  times: `new(0xc)`, 0x4fc470(file), push_back (MSVC vector insert code, ported
  as `push_back`).
- **0x4fc470** clears the three fields and calls **0x4fc570 `Material::read(file)`**:
  `malloc(0x28)`, `ReadFile` 0x28 bytes into it, resolved = 0; if raw +0x10
  (texture pointer in the file) is non-zero, sets it to 0 and reads the
  texture name with zar `read_string` 0x576460 (u32 length + bytes, into a
  zeroed `malloc(len + 1)`) into +8. Always zeroes raw +0x24 (cycle pointer).
  Returns 1. No checks.

Material, 0x28 bytes (mech3ax `MaterialC`): u8 alpha, u8 flags (0x10 always
set; 0x01 textured), u16 rgb, f32 r,g,b, u32 texture (non-zero = a name
follows), f32 x3 (0, 0.5, 0.5), u32 soil, u32 cycle pointer. In the file
the flags are only 0x10 and 0x11.

## Loading one model

Callers (MW3): 0x4308a0 (mech_data.cpp, both LODs: `FUN_00484760` twice, the
loop variable is the variant), 0x44fd20 (one mech, then 0x4f9af0(1), likely a
copy of the tree), 0x430ad0 (HUD model).

- **0x484760 `load_mech_model(mech index ecx, variant edx)`**: saves the
  current texture group (0x518f60 -> 0x6384d4), sets it to 3 (0x518f50), builds
  `mech_%s.flt` (variant 0) or `mech_%s_%d.flt` into 0x609440 with the name from
  the 19-entry mech table at 0x5e6078 (0x3c bytes each, name first; 0x484650
  returns the entry, index checked 0..18), calls 0x484740; if the result is
  non-null calls 0x519080(3) (walks the texture table 0x610484 and calls
  0x518f70(1) on group-3 textures, likely loads them); restores the group.
- **0x4847e0 `load_hud_model(mech index)`**: same with `mech_%s_hud.flt` into
  0x609000.
- **0x484740**: `mechlib ? 0x4fc8f0(name, 0, 0) : 0`.
- **0x4fc8f0 `Mechlib::load(name, unused, unused)`** (ret 0xc):
  1. 0x4fc650 looks in the list (+0x30) for a tree whose **root node name**
     equals `name` (inline `strcmp`). A hit returns that same tree; nothing more
     is done.
  2. Otherwise: zar find (0x576a80) + seek (0x576ae0, handle or -1), then
     0x4fcdc0(handle, parent 0) reads the tree, 0x4fcd40(root) resolves the
     materials, and if 0x553450 (flag 0x72be74, set by zvid_init.c at 0x579e79
     in one video mode) is set, 0x4fc8b0(root) post-processes every model.
  3. A non-null new root is appended to the list (`new(0xc)`, size +1).
  Returns the root (or 0).

### 0x4fcdc0 `read_tree(handle, parent)` (ret 8)

Handle -1 returns 0. Depth +1; node = 0x4fce50(handle, parent); model =
0x4fcf20(handle, node); `n = node+0x5c` (child count from the file), then
`node+0x5c = node+0x60 = 0`, and `n` times `read_tree(handle, node)` (each
child attaches itself to `node`, rebuilding +0x5c/+0x60). Depth -1. Returns
the node. The file is depth-first: node, its object3d, its model, then its
children.

### 0x4fce50 `read_node(handle, parent)` (ret 8)

1. `ReadFile` 0xd0 bytes into a stack copy (the `Node`, `structs.md`; mech3ax
   `NodeMwC`).
2. Debug log (0x5ac5c0 is an empty stub) with the type name from 0x4fce40
   (5 -> `gw_object3d`, else `unknown`).
3. If the file node's type (+0x34) is 5: 0x4fb740 creates an object3d node
   (node pool 0x4eacd0, 0xd4-byte slots, defaults name `Default_node_name`,
   flags 0x01080000 | 0x1c, +0x44 = 1, +0x4c..+0x53 = -1, +0x30 = -1; type 5;
   `calloc(1, 0x90)` at +0x38; object3d init 0x4fb650), then `ReadFile` 0x90
   bytes straight into the object3d data (mech3ax `Object3dC`), overwriting the
   init values.
4. Copies the 0xd0 file bytes over the node, keeping from the new node: +0x38
   (data pointer), the low two bits of +0x24 (flags; set by 0x4fb650) and +0xcc.
   Zeroes +0x54/+0x58 (parent count and array).
5. If `parent` is non-null, 0x4eb9b0(parent ecx, node edx): the zClass
   add-child switch; type 5 goes to 0x4fb7a0 -> 0x4eba90, which `realloc`s
   the parent's children (+0x5c/+0x60) and the child's parents (+0x54/+0x58),
   sets parent +0x2c |= 2 and parent flags |= 3.

No check of anything in the file node. The fields that stay as the file has
them include +0x3c (in mechlib a non-zero pointer from the authoring tool, used
only as "has a model"), +0x5c (child count) and the bounding boxes.

### 0x4fcf20 `read_model(handle, node)` (ret 8)

Only when `node+0x3c != 0`:

1. `ReadFile` 0x5c bytes (mech3ax `ModelMwC`), take a model from the pool
   0x538fe0 (`gmod_const.c`: 0x60-byte slots at 0x7402e4, free index 0x7402ec,
   next at +0x5c; zeroes 0x5c bytes and sets +8 = (+8 & ~0x10) | 3; an empty
   pool shows `GameZ model resource failure` and calls `ExitProcess(0)`),
   copy the 0x5c bytes over it (the slot's +0x5c link stays), `node+0x3c =
   model`.
2. In order, each only when its count is > 0, `malloc` + `ReadFile`:
   vertices `+0x14 x 12` -> +0x38; normals `+0x18 x 12` -> +0x3c;
   morphs `+0x1c x 12` -> +0x44; polygons `+0x10 x 0x24` -> +0x34.
3. For each polygon (0x24 bytes, mech3ax `PolygonMwC`), `k = flags & 0xff`:
   - `k != 0`: vertex indices `k x 4` -> +8;
   - flags & 0x200 and +0xc non-zero: normal indices `k x 4` -> +0xc;
   - +0x10 non-zero: UVs `k x 8` -> +0x10;
   - +0x14 non-zero: vertex colours `k x 12` -> +0x14;
   - always `malloc(k x 4)` -> +0x18, not read (uninitialised scratch).
4. Debug log `read model: %d verts, %d polys [%d] bytes`; returns the byte
   count (0x5c header + arrays). The caller ignores it.

Light count (+0x20) and the lights pointer (+0x40) are never touched. No
checks: a `ReadFile` failure is not seen.

### 0x4fcd40 `resolve_materials(node)` (thiscall on the library, ret 4)

Recursive over children. For a node with a model, for each polygon replaces
the material index at polygon +0x1c with `materials[index]->resolve()`
(0x4fc520), with no bounds check.

**0x4fc520 `Material::resolve()`**: if not resolved: when it has a name,
0x518c60(name) finds the texture (it cuts the name at the last `.` in place,
looks up loaded textures 0x518220, else loads from the libraries 0x518cc0 and
marks it state 2 and group = current group (3 for mechs), else falls back to
0x518210) into raw +0x10; then 0x536ad0 (`gmod_matl.c`) registers the raw
material in the world material table (finds an equal one or allocates a
0x2c-byte entry). A non-null result: resolved = 1, `free(raw)`, keep the world
material. Returns the world material, or 0 while unresolved. So all mechs share
442 resolved materials, and the texture lookup runs once per material.

### 0x4fc8b0 `post_process(node)`

Recursive; for each model calls 0x552540(model ecx, 0x7fffffff edx, 4). That
function (between `gmod_puff.cpp` and `zreader.cpp`, about 300 lines) walks
polygons with normals (0x200) and either tri-strip (0x400) or at most 4 vertices
and builds edge lists, likely an edge/adjacency pass for one renderer. Not
traced; only on when 0x72be74 is set.

## Closing

0x4844b0, called from shutdown (0x4808be) and mission unload (0x4904a9):
0x4fc790 then 0x484540 + `delete`, pointer cleared; then deletes the
per-mech objects at table +0x38 (0x46b9b0, likely `MechDfn`) and clears the
two tables.

- **0x4fc790 `close`**: if +0x18, writes materials (0x4fc990: zar writer
  0x576890 with `materials` and callback 0x4fc9d0 -> 0x4fcca0 -> 0x4fc4d0 per
  material); clears the list's node pointers (the trees are not freed: the
  world owns them); deletes each `Material` (0x4fc490: frees raw unless
  resolved, frees the name); empties the list and the vector; closes the
  archive (0x576650).
- **0x484540**: frees the list sentinel and vector storage, zar `destroy`
  0x576500.

## Ported

`src/gamez/zclass/cls_mechlib.*`: `read_material` (0x4fc570) and `read_model`
(0x4fcf20). `tests/differential/test_mechlib.py` walks the file in the exe's
order, runs both originals on every material and model, and compares
everything read and the bytes consumed: all 442 materials and 770 models are
identical. FIX (A): models come from `new` rather than the 0x60-byte pool.
The data never has a normal-index pointer without flag 0x200, so the test
cannot see whether that condition is ported right; it is ported as the
disassembly reads.

## Exe and mech3ax compared

Same bytes in the same order; on the shipped file both agree. Where they
differ:

- mech3ax reads lights when `light_count > 0`; the exe never does. Every
  mechlib model has 0 lights, so it does not matter for the shipped file.
- Texture name: the exe reads one when material +0x10 is non-zero; mech3ax
  when flag 0x01 is set. They agree on all 442 materials.
- Normal indices: exe needs flag 0x200 and a non-zero pointer; mech3ax asserts
  they agree and reads by the pointer. Vertex colours: exe reads by pointer,
  mech3ax always (asserts non-null). No polygon in the file differs.
- mech3ax checks many fields (node type 5, parent counts, field196 == 160,
  model and polygon pointers); the exe checks nothing but `version` and
  `format`. Every node in the file is type 5, field +0xc4 is always 160, +0xc0,
  +0xc8, +0xcc are 0.
- mech3ax fakes a material count of 4096 when checking polygon indices; the
  exe does no check. In the file indices run 0..441 of 442.

## Discoveries

- `lab.flt` is never loaded: every name the exe builds starts with `mech_`
  and the mech table (0x5e6078) has no `lab`. Likely a leftover.
- The cache (0x4fc650) is keyed by the root node's name, not the entry name;
  it works because every root node is named after its entry (all 57 checked).
  A hit returns the same tree, not a copy.
- 0x4fc6d0 reads `version` and `format` even when the open failed (both stay
  0, so the check fails and it closes again).
- A missing `materials` entry is ignored (0x4fc9e0's result is dropped); later
  `materials[index]` reads past an empty vector.
- A node whose type is not 5, or a full node pool, makes 0x4fce50 write
  through a null pointer (`[0 + 0x38]`).
- Polygon +0x18 gets an uninitialised `malloc(k x 4)` for every polygon.
- The model's lights pointer (+0x40) keeps whatever the file had (0 here).
- Material +0x10 changes meaning: in the file a pointer (presence), then 0,
  then the texture handle. The unused writer (0x4fc4d0) writes raw +0 and
  would write a world material after resolution.
- 0x4fc8f0's second and third arguments are unused (both callers pass 0, 0).
- 0x518c60 writes `\0` over the texture name's last `.` while it looks the
  texture up, then puts it back.

## Porting order (leaves first)

1. `Material`: 0x4fc470 / 0x4fc570 (read), 0x4fc490 (free). Leaf apart from
   zar `read_string`. Differential test: Unicorn with `ReadFile`/`malloc`
   stubbed, over all 442 materials.
2. 0x4fc9e0 `read_materials`, 0x4fc5d0 / 0x484540 (library ctor/dtor),
   0x4fc6d0 `open`. Use the ported zArchive.
3. 0x4fcf20 `read_model` (needs a model pool in place of 0x538fe0). Leaf
   otherwise; test on all 770 models.
4. 0x4fce50 `read_node` + 0x4fcdc0 `read_tree`. Need the zClass node pool
   (0x4eacd0), object3d create/init (0x4fb740, 0x4fb650) and add-child
   (0x4eb9b0 -> 0x4fb7a0 -> 0x4eba90), which belong to the zClass port; stub
   them for the first test and compare node bytes except the runtime fields
   (+0x24 low bits, +0x38, +0x54..+0x60, +0xcc).
5. 0x4fc650 (cache), 0x4fc8f0 (load), 0x4fc790 (close).
6. 0x4fcd40 / 0x4fc520 material resolution: needs the texture lookup
   (0x518c60 and the zimg_library port) and the world material table
   (0x536ad0, `gmod_matl.c`), so it waits for those.
7. MW3 side: 0x484270, 0x484740, 0x484760, 0x4847e0, 0x4844b0, and the mech
   table at 0x5e6078.
8. Last or never: 0x4fc8b0 / 0x552540 (only in one video mode) and the writer
   (0x4fc990, 0x4fcca0, 0x4fc4d0), which is dead.

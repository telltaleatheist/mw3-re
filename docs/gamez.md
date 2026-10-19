# Mission world (`gamez.zbd`), Mech3.exe 1.2 (researched by a subagent, spot-checked: signature 0x02971222 and version 27 at 0x4ff6dc/0x4ff6f0)

Mech3.exe 1.2, 2026-10-19. Research draft; nothing ported. `zbd/<db>/gamez.zbd`
(c1, c2, c3, c4, c4b, t1) is not a zArchive. It is a direct dump of four
in-memory GameZ pools: the global texture table, the material table, the
model pool and the node pool. The reader in `zclass\cls_zbd.c`
(0x4fe210..0x4ff8f0) restores each pool in place with `fopen`/`fread`/`fseek`
(CRT, not `ReadFile`). Each pool's reader lives in its own module
(`zimg_texture.cpp`, `gmod_matl.c`, `gmod_const.c`, `cls_zbd.c`).

A scratch walk (not in the repo) that follows the exe's reads below covers
all six shipped files, header to last byte, and ends exactly at the file
size. Every section starts at the header offset the exe seeks to.

| File | Textures | Materials (used/array) | Models (used/array) | Nodes (typed/free/array) |
|---|---|---|---|---|
| c1 | 553 | 497/1000 | 2737/20000 | 8167/11833/20000 |
| c2 | 619 | 587/1000 | 2419/20000 | 6061/13939/20000 |
| c3 | 555 | 494/1000 | 3354/20000 | 7718/12282/20000 |
| c4 | 499 | 455/1000 | 2945/20000 | 7359/12641/20000 |
| c4b | 403 | 354/1000 | 1636/20000 | 2894/17106/20000 |
| t1 | 344 | 276/1000 | 850/6000 | 1869/14131/16000 |

## Where it is opened

The mission load code never names the file. A script command does:

- 0x48faf0 (mission.cpp) runs the interpreter (0x595990, `zinterp`) twice:
  first `support\<mission>size.gw` (0x5e741c; it sets
  `SetGameZNodeArraySize` / `SetModel3DArraySize`), then, after 0x4f8f70
  (zClass reset) and 0x526800 (`gmod_init.c`: 0x536830 resets the material
  table, then the model pool is `malloc`ed and zeroed, size 1750 by default,
  its free list linked through +0x5c), the main script (`%s.gs`, 0x5e7404).
  A script that fails logs `Failed to Interpret: %s -- Mission Load Aborted`.
- The script holds `GameZReadZBDFile` with the path
  `%RTIME%\%DB%\gamez.zbd` (strings in `interp.zbd`; see
  `docs/drafts/interp.md`). The handler sits in the big command switch in
  `zinterp_parse.cpp` (0x591300): at 0x5923a4 it compares the command name
  (0x5e16d8, 16 chars, IAT 0x5bb654), gets the string argument (0x595e70) and calls
  **0x4ff520 `gClsReadZBD(path ecx)`** at 0x5923c3.
- `GameZWriteZBDFile` (name 0x5f3a38 compared at 0x5923fa, writer 0x4fe8e0
  called at 0x592423) is the matching save. The script has it only inside
  `ifdef COMPILE ... endif` (`GameZWriteZBDFile %GAMEZ%`): the authoring
  build's path.

`zerr_report` (0x49bfd0) is an empty stub in this build, so every error
below is silent. The only result anyone sees is the return value.

## 0x4ff520 `gClsReadZBD(path)` (ecx, returns 0 or -1)

1. An empty path returns -1. A path shorter than 0x2f characters is copied to
   0x60bf50 (the remembered file, used again by 0x4ff730). A longer one only
   logs `zbd_filename length %d exceeds storage string size %d.` and goes on
   with the old 0x60bf50 left in place.
2. **0x4ff6a0 `open_header(path ecx, header edx)`**: `fopen(path, "rb")`
   (IAT 0x5bb628), `fread` 0x24 bytes. Checks: read OK (else `Error reading
   GameZ header data.`), signature `0x02971222` (else `...; incompatible file
   type`), version 27 (`0x1b`; else `...; incompatible file version`). On
   failure it closes the file and returns 0. Nothing else in the header is
   checked.
3. For each section: `fseek(file, header offset, SEEK_SET)`, then the
   reader, and on a negative result one error message, `fclose`, -1:

| Order | Seek to | Reader | Arguments | Error |
|---|---|---|---|---|
| 1 | +0x0c textures_offset | 0x518160 | ecx = texture_count (+0x08), edx = file | `Error reading GameZ texture data.` (0x5eb84c) |
| 2 | +0x10 materials_offset | 0x5363d0 | ecx = file | `Error reading GameZ material data.` |
| 3 | +0x14 models_offset | 0x538e30 | ecx = file | `Error reading GameZ model3d data.` |
| 4 | +0x20 nodes_offset | 0x4ff200 | ecx = node_array_size (+0x18), edx = file | `Error reading GameZ node data.` |

   Before step 4, header +0x1c (node_last_free) goes into 0x5ea834, the
   node pool's free-list head.
4. `fclose`, return 0.

### Header (0x24 bytes)

| Offset | Field | Shipped | Exe use |
|---|---|---|---|
| +0x00 | signature | 0x02971222 | checked |
| +0x04 | version | 27 | checked |
| +0x08 | texture_count | 344..619 | passed to 0x518160 (max 0x1000) |
| +0x0c | textures_offset | 0x24 | fseek |
| +0x10 | materials_offset | | fseek |
| +0x14 | models_offset | | fseek; also used by 0x4ff7a0 |
| +0x18 | node_array_size | 20000 (t1 16000) | node count to read; also bound in 0x4ff7a0 |
| +0x1c | node_last_free | | -> 0x5ea834 (node free-list head) |
| +0x20 | nodes_offset | | fseek; also used by 0x4ff7a0 |

## 1. Texture directory: 0x518160 (`zimg_texture.cpp`)

Ported: `zimg::read_texture_directory` (`src/gamez/zimage/zimg_texture.*`),
identical on all six worlds (`tests/differential/test_gamez.py`).

`(count ecx, file edx)`, returns the count or -1:

- `count == 0`: returns 0 at once. It does **not** clear the table count
  0x610480 (see Discoveries).
- `count > 0x1000`: `Too many textures for texture array size.` (level
  0x100), -1.
- One `fread(0x610484, count * 0x28, 1)` straight into the global texture
  table, the same table the texture code searches by name (0x518220) and
  that mechlib textures are later added to. A short read gives `Error
  reading GameZ Texture directory data.` (0x5ecc84), -1.
- `0x610480 = count`; then for each entry, +0x20 (group) = the current
  texture group (0x6384d4, set by 0x518f50 at mission load), and +0x24 = 
  0x518080(+0x24), index to pointer (`-1` -> 0, else `0x610484 + i*0x28`,
  no bounds check).

Entry (0x28), in the file and in memory:

| Offset | Field | File | After load |
|---|---|---|---|
| +0x00 | image pointer | 0 | 0, filled when the texture is loaded by name from `rimage.zbd` (`textures.md`) |
| +0x04 | surface pointer | 0 | 0 |
| +0x08 | name[20] | e.g. `sfoam03` | the key 0x518220 compares (only entries with +0x1c != 0) |
| +0x1c | state | 2 everywhere | kept; non-zero = slot in use |
| +0x20 | group | 0 | current group (0x6384d4) |
| +0x24 | mip index | -1 everywhere | entry pointer or 0 |

Writer: 0x5180a0 copies the table, turns +0x24 back into indices
(0x518050) and `fwrite`s it (`Error writing texture directory.`).

## 2. Materials: 0x5363d0 (`gmod_matl.c`)

Ported: `zmod::read_materials` (`src/gamez/zmodel/gmod_matl.*`) with the
colour packing `zvid::set_pixel_format` / `pack_rgb` (`src/gamez/zvideo/
zvid_format.*`); identical on all six worlds with 5-6-5 and 5-5-5 displays,
cycles included. The partial-last-block free list is ported but no file
exercises it.

The world material table is a block array: block size 1000 (0x5edd50),
block pointers at 0x63fbc8, block count 0x63fbc0, entries 0x2c bytes. The
mechlib `Material::resolve` (0x536ad0) allocates into this same table later.
Globals:

| Address | Meaning |
|---|---|
| 0x63fbc4 | array size (capacity) |
| 0x63fbcc | used count |
| 0x5edd54 | free-list head |
| 0x5edd58 | used-list head |
| 0x5edd5c | set to -1 at the start (also in reset 0x536830); likely a "current material" cache |

Read (`file ecx`), returns the capacity, 0 or -1:

1. Four `fread`s of 4 bytes: array size -> 0x63fbc4, count -> 0x63fbcc,
   free head -> 0x5edd54, used head -> 0x5edd58 (each failure: `Error reading
   GameZ Material buffer data.`, -1). mech3ax calls these `array_size`,
   `count`, `index_free`, `index_last`.
2. Array size 0: return 0.
3. Blocks: for block b while `b*1000 < array size`: if `b` equals the block
   count, `realloc` the pointer array, `malloc(1000*0x2c)` and zero it
   (existing blocks, e.g. from the reset or an earlier mission, are reused and
   overwritten). `fread` `min(1000, rest) * 0x2c` bytes into the block.
4. If the array size is not a multiple of 1000, the slots left in the last
   block are linked onto the front of the free list (flag 0x20, links
   below). Never runs on the shipped files (all are 1000).
5. Capacity 0x63fbc4 = rounded-up block total.
6. Walk the **used list** from 0x5edd58 through +0x2a. For each material:
   - flags (+1) & 0x01 (textured): +0x10 = 0x518080(+0x10), texture index to
     table pointer (no bounds check);
   - otherwise: +0x02 = 0x5793c0(&+0x04), the r,g,b floats packed into the
     display's 16-bit format (masks and shifts 0x802738..0x802748 from zvid).
     The file has 0 there for all coloured materials;
   - flags & 0x04 (cycled): `malloc(0x1c)` -> +0x24, `fread` 0x1c; then
     `malloc(count*4)` -> cycle +0x18, `fread` `cycle+0x10` u32 texture
     indices, each turned into a table pointer (0x518080). Errors: `Error
     reading GameZ Material cycle texture data.` (twice, lines 0x3c0/0x3d1).
7. Return the capacity.

Material (0x2c): the 0x28-byte `MaterialC` from mechlib (`mechlib.md`)
plus two i16 links.

| Offset | Field | Notes |
|---|---|---|
| +0x00 | u8 alpha | 0xff textured |
| +0x01 | u8 flags | 0x01 textured, 0x02 unknown (c2 only, flags 0x13), 0x04 cycled, 0x10 referenced (set by 0x536070 / 0x535ff0 when a polygon or caller takes it; always set in the file), 0x20 free |
| +0x02 | u16 rgb | 0x7fff textured; recomputed for coloured |
| +0x04 | f32 r, g, b | 255.0 for textured |
| +0x10 | texture | index in the file, pointer after load |
| +0x14 | f32 x3 | 0, 0.5, 0.5 |
| +0x20 | u32 soil | 0..13 |
| +0x24 | cycle pointer | stale authoring pointer in the file; the exe reads by flag 0x04 and overwrites it |
| +0x28 | i16 prev | |
| +0x2a | i16 next | the list link the reader follows |

The reset (0x536830) builds the free list as `prev = i-1`, `next = i+1` from
0, with flag 0x20; allocation pushes onto the front of the used list. So in
the file the used list runs from the last material down to 0
(`used head = count-1`, `material[i].next = i-1`), and the free list from
`count` upwards.

Cycle (0x1c, mech3ax `MaterialCycleC`): +0 looping, +4 current frame
(garbage in the file: 0x78657265, "erex"), +8 f32 current index, +0xc f32
speed, +0x10 texture count, +0x14 index, +0x18 texture array pointer.

**Cycles are stored in used-list order, highest material index first.**
Checked against the stale pointers: read that way, each cycle's texture-array
pointer is its material's cycle pointer minus 0x50 for every cycle in c1;
read in ascending order (as mech3ax does) they do not match. See Discoveries.

Writer: 0x5360c0 (header, whole blocks with texture pointers turned back
into indices, then cycles in the same used-list order).

## 3. Models: 0x538e30 (`gmod_const.c`)

Ported: `zmod::read_models` / `read_model_data` (`src/gamez/zmodel/
gmod_const.*`) with `zvid::pack_bytes` (0x579370); identical on all 13,941
models of the six worlds, lights included.

The model pool: 0x7402e4 (array of 0x60-byte slots), size 0x7402e0, used
count 0x7402e8, free head 0x7402ec, free link at slot +0x5c. It is the same
pool mechlib models are taken from (0x538fe0).

Read (`file ecx`), returns the size, 0 or -1:

1. `fread` 4 x 3: size -> 0x7402e0 (the old size is kept for the realloc
   test), count -> 0x7402e8, free head -> 0x7402ec.
2. Size 0: return 0.
3. No pool: `malloc(size*0x60)`; pool smaller than the file: `realloc`.
   Then one `fread` of `size * 0x60` bytes: **every slot, used and free, as
   it was in memory**. Error: `Error reading GameZ Model3D buffer data.`
   (logged twice on the early failures).
4. For each of the `size` slots: if parent count (+0x0c) < 1 and the first
   0x5c bytes are not all zero, log `Reading GameZ Model3D buffer data for
   model that has no parents.` (no effect). Then **0x538a70(file ecx, model
   edx)** reads its arrays. Free slots have zero counts, so it reads nothing
   for them.

Slot +0x5c is the free link in free slots (`i+1`, last -1) and the **file
offset of the model's data** in used ones. 0x538a70 does not use it (the data
follows in slot order), but the single-model re-read 0x538640 does (below).

0x538a70, in order, each only when its count > 0 (`malloc` + `fread`):

1. vertices `+0x14 x 12` -> +0x38;
2. normals `+0x18 x 12` -> +0x3c;
3. morphs `+0x1c x 12` -> +0x44;
4. lights `+0x20 x 0x4c` -> +0x40; for each light, +0x28 (u16) = 0x579370
   of its colour floats at +0x1c/+0x20/+0x24 (each `ftol(x - c)`, c = -0.5
   at 0x5ce178, so rounded to nearest), and when light +0x0c > 0,
   `+0x0c x 12` vertices -> light +0x2c (`Error reading GameZ Model3D point light data.`);
5. polygons `+0x10 x 0x24` -> +0x34. Every polygon's material index (+0x1c)
   becomes a material pointer (0x536070: index < 0 -> 0, no upper bound;
   sets the material's flag 0x10). Then for each polygon, `k = flags & 0xff`:
   - `k != 0`: vertex indices `k x 4` -> +0x08, and if flags & 0x200 normal
     indices `k x 4` -> +0x0c;
   - if the **polygon's material** is textured (material +1 & 1): UVs
     `k x 8` -> +0x10;
   - vertex colours `k x 12` -> +0x14, always;
   - `malloc(k x 4)` -> +0x18, not read (scratch).
   Returns 0 or -1, which 0x538e30 **ignores**.

The polygon conditions differ from mechlib's reader (`mechlib.md`: there,
normals need the pointer too, UVs and colours follow the pointers). On the
shipped files they agree: the normal pointer is non-zero exactly when 0x200
is set, the UV pointer non-zero exactly when the material is textured, the
colour pointer always non-zero (all 146,512 polygons checked).

The model header (0x5c) is mech3ax `ModelMwC`, as in `mechlib.md`.

## 4. Nodes: 0x4ff200 (`cls_zbd.c`)

Ported: `zcls::read_nodes` / `read_node` / `read_node_list`
(`src/gamez/zclass/cls_zbd.*`) on the node pool (`cls_node`), the class lists
(`list`), the camera, display and world setters (`camera`, `display`,
`cls_world`) and the fog they push (`zmodel/gmod_fog`, `zrender/zrndr_fog`,
down to the software renderer's fog ramp 0x56c170). Identical on all six
worlds: every slot (pointers as indices), every class's data, all 16 lists
with each item's place in the item pool, the fog globals, the clear colour
and the error reports; 34,068 live nodes. The worlds hold only classes 0-6
and 9 (one camera, world, window and light each; t1 two displays), so the
sound-node path and the unknown-class error are ported but never run. Not
ported: 0x4f18a0 (the world's pending area entries; always none when read,
a hook for now) and the Direct3D device's fog call (a hook, FIX (A)).

Port differences: a free (class 0) node's data pointer is null in the port;
the exe leaves the file's stale value there and never reads it. 0x5380c0 has
no bound check; the port returns null past the model pool.

The node pool: 0xd4-byte slots (the 0xd0 `Node` of `structs.md` plus a u32
at +0xd0) in blocks of 2500 (0x5ea830). Block pointers 0x60bf38, block count
0x60bf30, capacity 0x60bf34, live count 0x60bf3c, free head 0x5ea834. Slot
+0xd0: low 24 bits = free-list link (0xffffff ends it), bit 24 (0x1000000) =
live. 0x4fe1d0 turns an index into a slot pointer (`< 0` -> 0), 0x4fe100 a
pointer back into an index, 0x4f8ec0 frees an index (puts it at the free head;
it does not clear the slot).

Read `(array size ecx, file edx)`, returns the capacity, 0 or -1:

1. Size 0: return 0.
2. Blocks as for materials: allocate missing ones (`malloc(2500*0xd4)`,
   zeroed), `fread` `min(2500, rest) * 0xd4` per block. **The whole array is
   one read per block, before any node data.** The only error that returns
   -1: `Error reading GameZ Node buffer.` (line 0x4e8).
3. Slots past the array size in the last block (t1: 16000..17499) are zeroed
   and freed (0x4f8ec0) one by one. Whole blocks left over from an earlier,
   larger pool are zeroed and freed too.
4. Live count = 0. For each slot `i < size`, in index order:
   - +0x3c = 0x5380c0(+0x3c): model index to pool pointer (`< 0` -> 0,
     else `0x7402e4 + i*0x60`, no bounds check);
   - +0x48 (action callback) = 0;
   - **0x4feaf0(node ecx, file edx)** reads the node's data; result >= 1
     sets bit 24 of +0xd0 and counts the node live, otherwise bit 24 is
     cleared. A -1 from 0x4feaf0 is treated as "not live"; it is not an
     error here.
5. Fix-up over the world list (class list 13, head 0x60bed0, `next` at +8):
   world +0xa8[i] = (+0xa4[i])->data (+0x38), for its +0xa0 lights; the
   same for +0xac sounds (+0xb0 nodes -> +0xb4 data). Done after all nodes
   because the light nodes come after the world.
6. Return the capacity.

### 0x4feaf0 `gClsReadNode(node, file)`

Switch on the class (+0x34). Each `malloc`s the class data, `fread`s it into
+0x38, fixes it up, and adds the node to class lists with 0x4f8080(list ecx,
node edx). List 6 gets every node with data; lists are head/tail pairs in the
tables at 0x5ea6d0 / 0x5ea710. Result 1 = has data.

| Class | Name | Data | After the read | Lists |
|---|---|---|---|---|
| 0 | empty / free | none | | none; result 0 |
| 1 | camera | 0x1e8 | +0x00..+0x0c: four node indices -> pointers (0x4fe1d0); clip 0x4f3a70(+0xb0, +0xb4); field of view 0x4f3b90(+0xd8, +0xdc) (half angles, 1/tan) | 6, 8 |
| 2 | world | 0xbc | see below | 6, 13 |
| 3 | window | 0xf8 | | 6, 14 |
| 4 | display | 0x1c | clear colour 0x4fa680(+0x10, +0x14, +0x18) (packs it with 0x5793c0) | 6, 15 |
| 5 | object3d | 0x90 | | 6 |
| 6 | lod | 0x50 | | 6 |
| 9 | light | 0xd0 | +0xc8 node indices -> +0xcc (0x4fea80) | 6, 9 |
| 10 | sound | 0x94 | logs `Reading sound node data: Must complete software.`; +0, +4 = 0; +0x8c node indices -> +0x90 | 6, 10 |
| 7, 8, other | | | `gClsReadNode(): Unrecognized node class type` (0x400), -1 | |

Errors (`Error reading node <class> data.`, `Error reading world area
partition data.`) return -1.

World (class 2), in file order:

1. 0xbc bytes (mech3ax `WorldMwC`);
2. +0xa0 light node indices -> `malloc` +0xa4 (0x4fea80), and a second
   array +0xa8 for their data (filled in step 5 above); count < 1 zeroes both;
3. +0xac sound node indices -> +0xb0, data array +0xb4; same;
4. the partition grid: `calloc(+0x8c, 4)` -> +0x90, each row `calloc(+0x88,
   0x48)`; then for each row (z, +0x8c) and column (x, +0x88) one 0x48-byte
   partition (mech3ax `PartitionMwC`) and, when its i16 +0x3a > 0, that many
   node indices -> +0x3c (else +0x3c = 0);
5. +4, +8, +0xc = 0 (the area partition), then the setters 0x4f23e0 (fog
   type +0x10), 0x4f23f0 (fog colour +0x14..+0x1c), 0x4f2420 (+0x28/+0x2c),
   0x4f2440 (+0x20/+0x24), 0x4f2460 (+0x30): each stores the value it was
   given (the file's own) and sets a bit in world +0; 0x4f1df0 then pushes
   the fog to the renderer (0x526b80.., `zModel`) and clears +0.

Then, for every class including 0:

- parent count +0x54 > 0: `malloc` +0x58, read the indices (0x4fea80) and
  turn them into pointers, result 1; else +0x58 = 0;
- child count +0x5c > 0: the same into +0x60, result 1; else +0x60 = 0.

**0x4fea80 `read_node_list(array ecx, count edx, file)`**: `fread(count*4)`
(`Error reading GameZ Node list.`), then each index -> slot pointer with
0x4fe1d0. Indices are not bounds-checked.

### What the free nodes are

mech3ax calls the class-0 nodes with names "empty nodes" and reads their
+0xd0 as a parent index. The exe sees them otherwise: following the free
list from header +0x1c through the low 24 bits of +0xd0 visits **exactly**
every class-0 node, named or zeroed, and no typed node, ending at 0xffffff
(all six files). The named ones (c1: 1393, c3: 657, c4: 604, c2 and c4b: 4,
t1: 0) are nodes the authoring tool deleted. 0x4f8ec0 only rewrites the link,
so their names, flags and bounding boxes stay as they were. None has parents
or children, so 0x4feaf0 returns 0 for them and they stay free.

For typed nodes +0xd0 holds the file offset of the node's data (mech3ax
checks it). The exe never seeks by it: it reads the data in slot order and
only ORs in bit 24. Likely the writer stores it for tools.

## Re-reading part of the file (0x4ff730, 0x4ff7a0, 0x538640)

0x4ff730(node, recurse) re-opens the remembered file (0x60bf50) through
0x4ff6a0 and calls **0x4ff7a0**: node pointer -> index (0x4fe100), stops if
`index >= node_array_size`, seeks to `nodes_offset + index*0xd4` and reads
the original 0xd4-byte record. Then it seeks to `models_offset` and calls
**0x538640(file, model index)**: it reads the 12-byte model header again,
seeks `index*0x60` from there, reads the slot, takes a new pool slot,
**seeks to the slot's +0x5c data offset** and reads the model's arrays.
0x4eb420 / 0x4eb370 likely swap the node's model, and with `recurse` it does the
same for each child. Its caller is 0x500760 (between `zdec_crater.cpp` and
`zdec_init.cpp`, the destructible-environment client), likely restoring
destroyed buildings' original models. This is why the file keeps model data
offsets.

## Exe and mech3ax compared

Both read the same bytes in the same order. On the shipped files they agree
everywhere except the material cycles. The differences:

- **Material cycles**: mech3ax reads them in ascending material index; the
  exe reads (and writes) them in used-list order, descending. mech3ax's
  round trip is byte-identical, but it gives each cycle to the wrong
  material (in c1, material 23 gets material 444's 16-frame cycle and 444 gets
  23's 4-frame one). The stale pointers bear out the exe's order (above).
- **Cycle presence**: exe flag 0x04; mech3ax asserts flag and pointer agree.
- **"Empty" nodes** are free slots, and their +0xd0 is the free link, not a
  parent index (above). mech3ax's node count is the first all-zero name; the
  exe processes all `node_array_size` slots.
- **Offsets**: mech3ax asserts the header offsets equal its running
  position; the exe seeks to them. Model data offsets (+0x5c) and node data
  offsets (+0xd0) are checked by mech3ax; the exe uses only the model ones,
  and only in 0x538640.
- **Texture count**: mech3ax allows 0..4095, the exe 0..4096 (`> 0x1000`
  fails). mech3ax checks category 0 and the null pointers; the exe overwrites
  the category with the current group.
- **Polygons**: UVs by the material's textured flag (exe) or the UV pointer
  (mech3ax); normal indices by flag 0x200 (exe) or the pointer (mech3ax).
  Same result on every polygon.
- **Checks**: mech3ax checks most fields (classes, names, flags, pointers,
  link patterns, material and model indices). The exe checks only the
  signature, version, texture count and read lengths.
- mech3ax's `MaterialFlags` comment gives ALWAYS as 0x08; the bit is 0x10
  (`1 << 4`). In the exe it means "referenced".

## Discoveries

- The world file is the four pools dumped as they were in memory: free
  slots, stale authoring heap pointers, garbage (cycle +4 `"erex"`) and
  deleted nodes with their names included. That is why mech3ax has to check
  zero patterns and free links.
- `zerr_report` (0x49bfd0) is empty, so every error message here is dead
  text.
- Errors inside one node (short read, class 7/8) are lost: 0x4ff200 only
  clears the live bit and goes on reading from the wrong position. 0x538a70's
  -1 is ignored too. Only short reads of the headers, the texture directory,
  the material blocks, the cycles, the model pool and the node pool fail the
  load.
- A texture count of 0 returns before setting 0x610480, so a mission with no
  textures would keep the previous mission's count over the old table.
- A path of 0x2f characters or more is not stored, but the load goes on, so
  a later 0x4ff730 re-reads the previous file.
- 0x4fe100 (node pointer -> index) returns whatever is in EAX when the
  pointer is not in the pool (often 53 x 2500 = 132500), not -1. Its caller
  0x4ff7a0 rejects it only because that is past `node_array_size`.
- Indices are trusted everywhere: texture (0x518080), material (0x536070)
  and model (0x5380c0) only reject negatives; node lists (0x4fe1d0) the same.
- Free model slots go through 0x538a70 (counts 0, so nothing is read). The
  "no parents" warning is never triggered by the shipped files.
- In the model reader the old pool is overwritten in place, with no free of
  the previous mission's arrays (likely freed by mission unload; not traced).
- Sound nodes (class 10) are a stub ("Must complete software"); none
  shipped. Classes 7 and 8 are unknown to the reader.
- t1's node array (16000) is not a multiple of 2500; it is the only file
  that runs the reader's "free the rest of the last block" path. All
  material arrays are exactly one block (1000), so the material equivalent
  never runs.
- The four world fog setters store values the world struct already has;
  they only set the bits that make 0x4f1df0 send the fog to the renderer.
- Live typed nodes keep their file data offset in the low 24 bits of
  +0xd0, where free nodes keep their link. The offsets stay clear of the flag byte only because c3, the
  largest file, is 12.4 MiB, under 2^24 bytes.

## Porting order (leaves first) and checks

The pattern of `test_mechlib.py` / `test_textures.py` fits: a Python walk
(the scratch walker above, to be moved into the test) gives each section's
offset; the original runs in Unicorn with `fopen` (IAT 0x5bb628), `fread`
0x5bb594, `fseek` 0x5bb59c, `fclose` 0x5bb630, `malloc` 0x5bb664, `calloc`
0x5bb60c, `realloc` 0x5bb610, `free` 0x5bb65c stubbed over an in-memory copy
of the file. The port runs on the same bytes through `tests/probe.cpp`.
Compare the globals and every allocated buffer (pointers mapped back to
indices), plus the bytes consumed.

1. **Texture directory**, 0x518160 + 0x518080. Leaf. Set 0x6384d4 to a
   known group, compare the 0x610484 table and 0x610480. Run all six files,
   plus count 0 and 0x1001.
2. **Materials**, 0x5363d0 (+ reset 0x536830, index helpers 0x536070,
   0x535ff0). Needs a stub or port of the colour packer 0x5793c0: set the
   zvid masks/shifts at 0x802738..0x802748 for RGB565 and RGB555, as
   `test_textures.py` does for the display format. Compare the blocks, the
   four globals and each cycle (by material). Also run a synthetic file with
   an array size of 1500 for the unused free-list path.
3. **Model data**, 0x538a70, then 0x538e30. Needs the material table from
   step 2 (UVs depend on the material) and 0x579370. It is the same shape
   as the mechlib model port (`cls_mechlib.*`), so share the array readers
   but keep the two polygon rules separate. Compare every slot of all
   six pools (146,512 polygons).
4. **Node data per class**, 0x4feaf0 with 0x4fea80 / 0x4fe1d0. The class
   setters (0x4f3a70, 0x4f3b90, 0x4fa680, 0x4f23e0..0x4f2460) are small
   leaves; port them with it. Stub 0x4f8080 (class lists) and 0x4f1df0 (fog
   to the renderer) for the first test and record their calls. Compare the
   data buffers per class (cameras, worlds with partitions, lights, lods,
   object3ds).
5. **Node pool**, 0x4ff200 with 0x4f8ec0, 0x5380c0 and the world fix-up:
   compare the pool (all 0xd4 slots, pointers as indices), the free head,
   the live count and the live bits. t1 covers the partial block.
6. **Top level**, 0x4ff6a0 + 0x4ff520 on each file, plus a bad signature, a
   bad version and a truncated file.
7. Later: the re-read path (0x4ff730, 0x4ff7a0, 0x538640), once the zClass
   model swap (0x4eb420 / 0x4eb370) and the destructible-environment caller
   are mapped. The writers (0x4fe8e0, 0x5180a0, 0x5360c0, model and node
   writers) are only for the authoring tool; port last or never.

FIX (A) candidates: the pools hold 32-bit pointers in fields that are 32-bit
in the file (texture +0x24, material +0x10/+0x24, node +0x38/+0x3c/+0x58/
+0x60, the list arrays). A 64-bit port needs separate in-memory structs with
the file layout kept for reading. That is the same change the mechlib port
made with `new` instead of the pool.

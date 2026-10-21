# Craters and quicksand (zDEClient)

zDEClient ("deformable environment client", `gamez\zdeclient\zdec_crater.cpp`,
`zdec_init.cpp`, `zdec_qsand.cpp`) cuts craters and quicksand pits into the
terrain where weapons hit. It does not paint them: it removes the ground
polygons under the hole, cuts the ones on its edge with a polygon clipper
(zGeometry, `zgeo_model.cpp`, `zgeo_weiler.cpp`, `zgeo_convexify.cpp`) and
puts a new mesh in: a ring of sloped polygons and a floor. This page maps it
and splits the port into parts (section 6).

## 1. Data: declient.zrd

Each world's reader archive (`zbd\<dir>\reader.zbd`) holds a `declient.zrd`,
read by the init 0x00501ab0 at mission load (the `decals_load` hook,
mission-load.md). c1's:

```
CRATER (DEFAULT_TEXTURE ("cratrc1") DEFAULT_ANIM ("dust_fly01.flt")
        TEXTURE_PARMS (1.0 1.0) SLOPE (20.0) RADIUS (15.0) DEPTH (2.0)
        POINTS (7)
        TEXTURE_ANIM ("beach01" ("cratrc1a") "sandtrans1" ("cratrc1a") ..))
QUICK_SAND (DEFAULT_TEXTURE (10.0 "qsnd01" .. "qsnd07") TEXTURE_PARMS (1.0 1.0)
            SLOPE (40.0) RADIUS (20.0) DEPTH (5.0) ..)
```

The crater defaults (10 dwords at 0x0060bfc0, copied out by 0x004ff920):
+0x00 flags 0x100c, +0x04 `POINTS` (7), +0x08 the ground material (the
order's), +0x0c `SLOPE` (0), +0x10 `DEPTH` (4.0), +0x14 `RADIUS` (20.0), in
brackets the built-in values when a key or the file is missing.
`TEXTURE_PARMS` is never read, in either block. `TEXTURE_ANIM` pairs a ground texture with
the crater texture used on it: a table at [0x0060bfbc], [0x0060bfb8]
entries of 12 bytes (ground material, crater material, anim), entry 0 the
default. Materials come from 0x00501fa0 (the texture by name, a material
made for it, flag 8). A `TEXTURE_ANIM` value is (crater texture
[animation]); without the animation the default one. The quicksand defaults
(11 dwords at 0x0060c00c, copied by 0x005020f0): flags 0x1008, `POINTS` 7,
+0x10 `SLOPE` 40, +0x14 `DEPTH` 4.0, +0x18 `RADIUS` 20.0. Its texture is a
material (soil 3) cycling the `DEFAULT_TEXTURE` frames at the given rate
(the list's first value), looping, plus a still one showing the first
frame (soil 0). No `QUICK_SAND` record turns quicksand off ([0x0060bfe8] =
0) and leaves the two materials of the last world.

Two files the exe mishandles (no shipped one): one quicksand frame reads
the name past the list's end and drops the texture; a `QUICK_SAND` without
`DEFAULT_TEXTURE` reads the last world's freed name list, or crashes. The
port shows the one frame, or leaves quicksand untextured (fixes.md).

Ported: `gamez/zdeclient/zdec_init` (0x00501ab0, 0x00501fa0, 0x00502070,
0x00501f90, 0x004ff920, 0x005020f0) and the material calls it makes
(`gmod_matl`: 0x00536a70, 0x005375c0, 0x005375d0, 0x00537680, 0x005377a0,
0x005377e0, 0x00537be0). `test_zdec_init.py` runs the original 0x00501ab0
on every world's file, with no texture, all and half of them loaded, and
on 400 made-up files; calls, reports, defaults, the table and the whole
material table identical. Breaks caught: the loop flag, the material's
init, flag 8, the default animation, the quicksand slope, reusing a
texture's material, soil 3. The table's size rounding ((n - 1) / 2) shows
only with a key left without a value, where the exe reads past the list.

The init also keeps the world node (0x00502070: [0x0060c0c0], its area
partition [0x0060c0c4]) and, when [0x005eba1c], registers an anim entity
class (0x00500c20, 1000) that replays the craters on a restore.

## 2. Making a crater, 0x004ffaa0 / 0x004ff940

Called by zWeapon's impact (0x005826b0 -> 0x005825b0, docs/weapon-effects.md)
with an order made from the defaults, the def's `CRATER` values and the
point. [0x0060bf88] may veto it first: multiplayer sets it to 0x0046fd30,
which sends the crater to the other players instead (the host makes it).

1. The precision set to 0.005 (0x00538060; old value 0x00538050).
2. Build 0x004ffad0: a record (0x50 bytes, 0x004ffe80) with the order and
   a ring of `points` vertices on the circle; the area cell under the
   centre (0x004f1fe0); refused when the cell holds as many craters as the
   partition allows (cell +0x39 against partition +0x4c), pushed inside the
   cell's edges, and refused near a `ZDEC_FEATURE` node's box (+5 m).
3. Clip 0x004fff20 -> 0x0050ec10 (zGeometry): the terrain polygons of the
   cell (flags 4 and 0x20000 / 0x10000) cut against the ring; the
   polygons inside dropped, those across the edge cut along it.
4. Tessellate 0x004fff80: a new node (0x0050e950) with the slope ring
   (each ring vertex to the midpoint toward the floor) and the floor fan,
   textured by the ground's crater material and UVs over the radius
   (flags 0x1008), each polygon added by 0x0050dea0.
5. Record 0x00500850 (the history vector [0x0060bf94..9c], 0x34 bytes:
   type 1 crater, 3 quicksand, the order), note the touched nodes
   (0x00500600 into the set at 0x0060bfa0), commit 0x0050e850 (cell
   counts +1, old polygons released), the precision back.
6. With EDX set, the `DEFAULT_ANIM` (dust) at the centre (0x00508aa0).

## 3. Quicksand, 0x00502110

The same steps with the quicksand defaults: gated by [0x0060c0c8] and
[0x0060bfe8]; build 0x00502230 (0x005025e0 the record), clip 0x00502660,
tessellate 0x005026c0 (the animated material, 0x00539150), record,
commit. Who calls it: zWeapon 0x005824b0 for a weapon with the
`QUICKSAND` flag (no shipped weapon has it) and the restore 0x00500c20.

## 4. Bookkeeping

- The history vector (0x00500850 push, 0x00500af0 clear, 0x00500670 /
  0x005006b0 / 0x005006f0 its storage, 0x00500c60 walks it calling a
  crater and a quicksand callback: multiplayer sends every crater to a
  joining player, 0x0046d7e3).
- The touched-node set at 0x0060bfa0 (an MSVC `std::set`: 0x00500da0
  insert, 0x005015b0 node insert, 0x00501980 / 0x005018d0 iterators,
  0x00500e90 / 0x00500ff0 / 0x00501520 erase).
- The reset 0x00500760: every touched node re-read from gamez.zbd
  (0x004ff730, the re-read path not yet ported: gamez.md step 7), its cell's
  crater count to 0; the crater nodes (node list, 0x004f7fe0) destroyed.
- 0x00502020 (the unload's `decals_free`): 0x00500af0. 0x00501f90 sets a
  mode word at app start (`decals_set_mode`).

## 5. zGeometry, what the clip needs

| File | Functions | Pseudocode lines | What |
|---|---|---|---|
| zgeo_model.cpp 0x0050da90..0x0050f950 | 31 | 1,425 | area partitions, polygon buffers, node building (0x0050dea0 add a polygon), the clip driver 0x0050ec10 / 0x0050ea50 |
| zgeo_weiler.cpp 0x0050f950..0x005158c0 | 46 | 3,794 | Weiler–Atherton polygon clipping (0x00515410 the entry, 0x00510da0 the core, 926 lines) |
| zgeo_convexify.cpp 0x005158c0..0x005166c0 | 13 | 724 | splitting the clipped (concave) polygons into convex ones |

zDEClient itself is 40 functions, ~2,700 lines. So craters are about 8,600
lines of pseudocode, five times the first estimate.

## 6. Porting order

1. **declient.zrd** (0x00501ab0, 0x00501fa0, the world setters, the
   defaults 0x004ff920 / 0x005020f0, 0x00501f90): differential test on the
   six worlds' files, with the material and texture calls stubbed the same
   on both sides.
2. **zgeo_model's buffers and partitions** (the small functions).
3. **Weiler–Atherton** (two or three parts: the vertex lists, the
   intersections, the walk): differential tests on made-up polygon pairs
   (touching edges, shared vertices, holes) and on the worlds' cells.
4. **Convexify**.
5. **The crater** (0x004ffad0, 0x004fff20, 0x004fff80, 0x004ff940, the
   history and the set): differential test making craters on the worlds.
6. **Quicksand** (0x00502230 .. 0x005026c0).
7. **Reset and restore** (0x00500760, 0x00500c20) with gamez.zbd's re-read
   path; the program's wiring (zWeapon's crater hooks, the mission hooks).

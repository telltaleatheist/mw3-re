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
| zgeo_weiler.cpp 0x0050f950..0x005158c0 | 46 | 3,794 | Weiler–Atherton polygon clipping (0x0050faf0 the entry, 0x00510250 shared edges, 0x00510da0 the crossings and the walk, 926 lines) |
| zgeo_convexify.cpp 0x005158c0..0x005166c0 | 13 | 724 | splitting the clipped (concave) polygons into convex ones |

### zgeo_model, ported (part 2)

`src/gamez/zgeometry/zgeo_model` (`test_zgeo_model.py`, identical on 2,000
made-up runs at 24-bit x87). The clip works in a plane: (x, y, z) becomes
(x, -z, y) (0x0050dc90, back 0x0050dc70), so the ground's (x, z) is the
outline's 2D (x, y).

- The colour cache (0x0050da90 begin, 0x0050dad0 add, 0x0050dbc0 find;
  63 entries at 0x0060cb50): each cut polygon's new vertices (0x0050f4e0)
  note their colour when within the radius + 4 of the crater's centre; the
  crater's own vertices (0x004fff80) take the colour of the first noted
  vertex within 0.5 m in x and z, else of the first one noted. So a crater
  carries the ground's baked shading.
- Carrying a polygon's UVs, colours and normals to new points: the gradient
  of each value over the polygon's first three vertices in x and z
  (0x0050e020; 0 for a triangle with no area there), from vertex 1
  (0x0050df30, 0x0050e100, 0x0050e240; normals made unit length).
- The clip polygon (0x1c bytes: Weiler's polygon, the points, the count, the
  box; 0x0050e450 new, 0x0050e4c0 out, 0x0050e520 free), the point search
  within 0.01 (0x0050e550, Weiler's 0x00515130), the edge search
  (0x0050e690: strictly between the ends, within 0.01) and the inserts
  (0x0050e5a0: a point replaces the one it matches or goes in after its
  edge).
- The clip set (0x10 bytes: the outline left, the cells; each cell 0x0c:
  the (node, new model) pairs and the cell; 0x0050e910 / 0x0050e930) and its
  commit 0x0050e850: the new models onto the nodes, the old ones released,
  each cell's crater count (+0x39) up by one.
- Polygon adds through zModel's 0x0053c6c0 (a hook until zModel's builder is
  ported): 0x0050dea0 (no UVs and no material: a random colour), 0x0050f4e0
  (a cut piece of a ground polygon, its values carried over; an untextured
  one gets a random colour and no colours or normals), 0x0050f640 (a
  polygon copied whole), with the vertex buffer 0x0050f0a0.
- The random colour 0x0050dd20 (rand() x 255 / 32767 per channel) and the
  fixed one 0x0050de00 (used by 0x004c42e0 for pure red and blue) pack the
  16-bit colour from the 0..255 values' low bits instead of their high
  ones. No shipped world has untextured polygons on the nodes a crater cuts
  (flag 0x10000), so the random colours never show.

The tessellation 0x004fff80 adds the crater's own polygons through
0x0050dea0 with no UVs and no material when the order's flags have neither
0x1000 nor 8: random colours again. It never does: the weapon's order
(0x005825b0) changes only the points, depth and radius of the defaults,
whose flags are 0x100c.

Left for parts 3 and 5: the clip itself (0x0050e950 the crater's node,
0x0050ea50 / 0x0050ef70 / 0x0050f7b0 / 0x0050f120 per node, 0x0050ec10 the
driver), which need Weiler–Atherton.

### Weiler–Atherton, part 3a

`src/gamez/zgeometry/zgeo_weiler` (`test_weiler.py`, identical on 2,400
made-up runs at 24-bit x87). A Weiler (0x28cc bytes, 0x0050f960) holds the
clip region, the crater's outline: its points with repeats within 0.01
dropped (0x00515360, in the caller's buffer too), moved to the origin when
its first point is past 65,536 m (0x00514c40; back 0x00514dd0), and kept
as rings of edges. Every polygon is two rings over the same points (0x3c-byte
edges: prev, next, flags, the two points, a box, the crossing, the contour;
0x0050fe70 / 0x005146a0), the first flagged 1 (the region) or 2 (the clipped
polygon), the second 4, each with a 0x0c-byte contour record (0x005129a0).

0x0050faf0 clips a polygon (+0x28 count, +0x2c points, the caller's
buffer, changed in place and back) into a 0x20-byte result: three lists of
contours (point count, first float) over one point array. Its first steps,
ported: the case by the boxes (0x0050ff70): 1 apart; 2 the region inside the
polygon, 3 the polygon inside the region (every point inside, 0x00510180
by the crossing count 0x00513cf0); 4 the two the same (every point within
0.001); 0 they cross. The whole-polygon results: 0x005139e0 (the polygon or
the region as the inside list's one contour), 0x00513a90 (the polygon with
the region as a hole, joined between their rightmost points, 0x00515040 the
bridge), the results' z from the plane through the polygon's first three
points (0x00514e40). For the crossing case (part 3b): the reset 0x00514d10
and each polygon's points against the other's edges (0x00513750).

The snap 0x00515410 (from 0x0050ea50, not the clip's entry) moves the
outline's points onto the terrain polygon's points within 0.1, else onto its
edges (0x00515170).

Quirks kept: the axes swap (0x00513680) runs before the axes are stored, so
it always swaps x and z (every caller passes 0); the first rings point into
the caller's buffer (0x0050faf0 rebuilds them first); the bridge check
(0x00515040) only ever tests the region's closing edge, harmless from the
rightmost points. Fixed (FIX (bug), fixes.md): the buffers grow by realloc
while edges keep pointers into them (past 128 entries the clip reads freed
memory), and the side tables are 1,280 floats each, overrun past that.

### Weiler–Atherton, part 3b: shared edges

0x00510250 ("WeedOut", its error text) is not the crossing search: it
merges edges of the region and the polygon that lie on each other (each
end within 1e-5 of the other's line, from the side tables 0x00513750).
For every region edge R against every polygon edge P, in ring order over the
edges as built: the case is which ends lie within the other edge (R.a on P
8, R.b on P 4, P.a on R 2, P.b on R 1, 0x00514f80), and whether the two run
opposite ways in x or y. The shared part is kept once, in the polygon's
rings, with both edges' flags or-ed in; the region's edge is cut back to
the rest (0x00513930 splits an edge and its twin at a point); a polygon
edge equal to a region edge is unlinked from its rings (the contour moved
on when it was the first). Codes 4 and 8 and those below 3 do nothing.
`test_weiler.py` builds region and polygon pairs on a grid (rectangles and
diamonds sharing, overlapping and reversing sides) and reaches every branch.

The real crossing search is in the walk, 0x00510da0 (with 0x00513860, which
divides an edge at a crossing).

### Weiler–Atherton, part 3c (first half): the walk

0x00510da0 steps the region's two rings (R, R2) and, for each edge, the
polygon's (P, P2) by their links, so pieces split off are visited too. A
pair is skipped when their boxes are apart or they already share a
crossing. 0x00513f20 types the meeting (0x00514280: the four ends' sides
of the other edge, indexed into the 81-entry table at 0x005ec348; when two
ends touch, a point just past the touching end is tested against the
region (0x00513cf0) to say which side the touch counts on) and makes the
crossing record (0x30 bytes: the point, eight edge links, the type): 4/5
across (the point solved from the two lines, z along the polygon's edge),
6/7 at R.a, 8/9 at R.b, 12..17 at P.a, 18..23 at P.b, 3 along each other
(the case by which ends lie on the other edge), others none. Then by type:
edges divided at the crossing (0x00513860; an edge whose end is within
0.001 of it is not cut, its next edge takes the crossing), or the corners
judged (0x00514730 one corner against an edge, 0x00514840 two corners at
one point) and the crossing linked as the end of one edge and the start of
the next (+0x34 / +0x30). A last pass writes each crossing's eight links
(which R, R2, P, P2 edges end and start there). The or of the types is
returned.

FIX (bug, fixes.md): with a polygon corner on a region edge to within a
float's error, each pass makes a new crossing a bit off the last and the
divided pieces link into a cycle that never reaches the ring's first edge:
the exe loops forever, allocating crossings (`test_weiler.py` confirms the
original never returns on those inputs). The port gives up after
64 + 16 x the edge count steps for one region edge, as the walk's
intersection errors do (returns 1).

### Weiler–Atherton, part 3c (second half): contours and the driver

0x005129f0 ("contourMerge") first checks every crossing (0x005154d0,
"validateXing"): each type needs some of its eight links (all eight for
4/5; for 10/11 it depends on whether an R2 edge ends there, for 24/25 on
P2), else the clip fails. Then it rejoins the rings at each crossing by its
type, setting next/prev of the R, R2, P and P2 edges meeting there (the
twins run the other way, so some joins are next-to-next), and gives one or
two of those edges a contour record (0x005129a0). The pseudocode hides
which edges: the asm passes, per type, 4/5/6/8 P end and P start, 7 P2 end
and P start, 9 R end and P2 start, 10/11 R2 end and R2 start (R2 present)
or R end and R start, 12/18 R start, 13/19 R end and R start, 14/20 R end,
15 P start, 16 R2 end and P start, 17 R2 end, 21 R2 start, 22 P end and P2
start, 23 P end, 24/25 P2 end and P2 start (P2 present) or P end and P
start.

0x00513390 then walks each contour by its links with 0x00514710 (the next
edge, turned to follow when it runs the other way: prev and next, a and b
swapped), counting its edges and or-ing their flags; contour records met on
the way are dropped (only the first of a merged ring keeps one). A contour
that starts on a twin (4) edge is turned round at its first region or
polygon edge and counted again. Flags 3 (region and polygon edges, the part
inside) clear the `untouched` byte (+0x28c9). 0x00513480 sends contours by
their flags to the wanted lists: 3 to the first (inside), 2 or 6 (the
polygon's) to the second (outside), 1 or 5 (the region's) to the third;
0x005135a0 writes a ring's points (its first edge's a, then each b).

The driver 0x0050faf0 ("weiler_clip", ECX the Weiler, EDX the lists
wanted, then points, count, Result): the polygon is swapped and moved into
the clip's axes and origin in the caller's buffer, the Result pointed at
four new buffers, then the case (0x0050ff70): 1 apart returns 1 at once; 4
equal gives the polygon whole (3). Otherwise the rings, side tables,
weed-out and walk run; with crossings, merge, count and, unless
`untouched` (and the region is not inside the polygon), output: code 2.
Without crossings (or untouched) the trivial case decides: 2 the polygon
with the region as a hole and the region (code 4), 3 the polygon whole (3),
else 1. Every exit but the first moves the polygon back; codes 2..4 set the
results' z on the polygon's plane (0x00514e40). Failures free the Result
(0x0050fe10) and return 0.

FIX (bug, fixes.md): when the polygon shares an edge with the region and
touches it at a corner (grid shapes), a merged contour's links can cycle
without returning to its first edge and 0x00513390 loops forever
(`test_weiler.py` confirms the original never returns). The port gives up
after as many steps as there are edges and the clip fails (code 0), so
that piece of ground stays uncut. 0x005135a0 writes until it reaches the
first edge's prev, which can be off the ring (the first edge was turned
before its neighbours): the exe writes on forever; the port stops after
one lap, the whole contour, and the cut is made.

Quirk kept: "apart" returns before the polygon is moved back, so for a
region past 65,536 m from the origin (or swapped axes) the caller's
polygon is left moved. The game's worlds are smaller and every caller
passes axes 0, so it never shows.

zDEClient itself is 40 functions, ~2,700 lines. So craters are about 8,600
lines of pseudocode, five times the first estimate.

## 6. Porting order

1. **declient.zrd** (0x00501ab0, 0x00501fa0, the world setters, the
   defaults 0x004ff920 / 0x005020f0, 0x00501f90): differential test on the
   six worlds' files, with the material and texture calls stubbed the same
   on both sides.
2. **zgeo_model's buffers and partitions** (the small functions). Done:
   24 of its 32 functions (section 5); the six clip functions wait for
   Weiler.
3. **Weiler–Atherton** (two or three parts: the vertex lists, the
   intersections, the walk): differential tests on made-up polygon pairs
   (touching edges, shared vertices, holes) and on the worlds' cells. Done:
   all of zgeo_weiler (parts 3a..3c, `test_weiler.py`); the worlds' cells
   come with the crater (step 5).
4. **Convexify**.
5. **The crater** (0x004ffad0, 0x004fff20, 0x004fff80, 0x004ff940, the
   history and the set): differential test making craters on the worlds.
6. **Quicksand** (0x00502230 .. 0x005026c0).
7. **Reset and restore** (0x00500760, 0x00500c20) with gamez.zbd's re-read
   path; the program's wiring (zWeapon's crater hooks, the mission hooks).

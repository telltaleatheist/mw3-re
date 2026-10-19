# zClass core for the mechlib node-tree reader 

Mech3.exe 1.2, 2026-10-19. Everything `read_node` (0x4fce50) and `read_tree`
(0x4fcdc0) reach in GameZ `zclass\` (class.c, list.c, cls_di.c, cls_util.c,
object3d.c). Calling conventions are from the asm (Ghidra drops ECX/EDX).
"fastcall" here means MSVC `__fastcall`: arg 1 in ECX, arg 2 in EDX, callee
pops nothing unless stated. Checked in Unicorn (scratch scripts, see the end).

## 1. Node pool

Globals:

| Address | Meaning |
|---|---|
| 0x5ea830 | slots per chunk, .data initial 0x9c4 (2500); never written by the exe |
| 0x5ea834 | free-list head (slot index), initial -1 |
| 0x60bf38 | `Node**` chunk array (`realloc`ed) |
| 0x60bf30 | chunk count |
| 0x60bf34 | total slots (chunks x 0x5ea830) |
| 0x60bf3c | live node count |

Slot = 0xd4 bytes: the 0xd0-byte `Node`, then a dword at +0xd0 whose low 24
bits are the next free index (0xffffff = end; read sign-extended, so -1) and
whose bit 24 (0x01000000) is an "in use" bit that only `cls_zbd.c` maintains
(set/cleared by 0x4ff200 when gamez.zbd is loaded, tested by 0x4fe180).
Slot address = `chunks[i / per] + (i % per) * 0xd4` (the `*53*4` sequence at
0x4ead09).

### 0x4eacd0 `node_alloc()` -> Node* (no args)

1. If free == -1, `0x4f8c80()`; if still -1, `0x4eada0(msg)` (abort) and
   return 0 (never reached: the abort exits).
2. slot = index free; free = sign-extended low 24 bits of slot+0xd0.
3. Zero 0x34 dwords = 0xd0 bytes (the +0xd0 dword is not touched).
4. count (0x60bf3c) += 1.
5. `0x4f8080(6, node)`: prepend to list 6 (all nodes).
6. Defaults: flags +0x24 = 0x0108001c (ID_ZONE_CHECK | TREE_VALID | ACTIVE |
   ALTITUDE_SURFACE | INTERSECT_SURFACE, mech3ax names); +0x44 = 1;
   +0x4c = +0x50 = -1; **byte** +0x30 = 0xff (so the dword is 255 =
   mech3ax `ZONE_DEFAULT`, not -1); `sprintf(node, "%s", "Default_node_name")`
   (0x5bb658, fmt 0x5e2360, str 0x5e9e78); +0xbc = 0 (already zero).
7. Returns the node.

Globals: reads/writes 0x5ea834, 0x60bf3c; reads 0x5ea830, 0x60bf38. Calls
0x4f8c80, 0x4f8080, sprintf, 0x4eada0. Callers (10): world 0x4f1bd1, camera
0x4f32e2, window 0x4f9e95, display 0x4fa4c1, light 0x4fa722, object3d 0x4fb741,
0x4fd281, 0x4fdc71, 0x4fe031, 0x5930ff.

### 0x4f8c80 `node_pool_grow()` -> 0 ok, 1 fail

`chunks = realloc(chunks, 4*(nchunks+1))`; NULL -> return 1.
`chunks[nchunks] = malloc(per*0xd4)`; NULL -> return 1. Zero it
(`per*0x35` dwords). free = old total; total += per; slots old..total-2 get
link = i+1 (low 24 bits, top byte kept, i.e. 0 here), the last gets
`|= 0xffffff`. nchunks += 1. Only caller 0x4eacdd.

### 0x4eada0 `gamez_abort(msg)` (ECX = msg), never returns

If [0x72f348] == 2: `fflush(stdout)`, `fflush(stderr)` only (message never
shown). Else: 0x579b40(0) and 0x577340 (zvid: leave the DirectDraw mode),
`ShowCursor(1)`, `MessageBeep(0x30)`, `MessageBoxA([0x608f80], msg,
"This program must Abort.", 0x10)`, `ShowCursor(0)`, 0x579b40(1), then
`fprintf(stdout/stderr, "%s", msg)` and `"ABORT.  Shutting down...\n"` to
both, flush both. Then 0x579ef0 (zvid shutdown) and `ExitProcess(0)`.
Message for the pool: 0x5e9e30 "GameZ node resource failure: restarting your
game usually solves this."

### Free functions

- 0x4f8de0 `node_slot_free(node)` (ECX): finds the chunk with
  `chunk <= node <= chunk + (per-1)*0xd4` (unsigned), index = chunk_base +
  (node-chunk)/0xd4; slot link = old free (low 24 bits), free = index,
  count -= 1. Does not clear the node, touch lists or bit 24. Only caller
  0x4eaf90.
- 0x4f8ec0 `node_slot_free_index(i)` (ECX): same by index; callers 0x4ff200
  (cls_zbd, rebuilding the free list after loading slots).
- 0x4eacb0 returns total (0x60bf34); 0x4eacc0 returns count (0x60bf3c).

### Init / reset

There is no pool init: the statics start as chunk size 2500, free -1, and the
first allocation grows. zClass startup 0x4f8f20 (called at 0x4805f0, 0x58faf7)
sets [0x5e9e2c] = 1, [0x60b94c] = 0; 0x4f8f70 (0x48fd88) installs the two
log callbacks (0x4f7630/0x4f7650 -> 0x5ea510/0x5ea514, default 0x5ac5c0) and, if [0x5ea82c], likely a
periodic callback (0x5a8a20(0x4f3230, 1000, 0)), sets [0x60bf48] = 1. Reset `0x4f8fb0` (via 0x4f8f40 at
0x480920, and 0x490545): 0x4f8ab0 (destroy every node on list 6, below), then
if total > 0: free each chunk and the array, nchunks = total = count = 0,
free = -1. Then the callbacks reset, 0x4fe0f0, [0x60bf48] = 0.

## 2. The node lists (list.c)

### Layout

16 lists of 12 bytes `{head, tail, needs_purge}` at 0x60be4c..0x60bf0b,
reached through two pointer tables: 0x5ea6d0[i] = &head, 0x5ea710[i] = &tail.
Memory order: 6, 0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 13, 14, 15, 11, 12
(list 6 at 0x60be4c, list 7 at 0x60bea0).

| List | Holds (from 0x4f8390's removals, table 0x4f847c) |
|---|---|
| 0..5 | action lists: nodes with +0x48 (callback) != 0 and 0 <= +0x44 < 6 |
| 6 | every node (0x4eacd0) |
| 7 | changed nodes; node flag bit 0 = "on list 7", node+0xcc = its item |
| 8 camera, 9 light, 10 sound, 11 sequence (type 7), 12 animate (type 8), 13 world, 14 window, 15 display | per type; object3d (5), LoD (6), switch (11) have none |

Item = 16 bytes `{node, prev (+4), next (+8), removable (+0xc)}`. Item pool:
chunks of [0x5ea750] = 2500 items (`calloc`), chunk array 0x60bf18, count
0x60bf14, free list 0x60bf0c (linked by +8). 0x60bf10 = deferred-delete list.

- 0x4f77d0 `item_alloc()` -> item: pops the free list (clears +4, +8, +0xc,
  not +0); else grows: realloc array, calloc a chunk, items 1..n-1 go on the
  free list, **item 0 is returned**. Returns 0 on failure (callers do not
  check).
- 0x4f78e0 `item_free(item)` (ECX): push on the free list.
- 0x4f7920 frees all item chunks; 0x4f89a0 zeroes all 16 lists, 0x60bf0c,
  0x60bf10 and sets [0x5ea6b0] = 1 (lists enabled; .data initial 1).

### 0x4f8080 `list_push_front(list, node)` (ECX list, EDX node) -> 0

1. Only if list == 7 and `node+0xc4 <= [0x72f338]` (unsigned): [0x73f3a8] =
   [0x72f338] + 0xa0, `0x4ed990(node)`.
2. item = 0x4f77d0(); item.node = node; prepend (if head was 0, tail = item;
   else item.next = old head, old head.prev = item); head = item.
3. List 7 only: node flags |= 1; node+0xcc = item; item.removable =
   !(flags & 2); then for each parent (+0x58[i], i < +0x54) whose flags bit 0
   is clear and type != 2 (world): `0x4f8140(7, parent)`.

### 0x4f8140 `list_push_back(list, node)` (ECX, EDX) -> 0

Same as 0x4f8080 but appends at the tail and has no step 1. The parent walk
recurses through 0x4f8140, so a changed node is prepended and its unlisted
ancestors (up to, not including, a world) are appended.

### Others (for the port of removal, not needed by read_tree)

- 0x4f8220 `list_remove(list, node)` (ECX, EDX): lazy: list 7 uses
  node+0xcc (returns 0 without removing if flags bit 1 is set; else clears
  +0xcc and bit 0); others search for an item with this node and
  removable == 0. The item gets node = 0, removable = 1; the list's
  needs_purge = 1. Returns 1 if the list is empty.
- 0x4f7980 `list_purge(list)` (ECX): unlinks and frees items with removable
  != 0 or node == 0, except list-7 items whose node has flags bit 1 (those get
  removable = 0); for list 7 clears node bit 0 and +0xcc. Clears needs_purge.
  Returns 0 at once if [0x5ea6b0] == 0.
- 0x4f7bd0 purges every list with needs_purge set, then 0x4f81e0 (the
  deferred-delete list: 0x4eaf90 on each node). Returns 1 if [0x5ea6b0] == 0.
- 0x4f8390 `node_unlist(node)` (ECX): 0x4f8220 on its type list, on 7 if
  flags bit 0, on its action list, and on 6.
- 0x4f7f80 count, 0x4f8040 head, 0x4f7fe0 find by name (strcmp), 0x4f7fa0
  debug print, 0x4f8050 push on the deferred-delete list, 0x4f7e50 walk list
  7 (sets bits 0/1, 0x4f7e00), the frame update, not needed here.

### 0x4ed990 `node_set_expiry(node)` (ECX), recursive

node+0xc0 = 0; node+0xc4 = [0x73f3a8]; then the same for children, last to
first. +0xc4 is a frame number (likely "cached data valid until frame"),
+0xc0 is set elsewhere by 0x4ed980 from [0x5e9e2c] (= frame + 1 when
[0x60b94c] == 0, 0x4ed960).

### 0x4ed8f0 `node_set_tree_valid(node, on)` (ECX, EDX) -> 0, or 1 if node null

on != 0: if node+0x54 (parent count) > 1, stop (no change, no recursion);
else flags |= 0x80000. on == 0: flags &= ~0x80000. Then recurse into every
child with the same `on`. 0x80000 is mech3ax `TREE_VALID`: "one path to the
root". Callers: 0x4eba90 (child gained a 2nd parent, on = 0), 0x4ebc30
(child down to 1 parent, on = 1), 0x4f2990, 0x4f2a2a (world).
0x4ed8b0 `node_root(node)` (ECX) walks single parents up; returns 0 (and logs
"Multiple parents found") at a node with > 1 parent.

## 3. Object3d

### Data (0x90 bytes, `calloc`; mech3ax `Object3dC`, offsets in hex)

| Off | Field | Init (0x4fb650) |
|---|---|---|
| 0x00 | flags | `(flags & ~0x10) | 0x09`, then 0x4fb600 adds 0x20 -> 0x29 |
| 0x04 | opacity | 0 |
| 0x08 | color (3 floats) | 0 |
| 0x14 | field020 | 0 |
| 0x18 | rotate (3 floats) | 0 |
| 0x24 | scale (3 floats) | 1.0 |
| 0x30 | transform 3x4 (rows of 3; translation at 0x54) | identity (1.0 at 0x30, 0x40, 0x50) |
| 0x60 | second 3x4 (mech3ax field096) | 0 |

Flags (mech3ax): 0x01 needs update, 0x02 OPACITY, 0x04 COLOR,
0x08 TRANSFORM_INITIAL, 0x10 USE_MATRIX, 0x20 UNK5. In read_node the file's
0x90 bytes overwrite all of this, so the init values are dead there.

### 0x4fb740 `object3d_create()` -> Node* or 0 (no args)

node = 0x4eacd0(); null -> log (0x49bfd0, line 0x159) and 0. node+0x34 = 5;
node+0x38 = `calloc(1, 0x90)`; `0x4fb650(node)`; returns node if that returned
0, else 0 (the slot leaks). 27 callers.

### 0x4fb650 `object3d_init(node)` (ECX) -> 0, or 5 if node or data is null

Writes the data as in the table, then `0x4fb600(node)`. Then:
if flags bit 0 clear: `0x4f8080(7, node)`, flags |= 1 (redundant: 0x4f8080
set it); else if node+0xc4 <= [0x72f338]: [0x73f3a8] = [0x72f338] + 0xa0,
`0x4ed990(node)`. Finally flags |= 2, and if node+0xcc: item.removable = 0.
This "mark changed" tail (bit 0 via list 7, bit 1, removable = 0) is the same
block in 0x4eba90, 0x4ebc30, 0x4eb370 and the object3d setters 0x4fbcc4 etc.

### 0x4fb600 `node_mark_unk25(node)` (ECX), recursive

If type 5: data flags |= 0x20. node+0x2c |= 1, flags |= 0x02000000 (mech3ax
UNK25); recurse into children that lack 0x02000000.

### Globals

- 0x72f338: video frame counter (zvid; `inc` at 0x576eb9 after each flip,
  zeroed at 0x579be7 and 0x579c8a). Read only here.
- 0x73f3a8: expiry frame handed to 0x4ed990 (always frame + 0xa0 = 160
  frames); written at 0x4f80a3, 0x4fb70e, 0x4fbce9.. and 0x50f077.

### Node state after `object3d_create()` (emulated, frame 100)

name "Default_node_name", flags 0x0308001f, +0x2c = 1, dword +0x30 = 0xff,
+0x34 = 5, +0x44 = 1, +0x4c/+0x50 = -1, +0xc0 = 0, +0xc4 = 260, +0xcc =
item {node, prev, 0, removable 0}; on list 6 and list 7 (both newest first).

## 4. Add / remove child

### 0x4eb9b0 `node_add_child(parent, child)` (ECX, EDX)

child null -> log line 0x78f, return 5; parent null -> line 0x790, 5;
parent == child -> return 1 (silently). Jump on parent type (table 0x4eba64,
type-1): 1 camera 0x4f33a0, 2 world 0x4f2a40, 3/4/9/10 -> 0x4eba90 directly,
5 object3d 0x4fb7a0, 6 LoD 0x4fe0a0, 7 sequence: `sprintf(0x73f780, "...use
dedicated function gwSequenceAddChild()...")` + fatal 0x50da70(1), 8
0x4fd8c0, 11 0x4fd1e0, other: "Unrecognized node class type" + 0x50da70(1),
return 1. The case returns the callee's EAX.

### 0x4fb7a0 `object3d_add_child(parent, child)` (ECX, EDX)

Null checks (lines 0x1a2/0x1a3, parent data 0x1a4) -> 5; else tail-jumps to
0x4eba90.

### 0x4eba90 `node_link(parent, child)` (ECX, EDX) -> 0

1. parent+0x60 = realloc(parent+0x60, 4*(n+1)); append child; +0x5c += 1.
2. child+0x58 = realloc(child+0x58, 4*(m+1)); append parent; +0x54 += 1.
3. If child now has > 1 parent: `0x4ed8f0(child, 0)`.
4. parent+0x2c |= 2; then the "mark changed" tail on the parent (list 7 if bit
   0 clear, flags |= 3, item.removable = 0).
No duplicate check, no realloc check. Other caller 0x4fdd29 (seq.c).

### Remove

- 0x4ebb40 `node_remove_child(parent, child)` (ECX, EDX): null -> 5; type
  switch: 1 0x4f33e0, 2 0x4f2c30, 3 0x4f9f60, 4 0x4fa540, 5 0x4fb7f0,
  6 0x4fe0b0, 7 0x4fdd90, 8 0x4fd900, 9 0x4fa900, 10 0x4fd440, 11 0x4fd230.
- 0x4fb7f0 object3d: null checks (0x1be..0x1c0), then jumps to 0x4ebc30.
- 0x4ebc30 `node_unlink(parent, child)`: find child in parent's children
  (not found: log, return 1), shift the rest down, +0x5c -= 1 (array not
  shrunk); find parent in child's parents, shift, +0x54 -= 1; if it is now 1
  and the **removed parent's** flags have 0x80000: `0x4ed8f0(child, 1)`. Then
  parent+0x2c |= 2 and the mark-changed tail on the parent. Returns 0.

### Destroy

- 0x4f84b0 `node_destroy(node)` (ECX): gets the model (0x4eb420); if any:
  0x4eb370(node, 0) detaches it, and if the model's refcount (+0xc, 0x539d70)
  is 0, 0x539150 frees it. Object3d case: while children, 0x4fb7f0(node,
  children[0]); while parents, 0x4ebb40(parents[0], node); then 0x4f3390 ->
  0x4eb050. Returns the first non-zero error.
- 0x4eb050 `node_release(node)`: flags &= ~2, item.removable = 1; if
  [0x5e9e18] (immediate delete, .data initial 1): 0x4f8390 (unlist) and
  0x4f7bd0 (purge); if purge returned 1 (lists disabled) push on the
  deferred-delete list and return; else 0x4eaf90.
- 0x4eaf90 `node_free(node)`: returns 1 if it still has children, parents
  or a model (+0x3c); frees +0x60, +0x58, and +0x38 (then +0x34 = 0);
  0x4f8de0. Does not free +0xc8.
- 0x4eaea0 (other destroy, by type): only when no parents; frees +0xc8
  first, then per type (object3d -> 0x4eb050).
- Shutdown 0x4f8ab0: saves [0x5e9e18] and sets it 0, purges, repeatedly
  calls 0x4f84b0 on every list-6 node until a pass destroys none, 0x4f89a0,
  restores [0x5e9e18], 0x4f7920.

## 5. What read_tree produces (all of mechlib, emulated)

Original 0x4fcdc0 (thiscall: ECX = library object, depth counter at +0x3c;
stack handle, parent; ret 8) on all 57 `.flt` trees, 1400 nodes: every dword
of every node equals the file except

| Field | Nodes | Value |
|---|---|---|
| +0x24 flags | 1400 | file flags \| 3 (files never have bits 0/1) |
| +0x38 | 1400 | object3d data pointer |
| +0xcc | 1400 | list-7 item |
| +0x3c | 770 | model pointer (read_model) |
| +0x58 | 1343 | parents array (roots keep the file's 0) |
| +0x60 | 471 | children array |
| +0x2c | 471 | file \| 2 (every node with children; add-child) |

+0x54 and +0x5c end equal to the file (1 parent per non-root, children in
file order). So the step-4 comparison in `mechlib.md` should also exclude
+0x2c bit 1 and +0x3c, and +0x54 need not be excluded. The created node's
+0xc0/+0xc4 and 0x02000000 are overwritten by the file copy.

## 6. Testing in Unicorn

All of the above runs in the existing harness with only CRT stubs: `calloc`
0x5bb60c, `realloc` 0x5bb610, `malloc` 0x5bb664, `free` 0x5bb65c,
`sprintf` 0x5bb658 (only `"%s"` is used), plus `ReadFile` 0x5bb0c4 and the
model pool 0x538fe0 for read_tree. 0x49bfd0 (`zerr_report`) and 0x5ac5c0 are
empty in the exe and need no stub. Stub 0x50da70 (zerr fatal) to fail the
test. 0x4eada0 calls USER32/zvid and ExitProcess: stub the function itself
if a test forces pool failure. Set [0x72f338] (frame) explicitly; set
[0x5ea830] small (e.g. 3) to exercise chunk growth.

| Function | Isolated test |
|---|---|
| 0x4f8c80, 0x4eacd0, 0x4f8de0, 0x4f8ec0 | yes: compare globals + slot bytes |
| 0x4f77d0, 0x4f78e0, 0x4f8080, 0x4f8140, 0x4f8220, 0x4f7980 | yes: compare list heads/tails and item chains |
| 0x4ed990, 0x4ed8f0, 0x4ed8b0, 0x4fb600 | yes, pure on node graphs |
| 0x4fb650, 0x4fb740 | yes |
| 0x4eb9b0 (types 3/4/5/9/10), 0x4fb7a0, 0x4eba90, 0x4ebc30, 0x4fb7f0 | yes; other types call their class code |
| 0x4eb050 / 0x4eaf90 / 0x4f84b0 | yes for object3d without a model; with a model needs gmod (0x539150) |
| 0x4fcdc0 / 0x4fce50 | yes, done in scratch on all 1400 nodes (2.7 s) |

Compare graphs by structure (index each node by pool slot, pointers by slot
or item index), not raw pointers.

## Discoveries

1. `read_node` with a file node whose type is not 5 dereferences a null node
   (0x4fcebe: `ebx` is 0). Mechlib only has object3d nodes.
2. The default zone is the **byte** 0xff at +0x30 (dword 255), not -1.
3. The pool's slot +0xd0 "in use" bit (0x01000000) is neither set by
   0x4eacd0 nor cleared by 0x4f8de0; only the gamez.zbd loader keeps it, so
   run-time nodes carry whatever the slot had (likely harmless; matters for
   the cls_zbd save path 0x4fe180).
4. 0x4f8c80 overwrites the chunk-array pointer with realloc's result before
   checking it (leak, then a crash on the next lookup); malloc failure leaves
   a NULL entry without counting it.
5. 0x4f8de0, given a pointer outside every chunk, uses uninitialised locals.
6. 0x4f77d0 hands out item 0 of a new chunk and its `{prev,next}` from
   calloc; the free path does not clear +0 (node).
7. Object3d init is dead in read_node: ReadFile replaces all 0x90 bytes and
   the 0xd0 node copy replaces everything but +0x38, flag bits 0/1 and +0xcc.
   Only its list-7 membership survives.
8. 0x4fb740 leaks the slot if init fails (calloc NULL); the node stays on
   list 6.
9. TREE_VALID (0x80000) only reaches existing descendants: a child linked
   later under a multi-parent node keeps 0x80000 (emulated: node 4 under
   node 3 with two parents). On remove, the child regains it based on the
   **removed** parent's flag, not the remaining parent's path (likely wrong
   either way in DAGs; mechlib trees never have multiple parents).
10. add-child: no duplicate check, realloc unchecked, self-link returns 1
    without a log; the arrays never shrink on remove.
11. 0x4fb650 sets flags bit 0 again after 0x4f8080 already did.
12. 0x4f8080 prepends while 0x4f8140 appends, so list 7 holds the changed
    node at the front and its ancestors at the back.
13. The abort path in mode [0x72f348] == 2 prints nothing, just flushes and
    exits.

## Porting order (leaves first)

1. Node pool: 0x4f8c80, 0x4eacd0 (with the failure path as a call into the
   platform layer, FIX (A) for MessageBox/ExitProcess), 0x4f8de0, 0x4f8ec0,
   0x4eacb0/0x4eacc0, reset 0x4f8fb0's pool part. Keep the 0xd4-byte slots and
   index links: gamez.zbd stores raw slots (0x4ff200 `fread`s them).
2. List items and lists: 0x4f77d0, 0x4f78e0, 0x4f7920, 0x4f89a0, then
   0x4f8080, 0x4f8140 (mutually recursive with step 3's 0x4ed990 only through
   0x4f8080).
3. 0x4ed990, 0x4ed8f0, 0x4ed8b0, 0x4fb600.
4. 0x4fb650, 0x4fb740.
5. 0x4eba90, 0x4fb7a0, 0x4eb9b0 (other types stubbed to the zerr fatal until
   their classes are ported).
6. read_node / read_tree port, diffed against the original as in section 5.
7. Removal: 0x4f8220, 0x4f7980, 0x4f7bd0, 0x4f81e0, 0x4f8050, 0x4f8390,
   0x4ebc30, 0x4fb7f0, 0x4ebb40, 0x4eaf90, 0x4eb050, 0x4f84b0 (needs gmod's
   model release), 0x4f8ab0.

Scratch scripts used (session scratchpad, disposable): `emu.py` (pool growth
with chunk size 3, create, add/remove, free and reuse) and `tree.py`
(original read_tree on all mechlib trees, diff against file bytes).

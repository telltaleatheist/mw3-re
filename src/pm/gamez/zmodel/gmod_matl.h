#pragma once
#include <cstdio>

#include "../../../gamez/zmodel/gmod_matl.h"

// C:\zipper\branchep1\gamez\zModel\gmod_matl.c: what Pirate's Moon changed in
// the material table (docs/pm-map.md, gamez.zbd 41). The table is zmod's.
namespace pm::zmod {

// Pirate's Moon 1.0 @ 0x00555e00: a gamez.zbd's materials appended to the
// table in new blocks. The file's count and list heads are taken on the first
// load only; later ones put each material on the used or free list. Returns
// the first one's index (the file's material indices are offset by it), 0
// for an empty table in the file, -1 on a read error.
int read_materials(std::FILE* f);

// Pirate's Moon 1.0 @ 0x00557990 (ECX step), MW3's 0x00537f20 on PM's model pool, where a polygon
// lists its materials: each Instant Action and multiplayer frame with 10. The materials of `step`
// model slots marked a call (flags 0x10); past the last slot, each material without 0x08, 0x10 or
// 0x20 freed (0x00556ae0) and the marks cleared, the freed count returned.
int32_t material_sweep(int32_t step);

}  // namespace pm::zmod

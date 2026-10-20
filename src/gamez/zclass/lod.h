#pragma once
#include <cstdint>

#include "gamez/zclass/cls_node.h"

// The level-of-detail node (class 6). Original file unknown: these sit
// between zClass\Seq.c and cls_zbd.c (likely LoD.c). Data (0x50 bytes,
// mech3ax LevelOfDetail): +0 level, +8 / +0xc the range, +0x44 1, +0x48 and
// +0x4c (its square) a second range.
namespace zcls {

Node* lod_create();                               // 0x004fe030, no checks
int lod_set_level(Node* node, uint32_t level);    // 0x004fe0c0, data +0
int lod_set_range2(Node* node, uint32_t on, float range);  // 0x004fe0d0: +0x48 = on, +0x4c = range^2 when on

}  // namespace zcls

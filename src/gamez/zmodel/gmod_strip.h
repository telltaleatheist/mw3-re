#pragma once

#include <cstdint>

#include "gamez/zmodel/gmod_const.h"

// The hardware mode's triangle stripper (0x00552540, from mechlib's post-pass 0x004fc8b0; docs/mechlib.md).
// Original file unknown: 0x00550010..0x00555620, between gmod_puff.cpp and zreader.cpp; named for what it does.
// Polygons with vertex normals (0x200) whose material's flag 1 is set and that are strips (0x400) or have
// at most `small` vertices are joined along shared edges into strips of up to 64 vertices (and `limit`).
namespace zmod {

void strip_model(Model& m, uint32_t limit, uint32_t small);  // 0x00552540 (ECX m, EDX limit, small)

// [0x0072be74] (read 0x00553450, set 0x00553460): strip the models as they load, set by the video open in
// hardware mode (zvid_init.c 0x00579e79). Read by mechlib's load (0x004fc8f0) and a model's re-read (0x00538640).
bool& strips();

}  // namespace zmod

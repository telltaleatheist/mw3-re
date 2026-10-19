#pragma once
#include <cstdint>

#include "gamez/zclass/cls_di.h"
#include "gamez/zmodel/gmod_const.h"

// A model's polygons against the world queries' probes and segments
// (zclass/cls_di.c calls these per node). Original file unknown: the code
// sits between zModel\gmod_puff.cpp and zReader\zreader.cpp; named gmod_hit
// here.
namespace zmod {

// ECX node, EDX its model; the probes (x, y, z each), their mask (0 skips a
// probe), count, top and the out records. The model's vertices go through
// the top of the matrix stack (morphed first when the node set a morph);
// each polygon facing up (n.y > 0) that holds a probe in x and z, at a
// height at most top, is added to the probe's record. The polygon that held
// every probe is cached (header +0x58) and tried first next time; a hit
// there ends the search.
void model_ground(zcls::Node* node, Model* model, const float* probes, const int32_t* mask, int32_t count,
                  float top, zcls::GroundHits* out);  // 0x0054c5f0

// One polygon (its vertices in world space, `poly` for the flags, zone and
// material): 1 when every masked probe is inside it and under top.
int32_t polygon_ground(zcls::Node* node, zcls::GroundHits* out, const float* probes, const int32_t* mask,
                       int32_t count, float top, const float* verts, const Polygon& poly);  // 0x0054c330

}  // namespace zmod

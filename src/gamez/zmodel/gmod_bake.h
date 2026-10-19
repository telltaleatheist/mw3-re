#pragma once
#include <cstdint>

#include "gamez/zmodel/gmod_const.h"

// The baked static light (0x00544ed0): per-vertex light from every light with
// flag 0x100 (the sun among them) into each polygon's +0x18 array, which the
// shading (gmod_light, 0x00545720) reads instead of those lights. The model
// draw 0x0053f2e0 runs it when the node's light cache is stale
// (docs/drafts/bake-textures.md). Original file: between zModel\gmod_light.c
// and gmod_puff.cpp (func2file.json), most likely gmod_light.c itself.
namespace zmod {

struct BakeState {
  // [ebp-0x1c]: the light-to-vertex vector. The spot factor of a polygon's
  // first vertex reads it before it is set, so it carries over between
  // lights, polygons and calls (on the stack in the original).
  float stale[3] = {};
};
BakeState& bake_state();

// ECX model. cam: its vertices in camera space ([0x0074639c], the draw's
// transform); normals: its normals rotated into camera space ([0x007463a0]),
// read only for polygons with flag 0x200. Uses the light list as
// lights_reaching left it for this model (full, reach) and writes each
// non-two-sided polygon's scratch (+0x18) as floats, one per vertex in file order.
void bake_static_light(Model& model, const float (*cam)[3], const float (*normals)[3]);  // 0x00544ed0

}  // namespace zmod

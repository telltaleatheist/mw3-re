#pragma once
#include <cstdint>

#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zmodel/gmod_const.h"

// MakeShadows (a script command; ported for preservation, the engine's entry left null): shadows baked into a
// world's vertex colours, each point toward "sunlight" tested by a segment and darkened to a fifth when anything
// crosses it (docs/interp.md, The commands the shipped scripts never use). Original file unknown: between
// zModel\gmod_light.c and gmod_puff.cpp (func2file.json), after the baked static light (gmod_bake).
namespace zmod {

struct BakeShadowState {
  float offset[3] = {};             // 0x0073fc40  the light's direction times -1000
  zcls::Node* world = nullptr;      // 0x0073fc34  "world1"
};
BakeShadowState& bake_shadow_state();

// The segment query each point asks (zcls::segment_list, 0x004eed90); a test may stand in for it. Null: the port.
using ShadowQuery = int (*)(zcls::Node* world, const float from[3], const float to[3], zcls::LineHits* out);
void set_shadow_query(ShadowQuery q);

void shadow_model(Model& m);              // 0x00548250
void shadow_object(zcls::Node* node);     // 0x005484c0
void shadow_lod_children(zcls::Node* node);  // 0x005485b0 (unreferenced in the exe)
void bake_shadow_node(zcls::Node* node);       // 0x00548600
void shadow_world(zcls::Node* world);     // 0x00548650
int make_shadows();                       // 0x005487d0

}  // namespace zmod

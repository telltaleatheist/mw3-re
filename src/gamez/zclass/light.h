#pragma once
#include "gamez/zclass/cls_node.h"

// zClass\Light.c
namespace zcls {

Node* light_create();              // 0x004fa720

// The setters the mission's world.zrd reader calls on the sun (mission_world).
// Each marks the light changed (+0xb0 |= 1) and, for a static light (0x100),
// bumps the frame stamp so cached models bake again (gmod_bake). 0 or 5.
int light_set_orientation(Node* node, float x, float y, float z);       // 0x004faff0  data +0..+8
int light_get_orientation(Node* node, float* x, float* y, float* z);    // 0x004fb060
int light_set_ambient(Node* node, float v);                             // 0x004fa940  data +0xa0
int light_set_diffuse(Node* node, float v);                             // 0x004fa9b0  data +0x9c

}  // namespace zcls

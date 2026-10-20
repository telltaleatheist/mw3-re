#pragma once
#include "gamez/zclass/cls_node.h"

// zClass\Object3d.c (docs/zclass-world.md, section 3).
namespace zcls {

void node_mark_unk25(Node* node);                    // 0x004fb600
int object3d_init(Node* node);                       // 0x004fb650
Node* object3d_create();                             // 0x004fb740
int object3d_add_child(Node* parent, Node* child);   // 0x004fb7a0
int object3d_remove_child(Node* parent, Node* child);  // 0x004fb7f0
int object3d_get_translation(Node* node, float* x, float* y, float* z);  // 0x004fc020
int object3d_set_translation(Node* node, float x, float y, float z);     // 0x004fc0b0
int object3d_set_opacity(Node* node, float v);                    // 0x004fba20  data +4
int object3d_get_opacity(Node* node, float* out);                 // 0x004fbaa0
int object3d_use_opacity(Node* node, int on);                     // 0x004fbb90  data flag 2
int object3d_set_scale(Node* node, float x, float y, float z);    // 0x004fbc10
int object3d_get_scale(Node* node, float* x, float* y, float* z); // 0x004fbd20  no class check
int object3d_set_rotation(Node* node, float x, float y, float z);  // 0x004fbd80  no class check
int object3d_get_rotation(Node* node, float* x, float* y, float* z);  // 0x004fbe90  no class check
int object3d_add_rotation(Node* node, float x, float y, float z);  // 0x004fbef0  no class check
int object3d_add_translation(Node* node, float x, float y, float z);  // 0x004fc1b0
int object3d_set_matrix(Node* node, const float* m);              // 0x004fc300  EDX the 3x4
float* object3d_matrix(Node* node);                               // 0x004fc3f0  data +0x30, or null
// The movement port's name for 0x004fc1b0.
inline int object3d_translate(Node* node, float dx, float dy, float dz) { return object3d_add_translation(node, dx, dy, dz); }

}  // namespace zcls

#pragma once
#include "gamez/zclass/cls_node.h"

// zClass\Object3d.c (docs/zclass-world.md, section 3).
namespace zcls {

void node_mark_unk25(Node* node);                    // 0x004fb600
int object3d_init(Node* node);                       // 0x004fb650
Node* object3d_create();                             // 0x004fb740
int object3d_add_child(Node* parent, Node* child);   // 0x004fb7a0
int object3d_get_translation(Node* node, float* x, float* y, float* z);  // 0x004fc020
int object3d_set_translation(Node* node, float x, float y, float z);     // 0x004fc0b0
int object3d_translate(Node* node, float dx, float dy, float dz);        // 0x004fc1b0

}  // namespace zcls

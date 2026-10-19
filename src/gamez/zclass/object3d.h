#pragma once
#include "gamez/zclass/cls_node.h"

// zClass\Object3d.c (docs/zclass-world.md, section 3).
namespace zcls {

void node_mark_unk25(Node* node);                    // 0x004fb600
int object3d_init(Node* node);                       // 0x004fb650
Node* object3d_create();                             // 0x004fb740
int object3d_add_child(Node* parent, Node* child);   // 0x004fb7a0

}  // namespace zcls

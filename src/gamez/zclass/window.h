#pragma once
#include <cstdint>

#include "gamez/zclass/cls_node.h"

// zClass\Window.c: the window node's rect (data +0 x, +4 y, +8 width, +0xc height).
namespace zcls {

int window_size(Node* node, int32_t* width, int32_t* height);  // 0x004fa020
int window_origin(Node* node, int32_t* x, int32_t* y);         // 0x004fa130

}  // namespace zcls

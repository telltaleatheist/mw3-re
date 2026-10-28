#pragma once
#include "gamez/zclass/cls_node.h"

// zClass\Display.c
namespace zcls {

int display_set_size(Node* node, int32_t width, int32_t height);     // 0x004fa580: data +0x08, +0x0c
int display_set_origin(Node* node, int32_t x, int32_t y);            // 0x004fa600: data +0x00, +0x04
int display_set_clear_color(Node* node, float r, float g, float b);  // 0x004fa680

}  // namespace zcls

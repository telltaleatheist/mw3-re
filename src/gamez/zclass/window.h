#pragma once
#include <cstdint>

#include "gamez/zclass/cls_node.h"

// zClass\Window.c: the window node's rect (data +0 x, +4 y, +8 width, +0xc height).
namespace zcls {

int window_size(Node* node, int32_t* width, int32_t* height);  // 0x004fa020
int window_origin(Node* node, int32_t* x, int32_t* y);         // 0x004fa130
// A window node (class 3), 1 x 1, on list 14 (the HUD's views make theirs).
Node* window_create();                                         // 0x004f9e90
int window_set_size(Node* node, int32_t width, int32_t height); // 0x004f9fa0
int window_set_origin(Node* node, int32_t x, int32_t y);       // 0x004fa0b0
// The script commands' (ported for preservation; the engine's entries are null): WindowSetBuffer,
// WindowSetClearPolygon, WindowAddClearVertex, WindowCloseClearPolygon.
int window_set_buffer(Node* node, int32_t buffer);             // 0x004fa1c0
int window_set_clear_polygon(Node* node, int32_t on);          // 0x004fa230
int window_add_clear_vertex(Node* node, const float* xyz);     // 0x004fa2c0
int window_close_clear_polygon(Node* node);                    // 0x004fa3e0

}  // namespace zcls

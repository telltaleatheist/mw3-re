#pragma once
#include "gamez/zclass/cls_node.h"

// zClass\class.c: a node's matrix to the world and its box's world corners
// (docs/render.md section 1, docs/drafts/scenewalk.md).
namespace zcls {

// The frame update of changed nodes (list 7): 0x004f7e00 brings one node and
// its ancestors up to date, 0x004f7f70 runs the whole pass. Not ported yet
// (they rebuild +0x30 from the euler angles, 0x004eca00, and purge the
// lists); null hooks skip them.
struct UpdateHooks {
  void* ctx = nullptr;
  void (*update_node)(void* ctx, Node* node) = nullptr;  // 0x004f7e00
  void (*frame_update)(void* ctx) = nullptr;             // 0x004f7f70
};
UpdateHooks& update_hooks();

// ECX node, EDX multiply mode (0x0051fee0): the top of the matrix stack
// becomes node-to-world times what it held. 0 ok, 1 several parents, 3 a
// class with no matrix, 5 null node.
int node_to_world(Node* node, int mode);   // 0x004ed280
// ECX node, EDX out: its box (+0x74..+0x88) corners through its local matrix
// and the top of the stack. 0 ok, 1 no box (flag 0x100), 3 bad class, 5 null.
int node_box_corners(Node* node, float out[8][3]);  // 0x004ebdb0

}  // namespace zcls

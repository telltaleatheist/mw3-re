#include "gamez/zclass/cls_node.h"

#include <cstdio>

#include "gamez/zerror/zerr.h"

namespace zcls {

namespace {
NodePool g_pool;
Frames g_frames;
}  // namespace

NodePool& nodes() { return g_pool; }
Frames& frames() { return g_frames; }

// Mech3.exe 1.2 @ 0x004fe1d0. No upper bound check.
Node* node_at(int32_t index) {
  if (index < 0) return nullptr;
  return &g_pool.blocks[index / g_pool.block_size][index % g_pool.block_size];
}

// Mech3.exe 1.2 @ 0x004f8ec0. Only the link changes; the slot keeps its contents.
void free_node(int32_t index) {
  Node* n = node_at(index);
  n->link = (n->link & ~kNodeLinkMask) | (static_cast<uint32_t>(g_pool.free_head) & kNodeLinkMask);
  g_pool.free_head = index;
  --g_pool.live;
}

// Mech3.exe 1.2 @ 0x004ed990. Children last to first.
void set_expiry(Node* node) {
  node->put<uint32_t>(0xc0, 0);
  node->put<uint32_t>(0xc4, g_frames.expiry);
  for (int32_t i = node->child_count() - 1; i >= 0; --i) set_expiry(node->children[i]);
}

// The message's arguments are swapped: "Wanted" shows the class found.
int check_class(const Node* node, int cls, const char* file, int line) {
  if (!node) return zerr::report(0x400, file, line, "Null node pointer."), 5;
  if (!node->data) return zerr::report(0x400, file, line + 1, "Null class data pointer"), 5;
  if (node->cls() != cls) {
    char msg[64];
    std::snprintf(msg, sizeof msg, "Bad Class Found.\n Wanted (%d)\n Found (%d)", node->cls(), cls);
    return zerr::report(0x400, file, line + 2, msg), 3;
  }
  return 0;
}

}  // namespace zcls

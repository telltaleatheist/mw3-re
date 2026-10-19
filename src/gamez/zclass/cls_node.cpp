#include "gamez/zclass/cls_node.h"

#include <cstdio>
#include <cstdlib>
#include <new>

#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "platform/platform.h"

namespace zcls {

namespace {
NodePool g_pool;
Frames g_frames;
platform::Alert* g_alert = nullptr;
}  // namespace

NodePool& nodes() { return g_pool; }
Frames& frames() { return g_frames; }

void set_alert(platform::Alert* alert) { g_alert = alert; }

// Mech3.exe 1.2 @ 0x004eada0. FIX (A): the message box goes through the
// platform; the exe also leaves the DirectDraw mode first.
void gamez_abort(const char* message) {
  if (g_alert) g_alert->abort_message(message);
  for (std::FILE* f : {stdout, stderr}) {
    std::fprintf(f, "%s", message);
    std::fprintf(f, "ABORT.  Shutting down...\n");
    std::fflush(f);
  }
  std::exit(0);
}

// Mech3.exe 1.2 @ 0x004f8c80. The new block's slots link in order.
int node_pool_grow() {
  NodePool& p = g_pool;
  Node* block = new (std::nothrow) Node[p.block_size]{};
  if (!block) return 1;
  p.blocks.emplace_back(block);
  p.free_head = p.capacity;
  p.capacity += p.block_size;
  for (int32_t i = 0; i < p.block_size - 1; ++i) block[i].link = static_cast<uint32_t>(p.free_head + i + 1);
  block[p.block_size - 1].link |= kNodeLinkMask;
  return 0;
}

// Mech3.exe 1.2 @ 0x004eacd0. Every node starts on list 6 with these defaults.
Node* node_alloc() {
  NodePool& p = g_pool;
  if (p.free_head == -1) node_pool_grow();
  if (p.free_head == -1) gamez_abort("GameZ node resource failure: restarting your game usually solves this.");
  Node* n = node_at(p.free_head);
  p.free_head = static_cast<int32_t>(n->link << 8) >> 8;  // sign-extended: 0xffffff is -1
  const uint32_t link = n->link;
  *n = Node{};
  n->link = link;
  ++p.live;
  push_front(kAllNodes, n);
  n->set_flags(0x0108001c);
  n->put<int32_t>(0x44, 1);
  n->put<int32_t>(0x4c, -1), n->put<int32_t>(0x50, -1);
  n->raw[0x30] = 0xff;
  std::snprintf(reinterpret_cast<char*>(n->raw), 0x24, "%s", "Default_node_name");
  n->put<int32_t>(0xbc, 0);
  return n;
}

// Mech3.exe 1.2 @ 0x004f8de0. The slot keeps its contents and lists.
void node_slot_free(Node* node) {
  NodePool& p = g_pool;
  int32_t index = -1;
  for (size_t b = 0; b < p.blocks.size(); ++b) {
    const Node* base = p.blocks[b].get();
    if (node >= base && node <= base + p.block_size - 1)
      index = static_cast<int32_t>(b) * p.block_size + static_cast<int32_t>(node - base);
  }
  node->link = (node->link & ~kNodeLinkMask) | (static_cast<uint32_t>(p.free_head) & kNodeLinkMask);
  p.free_head = index;
  --p.live;
}

// Mech3.exe 1.2 @ 0x004eacb0
int32_t node_total() { return g_pool.capacity; }

// Mech3.exe 1.2 @ 0x004eacc0
int32_t node_live() { return g_pool.live; }

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

#include "gamez/zclass/cls_node.h"

#include <cstdio>
#include <cstdlib>
#include <new>

#include "gamez/zclass/list.h"
#include "gamez/zclass/object3d.h"
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

namespace {
const char kClassFile[] = "C:\\zipper\\GameZMech3\\zClass\\Class.c";

template <typename T>
T* append(T* array, int32_t count, T value) {
  T* grown = new T[count + 1];
  for (int32_t i = 0; i < count; ++i) grown[i] = array[i];
  grown[count] = value;
  delete[] array;
  return grown;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004ed8f0. TREE_VALID (0x80000): one path to the root.
// Setting it stops at a node with several parents; clearing goes all the way.
int node_set_tree_valid(Node* node, int on) {
  if (!node) return 1;
  if (on) {
    if (node->parent_count() > 1) return 0;
    node->set_flags(node->flags() | 0x80000);
  } else {
    node->set_flags(node->flags() & ~0x80000u);
  }
  for (int32_t i = 0; i < node->child_count(); ++i) node_set_tree_valid(node->children[i], on);
  return 0;
}

// Mech3.exe 1.2 @ 0x004ed8b0
Node* node_root(Node* node) {
  while (node) {
    if (node->parent_count() == 0) return node;
    if (node->parent_count() != 1) {
      char msg[96];
      std::snprintf(msg, sizeof msg, "Error getting root node; Multiple parents found.\n  Node: %s\n", node->name());
      zerr::report(0x200, kClassFile, 0x10b4, msg);
      return nullptr;
    }
    node = node->parents[0];
  }
  return nullptr;
}

// Mech3.exe 1.2 @ 0x004eba90. No duplicate check. FIX (A): new arrays for realloc.
int node_link(Node* parent, Node* child) {
  parent->children = append(parent->children, parent->child_count(), child);
  parent->put<int32_t>(0x5c, parent->child_count() + 1);
  child->parents = append(child->parents, child->parent_count(), parent);
  child->put<int32_t>(0x54, child->parent_count() + 1);
  if (child->parent_count() > 1) node_set_tree_valid(child, 0);
  parent->put<uint32_t>(0x2c, parent->get<uint32_t>(0x2c) | 2);
  if ((parent->flags() & 1) == 0) {
    push_front(kChanged, parent);
    parent->set_flags(parent->flags() | 1);
  }
  parent->set_flags(parent->flags() | 2);
  if (parent->changed) parent->changed->removable = 0;
  return 0;
}

// Mech3.exe 1.2 @ 0x004eb9b0. By the parent's class; the classes whose own
// add-child is not ported yet stop the game rather than guess.
int node_add_child(Node* parent, Node* child) {
  if (!child) return zerr::report(0x400, kClassFile, 0x78f, "Null node pointer."), 5;
  if (!parent) return zerr::report(0x400, kClassFile, 0x790, "Null node pointer."), 5;
  if (parent == child) return 1;
  switch (parent->cls()) {
    case 3: case 4: case 9: case 10: return node_link(parent, child);
    case 5: return object3d_add_child(parent, child);
    default: {
      char msg[80];
      std::snprintf(msg, sizeof msg, "node_add_child: class %d is not ported yet", parent->cls());
      gamez_abort(msg);
    }
  }
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

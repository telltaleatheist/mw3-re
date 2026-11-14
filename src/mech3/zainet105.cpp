#include "mech3/zainet105.h"

#include <cstdlib>

namespace mech3::ai {
namespace {
Net105* g_head = nullptr;  // [0x005f4c4c]
Net105* g_tail = nullptr;  // [0x005f4c50]
}  // namespace

// [0x005e8654]: the null node every "none" answer gives (0x004ae920).
Net105Node* Net105Node::none() {
  static Net105Node n;
  return &n;
}

// 0x004024a0: each link record freed (the exe's malloc), then the node.
Net105Node::~Net105Node() {
  for (Net105Link*& r : link_rec) {
    std::free(r);
    r = nullptr;
  }
}

// 0x004024e0: each node by +0x30, then the net.
Net105::~Net105() {
  for (Net105Node* n = first; n;) {
    Net105Node* next_node = n->next;
    delete n;
    n = next_node;
  }
  first = nullptr;
}

// Mech3.exe 1.2 @ 0x00401ab0's making (ours to own): a node of the defaults.
Net105Node* Net105::make_node() {
  made.push_back(std::make_unique<Net105Node>());
  return made.back().get();
}

Net105* nets105() { return g_head; }

// Mech3.exe 1.2 @ 0x004021c0's link: the first is the head, each after it the tail's +0x58.
void nets105_append(Net105* n) {
  if (g_head) g_tail->next = n;
  else g_head = n;
  g_tail = n;
}

// Mech3.exe 1.2 @ 0x00402560's walk over [0x005f4c4c].
void nets105_free() {
  for (Net105* n = g_head; n;) {
    Net105* next = n->next;
    delete n;
    n = next;
  }
  g_head = g_tail = nullptr;
}

}  // namespace mech3::ai
